/************************************************************************
 *   IRC - Internet Relay Chat, mod_passwd.c
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
static  char rcsid[] = "@(#)$Id: mod_passwd.c,v 1.1 1999/03/13 23:06:15 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#include "a_externs.h"

/*
 * passwd_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
char *
passwd_init(self)
AnInstance *self;
{
	return NULL;
}

/*
 * passwd_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
void
passwd_release(self)
AnInstance *self;
{
}

/*
 * passwd_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
void
passwd_stats(self)
AnInstance *self;
{
}

/*
 * passwd_start
 *
 *	This procedure is called to start an authentication.
 *	Returns 0 if everything went fine,
 *	+1 if still waiting for some data (username, password)..
 *	-1 otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. passwd_clean
 *	will NOT be called)
 */
int
passwd_start(cl)
u_int cl;
{
	if (cldata[cl].authuser &&
	    cldata[cl].authfrom < cldata[cl].instance->in)
	    {
		/*
		** another instance preceding this one in the configuration
		** has already authenticated the user, no need to bother
		** doing anything here then. (the other takes precedence)
		*/
		return -1;
	    }
	if ((cldata[cl].state & A_GOTU) == 0)
		/* haven't received username/password pair from ircd yet */
		return 1;
	if ((cldata[cl].state & A_GOTP) == 0)
	    {
		/* no password to check -> reject user! */
		cldata[cl].state |= A_DENY;
		sendto_ircd("K %d %s %u ", cl, cldata[cl].itsip,
			    cldata[cl].itsport);
		return -1; /* done */
	    }
	/* 
	**
	**
	** INSERT FUNCTION TO CHECK PASSWORD VALIDITY
	**
	**
	*/
	/* if failure, see above */
	/* if success: */
	cldata[cl].state |= A_UNIX;
	if (cldata[cl].authuser)
		free(cldata[cl].authuser);
	cldata[cl].authuser = mystrdup(cldata[cl].user);
	cldata[cl].authfrom = cldata[cl].instance->in;
	sendto_ircd("U %d %s %u %s", cl, cldata[cl].itsip, cldata[cl].itsport,
		    cldata[cl].authuser);
	return -1; /* done */
}

/*
 * passwd_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int
passwd_work(cl)
u_int cl;
{
	return -1;
}

/*
 * passwd_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
passwd_clean(cl)
u_int cl;
{
}

/*
 * passwd_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if authentication was aborted.
 */
int
passwd_timeout(cl)
u_int cl;
{
}

static aModule Module_passwd =
	{ "passwd", passwd_init, passwd_release, passwd_stats,
	  passwd_start, passwd_work, passwd_timeout, passwd_clean };

aModule *
passwd_load()
{
	return &Module_passwd;
}
