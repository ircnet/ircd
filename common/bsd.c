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
static  char rcsid[] = "@(#)$Id: bsd.c,v 1.2 1997/09/03 17:45:11 kalt Exp $";
#endif

#include "os.h"
#ifndef CLIENT_COMPILE
# include "s_defines.h"
#else
# include "c_defines.h"
#endif
#define BSD_C
#ifndef CLIENT_COMPILE
# include "s_externs.h"
#else
# include "c_externs.h"
#endif
#undef BSD_C

#ifdef DEBUGMODE
int	writecalls = 0, writeb[10] = {0,0,0,0,0,0,0,0,0,0};
#endif
RETSIGTYPE dummy(s)
int s;
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
int	deliver_it(cptr, str, len)
aClient *cptr;
int	len;
char	*str;
    {
	int	retval;
	aClient	*acpt = cptr->acpt;

#ifdef	DEBUGMODE
	writecalls++;
#endif
#ifndef	NOWRITEALARM
	(void)alarm(WRITEWAITDELAY);
#endif
	retval = send(cptr->fd, str, len, 0);
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

#ifndef	NOWRITEALARM
	(void )alarm(0);
#endif
#ifdef DEBUGMODE
	if (retval < 0) {
		writeb[0]++;
		Debug((DEBUG_ERROR,"write error (%s) to %s",
			sys_errlist[errno], cptr->name));
#ifndef	CLIENT_COMPILE
		hold_server(cptr);
#endif
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
		if (cptr->sendB > 1023)
		    {
			cptr->sendK += (cptr->sendB >> 10);
			cptr->sendB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
		    }
		if (acpt != &me)
		    {
			acpt->sendB += retval;
			if (acpt->sendB > 1023)
			    {
				acpt->sendK += (acpt->sendB >> 10);
				acpt->sendB &= 0x03ff;
			    }
		    }
		else if (me.sendB > 1023)
		    {
			me.sendK += (me.sendB >> 10);
			me.sendB &= 0x03ff;
		    }
	    }
	return(retval);
}
