/************************************************************************
*   IRC - Internet Relay Chat, iauth/mod_external.c
*   Copyright (C) 2025 IRCnet.com team
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

/************************************************************************
*   iauth/mod_external.c
*
*  Sends to a TCP server:
*   CONN <sid> <id> <remote-ip> <host> <ident> <sasl-user> [<nick> <user1> <user2> <user3> :<realname>]\r\n
*
*   Receives:
*     ALLOW <id>\r\n
*     RAW   <id> :<msg>\r\n
*     DENY  <id> :<reason>\r\n
************************************************************************/

#include "os.h"
#include "a_defines.h"
#define MOD_EXTERNAL_C
#include "a_externs.h"
#undef MOD_EXTERNAL_C

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Configuration / State                                              */
/* ------------------------------------------------------------------ */

/* Allowlist of internal/private networks */
static const char *external_whitelist[] = {
	"127.0.0.0/8",
	"10.0.0.0/8",
	"172.16.0.0/12",
	"192.168.0.0/16",
	"::1/128",
	"fe80::/10",
	"fec0::/10",
	"fc00::/7",
	"ff00::/8",
	NULL
};

/* Returns 1 if the IP matches one of the internal networks. */
static int external_is_whitelisted_ip(const char *ip)
{
	const char **m;

	if (!ip || !*ip)
	{
		return 0;
	}
	for (m = external_whitelist; *m; ++m)
	{
		if (match_ipmask((char *) *m, (char *) ip) == 0)
		{
			return 1;
		}
	}
	return 0;
}

struct external_conf {
	char host[256];
	u_short port;
	u_int timeout;           /* seconds per client */
	u_char allow_on_timeout; /* 0=deny, 1=allow */
	char reason[128];
	char srcip[64]; /* optional bind() source IP */
};

struct external_stats {
	u_int connected, allowed, denied, timeouts, errors;
};

typedef struct
{
	int active;  /* 1 = request sent, reply pending */
	int wake_w;  /* write end of the wakeup socket */
	char result; /* 'A' allow, 'D' deny */
	char reason[128];

	int raw_pending;  /* 1 = RAW message pending */
	char rawmsg[256]; /* RAW text forwarded to ircd */
} ExternalPending;

#define EXTERNAL_OQ_MAX 2048 /* out-queue limit (AUTH/CONN lines) */

struct external_state {
	struct external_conf cfg;
	struct external_stats st;

	/* persistent connection */
	int gfd;               /* global socket to an external service (nonblocking) */
	time_t next_reconnect; /* earliest time for next connect attempt */

	/* output queue */
	char *outq[EXTERNAL_OQ_MAX];
	int qh, qt; /* ring-buffer head/tail */

	/* input buffer */
	char ibuf[8192];
	size_t ibuf_len;

	/* per-client pending state */
	ExternalPending pend[MAXCONNECTIONS];
};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void external_sanitize_ascii(char *s, size_t max)
{
	size_t i = 0, w = 0;
	for (; s[i] && w + 1 < max; ++i)
	{
		unsigned char c = (unsigned char) s[i];
		if (c == '\r' || c == '\n')
		{
			continue;
		}
		if (c < 0x20 || c > 0x7e)
		{
			continue;
		}
		s[w++] = (char) c;
	}
	s[w] = '\0';
}

static void external_safe_copy(char *dst, size_t dstsz, const char *src)
{
	if (!dst || dstsz == 0)
	{
		return;
	}
	if (!src)
	{
		dst[0] = '\0';
		return;
	}
	snprintf(dst, dstsz, "%s", src);
}

static const char *external_nonempty_or_asterisk(const char *s)
{
	return (s && *s) ? s : "*";
}

/* Local nonblocking helper. */
static void external_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		flags = 0;
	}
	(void) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void external_pend_clear(ExternalPending *p)
{
	if (!p)
	{
		return;
	}

	if (p->wake_w > 0)
	{
		DebugLog((ALOG_DMISC, 0,
				  "external: external_pend_clear: closing stale wake_w=%d "
				  "(active=%d, result=%c, reason=[%s])",
				  p->wake_w, p->active, p->result ? p->result : '-',
				  p->reason));
		close(p->wake_w);
		p->wake_w = 0;
	}
	p->active = 0;
	p->result = 0;
	p->reason[0] = '\0';

	p->raw_pending = 0;
	p->rawmsg[0] = '\0';
}

static void external_state_reset_io(struct external_state *S)
{
	int i;

	for (i = 0; i < EXTERNAL_OQ_MAX; ++i)
	{
		if (S->outq[i])
		{
			free(S->outq[i]);
			S->outq[i] = NULL;
		}
	}
	S->qh = S->qt = 0;
	S->ibuf_len = 0;
	S->ibuf[0] = '\0';
}

static void external_qpush(struct external_state *S, const char *line)
{
	int next = (S->qt + 1) % EXTERNAL_OQ_MAX;
	if (next == S->qh)
	{
		DebugLog((ALOG_DMISC, 0, "external: outq full, dropping AUTH"));
		S->st.errors += 1;
		return;
	}
	S->outq[S->qt] = strdup(line ? line : "");
	S->qt = next;
}

static int external_qpop(struct external_state *S, char **pline_out)
{
	if (S->qh == S->qt)
	{
		return 0;
	}
	if (pline_out)
	{
		*pline_out = S->outq[S->qh];
	}
	else
	{
		free(S->outq[S->qh]);
	}
	S->outq[S->qh] = NULL;
	S->qh = (S->qh + 1) % EXTERNAL_OQ_MAX;
	return 1;
}

/* Parse "ALLOW <id>" and return id (or -1). */
static int external_parse_allow(char *line)
{
	char *p = line + 6;
	int id;

	while (*p == ' ')
	{
		++p;
	}
	id = atoi(p);
	return id;
}

/* Parse "DENY <id> :<reason>" and return id (or -1). */
static int external_parse_deny(char *line, char *out_reason, size_t out_reason_sz)
{
	char *p = line + 4;
	int id;
	char *c;

	while (*p == ' ')
	{
		++p;
	}
	id = atoi(p);
	out_reason[0] = '\0';
	c = strchr(p, ':');
	if (c)
	{
		++c;
		while (*c == ' ')
		{
			++c;
		}
		external_safe_copy(out_reason, out_reason_sz, c);
		external_sanitize_ascii(out_reason, out_reason_sz);
	}
	return id;
}

/* Parse "RAW <id> :<reason>" and return id (or -1). */
static int external_parse_raw(char *line, char *out_reason, size_t out_reason_sz)
{
	char *p = line + 4;
	int id;
	char *c;

	while (*p == ' ')
	{
		++p;
	}

	id = atoi(p);
	out_reason[0] = '\0';
	c = strchr(p, ':');
	if (c)
	{
		++c;
		while (*c == ' ')
		{
			++c;
		}
		external_safe_copy(out_reason, out_reason_sz, c);
		external_sanitize_ascii(out_reason, out_reason_sz);
	}
	return id;
}

/* Create a wakeup socketpair so recv() works in the iauth loop. */
static int external_make_wakeup(u_int cl, int *wfd_out)
{
	int sp[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0)
	{
		return -1;
	}

	external_set_nonblock(sp[0]);
	external_set_nonblock(sp[1]);

	cldata[cl].rfd = sp[0];
	*wfd_out = sp[1];
	return 0;
}

/* Common tail: mark client processing finished and clean per-connection state.
 * Keep allow/deny-specific bits (A_DENY flag, stats, sendto_ircd) outside. */
static void external_common_finalize(u_int cl, struct external_state *S)
{
	cldata[cl].timeout = 0;

	if (cldata[cl].rfd > 0)
	{
		close(cldata[cl].rfd);
		cldata[cl].rfd = 0;
	}

	external_pend_clear(&S->pend[cl]);
}

static void external_finish_client_allow(u_int cl, struct external_state *S)
{
	S->st.allowed += 1;
	cldata[cl].state &= ~A_DENY;
	external_common_finalize(cl, S);
}


static void external_finish_client_deny(u_int cl, struct external_state *S)
{
	const char *reason =
			(S->pend[cl].reason[0] ? S->pend[cl].reason : S->cfg.reason);

	cldata[cl].state |= A_DENY;
	if (reason && *reason)
	{
		sendto_ircd("K %d %s %u :%s", cl, cldata[cl].itsip, cldata[cl].itsport,
					reason);
	}
	else
	{
		sendto_ircd("R %d %s %u ", cl, cldata[cl].itsip, cldata[cl].itsport);
		sendto_ircd("K %d %s %u ", cl, cldata[cl].itsip, cldata[cl].itsport);
	}

	S->st.denied += 1;
	external_common_finalize(cl, S);
}

static int external_connect_attempt(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;
	char *err = NULL;
	char *bind_ip = (S->cfg.srcip[0] ? S->cfg.srcip : NULL);
	time_t now = time(NULL);
	long remaining;

	/* Already connected. */
	if (S->gfd > 0)
	{
		return 0;
	}
	/* Backoff. */
	if (S->next_reconnect != 0 && now < S->next_reconnect)
	{
		remaining = (long) (S->next_reconnect - now);
		if (remaining < 0)
		{
			remaining = 0;
		}
		DebugLog((ALOG_DMISC, 0,
				  "external_connect_attempt: reconnect suppressed (%lds remaining)",
				  remaining));
		return -1;
	}

	S->ibuf_len = 0;
	S->ibuf[0] = '\0';

	S->gfd = tcp_connect(bind_ip, S->cfg.host, S->cfg.port, &err);
	if (S->gfd < 0)
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_connect_attempt: tcp_connect failed: %s",
				  err ? err : "(nil)"));

		S->next_reconnect = now + 5;
		S->st.errors += 1;
		return -1;
	}

	external_set_nonblock(S->gfd);

	if (io_register_gfd(self, S->gfd, 0) < 0)
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_connect_attempt: io_register_gfd failed"));
		close(S->gfd);
		S->gfd = 0;
		S->st.errors += 1;
		S->next_reconnect = now + 5;
		return -1;
	}

	if (S->qh != S->qt)
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_connect_attempt: outq not empty (qh=%d, qt=%d), enabling write on gfd=%d",
				  S->qh, S->qt, S->gfd));
		(void) io_update_gfd(self, S->gfd, 1);
	}

	S->next_reconnect = 0;

	DebugLog((ALOG_DMISC, 0,
			  "external_connect_attempt: connected to %s:%u (src=%s)",
			  S->cfg.host, S->cfg.port,
			  S->cfg.srcip[0] ? S->cfg.srcip : "auto"));
	return 0;
}

/* ------------------------------------------------------------------ */
/* Module API                                                         */
/* ------------------------------------------------------------------ */

static char *external_init(AnInstance *self)
{
	struct external_state *S = (struct external_state *) malloc(sizeof(*S));
	char *opts;
	char *save;
	char *tok;
	char vbuf[32];

	if (!S)
	{
		return "malloc failed";
	}
	bzero((char *) S, sizeof(*S));
	self->data = S;

	external_safe_copy(S->cfg.host, sizeof(S->cfg.host), "127.0.0.1");
	S->cfg.port = 9001;
	S->cfg.timeout = (self->timeout > 0) ? self->timeout : 1;
	S->cfg.allow_on_timeout = 0;
	external_safe_copy(S->cfg.reason, sizeof(S->cfg.reason),
					   "Denied access (policy)");
	S->gfd = 0;
	S->qh = S->qt = 0;
	S->ibuf_len = 0;

	if (self->reason && *self->reason)
	{
		external_safe_copy(S->cfg.reason, sizeof(S->cfg.reason), self->reason);
		external_sanitize_ascii(S->cfg.reason, sizeof(S->cfg.reason));
	}

	if (self->opt && *self->opt)
	{
		opts = strdup(self->opt);
		save = NULL;
		for (tok = strtok_r(opts, ", \t", &save); tok;
			 tok = strtok_r(NULL, ", \t", &save))
		{
			if (!strncasecmp(tok, "host=", 5))
			{
				external_safe_copy(S->cfg.host, sizeof(S->cfg.host), tok + 5);
			}
			else if (!strncasecmp(tok, "port=", 5))
			{
				S->cfg.port = (u_short) atoi(tok + 5);
			}
			else if (!strncasecmp(tok, "timeout=", 8))
			{
				S->cfg.timeout = (u_int) atoi(tok + 8);
			}
			else if (!strncasecmp(tok, "allow_on_timeout=", 17))
			{
				/* avoid const->char* warnings from mycmp/strcasecmp macro */
				external_safe_copy(vbuf, sizeof(vbuf), tok + 17);
				external_sanitize_ascii(vbuf, sizeof(vbuf));
				S->cfg.allow_on_timeout =
						(!strcasecmp(vbuf, "yes") || !strcasecmp(vbuf, "true") ||
						 atoi(vbuf) != 0)
								? 1
								: 0;
			}
			else if (!strncasecmp(tok, "srcip=", 6))
			{
				external_safe_copy(S->cfg.srcip, sizeof(S->cfg.srcip), tok + 6);
				external_sanitize_ascii(S->cfg.srcip, sizeof(S->cfg.srcip));
			}
		}
		free(opts);
	}
	if (S->cfg.timeout == 0)
	{
		S->cfg.timeout = 1;
	}

	DebugLog((ALOG_DMISC, 0,
			  "external_init: host=%s port=%u timeout=%u allow_on_timeout=%u "
			  "reason=[%s]",
			  S->cfg.host, S->cfg.port, S->cfg.timeout, S->cfg.allow_on_timeout,
			  S->cfg.reason));
	return NULL;
}

/* Initialize persistent state for the global connection. */
static int external_ginit(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;

	/* Do not reset state if already initialized. */
	if (S->gfd != 0 || S->next_reconnect != 0)
	{
		DebugLog((ALOG_DMISC, 0, "external_ginit: already initialized, skipping"));
		return 0;
	}

	/* First-time initialization only. */
	S->gfd = 0;
	S->next_reconnect = 0;
	S->ibuf_len = 0;
	S->ibuf[0] = '\0';

	DebugLog((ALOG_DMISC, 0,
			  "external_ginit: persistent state initialized"));
	return 0;
}

static int external_gwork(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;
	char *line;
	ssize_t w;
	int e;
	ssize_t r;
	char *start;
	char *eol;
	int id;
	char msg[256];
	char reason[128];
	size_t remain;

	DebugLog((ALOG_DMISC, 0,
			  "external_gwork: enter (gfd=%d, qh=%d, qt=%d, ibuf_len=%zu)",
			  S->gfd, S->qh, S->qt, S->ibuf_len));

	for (;;)
	{
		line = NULL;
		if (!external_qpop(S, &line))
		{
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: outq empty, nothing to send"));
			break;
		}

		DebugLog((ALOG_DMISC, 0,
				  "external_gwork: sending [%s] on gfd=%d",
				  line, S->gfd));

		w = write(S->gfd, line, (int) strlen(line));
		if (w < 0)
		{
			e = errno;
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: write() failed: %s (errno=%d)",
					  strerror(e), e));

			/*
             * On any write error (including EAGAIN/EWOULDBLOCK and hard errors),
             * put the line back into the output queue so it can be retried
             * after a later reconnect.
             */
			external_qpush(S, line);
			free(line);

			break;
		}
		else
		{
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: wrote %zd bytes", w));
		}
		free(line);
	}

	if (S->qh == S->qt)
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_gwork: outq drained, disabling write interest"));
		(void) io_update_gfd(self, S->gfd, 0);
	}
	else
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_gwork: outq not empty (qh=%d, qt=%d) – keep write interest",
				  S->qh, S->qt));
	}

	for (;;)
	{
		r = recv(S->gfd, S->ibuf + S->ibuf_len,
				 sizeof(S->ibuf) - 1 - S->ibuf_len, 0);
		if (r < 0)
		{
			e = errno;
			if (e == EAGAIN || e == EWOULDBLOCK)
			{
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: recv() would block (ibuf_len=%zu)",
						  S->ibuf_len));
				break;
			}
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: recv() failed: %s (errno=%d)",
					  strerror(e), e));
			r = 0;
		}

		if (r == 0)
		{
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: upstream closed (EOF), keeping pending "
					  "clients for timeout handling"));
			io_unregister_gfd(self);
			if (S->gfd > 0)
			{
				close(S->gfd);
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: closed gfd=%d after EOF/error", S->gfd));
				S->gfd = 0;
			}
			S->next_reconnect = time(NULL) + 5;
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: next_reconnect in 5s (time=%ld)",
					  (long) S->next_reconnect));
			break;
		}

		S->ibuf_len += (size_t) r;
		S->ibuf[S->ibuf_len] = '\0';

		DebugLog((ALOG_DMISC, 0,
				  "external_gwork: read %zd bytes, ibuf_len=%zu, buf=[%s]",
				  r, S->ibuf_len, S->ibuf));

		start = S->ibuf;
		for (;;)
		{
			eol = strstr(start, "\r\n");
			if (!eol)
			{
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: no complete line, leftover=[%s]", start));
				break;
			}
			*eol = '\0';

			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: got line from upstream: [%s]", start));

			if (!strncasecmp(start, "ALLOW ", 6))
			{
				id = external_parse_allow(start);
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: parsed ALLOW id=%d", id));

				if (id >= 0 && id < (int) MAXCONNECTIONS && S->pend[id].active)
				{
					S->pend[id].result = 'A';

					if (S->pend[id].wake_w > 0)
					{
						DebugLog((ALOG_DMISC, 0,
								  "external_gwork: waking client %d with 'A'",
								  id));
						(void) write(S->pend[id].wake_w, "A", 1);
					}
				}
				else
				{
					DebugLog((ALOG_DMISC, 0,
							  "external_gwork: ALLOW ignored: invalid/inactive id=%d "
							  "(active=%d, MAX=%d)",
							  id,
							  (id >= 0 && id < (int) MAXCONNECTIONS)
									  ? S->pend[id].active
									  : -1,
							  (int) MAXCONNECTIONS));
				}
			}
			else if (!strncasecmp(start, "RAW ", 4))
			{
				msg[0] = '\0';
				id = external_parse_raw(start, msg, sizeof(msg));

				if (id >= 0 && id < (int) MAXCONNECTIONS && S->pend[id].active)
				{
					S->pend[id].raw_pending = 1;
					external_safe_copy(S->pend[id].rawmsg,
									   sizeof(S->pend[id].rawmsg),
									   msg);

					if (S->pend[id].wake_w > 0)
					{
						(void) write(S->pend[id].wake_w, "R", 1);
					}
				}
			}
			else if (!strncasecmp(start, "DENY", 4))
			{
				reason[0] = '\0';
				id = external_parse_deny(start, reason, sizeof(reason));
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: parsed DENY id=%d reason=[%s]", id,
						  reason));
				if (id >= 0 && id < (int) MAXCONNECTIONS && S->pend[id].active)
				{
					S->pend[id].result = 'D';
					if (reason[0])
					{
						external_safe_copy(S->pend[id].reason,
										   sizeof(S->pend[id].reason), reason);
					}
					if (S->pend[id].wake_w > 0)
					{
						DebugLog((ALOG_DMISC, 0,
								  "external_gwork: waking client %d with 'D'",
								  id));
						(void) write(S->pend[id].wake_w, "D", 1);
					}
				}
				else
				{
					DebugLog((ALOG_DMISC, 0,
							  "external_gwork: DENY ignored: invalid/inactive id=%d "
							  "(active=%d, MAX=%d)",
							  id,
							  (id >= 0 && id < (int) MAXCONNECTIONS)
									  ? S->pend[id].active
									  : -1,
							  (int) MAXCONNECTIONS));
				}
			}
			else
			{
				DebugLog((ALOG_DMISC, 0,
						  "external_gwork: unknown upstream line, ignoring: [%s]",
						  start));
			}

			start = eol + 2;
		}

		remain = strlen(start);
		if (start != S->ibuf)
		{
			memmove(S->ibuf, start, remain + 1);
			DebugLog((ALOG_DMISC, 0,
					  "external_gwork: compacted buffer, remain=%zu, buf=[%s]",
					  remain, S->ibuf));
		}

		S->ibuf_len = remain;
	}

	DebugLog((ALOG_DMISC, 0,
			  "external_gwork: leave (gfd=%d, qh=%d, qt=%d, ibuf_len=%zu)",
			  S->gfd, S->qh, S->qt, S->ibuf_len));
	return 0;
}

static void external_gtick(AnInstance *self)
{
	struct external_state *S;

	DebugLog((ALOG_DMISC, 0,
			  "external: tick"));

	S = (struct external_state *) self->data;

	if (S->gfd > 0)
	{
		return;
	}
	(void) external_connect_attempt(self);
}

static void external_grelease(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;
	u_int cl;

	if (!S)
	{
		return;
	}

	io_unregister_gfd(self);

	if (S->gfd > 0)
	{
		close(S->gfd);
		S->gfd = 0;
	}

	external_state_reset_io(S);

	for (cl = 0; cl < MAXCONNECTIONS; ++cl)
	{
		if (S->pend[cl].active)
		{
			external_pend_clear(&S->pend[cl]);
		}
	}
}

static void external_release(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;
	if (!S)
	{
		return;
	}
	external_grelease(self);
	free(S);
	self->data = NULL;
}

static void external_stats(AnInstance *self)
{
	struct external_state *S = (struct external_state *) self->data;
	sendto_ircd("S external connected %u allowed %u denied %u timeouts %u "
				"errors %u",
				S->st.connected, S->st.allowed, S->st.denied, S->st.timeouts,
				S->st.errors);
}

/* ------------------------------------------------------------------ */
/* Per-client hooks                                                     */
/* ------------------------------------------------------------------ */

static int external_start(u_int cl)
{
	struct external_state *S = (struct external_state *) cldata[cl].instance->data;
	int rc;
	char line[BUFSIZ];

	DebugLog((ALOG_DMISC, 0,
			  "external_start(%u): gfd=%d active=%d itsip=%s ourip=%s", cl,
			  S->gfd, S->pend[cl].active, cldata[cl].itsip, cldata[cl].ourip));

	if (external_is_whitelisted_ip(cldata[cl].itsip))
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_start(%u): %s matches local allowlist, "
				  "skipping upstream auth",
				  cl,
				  cldata[cl].itsip));

		external_finish_client_allow(cl, S);
		return -1;
	}

	if (S->gfd <= 0)
	{
		rc = external_connect_attempt(cldata[cl].instance);
		DebugLog((ALOG_DMISC, 0,
				  "external_start(%u): connect_attempt -> %d, gfd=%d", cl, rc,
				  S->gfd));
	}

	if (S->pend[cl].active ||
		S->pend[cl].wake_w > 0 ||
		S->pend[cl].result != 0 ||
		S->pend[cl].reason[0] != '\0')
	{
		DebugLog((ALOG_DMISC, 0,
				  "external_start(%u): clearing stale pending state "
				  "(active=%d, wake_w=%d, result=%c, reason=[%s])",
				  cl, S->pend[cl].active, S->pend[cl].wake_w,
				  S->pend[cl].result ? S->pend[cl].result : '-',
				  S->pend[cl].reason));
	}
	external_pend_clear(&S->pend[cl]);

	if (external_make_wakeup(cl, &S->pend[cl].wake_w) < 0)
	{
		if (!S->cfg.allow_on_timeout)
		{
			cldata[cl].state |= A_DENY;
			sendto_ircd("K %d %s %u %s", cl, cldata[cl].itsip,
						cldata[cl].itsport, S->cfg.reason);
		}
		return -1;
	}
	if (*cldata[cl].nick && *cldata[cl].user1 && *cldata[cl].user2 &&
		*cldata[cl].user3 && *cldata[cl].realname)
	{
		snprintf(line, sizeof(line), "CONN %s %u %s %s %s %s %s %s %s %s :%s\r\n",
				 iauth_ircd_sid(), cl, cldata[cl].itsip,
				 external_nonempty_or_asterisk(cldata[cl].host),
				 external_nonempty_or_asterisk(cldata[cl].authuser),
				 external_nonempty_or_asterisk(cldata[cl].sasl_user),
				 cldata[cl].nick, cldata[cl].user1,
				 cldata[cl].user2, cldata[cl].user3, cldata[cl].realname);
	}
	else
	{
		snprintf(line, sizeof(line), "CONN %s %u %s %s %s %s\r\n",
				 iauth_ircd_sid(), cl, cldata[cl].itsip,
				 external_nonempty_or_asterisk(cldata[cl].host),
				 external_nonempty_or_asterisk(cldata[cl].authuser),
				 external_nonempty_or_asterisk(cldata[cl].sasl_user));
	}

	DebugLog((ALOG_DMISC, 0, "external_start(%u): queueing line=[%s]", cl,
			  line));

	if (S->qh == S->qt && S->gfd > 0)
	{
		(void) io_update_gfd(cldata[cl].instance, S->gfd, 1);
	}
	external_qpush(S, line);

	S->pend[cl].active = 1;
	S->pend[cl].result = 0;
	S->pend[cl].reason[0] = '\0';
	S->pend[cl].raw_pending = 0;
	S->pend[cl].rawmsg[0] = '\0';

	cldata[cl].timeout = time(NULL) + S->cfg.timeout;
	S->st.connected += 1;
	return 0;
}

static int external_work(u_int cl)
{
	struct external_state *S = (struct external_state *) cldata[cl].instance->data;
	const char *msg;

	if (!S->pend[cl].active)
	{
		if (cldata[cl].rfd > 0)
		{
			close(cldata[cl].rfd);
			cldata[cl].rfd = 0;
		}
		return -1;
	}

	if (S->pend[cl].raw_pending)
	{
		msg = S->pend[cl].rawmsg[0] ? S->pend[cl].rawmsg : "";
		if (*msg)
		{
			sendto_ircd("R %d %s %u :%s", cl, cldata[cl].itsip, cldata[cl].itsport, msg);
		}

		S->pend[cl].raw_pending = 0;
		S->pend[cl].rawmsg[0] = '\0';

		if (S->pend[cl].result == 0)
		{
			return 0;
		}
	}

	if (S->pend[cl].result == 'A')
	{
		external_finish_client_allow(cl, S);
	}
	else if (S->pend[cl].result == 'D')
	{
		external_finish_client_deny(cl, S);
	}
	else
	{
		return 0;
	}

	return -1;
}

static int external_timeout(u_int cl)
{
	struct external_state *S = (struct external_state *) cldata[cl].instance->data;

	DebugLog((ALOG_DMISC, 0, "external_timeout(%d): policy=%s", cl,
			  S->cfg.allow_on_timeout ? "allow" : "deny"));

	if (S->pend[cl].active)
	{
		S->st.timeouts += 1;

		if (S->cfg.allow_on_timeout)
		{
			external_finish_client_allow(cl, S);
		}
		else
		{
			external_finish_client_deny(cl, S);
		}
	}
	return -1;
}

static void external_clean(u_int cl)
{
	struct external_state *S = (struct external_state *) cldata[cl].instance->data;

	if (cldata[cl].rfd > 0)
	{
		close(cldata[cl].rfd);
		cldata[cl].rfd = 0;
	}
	if (S)
	{
		external_pend_clear(&S->pend[cl]);
	}

	DebugLog((ALOG_DMISC, 0, "external_clean(%d)", cl));
}

/* ------------------------------------------------------------------ */
/* Export                                                             */
/* ------------------------------------------------------------------ */

aModule Module_external = {
	"external", external_init, external_release, external_stats,
	external_start, external_work, external_timeout, external_clean,
	external_ginit, external_gtick, external_gwork, external_grelease
};
