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
static	char rcsid[] = "@(#)$Id: channel.c,v 1.2 1997/04/14 15:04:15 kalt Exp $";
#endif

#include "struct.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "h.h"

#ifdef EPATH
#define m_names n_names
#define m_list n_list
#define m_join n_join
#define m_mode n_mode
#endif

aChannel *channel = NullChn;

static	void	add_invite __P((aClient *, aChannel *));
static	int	add_banid __P((aClient *, aChannel *, char *));
static	int	can_join __P((aClient *, aChannel *, char *));
static	void	channel_modes __P((aClient *, char *, char *, aChannel *));
static	int	check_channelmask __P((aClient *, aClient *, char *));
static	int	del_banid __P((aChannel *, char *));
static	aChannel *get_channel __P((aClient *, char *, int));
static	Link	*is_banned __P((aClient *, aChannel *));
static	int	set_mode __P((aClient *, aClient *, aChannel *, int *, int,\
				char **, char *,char *));
static	void	sub1_from_channel __P((aChannel *));

void	clean_channelname __P((char *));
void	del_invite __P((aClient *, aChannel *));

static	char	*PartFmt = ":%s PART %s :%s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];

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
		sendto_one(sptr, err_str(ERR_NOSUCHNICK, sptr->name), user);
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
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static	int	add_banid(cptr, chptr, banid)
aClient	*cptr;
aChannel *chptr;
char	*banid;
{
	Reg	Link	*ban;
	Reg	int	cnt = 0, len = 0;

	if (MyClient(cptr))
		(void) collapse(banid);
	for (ban = chptr->banlist; ban; ban = ban->next)
	    {
		len += strlen(ban->value.cp);
		if (MyClient(cptr) &&
		    ((len > MAXBANLENGTH) || (++cnt >= MAXBANS) ||
		     !match(ban->value.cp, banid) ||
		     !match(banid, ban->value.cp)))
			return -1;
		else if (!mycmp(ban->value.cp, banid))
			return -1;
		
	    }
	ban = make_link();
	istat.is_bans++;
	bzero((char *)ban, sizeof(Link));
	ban->flags = CHFL_BAN;
	ban->next = chptr->banlist;
	ban->value.cp = (char *)MyMalloc(len = strlen(banid)+1);
	istat.is_banmem += len;
	(void)strcpy(ban->value.cp, banid);
	chptr->banlist = ban;
	return 0;
}

/*
 * del_banid - delete an id belonging to cptr
 * if banid is null, deleteall banids belonging to cptr.
 */
static	int	del_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Reg	Link	**ban;
	Reg	Link	*tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->value.cp)==0)
		    {
			tmp = *ban;
			*ban = tmp->next;
			istat.is_banmem -= (strlen(banid) + 1);
			istat.is_bans--;
			MyFree(tmp->value.cp);
			free_link(tmp);
			break;
		    }
	return 0;
}

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
static	Link	*is_banned(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*tmp;
	char	*s;

	if (!IsPerson(cptr))
		return NULL;

	s = make_nick_user_host(cptr->name, cptr->user->username,
				  cptr->user->host);

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if (match(tmp->value.cp, s) == 0)
			break;
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
		if (chptr->users++ == 0 && chptr->history)
		    {
			/* Locked channel */
			istat.is_chan++;
			istat.is_chanmem += sz;
			istat.is_hchan--;
			istat.is_hchanmem -= sz;
			/*
			** The modes had been kept, but now someone is joining,
			** they should be reset to avoid desynchs
			** (you wouldn't want to join a +i channel, either)
			**
			** This can be wrong in some cases such as a netjoin
			** which will not complete, or on a mixed net (with
			** servers that don't do channel delay) - krys
			*/
			bzero((char *)&chptr->mode, sizeof(Mode));
#ifdef USE_SERVICES
			check_services_butone(SERVICE_WANT_CHANNEL,
					      NULL, &me, "CHANNEL %s %d",
					      chptr->chname, chptr->users);
#endif
		    }

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
#ifdef NPATH            
                note_join(who, chptr);
#endif
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
	if (--chptr->users <= 0)
		sub1_from_channel(chptr);
#ifdef USE_SERVICES
	check_services_butone(SERVICE_WANT_CHANNEL, NULL, &me, "CHANNEL %s %d",
			      chptr->chname, chptr->users);
#endif
	istat.is_chanusers--;
#ifdef NPATH            
        note_leave(sptr, chptr);
#endif
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
		tmp->flags &= ~lp->flags & MODE_FLAGS;
	/*
	 * and make sure client membership mirrors channel
	 */
	tmp = find_user_link(cptr->user->channel, (aClient *)chptr);
	if (lp->flags & MODE_ADD)
		tmp->flags |= lp->flags & MODE_FLAGS;
	else
		tmp->flags &= ~lp->flags & MODE_FLAGS;
}

int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Reg	Link	*lp;

	if (MyConnect(cptr) && IsPerson(cptr) && IsRestricted(cptr) &&
	    *chptr->chname != '&')
		return 0;
	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_CHANOP);

	return 0;
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
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&NOTICES", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&KILLS", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&CHANNEL", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&NUMERICS", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&SERVERS", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&HASH", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
	chptr = get_channel(mp, "&LOCAL", CREATE);
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;

	setup_svchans();
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
static	void	channel_modes(cptr, mbuf, pbuf, chptr)
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
	if (*parabuf)	/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	    {
		if (!(lp->flags & mask))
			continue;
		if (mask == CHFL_BAN)
			name = lp->value.cp;
		else
			name = lp->value.cptr->name;
		if (strlen(parabuf) + strlen(name) + 10 < (size_t) MODEBUFLEN)
		    {
			(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = flag;
			*cp = '\0';
		    }
		else if (*parabuf)
			send = 1;
		if (count == 3)
			send = 1;
		if (send)
		    {
			sendto_one(cptr, ":%s MODE %s %s %s",
				   ME, chname, modebuf, parabuf);
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != 3)
			    {
				(void)strcpy(parabuf, name);
				*cp++ = flag;
			    }
			count = 0;
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
  if (*chptr->chname != '#'
      || chptr->users == 0) /* channel is empty (locked), thus no mode */
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (cptr->serv->version == SV_OLD)
		send_mode_list(cptr, chptr->chname, chptr->members,
				CHFL_CHANOP, 'o');
	if (modebuf[1] || *parabuf)
		sendto_one(cptr, ":%s MODE %s %s %s",
			   ME, chptr->chname, modebuf, parabuf);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_mode_list(cptr, chptr->chname, chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
		sendto_one(cptr, ":%s MODE %s %s %s",
			   ME, chptr->chname, modebuf, parabuf);

	if (cptr->serv->version == SV_OLD)
	    {
		*parabuf = '\0';
		*modebuf = '+';
		modebuf[1] = '\0';
		send_mode_list(cptr, chptr->chname, chptr->members,
				CHFL_VOICE, 'v');
		if (modebuf[1] || *parabuf)
			sendto_one(cptr, ":%s MODE %s %s %s",
				   ME, chptr->chname, modebuf, parabuf);
	    }
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
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "MODE");
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
		if (check_channelmask(sptr, cptr, name)) {
			penalty += 1;
			continue;
		}
		if (!UseModes(name)) {
			sendto_one(sptr, err_str(ERR_NOCHANMODES, parv[0]),
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
			sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS, parv[0]),
				   name, modebuf, parabuf);
			penalty += 1;
		    }
		else	/* Check parameters for the channel */
		    {
			if(!(mcount = set_mode(cptr, sptr, chptr, &penalty,
					       parc - 2, parv + 2,
					       modebuf, parabuf)))
				continue;	/* no valid mode change */
			if ((mcount < 0) && MyConnect(sptr) && !IsServer(sptr))
			    {	/* rejected mode change */
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED,
							 parv[0]), name);
				continue;
			    }
			if (strlen(modebuf) > (size_t)1)
			    {	/* got new mode to pass on */
				if ((IsServer(cptr) && !IsServer(sptr) &&
				     !chanop) || mcount < 0)
				    {
					sendto_flag(SCH_CHAN,
						    "Fake: %s MODE %s %s %s",
						    parv[0], name, modebuf,
						    parabuf);
					ircstp->is_fake++;
				    }
				else
				    {
					sendto_channel_butserv(chptr, sptr,
						        ":%s MODE %s %s %s",
							parv[0], name,
							modebuf, parabuf);
#ifdef USE_SERVICES
					*modebuf = *parabuf = '\0';
					modebuf[1] = '\0';
					channel_modes(&me, modebuf, parabuf,
						      chptr);
				check_services_butone(SERVICE_WANT_MODE,
						      NULL, sptr,
						      "MODE %s %s",
						      parv[0], name, modebuf);
#endif
				    }
				sendto_match_servs(chptr, cptr,
						   ":%s MODE %s %s %s",
						   parv[0], name, modebuf,
						   parabuf);
			   } /* if(modebuf) */
		    } /* else(parc>2) */
	    } /* for (parv1) */
	return penalty;
}

/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client ccptr to channel chptr.  The resultant changes are printed
 * into mbuf and pbuf (if any) and applied to the channel.
 */
static	int	set_mode(cptr, sptr, chptr, penalty, parc, parv, mbuf, pbuf)
Reg	aClient *cptr, *sptr;
aChannel *chptr;
int	parc, *penalty;
char	*parv[], *mbuf, *pbuf;
{
	static	Link	chops[MAXMODEPARAMS+3];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				MODE_ANONYMOUS,  'a', 0x0, 0x0 };

	Reg	Link	*lp = NULL;
	Reg	char	*curr = parv[0], *cp = NULL;
	Reg	int	*ip;
	u_int	whatt = MODE_ADD;
	int	limitset = 0, count = 0, chasing = 0;
	int	nusers = 0, ischop, new, len, keychange = 0, opcnt = 0;
	char	fm = '\0';
	aClient *who;
	Mode	*mode, oldm;
	Link	*plp = NULL;

	*mbuf = *pbuf = '\0';
	if (parc < 1)
		return 0;

	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
	ischop = IsServer(sptr) || is_chan_op(sptr, chptr);
	new = mode->mode;

	while (curr && *curr && count >= 0)
	    {
		switch (*curr)
		{
		case '+':
			whatt = MODE_ADD;
			break;
		case '-':
			whatt = MODE_DEL;
			break;
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
			/*
			 * Check for nickname changes and try to follow these
			 * to make sure the right client is affected by the
			 * mode change.
			 */
			if (!(who = find_chasing(sptr, parv[0], &chasing)))
				break;
	  		if (!IsMember(who, chptr))
			    {
				sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL,
							 sptr->name),
					   parv[0], chptr->chname);
				break;
			    }
			if (who == cptr && whatt == MODE_ADD && *curr == 'o')
				break;

			/*
			 * to stop problems, don't allow +v and +o to mix
			 * into the one message if from a client.
			 */
			if (!fm)
				fm = *curr;
			else if (MyClient(sptr) && (*curr != fm))
				break;
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
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
			if (chasing && ischop)
				sendto_one(cptr, ":%s MODE %s %c%c %s",
					   ME, chptr->chname,
					   whatt == MODE_ADD ? '+' : '-',
					   *curr, who->name);
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
					*s++ &= 0x7f;
			}
			*parv = check_string(*parv);
			if (opcnt >= MAXMODEPARAMS)
#ifndef V29PlusOnly
			    if (MyClient(sptr) || opcnt >= MAXMODEPARAMS + 1)
#endif
				break;
			if (!fm)
				fm = *curr;
			else if (MyClient(sptr) && (*curr != fm))
				break;
			if (whatt == MODE_ADD)
			    {
				if (*mode->key && !IsServer(cptr))
					sendto_one(cptr, err_str(ERR_KEYSET,
						   cptr->name), chptr->chname);
				else if (ischop &&
				    (!*mode->key || IsServer(cptr)))
				    {
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
				if (ischop && (mycmp(mode->key, *parv) == 0 ||
				     IsServer(cptr)))
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
				for (lp = chptr->banlist; lp; lp = lp->next)
					sendto_one(cptr, rpl_str(RPL_BANLIST,
					     cptr->name),
					     chptr->chname, lp->value.cp);
				sendto_one(cptr, rpl_str(RPL_ENDOFBANLIST,
					   cptr->name), chptr->chname);
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
			sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS,
				   cptr->name), "MODE +l");
			break;
		case 'i' : /* falls through for default case */
			if (whatt == MODE_DEL)
				while ((lp = chptr->invites))
					del_invite(lp->value.cptr, chptr);
		default:
			*penalty += 1;
			for (ip = flags; *ip; ip += 2)
				if (*(ip+1) == *curr)
					break;

			if (*ip && !(*ip == MODE_ANONYMOUS && whatt == MODE_ADD
				     && !IsServer(sptr)
				     && *chptr->chname == '#'))
			    {
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
			else if (!IsServer(cptr))
				sendto_one(cptr, err_str(ERR_UNKNOWNMODE,
					   cptr->name), *curr);
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
				mode->mode |= *ip;
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
	 * Reconstruct "+bkov" chain.
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
			/*
			 * get c as the mode char and tmp as a pointer to
			 * the paramter for this mode change.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_CHANOP :
				c = 'o';
				cp = lp->value.cptr->name;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				break;
			case MODE_BAN :
				c = 'b';
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				cp = make_nick_user_host(cp, user, host);
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
				len += strlen(cp);
				(void)strcat(pbuf, " ");
				len++;
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
				len += strlen(cp);
				(void)strcat(pbuf, " ");
				len++;
				if (!ischop)
					break;
				mode->limit = nusers;
				break;
			case MODE_CHANOP :
			case MODE_VOICE :
				*mbuf++ = c;
				(void)strcat(pbuf, cp);
				len += strlen(cp);
				(void)strcat(pbuf, " ");
				len++;
				if (ischop)
					change_chan_flag(lp, chptr);
				break;
			case MODE_BAN :
				if (ischop && (((whatt & MODE_ADD) &&
					       !add_banid(sptr, chptr, cp)) ||
					       ((whatt & MODE_DEL) &&
					       !del_banid(chptr, cp))))
				    {
					*mbuf++ = c;
					(void)strcat(pbuf, cp);
					len += strlen(cp);
					(void)strcat(pbuf, " ");
					len++;
				    }
				break;
			}
		    } /* for (; i < opcnt; i++) */
	    } /* if (opcnt) */

	*mbuf++ = '\0';

	return ischop ? count : -count;
}

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
Reg	aChannel *chptr;
char	*key;
{
	Reg	Link	*lp;

	if (chptr->users == 0 && chptr->history != 0)
		return (timeofday > chptr->history) ? 0 : ERR_UNAVAILRESOURCE;
	if (is_banned(sptr, chptr))
		return (ERR_BANNEDFROMCHAN);
	if (chptr->mode.mode & MODE_INVITEONLY)
	    {
		for (lp = sptr->user->invited; lp; lp = lp->next)
			if (lp->value.chptr == chptr)
				break;
		if (!lp)
			return (ERR_INVITEONLYCHAN);
	    }

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (chptr->mode.limit && chptr->users >= chptr->mode.limit)
		return (ERR_CHANNELISFULL);

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
			sendto_one(sptr, err_str(ERR_BADCHANMASK, sptr->name),
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
		istat.is_chanmem += sizeof(aChannel) + len;
		istat.is_chan++;
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
**  The last user has left the channel, free the channel block
*/
static	void	sub1_from_channel(chptr)
Reg	aChannel *chptr;
{
	Reg	Link *tmp;
	Link	*obtmp;
	int	len = sizeof(aChannel) + strlen(chptr->chname);

	/*
	 * Now, find all invite links from channel structure
	 */
	while ((tmp = chptr->invites))
		del_invite(tmp->value.cptr, chptr);

	tmp = chptr->banlist;
	while (tmp)
	    {
		obtmp = tmp;
		tmp = tmp->next;
		istat.is_banmem -= (strlen(obtmp->value.cp) + 1);
		istat.is_bans--;
		MyFree(obtmp->value.cp);
		free_link(obtmp);
	    }

	istat.is_chan--;
	istat.is_chanmem -= len;
	if (chptr->history == 0 || timeofday >= chptr->history)
	    {	/* No lock nor expired lock, we can release the channel */
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname,chptr);
		MyFree((char *)chptr);
	    }
	else	/* Lock is active */
	    {
		/*
		** make sure adding a ban will work next time,
		** keep topic (not sent on netjoins)
		** keep modes for local use (don't send on netjoins!)
		*/
		chptr->banlist = NULL;
		istat.is_hchan++;
		istat.is_hchanmem += len;
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
	static	char	jbuf[BUFSIZE];
	Reg	Link	*lp;
	Reg	aChannel *chptr;
	Reg	char	*name, *key = NULL;
	int	i, flags = 0;
	char	*p = NULL, *p2 = NULL, *s;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "JOIN");
		return 1;
	    }

	*jbuf = '\0';
	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
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
		else if (!IsChannelName(name))
		    {
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL,
					   parv[0]), name);
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
	parv[2] = NULL;	/* for m_names call later, parv[parc] must == NULL */
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

		if (cptr->serv && cptr->serv->version != SV_OLD &&
		    (s = index(name, '\007')))
			*s++ = '\0';
		else
			clean_channelname(name), s = NULL;

		flags = 0;

		if (MyConnect(sptr) &&
		    sptr->user->joined >= MAXCHANNELSPERUSER) {
			/* Feature: Cannot join &flagchannels either
			   if already joined MAXCHANNELSPERUSER times. */
			sendto_one(sptr, err_str(ERR_TOOMANYCHANNELS,
				   parv[0]), name);
			return 2;
		}

		chptr = get_channel(sptr, name, CREATE);

		if (IsMember(sptr, chptr))
			continue;
		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		    {
			sendto_one(sptr, err_str(i, parv[0]), name);
			continue;
		    }

		/*
		** local client is first to enter previously nonexistant
		** channel so make them (rightfully) the Channel
		** Operator.
		*/
		flags = 0;
		if (MyConnect(sptr) && UseModes(name) &&
		    (!IsRestricted(sptr) || (*name == '&')) && !chptr->users)
			flags = CHFL_CHANOP;
		/*
		**  Complete user entry to the new channel (if any)
		*/
		if (s)
		    {
			if (*s == 'o')
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
		** notify all other users on the new channel
		*/
#ifndef MIRC_KLUDGE
		if (s)
			*--s = '\007';
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
						parv[0], name);
#else
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
						parv[0], name);
		if (s) {
			sendto_channel_butserv(chptr, sptr,
						":%s MODE %s +%s %s",
						cptr->name, name, s, parv[0]);
			*--s = '\007';
		}
#endif
		sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0], name);

		if (MyClient(sptr))
		    {
			del_invite(sptr, chptr);
			if (flags == CHFL_CHANOP)
				sendto_match_servs(chptr, cptr,
						   ":%s MODE %s +o %s",
						   ME, name, parv[0]);
			if (chptr->topic[0] != '\0')
				sendto_one(sptr, rpl_str(RPL_TOPIC, parv[0]),
					   name, chptr->topic);
			parv[1] = name;
			(void)m_names(cptr, sptr, 2, parv);
		    }
#ifndef NoV28Links
		else if (s && flags & (CHFL_VOICE|CHFL_CHANOP))
		    {
			*s++ = '\0';
			sendto_serv_v(cptr, SV_OLD, ":%s MODE %s +%c%c %s %s",
				      ME, name, *s, *(s+1) == 'v' ? 'v' : '+',
				      parv[0], *(s+1) == 'v' ? parv[0] : "");
		    }
#endif
	    }
	return 2;
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
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "PART");
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
					   err_str(ERR_NOSUCHCHANNEL, parv[0]),
					   name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_NOTONCHANNEL, parv[0]),
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
		if (!index(name, ':')) {	/* channel:*.mask */
			if (*name != '&')
# ifndef NoV28Links
				if (*name == '+')	/* 2.8 has no +chan */
					sendto_serv_v(cptr, SV_29, PartFmt,
						      parv[0], name, comment);
				else
# endif /* NoV28Links */
				{
					if (*buf)
						(void)strcat(buf, ",");
					(void)strcat(buf, name);
				}
		} else
			sendto_match_servs(chptr, cptr, PartFmt,
				   	   parv[0], name, comment);
		sendto_channel_butserv(chptr, sptr, PartFmt,
				       parv[0], name, comment);
		remove_user_from_channel(sptr, chptr);
	    }
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, parv[0], buf, comment);
	return 2;
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
	int	mlen, len = 0, nlen;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "KICK");
		return 1;
	    }
	if (IsServer(sptr))
		sendto_flag(SCH_NOTICE, "KICK from %s for %s %s",
			    parv[0], parv[1], parv[2]);
	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';

	*nickbuf = *buf = '\0';
	mlen = 7 + strlen(parv[0]);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		penalty++;
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		    {
			if (MyPerson(sptr))
				sendto_one(sptr,
					   err_str(ERR_NOSUCHCHANNEL, parv[0]),
				   	   name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsServer(sptr) && !is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED,
				   parv[0]), chptr->chname);
			continue;
		    }
		if (len + mlen + strlen(name) < (size_t) BUFSIZE / 2)
		    {
			if (*buf)
				(void)strcat(buf, ",");
			(void)strcat(buf, name);
			len += strlen(name) + 1;
		    }
		else
			continue;
		nlen = 0;

		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		    {
			penalty++;
			if (!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
			if (nlen + mlen + strlen(who->name) >
			    (size_t) BUFSIZE - NICKLEN)
				continue;
			if (IsMember(who, chptr))
			    {
				sendto_channel_butserv(chptr, sptr,
						":%s KICK %s %s :%s", parv[0],
						name, who->name, comment);
				/* Don't send &local &kicks out */
				if (*chptr->chname != '&') {
					if (*nickbuf)
						(void)strcat(nickbuf, ",");
					(void)strcat(nickbuf, who->name);
					nlen += strlen(who->name);
				} else
				  sendto_match_servs(chptr, cptr,
						   ":%s KICK %s %s :%s",
						   parv[0], name,
						   who->name, comment);
				remove_user_from_channel(who,chptr);
				penalty += 2;
				if (penalty > MAXPENALTY)
					break;
			    }
			else
				sendto_one(sptr,
					   err_str(ERR_USERNOTINCHANNEL,
					   parv[0]), user, name);
		    } /* loop on parv[2] */
	    } /* loop on parv[1] */

	if (*buf && *nickbuf)
		sendto_serv_butone(cptr, ":%s KICK %s %s :%s",
				   parv[0], buf, nickbuf, comment);
	return penalty;
}

int	count_channels(sptr)
aClient	*sptr;
{
Reg	aChannel	*chptr;
	Reg	int	count = 0;

	for (chptr = channel; chptr; chptr = chptr->nextch)
#ifdef	SHOW_INVISIBLE_LUSERS
		if (SecretChannel(chptr))
		    {
			if (IsAnOper(sptr))
				count++;
		    }
		else
#endif
			count++;
	return (count);
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
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
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
			   "TOPIC");
		return 1;
	    }

	parv[1] = canonize(parv[1]);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (!UseModes(name))
		    {
			sendto_one(sptr, err_str(ERR_NOCHANMODES, parv[0]),
				   name);
			continue;
		    }
		if (parc > 1 && IsChannelName(name))
		    {
			chptr = find_channel(name, NullChn);
			if (!chptr || !IsMember(sptr, chptr))
			    {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL,
					   parv[0]), name);
				continue;
			    }
			if (parc > 2)
				topic = parv[2];
		    }

		if (!chptr)
		    {
			sendto_one(sptr, rpl_str(RPL_NOTOPIC, parv[0]), name);
			return penalty;
		    }

		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (!topic)  /* only asking  for topic  */
		    {
			if (chptr->topic[0] == '\0')
				sendto_one(sptr, rpl_str(RPL_NOTOPIC, parv[0]),
					   chptr->chname);
			else
				sendto_one(sptr, rpl_str(RPL_TOPIC, parv[0]),
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
			penalty += 2;
		    }
		else
		      sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED, parv[0]),
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
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
			   "INVITE");
		return 1;
	    }

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK, parv[0]), parv[1]);
		return 1;
	    }
	clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 1;
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
				  parv[0], parv[1], parv[2]);
		return 3;
	    }

	if (chptr && !IsMember(sptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL, parv[0]), parv[2]);
		return 1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_USERONCHANNEL, parv[0]),
			   parv[1], parv[2]);
		return 1;
	    }
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	    {
		if (!is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED,
				   parv[0]), chptr->chname);
			return 1;
		    }
		else if (!IsMember(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED,
				   parv[0]),
				   ((chptr) ? (chptr->chname) : parv[2]));
			return 1;
		    }
	    }

	if (MyConnect(sptr))
	    {
		sendto_one(sptr, rpl_str(RPL_INVITING, parv[0]),
			   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY, parv[0]),
				   acptr->name, acptr->user->away);
	    }
	if (MyConnect(acptr))
		if (chptr && (chptr->mode.mode & MODE_INVITEONLY) &&
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

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s LIST %s %s", 2, parc, parv))
		return 10;
#ifdef MIRC_KLUDGE
        sendto_one(sptr, rpl_str(RPL_LISTSTART, parv[0]));
#endif
	if (BadPtr(parv[1]))
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (!sptr->user ||
			    !chptr->users ||	/* empty locked channel */
			    (SecretChannel(chptr) && !IsMember(sptr, chptr)))
				continue;
			name = ShowChannel(sptr, chptr) ? chptr->chname : NULL;
			rlen += sendto_one(sptr, rpl_str(RPL_LIST, parv[0]),
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
				rlen += sendto_one(sptr, rpl_str(RPL_LIST,
						   parv[0]), name,
						   chptr->users, chptr->topic);
				if (!MyConnect(sptr) && rlen > CHREPLLEN)
					break;
			    }
		     }
	}
	if (!MyConnect(sptr) && rlen > CHREPLLEN)
		sendto_one(sptr, err_str(ERR_TOOMANYMATCHES, parv[0]),
			   !BadPtr(parv[1]) ? parv[1] : "*");
	sendto_one(sptr, rpl_str(RPL_LISTEND, parv[0]));
	return 2;
    }


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
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
	Reg	aClient *c2ptr;
	Reg	Link	*lp;
	aChannel *ch2ptr = NULL;
	int	idx, flag, len, mlen, rlen = 0;
	char	*s, *para = parc > 1 ? parv[1] : NULL;

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
		return 10;

	mlen = strlen(ME) + 10;
	if (!BadPtr(para))
	    {
		s = index(para, ',');
		if (s && MyConnect(sptr))
		    {
			parv[1] = ++s;
			(void)m_names(cptr, sptr, parc, parv);
		    }
		clean_channelname(para);
		ch2ptr = find_channel(para, (aChannel *)NULL);
	    }

	*buf = '\0';

	/*
	 * First, do all visible channels (public and the one user self is)
	 */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if (!chptr->users ||	/* locked empty channel */
		    ((chptr != ch2ptr) && !BadPtr(para))) /* 'wrong' channel */
			continue;
		if (!MyConnect(sptr) && (BadPtr(para) || (rlen > CHREPLLEN)))
			break;
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		(void)strcpy(buf, "* ");
		len = strlen(chptr->chname);
		(void)strcpy(buf + 2, chptr->chname);
		(void)strcpy(buf + 2 + len, " :");

		if (PubChannel(chptr))
			*buf = '=';
		else if (SecretChannel(chptr))
			*buf = '@';

		if (IsAnonymous(chptr))
		    {
			if ((lp = find_user_link(chptr->members, sptr)))
			    {
				if (lp->flags & CHFL_CHANOP)
					(void)strcat(buf, "@");
				else if (lp->flags & CHFL_VOICE)
					(void)strcat(buf, "+");
				(void)strcat(buf, parv[0]);
			    }
			rlen += strlen(buf);
			sendto_one(sptr, rpl_str(RPL_NAMREPLY, parv[0]), buf);
			continue;
		    }
		idx = len + 4;
		flag = 1;
		for (lp = chptr->members; lp; lp = lp->next)
		    {
			c2ptr = lp->value.cptr;
			if (IsInvisible(c2ptr) && !IsMember(sptr,chptr))
				continue;
			if (lp->flags & CHFL_CHANOP)
			    {
				(void)strcat(buf, "@");
				idx++;
			    }
			else if (lp->flags & CHFL_VOICE)
			    {
				(void)strcat(buf, "+");
				idx++;
			    }
			(void)strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			(void)strcat(buf," ");
			if (mlen + idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, rpl_str(RPL_NAMREPLY,
					   parv[0]), buf);
				(void)strncpy(buf, "* ", 3);
				(void)strncpy(buf+2, chptr->chname,
						len + 1);
				(void)strcat(buf, " :");
				if (PubChannel(chptr))
					*buf = '=';
				else if (SecretChannel(chptr))
					*buf = '@';
				idx = len + 4;
				flag = 0;
			    }
		    }
		if (flag)
		    {
			rlen += strlen(buf);
			sendto_one(sptr, rpl_str(RPL_NAMREPLY, parv[0]), buf);
		    }
	    } /* for(channels) */
	if (!BadPtr(para))
	    {
		if (!MyConnect(sptr) && (rlen > CHREPLLEN))
			sendto_one(sptr, err_str(ERR_TOOMANYMATCHES, parv[0]),
				   para);
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES, parv[0]), para);
		return(1);
	    }

	/* Second, do all non-public, non-secret channels in one big sweep */

	(void)strncpy(buf, "* * :", 6);
	idx = 5;
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
  		aChannel *ch3ptr;
		int	showflag = 0, secret = 0;

		if (!IsPerson(c2ptr) || IsInvisible(c2ptr))
			continue;
		if (!MyConnect(sptr) && (BadPtr(para) || (rlen > CHREPLLEN)))
			break;
		lp = c2ptr->user->channel;
		/*
		 * dont show a client if they are on a secret channel or
		 * they are on a channel sptr is on since they have already
		 * been show earlier. -avalon
		 */
		while (lp)
		    {
			ch3ptr = lp->value.chptr;
			if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
				showflag = 1;
			if (SecretChannel(ch3ptr))
				secret = 1;
			lp = lp->next;
		    }
		if (showflag) /* have we already shown them ? */
			continue;
		if (secret) /* on any secret channels ? */
			continue;
		(void)strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		(void)strcat(buf," ");
		flag = 1;
		if (mlen + idx + NICKLEN > BUFSIZE - 2)
		    {
			rlen += strlen(buf);
			sendto_one(sptr, rpl_str(RPL_NAMREPLY, parv[0]), buf);
			(void)strncpy(buf, "* * :", 6);
			idx = 5;
			flag = 0;
		    }
	    }
	if (flag)
	    {
		rlen += strlen(buf);
		sendto_one(sptr, rpl_str(RPL_NAMREPLY, parv[0]), buf);
	    }
	if (!MyConnect(sptr) && rlen > CHREPLLEN)
		sendto_one(sptr, err_str(ERR_TOOMANYMATCHES, parv[0]),
			   para ? para : "*");
	/* This is broken.. remove the recursion? */
	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES, parv[0]), "*");
	return 2;
}


void	send_user_joins(cptr, user)
aClient	*cptr, *user;
{
	Reg	Link	*lp;
	Reg	aChannel *chptr;
	Reg	int	cnt = 0, len = 0, clen;
	char	 *mask;

	*buf = ':';
	(void)strcpy(buf+1, user->name);
	(void)strcat(buf, " JOIN ");
	len = strlen(user->name) + 7;

	for (lp = user->user->channel; lp; lp = lp->next)
	    {
		chptr = lp->value.chptr;
		if (*chptr->chname == '&')
			continue;
#ifndef NoV28Links
		if (*chptr->chname == '+' && cptr->serv->version == SV_OLD)
			continue;
#endif
		if ((mask = index(chptr->chname, ':')))
			if (match(++mask, cptr->name))
				continue;
		clen = strlen(chptr->chname);
		if (clen > (size_t) BUFSIZE - 7 - len)
		    {
			if (cnt)
				sendto_one(cptr, "%s", buf);
			*buf = ':';
			(void)strcpy(buf+1, user->name);
			(void)strcat(buf, " JOIN ");
			len = strlen(user->name) + 7;
			cnt = 0;
		    }
		if (cnt)
		    {
			len++;
			(void)strcat(buf, ",");
		    }
		(void)strcpy(buf + len, chptr->chname);
		len += clen;
		if (cptr->serv->version != SV_OLD &&
		    lp->flags & (CHFL_CHANOP|CHFL_VOICE))
		    {
			buf[len++] = '\007';
			if (lp->flags & CHFL_CHANOP)
				buf[len++] = 'o';
			if (lp->flags & CHFL_VOICE)
				buf[len++] = 'v';
			buf[len] = '\0';
		    }
		cnt++;
	    }
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
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
	Reg	u_int	cur_nb = 1, curh_nb = 0;
	aChannel *delch;
#ifdef DEBUGMODE
	u_int	del = istat.is_hchan;
#endif
#define CHECKFREQ	300
#define SPLITBONUS	(CHECKFREQ - 50)

	while (chptr)
	    {
		if (chptr->users == 0)
			curh_nb++;
		else
			cur_nb++;
		chptr = chptr->nextch;
	    }
	if (cur_nb > max_nb)
		max_nb = cur_nb;

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
			delch = chptr;
			if (delch->prevch)
				chptr = delch->prevch->nextch = delch->nextch;
			else
				chptr = channel = delch->nextch;
			if (delch->nextch)
				delch->nextch->prevch = delch->prevch;
			(void)del_from_channel_hash_table(delch->chname,delch);
			istat.is_hchan--;
			istat.is_hchanmem -= sizeof(aChannel) 
				+ strlen(delch->chname);
			MyFree((char *)delch);
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
