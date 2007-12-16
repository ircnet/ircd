/*
 *   IRC - Internet Relay Chat, ircd/s_send.c
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
static const volatile char rcsid[] = "@(#)$Id: s_send.c,v 1.11 2007/12/16 06:10:13 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_SEND_C
#include "s_externs.h"
#undef S_SEND_C

static	char	oldprefixbuf[512];	/* old style server-server prefix */
static	char	newprefixbuf[512];	/* new style server-server prefix */
static	char	prefixbuf[512];		/* server-client prefix */
static	char	suffixbuf[2048];	/* suffix */

static	int	oldplen, newplen, plen, slen; /* length of the above buffers */
static	int	maxplen, lastmax;

#define	CLEAR_LENGTHS	oldplen = newplen = plen = slen = maxplen = lastmax =0;

/*
** esend_message
**	Wrapper for send_message() that deals with the multiple buffers
**	we now have to deal with UIDs.
*/
static	void	esend_message(aClient *to)
{
	if (maxplen != lastmax)
	{
		if (maxplen + slen > 512)
		{
			slen = 510 - maxplen;
			suffixbuf[slen++] = '\r';
			suffixbuf[slen++] = '\n';
			suffixbuf[slen] = '0';
		}
		lastmax = maxplen;
	}

	if (/*ST_UID*/IsServer(to) && newplen > 0)
	{
		send_message(to, newprefixbuf, newplen);
		if (slen)
		{
			send_message(to, suffixbuf, slen);
		}
	}
	else
	{
		if (IsServer(to))
		{
		    send_message(to, oldprefixbuf, oldplen);
		}
		else
		{
			send_message(to, prefixbuf, plen);
		}
		if (slen)
		{
			send_message(to, suffixbuf, slen);
		}
	}
}

/*
** build_old_prefix
**	function responsible for filling oldprefixbuf using pre-UID conventions
*/
static void	build_old_prefix(aClient *orig, char *imsg, aClient *dest,
	char *dname)
{
	if (oldplen != 0)
	{
		return;
	}
	if (dname == NULL)
	{
		dname = dest->name;
	}
	oldplen = sprintf(oldprefixbuf, ":%s %s %s", orig->name, imsg, dname);
	if (oldplen > maxplen)
	{
		maxplen = oldplen;
	}
}

/*
** build_new_prefix
**	function responsible for filling oldprefixbuf with the origin and/or
**	destination's UID if they exist.
*/
static void	build_new_prefix(aClient *orig, char *imsg, aClient *dest,
	char *dname)
{
	char *oname = NULL;

	if (newplen != 0)
	{
		return;
	}
	if (IsRegisteredUser(orig) && orig->user->uid[0])
	{
		oname = orig->user->uid;
	}
	if (dname == NULL)
	{
		if (IsRegisteredUser(dest) && dest->user->uid[0])
		{
			dname = dest->user->uid;
		}
		else if (oname)
		{
			dname = dest->name;
		}
		else
		{
			newplen = -1;
			return; /* no uid anywhere, bail out */
		}
	}
	newplen = sprintf(newprefixbuf, ":%s %s %s", oname, imsg, dname);
	if (newplen > maxplen)
	{
	    maxplen = newplen;
	}
}

#if 0 /* seems unused */
/*
** build_prefix
**	function responsible for filling prefixbuf
*/
static void	build_prefix(aClient *orig, char *imsg, aClient *dest,
	char *dname)
{
	char *cp = prefixbuf, *ch;

	if (plen != 0)
	{
		return;
	}
	if (dname == NULL)
	{
		dname = dest->name;
	}

	if (IsPerson(orig))
	{
		*cp++ = ':';

		ch = orig->name; while (*ch) *cp++ = *ch++;
		*cp++ = '!';
		ch = orig->user->username; while (*ch) *cp++ = *ch++;
		*cp++ = '@';
		ch = orig->user->host; while (*ch) *cp++ = *ch++;

		*cp++ = ' ';
		while (*imsg) *cp++ = *imsg++;

		*cp++ = ' ';
		ch = (dname) ? dname : dest->name; while (*ch) *cp++ = *ch++;

		*cp++ = ' ';
		*cp = '0';
		plen = cp - prefixbuf;
	}
	else
	{
		if (dname == NULL)
		{
			dname = dest->name;
		}
		plen = sprintf(prefixbuf, ":%s %s %s", orig->name, imsg, dname);
	}

	if (plen > maxplen)
	{
		maxplen = plen;
	}
}
#endif

/*
** build_suffix
**	function responsible for filling suffixbuf.
*/
static void	build_suffix(char *format, va_list va)
{
	if (slen)
	{
		return;
	}
	
	slen = vsprintf(suffixbuf, format, va);
	suffixbuf[slen++] = '\r';
	suffixbuf[slen++] = '\n';
	suffixbuf[slen] = '0';
}

/*
** ALL the esendto_*() functions follow about the same parameter order:
**
**	origin (remote when applicable)
**	destination (remote when applicable, eventually NULL)
**	destination name (if not aClient)
**	IRC message
**	but one (exception to multiple destinations)
**	eventual extra delivery related arguments
**
**	suffix format
**	suffix parameters
*/

/*
** esendto_one()
**	send a message to a single client
*/
void	esendto_one(aClient *orig, aClient *dest, char *imsg, char *fmt, ...)
{
	va_list va;

	CLEAR_LENGTHS;
	if (/*ST_UID*/IsServer(dest->from))
	{
		build_new_prefix(orig, imsg, dest, NULL);
	}
	if (newplen <= 0)
	{
		build_old_prefix(orig, imsg, dest, NULL);
	}
	va_start(va, fmt);
	build_suffix(fmt, va);
	va_end(va);
	esend_message(dest->from);
}

/*
** esendto_serv_butone
** 	send message to all connected servers except 'one'
*/
void	esendto_serv_butone(aClient *orig, aClient *dest, char *dname,
		char *imsg, aClient *one, char *fmt, ...)
{
	int	i;
	aClient *acptr;

	CLEAR_LENGTHS;
	for (i = fdas.highest; i >= 0; i--)
	{
		if ((acptr = local[fdas.fd[i]]) &&
			(!one || acptr != one->from) && !IsMe(acptr))
		{
			if (newplen == 0 && /*ST_UID*/IsServer(acptr))
				build_new_prefix(orig, imsg, dest, dname);
			if (oldplen == 0 && (/*ST_NOTUID*/0 || newplen <= 0))
				build_old_prefix(orig, imsg, dest, dname);
			if (slen == 0)
			{
				va_list va;
				va_start(va, fmt);
				build_suffix(fmt, va);
				va_end(va);
			}
			esend_message(acptr);
		}
	}
}

/*
** esendto_channel_butone
** 	send message to all connected servers except 'one'
*/
void	esendto_channel_butone(aClient *orig, char *imsg, aClient *one,
		       aChannel *chptr, char *fmt, ...)
{
	Link	*lp;
	aClient	*acptr;

	CLEAR_LENGTHS;

	for (lp = chptr->clist; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->from == one || IsMe(acptr))
		{
			continue;	/* ...was the one I should skip */
		}
		if (acptr == orig)
		{
			continue;
		}

		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			/* to local users */
			if (plen == 0)
			{
				plen = sprintf(prefixbuf,
					":anonymous!anonymous@"
					"anonymous. %s %s", imsg,
					chptr->chname);
			}
		}
		else
		{
			/* to servers */
			if (newplen == 0 && /*ST_UID*/IsServer(acptr))
			{
				build_new_prefix(orig, imsg, NULL,
					chptr->chname);
			}
			if (oldplen == 0 && (/*ST_NOTUID*/0 || newplen <= 0))
			{
				build_old_prefix(orig, imsg, NULL,
					chptr->chname);
			}
		}

		if (slen == 0)
		{
			va_list va;
			va_start(va, fmt);
			build_suffix(fmt, va);
			va_end(va);
		}

		esend_message(acptr);
	}
}

/*
** sendto_match_servs
**	send to all servers which match the mask at the end of a channel name
**	(if there is a mask present) or to all if no mask.
*/
void	esendto_match_servs(aClient *orig, char *imsg, aChannel *chptr,
		char *fmt, ...)
{
	int	i;
	aClient	*cptr;
	char	*mask;

	CLEAR_LENGTHS;

	if (chptr)
	{
		if (*chptr->chname == '&')
		{
		    return;
		}
		if ((mask = get_channelmask(chptr->chname)))
		{
		    mask++;
		}
	}
	else
	{
		mask = NULL;
	}

	for (i = fdas.highest; i >= 0; i--)
	{
		if (!(cptr = local[fdas.fd[i]]) || (cptr == orig) || IsMe(cptr))
		{
			continue;
		}
		if (!BadPtr(mask) && match(mask, cptr->name))
		{
		    continue;
		}
		if (newplen == 0 && /*ST_UID*/IsServer(cptr))
		{
		    build_new_prefix(orig, imsg, NULL, chptr->chname);
		}
		if (oldplen == 0 && (/*ST_NOTUID*/0 || newplen <= 0))
		{
		    build_old_prefix(orig, imsg, NULL, chptr->chname);
		}
		if (slen == 0)
		{
			va_list va;
			va_start(va, fmt);
			build_suffix(fmt, va);
			va_end(va);
		}

		esend_message(cptr);
	}
}

