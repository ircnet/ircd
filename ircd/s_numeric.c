/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_numeric.c
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   Numerous fixes by Markku Savela
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
static  char rcsid[] = "@(#)$Id: s_numeric.c,v 1.2 1997/04/14 15:04:31 kalt Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h" 
#include "numeric.h"
#include "h.h"

static char buffer[1024];

/*
** DoNumeric (replacement for the old do_numeric)
**
**	parc	number of arguments ('sender' counted as one!)
**	parv[0]	pointer to 'sender' (may point to empty string) (not used)
**	parv[1]..parv[parc-1]
**		pointers to additional parameters, this is a NULL
**		terminated list (parv[parc] == NULL).
**
** *WARNING*
**	Numerics are mostly error reports. If there is something
**	wrong with the message, just *DROP* it! Don't even think of
**	sending back a neat error message -- big danger of creating
**	a ping pong error message...
*/
int	do_numeric(numeric, cptr, sptr, parc, parv)
int	numeric;
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	aChannel *chptr;
	char	*nick, *p;
	int	i;

	if (parc < 1 || !IsServer(sptr))
		return 1;
	/* Remap low number numerics. */
	if (numeric < 100)
		numeric += 100;
	/*
	** Prepare the parameter portion of the message into 'buffer'.
	** (Because the buffer is twice as large as the message buffer
	** for the socket, no overflow can occur here... ...on current
	** assumptions--bets are off, if these are changed --msa)
	** Note: if buffer is non-empty, it will begin with SPACE.
	*/
	buffer[0] = '\0';
	if (parc > 1)
	    {
		for (i = 2; i < (parc - 1); i++)
		    {
			(void)strcat(buffer, " ");
			(void)strcat(buffer, parv[i]);
		    }
		(void)strcat(buffer, " :");
		(void)strcat(buffer, parv[parc-1]);
	    }
	for (; (nick = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if ((acptr = find_client(nick, (aClient *)NULL)))
		    {
			/*
			** Drop to bit bucket if for me...
			** ...one might consider sendto_ops
			** here... --msa
			** And so it was done. -avalon
			** And regretted. Dont do it that way. Make sure
			** it goes only to non-servers. -avalon
			** Check added to make sure servers don't try to loop
			** with numerics which can happen with nick collisions.
			** - Avalon
			*/
			if (IsMe(acptr) || acptr->from == cptr)
				sendto_flag(SCH_NUM,
					    "From %s for %s: %s %d %s %s.",
					    get_client_name(cptr, TRUE),
					    acptr->name, sptr->name,
					    numeric, nick, buffer);
			else if (IsPerson(acptr) || IsServer(acptr) ||
				 IsService(acptr))
				sendto_prefix_one(acptr, sptr,":%s %d %s%s",
					parv[0], numeric, nick, buffer);
		    }
		/* any reason why no cptr == acptr->from checks here? -krys */
/* because these are not used.. -Vesa
		else if ((acptr = find_service(nick, (aClient *)NULL)))
			sendto_prefix_one(acptr, sptr,":%s %d %s%s",
				parv[0], numeric, nick, buffer);
		else if ((acptr = find_server(nick, (aClient *)NULL)))
		    {
			if (!IsMe(acptr) && acptr->from != cptr)
				sendto_prefix_one(acptr, sptr,":%s %d %s%s",
					parv[0], numeric, nick, buffer);
		    }
..nuke them */
		else if ((chptr = find_channel(nick, (aChannel *)NULL)))
			sendto_channel_butone(cptr,sptr,chptr,":%s %d %s%s",
					      parv[0],
					      numeric, chptr->chname, buffer);
	    }
	return 1;
}
