/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_pipe.c
 *   Copyright (C) 1999 Christophe Kalt
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
static  char rcsid[] = "@(#)$Id: mod_pipe.c,v 1.3 1999/03/11 19:53:20 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_PIPE_C
#include "a_externs.h"
#undef MOD_PIPE_C

/*
 * pipe_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
char *
pipe_init(self)
AnInstance *self;
{
	if (self->opt == NULL)
		return "Aie! no option(s): nothing to be done!";
	if (strncasecmp(self->opt, "prog=", 5))
		return "Aie! unknown option(s): nothing to be done!";
	self->popt = self->opt + 5;
	return self->popt;
}

/*
 * pipe_release
 *
 *	This procedure is called when a particular module is unloaded.
void
pipe_release(self)
AnInstance *self;
{
}
 */

/*
 * pipe_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
void
pipe_stats(self)
AnInstance *self;
{
}
 */

/*
 * pipe_start
 *
 *	This procedure is called to start an authentication.
 *	Returns 0 if everything went fine,
 *	-1 otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. pipe_clean
 *	will NOT be called)
 */
int
pipe_start(cl)
u_int cl;
{
	int pp[2], rc;
	
	DebugLog((ALOG_DPIPE, 0, "pipe_start(%d): Forking for %s %u", cl,
		  cldata[cl].itsip, cldata[cl].itsport));
	if (pipe(pp) == -1)
	    {
		DebugLog((ALOG_DPIPE, 0,
			  "pipe_start(%d): Error creating pipe: %s",
			  cl, strerror(errno)));
		return -1;
	    }
	switch (rc = vfork())
	    {
	    case -1 :
		    DebugLog((ALOG_DPIPE, 0,
			      "pipe_start(%d): Error forking: %s",
			      cl, strerror(errno)));
		    return -1;
	    case 0 :
		    {
			(void)close(pp[0]);
			for (rc = 2; rc < MAXCONNECTIONS; rc++)
				if (rc != pp[1])
					(void)close(rc);
			if (pp[1] != 2)
				(void)dup2(pp[1], 2);
			(void)dup2(2, 1);
			if (pp[1] != 2 && pp[1] != 1)
				(void)close(pp[1]);
			(void)execlp(cldata[cl].instance->popt,
				     cldata[cl].instance->popt,
				     cldata[cl].itsip, cldata[cl].itsport);
			_exit(-1);
		    }
	    default :
		    (void)close(pp[1]);
		    break;
	    }

	cldata[cl].rfd = pp[0];
	return 0;
}

/*
 * pipe_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int
pipe_work(cl)
u_int cl;
{
    	DebugLog((ALOG_DPIPE, 0, "pipe_work(%d): %d %d buflen=%d %c", cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen,
		  cldata[cl].inbuffer[0]));

	switch (cldata[cl].inbuffer[0])
	    {
	    case 'Y':
		    break;
	    case 'N':
		    cldata[cl].state |= A_DENY;
		    sendto_ircd("K %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
		    break;
#if 0
		    /* hm.. need deeper mods to ircd */
	    case 'y':
		    /* restricted connection only */
		    cldata[cl].state |= A_RESTRICT;
		    sendto_ircd("k %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
		    break;
#endif
	    default :
		    /* error */
		    sendto_log(ALOG_FLOG|ALOG_IRCD, LOG_WARNING,
			       "pipe: unexpected %c for %s[%s]",
			       cldata[cl].inbuffer[0],
			       cldata[cl].host,
			       cldata[cl].itsip);
		    break;
	    }

	/* We're done */
	close(cldata[cl].rfd);
	cldata[cl].rfd = 0;
	return -1;
}

/*
 * pipe_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
pipe_clean(cl)
u_int cl;
{
	DebugLog((ALOG_DPIPE, 0, "pipe_clean(%d): cleaning up", cl));
	if (cldata[cl].rfd)
		close(cldata[cl].rfd);
	cldata[cl].rfd = 0;
}

/*
 * pipe_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if authentication was aborted.
 */
int
pipe_timeout(cl)
u_int cl;
{
	DebugLog((ALOG_DPIPE, 0, "pipe_timeout(%d): calling pipe_clean ",
		  cl));
	pipe_clean(cl);
	return -1;
}

aModule Module_pipe =
	{ "pipe", pipe_init, NULL, NULL,
	  pipe_start, pipe_work, pipe_timeout, pipe_clean };
