/************************************************************************
 *   IRC - Internet Relay Chat, common/bsd.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static const volatile char rcsid[] = "@(#)$Id: bsd.c,v 1.13 2005/12/27 02:23:48 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define BSD_C
#include "s_externs.h"
#undef BSD_C

#ifdef DEBUGMODE
int	writecalls = 0, writeb[10] = {0,0,0,0,0,0,0,0,0,0};
#endif
RETSIGTYPE	dummy(int s)
{
#ifndef HAVE_RELIABLE_SIGNALS
	(void)signal(SIGALRM, dummy);
	(void)signal(SIGPIPE, dummy);
# ifndef HPUX	/* Only 9k/800 series require this, but don't know how to.. */
#  ifdef SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# endif
#else
# if POSIX_SIGNALS
	struct  sigaction       act;

	act.sa_handler = dummy;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGALRM);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
#  ifdef SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
#  endif
	(void)sigaction(SIGALRM, &act, (struct sigaction *)NULL);
	(void)sigaction(SIGPIPE, &act, (struct sigaction *)NULL);
#  ifdef SIGWINCH
	(void)sigaction(SIGWINCH, &act, (struct sigaction *)NULL);
#  endif
# endif
#endif
}


/*
** deliver_it
**	Attempt to send a sequence of bytes to the connection.
**	Returns
**
**	< 0	Some fatal error occurred, (but not EWOULDBLOCK).
**		This return is a request to close the socket and
**		clean up the link.
**	
**	>= 0	No real error occurred, returns the number of
**		bytes actually transferred. EWOULDBLOCK and other
**		possibly similar conditions should be mapped to
**		zero return. Upper level routine will have to
**		decide what to do with those unwritten bytes...
**
**	*NOTE*	alarm calls have been preserved, so this should
**		work equally well whether blocking or non-blocking
**		mode is used...
*/
int	deliver_it(aClient *cptr, char *str, int len)
{
	int	retval;
	aClient	*acpt = cptr->acpt;
	int	savederrno = 0;

#ifdef	DEBUGMODE
	writecalls++;
#endif
	retval = send(cptr->fd, str, len, 0);

	/* Prevent overwriting errno of send(). */
	if (retval < 0)
		savederrno = errno;
	/*
	** Convert WOULDBLOCK to a return of "0 bytes moved". This
	** should occur only if socket was non-blocking. Note, that
	** all is Ok, if the 'write' just returns '0' instead of an
	** error and errno=EWOULDBLOCK.
	**
	** ...now, would this work on VMS too? --msa
	*/
	if (retval < 0 && (errno == EWOULDBLOCK || errno == EAGAIN ||
#ifdef	EMSGSIZE
			   errno == EMSGSIZE ||
#endif
			   errno == ENOBUFS))
	    {
		retval = 0;
		cptr->flags |= FLAGS_BLOCKED;
	    }
	else if (retval > 0)
		cptr->flags &= ~FLAGS_BLOCKED;

#ifdef DEBUGMODE
	if (retval < 0) {
		writeb[0]++;
		Debug((DEBUG_ERROR,"write error (%s) to %s",
			strerror(errno), cptr->name));
	} else if (retval == 0)
		writeb[1]++;
	else if (retval < 16)
		writeb[2]++;
	else if (retval < 32)
		writeb[3]++;
	else if (retval < 64)
		writeb[4]++;
	else if (retval < 128)
		writeb[5]++;
	else if (retval < 256)
		writeb[6]++;
	else if (retval < 512)
		writeb[7]++;
	else if (retval < 1024)
		writeb[8]++;
	else
		writeb[9]++;
#endif
	if (retval > 0)
	    {
#if defined(DEBUGMODE) && defined(DEBUG_WRITE)
		Debug((DEBUG_WRITE, "send = %d bytes to %d[%s]:[%*.*s]\n",
			retval, cptr->fd, cptr->name, retval, retval, str));
#endif
		cptr->sendB += retval;
		me.sendB += retval;
		if (acpt != &me)
		    {
			acpt->sendB += retval;
		    }
	    }
	/* Retval of erroneous send() would always be -1, so we return
	** (negative) saved errno, so upper layer will give proper notice.
	** Note above about EAGAIN or EWOULDBLOCK. --B. */
	if (retval < 0)
		retval = -savederrno;
	
	return(retval);
}

