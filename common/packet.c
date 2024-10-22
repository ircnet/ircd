/************************************************************************
 *   IRC - Internet Relay Chat, common/packet.c
 *   Copyright (C) 1990  Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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
static const volatile char rcsid[] = "@(#)$Id: packet.c,v 1.13 2004/10/01 20:22:12 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define PACKET_C
#include "s_externs.h"
#undef PACKET_C

/*
** dopacket
**	cptr - pointer to client structure for which the buffer data
**	       applies.
**	buffer - pointr to the buffer containing the newly read data
**	length - number of valid bytes of data in the buffer
**
**	The buffer might be partially or totally zipped.
**	At the beginning of the compressed flow, it is possible that
**	an uncompressed ERROR message will be found.  This occurs when
**	the connection fails on the other server before switching
**	to compressed mode.
**
** Note:
**	It is implicitly assumed that dopacket is called only
**	with cptr of "local" variation, which contains all the
**	necessary fields (buffer etc..)
*/
int dopacket(aClient *cptr, char *buffer, int length)
{
	Reg char *ch1;
	Reg char *ch2, *bufptr;
	aClient *acpt = cptr->acpt;
	int r = 1;
#ifdef ZIP_LINKS
	int unzipped = 0;
#endif

	me.receiveB += length; /* Update bytes received */
	cptr->receiveB += length;
	if (acpt != &me)
	{
		acpt->receiveB += length;
	}

	bufptr = cptr->buffer;
	ch1 = bufptr + cptr->count;
	ch2 = buffer;

#ifdef ZIP_LINKS
	while ((length > 0 && ch2) || ((cptr->flags & FLAGS_ZIP) &&
								   (cptr->zip->in->avail_in ||
									!unzipped)))
#else
	while (length > 0 && ch2)
#endif
	{
		Reg char c;

#ifdef ZIP_LINKS
		if (cptr->flags & FLAGS_ZIPSTART)
		{
			/*
			** beginning of server connection, the buffer
			** contained PASS/SERVER and is now zipped!
			** Ignore the '\n' that should be here.
			*/
			if (*ch2 == '\n') /* also check \r ? */
			{
				ch2++;
				length--;
				cptr->flags &= ~FLAGS_ZIPSTART;
			}
			if (length == 0)
				return 1;
		}

		if ((cptr->flags & FLAGS_ZIP) && !(unzipped && length))
		{
			/* uncompressed buffer first */
			unzipped = length; /* length is register, store
						  temp in unzipped */
			ch2 = unzip_packet(cptr, ch2, &unzipped);
			length = unzipped;
			unzipped = 1;
			if (length == -1)
				return exit_client(cptr, cptr, &me,
								   "fatal error in unzip_packet()");
			if (length == 0 || *ch2 == '\0')
				break;
		}
#endif
		length--;
		c = (*ch1 = *ch2++);
		/*
		 * Yuck.  Stuck.  To make sure we stay backward compatible,
		 * we must assume that either CR or LF terminates the message
		 * and not CR-LF.  By allowing CR or LF (alone) into the body
		 * of messages, backward compatibility is lost and major
		 * problems will arise. - Avalon
		 */
		if ((c <= '\r') && (c == '\n' || c == '\r'))
		{
			if (ch1 == bufptr)
				continue; /* Skip extra LF/CR's */
			*ch1 = '\0';
			me.receiveM += 1; /* Update messages received */
			cptr->receiveM += 1;
			if (cptr->acpt != &me)
				cptr->acpt->receiveM += 1;
			cptr->count = 0; /* ...just in case parse returns with
					 ** FLUSH_BUFFER without removing the
					 ** structure pointed by cptr... --msa
					 */
			if ((r = parse(cptr, bufptr, ch1)) ==
				FLUSH_BUFFER)
				/*
				** FLUSH_BUFFER means actually that cptr
				** structure *does* not exist anymore!!! --msa
				*/
				return FLUSH_BUFFER;
			/*
			** Socket is dead so exit (which always returns with
			** FLUSH_BUFFER here).  - avalon
			*/
			if (IsDead(cptr))
			{
				if (cptr->exitc == EXITC_REG)
					cptr->exitc = EXITC_DEAD;
				return exit_client(cptr, cptr, &me,
								   (cptr->exitc == EXITC_SENDQ) ? "Max SendQ exceeded" : "Dead Socket");
			}
			/*
			** Something is wrong, really wrong, and nothing
			** else should be allowed to be parsed!
			** This covers a bug which is somewhere else,
			** since no decent server would send such thing
			** as an unknown command. -krys
			*/
			if (IsServer(cptr) && (cptr->flags & FLAGS_UNKCMD))
				break;
			ch1 = bufptr;
		}
		else if (ch1 < bufptr + (sizeof(cptr->buffer) - 1))
			ch1++; /* There is always room for the null */
	}
	cptr->count = ch1 - bufptr;
	return r;
}
