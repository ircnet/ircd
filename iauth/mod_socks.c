/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_socks.c
 *   Copyright (C) 1998 Christophe Kalt
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
static const volatile char rcsid[] = "@(#)$Id: mod_socks.c,v 1.43 2004/10/03 17:13:42 chopin Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_SOCKS_C
#include "a_externs.h"
#undef MOD_SOCKS_C

/****************************** PRIVATE *************************************/

static	int	socks_start(u_int cl);

#define CACHETIME 30
#define SOCKSPORT (cldata[cl].instance->port)

struct proxylog
{
	struct proxylog *next;
	char ip[HOSTLEN+1];
	u_char state; /* 0 = no proxy, 1 = open proxy, 2 = closed proxy */
	time_t expire;
};

#define OPT_LOG     	0x001
#define OPT_DENY    	0x002
#define OPT_PARANOID	0x004
#define OPT_CAREFUL 	0x008
#define OPT_V4ONLY  	0x010
#define OPT_V5ONLY  	0x020
#define OPT_PROTOCOL	0x040
#define OPT_BOFH    	0x080

#define PROXY_NONE		0
#define PROXY_OPEN		1
#define PROXY_CLOSE		2
#define PROXY_UNEXPECTED	3
#define PROXY_BADPROTO		4

#define ST_V4	0x01
#define ST_V5	0x02
#define ST_V5b	0x04

struct socks_private
{
	struct proxylog *cache;
	u_int lifetime;
	u_char options;
	/* stats */
	u_int chitc, chito, chitn, cmiss, cnow, cmax;
	u_int noproxy, open, closed;
};

/*
 * socks_open_proxy
 *
 *	Found an open proxy for cl: deal with it!
 */
static	void	socks_open_proxy(int cl, char *strver)
{
	struct socks_private *mydata = cldata[cl].instance->data;
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
			"socks%s: open proxy: %s[%s]",
			strver, cldata[cl].host, cldata[cl].itsip);
	}
}

/*
 * socks_add_cache
 *
 *	Add an entry to the cache.
 */
static	void	socks_add_cache(int cl, int state)
{
	struct socks_private *mydata = cldata[cl].instance->data;
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
		"socks_add_cache(%d): new cache %s, open=%d",
		cl, mydata->cache->ip, state));
}

/*
 * socks_check_cache
 *
 *	Check cache for an entry.
 */
static	int	socks_check_cache(u_int cl)
{
	struct socks_private *mydata = cldata[cl].instance->data;
	struct proxylog **last, *pl;
	time_t now = time(NULL);

	if (mydata->lifetime == 0)
	{
		return 0;
	}

	DebugLog((ALOG_DSOCKSC, 0,
		"socks_check_cache(%d): Checking cache for %s",
		cl, cldata[cl].itsip));

	last = &(mydata->cache);
	while ((pl = *last))
	{
		DebugLog((ALOG_DSOCKSC, 0, "socks_check_cache(%d): cache %s",
			cl, pl->ip));
		if (pl->expire < now)
		{
			DebugLog((ALOG_DSOCKSC, 0,
				"socks_check_cache(%d): free %s (%d < %d)",
				cl, pl->ip, pl->expire, now));
			*last = pl->next;
			free(pl);
			mydata->cnow -= 1;
			continue;
		}
		if (!strcasecmp(pl->ip, cldata[cl].itsip))
		{
			DebugLog((ALOG_DSOCKSC, 0,
				"socks_check_cache(%d): match (%u)",
				cl, pl->state));
			pl->expire = now + mydata->lifetime; /* dubious */
			if (pl->state == 1)
			{
				socks_open_proxy(cl, "C");
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

static	int	socks_write(u_int cl, char *strver)
{
	u_char query[128];	/* big enough to hold all queries */
	int query_len;  	/* length of query */
#ifndef	INET6
	u_int a, b, c, d;
#else
	struct in6_addr	addr;
	struct socks_private *mydata = cldata[cl].instance->data;
#endif

#ifndef	INET6
	if (sscanf(cldata[cl].ourip, "%u.%u.%u.%u", &a,&b,&c,&d) != 4)
#else
	if (inetpton(AF_INET6, cldata[cl].ourip, (void *) addr.s6_addr) != 1)
#endif
	{
		sendto_log(ALOG_DSOCKS|ALOG_IRCD, LOG_ERR,
			"socks_write%s(%d): "
#ifndef INET6
			"sscanf"
#else
			"inetpton"
#endif
			"(\"%s\") failed", strver, cl, cldata[cl].ourip);
		close(cldata[cl].wfd);
		cldata[cl].wfd = 0;
		return -1;
	}
#ifdef INET6
	/*
	 * socks4 does not support ipv6, so we switch to socks5, if
	 * address is not ipv4 mapped in ipv6
	 */
	if (cldata[cl].mod_status == ST_V4 && !IN6_IS_ADDR_V4MAPPED(&addr))
	{
		if (mydata->options & OPT_V4ONLY)
		{
			/* we cannot do work! */
			sendto_log(ALOG_DSOCKS|ALOG_IRCD, LOG_WARNING,
				"socks4 does not work on ipv6");
			close(cldata[cl].wfd);
			cldata[cl].wfd = 0;
			return -1;
		}
		else
		{
			cldata[cl].mod_status = ST_V5;
		}
	}
#endif
	if (cldata[cl].mod_status == ST_V4)
	{
		query[0] = 4; query[1] = 1;
		query[2] = ((cldata[cl].ourport & 0xff00) >> 8);
		query[3] = (cldata[cl].ourport & 0x00ff);
#ifndef	INET6
		query[4] = a; query[5] = b; query[6] = c; query[7] = d;
#else
		/* socks v4 only supports IPv4,
		 * so it must be a ipv4 mapped ipv6.
		 * Just copy the ipv4 portion.
		 */
		memcpy(query + 4, ((char *)addr.s6_addr) + 12, 4);
#endif
		query[8] = 'u'; query[9] = 's';
		query[10] = 'e'; query[11] = 'r';
		query[12] = 0;
		query_len = 13;
	}
	else 
	{
		query[0] = 5; query[1] = 1; query[2] = 0;
		query_len = 3;
		if (cldata[cl].mod_status == ST_V5b)
		{
#ifndef	INET6
			query_len = 10;
			query[3] = 1;
			query[4] = a; query[5] = b; query[6] = c; query[7] = d;
			query[8] = ((cldata[cl].ourport & 0xff00) >>8);
			query[9] = (cldata[cl].ourport & 0x00ff);
#else
			if (IN6_IS_ADDR_V4MAPPED(&addr))
			{
				query_len = 10;
				query[3] = 1;	/* ipv4 address */
				memcpy(query + 4,
					((char *)addr.s6_addr) + 12, 4);
				query[8] = ((cldata[cl].ourport & 0xff00) >>8);
				query[9] = (cldata[cl].ourport & 0x00ff);
			}
			else
			{
				query_len = 22;
				query[3] = 4;
				memcpy(query + 4, addr.s6_addr, 16);
				query[20] = ((cldata[cl].ourport & 0xff00) >>8);
				query[21] = (cldata[cl].ourport & 0x00ff);
			}
#endif
		}
	}

	DebugLog((ALOG_DSOCKS, 0, "socks%s_write(%d): Checking %s %u",
		strver, cl, cldata[cl].ourip, SOCKSPORT));
	if (write(cldata[cl].wfd, query, query_len) != query_len)
	{
	/* most likely the connection failed */
		DebugLog((ALOG_DSOCKS, 0,
			"socks%s_write(%d): write() failed: %s",
			strver, cl, strerror(errno)));
		socks_add_cache(cl, PROXY_NONE);
		close(cldata[cl].wfd);
		cldata[cl].rfd = cldata[cl].wfd = 0;
		return 1;
	}
	cldata[cl].rfd = cldata[cl].wfd;
	cldata[cl].wfd = 0;
	return 0;
}

static	int	socks_read(u_int cl, char *strver)
{
	struct socks_private *mydata = cldata[cl].instance->data;
	u_char state = PROXY_CLOSE;

	/* not enough data from the other end */
	if (cldata[cl].buflen < 2)
	{
		return 0;
	}

	/* got all we need */
	DebugLog((ALOG_DSOCKS, 0, "socks%s_read(%d): Got [%d %d]", strver, cl,
		cldata[cl].inbuffer[0], cldata[cl].inbuffer[1]));

	if (cldata[cl].mod_status == ST_V4)
	{
		if (cldata[cl].inbuffer[0] == 0
/* 
   A lot of socks v4 proxies return 4,91 instead of 0,91 otherwise
   working perfectly -- this will deal with them.
*/
#define BROKEN_PROXIES
#ifdef BROKEN_PROXIES
			|| cldata[cl].inbuffer[0] == 4
#endif
			)
		{
			if (cldata[cl].inbuffer[1] < 90 ||
				cldata[cl].inbuffer[1] > 93)
			{
				state = PROXY_UNEXPECTED;
			}
			else
			{
				if (cldata[cl].inbuffer[1] == 90)
				{
					state = PROXY_OPEN;
				}
				else if ((mydata->options & OPT_PARANOID) &&
					cldata[cl].inbuffer[1] != 91)
				{
					state = PROXY_OPEN;
				}
			}
		}
		else
		{
			state = PROXY_BADPROTO;
		}
	}
	else /* ST_V5 or ST_V5b */
	{
		if (cldata[cl].inbuffer[0] == 5)
		{
			if (cldata[cl].inbuffer[1] == 0)
			{
				state = PROXY_OPEN;
			}
			else
			{
				if (cldata[cl].mod_status == ST_V5)
				{
					if ((u_char)cldata[cl].inbuffer[1] == 4 ||
						((u_char)cldata[cl].inbuffer[1] > 9 &&
						(u_char)cldata[cl].inbuffer[1] != 255))
					{
						state = PROXY_UNEXPECTED;
					}
				}
				else /* ST_V5b */
				{
					if ((u_char) cldata[cl].inbuffer[1] > 8)
					{
						state = PROXY_UNEXPECTED;
					}
					else if ((mydata->options&OPT_PARANOID) &&
						cldata[cl].inbuffer[1] != 2)
					{
						state = PROXY_OPEN;
					}
				}
			}
		}
		else
		{
			state = PROXY_BADPROTO;
		}
	}

	if (cldata[cl].mod_status == ST_V4)
	{
		/* we just checked socks 4 */
		if (!(mydata->options & OPT_V4ONLY) && state != PROXY_OPEN)
		{
			/* if we're not configured to do only v4
			   and proxy state was not OPEN, try v5 */
			cldata[cl].mod_status = ST_V5;
			cldata[cl].buflen = 0;
			close(cldata[cl].rfd);
			cldata[cl].rfd = 0;
			goto again;
		}
	}
	else if (cldata[cl].mod_status == ST_V5)
	{
		/* we just checked socks 5 */
		if (state == PROXY_OPEN && mydata->options & OPT_CAREFUL)
		{
			/* we found socks 5 OPEN, but (option says so)
			   we will double check in second stage */
			cldata[cl].mod_status = ST_V5b;
			cldata[cl].buflen = 0;
			cldata[cl].wfd = cldata[cl].rfd;
			cldata[cl].rfd = 0;
			goto again;
		}
	}
	else	/* ST_V5b */
	{
		/* we just checked second phase of socks 5b.
		   nothing left to do. */
	}

	if (state == PROXY_UNEXPECTED)
	{
		sendto_log(ALOG_FLOG, LOG_WARNING,
			"socks%s: unexpected reply: %u,%u %s[%s]", strver,
			cldata[cl].inbuffer[0], cldata[cl].inbuffer[1],
			cldata[cl].host, cldata[cl].itsip);
		sendto_log(ALOG_IRCD, 0, "socks%s: unexpected reply: %u,%u",
			strver, cldata[cl].inbuffer[0],
			cldata[cl].inbuffer[1]);
		/* oh well. unexpected response can mean anything.
		   so if we're megaparanoid, we assume it's open proxy */
		state = mydata->options & OPT_BOFH ? PROXY_OPEN : PROXY_CLOSE;
	}
	else if (state == PROXY_BADPROTO)
	{
		if (mydata->options & OPT_PROTOCOL)
		{
			sendto_log(ALOG_FLOG, LOG_WARNING,
				"socks%s: protocol error: %u,%u %s[%s]",
				strver,
				cldata[cl].inbuffer[0], cldata[cl].inbuffer[1],
				cldata[cl].host, cldata[cl].itsip);
			sendto_log(ALOG_IRCD, 0,
				"socks%s: protocol error: %u,%u",
				strver, cldata[cl].inbuffer[0],
				cldata[cl].inbuffer[1]);
		}
		/* oh well. protocol error can mean anything.
		   so if we're megaparanoid, we assume it's open proxy */
		state = mydata->options & OPT_BOFH ? PROXY_OPEN : PROXY_CLOSE;
	}

	/* We're past checking of socks 4, socks 5 and even socks 5b,
	   if it was needed. Now deal with final state */

	/* Here state can be only OPEN, CLOSE or NONE */
	if (state == PROXY_OPEN)
	{
		socks_open_proxy(cl, strver);
	}
	socks_add_cache(cl, state);
	close(cldata[cl].rfd);
	cldata[cl].rfd = 0;
	return -1;

again:
	if (cldata[cl].mod_status != ST_V5b)
	{
		return socks_start(cl);
	}

	return 0;
}

/******************************** PUBLIC ************************************/

/*
 * socks_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
static	char	*socks_init(AnInstance *self)
{
	struct socks_private *mydata;
	char tmpbuf[80], cbuf[32];
	static char txtbuf[80];

	if (self->opt == NULL)
	{
		return "Aie! no option(s): nothing to be done!";
	}

	mydata = (struct socks_private *) malloc(sizeof(struct socks_private));
	bzero((char *) mydata, sizeof(struct socks_private));
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
	if (strstr(self->opt, "megaparanoid"))
	{
		mydata->options |= OPT_PARANOID|OPT_BOFH;
		strcat(tmpbuf, ",megaparanoid");
		strcat(txtbuf, ", Megaparanoid");
	}
	else if (strstr(self->opt, "paranoid"))
	{
		mydata->options |= OPT_PARANOID;
		strcat(tmpbuf, ",paranoid");
		strcat(txtbuf, ", Paranoid");
	}
	if (strstr(self->opt, "careful"))
	{
		mydata->options |= OPT_CAREFUL;
		strcat(tmpbuf, ",careful");
		strcat(txtbuf, ", Careful");
	}
	if (strstr(self->opt, "v4only"))
	{
		mydata->options |= OPT_V4ONLY;
		strcat(tmpbuf, ",v4only");
		strcat(txtbuf, ", V4only");
	}
	if (strstr(self->opt, "v5only"))
	{
		mydata->options |= OPT_V5ONLY;
		strcat(tmpbuf, ",v5only");
		strcat(txtbuf, ", V5only");
	}
	if (strstr(self->opt, "protocol"))
	{
		mydata->options |= OPT_PROTOCOL;
		strcat(tmpbuf, ",protocol");
		strcat(txtbuf, ", Protocol");
	}

	if (mydata->options == 0)
	{
		return "Aie! unknown option(s): nothing to be done!";
	}

	if (strstr(self->opt, "cache"))
	{
		char *ch = index(self->opt, '=');

		if (ch)
		{
			mydata->lifetime = atoi(ch+1);
		}
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
 * socks_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
static	void	socks_release(AnInstance *self)
{
	struct sock_private *mydata = self->data;

	free(mydata);
	free(self->popt);
}

/*
 * socks_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
static	void	socks_stats(AnInstance *self)
{
	struct socks_private *mydata = self->data;

	sendto_ircd("S socks:%u open %u closed %u noproxy %u",
		self->port,
		mydata->open, mydata->closed, mydata->noproxy);
	sendto_ircd("S socks:%u cache open %u closed %u noproxy %u miss %u (%u <= %u)",
		self->port,
		mydata->chito, mydata->chitc, mydata->chitn,
		mydata->cmiss, mydata->cnow, mydata->cmax);
}

/*
 * socks_start
 *
 *	This procedure is called to start the socks check procedure.
 *	Returns 0 if everything went fine,
 *	-1 otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. socks_clean
 *	will NOT be called)
 */
static	int	socks_start(u_int cl)
{
	char *error;
	int fd;

	if (cldata[cl].state & A_DENY)
	{
		/* no point of doing anything */
		DebugLog((ALOG_DSOCKS, 0,
			"socks_start(%d): A_DENY already set ", cl));
		return -1;
	}

	if (socks_check_cache(cl))
	{
		return -1;
	}

	DebugLog((ALOG_DSOCKS, 0,
		"socks_start(%d): Connecting to %s", cl,
		cldata[cl].itsip));
	fd= tcp_connect(cldata[cl].ourip, cldata[cl].itsip, SOCKSPORT, &error);
	if (fd < 0)
	{
		DebugLog((ALOG_DSOCKS, 0,
			"socks_start(%d): tcp_connect() reported %s",
			cl, error));
		socks_add_cache(cl, PROXY_NONE);
		return -1;
	}

	/* so that socks_work() is called when connected */
	cldata[cl].wfd = fd;

	return 0;
}

/*
 * socks_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
static	int	socks_work(u_int cl)
{
	char *strver = "4";
	struct socks_private *mydata = cldata[cl].instance->data;

	if (cldata[cl].mod_status == 0)
	{
		if (mydata->options & OPT_V5ONLY)
		{
			cldata[cl].mod_status = ST_V5;
		}
		else
		{
			cldata[cl].mod_status = ST_V4;
		}
	}

	if (cldata[cl].mod_status & ST_V5)
	{
		strver = "5";
	}
	else if (cldata[cl].mod_status & ST_V5b)
	{
		strver = "5b";
	}

	DebugLog((ALOG_DSOCKS, 0,
		"socks%s_work(%d): %d %d buflen=%d", strver,
		cl, cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));

	if (cldata[cl].wfd > 0)
	{
		/*
		** We haven't sent the query yet, the connection
		** was just established.
		*/
		return socks_write(cl, strver);
	}
	else
	{
		return socks_read(cl, strver);
	}
}

/*
 * socks_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
static	void	socks_clean(u_int cl)
{
	DebugLog((ALOG_DSOCKS, 0, "socks_clean(%d): cleaning up", cl));
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
 * socks_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if check was aborted.
 */
static	int	socks_timeout(u_int cl)
{
	DebugLog((ALOG_DSOCKS, 0,
		"socks_timeout(%d): calling socks_clean ", cl));
	socks_clean(cl);
	return -1;
}

aModule Module_socks =
	{ "socks", socks_init, socks_release, socks_stats,
	  socks_start, socks_work, socks_timeout, socks_clean };

