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
static  char rcsid[] = "@(#)$Id: mod_socks.c,v 1.4 1998/08/07 02:04:23 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_SOCKS_C
#include "a_externs.h"
#undef MOD_SOCKS_C

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
	if (self->opt == NULL)
		return "Aie! no option(s): nothing to be done!";
	if (strstr(self->opt, "log") && strstr(self->opt, "reject"))
	    {
		self->popt = "log,reject";
		return "Set to log and reject.";
	    }
	if (strstr(self->opt, "log"))
	    {
		self->popt = "log";
		return "Set to log (but not reject).";
	    }
	if (strstr(self->opt, "reject"))
	    {
		self->popt = "reject";
		return "Set to reject without logging.";
	    }
	return "Aie! unknown option(s): nothing to be done!";
}

/*
 * socks_start
 *
 *	This procedure is called to start the socks check procedure.
 *	Returns 0 if everything went fine,
 *	anything else otherwise (nothing to be done, or failure)
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
	
	DebugLog((ALOG_DSOCKS, 0, "socks_start(%d): Connecting to %s %u", cl,
		  cldata[cl].itsip, 1080));
	fd = tcp_connect(cldata[cl].ourip, cldata[cl].itsip, 1080, &error);
	if (fd < 0)
	    {
		DebugLog((ALOG_DSOCKS, 0,
			  "socks_start(%d): tcp_connect() reported %s",
			  cl, error));
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
    	DebugLog((ALOG_DSOCKS, 0, "socks_work(%d): %d %d buflen=%d", cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));
	if (cldata[cl].wfd > 0)
	    {
		/*
		** We haven't sent the query yet, the connection was just
		** established.
		*/
		static u_char   query[3] = {5, 1, 0};

		if (write(cldata[cl].wfd, query, 3) != 3)
		    {
			/* most likely the connection failed */
			DebugLog((ALOG_DSOCKS, 0,
				  "socks_work(%d): write() failed: %s", cl,
				  strerror(errno)));
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
				  "socks_work(%d): Got [%d.%d]",
				  cl, cldata[cl].inbuffer[0],
				  cldata[cl].inbuffer[1]));
			if (cldata[cl].inbuffer[0] == 5 &&
			    cldata[cl].inbuffer[1] == 0)
			    {
				/* ack, open SOCKS proxy! */
				if (cldata[cl].instance->opt &&
				    strstr(cldata[cl].instance->opt, "reject"))
				    {
					sendto_ircd("K %d %s %u ", cl,
						    cldata[cl].itsip,
						    cldata[cl].itsport);
					cldata[cl].state |= A_DENY;
				    }
				if (cldata[cl].instance->opt &&
				    strstr(cldata[cl].instance->opt, "log"))
					sendto_log(ALOG_FLOG, LOG_INFO,
						   "socks: open proxy: %s[%s]",
						   cldata[cl].host,
						   cldata[cl].itsip);
			    }
			/* else there's some error */
			/*
			** In any case, our job is done, let's cleanup.
			*/
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
	{ "socks", socks_init, NULL, socks_start, socks_work, 
		  socks_timeout, socks_clean };
