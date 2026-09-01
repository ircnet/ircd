/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_dnsbl.c
 *   Copyright (C) 2003 erra@RusNet
 *   Copyright (C) 2003 Francois Baligant
 *   Copyright (C) 2024 IRCnet.com team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef lint
static const volatile char rcsid[] = "@(#)$Id: mod_dnsbl.c,v 1.3 2026/04/04 00:00:00 ai Exp $";
#endif

// clang-format off
#include "os.h"
#include "a_defines.h"
// clang-format on
#define MOD_DNSBL_C
#include "a_externs.h"
#undef MOD_DNSBL_C

#include "../ircd/resolv_def.h"
#include "../ircd/res_init_ext.h"
#include "../ircd/res_comp_ext.h"
#include "../ircd/res_mkquery_ext.h"

/* Cache time in minutes */
#define CACHETIME 30
#define DNSBL_LOOKUPLEN 512
#define DNSBL_MAXPACKET 512
#define DNSBL_GWORK_BUDGET 64
#define DNSBL_MAX_ACTIVE_QUERIES 128

struct hostlog {
	struct hostlog *next;
	char ip[HOSTLEN + 1];
	char listname[DNSBL_LOOKUPLEN];
	unsigned char answer[INADDRSZ];
	u_char state; /* 0 = not found, 1 = found, 2 = timeout */
	time_t expire;
};

#define OPT_LOG 0x001
#define OPT_DENY 0x002
#define OPT_PARANOID 0x004

#define OK 0
#define DNSBL_FOUND 1
#define DNSBL_FAILED 2

#define DNSBL_PENDING 0
#define DNSBL_DONE_FOUND 1
#define DNSBL_DONE_CLEAN 2
#define DNSBL_DONE_FAILED 3

#define DNSBL_PARSE_IGNORE 0
#define DNSBL_PARSE_FOUND 1
#define DNSBL_PARSE_NEXT_HOST 2
#define DNSBL_PARSE_RETRY 3

struct dnsbl_list {
	char *host;
	char *reason;
	struct dnsbl_list *next;
};

typedef struct dnsbl_pending {
	int active;
	int wake_r;
	int wake_w;
	int final_state;
	u_short qid;
	u_int ns_index;
	u_int tries;
	time_t deadline;
	struct dnsbl_list *current;
	char lookup[DNSBL_LOOKUPLEN];
	char listname[DNSBL_LOOKUPLEN];
	unsigned char answer[INADDRSZ];
	u_int queue_seq;
} DnsblPending;

struct dnsbl_private {
	struct hostlog *cache;
	char *reason;
	u_int lifetime;
	u_char options;
	/* stats */
	u_int chitc, chito, chitn, cmiss, cnow, cmax;
	u_int found, failed, good, total, rejects;
	struct dnsbl_list *host_list;
	char *desc;
	int gfd;
	u_int qid_seq;
	u_int queue_seq;
	DnsblPending pend[MAXCONNECTIONS];
};

static int dnsbl_resolver_ready = 0;

static void dnsbl_close_client_rfd(u_int cl)
{
	if (cldata[cl].rfd > 0)
	{
		close(cldata[cl].rfd);
		cldata[cl].rfd = 0;
	}
	cldata[cl].wfd = 0;
	cldata[cl].buflen = 0;
}

static void dnsbl_reset_pending(DnsblPending *p)
{
	if (p->wake_w > 0)
	{
		close(p->wake_w);
		p->wake_w = 0;
	}
	p->wake_r = 0;
	bzero((char *) p, sizeof(*p));
}

static int dnsbl_make_wakeup(u_int cl, int *rfd_out, int *wfd_out)
{
	int sp[2];
	int flags;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_make_wakeup(%d): socketpair() failed: %s",
				  cl, strerror(errno)));
		return -1;
	}

	flags = fcntl(sp[0], F_GETFL, 0);
	if (flags >= 0)
		(void) fcntl(sp[0], F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(sp[1], F_GETFL, 0);
	if (flags >= 0)
		(void) fcntl(sp[1], F_SETFL, flags | O_NONBLOCK);

	cldata[cl].rfd = sp[0];
	*rfd_out = sp[0];
	*wfd_out = sp[1];
	return 0;
}

static int dnsbl_ns_count(void)
{
	return MAX(1, ircd_res.nscount);
}

static int dnsbl_max_sends(void)
{
	return MAX(1, dnsbl_ns_count() * MAX(1, ircd_res.retry));
}

static int dnsbl_max_active_queries(void)
{
	return (DNSBL_MAX_ACTIVE_QUERIES < MAXCONNECTIONS) ?
		DNSBL_MAX_ACTIVE_QUERIES : MAXCONNECTIONS;
}

static int dnsbl_count_inflight(struct dnsbl_private *mydata)
{
	u_int i;
	int n = 0;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		DnsblPending *p = &mydata->pend[i];

		if (p->active && p->final_state == DNSBL_PENDING && p->qid != 0)
			n++;
	}
	return n;
}

static void dnsbl_dispatch_queued(struct dnsbl_private *mydata);

static int dnsbl_opt_eq(const char *token, const char *name)
{
	return strcmp(token, name) == 0;
}

static int dnsbl_opt_startswith(const char *token, const char *name)
{
	size_t n = strlen(name);
	return strncmp(token, name, n) == 0;
}

static int dnsbl_appendf(char **buf, size_t *size, size_t *used, const char *fmt, ...)
{
	va_list ap;
	int needed;
	char *nbuf;
	size_t nsize;

	if (!buf || !size || !used || !fmt)
		return -1;

	va_start(ap, fmt);
	needed = vsnprintf((*buf) ? (*buf + *used) : NULL,
					 (*buf && *size > *used) ? (*size - *used) : 0,
					 fmt, ap);
	va_end(ap);
	if (needed < 0)
		return -1;
	if (*buf && (size_t) needed < (*size - *used))
	{
		*used += (size_t) needed;
		return 0;
	}

	nsize = (*size != 0) ? *size : 128;
	while (nsize <= *used + (size_t) needed)
		nsize *= 2;
	nbuf = (char *) realloc(*buf, nsize);
	if (!nbuf)
		return -1;
	*buf = nbuf;
	*size = nsize;

	va_start(ap, fmt);
	needed = vsnprintf(*buf + *used, *size - *used, fmt, ap);
	va_end(ap);
	if (needed < 0)
		return -1;
	*used += (size_t) needed;
	return 0;
}


static int dnsbl_same_ns(const struct SOCKADDR_IN *a, const struct SOCKADDR_IN *b)
{
	return a->SIN_FAMILY == b->SIN_FAMILY &&
			a->SIN_PORT == b->SIN_PORT &&
			memcmp(a->SIN_ADDR.S_ADDR, b->SIN_ADDR.S_ADDR,
				   sizeof(a->SIN_ADDR.S_ADDR)) == 0;
}

static u_short dnsbl_new_qid(struct dnsbl_private *mydata)
{
	u_int n;

	for (n = 0; n < 1024; n++)
	{
		u_short qid = (u_short) (ircd_res_randomid() ^ (++mydata->qid_seq));
		u_int i;
		int used = 0;

		if (qid == 0)
			qid = 1;
		for (i = 0; i < MAXCONNECTIONS; i++)
		{
			if (mydata->pend[i].active && mydata->pend[i].qid == qid)
			{
				used = 1;
				break;
			}
		}
		if (!used)
			return qid;
	}
	return 0;
}

static int dnsbl_find_by_wake_r(struct dnsbl_private *mydata, int wake_r)
{
	u_int i;

	if (wake_r <= 0)
		return -1;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if ((mydata->pend[i].active || mydata->pend[i].wake_w > 0) &&
			mydata->pend[i].wake_r == wake_r)
			return (int) i;
	}
	return -1;
}

static DnsblPending *dnsbl_pending_for_cl(struct dnsbl_private *mydata, u_int cl, u_int *owner_out)
{
	int owner = -1;

	if (cldata[cl].rfd > 0)
		owner = dnsbl_find_by_wake_r(mydata, cldata[cl].rfd);
	if (owner < 0 && (mydata->pend[cl].active || mydata->pend[cl].wake_w > 0))
		owner = (int) cl;
	if (owner < 0)
		return NULL;
	if (owner_out)
		*owner_out = (u_int) owner;
	return &mydata->pend[owner];
}

static int dnsbl_build_lookup(u_int cl, struct dnsbl_list *l, char *lookup,
					 size_t lookup_len)
{
	struct addrinfo hints, *addr_res;
	int rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;

	rc = getaddrinfo(cldata[cl].itsip, NULL, &hints, &addr_res);
	if (rc)
		return -1;

	if (addr_res->ai_family == AF_INET)
	{
		const struct sockaddr_in *v4 = (const struct sockaddr_in *) addr_res->ai_addr;
		const uint8_t *b = (const uint8_t *) &v4->sin_addr.s_addr;

		snprintf(lookup, lookup_len, "%u.%u.%u.%u.%s",
				 (unsigned int) (b[3]), (unsigned int) (b[2]),
				 (unsigned int) (b[1]), (unsigned int) (b[0]),
				 l->host);
	}
	else if (addr_res->ai_family == AF_INET6)
	{
		const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *) addr_res->ai_addr;
		const uint8_t *b = (const uint8_t *) &v6->sin6_addr.s6_addr;

		snprintf(lookup, lookup_len,
				 "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
				 "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%s",
				 (unsigned int) (b[15] & 0xF), (unsigned int) (b[15] >> 4),
				 (unsigned int) (b[14] & 0xF), (unsigned int) (b[14] >> 4),
				 (unsigned int) (b[13] & 0xF), (unsigned int) (b[13] >> 4),
				 (unsigned int) (b[12] & 0xF), (unsigned int) (b[12] >> 4),
				 (unsigned int) (b[11] & 0xF), (unsigned int) (b[11] >> 4),
				 (unsigned int) (b[10] & 0xF), (unsigned int) (b[10] >> 4),
				 (unsigned int) (b[9] & 0xF), (unsigned int) (b[9] >> 4),
				 (unsigned int) (b[8] & 0xF), (unsigned int) (b[8] >> 4),
				 (unsigned int) (b[7] & 0xF), (unsigned int) (b[7] >> 4),
				 (unsigned int) (b[6] & 0xF), (unsigned int) (b[6] >> 4),
				 (unsigned int) (b[5] & 0xF), (unsigned int) (b[5] >> 4),
				 (unsigned int) (b[4] & 0xF), (unsigned int) (b[4] >> 4),
				 (unsigned int) (b[3] & 0xF), (unsigned int) (b[3] >> 4),
				 (unsigned int) (b[2] & 0xF), (unsigned int) (b[2] >> 4),
				 (unsigned int) (b[1] & 0xF), (unsigned int) (b[1] >> 4),
				 (unsigned int) (b[0] & 0xF), (unsigned int) (b[0] >> 4),
				 l->host);
	}
	else
	{
		freeaddrinfo(addr_res);
		return -1;
	}

	freeaddrinfo(addr_res);
	return 0;
}

static void dnsbl_free_cache(struct dnsbl_private *mydata)
{
	struct hostlog *pl, *next;

	for (pl = mydata->cache; pl; pl = next)
	{
		next = pl->next;
		free(pl);
	}
	mydata->cache = NULL;
	mydata->cnow = 0;
}

static void dnsbl_free_host_list(struct dnsbl_private *mydata)
{
	struct dnsbl_list *l, *n;

	for (l = mydata->host_list; l; l = n)
	{
		free(l->host);
		if (l->reason)
			free(l->reason);
		n = l->next;
		free(l);
	}
	mydata->host_list = NULL;
}

static void dnsbl_free_private(struct dnsbl_private *mydata)
{
	if (!mydata)
		return;

	dnsbl_free_cache(mydata);
	dnsbl_free_host_list(mydata);
	if (mydata->reason)
		free(mydata->reason);
	if (mydata->desc)
		free(mydata->desc);
	free(mydata);
}

static struct dnsbl_list *dnsbl_add_host(struct dnsbl_private *mydata, const char *host, const char *reason)
{
	struct dnsbl_list *l, **tail;

	if (!mydata || !host || !*host)
		return NULL;

	l = (struct dnsbl_list *) malloc(sizeof(struct dnsbl_list));
	if (!l)
		return NULL;
	l->host = mystrdup((char *) host);
	l->reason = (reason && *reason) ? mystrdup((char *) reason) : NULL;
	l->next = NULL;

	tail = &mydata->host_list;
	while (*tail)
		tail = &(*tail)->next;
	*tail = l;
	return l;
}

static const char *dnsbl_reason_template(struct dnsbl_private *mydata, const char *listname)
{
	struct dnsbl_list *l;

	for (l = mydata->host_list; l; l = l->next)
	{
		if (!strcasecmp(l->host, (char *) listname))
			return l->reason ? l->reason : mydata->reason;
	}
	return mydata->reason;
}

static char *dnsbl_expand_reason(u_int cl, const char *tmpl, const char *domain)
{
	const char *ip = cldata[cl].itsip ? cldata[cl].itsip : "";
	const char *host = cldata[cl].host ? cldata[cl].host : "";
	size_t len = 0;
	const char *p;
	char *out, *w;

	if (!tmpl)
		return mystrdup("");

	for (p = tmpl; *p; )
	{
		if (!strncmp(p, "{ip}", 4))
		{
			len += strlen(ip);
			p += 4;
		}
		else if (!strncmp(p, "{domain}", 8))
		{
			len += strlen(domain ? domain : "");
			p += 8;
		}
		else if (!strncmp(p, "{host}", 6))
		{
			len += strlen(host);
			p += 6;
		}
		else
		{
			len++;
			p++;
		}
	}

	out = (char *) malloc(len + 1);
	for (p = tmpl, w = out; *p; )
	{
		if (!strncmp(p, "{ip}", 4))
		{
			strcpy(w, ip);
			w += strlen(ip);
			p += 4;
		}
		else if (!strncmp(p, "{domain}", 8))
		{
			const char *d = domain ? domain : "";
			strcpy(w, d);
			w += strlen(d);
			p += 8;
		}
		else if (!strncmp(p, "{host}", 6))
		{
			strcpy(w, host);
			w += strlen(host);
			p += 6;
		}
		else
		{
			*w++ = *p++;
		}
	}
	*w = '\0';
	return out;
}

static void dnsbl_succeed(u_int cl, const char *listname, const unsigned char *result)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	const char *tmpl = dnsbl_reason_template(mydata, listname);
	char *reason = dnsbl_expand_reason(cl, tmpl, listname);

	if (mydata->options & OPT_PARANOID || (mydata->options & OPT_DENY &&
					   result[0] == '\177' && result[1] == '\0' &&
					   result[2] == '\0'))
	{
		cldata[cl].state |= A_DENY;
		sendto_ircd("k %d %s %u #dnsbl :%s", cl,
					cldata[cl].itsip, cldata[cl].itsport,
					reason ? reason : "");
		mydata->rejects++;
	}
	if (mydata->options & OPT_LOG)
		sendto_log(ALOG_FLOG | ALOG_IRCD | ALOG_DNSBL, LOG_INFO, "%s: found: %s[%s]",
				   listname, cldata[cl].host, cldata[cl].itsip);
	if (reason)
		free(reason);
}
static void dnsbl_add_cache(u_int cl, u_int state, const char *listname,
				 const unsigned char *answer)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	struct hostlog *pl;
	time_t expire;

	if (state == DNSBL_FOUND)
		mydata->found++;
	else if (state == DNSBL_FAILED)
		mydata->failed++;
	else
		mydata->good++;

	if (mydata->lifetime == 0 || state == DNSBL_FAILED)
		return;

	expire = time(NULL) + mydata->lifetime;
	for (pl = mydata->cache; pl; pl = pl->next)
	{
		if (!strcasecmp(pl->ip, cldata[cl].itsip))
		{
			pl->expire = expire;
			pl->state = state;
			if (listname)
			{
				strncpy(pl->listname, listname, sizeof(pl->listname) - 1);
				pl->listname[sizeof(pl->listname) - 1] = '\0';
			}
			else
				pl->listname[0] = '\0';
			if (answer)
				memcpy(pl->answer, answer, sizeof(pl->answer));
			else
				bzero(pl->answer, sizeof(pl->answer));
			DebugLog((ALOG_DNSBLC, 0,
				  "dnsbl_add_cache(%d): refreshed cache %s, result=%d",
				  cl, pl->ip, state));
			return;
		}
	}

	pl = (struct hostlog *) malloc(sizeof(struct hostlog));
	if (!pl)
		return;
	bzero((char *) pl, sizeof(*pl));
	pl->expire = expire;
	strcpy(pl->ip, cldata[cl].itsip);
	pl->state = state;
	if (listname)
	{
		strncpy(pl->listname, listname, sizeof(pl->listname) - 1);
		pl->listname[sizeof(pl->listname) - 1] = '\0';
	}
	if (answer)
		memcpy(pl->answer, answer, sizeof(pl->answer));
	pl->next = mydata->cache;
	mydata->cache = pl;
	mydata->cnow++;
	if (mydata->cnow > mydata->cmax)
		mydata->cmax = mydata->cnow;
	DebugLog((ALOG_DNSBLC, 0,
			  "dnsbl_add_cache(%d): new cache %s, result=%d",
			  cl, mydata->cache->ip, state));
}

static int dnsbl_check_cache(u_int cl)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	struct hostlog **last, *pl;
	time_t now = time(NULL);
	if (!mydata || mydata->lifetime == 0)
		return 0;

	DebugLog((ALOG_DNSBLC, 0,
			  "dnsbl_check_cache(%d): Checking cache for %s",
			  cl, cldata[cl].itsip));

	last = &(mydata->cache);
	while ((pl = *last))
	{
		DebugLog((ALOG_DNSBLC, 0, "dnsbl_check_cache(%d): cache %s",
				  cl, pl->ip));
		if (pl->expire < now)
		{
			DebugLog((ALOG_DNSBLC, 0,
					  "dnsbl_check_cache(%d): free %s (%ld < %ld)",
					  cl, pl->ip, (long) pl->expire, (long) now));
			*last = pl->next;
			free(pl);
			mydata->cnow--;
			continue;
		}
		if (!strcasecmp(pl->ip, cldata[cl].itsip))
		{
			DebugLog((ALOG_DNSBLC, 0,
					  "dnsbl_check_cache(%d): match (%u)",
					  cl, pl->state));
			pl->expire = now + mydata->lifetime;
			if (pl->state == DNSBL_FOUND)
			{
				dnsbl_succeed(cl,
						 pl->listname[0] ? pl->listname : "dnsbl",
						 pl->answer);
				mydata->chito++;
			}
			else if (pl->state == OK)
				mydata->chitn++;
			else
				mydata->chitc++;
			return -1;
		}
		last = &(pl->next);
	}
	mydata->cmiss++;
	return 0;
}

static int dnsbl_ginit(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;
	int flags;

	if (!dnsbl_resolver_ready)
	{
		if (ircd_res_init() == 0)
			dnsbl_resolver_ready = 1;
		else
			return -1;
	}

	if (mydata->gfd > 0)
		return 0;

	mydata->gfd = socket(AFINET, SOCK_DGRAM, 0);
	if (mydata->gfd < 0)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_ginit: socket() failed: %s",
				  strerror(errno)));
		return -1;
	}
	flags = fcntl(mydata->gfd, F_GETFL, 0);
	if (flags >= 0)
		(void) fcntl(mydata->gfd, F_SETFL, flags | O_NONBLOCK);

	if (io_register_gfd(self, mydata->gfd, 0) < 0)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_ginit: io_register_gfd failed"));
		close(mydata->gfd);
		mydata->gfd = 0;
		return -1;
	}

	return 0;
}

static void dnsbl_grelease(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;
	u_int i;

	if (!mydata)
		return;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if (cldata[i].instance == self && (cldata[i].rfd > 0 || cldata[i].wfd > 0))
			dnsbl_close_client_rfd(i);
	}

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if (mydata->pend[i].active || mydata->pend[i].wake_w > 0)
			dnsbl_reset_pending(&mydata->pend[i]);
	}

	if (mydata->gfd > 0)
	{
		io_unregister_gfd(self);
		close(mydata->gfd);
		mydata->gfd = 0;
	}
}

static void dnsbl_complete(struct dnsbl_private *mydata, u_int cl, int final_state)
{
	DnsblPending *p = &mydata->pend[cl];
	ssize_t wr;

	if (!p->active)
		return;

	p->qid = 0;
	p->deadline = 0;
	p->final_state = final_state;
	if (p->wake_w > 0)
	{
		wr = write(p->wake_w, "D", 1);
		if (wr < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
			DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_complete(%u): wake write failed: %s",
				  cl, strerror(errno)));
	}

	dnsbl_dispatch_queued(mydata);
}

static int dnsbl_send_request(u_int cl, struct dnsbl_private *mydata)
{
	DnsblPending *p = &mydata->pend[cl];
	char packet[DNSBL_MAXPACKET];
	HEADER *hptr = (HEADER *) packet;
	int packet_len;
	int send_len;

	if (mydata->gfd <= 0 && dnsbl_ginit(cldata[cl].instance) < 0)
		return -1;

	while (p->current)
	{
		if (p->tries == 0)
		{
			if (dnsbl_build_lookup(cl, p->current, p->lookup, sizeof(p->lookup)) != 0)
			{
				DebugLog((ALOG_DNSBL, 0,
						  "dnsbl_send_request(%d): invalid lookup host=%s, skipping",
						  cl, p->current ? p->current->host : "?"));
				p->current = p->current->next;
				p->tries = 0;
				p->qid = 0;
				p->deadline = 0;
				p->lookup[0] = '\0';
				continue;
			}
		}

		if (p->tries >= (u_int) dnsbl_max_sends())
		{
			p->current = p->current->next;
			p->tries = 0;
			p->lookup[0] = '\0';
			continue;
		}

		p->ns_index = p->tries % dnsbl_ns_count();
		p->qid = dnsbl_new_qid(mydata);
		if (p->qid == 0)
			return -1;

		bzero(packet, sizeof(packet));
		packet_len = ircd_res_mkquery(QUERY, p->lookup, C_IN, T_A,
					 NULL, 0, NULL, (u_char *) packet, sizeof(packet));
		if (packet_len <= 0)
		{
			DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_send_request(%d): mkquery failed host=%s name=%s, skipping",
				  cl, p->current ? p->current->host : "?", p->lookup));
			p->current = p->current->next;
			p->tries = 0;
			p->qid = 0;
			p->deadline = 0;
			p->lookup[0] = '\0';
			continue;
		}
		hptr->id = htons(p->qid);

		send_len = sendto(mydata->gfd, packet, packet_len, 0,
				  (struct sockaddr *) &ircd_res.nsaddr_list[p->ns_index],
				  sizeof(ircd_res.nsaddr_list[p->ns_index]));
		p->tries++;
		if (send_len == packet_len)
		{
			p->deadline = time(NULL) + MAX(1, ircd_res.retrans);
			if (cldata[cl].timeout == 0 && cldata[cl].instance)
				cldata[cl].timeout = time(NULL) + cldata[cl].instance->timeout;
			DebugLog((ALOG_DNSBL, 0,
					  "dnsbl_send_request(%d): qid=%u ns=%u try=%u host=%s name=%s",
					  cl, p->qid, p->ns_index, p->tries,
					  p->current ? p->current->host : "?", p->lookup));
			return 0;
		}

		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_send_request(%d): sendto failed on ns=%u: %s",
				  cl, p->ns_index, strerror(errno)));
		p->qid = 0;
	}

	return -1;
}

static void dnsbl_dispatch_queued(struct dnsbl_private *mydata)
{
	int inflight;

	if (!mydata || mydata->gfd <= 0)
		return;

	inflight = dnsbl_count_inflight(mydata);
	while (inflight < dnsbl_max_active_queries())
	{
		u_int i;
		int best = -1;
		u_int best_seq = 0;

		for (i = 0; i < MAXCONNECTIONS; i++)
		{
			DnsblPending *p = &mydata->pend[i];

			if (!p->active || p->final_state != DNSBL_PENDING || p->qid != 0)
				continue;
			if (best < 0 || p->queue_seq < best_seq)
			{
				best = (int) i;
				best_seq = p->queue_seq;
			}
		}

		if (best < 0)
			break;

		if (dnsbl_send_request((u_int) best, mydata) == 0)
		{
			DebugLog((ALOG_DNSBL, 0,
					  "dnsbl_dispatch_queued: activated cl=%d inflight=%d/%d",
					  best, inflight + 1, dnsbl_max_active_queries()));
			inflight++;
			continue;
		}

		{
			DnsblPending *p = &mydata->pend[best];
			ssize_t wr = -1;

			DebugLog((ALOG_DNSBL, 0,
					  "dnsbl_dispatch_queued: activation failed for cl=%d", best));
			p->qid = 0;
			p->deadline = 0;
			p->final_state = DNSBL_DONE_FAILED;
			if (p->wake_w > 0)
			{
				wr = write(p->wake_w, "D", 1);
				if (wr < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
					DebugLog((ALOG_DNSBL, 0,
						  "dnsbl_dispatch_queued(%d): wake write failed: %s",
						  best, strerror(errno)));
			}
		}
	}
}

static int dnsbl_parse_reply(DnsblPending *p, const u_char *buf, size_t len,
				 const struct SOCKADDR_IN *src,
				 unsigned char *answer)
{
	const HEADER *hptr = (const HEADER *) buf;
	const u_char *cp = buf + HFIXEDSZ;
	const u_char *eob = buf + len;
	char qname[DNSBL_LOOKUPLEN];
	char rrname[DNSBL_LOOKUPLEN];
	u_int16_t id, qdcount, ancount, qtype, qclass;
	int n;

	if (len < HFIXEDSZ)
		return DNSBL_PARSE_IGNORE;

	id = ntohs(hptr->id);
	qdcount = ntohs(hptr->qdcount);
	ancount = ntohs(hptr->ancount);
	if (!hptr->qr || id != p->qid)
		return DNSBL_PARSE_IGNORE;
	if (!dnsbl_same_ns(src, &ircd_res.nsaddr_list[p->ns_index]))
		return DNSBL_PARSE_IGNORE;

	if (qdcount == 0)
		return DNSBL_PARSE_RETRY;

	n = ircd_dn_expand(buf, eob, cp, qname, sizeof(qname));
	if (n <= 0)
		return DNSBL_PARSE_RETRY;
	cp += n;
	if (cp + QFIXEDSZ > eob)
		return DNSBL_PARSE_RETRY;
	qtype = ircd_getshort(cp);
	cp += INT16SZ;
	qclass = ircd_getshort(cp);
	cp += INT16SZ;
	if (qtype != T_A || qclass != C_IN || strcasecmp(qname, p->lookup) != 0)
		return DNSBL_PARSE_IGNORE;

	while (--qdcount > 0)
	{
		n = __ircd_dn_skipname(cp, eob);
		if (n < 0)
			return DNSBL_PARSE_RETRY;
		cp += n;
		if (cp + QFIXEDSZ > eob)
			return DNSBL_PARSE_RETRY;
		cp += QFIXEDSZ;
	}

	if (hptr->tc)
		return DNSBL_PARSE_RETRY;
	if (hptr->rcode == NXDOMAIN)
		return DNSBL_PARSE_NEXT_HOST;
	if (hptr->rcode != NOERROR)
		return DNSBL_PARSE_RETRY;
	if (ancount == 0)
		return DNSBL_PARSE_NEXT_HOST;

	while (ancount-- > 0 && cp < eob)
	{
		u_int16_t type, class, dlen;

		n = ircd_dn_expand(buf, eob, cp, rrname, sizeof(rrname));
		if (n <= 0)
			return DNSBL_PARSE_RETRY;
		cp += n;
		if (cp + RRFIXEDSZ > eob)
			return DNSBL_PARSE_RETRY;
		type = ircd_getshort(cp);
		cp += INT16SZ;
		class = ircd_getshort(cp);
		cp += INT16SZ;
		cp += INT32SZ;
		dlen = ircd_getshort(cp);
		cp += INT16SZ;
		if (cp + dlen > eob)
			return DNSBL_PARSE_RETRY;
		if (type == T_A && class == C_IN && dlen == INADDRSZ)
		{
			memcpy(answer, cp, INADDRSZ);
			return DNSBL_PARSE_FOUND;
		}
		cp += dlen;
	}

	return DNSBL_PARSE_NEXT_HOST;
}

static int dnsbl_find_by_qid(struct dnsbl_private *mydata, u_short qid)
{
	u_int i;

	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if (mydata->pend[i].active && mydata->pend[i].qid == qid)
			return (int) i;
	}
	return -1;
}

static void dnsbl_gwork_one(struct dnsbl_private *mydata, const u_char *buf,
			 size_t len, const struct SOCKADDR_IN *src)
{
	const HEADER *hptr = (const HEADER *) buf;
	unsigned char answer[INADDRSZ];
	int cl;
	int action;
	DnsblPending *p;

	if (len < HFIXEDSZ)
		return;

	cl = dnsbl_find_by_qid(mydata, ntohs(hptr->id));
	if (cl < 0)
		return;

	p = &mydata->pend[cl];
	bzero(answer, sizeof(answer));
	action = dnsbl_parse_reply(p, buf, len, src, answer);
	if (action == DNSBL_PARSE_IGNORE)
		return;

	if (action == DNSBL_PARSE_FOUND)
	{
		memcpy(p->answer, answer, sizeof(p->answer));
		if (p->current && p->current->host)
		{
			strncpy(p->listname, p->current->host, sizeof(p->listname) - 1);
			p->listname[sizeof(p->listname) - 1] = '\0';
		}
		dnsbl_complete(mydata, (u_int) cl, DNSBL_DONE_FOUND);
		return;
	}

	p->qid = 0;
	p->deadline = 0;
	if (action == DNSBL_PARSE_NEXT_HOST)
	{
		p->current = p->current ? p->current->next : NULL;
		p->tries = 0;
		p->lookup[0] = '\0';
		if (dnsbl_send_request((u_int) cl, mydata) == 0)
			return;
		dnsbl_complete(mydata, (u_int) cl, DNSBL_DONE_CLEAN);
		return;
	}

	if (dnsbl_send_request((u_int) cl, mydata) == 0)
		return;
	dnsbl_complete(mydata, (u_int) cl, DNSBL_DONE_FAILED);
}

static char *dnsbl_init(AnInstance *self)
{
	struct dnsbl_private *mydata;
	struct dnsbl_list *l;
	char *tmpbuf = NULL, *s;
	char *txtbuf = NULL;
	size_t tmpbuf_size = 0, tmpbuf_used = 0;
	size_t txtbuf_size = 0, txtbuf_used = 0;

	if (self->opt == NULL)
		return "Aie! no option(s): nothing to be done!";

	mydata = (struct dnsbl_private *) malloc(sizeof(struct dnsbl_private));
	bzero((char *) mydata, sizeof(struct dnsbl_private));
	self->data = mydata;
	mydata->cache = NULL;
	mydata->host_list = NULL;
	mydata->lifetime = CACHETIME;
	mydata->gfd = 0;

	if (dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, "") < 0 ||
		dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, "") < 0)
	{
		dnsbl_free_private(mydata);
		self->data = NULL;
		return "Aie! out of memory";
	}
	if (self->opt)
	{
		char *opts = mystrdup(self->opt);
		char *last = NULL;
		char *tok;

		if (!opts)
		{
			dnsbl_free_private(mydata);
			self->data = NULL;
			return "Aie! out of memory";
		}
		for (tok = strtoken(&last, opts, ","); tok;
			 tok = strtoken(&last, NULL, ","))
		{
			char *eq;
			char *name;
			char *value = NULL;

			while (*tok && isspace((unsigned char) *tok))
				tok++;
			for (s = tok + strlen(tok); s > tok && isspace((unsigned char) s[-1]); )
				*--s = '\0';
			if (!*tok)
				continue;

			eq = strchr(tok, '=');
			if (eq)
			{
				*eq++ = '\0';
				value = eq;
				while (*value && isspace((unsigned char) *value))
					value++;
			}
			name = tok;

			if (dnsbl_opt_eq(name, "log"))
			{
				mydata->options |= OPT_LOG;
				dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",log");
				dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", Log");
			}
			else if (dnsbl_opt_eq(name, "reject"))
			{
				mydata->options |= OPT_DENY;
				dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",reject");
				dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", Reject");
			}
			else if (dnsbl_opt_eq(name, "paranoid"))
			{
				mydata->options |= OPT_PARANOID;
				dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",paranoid");
				dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", Paranoid");
			}
			else if (dnsbl_opt_startswith(name, "servers") && value)
			{
				char *servers = mystrdup(value);
				char *slast = NULL;
				char *name2;

				if (!servers)
					continue;
				for (name2 = strtoken(&slast, servers, ","); name2;
					 name2 = strtoken(&slast, NULL, ","))
				{
					while (*name2 && isspace((unsigned char) *name2))
						name2++;
					for (s = name2 + strlen(name2);
						 s > name2 && isspace((unsigned char) s[-1]); )
						*--s = '\0';
					if (*name2)
					{
						dnsbl_add_host(mydata, name2, NULL);
						sendto_log(ALOG_DNSBL, LOG_NOTICE,
						   "dnsbl_init: Added %s as dnsbl", name2);
					}
				}
				free(servers);
			}
			else if (dnsbl_opt_startswith(name, "cache") && value)
			{
				char *endp = NULL;
				unsigned long v;

				errno = 0;
				v = strtoul(value, &endp, 10);
				if (errno == 0 && endp && *endp == '\0' && v <= (unsigned long) (UINT_MAX / 60))
					mydata->lifetime = (u_int) v;
			}
		}
		free(opts);
	}

	if (mydata->options == 0)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_init: Aie! unknown option(s): nothing to be done!"));
		free(tmpbuf);
		free(txtbuf);
		dnsbl_free_private(mydata);
		self->data = NULL;
		return "Aie! unknown option(s): nothing to be done!";
	}


	if (self->module_kv)
	{
		aConfKV *kv = self->module_kv;
		while (kv)
		{
			if (!strcasecmp(kv->key, "entry"))
			{
				char *tmp = mystrdup(kv->value);
				char *sep = strchr(tmp, '|');
				char *host = tmp;
				char *rsn = NULL;
				char *e;

				while (*host && isspace(*host))
					host++;
				if (sep)
				{
					*sep++ = '\0';
					rsn = sep;
					while (*rsn && isspace(*rsn))
						rsn++;
					for (e = rsn + strlen(rsn); e > rsn && isspace((unsigned char)e[-1]); )
						*--e = '\0';
				}
				for (e = host + strlen(host); e > host && isspace((unsigned char)e[-1]); )
					*--e = '\0';
				if (*host)
				{
					dnsbl_add_host(mydata, host, rsn);
					sendto_log(ALOG_DNSBL, LOG_NOTICE,
					   "dnsbl_init: Added %s as dnsbl%s", host,
					   (rsn && *rsn) ? " with custom reason" : "");
				}
				free(tmp);
			}
			kv = kv->next;
		}
	}

	if (mydata->host_list == NULL)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_init: Aie! No DNSBL host: nothing to be done!"));
		free(tmpbuf);
		free(txtbuf);
		dnsbl_free_private(mydata);
		self->data = NULL;
		return "Aie! No DNSBL host: nothing to be done!";
	}

	dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",cache=%u", mydata->lifetime);
	dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", Cache %u (min)", mydata->lifetime);
	mydata->lifetime *= 60;
	dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",list=");
	dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", List(s): ");
	l = mydata->host_list;
	if (l)
	{
		dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, "%s", l->host);
		dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, "%s", l->host);
		for (l = l->next; l; l = l->next)
		{
			dnsbl_appendf(&tmpbuf, &tmpbuf_size, &tmpbuf_used, ",%s", l->host);
			dnsbl_appendf(&txtbuf, &txtbuf_size, &txtbuf_used, ", %s", l->host);
		}
	}

	if (self->reason)
		mydata->reason = mystrdup(self->reason);
	mydata->desc = txtbuf;
	self->popt = (tmpbuf && tmpbuf[0]) ? mystrdup(tmpbuf + 1) : mystrdup("");
	if (!self->popt)
		self->popt = mystrdup("");
	if (!mydata->desc)
		mydata->desc = mystrdup("");
	return (mydata->desc && mydata->desc[0]) ? mydata->desc + 2 : mydata->desc;
}

void dnsbl_release(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;

	dnsbl_grelease(self);
	dnsbl_free_private(mydata);
	self->data = NULL;
	free(self->popt);
	self->popt = NULL;
}

static void dnsbl_stats(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;

	sendto_ircd("S dnsbl verified %u rejected %u",
				mydata->total, mydata->rejects);
}

static int dnsbl_start(u_int cl)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	DnsblPending *p = &mydata->pend[cl];
	struct addrinfo hints, *addr_res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(cldata[cl].itsip, NULL, &hints, &addr_res))
	{
		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_start(%d): invalid address '%s', skipping ",
				  cl, cldata[cl].itsip));
		return -1;
	}
	freeaddrinfo(addr_res);

	if (cldata[cl].state & A_DENY)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_start(%d): A_DENY already set ", cl));
		return -1;
	}

	if (dnsbl_check_cache(cl))
		return -1;

	if (mydata->gfd <= 0 && dnsbl_ginit(cldata[cl].instance) < 0)
		return -1;

	if (p->active || p->wake_w > 0)
		dnsbl_reset_pending(p);
	if (cldata[cl].rfd > 0)
		dnsbl_close_client_rfd(cl);

	if (dnsbl_make_wakeup(cl, &p->wake_r, &p->wake_w) < 0)
		return -1;

	p->active = 1;
	p->final_state = DNSBL_PENDING;
	p->current = mydata->host_list;
	p->tries = 0;
	p->qid = 0;
	p->deadline = 0;
	p->lookup[0] = '\0';
	p->listname[0] = '\0';
	p->queue_seq = ++mydata->queue_seq;
	bzero(p->answer, sizeof(p->answer));
	mydata->total++;

	if (dnsbl_count_inflight(mydata) >= dnsbl_max_active_queries())
	{
		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_start(%d): queued (inflight=%d limit=%d)",
				  cl, dnsbl_count_inflight(mydata), dnsbl_max_active_queries()));
	}

	dnsbl_dispatch_queued(mydata);
	if (p->final_state != DNSBL_PENDING)
		return 0;
	if (p->qid != 0)
		return 0;

	// Keep the module timeout running even while the lookup is still queued.
	DebugLog((ALOG_DNSBL, 0,
		      "dnsbl_start(%d): waiting in queue (timeout_at=%ld)",
		      cl, (long) cldata[cl].timeout));
	return 0;
}

static int dnsbl_work(u_int cl)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	u_int owner;
	DnsblPending *p = dnsbl_pending_for_cl(mydata, cl, &owner);

	if (!p || !p->active)
	{
		dnsbl_close_client_rfd(cl);
		return -1;
	}

	if (p->final_state == DNSBL_PENDING)
		return 0;

	if (p->final_state == DNSBL_DONE_FOUND)
	{
		dnsbl_add_cache(cl, DNSBL_FOUND, p->listname, p->answer);
		dnsbl_succeed(cl, p->listname[0] ? p->listname : "dnsbl",
				     (char *) p->answer);
	}
	else if (p->final_state == DNSBL_DONE_CLEAN)
	{
		dnsbl_add_cache(cl, OK, NULL, NULL);
	}
	else
	{
		dnsbl_add_cache(cl, DNSBL_FAILED, NULL, NULL);
	}

	cldata[cl].timeout = 0;
	dnsbl_close_client_rfd(cl);
	dnsbl_reset_pending(&mydata->pend[owner]);
	return -1;
}

static void dnsbl_clean(u_int cl)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	u_int owner;
	DnsblPending *p = dnsbl_pending_for_cl(mydata, cl, &owner);

	DebugLog((ALOG_DNSBL, 0, "dnsbl_clean(%d): cleaning up", cl));
	cldata[cl].timeout = 0;
	dnsbl_close_client_rfd(cl);
	if (p && (p->active || p->wake_w > 0))
	{
		dnsbl_reset_pending(&mydata->pend[owner]);
		dnsbl_dispatch_queued(mydata);
	}
}

static int dnsbl_timeout(u_int cl)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	u_int owner;
	DnsblPending *p = dnsbl_pending_for_cl(mydata, cl, &owner);

	DebugLog((ALOG_DNSBL, 0, "dnsbl_timeout(%d)", cl));
	if (!p || !p->active)
		return -1;

	p->final_state = DNSBL_DONE_FAILED;
	return dnsbl_work(cl);
}

static int dnsbl_gwork(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;
	u_char buf[DNSBL_MAXPACKET];
	struct SOCKADDR_IN src;
	socklen_t slen;
	ssize_t n;
	int budget;

	if (!mydata || mydata->gfd <= 0)
		return 0;

	for (budget = DNSBL_GWORK_BUDGET; budget > 0; budget--)
	{
		slen = sizeof(src);
		n = recvfrom(mydata->gfd, buf, sizeof(buf), 0,
				 (struct sockaddr *) &src, &slen);
		if (n < 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				break;
			DebugLog((ALOG_DNSBL, 0, "dnsbl_gwork: recvfrom failed: %s",
					  strerror(errno)));
			break;
		}
		if (n == 0)
			break;

		dnsbl_gwork_one(mydata, buf, (size_t) n, &src);
	}
	if (budget == 0)
		DebugLog((ALOG_DNSBL, 0,
			  "dnsbl_gwork: budget exhausted, deferring remaining packets"));
	return 0;
}

static void dnsbl_gtick(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;
	time_t now = time(NULL);
	u_int cl;

	if (!mydata)
		return;
	if (mydata->gfd <= 0)
		(void) dnsbl_ginit(self);

	for (cl = 0; cl < MAXCONNECTIONS; cl++)
	{
		DnsblPending *p = &mydata->pend[cl];

		if (!p->active || p->final_state != DNSBL_PENDING || p->qid == 0)
			continue;
		if (p->deadline > now)
			continue;

		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_gtick: timeout cl=%u qid=%u host=%s try=%u",
				  cl, p->qid, p->current ? p->current->host : "?", p->tries));
		p->qid = 0;
		p->deadline = 0;
		if (dnsbl_send_request(cl, mydata) == 0)
			continue;
		dnsbl_complete(mydata, cl, DNSBL_DONE_FAILED);
	}

	dnsbl_dispatch_queued(mydata);
}

aModule Module_dnsbl = {
	"dnsbl", dnsbl_init, dnsbl_release, dnsbl_stats,
	dnsbl_start, dnsbl_work, dnsbl_timeout, dnsbl_clean,
	dnsbl_ginit, dnsbl_gtick, dnsbl_gwork, dnsbl_grelease
};
