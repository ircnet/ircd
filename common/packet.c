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
static  char rcsid[] = "@(#)$Id: packet.c,v 1.4 1998/04/05 00:14:32 kalt Exp $";
#endif

#include "os.h"
#ifndef CLIENT_COMPILE
# include "s_defines.h"
#else
# include "c_defines.h"
#endif
#define PACKET_C
#ifndef CLIENT_COMPILE
# include "s_externs.h"
#else
# include "c_externs.h"
#endif
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
int	dopacket(cptr, buffer, length)
Reg	aClient *cptr;
char	*buffer;
Reg	int	length;
{
	Reg	char	*ch1;
	Reg	char	*ch2, *bufptr;
	aClient	*acpt = cptr->acpt;
	int	r = 1;
#ifdef ZIP_LINKS
	int	zipped = 0;
#endif
 
	me.receiveB += length; /* Update bytes received */
	cptr->receiveB += length;
	if (cptr->receiveB > 1023)
	    {
		cptr->receiveK += (cptr->receiveB >> 10);
		cptr->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	    }
	if (acpt != &me)
	    {
		acpt->receiveB += length;
		if (acpt->receiveB > 1023)
		    {
			acpt->receiveK += (acpt->receiveB >> 10);
			acpt->receiveB &= 0x03ff;
		    }
	    }
	else if (me.receiveB > 1023)
	    {
		me.receiveK += (me.receiveB >> 10);
		me.receiveB &= 0x03ff;
	    }

	bufptr = cptr->buffer;
	ch1 = bufptr + cptr->count;
	ch2 = buffer;
#ifdef	ZIP_LINKS
        if (cptr->flags & FLAGS_ZIPSTART)
            {
                if (*ch2 == '\n') /* also check \r ? */
                    {
                        ch2++;
                        length--;
                    }   
                cptr->flags &= ~FLAGS_ZIPSTART;
            }   
	if (cptr->flags & FLAGS_ZIP)
	    {
		/* uncompressed buffer first */
		zipped = length;
		ch2 = unzip_packet(cptr, ch2, &zipped);
		length = zipped;
		zipped = 1;
		if (length == -1)
			return exit_client(cptr, cptr, &me,
					   "fatal error in unzip_packet()");
	    }
#endif

	while (--length >= 0 && ch2)
	    {
		Reg	char	c;

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
#ifndef CLIENT_COMPILE
			/*
			** Socket is dead so exit (which always returns with
			** FLUSH_BUFFER here).  - avalon
			*/
			if (cptr->flags & FLAGS_DEADSOCKET)
			    {
				if (cptr->exitc == EXITC_REG)
					cptr->exitc = EXITC_DEAD;
				return exit_client(cptr, cptr, &me,
						   "Dead Socket");
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
#endif
#ifdef	ZIP_LINKS
			if ((cptr->flags & FLAGS_ZIP) && (zipped == 0) &&
			    (length > 0))
			    {
				/*
				** beginning of server connection, the buffer
				** contained PASS/SERVER and is now zipped!
				** Ignore the '\n' that should be here.
				*/
				zipped = length;
				if (zipped > 0 && *ch2 == '\n')
				    {
					ch2++;
					zipped--;
				    }
				cptr->flags &= ~FLAGS_ZIPSTART;
				ch2 = unzip_packet(cptr, ch2, &zipped);
				length = zipped;
				zipped = 1;
				if (length == -1)
					return exit_client(cptr, cptr, &me,
					   "fatal error in unzip_packet()");
			    }
#endif
			ch1 = bufptr;
		    }
		else if (ch1 < bufptr + (sizeof(cptr->buffer)-1))
			ch1++; /* There is always room for the null */
	    }
	cptr->count = ch1 - bufptr;
	return r;
}
