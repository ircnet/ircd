/*
 *   IRC - Internet Relay Chat, common/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

#ifndef lint
static  char sccsid[] = "%W% %G% (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include <stdio.h>
#include <fcntl.h>

static	char	sendbuf[2048];
static	int	send_message __P((aClient *, char *, int));

#ifndef CLIENT_COMPILE
static	char	psendbuf[2048];
static	int	sentalong[MAXCONNECTIONS];
#endif

/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	If 'notice' is not NULL, it is assumed to be a format
**	for a message to local opers. I can contain only one
**	'%s', which will be replaced by the sockhost field of
**	the failing link.
**
**	Also, the notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static	int	dead_link(to, notice)
aClient *to;
char	*notice;
{
	if (IsHeld(to))
		return -1;
	to->flags |= FLAGS_DEADSOCKET;
	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
#ifndef CLIENT_COMPILE
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		sendto_flag(SCH_ERROR, notice, get_client_name(to, FALSE));
	Debug((DEBUG_ERROR, notice, get_client_name(to, FALSE)));
#endif
	return -1;
}

#ifndef CLIENT_COMPILE
/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we cant send it, it goes into the sendQ
**	-avalon
*/
void	flush_connections(fd)
int	fd;
{
#ifdef SENDQ_ALWAYS
	Reg	int	i;
	Reg	aClient *cptr;

	if (fd == me.fd)
	    {
		for (i = highest_fd; i >= 0; i--)
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				(void)send_queued(cptr);
	    }
	else if (fd >= 0 && (cptr = local[fd]) && DBufLength(&cptr->sendQ) > 0)
		(void)send_queued(cptr);
#endif
}
#endif

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
*/
static	int	send_message(to, msg, len)
aClient	*to;
char	*msg;	/* if msg is a null pointer, we are flushing connection */
int	len;
#ifdef SENDQ_ALWAYS
{
	int i;

	Debug((DEBUG_SEND,"Sending %s %d [%s] ", to->name, to->fd, msg));

	if (to->from)
		to = to->from;
	if (to->fd < 0)
	    {
		Debug((DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name));
	    }
#ifndef	CLIENT_COMPILE
	else if (IsMe(to))
	    {
		sendto_flag(SCH_ERROR, "Trying to send to myself! [%s]", msg);
		return 0;
	    }
#endif
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */
# ifndef	CLIENT_COMPILE
	if (DBufLength(&to->sendQ) > get_sendq(to))
	    {
#  ifdef HUB
		if (CBurst(to))
		    {
			aConfItem	*aconf = to->serv->nline;

			poolsize -= MaxSendq(aconf->class) >> 1;
			IncSendq(aconf->class);
			poolsize += MaxSendq(aconf->class) >> 1;
			sendto_flag(SCH_NOTICE, "New poolsize %d.",
				    poolsize);
		    }
		else if (IsServer(to))
			sendto_flag(SCH_ERROR,
				"Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		if (!CBurst(to))
		    {
			to->exitc = EXITC_SENDQ;
			return dead_link(to, "Max Sendq exceeded");
		    }
#  else
		if (IsServer(to))
			sendto_flag(SCH_ERROR,
				"Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		to->exitc = EXITC_SENDQ;
		return dead_link(to, "Max Sendq exceeded");
#  endif
	    }
	else
# endif
tryagain:
		if ((i = dbuf_put(&to->sendQ, msg, len)) < 0)
			if (i == -2 && CBurst(to))
			    {	/* poolsize was exceeded while connect burst */
				aConfItem	*aconf = to->serv->nline;

				poolsize -= MaxSendq(aconf->class) >> 1;
				IncSendq(aconf->class);
				poolsize += MaxSendq(aconf->class) >> 1;
				sendto_flag(SCH_NOTICE, "New poolsize %d. (r)",
					    poolsize);
				goto tryagain;
			    }
			else
			    {
				to->exitc = EXITC_MBUF;
				return dead_link(to,
					"Buffer allocation error for %s");
			    }
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	/*
	** This little bit is to stop the sendQ from growing too large when
	** there is no need for it to. Thus we call send_queued() every time
	** 2k has been added to the queue since the last non-fatal write.
	** Also stops us from deliberately building a large sendQ and then
	** trying to flood that link with data (possible during the net
	** relinking done by servers with a large load).
	*/
	if (DBufLength(&to->sendQ)/1024 > to->lastsq)
		send_queued(to);
	return 0;
}
#else /* SENDQ_ALWAYS */
{
	int	rlen = 0;

	Debug((DEBUG_SEND,"Sending %s %d [%s] ", to->name, to->fd, msg));

	if (to->from)
		to = to->from;
	if (to->fd < 0)
	    {
		Debug((DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name));
	    }
#ifndef	CLIENT_COMPILE
	else if (IsMe(to))
	    {
		sendto_flag(SCH_ERROR, "Trying to send to myself! [%s]", msg);
		return 0;
	    }
#endif
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */

	/*
	** DeliverIt can be called only if SendQ is empty...
	*/
	if ((DBufLength(&to->sendQ) == 0) &&
	    (rlen = deliver_it(to, msg, len)) < 0)
		return dead_link(to,"Write error to %s, closing link");
	else if (rlen < len)
	    {
		/*
		** Was unable to transfer all of the requested data. Queue
		** up the remainder for some later time...
		*/
# ifndef	CLIENT_COMPILE
		if (DBufLength(&to->sendQ) > get_sendq(to))
		    {
#  ifdef HUB
			if (IsServer(to) && CBurst(to))
			    {
				aClass	*cl = to->serv->nline->class;

				poolsize -= MaxSendq(cl) >> 1;
				IncSendq(cl);
				poolsize += MaxSendq(cl) >> 1;
			    }
			else if (IsServer(to))
#  else
			if (IsServer(to))
#  endif
			sendto_flag(SCH_ERROR,
				"Max SendQ limit exceeded for %s: %d > %d",
				 get_client_name(to, FALSE),
				 DBufLength(&to->sendQ), get_sendq(to));
			return dead_link(to, "Max Sendq exceeded");
		    }
		else
# endif
			if (dbuf_put(&to->sendQ,msg+rlen,len-rlen) < 0)
			    {
				to->exitc = EXITC_MBUF;
				return dead_link(to,
					"Buffer allocation error for %s");
			    }
	    }
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt != &me)
		to->acpt->sendM += 1;
	return 0;
}
#endif

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int	send_queued(to)
aClient *to;
{
	char	*msg;
	int	len, rlen;

	/*
	** Once socket is marked dead, we cannot start writing to it,
	** even if the error is removed...
	*/
	if (IsDead(to))
	    {
		/*
		** Actually, we should *NEVER* get here--something is
		** not working correct if send_queued is called for a
		** dead socket... --msa
		*/
#ifndef SENDQ_ALWAYS
		return dead_link(to, "send_queued called for a DEADSOCKET:%s");
#else
		return -1;
#endif
	    }
	while (DBufLength(&to->sendQ) > 0)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to,"Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/1024;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;
	    }

	return (IsDead(to)) ? -1 : 0;
}


/*
 *
 */
static	int	sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
	int	len;

	Debug((DEBUG_L10, "sendprep(%s)", pattern));
	len = irc_sprintf(sendbuf, pattern, p1, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
	if (len == -1)
		len = strlen(sendbuf);
	if (len > 510)
#ifdef	IRCII_KLUDGE
		len = 511;
#else
		len = 510;
	sendbuf[len++] = '\r';
#endif
	sendbuf[len++] = '\n';
	sendbuf[len] = '\0';
	return len;
}

#ifndef CLIENT_COMPILE
static	int	sendpreprep(to, from, pattern,
			    p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient	*to, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
	static	char	sender[HOSTLEN+NICKLEN+USERLEN+5];
	Reg	anUser	*user;
	char	*par;
	int	flag = 0, len;

	Debug((DEBUG_L10, "sendpreprep(%#x(%s),%#x(%s),%s)",
		to, to->name, from, from->name, pattern));
	par = p1;
	if (to && from && MyClient(to) && IsPerson(from) &&
	    !mycmp(par, from->name))
	    {
		user = from->user;
		(void)strcpy(sender, from->name);
		if (user)
		    {
			if (*user->username)
			    {
				(void)strcat(sender, "!");
				(void)strcat(sender, user->username);
			    }
			if (*user->host && !MyConnect(from))
			    {
				(void)strcat(sender, "@");
				(void)strcat(sender, user->host);
				flag = 1;
			    }
		    }
		/*
		** flag is used instead of index(sender, '@') for speed and
		** also since username/nick may have had a '@' in them. -avalon
		*/
		if (!flag && MyConnect(from) && *user->host)
		    {
			(void)strcat(sender, "@");
			if (IsUnixSocket(from))
				(void)strcat(sender, user->host);
			else
				(void)strcat(sender, from->sockhost);
		    }
		par = sender;
	    }
	len = irc_sprintf(psendbuf, pattern, par, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
	if (len == -1)
		len = strlen(psendbuf);
	if (len > 510)
#ifdef	IRCII_KLUDGE
		len = 511;
#else
		len = 510;
	psendbuf[len++] = '\r';
#endif
	psendbuf[len++] = '\n';
	psendbuf[len] = '\0';
	return len;
}
#endif /* CLIENT_COMPILE */

/*
** send message to single client
*/
int	sendto_one(to, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *to;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
	int	len;
#ifdef VMS
	extern int goodbye;
	
	if (StrEq("QUIT", pattern)) 
		goodbye = 1;
#endif
	len = sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
	(void)send_message(to, sendbuf, len);
	return len;
}

#ifndef CLIENT_COMPILE
static	anUser	ausr = { NULL, NULL, NULL, NULL, 0, 0, 0, 0, NULL, NULL, NULL,
			 NULL, "anonymous", "anonymous.", "anonymous."};

#ifndef KRYS
static	aClient	anon = { NULL, NULL, NULL, &ausr, NULL, NULL, 0, 0, 0, 0,
			 0,/*flags*/
			 &anon, -2, 0, STAT_CLIENT, "anonymous", "anonymous",
			 "anonymous identity hider", 0, "", 0,
			 {0, 0, NULL }, {0, 0, NULL },
			 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0, 0, 0, 0
#if defined(__STDC__)	/* hack around union{} initialization	-Vesa */
			 ,{0}, NULL, "", ""
#endif
			};
#else
static	aClient	anon = { NULL, NULL, NULL, &ausr, NULL, NULL, 0, 0,/*flags*/
			 &anon, -2, 0, STAT_CLIENT, "anonymous", "anonymous",
			 "anonymous identity hider", 0, "", 0,
			 {0, 0, NULL }, {0, 0, NULL },
			 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0, 0, 0, 0
#if defined(__STDC__)	/* hack around union{} initialization	-Vesa */
			 ,{0}, NULL, "", ""
#endif
			};
#endif

/*VARARGS*/
void	sendto_channel_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	Link	*lp;
	Reg	aClient *acptr, *lfrm = from;
	int	len1, len2 = 0;

	if (IsAnonymous(chptr) && IsClient(from))
	    {
		if (p1 && *p1 && !mycmp(p1, from->name))
			p1 = anon.name;
		lfrm = &anon;
	    }

	if (one != from && MyConnect(from) && IsRegisteredUser(from))
		sendto_prefix_one(from, from, pattern, p1, p2, p3, p4,
				  p5, p6, p7, p8);
	len1 = sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8);

	for (lp = chptr->clist; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || IsMe(acptr))
			continue;	/* ...was the one I should skip */
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
			if (!len2)
				len2 = sendpreprep(acptr, lfrm, pattern, p1,
						   p2, p3, p4, p5, p6, p7, p8);
			if (acptr != from)
				(void)send_message(acptr, psendbuf, len2);
		    }
		else if (!IsAnonymous(chptr) || /* Anonymous channel msgs */
			 !IsServer(acptr) ||    /* are not sent to old    */
			 !(acptr->serv->version == SV_OLD))/* server versions*/
			(void)send_message(acptr, sendbuf, len1);
	    }
	return;
}

/*
 * sendto_server_butone
 *
 * Send a message to all connected servers except the client 'one'.
 */
/*VARARGS*/
void	sendto_serv_butone(one, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i, len=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr)) {
			if (!len)
				len = sendprep(pattern, p1, p2, p3, p4, p5,
					       p6, p7, p8);
			(void)send_message(cptr, sendbuf, len);
	}
	return;
}

#ifndef NoV28Links
void	sendto_serv_v(one, ver, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one;
int	ver;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i, len=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr) &&
		    cptr->serv->version == ver) {
			if (!len)
				len = sendprep(pattern, p1, p2, p3, p4, p5,
					       p6, p7, p8);
			(void)send_message(cptr, sendbuf, len);
	}
	return;
}
#endif

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
/*VARARGS*/
void	sendto_common_channels(user, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *user;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i;
	Reg	aClient *cptr;
	Reg	Link	*lp;
	int	len = 0;

	if (MyConnect(user))
	    {
		len = sendpreprep(user, user, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
		(void)send_message(user, psendbuf, len);
	    }
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || IsServer(cptr) ||
		    user == cptr || !user->user)
			continue;
		for (lp = user->user->channel; lp; lp = lp->next)
			if (IsMember(cptr, lp->value.chptr) &&
			    !IsQuiet(lp->value.chptr))
			    {
#ifndef DEBUGMODE
				if (!len) /* This saves little cpu,
					     but breaks the debug code.. */
#endif
					len = sendpreprep(cptr, user, pattern,
							  p1, p2, p3, p4,
							  p5, p6, p7, p8);
				(void)send_message(cptr, psendbuf, len);
				break;
			    }
	    }
	return;
}

/*
 * sendto_channel_butserv
 *
 * Send a message to all members of a channel that are connected to this
 * server.
 */
/*VARARGS*/
void	sendto_channel_butserv(chptr, from, pattern, p1, p2, p3,
			       p4, p5, p6, p7, p8)
aChannel *chptr;
aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	Link	*lp;
	Reg	aClient	*acptr, *lfrm = from;
	int	len = 0;

	if (MyClient(from))
	    {	/* Always send to the client itself */
		sendto_prefix_one(from, from, pattern, p1, p2, p3, p4,
				  p5, p6, p7, p8);
		if (IsQuiet(chptr))	/* Really shut up.. */
			return;
	    }
	if (IsAnonymous(chptr) && IsClient(from))
	    {
		if (p1 && *p1 && !mycmp(p1, from->name))
			p1 = anon.name;
		lfrm = &anon;
	    }

	for (lp = chptr->members; lp; lp = lp->next)
		if (MyClient(acptr = lp->value.cptr) && acptr != from)
		    {
			if (!len)
				len = sendpreprep(acptr, lfrm, pattern, p1, p2,
						  p3, p4, p5, p6, p7, p8);
			(void)send_message(acptr, psendbuf, len);
		    }

	return;
}

/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static	int	match_it(one, mask, what)
aClient *one;
char	*mask;
int	what;
{
	switch (what)
	{
	case MATCH_HOST:
		return (matches(mask, one->user->host)==0);
	case MATCH_SERVER:
	default:
		return (matches(mask, one->user->server)==0);
	}
}

/*
 * sendto_match_servs
 *
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
/*VARARGS*/
void	sendto_match_servs(chptr, from, format, p1,p2,p3,p4,p5,p6,p7,p8,p9)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
	Reg	int	i, len=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return;
		if ((mask = (char *)rindex(chptr->chname, ':')))
			mask++;
	    }
	else
		mask = (char *)NULL;

	for (i = fdas.highest; i >= 0; i--)
	    {
		if (!(cptr = local[fdas.fd[i]]) || (cptr == from) ||
		    IsMe(cptr))
			continue;
		if (!BadPtr(mask) && matches(mask, cptr->name))
			continue;
#ifndef NoV28Links
		if (chptr && *chptr->chname == '+' &&
		    cptr->serv->version == SV_OLD)
			continue;
#endif
		if (!len)
			len = sendprep(format, p1, p2, p3, p4, p5, p6, p7,
				       p8, p9);
		(void)send_message(cptr, sendbuf, len);
	    }
}

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
/*VARARGS*/
void	sendto_match_butone(one, from, mask, what, pattern,
			    p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
int	what;
char	*mask, *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i;
	Reg	aClient *cptr, *acptr = NULL;
	Reg	anUser	*user;
  
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr) && what == MATCH_SERVER)
		    {
			for (user = usrtop; user; user = user->nextu)
				if (IsRegisteredUser(acptr = user->bcptr) &&
				    acptr->from == cptr &&
				    match_it(acptr, mask, what))
					break;
			if (!acptr)
				continue;
			sendto_prefix_one(cptr, from, pattern,
					  p1, p2, p3, p4, p5, p6, p7, p8);
		    }
		/* my client, does he match ? */
		else if (IsRegisteredUser(cptr) && match_it(cptr, mask, what))
			sendto_prefix_one(cptr, from, pattern,
					  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
	return;
}

/*
 * sendto_all_butone.
 *
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */
/*VARARGS*/
void	sendto_all_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i;
	Reg	aClient *cptr;
	int	len1 = 0, len2 = 0;

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			if (MyClient(cptr))
			    {
				if (!len1)
					len1 = sendpreprep(cptr, from, pattern,
							   p1, p2, p3, p4,
							   p5, p6, p7, p8);
				(void)send_message(cptr, psendbuf, len1);
			    }
			else
			    {
				if (!len2)
					len2 = sendprep(cptr, pattern,
							p1, p2, p3, p4,
							p5, p6, p7, p8);
				(void)send_message(cptr, sendbuf, len2);
			    }

	return;
}

/*
** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
/*VARARGS*/
void	sendto_ops_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	Reg	int	i;
	Reg	aClient *cptr;

	bzero((char *)&sentalong[0], sizeof(int) * MAXCONNECTIONS);
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if ((IsPerson(cptr) && !SendWallops(cptr)) || IsMe(cptr))
			continue;
		if (MyClient(cptr) && !(IsServer(from) || IsMe(from)))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
	return;
}

/*
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */
/*VARARGS*/
void	sendto_prefix_one(to, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
Reg	aClient *to;
Reg	aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	int	len;

	len = sendpreprep(to, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8);
	send_message(to, psendbuf, len);
	return;
}


/*
 * sends a message to a server-owned channel
 */
static	SChan	svchans[SCH_MAX] = {
	{ SCH_ERROR,	"&ERRORS",	NULL },
	{ SCH_NOTICE,	"&NOTICES",	NULL },
	{ SCH_KILL,	"&KILLS",	NULL },
	{ SCH_CHAN,	"&CHANNEL",	NULL },
	{ SCH_NUM,	"&NUMERICS",	NULL },
	{ SCH_SERVER,	"&SERVERS",	NULL },
	{ SCH_HASH,	"&HASH",	NULL },
	{ SCH_LOCAL,	"&LOCAL",	NULL },
};


void	setup_svchans()
{
	int	i;
	SChan	*shptr;

	for (i = SCH_MAX, shptr = svchans + (i - 1); i > 0; i--, shptr--)
		shptr->svc_ptr = find_channel(shptr->svc_chname, NULL);
}


void	sendto_flag(chan, pattern, p1, p2, p3, p4, p5, p6)
u_int	chan;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6;
{
	Reg	aChannel *chptr = NULL;
	SChan	*shptr;
	char	nbuf[256];

	if (chan < 1 || chan > SCH_MAX)
		chan = SCH_NOTICE;
	shptr = svchans + (chan - 1);

	if ((chptr = shptr->svc_ptr))
	    {
		(void)strcpy(nbuf, ":%s NOTICE %s :");
		(void)strcat(nbuf, pattern);
		sendto_channel_butserv(chptr, &me, nbuf, ME, chptr->chname,
				       p1, p2, p3, p4, p5, p6);
	    }
	return;
}

void	sendto_flog(ftime, msg, duration, username, hostname, ident, exitc)
char	*ftime, *msg, *username, *hostname, *ident, *exitc;
time_t	duration;
{
	char	linebuf[160];
	int	logfile;

	/*
	 * This conditional makes the logfile active only after
	 * it's been created, thus logging can be turned off by
	 * removing the file.
	 *
	 * stop NFS hangs...most systems should be able to
	 * file in 3 seconds. -avalon (curtesy of wumpus)
	 */
	(void)alarm(3);
	if (
#ifdef	FNAME_USERLOG
	    (duration && 
	     (logfile = open(FNAME_USERLOG, O_WRONLY|O_APPEND)) != -1)
# ifdef	FNAME_CONNLOG
	    ||
# endif
#endif
#ifdef	FNAME_CONNLOG
	    (!duration && 
	     (logfile = open(FNAME_CONNLOG, O_WRONLY|O_APPEND)) != -1)
#else
# ifndef	FNAME_USERLOG
	    0
# endif
#endif
	   )
	    {
		(void)alarm(0);
		if (duration)
			(void)sprintf(linebuf,
				     "%s (%3d:%02d:%02d): %s@%s [%s] %c\n",
				     ftime, (int) (duration / 3600),
				     (int) ((duration % 3600) / 60),
				     (int) (duration % 60),
				     username, hostname, ident, *exitc);
		else
			(void)sprintf(linebuf, "%s (%s): %s@%s [%s] %c\n",
				      ftime, msg, username, hostname, ident,
				      *exitc);
		(void)alarm(3);
		(void)write(logfile, linebuf, strlen(linebuf));
		(void)alarm(0);
		(void)close(logfile);
	    }
	(void)alarm(0);
}
#endif /* CLIENT_COMPILE */
