/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_rfc921.c
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
static  char rcsid[] = "@(#)$Id: mod_rfc931.c,v 1.6 1998/09/18 22:49:40 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_RFC931_C
#include "a_externs.h"
#undef MOD_RFC931_C

/*
 * rfc931_start
 *
 *	This procedure is called to start an authentication.
 *	Returns 0 if everything went fine,
 *	anything else otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. rfc931_clean
 *	will NOT be called)
 */
int
rfc931_start(cl)
u_int cl;
{
	char *error;
	int fd;
	
	DebugLog((ALOG_D931, 0, "rfc931_start(%d): Connecting to %s %u", cl,
		  cldata[cl].itsip, 113));
	fd = tcp_connect(cldata[cl].ourip, cldata[cl].itsip, 113, &error);
	if (fd < 0)
	    {
		DebugLog((ALOG_D931, 0,
			  "rfc931_start(%d): tcp_connect() reported %s",
			  cl, error));
		return -1;
	    }

	cldata[cl].wfd = fd; /*so that rfc931_work() is called when connected*/
	return 0;
}

/*
 * rfc931_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int
rfc931_work(cl)
u_int cl;
{
    	DebugLog((ALOG_D931, 0, "rfc931_work(%d): %d %d buflen=%d", cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));
	if (cldata[cl].wfd > 0)
	    {
		/*
		** We haven't sent the query yet, the connection was just
		** established.
		*/
		char query[32];

		sprintf(query, "%u , %u\r\n", cldata[cl].itsport,
			cldata[cl].ourport);
		if (write(cldata[cl].wfd, query, strlen(query)) < 0)
		    {
			/* most likely the connection failed */
			DebugLog((ALOG_D931, 0,
				  "rfc931_work(%d): write() failed: %s", cl,
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
		/* data's in from the ident server */
		char *ch;

		cldata[cl].inbuffer[cldata[cl].buflen] = '\0';
		ch = index(cldata[cl].inbuffer, '\r');
		if (ch)
		    {
			/* got all of it! */
			*ch = '\0';
			DebugLog((ALOG_D931, 0, "rfc931_work(%d): Got [%s]",
				  cl, cldata[cl].inbuffer));
			if (cldata[cl].buflen > 1024)
			    cldata[cl].inbuffer[1024] = '\0';
			ch = cldata[cl].inbuffer;
			while (*ch && !isdigit(*ch)) ch++;
			if (!*ch || atoi(ch) != cldata[cl].itsport)
			    {
				DebugLog((ALOG_D931, 0,
					  "remote port mismatch."));
				ch = NULL;
			    }
			while (ch && *ch && *ch != ',') ch++;
			while (ch && *ch && !isdigit(*ch)) ch++;
			if (ch && (!*ch || atoi(ch) != cldata[cl].ourport))
			    {
				DebugLog((ALOG_D931, 0,
					  "local port mismatch."));
				ch = NULL;
			    }
			if (ch) ch = index(ch, ':');
			if (ch) ch += 1;
			while (ch && *ch && *ch == ' ') ch++;
			if (ch && strncmp(ch, "USERID", 6))
			    {
				DebugLog((ALOG_D931, 0, "No USERID."));
				ch = NULL;
			    }
			if (ch) ch = index(ch, ':');
			if (ch) ch += 1;
			while (ch && *ch && *ch == ' ') ch++;
			if (ch)
			    {
				int other = 0;

				if (!strncmp(ch, "OTHER", 5))
					other = 1;
				ch = rindex(ch, ':');
				if (ch) ch += 1;
				while (ch && *ch && *ch == ' ') ch++;
				if (ch && *ch)
				    {
					char *chk = ch-1;

					while (*++chk)
						if (*chk == ':' ||
						    *chk == '@' ||
						    *chk == '[' ||
						    isspace(*chk))
							break;
					if (*chk)
						other = 1;
					if (cldata[cl].authuser)
						free(cldata[cl].authuser);
					cldata[cl].authuser = mystrdup(ch);
					cldata[cl].best = cldata[cl].instance;
					if (!other)
						cldata[cl].state |= A_UNIX;
					sendto_ircd("%c %d %s %u %s",
						    (other) ? 'u' : 'U', cl,
						    cldata[cl].itsip,
						    cldata[cl].itsport,
						    cldata[cl].authuser);
				    }
			    }
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
 * rfc931_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
rfc931_clean(cl)
u_int cl;
{
    DebugLog((ALOG_D931, 0, "rfc931_clean(%d): cleaning up", cl));
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
 * rfc931_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if authentication was aborted.
 */
int
rfc931_timeout(cl)
u_int cl;
{
    DebugLog((ALOG_D931, 0, "rfc931_timeout(%d): calling rfc931_clean ", cl));
    rfc931_clean(cl);
    return -1;
}

aModule Module_rfc931 =
	{ "rfc931", NULL, NULL, rfc931_start, rfc931_work, 
		  rfc931_timeout, rfc931_clean };
