/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_lhex.c
 *   Copyright (C) 1998-1999 Christophe Kalt and Andrew Snare
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
static  char rcsid[] = "@(#)$Id mod_lhex.c,v 1.12 1999/02/06 21:43:52 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_LHEX_C
#include "a_externs.h"
#undef MOD_LHEX_C

/****************************** PRIVATE *************************************/
#define LHEXPORT	9674

struct lhex_private
{
	/* stats */
	u_int ok, banned;
	u_int tried, clean, timeout;
};

/******************************** PUBLIC ************************************/

/*
 * lhex_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
char *
lhex_init(self)
AnInstance *self;
{
	struct lhex_private *mydata;

#if defined(INET6)
	return "IPv6 unsupported.";
#endif
	if(self->opt == NULL)
		return "Aie! no option(s): no LHEx server to connect to!";
	if(!inetaton(self->opt,NULL))
		return "Aie! Option wasn't a valid IP address!";

	/* Allocate the module data */
	mydata = (struct lhex_private *) malloc(sizeof(struct lhex_private));
	bzero((char *) mydata, sizeof(struct lhex_private));

	self->popt = mystrdup(self->opt);
	self->data = mydata;
	return NULL;
}

/*
 * lhex_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
void
lhex_release(self)
AnInstance *self;
{
	struct lhex_private *mydata = self->data;
	free(mydata);
	free(self->popt);
}

/*
 * lhex_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
void
lhex_stats(self)
AnInstance *self;
{
	struct lhex_private *mydata = self->data;

	sendto_ircd("S lhex ok %u banned %u", mydata->ok, mydata->banned);
	sendto_ircd("S lhex tried %u aborted %u / %u",
		    mydata->tried, mydata->clean, mydata->timeout);
}

/*
 * lhex_start
 *
 *	This procedure is called to start the LHEx check procedure.
 *	Returns 0 if everything went fine,
 *	anything else otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. lhex_clean
 *	will NOT be called)
 */
int
lhex_start(cl)
u_int cl;
{
	char *error;
	int fd;
	struct lhex_private *mydata = cldata[cl].instance->data;

	if (cldata[cl].state & A_DENY)
	    {
		/* no point of doing anything */
		DebugLog((ALOG_DLHEX, 0,
			  "lhex_start(%d): A_DENY already set ", cl));
		return -1;
	    }

	DebugLog((ALOG_DLHEX, 0, "lhex_start(%d): Connecting to %s", cl,
		  cldata[cl].instance->opt));
	mydata->tried += 1;
	fd= tcp_connect(cldata[cl].ourip, cldata[cl].instance->opt,
	                LHEXPORT, &error);
	if (fd < 0)
	    {
		DebugLog((ALOG_DLHEX, 0,
			  "lhex_start(%d): tcp_connect() reported %s",
			  cl, error));
		return -1;
	    }

	cldata[cl].wfd = fd; /*so that lhex_work() is called when connected*/
	return 0;
}

/*
 * lhex_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int
lhex_work(cl)
u_int cl;
{
    	DebugLog((ALOG_DLHEX, 0, "lhex_work(%d): %d %d buflen=%d", cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));
	if (cldata[cl].wfd > 0)
	    {
		/*
		** We haven't sent the query yet, the connection was just
		** established.
		*/
		char query[3+7+6+4+USERLEN+2*HOSTLEN+8+3];/*strlen(atoi(cl))<=8*/
		char *ident = cldata[cl].authuser;

		/* This is part of every request */
		sprintf(query, "id:%u ip:%s", cl, cldata[cl].itsip);

		/* These bits are optional, depending on what's known */
		if (ident)
		    {
		    	strcat(query, " ident:");
			strcat(query, ident);
		    }
		if (cldata[cl].state & A_GOTH)
		    {
		    	strcat(query, " host:");
			strcat(query, cldata[cl].host);
		    }
		/* Terminate the request */
		strcat(query, "\r\n");

		DebugLog((ALOG_DLHEX, 0, "lhex_work(%u): Sending query [%s]",
			  cl, query));
		if (write(cldata[cl].wfd, query, strlen(query)) < 0)
		    {
			/* most likely the connection failed */
			DebugLog((ALOG_DLHEX, 0,
				  "lhex_work(%u): write() failed: %s",
				  cl, strerror(errno)));
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
		char *ch, *nch;
		u_int id;
		int retval = 0;

		cldata[cl].inbuffer[cldata[cl].buflen] = '\0';
		nch = cldata[cl].inbuffer;
		while((nch < (cldata[cl].inbuffer + cldata[cl].buflen)) &&
		      (ch = index(nch, '\r')) && !retval)
		    {
			char *och = nch;
			nch = ch+2;		/* Skip the \r\n */
			*ch = '\0';
			DebugLog((ALOG_DLHEX, 0, "lhex_work(%u): Got [%s]",
				 cl, och));

			/* Have a go at parsing the return info */
			if(sscanf(och, "%u", &id) != 1)
				DebugLog((ALOG_DLHEX, 0, "lhex_work(%u): "
					 "Malformed data!", cl));
			else
			    {
				struct lhex_private *d=cldata[cl].instance->data;
				ch = index(och, ':');
				while(isspace(*(++ch)));
				if(!strcmp(ch,"OK"))
				    {
					d->ok++;
					DebugLog((ALOG_DLHEX, 0,
						 "lhex_work(%u): OK", id));
				    	close(cldata[cl].rfd);
					cldata[cl].rfd = 0;
					retval = -1;
				    }
				else if(!strcmp(ch,"Not OK"))
				    {
				        d->banned++;
					DebugLog((ALOG_DLHEX, 0,
					         "lhex_work(%u): Not OK", id));
					cldata[cl].state |= A_DENY;
					/* I really wish we could send the
					   client a "reason" here :P */
					sendto_ircd("K %d %s %u ", cl,
						    cldata[cl].itsip,
						    cldata[cl].itsport);
				    	close(cldata[cl].rfd);
					cldata[cl].rfd = 0;
					retval = -1;
				    }
				else
				    {
#if 0
				    	/* Call this info for the client */
					sendto_ircd("I %d %s %u NOTICE AUTH :%s",
						    cl, cldata[cl].itsip,
						    cldata[cl].itsport, ch);
#endif
				    	retval = 0;
				    }
			    }
		    }
	    	return retval;
	    }
	return 0;
}

/*
 * lhex_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
lhex_clean(cl)
u_int cl;
{
	struct lhex_private *mydata = cldata[cl].instance->data;

	mydata->clean += 1;
	DebugLog((ALOG_DLHEX, 0, "lhex_clean(%d): cleaning up", cl));
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
 * lhex_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if check was aborted.
 */
int
lhex_timeout(cl)
u_int cl;
{
	struct lhex_private *mydata = cldata[cl].instance->data;

	mydata->timeout += 1;
	DebugLog((ALOG_DLHEX, 0, "lhex_timeout(%d): calling lhex_clean ", cl));
	lhex_clean(cl);
	return -1;
}

aModule Module_lhex =
	{ "lhex", lhex_init, lhex_release, lhex_stats,
	  lhex_start, lhex_work, lhex_timeout, lhex_clean };
