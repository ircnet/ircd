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
static const volatile char rcsid[] = "@(#)$Id: mod_dnsbl.c,v 1.1 2024/10/1 15:35:00 patrick Exp $";
#endif

// clang-format off
// "os.h" must be included before "a_defines.h"
#include "os.h"
#include "a_defines.h"
// clang-format on
#define MOD_DNSBL_C
#include "a_externs.h"
#undef MOD_DNSBL_C

/****************************** PRIVATE *************************************/

/* Cache time in minutes */
#define CACHETIME 30

struct hostlog {
	struct hostlog *next;
	char ip[HOSTLEN + 1];
	u_char state; /* 0 = not found, 1 = found, 2 = timeout */
	time_t expire;
};

#define OPT_LOG 0x001
#define OPT_DENY 0x002
#define OPT_PARANOID 0x004

#define OK 0
#define DNSBL_FOUND 1
#define DNSBL_FAILED 2

struct dnsbl_list {
	char *host;
	struct dnsbl_list *next;
};

struct dnsbl_private {
	struct hostlog *cache;
	char *reason;
	u_int lifetime;
	u_char options;
	/* stats */
	u_int chitc, chito, chitn, cmiss, cnow, cmax;
	u_int found, failed, good, total, rejects;
	struct dnsbl_list *host_list;
};

/*
 * dnsbl_succeed
 *
 * Found a host in DNSBL. Deal with it.
 */
static void dnsbl_succeed(u_int cl, char *listname, char *result)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	char *reason = mydata->reason;

	if (mydata->options & OPT_PARANOID || (mydata->options & OPT_DENY &&
										   result[0] == '\177' && result[1] == '\0' &&
										   result[2] == '\0' /*  && result[3] == '\2' */))
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
}

/*
 * dnsbl_add_cache
 *
 * Add an entry to the cache.
 */
static void dnsbl_add_cache(u_int cl, u_int state)
{
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	struct hostlog *next;

	if (state == DNSBL_FOUND)
		mydata->found++;
	else if (state == DNSBL_FAILED)
		mydata->failed++;
	else /* state == OK */
		mydata->good++;

	if (mydata->lifetime == 0)
		return;

	mydata->cnow++;
	if (mydata->cnow > mydata->cmax)
		mydata->cmax = mydata->cnow;

	next = mydata->cache;
	mydata->cache = (struct hostlog *) malloc(sizeof(struct hostlog));
	mydata->cache->expire = time(NULL) + mydata->lifetime;
	strcpy(mydata->cache->ip, cldata[cl].itsip);
	mydata->cache->state = state;
	mydata->cache->next = next;
	DebugLog((ALOG_DNSBLC, 0,
			  "dnsbl_add_cache(%d): new cache %s, result=%d",
			  cl, mydata->cache->ip, state));
}

/*
 * dnsbl_check_cache
 *
 * Check cache for an entry.
 */
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
					  "dnsbl_check_cache(%d): free %s (%d < %d)",
					  cl, pl->ip, pl->expire, now));
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
			pl->expire = now + mydata->lifetime; /* dubious */
			if (pl->state == 1)
			{
				dnsbl_succeed(cl, "cached", "");
				mydata->chito++;
			}
			else if (pl->state == 0)
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

/******************************** PUBLIC ************************************/

/*
 * dnsbl_init
 *
 * This procedure is called when a particular module is loaded.
 * Returns NULL if everything went fine,
 * an error message otherwise.
 */
static char *dnsbl_init(AnInstance *self)
{
	struct dnsbl_private *mydata;
	struct dnsbl_list *l;
	char tmpbuf[255], cbuf[32], *s;
	static char txtbuf[255];

	if (self->opt == NULL)
		return "Aie! no option(s): nothing to be done!";

	mydata = (struct dnsbl_private *) malloc(sizeof(struct dnsbl_private));
	bzero((char *) mydata, sizeof(struct dnsbl_private));
	self->data = mydata;
	mydata->cache = NULL;
	mydata->host_list = NULL;
	mydata->lifetime = CACHETIME;

	tmpbuf[0] = txtbuf[0] = '\0';
	if (strstr(self->opt, "log"))
	{
		mydata->options |= OPT_LOG;
		strcat(tmpbuf, ",log");
		strcat(txtbuf, ", Log");
	}
	if (strstr(self->opt, "reject"))
	{
		mydata->options |= OPT_DENY;
		strcat(tmpbuf, ",reject");
		strcat(txtbuf, ", Reject");
	}
	if (strstr(self->opt, "paranoid"))
	{
		mydata->options |= OPT_PARANOID;
		strcat(tmpbuf, ",paranoid");
		strcat(txtbuf, ", Paranoid");
	}

	if (mydata->options == 0)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_init: Aie! unknown option(s): nothing to be done!"));
		return "Aie! unknown option(s): nothing to be done!";
	}

	if ((s = strstr(self->opt, "servers")))
	{
		char *ch = index(s, '=');

		if (++ch)
		{
			char *name, *last = NULL;
			for (name = strtoken(&last, ch, ","); name;
				 name = strtoken(&last, NULL, ","))
			{
				while (name && *name && isspace(*name))
					name++;
				if (!name || !*name)
					continue;
				l = (struct dnsbl_list *)
						malloc(sizeof(struct dnsbl_list));
				l->host = strdup(name);
				sendto_log(ALOG_DNSBL, LOG_NOTICE,
						   "dnsbl_init: Added %s as dnsbl", name);
				l->next = mydata->host_list;
				mydata->host_list = l;
			}
		}
	}

	if (mydata->host_list == NULL)
	{
		DebugLog((ALOG_DNSBL, 0, "dnsbl_init: Aie! No DNSBL host: nothing to be done!"));
		return "Aie! No DNSBL host: nothing to be done!";
	}

	if (strstr(self->opt, "cache"))
	{
		char *ch = index(self->opt, '=');

		if (ch)
			mydata->lifetime = atoi(++ch);
	}
	sprintf(cbuf, ",cache=%d", mydata->lifetime);
	strcat(tmpbuf, cbuf);
	sprintf(cbuf, ", Cache %d (min)", mydata->lifetime);
	strcat(txtbuf, cbuf);
	mydata->lifetime *= 60;
	strcat(tmpbuf, ",list=");
	strcat(txtbuf, ", List(s): ");
	l = mydata->host_list;
	strcat(tmpbuf, l->host);
	strcat(txtbuf, l->host);

	for (l = l->next; l; l = l->next)
	{
		strcat(tmpbuf, ",");
		strcat(txtbuf, ", ");
		strcat(tmpbuf, l->host);
		strcat(txtbuf, l->host);
	}

	if (self->reason)
	{
		mydata->reason = mystrdup(self->reason);
	}
	self->popt = strdup(tmpbuf + 1);
	return txtbuf + 2;
}

/*
 * dnsbl_release
 *
 * This procedure is called when a particular module is unloaded.
 */
void dnsbl_release(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;
	struct dnsbl_list *l, *n;

	for (l = mydata->host_list; l; l = n)
	{
		free(l->host);
		n = l->next;
		free(l);
	}

	free(mydata);
	free(self->popt);
}

/*
 * dnsbl_stats
 *
 * This procedure is called regularly to update statistics sent to ircd.
 */
static void dnsbl_stats(AnInstance *self)
{
	struct dnsbl_private *mydata = self->data;

	sendto_ircd("S dnsbl verified %u rejected %u",
				mydata->total, mydata->rejects);
}

/*
 * dnsbl_start
 *
 * This procedure is called to start the host check procedure.
 * Returns 0 if everything went fine,
 * -1 otherwise (nothing to be done, or failure)
 *
 * It is responsible for sending error messages where appropriate.
 * In case of failure, it's responsible for cleaning up (e.g. dnsbl_clean
 * will NOT be called)
 *
 * IPv4/IPv6 conversion has been taken from HOPM https://github.com/ircd-hybrid/hopm
 */
static int dnsbl_start(u_int cl)
{
	char lookup[128];
	struct dnsbl_private *mydata = cldata[cl].instance->data;
	struct dnsbl_list *l, *m;
	struct hostent *he = NULL;
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

	if (cldata[cl].state & A_DENY)
	{
		/* no point of doing anything */
		DebugLog((ALOG_DNSBL, 0, "dnsbl_start(%d): A_DENY already set ", cl));
		return -1;
	}

	if (dnsbl_check_cache(cl))
		return -1;

	DebugLog((ALOG_DNSBL, 0, "dnsbl_start(%d): checking %s", cl, cldata[cl].itsip));

	for (l = mydata->host_list; l && !he; l = l->next)
	{
		if (addr_res->ai_family == AF_INET)
		{
			const struct sockaddr_in *v4 = (const struct sockaddr_in *) addr_res->ai_addr;
			const uint8_t *b = (const uint8_t *) &v4->sin_addr.s_addr;

			snprintf(lookup, sizeof(lookup), "%u.%u.%u.%u.%s",
					 (unsigned int) (b[3]), (unsigned int) (b[2]),
					 (unsigned int) (b[1]), (unsigned int) (b[0]),
					 l->host);
		}
		else if (addr_res->ai_family == AF_INET6)
		{
			const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *) addr_res->ai_addr;
			const uint8_t *b = (const uint8_t *) &v6->sin6_addr.s6_addr;
			snprintf(lookup, sizeof(lookup),
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
			continue;

		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_start(%d): gethostbyname() for %s",
				  cl, lookup));
		he = gethostbyname(lookup);
		m = l;
	}
	mydata->total++;

	if (he)
		dnsbl_succeed(cl, m->host, he->h_addr_list[0]);
	else
	{
		DebugLog((ALOG_DNSBL, 0,
				  "dnsbl_start(%d): gethostbyname() reported %s",
				  cl, hstrerror(h_errno)));
		dnsbl_add_cache(cl, DNSBL_FAILED);
	}

	return -1;
}

/*
 * dnsbl_work
 *
 * This procedure is called whenever there's new data in the buffer.
 * Returns 0 if everything went fine, and there is more work to be done,
 * Returns -1 if the module has finished its work (and cleaned up).
 *
 * It is responsible for sending error messages where appropriate.
 */
static int dnsbl_work(u_int cl)
{
	/*
   ** There' nothing to do here
   */
	DebugLog((ALOG_DNSBL, 0,
			  "dnsbl_work(%d) invoked but why?", cl));
	return 0;
}

/*
 * dnsbl_clean
 *
 * This procedure is called whenever the module should interrupt its work.
 * It is responsible for cleaning up any allocated data, and in particular
 * closing file descriptors.
 */
static void dnsbl_clean(u_int cl)
{
	DebugLog((ALOG_DNSBL, 0, "dnsbl_clean(%d): cleaning up", cl));
	/*
	 * only one of rfd and wfd may be set at the same time,
	 * in any case, they would be the same fd, so only close() once
	 */
}

/*
 * dnsbl_timeout
 *
 * This procedure is called whenever the timeout set by the module is
 * reached.
 *
 * Returns 0 if things are okay, -1 if check was aborted.
 */
static int dnsbl_timeout(u_int cl)
{
	DebugLog((ALOG_DNSBL, 0, "dnsbl_timeout(%d): calling dnsbl_clean ", cl));
	dnsbl_clean(cl);
	return -1;
}

aModule Module_dnsbl = { "dnsbl", dnsbl_init, dnsbl_release, dnsbl_stats,
						 dnsbl_start, dnsbl_work, dnsbl_timeout, dnsbl_clean };