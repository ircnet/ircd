/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_bsd.c
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
static  char rcsid[] = "@(#)$Id: ctcp.c,v 1.2 1997/09/03 17:45:36 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define CTCP_C
#include "c_externs.h"
#undef CTCP_C

#define	CTCP_CHAR	0x1

void check_ctcp(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	char	*front = NULL, *back = NULL;

	if (parc < 3)
		return;

	if (!(front = index(parv[2], CTCP_CHAR)))
		return;
	if (!(back = index(++front, CTCP_CHAR)))
		return;
	*back = '\0';
	if (!strcmp(front, "VERSION"))
		sendto_one(sptr, "NOTICE %s :%cVERSION %s%c", parv[0],
			  CTCP_CHAR, version, CTCP_CHAR);
	*back = CTCP_CHAR;
}
