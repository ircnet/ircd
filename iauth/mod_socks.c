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
static  char rcsid[] = "@(#)$Id: mod_socks.c,v 1.16 1999/06/21 02:55:45 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_SOCKS_C
#include "a_externs.h"
#undef MOD_SOCKS_C

/****************************** PRIVATE *************************************/

#define CACHETIME 30

#define SOCKSPORT 1080

struct proxylog
{
	struct proxylog *next;
	char ip[HOSTLEN+1];
	u_char state; /* 0 = no proxy, 1 = open proxy, 2 = closed proxy */
	time_t expire;
};

#define OPT_LOG		0x01
#define OPT_DENY	0x02
#define OPT_PARANOID	0x04

#define PROXY_NONE 0
#define PROXY_OPEN 1
#define PROXY_CLOSE 2

struct socks_private
{
	struct proxylog *cache;
	u_int lifetime;
	u_char options;
	/* stats */
	u_int chitc, chito, chitn, cmiss, cnow, cmax;
	u_int closed4, closed5, open4, open5, noprox;
};

/*
 * socks_open_proxy
 *
 *	Found an open proxy for cl: deal with it!
 */
static void
socks_open_proxy(cl)
int cl;
{
    struct socks_private *mydata = cldata[cl].instance->data;

    /* open proxy */
    if (mydata->options & OPT_DENY)
	{
	    cldata[cl].state |= A_DENY;
	    sendto_ircd("k %d %s %u ", cl, cldata[cl].itsip,
			cldata[cl].itsport);
	}
    if (mydata->options & OPT_LOG)
	    sendto_log(ALOG_FLOG, LOG_INFO, "socks: open proxy: %s[%s]",
		       cldata[cl].host, cldata[cl].itsip);
}

/*
 * socks_add_cache
 *
 *	Add an entry to the cache.
 */
static void
socks_add_cache(cl, state, ver)
int cl, state;
{
    struct socks_private *mydata = cldata[cl].instance->data;
    struct proxylog *next;

    if (state == 1)
	    if (ver == 4)
	      mydata->open4 += 1;
	    else
	      mydata->open5 += 1;
    else if (state == 0)
	    mydata->noprox += 1;
    else
      if (ver == 4)
	      mydata->closed4 += 1;
	    else
	      mydata->closed5 += 1;

    if (mydata->lifetime == 0)
	    return;

    mydata->cnow += 1;
    if (mydata->cnow > mydata->cmax)
	    mydata->cmax = mydata->cnow;

    next = mydata->cache;
    mydata->cache = (struct proxylog *)malloc(sizeof(struct proxylog));
    mydata->cache->expire = time(NULL) + mydata->lifetime;
    strcpy(mydata->cache->ip, cldata[cl].itsip);
    mydata->cache->state = state;
    mydata->cache->next = next;
    DebugLog((ALOG_DSOCKSC, 0, "socks_add_cache(%d): new cache %s, open=%d",
	      cl, mydata->cache->ip, state));
}

/*
 * socks_check_cache
 *
 *	Check cache for an entry.
 */
static int
socks_check_cache(cl)
{
    struct socks_private *mydata = cldata[cl].instance->data;
    struct proxylog **last, *pl;
    time_t now = time(NULL);

    if (mydata->lifetime == 0)
	    return 0;

    DebugLog((ALOG_DSOCKSC, 0, "socks_check_cache(%d): Checking cache for %s",
	      cl, cldata[cl].itsip));

    last = &(mydata->cache);
    while (pl = *last)
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
			    socks_open_proxy(cl);
			    mydata->chito += 1;
			}
		    else if (pl->state == 0)
			    mydata->chitn += 1;
		    else
			    mydata->chitc += 1;
		    return -1;
		}
	    last = &(pl->next);
	}
    mydata->cmiss += 1;
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
char *
socks_init(self)
AnInstance *self;
{
	struct socks_private *mydata;
	char tmpbuf[32], cbuf[32];
	static char txtbuf[80];

#if defined(INET6)
	return "IPv6 unsupported.";
#endif
	if (self->opt == NULL)
		return "Aie! no option(s): nothing to be done!";
	
	mydata = (struct socks_private *) malloc(sizeof(struct socks_private));
	bzero((char *) mydata, sizeof(struct socks_private));
	mydata->cache = NULL;
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
		return "Aie! unknown option(s): nothing to be done!";

	if (strstr(self->opt, "cache"))
	    {
		char *ch = index(self->opt, '=');
		
		if (ch)
			mydata->lifetime = atoi(ch+1);
	    }
	sprintf(cbuf, ",cache=%d", mydata->lifetime);
	strcat(tmpbuf, cbuf);
	sprintf(cbuf, ", Cache %d (min)", mydata->lifetime);
	strcat(txtbuf, cbuf);
	mydata->lifetime *= 60;

	self->popt = mystrdup(tmpbuf+1);
	self->data = mydata;
	return txtbuf+2;
}

/*
 * socks_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
void
socks_release(self)
AnInstance *self;
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
void
socks_stats(self)
AnInstance *self;
{
	struct socks_private *mydata = self->data;

	sendto_ircd("S socks open %u/%u closed %u/%u noproxy %u",
		    mydata->open4, mydata->open5, mydata->closed4, mydata->closed5, mydata->noprox);
	sendto_ircd("S socks cache open %u closed %u noproxy %u miss %u (%u <= %u)",
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
int
socks_start(cl)
u_int cl;
{
	char *error;
	int fd;

	if (cldata[cl].state & A_DENY)
	    {
		/* no point of doing anything */
		DebugLog((ALOG_DSOCKS, 0,
			  "socks_start(%d): A_DENY alredy set ", cl));
		return -1;
	    }

	if (socks_check_cache(cl))
		return -1;

	DebugLog((ALOG_DSOCKS, 0, "socks_start(%d): Connecting to %s", cl,
		  cldata[cl].itsip));
	fd= tcp_connect(cldata[cl].ourip, cldata[cl].itsip, SOCKSPORT, &error);
	if (fd < 0)
	    {
		DebugLog((ALOG_DSOCKS, 0,
			  "socks_start(%d): tcp_connect() reported %s",
			  cl, error));
		socks_add_cache(cl, PROXY_NONE, 0);
		return -1;
	    }

	cldata[cl].wfd = fd; /*so that socks_work() is called when connected*/
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
int
socks_work(cl)
u_int cl;
{
	static	u_char query[13] =
		{ 4, 1, 26, 11, 0, 0, 0, 0, 'u', 's', 'e', 'r', 0 };

		/* we use .mod_status to call socks4 or socks5; */
		if (cldata[cl].mod_status == 0) cldata[cl].mod_status = 4;

    	DebugLog((ALOG_DSOCKS, 0, "socks%d_work(%d): %d %d buflen=%d", cldata[cl].mod_status, cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));
	if (cldata[cl].wfd > 0)
	    {
		/*
		** We haven't sent the query yet, the connection was just
		** established.
		*/
		u_int a, b, c, d;

		if (cldata[cl].mod_status == 4) 
			{
			if (sscanf(cldata[cl].ourip, "%u.%u.%u.%u", &a,&b,&c,&d) != 4)
		    {
			sendto_log(ALOG_DSOCKS|ALOG_IRCD, LOG_ERR,
				  "socks_work(%d): sscanf(\"%s\") failed", 
				   cl, cldata[cl].ourip);
			close(cldata[cl].wfd);
			cldata[cl].wfd = 0;
			return -1;
		    }
			query[0] = 4; query[1] = 1;
			query[2] = ((cldata[cl].ourport & 0xff00) >> 8);
			query[3] = (cldata[cl].ourport & 0x00ff);
			query[4] = a; query[5] = b; query[6] = c; query[7] = d;
			}
		else
			{
			query[0] = 5 ; query[1] = 1; query[2] = 0;
			}
		
		DebugLog((ALOG_DSOCKS, 0, "socks%d_work(%d): Checking %s %u", query[0],
			  cl, cldata[cl].ourip, SOCKSPORT));
		if (write(cldata[cl].wfd, query, (query[0]==4?13:3)) != (query[0]==4?13:3))
		    {
			/* most likely the connection failed */
			DebugLog((ALOG_DSOCKS, 0,
				  "socks_work(%d): write() failed: %s",
				  cl, strerror(errno)));
			socks_add_cache(cl, PROXY_NONE, 0);
			close(cldata[cl].wfd);
			cldata[cl].rfd = cldata[cl].wfd = 0;
			return 1;
		    }
		cldata[cl].rfd = cldata[cl].wfd;
		cldata[cl].wfd = 0;
	    }
	else
	    {
		/* data's in from the other end */
		if (cldata[cl].buflen >= 2)
		    {
			/* got all we need */
			DebugLog((ALOG_DSOCKS, 0,
				  "socks%d_work(%d): Got [%d.%d]", query[0],
				  cl, cldata[cl].inbuffer[0],
				  cldata[cl].inbuffer[1]));
			if (cldata[cl].inbuffer[0] == (query[0]==4?0:5))
			    {
				struct socks_private *mydata;
				u_char state = PROXY_CLOSE;

				mydata = cldata[cl].instance->data;
				if (cldata[cl].inbuffer[1] == (query[0]==4?90:0))
					state = PROXY_OPEN;
				else if (query[0]==4 && (mydata->options & OPT_PARANOID) &&
					cldata[cl].inbuffer[1] == 91)
					state = PROXY_OPEN;
				if (state == PROXY_OPEN)
					socks_open_proxy(cl);
				socks_add_cache(cl, state, query[0]);
				if ((query[0]==4 && (cldata[cl].inbuffer[1] != 90 &&
														 cldata[cl].inbuffer[1] != 91 &&
														 cldata[cl].inbuffer[1] != 92 &&
														 cldata[cl].inbuffer[1] != 93)) ||
				    (query[0]==5 && (cldata[cl].inbuffer[1] != 0 &&
											 			 cldata[cl].inbuffer[1] != 1 &&
											 			 cldata[cl].inbuffer[1] != 2 &&
											 			 cldata[cl].inbuffer[1] != -1)))
				    {
					sendto_log(ALOG_FLOG, LOG_WARNING,
				   "socks%d: unexpected reply: %u,%u %s[%s]",
						   query[0],
						   cldata[cl].inbuffer[0],
						   cldata[cl].inbuffer[1],
						   cldata[cl].host,
						   cldata[cl].itsip);
					sendto_log(ALOG_IRCD, 0,
					   "socks%d: unexpected reply: %u,%u",
						   query[0],
						   cldata[cl].inbuffer[0],
						   cldata[cl].inbuffer[1]);
					if (cldata[cl].mod_status == 4) cldata[cl].mod_status = 5;
				    }
			    }
			else
				if (cldata[cl].mod_status == 4)
				  cldata[cl].mod_status = 5;
				else
					socks_add_cache(cl, PROXY_NONE, 0);
			if (cldata[cl].mod_status == 5)
					{
					cldata[cl].mod_status = 1; /* neither 0 (equivalent to 4), nor 5 */
					cldata[cl].buflen=0;
					close(cldata[cl].rfd);
					cldata[cl].rfd = 0;
					return (socks_start(cl));
					}
			close(cldata[cl].rfd);
			cldata[cl].rfd = 0;
			return -1;
		    }
		else
			return 0;
	    }
	return 0;
}

/*
 * socks_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
socks_clean(cl)
u_int cl;
{
    DebugLog((ALOG_DSOCKS, 0, "socks_clean(%d): cleaning up", cl));
    /*
    ** only one of rfd and wfd may be set at the same time,
    ** in any case, they would be the same fd, so only close() once
    */
    if (cldata[cl].rfd)
	    close(cldata[cl].rfd);
    else if (cldata[cl].wfd)
	    close(cldata[cl].wfd);
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
int
socks_timeout(cl)
u_int cl;
{
    DebugLog((ALOG_DSOCKS, 0, "socks_timeout(%d): calling socks_clean ", cl));
    socks_clean(cl);
    return -1;
}

aModule Module_socks =
	{ "socks", socks_init, socks_release, socks_stats,
	  socks_start, socks_work, socks_timeout, socks_clean };
