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

#ifndef lint
static  char rcsid[] = "@(#)$Id: send.c,v 1.39 1999/07/21 22:57:40 kalt Exp $";
#endif

#include "os.h"
#ifndef CLIENT_COMPILE
# include "s_defines.h"
#else
# include "c_defines.h"
#endif
#define SEND_C
#ifndef CLIENT_COMPILE
# include "s_externs.h"
#else
# include "c_externs.h"
#endif
#undef SEND_C

static	char	sendbuf[2048];
static	int	send_message __P((aClient *, char *, int));

#if USE_STDARG
static void	vsendto_prefix_one(aClient *, aClient *, char *, va_list);
#endif


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
**	for a message to local opers. It can contain only one
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
	 * If because of BUFFERPOOL problem then clean dbufs now so that
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
** flush_fdary
**      Used to empty all output buffers for connections in fdary.
*/
void    flush_fdary(fdp)
FdAry   *fdp;
{
        int     i;
        aClient *cptr;

        for (i = 0; i <= fdp->highest; i++)
            {
                if (!(cptr = local[fdp->fd[i]]))
                        continue;
                if (!IsRegistered(cptr)) /* is this needed?? -kalt */
                        continue;
                if (DBufLength(&cptr->sendQ) > 0)
                        (void)send_queued(cptr);
            }
}

/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we can't send it, it goes into the sendQ
**	-avalon
*/
void	flush_connections(fd)
int	fd;
{
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
}
#endif

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
**	if ZIP_LINKS is defined, the message will eventually be compressed,
**	anything stored in the sendQ is compressed.
*/
static	int	send_message(to, msg, len)
aClient	*to;
char	*msg;	/* if msg is a null pointer, we are flushing connection */
int	len;
#if !defined(CLIENT_COMPILE)
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
	if (IsMe(to))
	    {
		sendto_flag(SCH_ERROR, "Trying to send to myself! [%s]", msg);
		return 0;
	    }
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */
	if (DBufLength(&to->sendQ) > get_sendq(to))
	    {
# ifdef HUB
		if (CBurst(to))
		    {
			aConfItem	*aconf = to->serv->nline;

			poolsize -= MaxSendq(aconf->class) >> 1;
			IncSendq(aconf->class);
			poolsize += MaxSendq(aconf->class) >> 1;
			sendto_flag(SCH_NOTICE,
				    "New poolsize %d. (sendq adjusted)",
				    poolsize);
			istat.is_dbufmore++;
		    }
		else if (IsServer(to) || IsService(to))
			sendto_flag(SCH_ERROR,
				"Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		if (!CBurst(to))
		    {
			to->exitc = EXITC_SENDQ;
			return dead_link(to, "Max Sendq exceeded");
		    }
# else /* HUB */
		if (IsService(to) || IsServer(to))
			sendto_flag(SCH_ERROR,
				"Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		to->exitc = EXITC_SENDQ;
		return dead_link(to, "Max Sendq exceeded");
# endif /* HUB */
	    }
	else
	    {
tryagain:
# ifdef	ZIP_LINKS
	        /*
		** data is first stored in to->zip->outbuf until
		** it's big enough to be compressed and stored in the sendq.
		** send_queued is then responsible to never let the sendQ
		** be empty and to->zip->outbuf not empty.
		*/
		if (to->flags & FLAGS_ZIP)
			msg = zip_buffer(to, msg, &len, 0);

		if (len && (i = dbuf_put(&to->sendQ, msg, len)) < 0)
# else 	/* ZIP_LINKS */
		if ((i = dbuf_put(&to->sendQ, msg, len)) < 0)
# endif	/* ZIP_LINKS */
			if (i == -2 && CBurst(to))
			    {	/* poolsize was exceeded while connect burst */
				aConfItem	*aconf = to->serv->nline;

				poolsize -= MaxSendq(aconf->class) >> 1;
				IncSendq(aconf->class);
				poolsize += MaxSendq(aconf->class) >> 1;
				sendto_flag(SCH_NOTICE,
					    "New poolsize %d. (reached)",
					    poolsize);
				istat.is_dbufmore++;
				goto tryagain;
			    }
			else
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
#else /* CLIENT_COMPILE */
{
	int	rlen = 0, i;

	Debug((DEBUG_SEND,"Sending %s %d [%s] ", to->name, to->fd, msg));

	if (to->from)
		to = to->from;
	if (to->fd < 0)
	    {
		Debug((DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name));
	    }
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */

	if ((rlen = deliver_it(to, msg, len)) < 0 && rlen < len)
		return dead_link(to,"Write error to %s, closing link");
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
	int	len, rlen, more = 0;

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
		return -1;
	    }
#ifdef	ZIP_LINKS
	/*
	** Here, we must make sure than nothing will be left in to->zip->outbuf
	** This buffer needs to be compressed and sent if all the sendQ is sent
	*/
	if ((to->flags & FLAGS_ZIP) && to->zip->outcount)
	    {
		if (DBufLength(&to->sendQ) > 0)
			more = 1;
		else
		    {
			msg = zip_buffer(to, NULL, &len, 1);
			
			if (len == -1)
			       return dead_link(to,
						"fatal error in zip_buffer()");

			if (dbuf_put(&to->sendQ, msg, len) < 0)
			    {
				to->exitc = EXITC_MBUF;
				return dead_link(to,
					 "Buffer allocation error for %s");
			    }
		    }
	    }
#endif
	while (DBufLength(&to->sendQ) > 0 || more)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to,"Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/1024;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;

#ifdef	ZIP_LINKS
		if (DBufLength(&to->sendQ) == 0 && more)
		    {
			/*
			** The sendQ is now empty, compress what's left
			** uncompressed and try to send it too
			*/
			more = 0;
			msg = zip_buffer(to, NULL, &len, 1);

			if (len == -1)
			       return dead_link(to,
						"fatal error in zip_buffer()");

			if (dbuf_put(&to->sendQ, msg, len) < 0)
			    {
				to->exitc = EXITC_MBUF;
				return dead_link(to,
					 "Buffer allocation error for %s");
			    }
		    }
#endif
	    }

	return (IsDead(to)) ? -1 : 0;
}


#ifndef CLIENT_COMPILE
static	anUser	ausr = { NULL, NULL, NULL, NULL, 0, 0, 0, 0, NULL,
			 NULL, "anonymous", "anonymous.", "anonymous."};

static	aClient	anon = { NULL, NULL, NULL, &ausr, NULL, NULL, 0, 0,/*flags*/
			 &anon, -2, 0, STAT_CLIENT, "anonymous", "anonymous",
			 "anonymous identity hider", 0, "",
# ifdef	ZIP_LINKS
			 NULL,
# endif
			 0, {0, 0, NULL }, {0, 0, NULL },
			 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, 0, NULL, 0
# if defined(__STDC__)	/* hack around union{} initialization	-Vesa */
			 ,{0}, NULL, "", ""
# endif
			};
#endif

/*
 *
 */
#if ! USE_STDARG
static	int	sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
static	int	vsendprep(char *pattern, va_list va)
#endif
{
	int	len;

	Debug((DEBUG_L10, "sendprep(%s)", pattern));
#if ! USE_STDARG
	len = irc_sprintf(sendbuf, pattern, p1, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
	if (len == -1)
		len = strlen(sendbuf);
#else
	len = vsprintf(sendbuf, pattern, va);
#endif
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
#if ! USE_STDARG
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

#else /* USE_STDARG */

static	int	vsendpreprep(aClient *to, aClient *from, char *pattern, va_list va)
{
	Reg	anUser	*user;
	int	flag = 0, len;

	Debug((DEBUG_L10, "sendpreprep(%#x(%s),%#x(%s),%s)",
		to, to->name, from, from->name, pattern));
	if (to && from && MyClient(to) && IsPerson(from) &&
	    !strncmp(pattern, ":%s", 3))
	    {
		char	*par = va_arg(va, char *);
		if (from == &anon || !mycmp(par, from->name))
		    {
			user = from->user;
			(void)strcpy(psendbuf, ":");
			(void)strcat(psendbuf, from->name);
			if (user)
			    {
				if (*user->username)
				    {
					(void)strcat(psendbuf, "!");
					(void)strcat(psendbuf, user->username);
				    }
				if (*user->host && !MyConnect(from))
				    {
					(void)strcat(psendbuf, "@");
					(void)strcat(psendbuf, user->host);
					flag = 1;
				    }
			    }
			/*
			** flag is used instead of index(newpat, '@') for speed and
			** also since username/nick may have had a '@' in them. -avalon
			*/
			if (!flag && MyConnect(from) && *user->host)
			    {
				(void)strcat(psendbuf, "@");
				if (IsUnixSocket(from))
				    (void)strcat(psendbuf, user->host);
				else
				    (void)strcat(psendbuf, from->sockhost);
			    }
		    }
		else
		    {
			(void)strcpy(psendbuf, ":");
			(void)strcat(psendbuf, par);
		    }

		len = strlen(psendbuf);
		len += vsprintf(psendbuf+len, pattern+3, va);
	    }
	else
		len = vsprintf(psendbuf, pattern, va);

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
#endif /* USE_STDARG */
#endif /* CLIENT_COMPILE */

/*
** send message to single client
*/
#if ! USE_STDARG
int	sendto_one(to, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *to;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
int	vsendto_one(aClient *to, char *pattern, va_list va)
#endif
{
	int	len;

#if ! USE_STDARG
	len = sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
#else
	len = vsendprep(pattern, va);
#endif

	(void)send_message(to, sendbuf, len);
	return len;
}

#if USE_STDARG
int	sendto_one(aClient *to, char *pattern, ...)
{
	int	len;
	va_list	va;
	va_start(va, pattern);
	len = vsendto_one(to, pattern, va);
	va_end(va);
	return len;
}
#endif

#ifndef CLIENT_COMPILE
#if ! USE_STDARG
/*VARARGS*/
void	sendto_channel_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *one, *from;
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr, char *pattern, ...)
#endif
{
	Reg	Link	*lp;
	Reg	aClient *acptr, *lfrm = from;
	int	len1, len2 = 0;

	if (IsAnonymous(chptr) && IsClient(from))
	    {
#if ! USE_STDARG
		if (p1 && *p1 && !mycmp(p1, from->name))
			p1 = anon.name;
#endif
		lfrm = &anon;
	    }

	if (one != from && MyConnect(from) && IsRegisteredUser(from))
	    {
		/* useless junk? */
#if ! USE_STDARG
		sendto_prefix_one(from, from, pattern, p1, p2, p3, p4,
				  p5, p6, p7, p8, p9, p10, p11);
#else
		va_list	va;
		va_start(va, pattern);
		vsendto_prefix_one(from, from, pattern, va);
		va_end(va);
#endif
	    }

#if ! USE_STDARG
	len1 = sendprep(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
#else
	{
		va_list	va;
		va_start(va, pattern);
		len1 = vsendprep(pattern, va);
		va_end(va);
	}
#endif


	for (lp = chptr->clist; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || IsMe(acptr))
			continue;	/* ...was the one I should skip */
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
			if (!len2)
			    {
#if ! USE_STDARG
				len2 = sendpreprep(acptr, lfrm, pattern, p1,
						   p2, p3, p4, p5, p6, p7, p8,
						   p9, p10, p11);
#else
				va_list	va;
				va_start(va, pattern);
				len2 = vsendpreprep(acptr, lfrm, pattern, va);
				va_end(va);
#endif
			    }

			if (acptr != from)
				(void)send_message(acptr, psendbuf, len2);
		    }
		else
			(void)send_message(acptr, sendbuf, len1);
	    }
	return;
}

/*
 * sendto_server_butone
 *
 * Send a message to all connected servers except the client 'one'.
 */
#if ! USE_STDARG
/*VARARGS*/
void	sendto_serv_butone(one, pattern, p1, p2, p3, p4,p5,p6,p7,p8,p9,p10,p11)
aClient *one;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_serv_butone(aClient *one, char *pattern, ...)
#endif
{
	Reg	int	i, len=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr)) {
			if (!len)
			    {
#if ! USE_STDARG
				len = sendprep(pattern, p1, p2, p3, p4, p5,
					       p6, p7, p8, p9, p10, p11);
#else
				va_list	va;
				va_start(va, pattern);
				len = vsendprep(pattern, va);
				va_end(va);
#endif
			    }
			(void)send_message(cptr, sendbuf, len);
	}
	return;
}

#if ! USE_STDARG
int
sendto_serv_v(one, ver, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *one;
int	ver;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
int
sendto_serv_v(aClient *one, int ver, char *pattern, ...)
#endif
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr))
			if (cptr->serv->version & ver)
			    {
				if (!len)
				    {
#if ! USE_STDARG
					len = sendprep(pattern, p1, p2, p3, p4,
						       p5, p6, p7, p8, p9, p10,
						       p11);
#else
					va_list	va;
					va_start(va, pattern);
					len = vsendprep(pattern, va);
					va_end(va);
#endif
				    }
				(void)send_message(cptr, sendbuf, len);
			    }
			else
				rc = 1;
	return rc;
}

#if ! USE_STDARG
int
sendto_serv_notv(one, ver, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9,p10,p11)
aClient *one;
int	ver;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
int
sendto_serv_notv(aClient *one, int ver, char *pattern, ...)
#endif
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr))
			if ((cptr->serv->version & ver) == 0)
			    {
				if (!len)
				    {
#if ! USE_STDARG
					len = sendprep(pattern, p1, p2, p3, p4,
						       p5, p6, p7, p8, p9, p10,
						       p11);
#else
					va_list	va;
					va_start(va, pattern);
					len = vsendprep(pattern, va);
					va_end(va);
#endif
				    }
				(void)send_message(cptr, sendbuf, len);
			    }
			else
				rc = 1;
	return rc;
}

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user, except for channels set Quiet or Anonymous
 * The calling procedure must take the necessary steps for such channels.
 */
#if ! USE_STDARG
/*VARARGS*/
void	sendto_common_channels(user,pattern,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11)
aClient *user;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_common_channels(aClient *user, char *pattern, ...)
#endif
{
	Reg	int	i;
	Reg	aClient *cptr;
	Reg	Link	*channels, *lp;
	int	len = 0;

/*      This is kind of funky, but should work.  The first part below
	is optimized for HUB servers or servers with few clients on
	them.  The second part is optimized for bigger client servers
	where looping through the whole client list is bad.  I'm not
	really certain of the point at which each function equals
	out...but I do know the 2nd part will help big client servers
	fairly well... - Comstud 97/04/24
*/
     
	if (highest_fd < 50) /* This part optimized for HUB servers... */
	    {
		if (MyConnect(user))
		    {
#if ! USE_STDARG
			len = sendpreprep(user, user, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,p11);

#else
			va_list	va;
			va_start(va, pattern);
			len = vsendpreprep(user, user, pattern, va);
			va_end(va);
#endif
			(void)send_message(user, psendbuf, len);
		    }
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(cptr = local[i]) || IsServer(cptr) ||
			    user == cptr || !user->user)
				continue;
			for (lp = user->user->channel; lp; lp = lp->next)
			    {
				if (!IsMember(cptr, lp->value.chptr))
					continue;
				if (IsAnonymous(lp->value.chptr))
					continue;
				if (!IsQuiet(lp->value.chptr))
				    {
#ifndef DEBUGMODE
					if (!len) /* This saves little cpu,
						     but breaks the debug code.. */
#endif
					    {
#if ! USE_STDARG
					      len = sendpreprep(cptr, user, pattern,
								p1, p2, p3, p4, p5,
								p6, p7, p8, p9, p10,
								p11);
#else
					      va_list	va;
					      va_start(va, pattern);
					      len = vsendpreprep(cptr, user, pattern, va);
					      va_end(va);
#endif
					    }
					(void)send_message(cptr, psendbuf,
							   len);
					break;
				    }
			    }
		    }
	    }
	else
	    {
		/* This part optimized for client servers */
		bzero((char *)&sentalong[0], sizeof(int) * MAXCONNECTIONS);
		if (MyConnect(user))
		    {
#if ! USE_STDARG
			len = sendpreprep(user, user, pattern, p1, p2, p3, p4,
					  p5, p6, p7, p8, p9, p10, p11);
#else
			va_list	va;
			va_start(va, pattern);
			len = vsendpreprep(user, user, pattern, va);
			va_end(va);
#endif
			(void)send_message(user, psendbuf, len);
			sentalong[user->fd] = 1;
		    }
		if (!user->user)
			return;
		for (channels=user->user->channel; channels;
		     channels=channels->next)
		    {
			if (IsQuiet(channels->value.chptr))
				continue;
			if (IsAnonymous(channels->value.chptr))
				continue;
			for (lp=channels->value.chptr->members;lp;
			     lp=lp->next)
			    {
				cptr = lp->value.cptr;
				if (user == cptr)
					continue;
				if (!MyConnect(cptr) || sentalong[cptr->fd])
					continue;
				sentalong[cptr->fd]++;
#ifndef DEBUGMODE
				if (!len) /* This saves little cpu,
					     but breaks the debug code.. */
#endif
				    {
#if ! USE_STDARG
					len = sendpreprep(cptr, user, pattern,
							  p1, p2, p3, p4, p5,
							  p6, p7, p8, p9, p10,
							  p11);
#else
					va_list	va;
					va_start(va, pattern);
					len = vsendpreprep(cptr, user, pattern, va);
					va_end(va);
#endif
				    }
				(void)send_message(cptr, psendbuf, len);
			    }
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
#if ! USE_STDARG
/*VARARGS*/
void	sendto_channel_butserv(chptr, from, pattern, p1, p2, p3,
			       p4, p5, p6, p7, p8, p9, p10, p11)
aChannel *chptr;
aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_channel_butserv(aChannel *chptr, aClient *from, char *pattern, ...)
#endif
{
	Reg	Link	*lp;
	Reg	aClient	*acptr, *lfrm = from;
	int	len = 0;

	if (MyClient(from))
	    {	/* Always send to the client itself */
#if ! USE_STDARG
		sendto_prefix_one(from, from, pattern, p1, p2, p3, p4,
				  p5, p6, p7, p8, p9, p10, p11);
#else
		va_list	va;
		va_start(va, pattern);
		vsendto_prefix_one(from, from, pattern, va);
		va_end(va);
#endif
		if (IsQuiet(chptr))	/* Really shut up.. */
			return;
	    }
	if (IsAnonymous(chptr) && IsClient(from))
	    {
#if ! USE_STDARG
		if (p1 && *p1 && !mycmp(p1, from->name))
			p1 = anon.name;
#endif
		lfrm = &anon;
	    }

	for (lp = chptr->members; lp; lp = lp->next)
		if (MyClient(acptr = lp->value.cptr) && acptr != from)
		    {
			if (!len)
			    {
#if ! USE_STDARG
				len = sendpreprep(acptr, lfrm, pattern, p1, p2,
						  p3, p4, p5, p6, p7, p8, p9,
						  p10, p11);
#else
				va_list	va;
				va_start(va, pattern);
				len = vsendpreprep(acptr, lfrm, pattern, va);
				va_end(va);
#endif
			    }
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
		return (match(mask, one->user->host)==0);
	case MATCH_SERVER:
	default:
		return (match(mask, one->user->server)==0);
	}
}

/*
 * sendto_match_servs
 *
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
#if ! USE_STDARG
/*VARARGS*/
void	sendto_match_servs(chptr, from, format, p1, p2, p3, p4, p5, p6, p7,
			   p8, p9, p10, p11)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_match_servs(aChannel *chptr, aClient *from, char *format, ...)
#endif
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
		if (!BadPtr(mask) && match(mask, cptr->name))
			continue;
		if (chptr &&
		    *chptr->chname == '!' && !(cptr->serv->version & SV_NJOIN))
			continue;
		if (!len)
		    {
#if ! USE_STDARG
			len = sendprep(format, p1, p2, p3, p4, p5, p6, p7,
				       p8, p9, p10, p11);
#else
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
#endif
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
}

#if ! USE_STDARG
/*VARARGS*/
int
sendto_match_servs_v(chptr, from, ver, format, p1, p2, p3, p4, p5, p6,
		     p7, p8, p9, p10, p11)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
int	ver;
#else
int
sendto_match_servs_v(aChannel *chptr, aClient *from, int ver,
		     char *format, ...)
#endif
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return 0;
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
		if (!BadPtr(mask) && match(mask, cptr->name))
			continue;
		if (chptr &&
		    *chptr->chname == '!' && !(cptr->serv->version & SV_NJOIN))
			continue;
		if ((ver & cptr->serv->version) == 0)
		    {
			rc = 1;
			continue;
		    }
		if (!len)
		    {
#if ! USE_STDARG
			len = sendprep(format, p1, p2, p3, p4, p5, p6, p7,
				       p8, p9, p10, p11);
#else
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
#endif
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
	return rc;
}

#if ! USE_STDARG
/*VARARGS*/
int
sendto_match_servs_notv(chptr, from, ver, format, p1, p2, p3, p4, p5,
			p6, p7, p8, p9, p10, p11)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
int	ver;
#else
int
sendto_match_servs_notv(aChannel *chptr, aClient *from, int ver,
			char *format, ...)
#endif
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return 0;
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
		if (!BadPtr(mask) && match(mask, cptr->name))
			continue;
		if (chptr &&
		    *chptr->chname == '!' && !(cptr->serv->version & SV_NJOIN))
			continue;
		if ((ver & cptr->serv->version) != 0)
		    {
			rc = 1;
			continue;
		    }
		if (!len)
		    {
#if ! USE_STDARG
			len = sendprep(format, p1, p2, p3, p4, p5, p6, p7,
				       p8, p9, p10, p11);
#else
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
#endif
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
	return rc;
}

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
/*VARARGS*/
#if ! USE_STDARG
void	sendto_match_butone(one, from, mask, what, pattern,
			    p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *one, *from;
int	what;
char	*mask, *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9,*p10,*p11;
#else
void	sendto_match_butone(aClient *one, aClient *from, char *mask, int what, char *pattern, ...)

#endif
{
	int	i;
	aClient *cptr,
		*srch;
  
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr))
		    {
			/*
			** we can save some CPU here by not searching the
			** entire list of users since it is ordered!
			** original idea/code from pht.
			** it could be made better by looping on the list of
			** servers to avoid non matching blocks in the list
			** (srch->from != cptr), but then again I never
			** bothered to worry or optimize this routine -kalt
			*/
			for (srch = cptr->prev; srch; srch = srch->prev)
			{
				if (!IsRegisteredUser(srch))
					continue;
				if (srch->from == cptr &&
				    match_it(srch, mask, what))
					break;
			}
			if (srch == NULL)
				continue;
		    }
		/* my client, does he match ? */
		else if (!(IsRegisteredUser(cptr) && 
			   match_it(cptr, mask, what)))
			continue;
#if ! USE_STDARG
		sendto_prefix_one(cptr, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,p11);
#else
		{
			va_list	va;
			va_start(va, pattern);
			vsendto_prefix_one(cptr, from, pattern, va);
			va_end(va);
		}
#endif

	    }
	return;
}

/*
** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
#if ! USE_STDARG
/*VARARGS*/
void	sendto_ops_butone(one, from, pattern,
			  p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_ops_butone(aClient *one, aClient *from, char *pattern, ...)
#endif
{
	Reg	int	i;
	Reg	aClient *cptr;

	bzero((char *)&sentalong[0], sizeof(int) * MAXCONNECTIONS);
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (IsService(cptr) || !IsRegistered(cptr))
			continue;
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
#if ! USE_STDARG
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,p11);
#else
		{
			va_list	va;
			va_start(va, pattern);
			vsendto_prefix_one(cptr->from, from, pattern, va);
			va_end(va);
		}
#endif
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
#if ! USE_STDARG
/*VARARGS*/
void	sendto_prefix_one(to, from, pattern,
			  p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
Reg	aClient *to;
Reg	aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_prefix_one(aClient *to, aClient *from, char *pattern, ...)
#endif
{
	int	len;

#if ! USE_STDARG
	len = sendpreprep(to, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8,
			  p9, p10, p11);
#else
	va_list	va;
	va_start(va, pattern);
	len = vsendpreprep(to, from, pattern, va);
	va_end(va);
#endif
	send_message(to, psendbuf, len);
	return;
}

#if USE_STDARG
static void	vsendto_prefix_one(aClient *to, aClient *from, char *pattern, va_list va)
{
	int	len;

	len = vsendpreprep(to, from, pattern, va);
	send_message(to, psendbuf, len);
	return;
}
#endif


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
	{ SCH_SERVICE,	"&SERVICES",	NULL },
	{ SCH_DEBUG,	"&DEBUG",	NULL },
	{ SCH_AUTH,	"&AUTH",	NULL },
};


void	setup_svchans()
{
	int	i;
	SChan	*shptr;

	for (i = SCH_MAX, shptr = svchans + (i - 1); i > 0; i--, shptr--)
		shptr->svc_ptr = find_channel(shptr->svc_chname, NULL);
}

#if ! USE_STDARG
void	sendto_flag(chan, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,p11)
u_int	chan;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
#else
void	sendto_flag(u_int chan, char *pattern, ...)
#endif
{
	Reg	aChannel *chptr = NULL;
	SChan	*shptr;
	char	nbuf[1024];

	if (chan < 1 || chan > SCH_MAX)
		chan = SCH_NOTICE;
	shptr = svchans + (chan - 1);

	if ((chptr = shptr->svc_ptr))
	    {
#if ! USE_STDARG
		(void)sprintf(nbuf, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);
#else
		{
			va_list	va;
			va_start(va, pattern);
			(void)vsprintf(nbuf, pattern, va);
			va_end(va);
		}
#endif
		sendto_channel_butserv(chptr, &me, ":%s NOTICE %s :%s", ME, chptr->chname, nbuf);

#ifdef	USE_SERVICES
		switch (chan)
		    {
		case SCH_ERROR:
			check_services_butone(SERVICE_WANT_ERRORS, NULL, &me,
					      "&ERRORS :%s", nbuf);
			break;
		case SCH_NOTICE:
			check_services_butone(SERVICE_WANT_NOTICES, NULL, &me,
					      "&NOTICES :%s", nbuf);
			break;
		case SCH_LOCAL:
			check_services_butone(SERVICE_WANT_LOCAL, NULL, &me,
					      "&LOCAL :%s", nbuf);
			break;
		case SCH_NUM:
			check_services_butone(SERVICE_WANT_NUMERICS, NULL, &me,
					      "&NUMERICS :%s", nbuf);
			break;
		    }
#endif
	    }

	return;
}

/*
 * sendto_flog
 *	cptr		used for firsttime, auth, exitc, send/received M/K
 *	msg		replaces duration if duration is 0
 *	duration	only used if non 0
 *	username	can't get it from cptr
 *	hostname	i.e.
 */
void	sendto_flog(cptr, msg, duration, username, hostname)
aClient	*cptr;
char	*msg, *username, *hostname;
time_t	duration;
{
	char	linebuf[1024]; /* auth reply might be long.. */
	int	logfile;

#ifdef	USE_SERVICES
	if (duration)
	    {
		(void)sprintf(linebuf,
	      "%s (%3d:%02d:%02d): %s@%s [%s] %c %lu %luKb %lu %luKb\n",
			      myctime(cptr->firsttime),
			      (int) (duration / 3600),
			      (int) ((duration % 3600) / 60),
			      (int) (duration % 60),
			      username, hostname, cptr->auth,
			      cptr->exitc, cptr->sendM, cptr->sendK,
			      cptr->receiveM, cptr->receiveK);
		check_services_butone(SERVICE_WANT_USERLOG, NULL, &me,
				      "USERLOG :%s", linebuf);
	    }
	else
	    {
		(void)sprintf(linebuf,
			      "%s (%s): %s@%s [%s] %c %lu %luKb %lu %luKb\n",
			      myctime(cptr->firsttime), msg, username,
			      hostname, cptr->auth,
			      cptr->exitc, cptr->sendM, cptr->sendK,
			      cptr->receiveM, cptr->receiveK);
		check_services_butone(SERVICE_WANT_CONNLOG, NULL, &me,
				      "CONNLOG :%s", linebuf);
	    }
#endif
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
#ifndef	USE_SERVICES
		if (duration)
			(void)sprintf(linebuf,
	      "%s (%3d:%02d:%02d): %s@%s [%s] %c %lu %luKb %lu %luKb\n",
				      myctime(cptr->firsttime),
				      (int) (duration / 3600),
				      (int) ((duration % 3600) / 60),
				      (int) (duration % 60),
				      username, hostname, cptr->auth,
				      cptr->exitc, cptr->sendM, cptr->sendK,
				      cptr->receiveM, cptr->receiveK);
		else
			(void)sprintf(linebuf,
			      "%s (%s): %s@%s [%s] %c %lu %luKb %lu %luKb\n",
				      myctime(cptr->firsttime), msg, username,
				      hostname, cptr->auth,
                                      cptr->exitc, cptr->sendM, cptr->sendK,
                                      cptr->receiveM, cptr->receiveK);
#endif
		(void)alarm(3);
		(void)write(logfile, linebuf, strlen(linebuf));
		(void)alarm(0);
		(void)close(logfile);
	    }
	(void)alarm(0);
}
#endif /* CLIENT_COMPILE */
