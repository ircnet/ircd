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
static const volatile char rcsid[] = "@(#)$Id: send.c,v 1.108 2008/06/08 17:17:10 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define SEND_C
#include "s_externs.h"
#undef SEND_C

static	char	sendbuf[2048];


static	void	vsendto_prefix_one(aClient *, aClient *, char *, va_list);
static	char	psendbuf[2048];
static	int	sentalong[MAXCONNECTIONS];

/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCK. This should
**	generate ExitClient from the main loop.
**
**	The notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static	int	dead_link(aClient *to, char *pattern, ...)
{
	char	notice[BUFSIZE];
	va_list	va;

	va_start(va, pattern);
	vsprintf(notice, pattern, va);
	va_end(va);

	SetDead(to);
	/*
	 * If because of BUFFERPOOL problem then clean dbufs now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		sendto_flag(SCH_ERROR, notice);
	Debug((DEBUG_ERROR, notice));
	return -1;
}

/*
** flush_fdary
**      Used to empty all output buffers for connections in fdary.
*/
void    flush_fdary(FdAry *fdp)
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
void	flush_connections(int fd)
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

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
**	if ZIP_LINKS is defined, the message will eventually be compressed,
**	anything stored in the sendQ is compressed.
**
**	If msg is a null pointer, we are flushing connection
*/
int	send_message(aClient *to, char *msg, int len)
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
	if (DBufLength(&to->sendQ) > (i=get_sendq(to, CBurst(to))))
	{
		to->exitc = EXITC_SENDQ;
		if (IsService(to) || IsServer(to))
		{
			return dead_link(to,
				"Max SendQ limit exceeded for %s: %d > %d",
				get_client_name(to, FALSE),
				DBufLength(&to->sendQ), i);
		}
		return dead_link(to, "Max Sendq exceeded");
	}
# ifdef	ZIP_LINKS
	/*
	** data is first stored in to->zip->outbuf until
	** it's big enough to be compressed and stored in the sendq.
	** send_queued is then responsible to never let the sendQ
	** be empty and to->zip->outbuf not empty.
	*/
	if (to->flags & FLAGS_ZIP)
		msg = zip_buffer(to, msg, &len, 0);

# endif	/* ZIP_LINKS */
tryagain:
	if (len && (i = dbuf_put(&to->sendQ, msg, len)) < 0)
	{
		if (i == -2	/* Poolsize was exceeded. */
#ifdef POOLSIZE_LIMITED
			/*
			** Defining this retains old ircd behaviour (will
			** allow client quit with buffer allocation error
			** as a result of poolsize starvation). As it may
			** happen to all clients on a big channel without
			** their fault, I think this is not right.
			** In the long run it should not matter (poolsize
			** or memory usage-wise), because if client lacks
			** the poolsize, the poolsize is too small anyway
			** and next netburst would probably make it grow.
			** IMO increasing poolsize with no limits is good
			** for clients -- hence this is not defined. --B.
			*/
			&& CBurst(to)
#endif
			)
		{
			/* Anyway, 10% increase. */
			poolsize *= 1.1;
			sendto_flag(SCH_NOTICE,
				    "New poolsize %u. (reached)",
				    poolsize);
			istat.is_dbufmore++;
			goto tryagain;
		}
		else
		{
			to->exitc = EXITC_MBUF;
			return dead_link(to,
				"Buffer allocation error for %s",
				get_client_name(to, FALSE));
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

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int	send_queued(aClient *to)
{
	char	*msg;
	int	len, rlen, more = 0;
	aClient *bysptr = NULL;

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
					 "Buffer allocation error for %s",
					get_client_name(to, FALSE));
			    }
		    }
	    }
#endif
	while (DBufLength(&to->sendQ) > 0 || more)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
		{
			if ( (IsConnecting(to) || IsHandshake(to))
			     && to->serv && to->serv->byuid[0])
			{
				bysptr = find_uid(to->serv->byuid, NULL);
				if (bysptr && !MyConnect(bysptr))
				{
					sendto_one(bysptr, ":%s NOTICE %s :"
					"Write error (%s) to %s, closing link",
					ME, bysptr->name, strerror(-rlen),
					to->name);
				}
			}
			return dead_link(to,
				"Write error (%s) to %s, closing link",
				strerror(-rlen), get_client_name(to, FALSE));
		}
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
					 "Buffer allocation error for %s",
					get_client_name(to, FALSE));
			    }
		    }
#endif
	    }

	return (IsDead(to)) ? -1 : 0;
}


static	anUser	ausr;
static	aClient	anon;

void initanonymous(void)
{
	memset(&ausr, 0, sizeof(anUser));
	strcpy(ausr.username, "anonymous");
	strcpy(ausr.uid, "0ANONYM");
	strcpy(ausr.host, "anonymous.");
	ausr.server = "anonymous.";

	memset(&anon, 0, sizeof(aClient));
	anon.user = &ausr;
	anon.from = &anon;
	anon.fd = -2;
	anon.status = STAT_CLIENT;
	anon.name = anon.namebuf;
	strcpy(anon.namebuf, "anonymous");
	strcpy(anon.username, "anonymous");
	anon.info = "anonymous identity hider";
	anon.exitc = EXITC_UNDEF;
}

/*
 * sendprep: takes care of building the string according to format & args
 */
static	int	vsendprep(char *pattern, va_list va)
{
	int	len;

	Debug((DEBUG_L10, "sendprep(%s)", pattern));
	len = vsprintf(sendbuf, pattern, va);
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

/*
 * sendpreprep: takes care of building the string according to format & args,
 *		and of adding a complete prefix if necessary
 */
static	int	vsendpreprep(aClient *to, aClient *from, char *pattern, va_list va)
{
	int	len;

	Debug((DEBUG_L10, "sendpreprep(%#x(%s),%#x(%s),%s)",
		to, to->name, from, from->name, pattern));
	if (to && from && MyClient(to) && IsPerson(from) &&
	    !strncmp(pattern, ":%s", 3))
	{
		char	*par = va_arg(va, char *);

		if (from == &anon || !mycmp(par, from->name))
		{
			len = sprintf(psendbuf, ":%s!%s@%s", from->name,
				from->user->username, from->user->host);
		}
		else
		{
			len = sprintf(psendbuf, ":%s", par);
		}

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

/*
** send message to single client
*/
int	vsendto_one(aClient *to, char *pattern, va_list va)
{
	int	len;

	len = vsendprep(pattern, va);

	(void)send_message(to, sendbuf, len);
	return len;
}

int	sendto_one(aClient *to, char *pattern, ...)
{
	int	len;
	va_list	va;
	va_start(va, pattern);
	len = vsendto_one(to, pattern, va);
	va_end(va);
	return len;
}

/*
 * sendto_channel_butone
 *
 * Send a message to all members of a channel that are connected to this
 * server except client 'one'.
 */
void	sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
		char *pattern, ...)
{
	Reg	Link	*lp;
	Reg	aClient *acptr, *lfrm = from;
	int	len1, len2 = 0;

	if (IsAnonymous(chptr) && IsClient(from))
	    {
		lfrm = &anon;
	    }

	if (one != from && MyConnect(from) && IsRegisteredUser(from))
	    {
		/* useless junk? */ /* who said that and why? --B. */
		va_list	va;
		va_start(va, pattern);
		vsendto_prefix_one(from, from, pattern, va);
		va_end(va);
	    }

	{
		va_list	va;
		va_start(va, pattern);
		len1 = vsendprep(pattern, va);
		va_end(va);
	}


	for (lp = chptr->clist; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || IsMe(acptr))
			continue;	/* ...was the one I should skip */
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
			if (!len2)
			    {
				va_list	va;
				va_start(va, pattern);
				len2 = vsendpreprep(acptr, lfrm, pattern, va);
				va_end(va);
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
void	sendto_serv_butone(aClient *one, char *pattern, ...)
{
	Reg	int	i, len=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr)) {
			if (!len)
			    {
				va_list	va;
				va_start(va, pattern);
				len = vsendprep(pattern, va);
				va_end(va);
			    }
			(void)send_message(cptr, sendbuf, len);
	}
	return;
}

int	sendto_serv_v(aClient *one, int ver, char *pattern, ...)
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
	{
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr))
		{
#if 0
/* We're not using it for now, so just save some cpu.
** Revive once we need it --B. */
			if ((cptr->serv->version & ver) == 0)
			{
				rc = 1;
				continue;
			}
#endif
			if (!len)
			{
				va_list	va;
				va_start(va, pattern);
				len = vsendprep(pattern, va);
				va_end(va);
			}
			(void)send_message(cptr, sendbuf, len);
		}
	}

	return rc;
}

#if 0
/* We're not using it for now, so just save some cpu.
** Revive once we need it --B. */
int	sendto_serv_notv(aClient *one, int ver, char *pattern, ...)
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient *cptr;

	for (i = fdas.highest; i >= 0; i--)
	{
		if ((cptr = local[fdas.fd[i]]) &&
		    (!one || cptr != one->from) && !IsMe(cptr))
		{
			if ((cptr->serv->version & ver) == 0)
			{
				if (!len)
				{
					va_list	va;
					va_start(va, pattern);
					len = vsendprep(pattern, va);
					va_end(va);
				}

				(void)send_message(cptr, sendbuf, len);
			}
			else
			{
				rc = 1;
			}
		}
	}

	return rc;
}
#endif

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user, except for channels set Quiet or Anonymous
 * The calling procedure must take the necessary steps for such channels.
 */
void	sendto_common_channels(aClient *user, char *pattern, ...)
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
			va_list	va;
			va_start(va, pattern);
			len = vsendpreprep(user, user, pattern, va);
			va_end(va);
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
					      va_list	va;
					      va_start(va, pattern);
					      len = vsendpreprep(cptr, user, pattern, va);
					      va_end(va);
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
			va_list	va;
			va_start(va, pattern);
			len = vsendpreprep(user, user, pattern, va);
			va_end(va);
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
			for (lp=channels->value.chptr->clist;lp;
			     lp=lp->next)
			    {
				cptr = lp->value.cptr;
				if (user == cptr)
					continue;
				if (!cptr->user || sentalong[cptr->fd])
					continue;
				sentalong[cptr->fd]++;
#ifndef DEBUGMODE
				if (!len) /* This saves little cpu,
					     but breaks the debug code.. */
#endif
				    {
					va_list	va;
					va_start(va, pattern);
					len = vsendpreprep(cptr, user, pattern, va);
					va_end(va);
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
void	sendto_channel_butserv(aChannel *chptr, aClient *from, char *pattern, ...)
{
	Reg	Link	*lp;
	Reg	aClient	*acptr, *lfrm = from;
	int	len = 0;

	if (MyClient(from))
	    {	/* Always send to the client itself */
		va_list	va;
		va_start(va, pattern);
		vsendto_prefix_one(from, from, pattern, va);
		va_end(va);
		if (IsQuiet(chptr))	/* Really shut up.. */
			return;
	    }
	if (IsAnonymous(chptr) && IsClient(from))
	    {
		lfrm = &anon;
	    }

	for (lp = chptr->clist; lp; lp = lp->next)
		if (MyClient(acptr = lp->value.cptr) && acptr != from)
		    {
			if (!len)
			    {
				va_list	va;
				va_start(va, pattern);
				len = vsendpreprep(acptr, lfrm, pattern, va);
				va_end(va);
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

static	int	match_it(aClient *one, char *mask, int what)
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
void	sendto_match_servs(aChannel *chptr, aClient *from, char *format, ...)
{
	Reg	int	i, len=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return;
		if ((mask = get_channelmask(chptr->chname)))
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
#ifdef JAPANESE
		if (!jp_valid(cptr, chptr, 0))
			continue;
#endif
		if (!len)
		    {
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
}

int	sendto_match_servs_v(aChannel *chptr, aClient *from, int ver,
		char *format, ...)
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return 0;
		if ((mask = get_channelmask(chptr->chname)))
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
#ifdef JAPANESE
		if (!jp_valid(cptr, chptr, 0))
			continue;
#endif

#if 0
/* We're not using it for now, so just save some cpu.
** Revive once we need it --B. */
		if ((ver & cptr->serv->version) == 0)
		    {
			rc = 1;
			continue;
		    }
#endif
		if (!len)
		    {
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
	return rc;
}

#if 0
/* We're not using it for now, so just save some cpu.
** Revive once we need it --B. */
int	sendto_match_servs_notv(aChannel *chptr, aClient *from, int ver,
		char *format, ...)
{
	Reg	int	i, len=0, rc=0;
	Reg	aClient	*cptr;
	char	*mask;

	if (chptr)
	    {
		if (*chptr->chname == '&')
			return 0;
		if ((mask = get_channelmask(chptr->chname)))
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
#ifdef JAPANESE
		if (!jp_valid(cptr, chptr, 0))
			continue;
#endif
		if ((ver & cptr->serv->version) != 0)
		    {
			rc = 1;
			continue;
		    }
		if (!len)
		    {
			va_list	va;
			va_start(va, format);
			len = vsendprep(format, va);
			va_end(va);
		    }
		(void)send_message(cptr, sendbuf, len);
	    }
	return rc;
}
#endif

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
void	sendto_match_butone(aClient *one, aClient *from, char *mask, int what,
		char *pattern, ...)
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
		{
			continue;
		}
		/* this frame have tricked me many times ;) and it's only
		** frame for having va declared ;) --Beeth */
		{
			va_list	va;
			va_start(va, pattern);
			vsendto_prefix_one(cptr, from, pattern, va);
			va_end(va);
		}

	    }
	return;
}

/*
** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
void	sendto_ops_butone(aClient *one, char *from, char *pattern, ...)
{
	va_list	va;
	char	buf[BUFSIZE];

	va_start(va, pattern);
	vsprintf(buf, pattern, va);
	va_end(va);
	sendto_serv_butone(one, ":%s WALLOPS :%s", from, buf);
	sendto_flag(SCH_WALLOP, "!%s! %s", from, buf);
	
	return;
}

/*
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */
void	sendto_prefix_one(aClient *to, aClient *from, char *pattern, ...)
{
	int	len;

	va_list	va;
	va_start(va, pattern);
	len = vsendpreprep(to, from, pattern, va);
	va_end(va);
	send_message(to, psendbuf, len);
	return;
}

static	void	vsendto_prefix_one(aClient *to, aClient *from, char *pattern,
		va_list va)
{
	int	len;

	len = vsendpreprep(to, from, pattern, va);
	send_message(to, psendbuf, len);
	return;
}


/*
 * sends a message to a server-owned channel
 */
static	SChan	svchans[SCH_MAX] = {
	{ SCH_ERROR,	"&ERRORS",	NULL, -2},
	{ SCH_NOTICE,	"&NOTICES",	NULL, -2},
	{ SCH_KILL,	"&KILLS",	NULL, -2},
	{ SCH_CHAN,	"&CHANNEL",	NULL, -2},
	{ SCH_NUM,	"&NUMERICS",	NULL, -2},
	{ SCH_SERVER,	"&SERVERS",	NULL, -2},
	{ SCH_HASH,	"&HASH",	NULL, -2},
	{ SCH_LOCAL,	"&LOCAL",	NULL, -2},
	{ SCH_SERVICE,	"&SERVICES",	NULL, -2},
	{ SCH_DEBUG,	"&DEBUG",	NULL, -2},
	{ SCH_AUTH,	"&AUTH",	NULL, -2},
	{ SCH_SAVE,	"&SAVE",	NULL, -2},
	{ SCH_WALLOP,	"&WALLOPS",	NULL, -2},
#ifdef CLIENTS_CHANNEL
	{ SCH_CLIENT,	"&CLIENTS",	NULL, -2},
#endif
	{ SCH_OPER,	"&OPER",	NULL, -2},
};


void	setup_svchans(void)
{
	int	i;
	SChan	*shptr;

#ifdef LOG_SERVER_CHANNELS
	/* They're here, as they need to be done only once per server run,
	** to determine if we want given channel logged. --B. */
# ifdef LOG_SCH_ERROR
	(svchans+SCH_ERROR)->fd = -1;
# endif
# ifdef LOG_SCH_NOTICE
 	(svchans+SCH_NOTICE)->fd = -1;
# endif
# ifdef LOG_SCH_KILL
	(svchans+SCH_KILL)->fd = -1;
# endif
# ifdef LOG_SCH_CHAN
	(svchans+SCH_CHAN)->fd = -1;
# endif
# ifdef LOG_SCH_NUM
	(svchans+SCH_NUM)->fd = -1;
# endif
# ifdef LOG_SCH_SERVER
	(svchans+SCH_SERVER)->fd = -1;
# endif
# ifdef LOG_SCH_HASH
	(svchans+SCH_HASH)->fd = -1;
# endif
# ifdef LOG_SCH_LOCAL
	(svchans+SCH_LOCAL)->fd = -1;
# endif
# ifdef LOG_SCH_SERVICE
	(svchans+SCH_SERVICE)->fd = -1;
# endif
# ifdef LOG_SCH_DEBUG
	(svchans+SCH_DEBUG)->fd = -1;
# endif
# ifdef LOG_SCH_AUTH
	(svchans+SCH_AUTH)->fd = -1;
# endif
# ifdef LOG_SCH_SAVE
	(svchans+SCH_SAVE)->fd = -1;
# endif
# ifdef LOG_SCH_WALLOP
	(svchans+SCH_WALLOP)->fd = -1;
# endif
# ifdef CLIENTS_CHANNEL
#  ifdef LOG_SCH_CLIENT
	(svchans+SCH_CLIENT)->fd = -1;
#  endif
# endif
# ifdef LOG_SCH_OPER
	(svchans+SCH_OPER)->fd = -1;
# endif
#endif
	for (i = SCH_MAX - 1, shptr = svchans + i; i >= 0; i--, shptr--)
		shptr->svc_ptr = find_channel(shptr->svc_chname, NULL);
}

void	sendto_flag(u_int chan, char *pattern, ...)
{
	Reg	aChannel *chptr = NULL;
	SChan	*shptr;
	char	nbuf[1024];

	if (chan >= SCH_MAX)
		chan = SCH_NOTICE;
	shptr = svchans + chan;

	if ((chptr = shptr->svc_ptr))
	    {
		{
			va_list	va;
			va_start(va, pattern);
			(void)vsprintf(nbuf, pattern, va);
			va_end(va);
		}
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
#ifdef CLIENTS_CHANNEL
		case SCH_CLIENT:
			check_services_butone(SERVICE_WANT_CLIENTS, NULL, &me,
					      "&CLIENTS :%s", nbuf);
			break;
#endif
		    }
#endif
	    }
#ifdef LOG_SERVER_CHANNELS
	if ((svchans+chan)->fd >= 0)
	{
		char	lbuf[1024];
		int	len;

		len = sprintf(lbuf, "%u %s\n", (u_int)timeofday, nbuf);
		(void)write((svchans+chan)->fd, lbuf, len);
	}
#endif
	return;
}

static int userlog = -1;
static int connlog = -1;

void	logfiles_open(void)
{
#ifdef LOG_SERVER_CHANNELS
	int	i;
	SChan	*shptr;
	char	fname[BUFSIZE];

	for (i = SCH_MAX - 1, shptr = svchans + i; i >= 0; i--, shptr--)
	{
		if (shptr->fd == -2)
		{
			/* We don't want this channel logged. */
			continue;
		}

		sprintf(fname, "%s.%s", FNAME_SCH_PREFIX, shptr->svc_chname+1);
		shptr->fd = open(fname, O_WRONLY|O_APPEND|O_NDELAY
#ifdef LOGFILES_ALWAYS_CREATE
			|O_CREAT, S_IRUSR|S_IWUSR
#endif
			);
		/* Better safe than sorry. */
		if (shptr->fd >= 0)
		{
			local[shptr->fd] = NULL;
		}
	}
#endif
#ifdef  FNAME_USERLOG
	userlog = open(FNAME_USERLOG, O_WRONLY|O_APPEND|O_NDELAY
# ifdef	LOGFILES_ALWAYS_CREATE
			|O_CREAT, S_IRUSR|S_IWUSR
# endif
			);
	/* Better safe than sorry. */
	if (userlog >= 0)
	{
		local[userlog] = NULL;
	}
#else
	userlog = -1;
#endif
#ifdef  FNAME_CONNLOG
	connlog = open(FNAME_CONNLOG, O_WRONLY|O_APPEND|O_NDELAY
# ifdef	LOGFILES_ALWAYS_CREATE
		|O_CREAT, S_IRUSR|S_IWUSR
# endif
			);
	if (connlog >= 0)
	{
		local[connlog] = NULL;
	}
#else
	connlog = -1;
#endif
}

void	logfiles_close(void)
{
#ifdef LOG_SERVER_CHANNELS
	int	i;
	SChan	*shptr;
	
	for (i = SCH_MAX - 1, shptr = svchans + i; i >= 0; i--, shptr--)
	{
		if (shptr->fd >= 0)
		{
			(void)close(shptr->fd);
		}
	}
#endif
#ifdef FNAME_USERLOG
	if (userlog != -1)
	{
		(void)close(userlog);
		userlog = -1;
	}
#endif
#ifdef FNAME_CONNLOG
	if (connlog != -1)
	{
		(void)close(connlog);
		connlog = -1;
	}
#endif
}

/*
 * sendto_flog
 *	cptr		used for firsttime, auth, exitc, send/receive M/B
 *	msg		exit code
 *	username	sometimes can't get it from cptr
 *	hostname	i.e.
 */
void	sendto_flog(aClient *cptr, char msg, char *username, char *hostname)
{
	/* 
	** One day we will rewrite linebuf to malloc()s, but for now
	** we are lazy. The longest linebuf I saw during last year
	** was 216. Max auth reply can be 1024, see rfc931_work() and
	** if iauth is disabled, read_authports() makes it max 513.
	** And the rest... just count, I got 154 --Beeth
	*/
	char	linebuf[1500];
	int	linebuflen;
	/*
	** This is a potential buffer overflow.
	** I mean, when you manage to keep ircd
	** running for almost 12 years ;-) --B.
	*/
#ifdef LOG_OLDFORMAT
	char	buf[12];
#endif
	int	logfile;

	/*
	** EXITC_REG == 0 means registered client quitting, so it goes to
	** userlog; otherwise it's rejection and goes to connlog --Beeth.
	*/
	logfile = (msg == EXITC_REG ? userlog : connlog);

#if !defined(USE_SERVICES) && !( defined(USE_SYSLOG) && \
	(defined(SYSLOG_USERS) || defined(SYSLOG_CONN)) )
	if (logfile == -1)
	{
		return;
	}
#endif
#ifdef LOG_OLDFORMAT
	if (msg == EXITC_REG)
	{
		time_t	duration;

		duration = timeofday - cptr->firsttime + 1;
		(void)sprintf(buf, "%3d:%02d:%02d",
			(int) (duration / 3600),
			(int) ((duration % 3600) / 60),
			(int) (duration % 60));
	}
	else
	{
		char *anyptr;

		switch(msg)
		{
			case EXITC_GHMAX:	anyptr="G IP  max"; break;
			case EXITC_GUHMAX:	anyptr="G u@h max"; break;
			case EXITC_LHMAX:	anyptr="L IP  max"; break;
			case EXITC_LUHMAX:	anyptr="L u@h max"; break;
			case EXITC_AREF:
			case EXITC_AREFQ:	anyptr=" Denied  "; break;
			case EXITC_KLINE:	anyptr=" K lined "; break;
			case EXITC_CLONE:	anyptr=" ?Clone? "; break;
			case EXITC_YLINEMAX:	anyptr="   max   "; break;
			case EXITC_NOILINE:	anyptr=" No Auth "; break;
			case EXITC_AUTHFAIL:	anyptr="No iauth!"; break;
			case EXITC_AUTHTOUT:	anyptr="iauth t/o"; break;
			case EXITC_FAILURE:	anyptr=" Failure "; break;
			default:		anyptr=" Unknown ";
		}
		(void)sprintf(buf, "%s", anyptr);
	}
	linebuflen = sprintf(linebuf,
		"%s (%s): %s@%s [%s] %c %lu %luKb %lu %luKb ",
		myctime(cptr->firsttime), buf,
		username[0] ? username : "<none>", hostname,
		cptr->auth ? cptr->auth : "<none>",
		cptr->exitc, cptr->sendM, (long)(cptr->sendB>>10),
		cptr->receiveM, (long)(cptr->receiveB>>10));
#else
	/*
	** This is the content of loglines.
	*/
	linebuflen = sprintf(linebuf,
		"%c %d %d %s %s %s %s %d %s %lu %llu %lu %llu ",
		/* exit code as defined in common/struct_def.h; some common:
		 * '0' normal exit, '-' unregistered client quit, 'k' k-lined,
		 * 'K' killed, 'X' x-lined, 'Y' max clients limit of Y-line,
		 * 'L' local @host limit, 'l' local user@host limit, 'P' ping
		 * timeout, 'Q' send queue exceeded, 'E' socket error */
		cptr->exitc,
		/* signon unix time */
		(u_int) cptr->firsttime,
		/* signoff unix time */
		(u_int) timeofday,
		/* username (if ident is not working, it's from USER cmd) */
		username,
		/* hmm, let me take an educated guess... a hostname? */
		hostname,
		/* ident, if available */
		cptr->auth ? cptr->auth : "?",
		/* client IP */
		cptr->user ? cptr->user->sip :
#ifdef INET6
		inetntop(AF_INET6, (char *)&cptr->ip, ipv6string, sizeof(ipv6string)),
#else
		inetntoa((char *)&cptr->ip),
#endif
		/* client (remote) port */
		cptr->port,
		/* server sockhost (IP plus port or unix socket path) */
		cptr->acpt ? cptr->acpt->sockhost : "?",
		/* messages and bytes sent to client */
		cptr->sendM, cptr->sendB,
		/* messages and bytes received from client */
		cptr->receiveM, cptr->receiveB);
#endif /* LOG_OLDFORMAT */
#if defined(USE_SYSLOG) && (defined(SYSLOG_USERS) || defined(SYSLOG_CONN))
	if (msg == EXITC_REG)
	{
#  ifdef SYSLOG_USERS
		syslog(LOG_NOTICE, "%s", linebuf);
#  endif
	}
	else
	{
#  ifdef SYSLOG_CONN
		syslog(LOG_NOTICE, "%s", linebuf);
#  endif
	}
#endif	/* USE_SYSLOG */

#ifdef	USE_SERVICES
	if (msg == EXITC_REG)
	{
		check_services_butone(SERVICE_WANT_USERLOG, NULL, &me,
				      "USERLOG :%s", linebuf);
	}
	else
	{
		check_services_butone(SERVICE_WANT_CONNLOG, NULL, &me,
				      "CONNLOG :%s", linebuf);
	}
#endif
	if (logfile != -1)
	{
		linebuf[linebuflen-1] = '\n';
		(void)write(logfile, linebuf, linebuflen);
	}
}
