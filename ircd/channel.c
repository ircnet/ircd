/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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

/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */

#ifndef	lint
static	char rcsid[] = "@(#)$Id: channel.c,v 1.120 2002/03/28 22:52:37 jv Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define CHANNEL_C
#include "s_externs.h"
#undef CHANNEL_C

aChannel *channel = NullChn;

static	void	add_invite __P((aClient *, aChannel *));
static	int	can_join __P((aClient *, aChannel *, char *));
void	channel_modes __P((aClient *, char *, char *, aChannel *));
static	int	check_channelmask __P((aClient *, aClient *, char *));
static	aChannel *get_channel __P((aClient *, char *, int));
static	int	set_mode __P((aClient *, aClient *, aChannel *, int *, 
			int, char **));
static	void	free_channel __P((aChannel *));

static	int	add_modeid __P((int, aClient *, aChannel *, char *));
static	int	del_modeid __P((int, aChannel *, char *));
static	Link	*match_modeid __P((int, aClient *, aChannel *));
static  void    names_channel __P((aClient *,aClient *,char *,aChannel *,int));

static	char	*PartFmt = ":%s PART %s :%s";

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], uparabuf[MODEBUFLEN];

/*
 * return the length (>=0) of a chain of links.
 */
static	int	list_length(lp)
Reg	Link	*lp;
{
	Reg	int	count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
static	aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char	*user;
Reg	int	*chasing;
{
	Reg	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	    {
		sendto_one(sptr, replies[ERR_NOSUCHNICK], ME, BadTo(sptr->name), user);
		return NULL;
	    }
	if (chasing)
		*chasing = 1;
	return who;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\-`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
static	char	*check_string(s)
Reg	char *s;
{
	static	char	star[2] = "*";
	char	*str = s;

	if (BadPtr(s))
		return star;

	for ( ;*s; s++)
		if (isspace(*s))
		    {
			*s = '\0';
			break;
		    }

	return (BadPtr(str)) ? star : str;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
static	char *make_nick_user_host(nick, name, host)
Reg	char	*nick, *name, *host;
{
	static	char	namebuf[NICKLEN+USERLEN+HOSTLEN+6];
	Reg	char	*s = namebuf;

	bzero(namebuf, sizeof(namebuf));
	nick = check_string(nick);
	strncpyzt(namebuf, nick, NICKLEN + 1);
	s += strlen(s);
	*s++ = '!';
	name = check_string(name);
	strncpyzt(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strncpyzt(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
}

/*
 * Ban functions to work with mode +b/+e/+I
 */
/* add_modeid - add an id to the list of modes "type" for chptr
 *  (belongs to cptr)
 */

static	int	add_modeid(type, cptr, chptr, modeid)
int type;
aClient	*cptr;
aChannel *chptr;
char	*modeid;
{
	Reg	Link	*mode;
	Reg	int	cnt = 0, len = 0;

	if (MyClient(cptr))
		(void) collapse(modeid);
	for (mode = chptr->mlist; mode; mode = mode->next)
	    {
		len += strlen(mode->value.cp);
		if (MyClient(cptr))
		    {
			if ((len > MAXBANLENGTH) || (++cnt >= MAXBANS))
			    {
				sendto_one(cptr, replies[ERR_BANLISTFULL],
							 ME, BadTo(cptr->name),
					   chptr->chname, modeid);
				return -1;
			    }
			if (type == mode->flags &&
			    (!match(mode->value.cp, modeid) ||
			    !match(modeid, mode->value.cp)))
			    {
				int rpl;

				if (type == CHFL_BAN)
					rpl = RPL_BANLIST;
				else if (type == CHFL_EXCEPTION)
					rpl = RPL_EXCEPTLIST;
				else
					rpl = RPL_INVITELIST;

				sendto_one(cptr, replies[rpl], ME, BadTo(cptr->name),
					   chptr->chname, mode->value.cp);
				return -1;
			    }
		    }
		else if (type == mode->flags && !mycmp(mode->value.cp, modeid))
			return -1;
		
	    }
	mode = make_link();
	istat.is_bans++;
	bzero((char *)mode, sizeof(Link));
	mode->flags = type;
	mode->next = chptr->mlist;
	mode->value.cp = (char *)MyMalloc(len = strlen(modeid)+1);
	istat.is_banmem += len;
	(void)strcpy(mode->value.cp, modeid);
	chptr->mlist = mode;
	return 0;
}

/*
 * del_modeid - delete an id belonging to chptr
 * if modeid is null, delete all ids belonging to chptr.
 */
static	int	del_modeid(type, chptr, modeid)
int type;
aChannel *chptr;
char	*modeid;
{
	Reg	Link	**mode;
	Reg	Link	*tmp;

	if (modeid == NULL)
	    {
	        for (mode = &(chptr->mlist); *mode; mode = &((*mode)->next))
		    if (type == (*mode)->flags)
		        {
			    tmp = *mode;
			    *mode = tmp->next;
			    istat.is_banmem -= (strlen(tmp->value.cp) + 1);
			    istat.is_bans--;
			    MyFree(tmp->value.cp);
			    free_link(tmp);
			    break;
			}
	    }
	else for (mode = &(chptr->mlist); *mode; mode = &((*mode)->next))
		if (type == (*mode)->flags &&
		    mycmp(modeid, (*mode)->value.cp)==0)
		    {
			tmp = *mode;
			*mode = tmp->next;
			istat.is_banmem -= (strlen(modeid) + 1);
			istat.is_bans--;
			MyFree(tmp->value.cp);
			free_link(tmp);
			break;
		    }
	return 0;
}

/*
 * match_modeid - returns a pointer to the mode structure if matching else NULL
 */
static	Link	*match_modeid(type, cptr, chptr)
int type;
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*tmp;
	char	*s;

	if (!IsPerson(cptr))
		return NULL;

	s = make_nick_user_host(cptr->name, cptr->user->username,
				  cptr->user->host);

	for (tmp = chptr->mlist; tmp; tmp = tmp->next)
		if (tmp->flags == type && match(tmp->value.cp, s) == 0)
			break;

	if (!tmp && MyConnect(cptr))
	    {
		char *ip = NULL;

#ifdef 	INET6
		ip = (char *) inetntop(AF_INET6, (char *)&cptr->ip,
				       mydummy, MYDUMMY_SIZE);
#else
		ip = (char *) inetntoa((char *)&cptr->ip);
#endif

		if (strcmp(ip, cptr->user->host))
		    {
			s = make_nick_user_host(cptr->name,
						cptr->user->username, ip);
	    
			for (tmp = chptr->mlist; tmp; tmp = tmp->next)
				if (tmp->flags == type &&
				    match(tmp->value.cp, s) == 0)
					break;
		    }
	  }

	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static	void	add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
int	flags;
{
	Reg	Link *ptr;
	Reg	int sz = sizeof(aChannel) + strlen(chptr->chname);

	if (who->user)
	    {
		ptr = make_link();
		ptr->flags = flags;
		ptr->value.cptr = who;
		ptr->next = chptr->members;
		chptr->members = ptr;
		istat.is_chanusers++;
		if (chptr->users++ == 0)
		    {
			istat.is_chan++;
			istat.is_chanmem += sz;
		    }
		if (chptr->users == 1 && chptr->history)
		    {
			/* Locked channel */
			istat.is_hchan--;
			istat.is_hchanmem -= sz;
			/*
			** The modes had been kept, but now someone is joining,
			** they should be reset to avoid desynchs
			** (you wouldn't want to join a +i channel, either)
			**
			** This can be wrong in some cases such as a netjoin
			** which will not complete, or on a mixed net (with
			** servers that don't do channel delay) - kalt
			*/
			if (*chptr->chname != '!')
				bzero((char *)&chptr->mode, sizeof(Mode));
		    }

#ifdef USE_SERVICES
		if (chptr->users == 1)
			check_services_butone(SERVICE_WANT_CHANNEL|
					      SERVICE_WANT_VCHANNEL,
					      NULL, &me, "CHANNEL %s %d",
					      chptr->chname, chptr->users);
		else
			check_services_butone(SERVICE_WANT_VCHANNEL,
					      NULL, &me, "CHANNEL %s %d",
					      chptr->chname, chptr->users);
#endif
		ptr = make_link();
		ptr->flags = flags;
		ptr->value.chptr = chptr;
		ptr->next = who->user->channel;
		who->user->channel = ptr;
		if (!IsQuiet(chptr))
		    {
			who->user->joined++;
			istat.is_userc++;
		    }

		if (!(ptr = find_user_link(chptr->clist, who->from)))
		    {
			ptr = make_link();
			ptr->value.cptr = who->from;
			ptr->next = chptr->clist;
			chptr->clist = ptr;
		    }
		ptr->flags++;
	    }
}

void	remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
	Reg	Link	**curr;
	Reg	Link	*tmp, *tmp2;

	for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
		if (tmp->value.cptr == sptr)
		    {
			/*
			 * if a chanop leaves (no matter how), record
			 * the time to be able to later massreop if
			 * necessary.
			 */
			if (*chptr->chname == '!' &&
			    (tmp->flags & CHFL_CHANOP))
				chptr->reop = timeofday + LDELAYCHASETIMELIMIT;

			*curr = tmp->next;
			free_link(tmp);
			break;
		    }
	for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*curr = tmp->next;
			free_link(tmp);
			break;
		    }
	if (sptr->from)
		tmp2 = find_user_link(chptr->clist, sptr->from);
	else
		tmp2 = find_user_link(chptr->clist, sptr);
	if (tmp2 && !--tmp2->flags)
		for (curr = &chptr->clist; (tmp = *curr); curr = &tmp->next)
			if (tmp2 == tmp)
			    {
				*curr = tmp->next;
				free_link(tmp);
				break;
			    }
	if (!IsQuiet(chptr))
	    {
		sptr->user->joined--;
		istat.is_userc--;
	    }
#ifdef USE_SERVICES
	if (chptr->users == 1)
		check_services_butone(SERVICE_WANT_CHANNEL|
				      SERVICE_WANT_VCHANNEL, NULL, &me,
				      "CHANNEL %s %d", chptr->chname,
				      chptr->users-1);
	else
		check_services_butone(SERVICE_WANT_VCHANNEL, NULL, &me,
				      "CHANNEL %s %d", chptr->chname,
				      chptr->users-1);
#endif
	if (--chptr->users <= 0)
	    {
		u_int sz = sizeof(aChannel) + strlen(chptr->chname);

		istat.is_chan--;
		istat.is_chanmem -= sz;
		istat.is_hchan++;
		istat.is_hchanmem += sz;
		free_channel(chptr);
	    }
	istat.is_chanusers--;
}

static	void	change_chan_flag(lp, chptr)
Link	*lp;
aChannel *chptr;
{
	Reg	Link *tmp;
	aClient	*cptr = lp->value.cptr;

	/*
	 * Set the channel members flags...
	 */
	tmp = find_user_link(chptr->members, cptr);
	if (lp->flags & MODE_ADD)
		tmp->flags |= lp->flags & MODE_FLAGS;
	else
	    {
		tmp->flags &= ~lp->flags & MODE_FLAGS;
		if (lp->flags & CHFL_CHANOP)
			tmp->flags &= ~CHFL_UNIQOP;
	    }
	/*
	 * and make sure client membership mirrors channel
	 */
	tmp = find_user_link(cptr->user->channel, (aClient *)chptr);
	if (lp->flags & MODE_ADD)
		tmp->flags |= lp->flags & MODE_FLAGS;
	else
	    {
		tmp->flags &= ~lp->flags & MODE_FLAGS;
		if (lp->flags & CHFL_CHANOP)
			tmp->flags &= ~CHFL_UNIQOP;
	    }
}

int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*lp;
	int	chanop = 0;

	if (MyConnect(cptr) && IsPerson(cptr) && IsRestricted(cptr) &&
	    *chptr->chname != '&')
		return 0;
	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			chanop = (lp->flags & (CHFL_CHANOP|CHFL_UNIQOP));
	if (chanop)
		chptr->reop = 0;
	return chanop;
}

int	has_voice(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*lp;
	Reg	int	member;

	member = IsMember(cptr, chptr);
	lp = find_user_link(chptr->members, cptr);

	if ((!lp || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE))) &&
	    !match_modeid(CHFL_EXCEPTION, cptr, chptr) &&
	    match_modeid(CHFL_BAN, cptr, chptr))
		return (MODE_BAN);

	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE))))
			return (MODE_MODERATED);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (MODE_NOPRIVMSGS);

	return 0;
}

aChannel *find_channel(chname, chptr)
Reg	char	*chname;
Reg	aChannel *chptr;
{
	aChannel *achptr = chptr;

	if (chname && *chname)
		achptr = hash_find_channel(chname, chptr);
	return achptr;
}

void	setup_server_channels(mp)
aClient	*mp;
{
	aChannel	*chptr;
	int	smode;

	smode = MODE_MODERATED|MODE_TOPICLIMIT|MODE_NOPRIVMSGS|MODE_ANONYMOUS|
		MODE_QUIET;

	chptr = get_channel(mp, "&ERRORS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: server errors");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&NOTICES", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: warnings and notices");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&KILLS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: operator and server kills");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&CHANNEL", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: fake modes");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&NUMERICS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: numerics received");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&SERVERS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: servers joining and leaving");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&HASH", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: hash tables growth");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&LOCAL", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: notices about local connections");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&SERVICES", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: services joining and leaving");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
#if defined(USE_IAUTH)
	chptr = get_channel(mp, "&AUTH", CREATE);
	strcpy(chptr->topic,
	       "SERVER MESSAGES: messages from the authentication slave");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
#endif
	chptr = get_channel(mp, "&SAVE", CREATE);
	strcpy(chptr->topic,
	       "SERVER MESSAGES: save messages");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&DEBUG", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: debug messages [you shouldn't be here! ;)]");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode|MODE_SECRET;

	setup_svchans();
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
void	channel_modes(cptr, mbuf, pbuf, chptr)
aClient	*cptr;
Reg	char	*mbuf, *pbuf;
aChannel *chptr;
{
	*mbuf++ = '+';
	if (chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	else if (chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if (chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if (chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if (chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if (chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';
	if (chptr->mode.mode & MODE_ANONYMOUS)
		*mbuf++ = 'a';
	if (chptr->mode.mode & MODE_QUIET)
		*mbuf++ = 'q';
	if (chptr->mode.mode & MODE_REOP)
		*mbuf++ = 'r';
	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr))
			SPRINTF(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr))
			(void)strcat(pbuf, chptr->mode.key);
	    }
	*mbuf++ = '\0';
	return;
}

static	void	send_mode_list(cptr, chname, top, mask, flag)
aClient	*cptr;
Link	*top;
int	mask;
char	flag, *chname;
{
	Reg	Link	*lp;
	Reg	char	*cp, *name;
	int	count = 0, send = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)
	{
		/*
		** we have some modes in parabuf,
		** so check how many of them.
		** however, don't count initial '+'
		*/
		count = strlen(modebuf) - 1;
	}
	for (lp = top; lp; lp = lp->next)
	{
		if (!(lp->flags & mask))
			continue;
		if (mask == CHFL_BAN || mask == CHFL_EXCEPTION ||
		    mask == CHFL_INVITE)
			name = lp->value.cp;
		else
			name = lp->value.cptr->name;
		if (strlen(parabuf) + strlen(name) + 10 < (size_t) MODEBUFLEN)
		{
			if (*parabuf)
			{
				(void)strcat(parabuf, " ");
			}
			(void)strcat(parabuf, name);
			count++;
			*cp++ = flag;
			*cp = '\0';
		}
		else
		{
			if (*parabuf)
			{
				send = 1;
			}
		}
		if (count == MAXMODEPARAMS)
		{
			send = 1;
		}
		if (send)
		{
			/*
			** send out MODEs, it's either MAXMODEPARAMS of them
			** or long enough that they filled up parabuf
			*/
			sendto_one(cptr, ":%s MODE %s %s %s",
				   ME, chname, modebuf, parabuf);
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MAXMODEPARAMS)
			{
				/*
				** we weren't able to fit another 'name'
				** into parabuf, so we have to send it
				** in another turn, appending it now to
				** empty parabuf and setting count to 1
				*/
				(void)strcpy(parabuf, name);
				*cp++ = flag;
				count = 1;
			}
			else
			{
				count = 0;
			}
			*cp = '\0';
		}
	}
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void	send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
#if 0
this is probably going to be very annoying, but leaving the following code
uncommented may just lead to desynchs..
	if ((*chptr->chname != '#' && *chptr->chname != '!')
	    || chptr->users == 0) /* channel is empty (locked), thus no mode */
		return;
#endif

	if (check_channelmask(&me, cptr, chptr->chname))
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (modebuf[1] || *parabuf)
		sendto_one(cptr, ":%s MODE %s %s %s",
			   ME, chptr->chname, modebuf, parabuf);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_mode_list(cptr, chptr->chname, chptr->mlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
	    {
		/* only needed to help compatibility */
		sendto_one(cptr, ":%s MODE %s %s %s",
			   ME, chptr->chname, modebuf, parabuf);
		*parabuf = '\0';
		*modebuf = '+';
		modebuf[1] = '\0';
	    }
	send_mode_list(cptr, chptr->chname, chptr->mlist,
		       CHFL_EXCEPTION, 'e');
	send_mode_list(cptr, chptr->chname, chptr->mlist,
		       CHFL_INVITE, 'I');
	if (modebuf[1] || *parabuf)
		sendto_one(cptr, ":%s MODE %s %s %s",
			   ME, chptr->chname, modebuf, parabuf);
}

/*
 * send "cptr" a full list of the channel "chptr" members and their
 * +ov status, using NJOIN
 */
void	send_channel_members(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*lp;
	Reg	aClient *c2ptr;
	Reg	int	cnt = 0, len = 0, nlen;
	char	*p;

	if (check_channelmask(&me, cptr, chptr->chname) == -1)
		return;
	sprintf(buf, ":%s NJOIN %s :", ME, chptr->chname);
	len = strlen(buf);

	for (lp = chptr->members; lp; lp = lp->next)
	    {
		c2ptr = lp->value.cptr;
		p = (ST_UID(cptr) && HasUID(c2ptr)) ?
			c2ptr->user->uid : c2ptr->name;
		nlen = strlen(p);
		if ((len + nlen) > (size_t) (BUFSIZE - 9)) /* ,@+ \r\n\0 */
		    {
			sendto_one(cptr, "%s", buf);
			sprintf(buf, ":%s NJOIN %s :", ME, chptr->chname);
			len = strlen(buf);
			cnt = 0;
		    }
		if (cnt)
		    {
			buf[len++] = ',';
			buf[len] = '\0';
		    }
		if (lp->flags & (CHFL_UNIQOP|CHFL_CHANOP|CHFL_VOICE))
		    {
			if (lp->flags & CHFL_UNIQOP)
			    {
				buf[len++] = '@';
				buf[len++] = '@';
			    }
			else
			    {
				if (lp->flags & CHFL_CHANOP)
					buf[len++] = '@';
			    }
			if (lp->flags & CHFL_VOICE)
				buf[len++] = '+';
			buf[len] = '\0';
		    }
		(void)strcpy(buf + len, p);
		len += nlen;
		cnt++;
	    }
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}

/*
 * m_mode
 * parv[0] - sender
 * parv[1] - target; channels and/or user
 * parv[2] - optional modes
 * parv[n] - optional parameters
 */

int	m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	int	mcount = 0, chanop;
	int	penalty = 0;
	aChannel *chptr;
	char	*name, *p = NULL;

	if (parc < 1)
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]), "MODE");
 	 	return 1;
	    }

	parv[1] = canonize(parv[1]);

	for (name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	    {
		clean_channelname(name);
		chptr = find_channel(name, NullChn);
		if (chptr == NullChn)
		    {
			parv[1] = name;
			penalty += m_umode(cptr, sptr, parc, parv);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
		    {
			penalty += 1;
			continue;
		    }
		if (!UseModes(name))
		    {
			sendto_one(sptr, replies[ERR_NOCHANMODES], ME, BadTo(parv[0]),
				   name);
			penalty += 1;
			continue;
		    }
		chanop = is_chan_op(sptr, chptr) || IsServer(sptr);

		if (parc < 3)	/* Only a query */
		    {
			*modebuf = *parabuf = '\0';
			modebuf[1] = '\0';
			channel_modes(sptr, modebuf, parabuf, chptr);
			sendto_one(sptr, replies[RPL_CHANNELMODEIS], ME, BadTo(parv[0]),
				   name, modebuf, parabuf);
			penalty += 1;
		    }
		else	/* Check parameters for the channel */
		    {
			if(!(mcount = set_mode(cptr, sptr, chptr,
				&penalty, parc - 2, parv + 2)))
				continue;	/* no valid mode change */
		    } /* else(parc>2) */
	    } /* for (parv1) */
	return penalty;
}

/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client cptr to channel chptr.
 * Also sends it to everybody that should get it.
 */
static	int	set_mode(cptr, sptr, chptr, penalty, parc, parv)
Reg	aClient *cptr, *sptr;
aChannel *chptr;
int	parc, *penalty;
char	*parv[];
{
	static	Link	chops[MAXMODEPARAMS+3];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				MODE_ANONYMOUS,  'a', MODE_REOP,       'r',
				0x0, 0x0 };

	Reg	Link	*lp = NULL;
	Reg	char	*curr = parv[0], *cp = NULL, *ucp;
	Reg	int	*ip;
	u_int	whatt = MODE_ADD;
	int	limitset = 0, count = 0, chasing = 0;
	int	nusers = 0, ischop, new, len, ulen, keychange = 0, opcnt = 0;
	aClient *who;
	Mode	*mode, oldm;
	Link	*plp = NULL;
	int	compat = -1; /* to prevent mixing old/new modes */
	char	*mbuf = modebuf, *pbuf = parabuf, *upbuf = uparabuf;

	*mbuf = *pbuf = *upbuf = '\0';
	if (parc < 1)
		return 0;

	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
	ischop = IsServer(sptr) || is_chan_op(sptr, chptr);
	new = mode->mode;

	while (curr && *curr && count >= 0)
	    {
		if (compat == -1 && *curr != '-' && *curr != '+')
			if (*curr == 'e' || *curr == 'I')
				compat = 1;
			else
				compat = 0;
		switch (*curr)
		{
		case '+':
			whatt = MODE_ADD;
			break;
		case '-':
			whatt = MODE_DEL;
			break;
		case 'O':
			if (parc > 0)
			    {
			if (*chptr->chname == '!')
			    {
			    if (IsMember(sptr, chptr))
			        {
					*penalty += 1;
					parc--;
					/* Feature: no other modes after this query */
     	                           *(curr+1) = '\0';
					for (lp = chptr->members; lp; lp = lp->next)
						if (lp->flags & CHFL_UNIQOP)
						    {
							sendto_one(sptr,
							   replies[RPL_UNIQOPIS],
								   ME, BadTo(sptr->name),
								   chptr->chname,
							   lp->value.cptr->name);
							break;
						    }
					if (!lp)
						sendto_one(sptr,
							   replies[ERR_NOSUCHNICK],
								   ME, BadTo(sptr->name),
							   chptr->chname);
					break;
				    }
				else /* not IsMember() */
				    {
					if (!IsServer(sptr))
					    {
						sendto_one(sptr, replies[ERR_NOTONCHANNEL], ME, BadTo(sptr->name),
							    chptr->chname);
						*(curr+1) = '\0';
						break;
					    }
				    }
			    }
			else /* *chptr->chname != '!' */
				sendto_one(cptr, replies[ERR_UNKNOWNMODE],
					ME, BadTo(sptr->name), *curr, chptr->chname);
					*(curr+1) = '\0';
					break;
			    }
			/*
			 * is this really ever used ?
			 * or do ^G & NJOIN do the trick?
			 */
			if (*chptr->chname != '!' || whatt == MODE_DEL ||
			    !IsServer(sptr))
			    {
				*penalty += 1;
				--parc;
				parv++;
				break;
			    }
		case 'o' :
		case 'v' :
			*penalty += 1;
			if (--parc <= 0)
				break;
			parv++;
			*parv = check_string(*parv);
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (!IsServer(sptr) && !IsMember(sptr, chptr))
			    {
				sendto_one(sptr, replies[ERR_NOTONCHANNEL],
								 ME, BadTo(sptr->name),
					    chptr->chname);
				break;
			    }
			/*
			 * Check for nickname changes and try to follow these
			 * to make sure the right client is affected by the
			 * mode change.
			 */
			if (!(IsServer(cptr) &&
				(who = find_uid(parv[0], NULL))) &&
				!(who = find_chasing(sptr, parv[0], &chasing)))
				break;
	  		if (!IsMember(who, chptr))
			    {
				sendto_one(sptr, replies[ERR_USERNOTINCHANNEL],
							 ME, BadTo(sptr->name),
					   who->name, chptr->chname);
				break;
			    }
			if (who == cptr && whatt == MODE_ADD && *curr == 'o')
				break;

			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				lp->flags = (*curr == 'O') ? MODE_UNIQOP:
			    			(*curr == 'o') ? MODE_CHANOP:
								  MODE_VOICE;
				lp->flags |= MODE_ADD;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
								  MODE_VOICE;
				lp->flags |= MODE_DEL;
			    }
			if (plp && plp->flags == lp->flags &&
			    plp->value.cptr == lp->value.cptr)
			    {
				opcnt--;
				break;
			    }
			plp = lp;
			/*
			** If this server noticed the nick change, the
			** information must be propagated back upstream.
			** This is a bit early, but at most this will generate
			** just some extra messages if nick appeared more than
			** once in the MODE message... --msa
			*/
/* nobody can figure this part of the code anymore.. -kalt
			if (chasing && ischop)
				sendto_one(cptr, ":%s MODE %s %c%c %s",
					   ME, chptr->chname,
					   whatt == MODE_ADD ? '+' : '-',
					   *curr, who->name);
*/
			count++;
			*penalty += 2;
			break;
		case 'k':
			*penalty += 1;
			if (--parc <= 0)
				break;
			parv++;
			/* check now so we eat the parameter if present */
			if (keychange)
				break;
			{
				Reg	u_char	*s;

				for (s = (u_char *)*parv; *s; )
				    {
					/* comma cannot be inside key --Beeth */
					if (*s == ',') 
						*s = '.';
					if (*s > 0x7f)
						if (*s > 0xa0)
							*s++ &= 0x7f;
						else
							*s = '\0';
					else
						s++;
				    }
			}

			if (!**parv)
				break;
			*parv = check_string(*parv);
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (whatt == MODE_ADD)
			    {
				if (*mode->key && !IsServer(cptr))
					sendto_one(cptr, replies[ERR_KEYSET],
						   ME, BadTo(cptr->name), chptr->chname);
				else if (ischop &&
				    (!*mode->key || IsServer(cptr)))
				    {
					if (**parv == ':')
						/* this won't propagate right*/
						break;
					lp = &chops[opcnt++];
					lp->value.cp = *parv;
					if (strlen(lp->value.cp) >
					    (size_t) KEYLEN)
						lp->value.cp[KEYLEN] = '\0';
					lp->flags = MODE_KEY|MODE_ADD;
					keychange = 1;
				    }
			    }
			else if (whatt == MODE_DEL)
			    {
				if (ischop || IsServer(cptr))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = mode->key;
					lp->flags = MODE_KEY|MODE_DEL;
					keychange = 1;
				    }
			    }
			count++;
			*penalty += 2;
			break;
		case 'b':
			*penalty += 1;
			if (--parc <= 0)	/* ban list query */
			    {
				/* Feature: no other modes after ban query */
				*(curr+1) = '\0';	/* Stop MODE # bb.. */
				for (lp = chptr->mlist; lp; lp = lp->next)
					if (lp->flags == CHFL_BAN)
						sendto_one(cptr,
							   replies[RPL_BANLIST],
								   ME, BadTo(cptr->name),
							   chptr->chname,
							   lp->value.cp);
				sendto_one(cptr, replies[RPL_ENDOFBANLIST],
					   ME, BadTo(cptr->name), chptr->chname);
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (whatt == MODE_ADD)
			    {
				if (**parv == ':')
					/* this won't propagate right */
					break;
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_BAN;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_BAN;
			    }
			count++;
			*penalty += 2;
			break;
		case 'e':
			*penalty += 1;
			if (--parc <= 0)	/* exception list query */
			    {
				/* Feature: no other modes after query */
				*(curr+1) = '\0';	/* Stop MODE # bb.. */
				for (lp = chptr->mlist; lp; lp = lp->next)
					if (lp->flags == CHFL_EXCEPTION)
						sendto_one(cptr,
						   replies[RPL_EXCEPTLIST],
								   ME, BadTo(cptr->name),
							   chptr->chname,
							   lp->value.cp);
				sendto_one(cptr, replies[RPL_ENDOFEXCEPTLIST],
					   ME, BadTo(cptr->name), chptr->chname);
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (whatt == MODE_ADD)
			    {
				if (**parv == ':')
					/* this won't propagate right */
					break;
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_EXCEPTION;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_EXCEPTION;
			    }
			count++;
			*penalty += 2;
			break;
		case 'I':
			*penalty += 1;
			if (--parc <= 0)	/* invite list query */
			    {
				/* Feature: no other modes after query */
				*(curr+1) = '\0';	/* Stop MODE # bb.. */
				for (lp = chptr->mlist; lp; lp = lp->next)
					if (lp->flags == CHFL_INVITE)
						sendto_one(cptr,
						   replies[RPL_INVITELIST],
								   ME, BadTo(cptr->name),
							   chptr->chname,
							   lp->value.cp);
				sendto_one(cptr, replies[RPL_ENDOFINVITELIST],
					   ME, BadTo(cptr->name), chptr->chname);
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (whatt == MODE_ADD)
			    {
				if (**parv == ':')
					/* this won't propagate right */
					break;
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_INVITE;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_INVITE;
			    }
			count++;
			*penalty += 2;
			break;
		case 'l':
			*penalty += 1;
			/*
			 * limit 'l' to only *1* change per mode command but
			 * eat up others.
			 */
			if (limitset || !ischop)
			    {
				if (whatt == MODE_ADD && --parc > 0)
					parv++;
				break;
			    }
			if (whatt == MODE_DEL)
			    {
				limitset = 1;
				nusers = 0;
				count++;
				break;
			    }
			if (--parc > 0)
			    {
				if (BadPtr(*parv))
					break;
				if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
				    if (MyClient(sptr) ||
					opcnt >= MAXMODEPARAMS + 1)
#endif
					break;
				if (!(nusers = atoi(*++parv)))
					break;
				lp = &chops[opcnt++];
				lp->flags = MODE_ADD|MODE_LIMIT;
				limitset = 1;
				count++;
				*penalty += 2;
				break;
			    }
			sendto_one(cptr, replies[ERR_NEEDMOREPARAMS],
				   ME, BadTo(cptr->name), "MODE +l");
			break;
		case 'i' : /* falls through for default case */
			if (whatt == MODE_DEL && ischop)
				while ((lp = chptr->invites))
					del_invite(lp->value.cptr, chptr);
		default:
			*penalty += 1;
			for (ip = flags; *ip; ip += 2)
				if (*(ip+1) == *curr)
					break;

			if (*ip)
			    {
				if (*ip == MODE_ANONYMOUS &&
				    whatt == MODE_DEL && *chptr->chname == '!')
					sendto_one(sptr,
					   replies[ERR_UNIQOPRIVSNEEDED],
						   ME, BadTo(sptr->name), chptr->chname);
				else if (((*ip == MODE_ANONYMOUS &&
					   whatt == MODE_ADD &&
					   *chptr->chname == '#') ||
					  (*ip == MODE_REOP &&
					   *chptr->chname != '!')) &&
					 !IsServer(sptr))
					sendto_one(cptr,
						   replies[ERR_UNKNOWNMODE],
						   ME, BadTo(sptr->name), *curr,
						   chptr->chname);
				else if ((*ip == MODE_REOP ||
					  *ip == MODE_ANONYMOUS) &&
					 !IsServer(sptr) &&
					 !(is_chan_op(sptr,chptr) &CHFL_UNIQOP)
					 && *chptr->chname == '!')
					/* 2 modes restricted to UNIQOP */
					sendto_one(sptr,
					   replies[ERR_UNIQOPRIVSNEEDED],
						   ME, BadTo(sptr->name), chptr->chname);
				else
				    {
					/*
				        ** If the channel is +s, ignore +p
					** modes coming from a server.
					** (Otherwise, it's desynch'ed) -kalt
					*/
					if (whatt == MODE_ADD &&
					    *ip == MODE_PRIVATE &&
					    IsServer(sptr) &&
					    (new & MODE_SECRET))
						break;
					if (whatt == MODE_ADD)
					    {
						if (*ip == MODE_PRIVATE)
							new &= ~MODE_SECRET;
						else if (*ip == MODE_SECRET)
							new &= ~MODE_PRIVATE;
						new |= *ip;
					    }
					else
						new &= ~*ip;
					count++;
					*penalty += 2;
				    }
			    }
			else if (!IsServer(cptr))
				sendto_one(cptr, replies[ERR_UNKNOWNMODE],
					   ME, BadTo(cptr->name), *curr, chptr->chname);
			break;
		}
		curr++;
		/*
		 * Make sure modes strings such as "+m +t +p +i" are parsed
		 * fully.
		 */
		if (!*curr && parc > 0)
		    {
			curr = *++parv;
			parc--;
		    }
		/*
		 * Make sure old and new (+e/+I) modes won't get mixed
		 * together on the same line
		 */
		if (MyClient(sptr) && curr && *curr != '-' && *curr != '+')
			if (*curr == 'e' || *curr == 'I')
			    {
				if (compat == 0)
					*curr = '\0';
			    }
			else if (compat == 1)
				*curr = '\0';
	    } /* end of while loop for MODE processing */

	whatt = 0;

	for (ip = flags; *ip; ip += 2)
		if ((*ip & new) && !(*ip & oldm.mode))
		    {
			if (whatt == 0)
			    {
				*mbuf++ = '+';
				whatt = 1;
			    }
			if (ischop)
			  {
				mode->mode |= *ip;
				if (*ip == MODE_ANONYMOUS && MyPerson(sptr))
				  {
				      sendto_channel_butone(NULL, &me, chptr, ":%s NOTICE %s :The anonymous flag is being set on channel %s.", ME, chptr->chname, chptr->chname);
				      sendto_channel_butone(NULL, &me, chptr, ":%s NOTICE %s :Be aware that anonymity on IRC is NOT securely enforced!", ME, chptr->chname);
				  }
			  }
			*mbuf++ = *(ip+1);
		    }

	for (ip = flags; *ip; ip += 2)
		if ((*ip & oldm.mode) && !(*ip & new))
		    {
			if (whatt != -1)
			    {
				*mbuf++ = '-';
				whatt = -1;
			    }
			if (ischop)
				mode->mode &= ~*ip;
			*mbuf++ = *(ip+1);
		    }

	if (limitset && !nusers && mode->limit)
	    {
		if (whatt != -1)
		    {
			*mbuf++ = '-';
			whatt = -1;
		    }
		mode->mode &= ~MODE_LIMIT;
		mode->limit = 0;
		*mbuf++ = 'l';
	    }

	/*
	 * Reconstruct "+beIkOov" chain.
	 */
	if (opcnt)
	    {
		Reg	int	i = 0;
		Reg	char	c = '\0';
		char	*user, *host, numeric[16];

/*		if (opcnt > MAXMODEPARAMS)
			opcnt = MAXMODEPARAMS;
*/
		for (; i < opcnt; i++)
		    {
			lp = &chops[i];
			/*
			 * make sure we have correct mode change sign
			 */
			if (whatt != (lp->flags & (MODE_ADD|MODE_DEL)))
				if (lp->flags & MODE_ADD)
				    {
					*mbuf++ = '+';
					whatt = MODE_ADD;
				    }
				else
				    {
					*mbuf++ = '-';
					whatt = MODE_DEL;
				    }
			len = strlen(pbuf);
			ulen = strlen(upbuf);
			/*
			 * get c as the mode char and tmp as a pointer to
			 * the paramter for this mode change.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_CHANOP :
				c = 'o';
				cp = lp->value.cptr->name;
				ucp = lp->value.cptr->user->uid;
				break;
			case MODE_UNIQOP :
				c = 'O';
				cp = lp->value.cptr->name;
				ucp = lp->value.cptr->user->uid;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				ucp = lp->value.cptr->user->uid;
				break;
			case MODE_BAN :
				c = 'b';
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				cp = make_nick_user_host(cp, user, host);
				if (user)
					*(--user) = '!';
				if (host)
					*(--host) = '@';
				break;
			case MODE_EXCEPTION :
				c = 'e';
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				cp = make_nick_user_host(cp, user, host);
				if (user)
					*(--user) = '!';
				if (host)
					*(--host) = '@';
				break;
			case MODE_INVITE :
				c = 'I';
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				cp = make_nick_user_host(cp, user, host);
				if (user)
					*(--user) = '!';
				if (host)
					*(--host) = '@';
				break;
			case MODE_KEY :
				c = 'k';
				cp = lp->value.cp;
				break;
			case MODE_LIMIT :
				c = 'l';
				(void)sprintf(numeric, "%-15d", nusers);
				if ((cp = index(numeric, ' ')))
					*cp = '\0';
				cp = numeric;
				break;
			}

			if (len + strlen(cp) + 2 > (size_t) MODEBUFLEN)
				break;
			/*
			 * pass on +/-o/v regardless of whether they are
			 * redundant or effective but check +b's to see if
			 * it existed before we created it.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_KEY :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				(void)strcat(upbuf, cp);
				len += strlen(cp);
				ulen += strlen(cp);
				(void)strcat(pbuf, " ");
				(void)strcat(upbuf, " ");
				len++;
				ulen++;
				if (!ischop)
					break;
				if (strlen(cp) > (size_t) KEYLEN)
					*(cp+KEYLEN) = '\0';
				if (whatt == MODE_ADD)
					strncpyzt(mode->key, cp,
						  sizeof(mode->key));
				else
					*mode->key = '\0';
				break;
			case MODE_LIMIT :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				(void)strcat(upbuf, cp);
				len += strlen(cp);
				ulen += strlen(cp);
				(void)strcat(pbuf, " ");
				(void)strcat(upbuf, " ");
				len++;
				ulen++;
				if (!ischop)
					break;
				mode->limit = nusers;
				break;
			case MODE_CHANOP : /* fall through case */
				if (ischop && lp->value.cptr == sptr &&
				    lp->flags == MODE_CHANOP|MODE_DEL)
					chptr->reop = timeofday + 
						LDELAYCHASETIMELIMIT;
			case MODE_UNIQOP :
			case MODE_VOICE :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				(void)strcat(upbuf, ucp);
				len += strlen(cp);
				ulen += strlen(ucp);
				(void)strcat(pbuf, " ");
				(void)strcat(upbuf, " ");
				len++;
				ulen++;
				if (ischop)
					change_chan_flag(lp, chptr);
				break;
			case MODE_BAN :
				if (ischop &&
				    (((whatt & MODE_ADD) &&
				      !add_modeid(CHFL_BAN, sptr, chptr, cp))||
				     ((whatt & MODE_DEL) &&
				      !del_modeid(CHFL_BAN, chptr, cp))))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					(void)strcat(upbuf, cp);
					len += strlen(cp);
					ulen += strlen(cp);
					(void)strcat(pbuf, " ");
					(void)strcat(upbuf, " ");
					len++;
					ulen++;
				    }
				break;
			case MODE_EXCEPTION :
				if (ischop &&
				    (((whatt & MODE_ADD) &&
			      !add_modeid(CHFL_EXCEPTION, sptr, chptr, cp))||
				     ((whatt & MODE_DEL) &&
				      !del_modeid(CHFL_EXCEPTION, chptr, cp))))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					(void)strcat(upbuf, cp);
					len += strlen(cp);
					ulen += strlen(cp);
					(void)strcat(pbuf, " ");
					(void)strcat(upbuf, " ");
					len++;
					ulen++;
				    }
				break;
			case MODE_INVITE :
				if (ischop &&
				    (((whatt & MODE_ADD) &&
			      !add_modeid(CHFL_INVITE, sptr, chptr, cp))||
				     ((whatt & MODE_DEL) &&
				      !del_modeid(CHFL_INVITE, chptr, cp))))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					(void)strcat(upbuf, cp);
					len += strlen(cp);
					ulen += strlen(cp);
					(void)strcat(pbuf, " ");
					(void)strcat(upbuf, " ");
					len++;
					ulen++;
				    }
				break;
			}
		    } /* for (; i < opcnt; i++) */
	    } /* if (opcnt) */

	*mbuf = '\0';
	mbuf = modebuf;

	if ((!ischop) && (count) && MyConnect(sptr) && !IsServer(sptr))
	{
		/* rejected mode change */
		int num = ERR_CHANOPRIVSNEEDED;

		if (IsClient(sptr) && IsRestricted(sptr))
		{
			num = ERR_RESTRICTED;
		}
		sendto_one(sptr, replies[num], ME, sptr->name, chptr->chname);
		return -count;
	}

	/* Send the mode changes out. */
	if (strlen(modebuf) > 1)
	{
		char	*s; /* Sender for messages to 2.11s. */

		if (IsServer(sptr))
		{
			s = sptr->serv->sid;
		}
		else if (HasUID(sptr))
		{
			s = sptr->user->uid;
		}
		else
		{
			s = sptr->name;
		}

		sendto_match_servs_v(chptr, cptr, SV_UID,
			":%s MODE %s %s %s", s, chptr->chname, mbuf, upbuf);
		sendto_match_servs_notv(chptr, cptr, SV_UID, 
			":%s MODE %s %s %s", sptr->name, chptr->chname, mbuf,
			pbuf);

		if ((IsServer(cptr) && !IsServer(sptr) && !ischop))
		{
			sendto_flag(SCH_CHAN, "Fake: %s MODE %s %s %s",
				    sptr->name, chptr->chname, mbuf, pbuf);
			ircstp->is_fake++;
		}
		else
		{
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
				sptr->name, chptr->chname, mbuf, pbuf);
#ifdef USE_SERVICES
			*modebuf = *parabuf = '\0';
			modebuf[1] = '\0';
			channel_modes(&me, modebuf, parabuf, chptr);
			check_services_butone(SERVICE_WANT_MODE, NULL, sptr,
				"MODE %s %s", chptr->chname, modebuf);
#endif
		}
	}

	return ischop ? count : -count;
}

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
Reg	aChannel *chptr;
char	*key;
{
	Link	*lp = NULL, *banned;

	if (chptr->users == 0 && (bootopt & BOOT_PROT) && 
	    chptr->history != 0 && *chptr->chname != '!')
		return (timeofday > chptr->history) ? 0 : ERR_UNAVAILRESOURCE;

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
			break;

	if (banned = match_modeid(CHFL_BAN, sptr, chptr))
		if (match_modeid(CHFL_EXCEPTION, sptr, chptr))
			banned = NULL;
		else if (lp == NULL)
			return (ERR_BANNEDFROMCHAN);

	if ((chptr->mode.mode & MODE_INVITEONLY)
	    && !match_modeid(CHFL_INVITE, sptr, chptr)
	    && (lp == NULL))
		return (ERR_INVITEONLYCHAN);

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (chptr->mode.limit && (chptr->users >= chptr->mode.limit) &&
	    (lp == NULL))
		return (ERR_CHANNELISFULL);

	if (banned)
		sendto_channel_butone(&me, &me, chptr,
       ":%s NOTICE %s :%s carries an invitation (overriding ban on %s).",
				       ME, chptr->chname, sptr->name,
				       banned->value.cp);
	return 0;
}

/*
** Remove bells and commas from channel name
*/

void	clean_channelname(cn)
Reg	char *cn;
{
	for (; *cn; cn++)
		if (*cn == '\007' || *cn == ' ' || *cn == ',')
		    {
			*cn = '\0';
			return 0;
		    }
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
static	int	check_channelmask(sptr, cptr, chname)
aClient	*sptr, *cptr;
char	*chname;
{
	Reg	char	*s, *t;

	if (*chname == '&' && IsServer(cptr))
		return -1;
	s = rindex(chname, ':');
	if (!s)
		return 0;
	if ((t = index(s, '\007')))
		*t = '\0';

	s++;
	if (match(s, ME) || (IsServer(cptr) && match(s, cptr->name)))
	    {
		if (MyClient(sptr))
			sendto_one(sptr, replies[ERR_BADCHANMASK], ME, BadTo(sptr->name),
				   chname);
		if (t)
			*t = '\007';
		return -1;
	    }
	if (t)
		*t = '\007';
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static	aChannel *get_channel(cptr, chname, flag)
aClient *cptr;
char	*chname;
int	flag;
    {
	Reg	aChannel *chptr;
	int	len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyClient(cptr) && len > CHANNELLEN)
	    {
		len = CHANNELLEN;
		*(chname+CHANNELLEN) = '\0';
	    }
	if ((chptr = find_channel(chname, (aChannel *)NULL)))
		return (chptr);
	if (flag == CREATE)
	    {
		chptr = (aChannel *)MyMalloc(sizeof(aChannel) + len);
		bzero((char *)chptr, sizeof(aChannel));
		strncpyzt(chptr->chname, chname, len+1);
		if (channel)
			channel->prevch = chptr;
		chptr->prevch = NULL;
		chptr->nextch = channel;
		chptr->history = 0;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
	    }
	return chptr;
    }

static	void	add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*inv, **tmp;

	del_invite(cptr, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	    {
/*		This forgets the channel side of invitation     -Vesa
		inv = cptr->user->invited;
		cptr->user->invited = inv->next;
		free_link(inv);
*/
		del_invite(cptr, cptr->user->invited->value.chptr);
	    }
	/*
	 * add client to channel invite list
	 */
	inv = make_link();
	inv->value.cptr = cptr;
	inv->next = chptr->invites;
	chptr->invites = inv;
	istat.is_useri++;
	/*
	 * add channel to the end of the client invite list
	 */
	for (tmp = &(cptr->user->invited); *tmp; tmp = &((*tmp)->next))
		;
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = NULL;
	(*tmp) = inv;
	istat.is_invite++;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void	del_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	**inv, *tmp;

	for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.cptr == cptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			istat.is_invite--;
			break;
		    }

	for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			istat.is_useri--;
			break;
		    }
}

/*
**  The last user has left the channel, free data in the channel block,
**  and eventually the channel block itself.
*/
static	void	free_channel(chptr)
aChannel *chptr;
{
	Reg	Link *tmp;
	Link	*obtmp;
	int	len = sizeof(aChannel) + strlen(chptr->chname), now = 0;

        if (chptr->history == 0 || timeofday >= chptr->history)
		/* no lock, nor expired lock, channel is no more, free it */
		now = 1;

	if (*chptr->chname != '!' || now)
	    {
		while ((tmp = chptr->invites))
			del_invite(tmp->value.cptr, chptr);
		
		tmp = chptr->mlist;
		while (tmp)
		    {
			obtmp = tmp;
			tmp = tmp->next;
			istat.is_banmem -= (strlen(obtmp->value.cp) + 1);
			istat.is_bans--;
			MyFree(obtmp->value.cp);
			free_link(obtmp);
		    }
		chptr->mlist = NULL;
	    }

	if (now)
	    {
		istat.is_hchan--;
		istat.is_hchanmem -= len;
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		del_from_channel_hash_table(chptr->chname, chptr);

		if (*chptr->chname == '!' && close_chid(chptr->chname+1))
			cache_chid(chptr);
		else
			MyFree((char *)chptr);
	    }
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int	m_join(cptr, sptr, parc, parv)
Reg	aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static	char	jbuf[BUFSIZE], cbuf[BUFSIZE];
	Reg	Link	*lp;
	Reg	aChannel *chptr;
	Reg	char	*name, *key = NULL;
	int	i, flags = 0;
	char	*p = NULL, *p2 = NULL, *s, chop[5];

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]), "JOIN");
		return 1;
	    }

	*jbuf = '\0';
	*cbuf = '\0';
	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
	** Also note that this can easily trash the correspondance between
	** parv[1] and parv[2] lists.
	*/
	for (i = 0, name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	    {
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;
		if (*name == '0' && !atoi(name))
		    {
			(void)strcpy(jbuf, "0");
			continue;
		    }
		if (MyClient(sptr))
		    {
			clean_channelname(name);
		    }
		if (*name == '!')
		    {
			chptr = NULL;
			/*
			** !channels are special:
			**	!!channel is supposed to be a new channel,
			**		and requires a unique name to be built.
			**		( !#channel is obsolete )
			**	!channel cannot be created, and must already
			**		exist.
			*/
			if (*(name+1) == '\0' ||
			    (*(name+1) == '#' && *(name+2) == '\0') ||
			    (*(name+1) == '!' && *(name+2) == '\0'))
			    {
				if (MyClient(sptr))
					sendto_one(sptr,
						   replies[ERR_NOSUCHCHANNEL],
							   ME, BadTo(parv[0]), name);
				continue;
			    }
			if (*name == '!' && (*(name+1) == '#' ||
					     *(name+1) == '!'))
			    {
				if (!MyClient(sptr))
				    {
					sendto_flag(SCH_NOTICE,
				   "Invalid !%c channel from %s for %s",
						    *(name+1),
						    get_client_name(cptr,TRUE),
						    sptr->name);
					continue;
				    }
#if 0
				/*
				** Note: creating !!!foo, e.g. !<ID>!foo is
				** a stupid thing to do because /join !!foo
				** will not join !<ID>!foo but create !<ID>foo
				** Some logic here could be reversed, but only
				** to find that !<ID>foo would be impossible to
				** create if !<ID>!foo exists.
				** which is better? it's hard to say -kalt
				*/
				if (*(name+3) == '!')
				    {
					sendto_one(sptr,
						   replies[ERR_NOSUCHCHANNEL],
							   ME, BadPtr(parv[0]), name);
					continue;
				    }
#endif
				chptr = hash_find_channels(name+2, NULL);
				if (chptr)
				    {
					sendto_one(sptr,
						   replies[ERR_TOOMANYTARGETS],
							   ME, BadTo(parv[0]),
						   "Duplicate", name,
						   "Join aborted.");
					continue;
				    }
				if (check_chid(name+2))
				    {
					/*
					 * This is a bit wrong: if a channel
					 * rightfully ceases to exist, it
 					 * can still be *locked* for up to
					 * 2*CHIDNB^3 seconds (~24h)
					 * Is it a reasonnable price to pay to
					 * ensure shortname uniqueness? -kalt
					 */
					sendto_one(sptr, replies[ERR_UNAVAILRESOURCE],
							   ME, BadTo(parv[0]), name);
					continue;
				    }
				sprintf(buf, "!%.*s%s", CHIDLEN, get_chid(),
					name+2);
				name = buf;
			    }
			else if (!find_channel(name, NullChn) &&
				 !(*name == '!' && *name != 0 &&
				   (chptr = hash_find_channels(name+1, NULL))))
			    {
				if (MyClient(sptr))
					sendto_one(sptr,
						   replies[ERR_NOSUCHCHANNEL],
							   ME, BadTo(parv[0]), name);
				if (!IsServer(cptr))
					continue;
				/* from a server, it is legitimate */
			    }
			else if (chptr)
			    {
				/* joining a !channel using the short name */
				if (MyConnect(sptr) &&
				    hash_find_channels(name+1, chptr))
				    {
					sendto_one(sptr,
						   replies[ERR_TOOMANYTARGETS],
							   ME, BadTo(parv[0]),
						   "Duplicate", name,
						   "Join aborted.");
					continue;
				    }
				name = chptr->chname;
			    }
		    }
		if (!IsChannelName(name) ||
		    (*name == '!' && IsChannelName(name+1)))
		    {
			if (MyClient(sptr))
				sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
					   ME, BadTo(parv[0]), name);
			continue;
		    }
		if (*jbuf)
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name)+1;
	    }

	p = NULL;
	if (parv[2])
		key = strtoken(&p2, parv[2], ",");
	for (name = strtoken(&p, jbuf, ","); name;
	     key = (key) ? strtoken(&p2, NULL, ",") : NULL,
	     name = strtoken(&p, NULL, ","))
	    {
		/*
		** JOIN 0 sends out a part for all channels a user
		** has joined.
		*/
		if (*name == '0' && !atoi(name))
		    {
			if (sptr->user->channel == NULL)
				continue;
			while ((lp = sptr->user->channel))
			    {
				chptr = lp->value.chptr;
				sendto_channel_butserv(chptr, sptr,
						PartFmt,
						parv[0], chptr->chname,
						IsAnonymous(chptr) ? "None" :
						(key ? key : parv[0]));
				remove_user_from_channel(sptr, chptr);
			    }
			sendto_match_servs(NULL, cptr, ":%s JOIN 0 :%s",
					   parv[0], key ? key : parv[0]);
			continue;
		    }

		if (cptr->serv && (s = index(name, '\007')))
			*s++ = '\0';
		else
			clean_channelname(name), s = NULL;

		if (MyConnect(sptr) &&
		    sptr->user->joined >= MAXCHANNELSPERUSER) {
			/* Feature: Cannot join &flagchannels either
			   if already joined MAXCHANNELSPERUSER times. */
			sendto_one(sptr, replies[ERR_TOOMANYCHANNELS],
				   ME, BadTo(parv[0]), name);
			/* can't return, need to send the info everywhere */
			continue;
		}

		if (MyConnect(sptr) &&
		    !strncmp(name, "\x23\x1f\x02\xb6\x03\x34\x63\x68\x02\x1f",
			     10))
		    {
			sptr->exitc = EXITC_VIRUS;
			return exit_client(sptr, sptr, &me, "Virus Carrier");
		    }

		chptr = get_channel(sptr, name, CREATE);

		if (IsMember(sptr, chptr))
			continue;
		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		    {
			sendto_one(sptr, replies[i], ME, BadTo(parv[0]), name);
			continue;
		    }

		/*
		** local client is first to enter previously nonexistant
		** channel so make them (rightfully) the Channel
		** Operator.
		*/
		flags = 0;
		chop[0] = '\0';
		if (MyConnect(sptr) && UseModes(name) &&
		    (!IsRestricted(sptr) || (*name == '&')) && !chptr->users &&
		    !(chptr->history && *chptr->chname == '!'))
		    {
			if (*name == '!')
				strcpy(chop, "\007O");
			else
				strcpy(chop, "\007o");
			s = chop+1; /* tricky */
		    }
		/*
		**  Complete user entry to the new channel (if any)
		*/
		if (s && UseModes(name))
		    {
			if (*s == 'O')
				/*
				 * there can never be another mode here,
				 * because we use NJOIN for netjoins.
				 * here, it *must* be a channel creation. -kalt
				 */
				flags |= CHFL_UNIQOP|CHFL_CHANOP;
			else if (*s == 'o')
			    {
				flags |= CHFL_CHANOP;
				if (*(s+1) == 'v')
					flags |= CHFL_VOICE;
			    }
			else if (*s == 'v')
				flags |= CHFL_VOICE;
		    }
		add_user_to_channel(chptr, sptr, flags);
		/*
		** notify all users on the channel
		*/
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
						parv[0], name);
		if (s && UseModes(name))
		    {
			/* no need if user is creating the channel */
			if (chptr->users != 1)
				sendto_channel_butserv(chptr, sptr,
						       ":%s MODE %s +%s %s %s",
						       cptr->name, name, s,
						       parv[0],
						       *(s+1)=='v'?parv[0]:"");
			*--s = '\007';
		    }
		/*
		** If s wasn't set to chop+1 above, name is now #chname^Gov
		** again (if coming from a server, and user is +o and/or +v
		** of course ;-)
		** This explains the weird use of name and chop..
		** Is this insane or subtle? -krys
		*/
		if (MyClient(sptr))
		    {
			del_invite(sptr, chptr);
			if (chptr->topic[0] != '\0')
				sendto_one(sptr, replies[RPL_TOPIC], ME, BadTo(parv[0]),
					   name, chptr->topic);

			names_channel(cptr, sptr, parv[0], chptr, 1);
			if (IsAnonymous(chptr) && !IsQuiet(chptr))
			    {
				sendto_one(sptr, ":%s NOTICE %s :Channel %s has the anonymous flag set.", ME, chptr->chname, chptr->chname);
				sendto_one(sptr, ":%s NOTICE %s :Be aware that anonymity on IRC is NOT securely enforced!", ME, chptr->chname);
			    }
		    }
		/*
	        ** notify other servers
		*/
		if (index(name, ':') || *chptr->chname == '!') /* compat */
			sendto_match_servs(chptr, cptr, ":%s JOIN :%s%s",
					   parv[0], name, chop);
		else if (*chptr->chname != '&')
		    {
			if (*cbuf)
				strcat(cbuf, ",");
			strcat(cbuf, name);
			if (chop)
				strcat(cbuf, chop);
		    }
	    }
	if (*cbuf)
		sendto_serv_butone(cptr, ":%s JOIN :%s", parv[0], cbuf);
	return 2;
}

/*
** m_njoin
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel members and modes
*/
int	m_njoin(cptr, sptr, parc, parv)
Reg	aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char nbuf[BUFSIZE], *q, *name, *target, mbuf[MAXMODEPARAMS + 1];
	char uidbuf[BUFSIZE], *u;
	char *p = NULL;
	int chop, cnt = 0, nj = 0;
	aChannel *chptr = NULL;
	aClient *acptr;

	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]),"NJOIN");
		return 1;
	    }
	*nbuf = '\0'; q = nbuf;
	u = uidbuf;
	*u = '\0';
	for (target = strtoken(&p, parv[2], ","); target;
	     target = strtoken(&p, NULL, ","))
	    {
		/* check for modes */
		chop = 0;
		mbuf[0] = '\0';
		if (*target == '@')
		    {
			if (*(target+1) == '@')
			    {
				/* actually never sends in a JOIN ^G */
				if (*(target+2) == '+')
				    {
					strcpy(mbuf, "\007ov");
					chop = CHFL_UNIQOP|CHFL_CHANOP| \
					  CHFL_VOICE;
					name = target + 3;
				    }
				else
				    {
					strcpy(mbuf, "\007o");
					chop = CHFL_UNIQOP|CHFL_CHANOP;
					name = target + 2;
				    }
			    }
			else
			    {
				if (*(target+1) == '+')
				    {
					strcpy(mbuf, "\007ov");
					chop = CHFL_CHANOP|CHFL_VOICE;
					name = target+2;
				    }
				else
				    {
					strcpy(mbuf, "\007o");
					chop = CHFL_CHANOP;
					name = target+1;
				    }
			    }
		    }
		else if (*target == '+')
		    {
			strcpy(mbuf, "\007v");
			chop = CHFL_VOICE;
			name = target+1;
		    }
		else
			name = target;
		/* find user */
		if (!(acptr = find_person(name, NULL)) &&
			!(acptr = find_uid(name, NULL)))
			continue;
		/* is user who we think? */
		if (acptr->from != cptr)
			continue;
		/* get channel pointer */
		if (!chptr)
		    {
			if (check_channelmask(sptr, cptr, parv[1]) == -1)
			    {
				sendto_flag(SCH_DEBUG,
					    "received NJOIN for %s from %s",
					    parv[1],
					    get_client_name(cptr, TRUE));
				return 0;
			    }
			chptr = get_channel(acptr, parv[1], CREATE);
			if (!IsChannelName(parv[1]) || chptr == NULL)
			    {
				sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
							 ME, BadTo(parv[0]), parv[1]);
				return 0;
			    }
		    }
		/* make sure user isn't already on channel */
		if (IsMember(acptr, chptr))
		    {
			sendto_flag(SCH_ERROR, "NJOIN protocol error from %s",
				    get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :NJOIN protocol error");
			continue;
		    }
		/* add user to channel */
		add_user_to_channel(chptr, acptr, UseModes(parv[1]) ? chop :0);

		/* build buffer for NJOIN and UID capable servers */

		if (q != nbuf)
		{
			*q++ = ',';
			*u++ = ',';
		}

		/* Copy the modes. */
		for (; target < name; target++)
		{
			*q++ = *target;
			*u++ = *target;
		}

		/* For 2.10 server. */
		target = acptr->name;
		while (*target)
		{
			*q++ = *target++;
		}

		/* For 2.11 servers. */
		target = HasUID(acptr) ? acptr->name : acptr->user->uid;
		while (*target)
		{
			*u++ = *target++;
		}

		/* send join to local users on channel */
		sendto_channel_butserv(chptr, acptr, ":%s JOIN %s", acptr->name,
				       parv[1]);
		/* build MODE for local users on channel, eventually send it */
		if (*mbuf)
		    {
			if (!UseModes(parv[1]))
			    {
				sendto_one(cptr, replies[ERR_NOCHANMODES],
							 ME, BadTo(parv[0]), parv[1]);
				continue;
			    }
			switch (cnt)
			    {
			case 0:
				*parabuf = '\0'; *modebuf = '\0';
				/* fall through */
			case 1:
				strcat(modebuf, mbuf+1);
				cnt += strlen(mbuf+1);
				if (*parabuf)
				    {
					strcat(parabuf, " ");
				    }
				strcat(parabuf, acptr->name);
				if (mbuf[2])
				    {
					strcat(parabuf, " ");
					strcat(parabuf, acptr->name);
				    }
				break;
			case 2:
				sendto_channel_butserv(chptr, &me,
					       ":%s MODE %s +%s%c %s %s",
						       sptr->name, parv[1],
						       modebuf, mbuf[1],
						       parabuf, acptr->name);
				if (mbuf[2])
				    {
					strcpy(modebuf, mbuf+2);
					strcpy(parabuf, acptr->name);
					cnt = 1;
				    }
				else
					cnt = 0;
				break;
			    }
			if (cnt == MAXMODEPARAMS)
			    {
				sendto_channel_butserv(chptr, &me,
						       ":%s MODE %s +%s %s",
						       sptr->name, parv[1],
						       modebuf, parabuf);
				cnt = 0;
			    }
		    }
	    }
	/* send eventual MODE leftover */
	if (cnt)
		sendto_channel_butserv(chptr, &me, ":%s MODE %s +%s %s",
				       sptr->name, parv[1], modebuf, parabuf);

	/* send NJOIN */
	*q = '\0';
	*u = '\0';
	if (nbuf[0])
	{
		sendto_match_servs_notv(chptr, cptr, SV_UID, ":%s NJOIN %s :%s",
				     parv[0], parv[1], nbuf);
		sendto_match_servs_v(chptr, cptr, SV_UID, ":%s NJOIN %s :%s",
				     parv[0], parv[1], uidbuf);
	}

	return 0;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	Reg	aChannel *chptr;
	char	*p = NULL, *name, *comment = "";

	if (parc < 2 || parv[1][0] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]), "PART");
		return 1;
	    }

	*buf = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		    {
			if (MyPerson(sptr))
				sendto_one(sptr,
					   replies[ERR_NOSUCHCHANNEL], ME, BadTo(parv[0]),
					   name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr, replies[ERR_NOTONCHANNEL], ME, BadTo(parv[0]),
				   name);
			continue;
		    }
		comment = (BadPtr(parv[2])) ? parv[0] : parv[2];
		if (IsAnonymous(chptr) && (comment == parv[0]))
			comment = "None";
		if (strlen(comment) > (size_t) TOPICLEN)
			comment[TOPICLEN] = '\0';

		/*
		**  Remove user from the old channel (if any)
		*/
		if (!index(name, ':') && (*chptr->chname != '!'))
		    {	/* channel:*.mask */
			if (*name != '&')
			    {
				if (*buf)
					(void)strcat(buf, ",");
				(void)strcat(buf, name);
			    }
		    }
		else
			sendto_match_servs(chptr, cptr, PartFmt,
				   	   parv[0], name, comment);
		sendto_channel_butserv(chptr, sptr, PartFmt,
				       parv[0], name, comment);
		remove_user_from_channel(sptr, chptr);
	    }
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, parv[0], buf, comment);
	return 4;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int	m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *who;
	aChannel *chptr;
	int	chasing = 0, penalty = 0;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
	char	*tmp, *tmp2;
	char	*sender;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]), "KICK");
		return 1;
	    }
	if (IsServer(sptr))
		sendto_flag(SCH_NOTICE, "KICK from %s for %s %s",
			    parv[0], parv[1], parv[2]);
	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';

	if (IsServer(sptr))
	{
		sender = sptr->serv->sid;
	}
	else if (HasUID(sptr))
	{
		sender = sptr->user->uid;
	}
	else
	{
		sender = sptr->name;
	}

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (penalty >= MAXPENALTY && MyPerson(sptr))
			break;
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		    {
			if (MyPerson(sptr))
				sendto_one(sptr,
					   replies[ERR_NOSUCHCHANNEL], ME, BadTo(parv[0]),
				   	   name);
			penalty += 2;
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
                if (!UseModes(name))
                    {
                        sendto_one(sptr, replies[ERR_NOCHANMODES], ME, BadTo(parv[0]),
				   name);
			penalty += 2;
			continue;
		    }
		if (!IsServer(sptr) && !is_chan_op(sptr, chptr))
		    {
			if (!IsMember(sptr, chptr))
				sendto_one(sptr, replies[ERR_NOTONCHANNEL],
					    ME, BadTo(parv[0]), chptr->chname);
			else
				sendto_one(sptr, replies[ERR_CHANOPRIVSNEEDED],
					    ME, BadTo(parv[0]), chptr->chname);
			penalty += 2;
			continue;
		    }

		tmp = mystrdup(parv[2]);
		for (tmp2 = tmp; (user = strtoken(&p2, tmp2, ",")); tmp2 = NULL)
		    {
			penalty++;
			if (!(IsServer(cptr) && (who = find_uid(user, NULL))) &&
				!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
			if (IsMember(who, chptr))
			    {
				/* Local clients. */
				sendto_channel_butserv(chptr, sptr,
					":%s KICK %s %s :%s", sptr->name,
					name, who->name, comment);
				/* 2.11 servers. */
				sendto_match_servs_v(chptr, cptr, SV_UID,
					":%s KICK %s %s :%s", sender, name,
					HasUID(who) ? who->user->uid : 
					who->name, comment);
				/* 2.10 servers. */
				sendto_match_servs_notv(chptr, cptr, SV_UID,
					":%s KICK %s %s :%s", sptr->name, name,
					 who->name, comment);

				remove_user_from_channel(who,chptr);
				penalty += 2;
				if (penalty >= MAXPENALTY && MyPerson(sptr))
					break;
			    }
			else
				sendto_one(sptr,
					   replies[ERR_USERNOTINCHANNEL],
					   ME, BadTo(parv[0]), user, name);
		    } /* loop on parv[2] */
		MyFree(tmp);
	    } /* loop on parv[1] */

	return penalty;
}

int	count_channels(sptr)
aClient	*sptr;
{
Reg	aChannel	*chptr;
	Reg	int	count = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (chptr->users) /* don't count channels in history */
#ifdef	SHOW_INVISIBLE_LUSERS
			if (SecretChannel(chptr))
			    {
				if (IsAnOper(sptr))
					count++;
			    }
			else
#endif
				count++;
	    }
	return (count);
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = channels list
**	parv[2] = topic text
*/
int	m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aChannel *chptr = NullChn;
	char	*topic = NULL, *name, *p = NULL;
	int	penalty = 1;
	
	if (parc < 2)
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]),
			   "TOPIC");
		return 1;
	    }

	parv[1] = canonize(parv[1]);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (!UseModes(name))
		    {
			sendto_one(sptr, replies[ERR_NOCHANMODES], ME, BadTo(parv[0]),
				   name);
			continue;
		    }
		if (parc > 1 && IsChannelName(name))
		    {
			chptr = find_channel(name, NullChn);
			if (!chptr || !IsMember(sptr, chptr))
			    {
				sendto_one(sptr, replies[ERR_NOTONCHANNEL],
					   ME, BadTo(parv[0]), name);
				continue;
			    }
			if (parc > 2)
				topic = parv[2];
		    }

		if (!chptr)
		    {
			sendto_one(sptr, replies[RPL_NOTOPIC], ME, BadTo(parv[0]), name);
			return penalty;
		    }

		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (!topic)  /* only asking  for topic  */
		    {
			if (chptr->topic[0] == '\0')
				sendto_one(sptr, replies[RPL_NOTOPIC], ME, BadTo(parv[0]),
					   chptr->chname);
			else
				sendto_one(sptr, replies[RPL_TOPIC], ME, BadTo(parv[0]),
					   chptr->chname, chptr->topic);
		    } 
		else if ((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
			 is_chan_op(sptr, chptr))
		    {	/* setting a topic */
			strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
			sendto_match_servs(chptr, cptr,":%s TOPIC %s :%s",
					   parv[0], chptr->chname,
					   chptr->topic);
			sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s",
					       parv[0],
					       chptr->chname, chptr->topic);
#ifdef USE_SERVICES
			check_services_butone(SERVICE_WANT_TOPIC,
					      NULL, sptr, ":%s TOPIC %s :%s",
					      parv[0], chptr->chname, 
					      chptr->topic);
#endif
			penalty += 2;
		    }
		else
		      sendto_one(sptr, replies[ERR_CHANOPRIVSNEEDED], ME, BadTo(parv[0]),
				 chptr->chname);
	    }
	return penalty;
    }

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int	m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	aChannel *chptr;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NEEDMOREPARAMS], ME, BadTo(parv[0]),
			   "INVITE");
		return 1;
	    }

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, replies[ERR_NOSUCHNICK], ME, BadTo(parv[0]), parv[1]);
		return 1;
	    }
	clean_channelname(parv[2]);
	if (check_channelmask(sptr, acptr->user->servp->bcptr, parv[2]))
		return 1;
	if (*parv[2] == '&' && !MyClient(acptr))
		return 1;
	chptr = find_channel(parv[2], NullChn);

	if (!chptr)
	    {
		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
				  parv[0], parv[1], parv[2]);
		if (MyConnect(sptr))
		    {
        	        sendto_one(sptr, replies[RPL_INVITING], ME, BadTo(parv[0]),
	                           acptr->name, parv[2]);
			if (acptr->user->flags & FLAGS_AWAY)
				sendto_one(sptr, replies[RPL_AWAY], ME, BadTo(parv[0]),
					   acptr->name, (acptr->user->away) ? 
					   acptr->user->away : "Gone");
		    }
		return 3;
	    }

	if (!IsMember(sptr, chptr))
	    {
		sendto_one(sptr, replies[ERR_NOTONCHANNEL], ME, BadTo(parv[0]), parv[2]);
		return 1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr, replies[ERR_USERONCHANNEL], ME, BadTo(parv[0]),
			   parv[1], parv[2]);
		return 1;
	    }

	if ((chptr->mode.mode & MODE_INVITEONLY) &&  !is_chan_op(sptr, chptr))
	    {
		sendto_one(sptr, replies[ERR_CHANOPRIVSNEEDED],
			   ME, BadTo(parv[0]), chptr->chname);
		return 1;
	    }

	if (MyConnect(sptr))
	    {
		sendto_one(sptr, replies[RPL_INVITING], ME, BadTo(parv[0]),
			   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->flags & FLAGS_AWAY)
			sendto_one(sptr, replies[RPL_AWAY], ME, BadTo(parv[0]),
				   acptr->name,
				   (acptr->user->away) ? acptr->user->away :
				   "Gone");
	    }

	if (MyConnect(acptr))
		if (chptr && /* (chptr->mode.mode & MODE_INVITEONLY) && */
		    sptr->user && is_chan_op(sptr, chptr))
			add_invite(acptr, chptr);

	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 2;
}


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int	m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aChannel *chptr;
	char	*name, *p = NULL;
	int	rlen = 0;

	if (parc > 2 &&
	    hunt_server(cptr, sptr, ":%s LIST %s %s", 2, parc, parv))
		return 10;
	if (BadPtr(parv[1]))
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (!sptr->user ||
			    !chptr->users ||	/* empty locked channel */
			    (SecretChannel(chptr) && !IsMember(sptr, chptr)))
				continue;
			name = ShowChannel(sptr, chptr) ? chptr->chname : NULL;
			rlen += sendto_one(sptr, replies[RPL_LIST], ME, BadTo(parv[0]),
				   name ? name : "*", chptr->users,
				   name ? chptr->topic : "");
			if (!MyConnect(sptr) && rlen > CHREPLLEN)
				break;
		    }
	else {
		parv[1] = canonize(parv[1]);
		for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
		    {
			chptr = find_channel(name, NullChn);
			if (chptr && ShowChannel(sptr, chptr) && sptr->user)
			    {
				rlen += sendto_one(sptr, replies[RPL_LIST],
						   ME, BadTo(parv[0]), name,
						   chptr->users, chptr->topic);
				if (!MyConnect(sptr) && rlen > CHREPLLEN)
					break;
			    }
			if (*name == '!')
			    {
				chptr = NULL;
				while (chptr=hash_find_channels(name+1, chptr))
				    {
					int scr = SecretChannel(chptr) &&
							!IsMember(sptr, chptr);
					rlen += sendto_one(sptr,
							   replies[RPL_LIST],
								   ME, BadTo(parv[0]),
							   chptr->chname,
							   (scr) ? -1 :
							   chptr->users,
							   (scr) ? "" :
							   chptr->topic);
					if (!MyConnect(sptr) &&
					    rlen > CHREPLLEN)
						break;
				    }		
			    }
		     }
	}
	if (!MyConnect(sptr) && rlen > CHREPLLEN)
		sendto_one(sptr, replies[ERR_TOOMANYMATCHES], ME, BadTo(parv[0]),
			   !BadPtr(parv[1]) ? parv[1] : "*");
	sendto_one(sptr, replies[RPL_LISTEND], ME, BadTo(parv[0]));
	return 2;
    }
/*
 * names_channel - send NAMES for one specific channel
 * sends RPL_ENDOFNAMES when sendeon > 0
 */
static void names_channel(aClient *cptr, aClient *sptr, char *to,
			  aChannel *chptr, int sendeon)
{
	Reg 	Link	*lp;
	Reg	aClient	*acptr;
	int 	pxlen, ismember, nlen, maxlen;
	char 	*pbuf = buf;
	int	showusers = 1, sent = 0;
	
	if (!chptr->users)     /* channel in ND */
	{
		showusers = 0;
	}
	else
	{
		ismember = (lp = find_channel_link(sptr->user->channel, chptr))
			   ? 1 : 0;
	}
	
	if (SecretChannel(chptr))
	{
		if (!ismember)
		{
			showusers = 0;
		}
		else
		{
			*pbuf++ = '@';
		}
	}
	else if (HiddenChannel(chptr))
	{
		*pbuf++ = '*';
	}
	else
	{
		*pbuf++ = '=';
	}
	
	if (showusers)
	{
		*pbuf++ = ' ';
		pxlen = strlen(chptr->chname);
		memcpy(pbuf, chptr->chname, pxlen);
		pbuf += pxlen;
		*pbuf++ = ' ';
		*pbuf++ = ':';
		*pbuf = '\0';
		pxlen += 4;  /* ' ' + ' ' + ':' + '\0' */
		
		if (IsAnonymous(chptr))
		{
			if (ismember)
			{
				if (lp->flags & CHFL_CHANOP)
				{
					*pbuf++ = '@';
				}
				else if (lp->flags & CHFL_VOICE)
				{
					*pbuf++ = '+';
				}
				strcpy(pbuf,to);
			}
			sendto_one(sptr, replies[RPL_NAMREPLY], ME, BadTo(to),
					buf);
		}
		else
		{
			/* server names + : : + spaces + "353" + nick length
			 * +\r\n */
			maxlen = BUFSIZE
				 - 1  		/* : */
				 - strlen(ME)
				 - 5 		/* " 353 " */
				 - strlen(to)
				 - 1		/* " " */
				 - pxlen	/* "= #chan :" */
				 - 2;		/* \r\n  */
			
			for (lp = chptr->members; lp; lp = lp->next)
			{
				acptr = lp->value.cptr;
				
				nlen = strlen(acptr->name);
				/* This check is needed for server channels
				 * when someone removes +a mode from them.
				 * (server is member of such channel).
				 */
				if (nlen > NICKLEN)
				{
					continue;
				}
				
				/* Exceeded allowed length.  */
				if (((size_t) pbuf - (size_t) buf) + nlen
				   >= maxlen)
				{
					*pbuf = '\0';
					sendto_one(sptr,
						replies[RPL_NAMREPLY],ME ,
						BadTo(to) , buf);
					pbuf = buf + pxlen; /* save prefix
							       for another
							       iteration */
					pbuf[0] = '\0';
				}

				if (!ismember && IsInvisible(acptr))
				{
					continue;
				}
				if (lp->flags & CHFL_CHANOP)
				{
					*pbuf++ = '@';
				}
				else if (lp->flags & CHFL_VOICE)
				{
					*pbuf++ = '+';
				}
	
				memcpy(pbuf, acptr->name, nlen);
				pbuf += nlen;
				*pbuf++ = ' ';
			}

			*pbuf = '\0';
			sendto_one(sptr, replies[RPL_NAMREPLY], ME,
				BadTo(to), buf);
		}
	}
	if (sendeon)
	{
		sendto_one(sptr, replies[RPL_ENDOFNAMES],ME ,BadTo(to),
			   chptr->chname);
	}
	return;
	
}


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 * 	       Rewritten by jv 27 Apr 2001
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aChannel *chptr;
	Reg	aClient *acptr;
	Reg	Link	*lp;
	int	maxlen ,pxlen,nlen,cansend = 0, sent = 1;
	char	*para = parc > 1 ? parv[1] : NULL,*name, *p = NULL, *pbuf = buf;
	
	if (parc > 2 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
	{
		return MAXPENALTY;
	}
	
	if (!BadPtr(para))
	{
		for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
		{
			clean_channelname(name);
			if BadPtr(name)
			{
				continue;
			}
			chptr = find_channel(name, NULL);
			if (chptr)
			{
				names_channel(cptr, sptr, parv[0], chptr, 1);
			}
			else
			{
				sendto_one(sptr, replies[RPL_ENDOFNAMES],ME ,
				parv[0], name);
			}
			sent++;
			if (!MyConnect(sptr) || sent > MAXCHANNELSPERUSER)
			{
				break;
			}
		}
		return sent ?
		   (int) ((float) MAXCHANNELSPERUSER / (float) MAXPENALTY) : 2;
	}
	/* Client wants all nicks/channels which is seriously cpu intensive
	 * Allowed for local clients only.
	 * First, list all secret channels user is on
	 */
	for (lp = sptr->user->channel; lp; lp = lp->next)
	{
		chptr = lp->value.chptr;
		if (SecretChannel(chptr))
		{
			names_channel(cptr, sptr, parv[0], chptr, 0);
		}
	}
	
	/* Second, list all non-secret channels */
	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		if (!chptr->users || /* channel in CD */
			SecretChannel(chptr))
		{
			continue;
		}
		names_channel(cptr, sptr, parv[0], chptr, 0);
	}
	/* Third, list all remaining users
	 * ie, those which aren't on any channel, or are at Anonymous one
	 */
	strcpy(pbuf, "* * :");
	pxlen = 5;
	pbuf += pxlen;
	maxlen = BUFSIZE
		 - 1  		/* : */
		 - strlen(ME)
		 - 5 		/* " 353 " */
		 - strlen(parv[0])
		 - 1		/* " " */
		 - pxlen	/* "* * :" */
		 - 2;		/* \r\n  */


	for (acptr = client; acptr ;acptr = acptr->next)
	{
		if (!IsPerson(acptr) || IsInvisible(acptr))
		{
			continue;
		}
		
		lp = acptr->user->channel;
		cansend = 1;
		while (lp)
		{
			chptr = lp->value.chptr;
			if (PubChannel(chptr) || SecretChannel(chptr)
			    || IsMember(sptr, chptr))
			{   /* already shown */
				cansend = 0;
				break;
			}
			lp = lp->next;
		}
		if (!cansend)
		{
			continue;
		}
		nlen = strlen(acptr->name);
		if (nlen > NICKLEN)
		{
			continue;
		}
		
		if (((size_t) pbuf - (size_t) buf) + nlen >= maxlen)
		{
			*pbuf = '\0';
			sendto_one(sptr, replies[RPL_NAMREPLY], ME, parv[0],
				   buf);
			sent = 1;
			pbuf = buf + pxlen;
			pbuf[0] = '\0';
		}
	
		memcpy(pbuf, acptr->name, nlen);
		pbuf += nlen;
		*pbuf++ = ' ';
		sent = 0;
	}
	
	*pbuf = '\0';
	sendto_one(sptr, replies[RPL_NAMREPLY], ME, parv[0], buf);
	sendto_one(sptr, replies[RPL_ENDOFNAMES], ME, parv[0], "*");
	return MAXPENALTY;
}

#define CHECKFREQ	300
/* consider reoping an opless !channel */
static int
reop_channel(now, chptr)
time_t now;
aChannel *chptr;
{
    Link *lp, op;

    op.value.chptr = NULL;
    if (chptr->users <= 5 && (now - chptr->history > DELAYCHASETIMELIMIT))
	{
	    /* few users, no recent split: this is really a small channel */
	    char mbuf[MAXMODEPARAMS + 1], nbuf[MAXMODEPARAMS*(NICKLEN+1)+1];
	    int cnt;
	    
		mbuf[0] = nbuf[0] = '\0';
	    lp = chptr->members;
	    while (lp)
		{
		    if (lp->flags & CHFL_CHANOP)
			{
			    chptr->reop = 0;
			    return 0;
			}
		    if (MyConnect(lp->value.cptr) && !IsRestricted(lp->value.cptr))
			    op.value.cptr = lp->value.cptr;
		    lp = lp->next;
		}
	    if (op.value.cptr == NULL &&
		((now - chptr->reop) < LDELAYCHASETIMELIMIT))
		    /*
		    ** do nothing if no unrestricted local users, 
		    ** unless the reop is really overdue.
		    */
		    return 0;
	    sendto_channel_butone(&me, &me, chptr,
			   ":%s NOTICE %s :Enforcing channel mode +r (%d)",
				   ME, chptr->chname, now - chptr->reop);
	    op.flags = MODE_ADD|MODE_CHANOP;
	    lp = chptr->members;
	    cnt = 0;
	    while (lp)
		{
		    if (cnt == MAXMODEPARAMS)
			{
			    mbuf[cnt] = '\0';
			    if (lp != chptr->members)
				{
				    sendto_match_servs(chptr, NULL,
							 ":%s MODE %s +%s %s",
							 ME, chptr->chname,
							 mbuf, nbuf);
				    sendto_channel_butserv(chptr, &me,
						   ":%s MODE %s +%s %s",
							   ME, chptr->chname,
							   mbuf, nbuf);
				}
			    cnt = 0;
			    mbuf[0] = nbuf[0] = '\0';
			}
		    if (!(MyConnect(lp->value.cptr) 
			&& IsRestricted(lp->value.cptr)))
			{
			    op.value.cptr = lp->value.cptr;
			    change_chan_flag(&op, chptr);
			    mbuf[cnt++] = 'o';
			    strcat(nbuf, lp->value.cptr->name);
			    strcat(nbuf, " ");
			}
		    lp = lp->next;
		}
	    if (cnt)
		{
		    mbuf[cnt] = '\0';
		    sendto_match_servs(chptr, NULL,
					 ":%s MODE %s +%s %s",
					 ME, chptr->chname, mbuf, nbuf);
		    sendto_channel_butserv(chptr, &me, ":%s MODE %s +%s %s",
					   ME, chptr->chname, mbuf, nbuf);
		}
	}
    else
	{
	    time_t idlelimit = now - 
		    MIN((LDELAYCHASETIMELIMIT/2), (2*CHECKFREQ));

	    lp = chptr->members;
	    while (lp)
		{
		    if (lp->flags & CHFL_CHANOP)
			{
			    chptr->reop = 0;
			    return 0;
			}
		    if (MyConnect(lp->value.cptr) &&
			!IsRestricted(lp->value.cptr) &&
			lp->value.cptr->user->last > idlelimit &&
			(op.value.cptr == NULL ||
			 lp->value.cptr->user->last>op.value.cptr->user->last))
			    op.value.cptr = lp->value.cptr;
		    lp = lp->next;
		}
	    if (op.value.cptr == NULL)
		    return 0;
	    sendto_channel_butone(&me, &me, chptr,
			   ":%s NOTICE %s :Enforcing channel mode +r (%d)", ME,
					   chptr->chname, now - chptr->reop);
	    op.flags = MODE_ADD|MODE_CHANOP;
	    change_chan_flag(&op, chptr);
	    sendto_match_servs(chptr, NULL, ":%s MODE %s +o %s",
				 ME, chptr->chname, op.value.cptr->name);
	    sendto_channel_butserv(chptr, &me, ":%s MODE %s +o %s",
				   ME, chptr->chname, op.value.cptr->name);
	}
    chptr->reop = 0;
    return 1;
}

/*
 * Cleanup locked channels, run frequently.
 *
 * A channel life is defined by its users and the history stamp.
 * It is alive if one of the following is true:
 *	chptr->users > 0		(normal state)
 *	chptr->history >= time(NULL)	(eventually locked)
 * It is locked if empty but alive.
 *
 * The history stamp is set when a remote user with channel op exits.
 */
time_t	collect_channel_garbage(now)
time_t	now;
{
	static	u_int	max_nb = 0; /* maximum of live channels */
	static	u_char	split = 0;
	Reg	aChannel *chptr = channel;
	Reg	u_int	cur_nb = 1, curh_nb = 0, r_cnt = 0;
	aChannel *del_ch;
#ifdef DEBUGMODE
	u_int	del = istat.is_hchan;
#endif
#define SPLITBONUS	(CHECKFREQ - 50)

	collect_chid();

	while (chptr)
	    {
		if (chptr->users == 0)
			curh_nb++;
		else
		    {
			cur_nb++;
			if (*chptr->chname == '!' &&
			    (chptr->mode.mode & MODE_REOP) &&
			    chptr->reop && chptr->reop <= now)
				r_cnt += reop_channel(now, chptr);
		    }
		chptr = chptr->nextch;
	    }
	if (cur_nb > max_nb)
		max_nb = cur_nb;

	if (r_cnt)
		sendto_flag(SCH_CHAN, "Re-opped %u channel(s).", r_cnt);

	/*
	** check for huge netsplits, if so, garbage collection is not really
	** done but make sure there aren't too many channels kept for
	** history - krys
	*/
	if ((2*curh_nb > cur_nb) && curh_nb < max_nb)
		split = 1;
	else
	    {
		split = 0;
		/* no empty channel? let's skip the while! */
		if (curh_nb == 0)
		    {
#ifdef	DEBUGMODE
			sendto_flag(SCH_LOCAL,
		       "Channel garbage: live %u (max %u), hist %u (extended)",
				    cur_nb - 1, max_nb - 1, curh_nb);
#endif
			/* Check again after CHECKFREQ seconds */
			return (time_t) (now + CHECKFREQ);
		    }
	    }

	chptr = channel;
	while (chptr)
	    {
		/*
		** In case we are likely to be split, extend channel locking.
		** most splits should be short, but reality seems to prove some
		** aren't.
		*/
		if (!chptr->history)
		    {
			chptr = chptr->nextch;
			continue;
		    }
		if (split)	/* net splitted recently and we have a lock */
			chptr->history += SPLITBONUS; /* extend lock */

		if ((chptr->users == 0) && (chptr->history <= now))
		    {
			del_ch = chptr;

			chptr = del_ch->nextch;
			free_channel(del_ch);
		    }
		else
			chptr = chptr->nextch;
	    }

#ifdef	DEBUGMODE
	sendto_flag(SCH_LOCAL,
		   "Channel garbage: live %u (max %u), hist %u (removed %u)%s",
		    cur_nb - 1, max_nb - 1, curh_nb, del - istat.is_hchan,
		    (split) ? " split detected" : "");
#endif
	/* Check again after CHECKFREQ seconds */
	return (time_t) (now + CHECKFREQ);
}
