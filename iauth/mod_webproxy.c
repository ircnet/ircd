/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_webproxy.c
 *   Copyright (C) 1998 Christophe Kalt
 *   Copyright (C) 2004 Piotr Kucharski
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
static const volatile char rcsid[] = "@(#)$Id: mod_webproxy.c,v 1.4 2005/01/27 19:17:44 chopin Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_PROXY_C
#include "a_externs.h"
#undef MOD_PROXY_C

/****************************** PRIVATE *************************************/

static	int	proxy_start(u_int cl);

#define CACHETIME 30
#define PROXYPORT (cldata[cl].instance->port)

struct proxylog
{
	struct proxylog *next;
	char ip[HOSTLEN+1];
	u_char state; /* 0 = no proxy, 1 = open proxy, 2 = closed proxy */
	time_t expire;
};

#define OPT_LOG     	0x001
#define OPT_DENY    	0x002
#define OPT_CAREFUL 	0x008

#define PROXY_NONE		0
#define PROXY_OPEN		1
#define PROXY_CLOSE		2

struct proxy_private
{
	struct proxylog *cache;
	u_int lifetime;
	u_char options;
	/* stats */
	u_int chitc, chito, chitn, cmiss, cnow, cmax;
	u_int noproxy, open, closed;
};

/*
 * proxy_open_proxy
 *
 *	Found an open proxy for cl: deal with it!
 */
static	void	proxy_open_proxy(int cl)
{
	struct proxy_private *mydata = cldata[cl].instance->data;
	char *reason = cldata[cl].instance->reason;

	if (!reason)
	{
		reason = "Denied access (insecure proxy found)";
	}
	/* open proxy */
	if (mydata->options & OPT_DENY)
	{
		cldata[cl].state |= A_DENY;
		sendto_ircd("k %d %s %u :%s", cl, cldata[cl].itsip,
			cldata[cl].itsport, reason);
	}
	if (mydata->options & OPT_LOG)
	{
		sendto_log(ALOG_FLOG, LOG_INFO,
			"webproxy%u: open proxy: %s[%s]",
			PROXYPORT, cldata[cl].host, cldata[cl].itsip);
	}
}

/*
 * proxy_add_cache
 *
 *	Add an entry to the cache.
 */
static	void	proxy_add_cache(int cl, int state)
{
	struct proxy_private *mydata = cldata[cl].instance->data;
	struct proxylog *next;

	if (state == PROXY_OPEN)
	{
		mydata->open += 1;
	}
	else if (state == PROXY_NONE)
	{
		mydata->noproxy += 1;
	}
	else
	{
		mydata->closed += 1;
	}

	if (mydata->lifetime == 0)
	{
		return;
	}

	mydata->cnow += 1;
	if (mydata->cnow > mydata->cmax)
	{
		mydata->cmax = mydata->cnow;
	}

	next = mydata->cache;
	mydata->cache = (struct proxylog *)malloc(sizeof(struct proxylog));
	mydata->cache->expire = time(NULL) + mydata->lifetime;
	strcpy(mydata->cache->ip, cldata[cl].itsip);
	mydata->cache->state = state;
	mydata->cache->next = next;
	DebugLog((ALOG_DSOCKSC, 0,
		"webproxy_add_cache(%d): new cache %s, open=%d",
		cl, mydata->cache->ip, state));
}

/*
 * proxy_check_cache
 *
 *	Check cache for an entry.
 */
static	int	proxy_check_cache(u_int cl)
{
	struct proxy_private *mydata = cldata[cl].instance->data;
	struct proxylog **last, *pl;
	time_t now = time(NULL);

	if (mydata->lifetime == 0)
	{
		return 0;
	}

	DebugLog((ALOG_DSOCKSC, 0,
		"webproxy_check_cache(%d): Checking cache for %s",
		cl, cldata[cl].itsip));

	last = &(mydata->cache);
	while ((pl = *last))
	{
		DebugLog((ALOG_DSOCKSC, 0, "webproxy_check_cache(%d): cache %s",
			cl, pl->ip));
		if (pl->expire < now)
		{
			DebugLog((ALOG_DSOCKSC, 0,
				"webproxy_check_cache(%d): free %s (%d < %d)",
				cl, pl->ip, pl->expire, now));
			*last = pl->next;
			free(pl);
			mydata->cnow -= 1;
			continue;
		}
		if (!strcasecmp(pl->ip, cldata[cl].itsip))
		{
			DebugLog((ALOG_DSOCKSC, 0,
				"webproxy_check_cache(%d): match (%u)",
				cl, pl->state));
			pl->expire = now + mydata->lifetime; /* dubious */
			if (pl->state == 1)
			{
				proxy_open_proxy(cl);
				mydata->chito += 1;
			}
			else if (pl->state == 0)
			{
				mydata->chitn += 1;
			}
			else
			{
				mydata->chitc += 1;
			}
			return -1;
		}
		last = &(pl->next);
	}
	mydata->cmiss += 1;
	return 0;
}

static	int	proxy_write(u_int cl)
{
	char query[128];	/* big enough to hold all queries */
	int query_len;		/* length of query */
#ifndef	INET6
	u_int a, b, c, d;
#else
	struct in6_addr	addr;
#endif

#ifndef	INET6
	if (sscanf(cldata[cl].ourip, "%u.%u.%u.%u", &a,&b,&c,&d) != 4)
#else
	if (inetpton(AF_INET6, cldata[cl].ourip, (void *) addr.s6_addr) != 1)
#endif
	{
		sendto_log(ALOG_DSOCKS|ALOG_IRCD, LOG_ERR,
			"webproxy_write(%d): "
#ifndef INET6
			"sscanf"
#else
			"inetpton"
#endif
			"(\"%s\") failed", cl, cldata[cl].ourip);
		close(cldata[cl].wfd);
		cldata[cl].wfd = 0;
		return -1;
	}
	query_len = sprintf(query, "CONNECT %s:%d HTTP/1.0\r\n\r\n",
		cldata[cl].ourip, cldata[cl].ourport);

	DebugLog((ALOG_DSOCKS, 0, "webproxy_write(%d): Checking %s %u",
		cl, cldata[cl].itsip, PROXYPORT));
	if (write(cldata[cl].wfd, query, query_len) != query_len)
	{
	/* most likely the connection failed */
		DebugLog((ALOG_DSOCKS, 0,
			"webproxy_write(%d): write() failed: %s",
			cl, strerror(errno)));
		proxy_add_cache(cl, PROXY_NONE);
		close(cldata[cl].wfd);
		cldata[cl].rfd = cldata[cl].wfd = 0;
		return 1;
	}
	cldata[cl].rfd = cldata[cl].wfd;
	cldata[cl].wfd = 0;
	return 0;
}

static	int	proxy_read(u_int cl)
{
	struct proxy_private *mydata = cldata[cl].instance->data;
	u_char state = PROXY_CLOSE;

	/* not enough data from the other end */
	if (cldata[cl].buflen <
		((mydata->options & OPT_CAREFUL) ? strConnLen + 
		/* strlen("HTTP/1.0 200 Connection established\n\n") = */ 38 :
			/* strlen("HTTP/1.0 200") = */ 12))
	{
		return 0;
	}

	/* got all we need */
	DebugLog((ALOG_DSOCKS, 0, "webproxy%u_read(%d): Got [%-64.64s]",
		PROXYPORT, cl, cldata[cl].inbuffer));

	if (mydata->options & OPT_CAREFUL)
	{
		/* strConn is welcome banner */
		if (strstr(cldata[cl].inbuffer, strConn))
		{
			state = PROXY_OPEN;
		}
	}
	else
	{
		/* little cheating to save on one strncmp for HTTP/1.1 */
		cldata[cl].inbuffer[7] = '0';
		/* Some Apache change "CONNECT" to "GET" and return
		 * "HTTP/1.0 200 OK" -- oops. Luckily they also return
		 * "Date:" header, let's hope it's in inbuffer already. --B. */
		if (!strstr(cldata[cl].inbuffer, "Date:")
			&& !strncmp(cldata[cl].inbuffer, "HTTP/1.0 200", 12))
		{
			state = PROXY_OPEN;
		}
	}

	/* Here state can be only OPEN, CLOSE or NONE */
	if (state == PROXY_OPEN)
	{
		proxy_open_proxy(cl);
	}
	proxy_add_cache(cl, state);
	close(cldata[cl].rfd);
	cldata[cl].rfd = 0;
	return -1;

}

/******************************** PUBLIC ************************************/

/*
 * proxy_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
static	char	*proxy_init(AnInstance *self)
{
	struct proxy_private *mydata;
	char tmpbuf[80], cbuf[32];
	static char txtbuf[80];
	char *ch;

	if (self->opt == NULL)
	{
		return "Aie! no option(s): nothing to be done!";
	}

	mydata = (struct proxy_private *) malloc(sizeof(struct proxy_private));
	bzero((char *) mydata, sizeof(struct proxy_private));
	mydata->cache = NULL;
	mydata->lifetime = CACHETIME;

	tmpbuf[0] = txtbuf[0] = '\0';

	/* for stats a */
	sprintf(tmpbuf, "port=%d", self->port);

	if (self->delayed)
	{
		strcat(tmpbuf, ",delayed");
		strcat(txtbuf, ", Delayed");
	}
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
	if (strstr(self->opt, "careful"))
	{
		mydata->options |= OPT_CAREFUL;
		strcat(tmpbuf, ",careful");
		strcat(txtbuf, ", Careful");
	}

	if (mydata->options == 0)
	{
		return "Aie! unknown option(s): nothing to be done!";
	}

	if ((ch = strstr(self->opt, "cache=")))
	{
		mydata->lifetime = atoi(ch+6);
	}
	sprintf(cbuf, ",cache=%d", mydata->lifetime);
	strcat(tmpbuf, cbuf);
	sprintf(cbuf, ", Cache %d (min)", mydata->lifetime);
	strcat(txtbuf, cbuf);
	mydata->lifetime *= 60;

	self->popt = mystrdup(tmpbuf);
	self->data = mydata;
	return txtbuf+2;
}

/*
 * proxy_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
static	void	proxy_release(AnInstance *self)
{
	struct proxy_private *mydata = self->data;

	free(mydata);
	free(self->popt);
}

/*
 * proxy_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
static	void	proxy_stats(AnInstance *self)
{
	struct proxy_private *mydata = self->data;

	sendto_ircd("S %s:%u open %u closed %u noproxy %u",
		self->mod->name, self->port,
		mydata->open, mydata->closed, mydata->noproxy);
	sendto_ircd("S %s:%u cache open %u closed %u noproxy %u miss %u"
		" (%u <= %u)", self->mod->name, self->port,
		mydata->chito, mydata->chitc, mydata->chitn,
		mydata->cmiss, mydata->cnow, mydata->cmax);
}

/*
 * proxy_start
 *
 *	This procedure is called to start the socks check procedure.
 *	Returns 0 if everything went fine,
 *	-1 otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. proxy_clean
 *	will NOT be called)
 */
static	int	proxy_start(u_int cl)
{
	char *error;
	int fd;

	if (cldata[cl].state & A_DENY)
	{
		/* no point of doing anything */
		DebugLog((ALOG_DSOCKS, 0,
			"webproxy_start(%d): A_DENY already set ", cl));
		return -1;
	}

	if (proxy_check_cache(cl))
	{
		return -1;
	}
	DebugLog((ALOG_DSOCKS, 0,
		"webproxy_start(%d): Connecting to %s", cl, cldata[cl].itsip));
	fd= tcp_connect(cldata[cl].ourip, cldata[cl].itsip, PROXYPORT, &error);
	if (fd < 0)
	{
		DebugLog((ALOG_DSOCKS, 0,
			"webproxy_start(%d): tcp_connect() reported %s",
			cl, error));
		proxy_add_cache(cl, PROXY_NONE);
		return -1;
	}

	/* so that proxy_work() is called when connected */
	cldata[cl].wfd = fd;

	return 0;
}

/*
 * proxy_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
static	int	proxy_work(u_int cl)
{

	DebugLog((ALOG_DSOCKS, 0,
		"webproxy_work(%d): %d %d buflen=%d",
		cl, cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));

	if (cldata[cl].wfd > 0)
	{
		/*
		** We haven't sent the query yet, the connection
		** was just established.
		*/
		return proxy_write(cl);
	}
	else
	{
		return proxy_read(cl);
	}
}

/*
 * proxy_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
static	void	proxy_clean(u_int cl)
{
	DebugLog((ALOG_DSOCKS, 0, "webproxy_clean(%d): cleaning up", cl));
	/*
	** only one of rfd and wfd may be set at the same time,
	** in any case, they would be the same fd, so only close() once
	*/
	if (cldata[cl].rfd)
	{
		close(cldata[cl].rfd);
	}
	else if (cldata[cl].wfd)
	{
		close(cldata[cl].wfd);
	}
	cldata[cl].rfd = cldata[cl].wfd = 0;
}

/*
 * proxy_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if check was aborted.
 */
static	int	proxy_timeout(u_int cl)
{
	DebugLog((ALOG_DSOCKS, 0,
		"webproxy_timeout(%d): calling proxy_clean ", cl));
	proxy_clean(cl);
	return -1;
}

aModule Module_webproxy =
	{ "webproxy", proxy_init, proxy_release, proxy_stats,
	  proxy_start, proxy_work, proxy_timeout, proxy_clean };

