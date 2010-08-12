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
static const volatile char rcsid[] = "@(#)$Id: channel.c,v 1.279 2010/08/12 16:23:14 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define CHANNEL_C
#include "s_externs.h"
#undef CHANNEL_C

static	char	asterix[2]="*";

#define	BanLen(x)	((strlen(x->nick)+strlen(x->user)+strlen(x->host)))
#define BanMatch(x,y)	((!match(x->nick,y->nick)&&!match(x->user,y->user)&&!match(x->host,y->host)))
#define BanExact(x,y)	((!mycmp(x->nick,y->nick)&&!mycmp(x->user,y->user)&&!mycmp(x->host,y->host)))

aChannel *channel = NullChn;

static	void	add_invite (aClient *, aClient *, aChannel *);
static	int	can_join (aClient *, aChannel *, char *);
void	channel_modes (aClient *, char *, char *, aChannel *);
static	int	check_channelmask (aClient *, aClient *, char *);

#ifdef JAPANESE
static	int	jp_chname (char *);
#endif

static	aChannel *get_channel (aClient *, char *, int);
static	int	set_mode (aClient *, aClient *, aChannel *, int *, 
			int, char **);
static	void	free_channel (aChannel *);

static	int	add_modeid (int, aClient *, aChannel *, aListItem *);
static	int	del_modeid (int, aChannel *, aListItem *);
static	Link	*match_modeid (int, aClient *, aChannel *);
static  void    names_channel (aClient *,aClient *,char *,aChannel *,int);
static	void	free_bei (aListItem *bei);
static	aListItem	*make_bei (char *nick, char *user, char *host);


static	char	*PartFmt = ":%s PART %s :%s";

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], uparabuf[MODEBUFLEN];

/*
 * return the length (>=0) of a chain of links.
 */
static	int	list_length(invLink *lp)
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
static	aClient	*find_chasing(aClient *sptr, char *user, int *chasing)
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
 * Fixes a string so that the first white space found becomes an end of
 * string marker (`\0`). Returns the 'fixed' string or static "*" if the string
 * was NULL length or a NULL pointer.
 */
static	char	*check_string(char *s)
{
	char	*str = s;

	if (BadPtr(s))
		return asterix;

	for ( ;*s; s++)
		if (isspace(*s))
		    {
			*s = '\0';
			break;
		    }

	return (BadPtr(str)) ? asterix : str;
}

static	void	free_bei(aListItem *bei)
{
	if (bei->nick != asterix)
	{
		MyFree(bei->nick);
	}
	if (bei->user != asterix)
	{
		MyFree(bei->user);
	}
	if (bei->host != asterix)
	{
		MyFree(bei->host);
	}
	MyFree(bei);
}

/* Prepare and fill ListItem struct. Note: check_string takes care of
** cleaning parts, including possible use of static asterix. */
static	aListItem	*make_bei(char *nick, char *user, char *host)
{
	aListItem	*tmp;
	int	len;

	tmp = (struct ListItem *)MyMalloc(sizeof(aListItem));

	nick = check_string(nick);
	if (nick == asterix)
	{
		tmp->nick = asterix;
	}
	else
	{
		len = MIN(strlen(nick), NICKLEN) + 1;
		tmp->nick = (char *) MyMalloc(len);
		strncpyzt(tmp->nick, nick, len);
	}
	user = check_string(user);
	if (user == asterix)
	{
		tmp->user = asterix;
	}
	else
	{
		len = MIN(strlen(user), USERLEN) + 1;
		tmp->user=(char *) MyMalloc(len);
		strncpyzt(tmp->user, user, len);
	}
	host = check_string(host);
	if (host == asterix)
	{
		tmp->host = asterix;
	}
	else
	{
		len = MIN(strlen(host), HOSTLEN) + 1;
		tmp->host=(char *) MyMalloc(len);
		strncpyzt(tmp->host, host, len);
	}

	return tmp;
}

/*
 * Ban functions to work with mode +b/+e/+I
 */
/* add_modeid - add an id to the list of modes "type" for chptr
 *  (belongs to cptr)
 */

static	int	add_modeid(int type, aClient *cptr, aChannel *chptr,
			aListItem *modeid)
{
	Reg	Link	*mode;
	Reg	int	cnt = 0, len = 0;

	if (MyClient(cptr))
	{
		(void) collapse(modeid->nick);
		(void) collapse(modeid->user);
		(void) collapse(modeid->host);
	}

	for (mode = chptr->mlist; mode; mode = mode->next)
	    {
		len += BanLen(mode->value.alist);
		if (MyClient(cptr))
		    {
			if ((len > MAXBANLENGTH) || (++cnt >= MAXBANS))
			    {
				sendto_one(cptr, replies[ERR_BANLISTFULL],
					ME, BadTo(cptr->name),
					chptr->chname, modeid->nick,
					modeid->user, modeid->host);
				return -1;
			    }
			if (type == mode->flags &&
				BanExact(mode->value.alist, modeid))
			    {
				int rpl;

				if (type == CHFL_BAN)
					rpl = RPL_BANLIST;
				else if (type == CHFL_EXCEPTION)
					rpl = RPL_EXCEPTLIST;
				else if (type == CHFL_REOPLIST)
					rpl = RPL_REOPLIST;
				else
					rpl = RPL_INVITELIST;

				sendto_one(cptr, replies[rpl], ME, BadTo(cptr->name),
					chptr->chname, mode->value.alist->nick,
					mode->value.alist->user,
					mode->value.alist->host);
				return -1;
			    }
		    }
		else if (type == mode->flags && BanExact(mode->value.alist, modeid))
		{
			return -1;
		}
		
	    }
	mode = make_link();
	istat.is_bans++;
	bzero((char *)mode, sizeof(Link));
	mode->flags = type;
	mode->next = chptr->mlist;
	mode->value.alist = modeid;
	istat.is_banmem += BanLen(modeid);
	chptr->mlist = mode;
	return 0;
}

/*
 * del_modeid - delete an id belonging to chptr
 * if modeid is null, delete all ids belonging to chptr. (seems to be unused)
 */
static	int	del_modeid(int type, aChannel *chptr, aListItem *modeid)
{
	Reg	Link	**mode;
	Reg	Link	*tmp;

	/* mode == NULL rewritten inside loop */
        for (mode = &(chptr->mlist); *mode; mode = &((*mode)->next))
	{
		if (type == (*mode)->flags &&
			(modeid == NULL ? 1 : BanExact(modeid, (*mode)->value.alist)))
		{
			tmp = *mode;
			*mode = tmp->next;
			istat.is_banmem -= BanLen(tmp->value.alist);
			istat.is_bans--;
			free_bei(tmp->value.alist);
			free_link(tmp);
			break;
		}
	}
	return 0;
}

/*
 * match_modeid - returns a pointer to the mode structure if matching else NULL
 */
static	Link	*match_modeid(int type, aClient *cptr, aChannel *chptr)
{
	Reg	Link	*tmp;

	if (!IsPerson(cptr))
		return NULL;

	for (tmp = chptr->mlist; tmp; tmp = tmp->next)
	{
		if (tmp->flags == type)
		{
			if (match(tmp->value.alist->nick, cptr->name) != 0)
			{
				/* seems like no match on nick, but... */
				if (isdigit(tmp->value.alist->nick[0]))
				{
					/* ...perhaps it is UID-ban? */
					if (match(tmp->value.alist->nick, 
						cptr->user->uid) != 0)
					{
						/* no match on UID */
						continue;
					}
					/* We have match on UID!
					 * Go now for the user part */
				}
				else
				{
					/* no match on nick part */
					continue;
				}
			}
			if (match(tmp->value.alist->user, cptr->user->username) != 0)
			{
				/* no match on user part */
				continue;
			}
			/* At this point n!u of a client matches that of a beI.
			 * Proceeding to more intensive checks of hostname,
			 * IP, CIDR
			 */
			if (match(tmp->value.alist->host, cptr->user->host) == 0)
			{
				/* match */
				break;
			}
			/* if our client, let's check IP and CIDR */
			/* perhaps we could relax it and check remotes too? */
			if (MyConnect(cptr))
			{
				Link *acf = cptr->confs;

				/* scroll acf to I:line */
				if (IsAnOper(cptr))
				{
					acf = acf->next;
				/* above is faster but will fail if we introduce
				** something that will attach another conf for
				** client -- the following will have to be used:
					for (; acf; acf = acf->next)
					if (acf->value.aconf->status & CONF_CLIENT)
					break;
				*/
				}

				if (IsConfNoResolveMatch(acf->value.aconf))
				{
					/* user->host contains IP and was just
					 * checked; try sockhost, it may have
					 * hostname.
					 */
					if (match(tmp->value.alist->host,
						cptr->sockhost) == 0)
					{
						/* match */
						break;
					}
				}
				else
				/* Yay, it's 2.11, we have string ip! */
				if (match(tmp->value.alist->host, cptr->user->sip) == 0)
				{
					/* match */
					break;
				}
				/* so now we check CIDR */
				if (strchr(tmp->value.alist->host, '/') &&
					match_ipmask(tmp->value.alist->host, cptr, 0) == 0)
				{
					break;
				}
			}
		}
	}
	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static	void	add_user_to_channel(aChannel *chptr, aClient *who, int flags)
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

void	remove_user_from_channel(aClient *sptr, aChannel *chptr)
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
			if (tmp->flags & CHFL_CHANOP)
			{
				chptr->reop = timeofday + LDELAYCHASETIMELIMIT +
					myrand() % 300;
			}

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

static	void	change_chan_flag(Link *lp, aChannel *chptr)
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

int	is_chan_op(aClient *cptr, aChannel *chptr)
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

int	has_voice(aClient *cptr, aChannel *chptr)
{
	Reg	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(aClient *cptr, aChannel *chptr)
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

	/* checking +be is not reliable for remote clients for case
	** when exception is in cidr format. working around that is
	** horrible. basically inet_pton for each UNICK and keeping
	** it in client struct, plus dealing somehow with non-inet6
	** servers getting inet6 clients ips from remote servers...
	** in short it seems better to allow remote clients to just
	** talk, trusting a bit remote servers, than to reject good
	** messages. --B. */
	if (!MyConnect(cptr))
		return 0;

	if ((!lp || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE))) &&
	    !match_modeid(CHFL_EXCEPTION, cptr, chptr) &&
	    match_modeid(CHFL_BAN, cptr, chptr))
		return (MODE_BAN);

	return 0;
}

#ifdef JAPANESE
char	*get_channelmask(char *chname)
{
	char	*mask;

	mask = rindex(chname, ':');
	if (!mask || index(mask, '\033'))
	{
		/* If '\033' is in the mask, well, it's not a real mask,
		** but a JIS encoded channel name. --Beeth */
		return NULL;
	}
	return mask;
}

/* This tries to find out if given chname is JIS encoded:
** a) ":" followed somewhere by '\033'
** b) comma in chname (impossible if not for JIS)
** c) one of {, }, ~, \ between JIS marks.
**
** Returns 1 if seems JIS encoded, 0 otherwise.
*/
int	jp_chname(char *chname)
{
	char *mask, *cn;
	int flag = 0;

	if (!chname || !*chname)
		return 0;
	mask = rindex(chname, ':');
	if (mask && index(mask, '\033'))
		return 1;
	if (index(chname, ','))
		return 1;

	cn = chname;
	while (*cn)
	{
		if (cn[0] == '\033'
			&& (cn[1] == '$' || cn[1] == '(')
			&& cn[2] == 'B')
		{
			flag = (cn[1] == '$') ? 1 : 0;
			cn += 2;
		}
		else if (flag == 1 &&
			(*cn == '{' || *cn == '}' || *cn == '~' || *cn == '\\'))
		{
			return 1;
		}
		cn++; 
	}
	return 0;
}

#define IsJPFlag(x)	(((x)->flags & FLAGS_JP))
#define IsJPChan(x, y)	( ((x) && IsJPFlag((x))) || jp_chname((y)) )

/* 
** This checks for valid combination of channel name and server,
** so Japanese channels are not sent to non-Japanese servers.
**
** If cptr is NULL, then function is reduced to checking if channel name
** is (likely to be) Japanese (if it is not, it can be sent anywhere).
**
** Otherwise cptr should be a JP flagged server or not a server at all.
**
** Returns 1 if it is safe to use given combination of params or 0 if not.
**
** Note: this should be split in two functions for clarity.
*/
int	jp_valid(aClient *cptr, aChannel *chptr, char *chname)
{
	return ( !IsJPChan(chptr, chname) ||
		(cptr && (!IsServer(cptr) || IsJPFlag(cptr))) );
}
#endif

aChannel	*find_channel(char *chname, aChannel *chptr)
{
	aChannel *achptr = chptr;

	if (chname && *chname)
		achptr = hash_find_channel(chname, chptr);
	return achptr;
}

void	setup_server_channels(aClient *mp)
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
	chptr = get_channel(mp, "&WALLOPS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: wallops received");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode;
#ifdef CLIENTS_CHANNEL
	chptr = get_channel(mp, "&CLIENTS", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: clients activity");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode|MODE_SECRET|MODE_INVITEONLY;
#endif
	chptr = get_channel(mp, "&OPER", CREATE);
	strcpy(chptr->topic, "SERVER MESSAGES: for your trusted eyes only");
	add_user_to_channel(chptr, mp, CHFL_CHANOP);
	chptr->mode.mode = smode|MODE_SECRET|MODE_INVITEONLY;

	setup_svchans();
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
void	channel_modes(aClient *cptr, char *mbuf, char *pbuf, aChannel *chptr)
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
			sprintf(pbuf, "%d ", chptr->mode.limit);
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

static	void	send_mode_list(aClient *cptr, char *chname, Link *top, 
			int mask, char flag)
{
	Reg	Link	*lp;
	Reg	char	*cp, *name;
	int	count = 0, send = 0;
	char	tmpbei[NICKLEN+1+USERLEN+1+HOSTLEN+1];

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
		    mask == CHFL_INVITE || mask == CHFL_REOPLIST)
		{
			/* XXX: rewrite latter to simply use alist, DO NOT copy it --B. */
			sprintf(tmpbei, "%s!%s@%s", lp->value.alist->nick,
				lp->value.alist->user, lp->value.alist->host);
			name = tmpbei;
		}
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
				IsServer(cptr) ? me.serv->sid : ME,
				chname, modebuf, parabuf);
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
void	send_channel_modes(aClient *cptr, aChannel *chptr)
{
	char	*me2 = me.serv->sid;

	if (check_channelmask(&me, cptr, chptr->chname))
		return;
#ifdef JAPANESE
	/* We did not send channel members, we don't send channel
	** modes to servers that are not prepared to handle JIS encoding. */
	if (!jp_valid(cptr, chptr, NULL))
                return;
#endif

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (modebuf[1] || *parabuf)
	{
		sendto_one(cptr, ":%s MODE %s %s %s",
			me2, chptr->chname, modebuf, parabuf);
	}

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_mode_list(cptr, chptr->chname, chptr->mlist, CHFL_BAN, 'b');
	send_mode_list(cptr, chptr->chname, chptr->mlist,
		CHFL_EXCEPTION, 'e');
	send_mode_list(cptr, chptr->chname, chptr->mlist,
		CHFL_INVITE, 'I');
	send_mode_list(cptr, chptr->chname, chptr->mlist,
		CHFL_REOPLIST, 'R');
	if (modebuf[1] || *parabuf)
	{
		/* complete sending, if anything left in buffers */
		sendto_one(cptr, ":%s MODE %s %s %s",
			me2, chptr->chname, modebuf, parabuf);
	}
}

/*
 * send "cptr" a full list of the channel "chptr" members and their
 * +ov status, using NJOIN
 */
void	send_channel_members(aClient *cptr, aChannel *chptr)
{
	Reg	Link	*lp;
	Reg	aClient *c2ptr;
	Reg	int	cnt = 0, len = 0, nlen;
	char	*p;
	char	*me2 = me.serv->sid;

	if (check_channelmask(&me, cptr, chptr->chname) == -1)
		return;
#ifdef JAPANESE
	/* We do not send channel members to servers that are not prepared 
	** to handle JIS encoding. */
	if (!jp_valid(cptr, chptr, NULL))
                return;
#endif
	sprintf(buf, ":%s NJOIN %s :", me2, chptr->chname);
	len = strlen(buf);

	for (lp = chptr->members; lp; lp = lp->next)
	    {
		c2ptr = lp->value.cptr;
		p = c2ptr->user ? c2ptr->user->uid : c2ptr->name;
		nlen = strlen(p);
		if ((len + nlen) > (size_t) (BUFSIZE - 9)) /* ,@+ \r\n\0 */
		    {
			sendto_one(cptr, "%s", buf);
			sprintf(buf, ":%s NJOIN %s :", me2, chptr->chname);
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

int	m_mode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int	penalty = 0;
	aChannel *chptr;
	char	*name, *p = NULL;

	parv[1] = canonize(parv[1]);

	for (name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	    {
		if (clean_channelname(name) == -1)
		{
			penalty += 1;
			continue;
		}
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
		if (parc < 3)	/* Only a query */
		    {
			*modebuf = *parabuf = '\0';
			modebuf[1] = '\0';
			channel_modes(sptr, modebuf, parabuf, chptr);
			sendto_one(sptr, replies[RPL_CHANNELMODEIS], ME, BadTo(parv[0]),
				   chptr->chname, modebuf, parabuf);
			penalty += 1;
		    }
		else	/* Check parameters for the channel */
		    {
			if(0==set_mode(cptr, sptr, chptr,
				&penalty, parc - 2, parv + 2))
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
static	int	set_mode(aClient *cptr, aClient *sptr, aChannel *chptr, 
			int *penalty, int parc, char *parv[])
{
	static	Link	chops[MAXMODEPARAMS+3];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				MODE_ANONYMOUS,  'a', MODE_REOP,       'r',
				0x0, 0x0 };

	Reg	Link	*lp = NULL;
	Reg	char	*curr = parv[0], *cp = NULL, *ucp = NULL;
	Reg	int	*ip;
	u_int	whatt = MODE_ADD;
	int	limitset = 0, count = 0, chasing = 0;
	int	nusers = 0, ischop, new, len, ulen, keychange = 0, opcnt = 0;
	int	reopseen = 0;
	aClient *who;
	Mode	*mode, oldm;
	Link	*plp = NULL;
#if 0
	int	compat = -1; /* to prevent mixing old/new modes */
#endif
	char	*mbuf = modebuf, *pbuf = parabuf, *upbuf = uparabuf;
	int	tmp_chfl = 0, tmp_rpl = 0, tmp_rpl2 = 0, tmp_mode = 0;

	*mbuf = *pbuf = *upbuf = '\0';
	if (parc < 1)
		return 0;

	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
	ischop = IsServer(sptr) || is_chan_op(sptr, chptr);
	new = mode->mode;

	while (curr && *curr && count >= 0)
	    {
#if 0
		if (compat == -1 && *curr != '-' && *curr != '+')
		{
			if (*curr == 'R')
			{
				compat = 1;
			}
			else
			{
				compat = 0;
			}
		}
#endif
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
				if (chptr->reop && IsServer(sptr) && !IsBursting(sptr))
				{
					reopseen = 1;
				}
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
				/* stop key swapping during netjoin
				** (prefer "highest" key) */
				if (IsServer(sptr) && IsBursting(sptr) &&
				    *mode->key && strncmp(mode->key, *parv,
				    (size_t) KEYLEN) >= 0)
					break;
				if (ischop)
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
				if (*mode->key && (ischop || IsServer(cptr)))
				    {
					lp = &chops[opcnt++];
					lp->value.cp = *parv;
					lp->value.cp[0] = '*';
					lp->value.cp[1] = '\0';
					lp->flags = MODE_KEY|MODE_DEL;
					keychange = 1;
				    }
			    }
			count++;
			*penalty += 2;
			break;
		case 'b':
		case 'e':
		case 'I':
		case 'R':
			switch (*curr)
			{
			case 'b':
				tmp_chfl = CHFL_BAN;
				tmp_rpl = RPL_BANLIST;
				tmp_rpl2 = RPL_ENDOFBANLIST;
				tmp_mode = MODE_BAN;
				break;
			case 'e':
				tmp_chfl = CHFL_EXCEPTION;
				tmp_rpl = RPL_EXCEPTLIST;
				tmp_rpl2 = RPL_ENDOFEXCEPTLIST;
				tmp_mode = MODE_EXCEPTION;
				break;
			case 'I':
				tmp_chfl = CHFL_INVITE;
				tmp_rpl = RPL_INVITELIST;
				tmp_rpl2 = RPL_ENDOFINVITELIST;
				tmp_mode = MODE_INVITE;
				break;
			case 'R':
				tmp_chfl = CHFL_REOPLIST;
				tmp_rpl = RPL_REOPLIST;
				tmp_rpl2 = RPL_ENDOFREOPLIST;
				tmp_mode = MODE_REOPLIST;
				break;
			}
			*penalty += 1;
			if (--parc <= 0)	/* beIR list query */
			{
				/* Feature: no other modes after query */
				*(curr+1) = '\0';	/* Stop MODE # bb.. */
				for (lp = chptr->mlist; lp; lp = lp->next)
				{
					if (lp->flags == tmp_chfl)
					{
						sendto_one(cptr,
							replies[tmp_rpl],
							ME, BadTo(cptr->name),
							chptr->chname,
							lp->value.alist->nick,
							lp->value.alist->user,
							lp->value.alist->host);
					}
				}
				sendto_one(cptr, replies[tmp_rpl2],
					ME, BadTo(cptr->name),
					chptr->chname);
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
				/* we deal with it later at parseNUH */
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|tmp_mode;
			}
			else if (whatt == MODE_DEL)
			{
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|tmp_mode;
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
				if (IsServer(sptr) && IsBursting(sptr) &&
				    mode->limit >= nusers)
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
					  (*ip == MODE_REOP && whatt == MODE_ADD &&
					   *chptr->chname != '!')) &&
					 !IsServer(sptr))
					sendto_one(cptr,
						   replies[ERR_UNKNOWNMODE],
						   ME, BadTo(sptr->name), *curr,
						   chptr->chname);
				else if (*ip == MODE_ANONYMOUS && whatt == MODE_ADD &&
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
#if 0
		/*
		 * Make sure new (+R) mode won't get mixed with old modes
		 * together on the same line.
		 */
		if (MyClient(sptr) && curr && *curr != '-' && *curr != '+')
		{
			if (*curr == 'R')
			{
				if (compat == 0)
				{
					*curr = '\0';
				}
			}
			else if (compat == 1)
			{
				*curr = '\0';
			}
		}
#endif
	    } /* end of while loop for MODE processing */

	if (reopseen)
	{
		ircstp->is_rreop++;
	}

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
			/* +r coming from server must trigger reop. If not
			** needed, it will be reset to 0 elsewhere, --B. */
			if (*ip == MODE_REOP && IsServer(sptr))
			{
				chptr->reop = timeofday + LDELAYCHASETIMELIMIT;
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
	 * Reconstruct "+beIRkOov" chain.
	 */
	if (opcnt)
	    {
		Reg	int	i = 0;
		Reg	char	c = '\0';
		char	*user, *host, numeric[16];
		int	tmplen;

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
			{
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
				ucp = lp->value.cptr->user ?
					lp->value.cptr->user->uid : cp;
				break;
			case MODE_UNIQOP :
				c = 'O';
				cp = lp->value.cptr->name;
				ucp = lp->value.cptr->user ?
					lp->value.cptr->user->uid : cp;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				ucp = lp->value.cptr->user ?
					lp->value.cptr->user->uid : cp;
				break;
			case MODE_BAN :
			case MODE_EXCEPTION :
			case MODE_INVITE :
			case MODE_REOPLIST :
				switch(lp->flags & MODE_WPARAS)
				{
				case MODE_BAN :
					c = 'b'; break;
				case MODE_EXCEPTION :
					c = 'e'; break;
				case MODE_INVITE :
					c = 'I'; break;
				case MODE_REOPLIST :
					c = 'R'; break;
				}
				/* parseNUH: */
				cp = lp->value.cp;
				if ((user = index(cp, '!')))
					*user++ = '\0';
				if ((host = rindex(user ? user : cp, '@')))
					*host++ = '\0';
				lp->value.alist = make_bei(cp, user, host);
				if (user)
					user[-1] = '!';
				if (host)
					host[-1] = '@';
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
			
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_BAN :
			case MODE_EXCEPTION :
			case MODE_INVITE :
			case MODE_REOPLIST :
				tmplen = BanLen(lp->value.alist) + 2 /* !@ */;
				if (len + tmplen + 2 > (size_t) MODEBUFLEN)
				{
					free_bei(lp->value.alist);
					tmplen = -1;
				}
				break;
			default:
				tmplen = strlen(cp);
				if (len + tmplen + 2 > (size_t) MODEBUFLEN)
				{
					tmplen = -1;
				}
			}

			if (tmplen == -1)
				break;
			/*
			 * pass on +/-o/v regardless of whether they are
			 * redundant or effective but check +b's to see if
			 * it existed before we created it.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_KEY :
				if (IsServer(sptr) &&
					!strncmp(mode->key, cp, (size_t) KEYLEN))
					break;
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
				if (IsServer(sptr) && mode->limit == nusers)
					break;
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
				    lp->flags == (MODE_CHANOP|MODE_DEL))
				{
					chptr->reop = timeofday + 
						LDELAYCHASETIMELIMIT +
						myrand() % 300;
				}
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
			case MODE_EXCEPTION :
			case MODE_INVITE :
			case MODE_REOPLIST :
				switch(lp->flags & MODE_WPARAS)
				{
				case MODE_BAN :
					tmp_chfl = CHFL_BAN; break;
				case MODE_EXCEPTION :
					tmp_chfl = CHFL_EXCEPTION; break;
				case MODE_INVITE :
					tmp_chfl = CHFL_INVITE; break;
				case MODE_REOPLIST :
					tmp_chfl = CHFL_REOPLIST; break;
				}
				if (tmp_chfl == CHFL_REOPLIST &&
					(whatt & MODE_ADD))
				{
					/* Just restarted servers will not have
					** chanops leaving, so no other way to
					** set ->reop. As we prefer not to op
					** remote clients, set this here, upon
					** each +R from remote server, so that
					** reop_channel has a chance to work.
					** It's mostly harmless, as chptr->reop
					** will be reset to 0 in is_chan_op()
					** and even if not, reop_channel() will
					** NOT give ops if ops are already on
					** the channel. --B. */
					if (IsServer(sptr))
					{
						chptr->reop = timeofday +
							LDELAYCHASETIMELIMIT +
							myrand() % 300;
					}
				}
				if (ischop &&
					(((whatt & MODE_ADD) &&
					!add_modeid(tmp_chfl, sptr, chptr,
						lp->value.alist))||
					((whatt & MODE_DEL) &&
					!del_modeid(tmp_chfl, chptr,
						lp->value.alist))))
				{
					char nuh[NICKLEN+USERLEN+HOSTLEN+3];

					/* I could strcat on u/pbuf directly,
					** but this looks nicer. Note that alist
					** values were already cleaned. --B. */
					tmplen = sprintf(nuh, "%s!%s@%s",
						lp->value.alist->nick,
						lp->value.alist->user,
						lp->value.alist->host);
					*mbuf++ = c;
					(void)strcat(pbuf, nuh);
					(void)strcat(upbuf, nuh);
					len += tmplen;
					ulen += tmplen;
					(void)strcat(pbuf, " ");
					(void)strcat(upbuf, " ");
					len++;
					ulen++;
					if ((whatt & MODE_DEL))
						free_bei(lp->value.alist);
				}
				else
				{
					/* We have to free lp->value.alist
					** allocated by make_bei, otherwise
					** it is memleak. del_modeid always
					** succeeds, so it is freed above.
					** If add_modeid succeeds, it uses
					** pointer, if not, we free it here.
					** This also covers all other cases,
					** like !ischop. --B. */
					free_bei(lp->value.alist);
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
		else if (sptr->user)
		{
			s = sptr->user->uid;
		}
		else
		{
			s = sptr->name;
		}

		sendto_match_servs_v(chptr, cptr, SV_UID,
			":%s MODE %s %s %s", s, chptr->chname, mbuf, upbuf);

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

static	int	can_join(aClient *sptr, aChannel *chptr, char *key)
{
	invLink	*lp = NULL;
	Link	*banned;
	int	limit = 0;

	if (chptr->users == 0 && (bootopt & BOOT_PROT) && 
	    chptr->history != 0 && *chptr->chname != '!')
		return (timeofday > chptr->history) ? 0 : ERR_UNAVAILRESOURCE;

#ifdef CLIENTS_CHANNEL
	if (*chptr->chname == '&' && !strcmp(chptr->chname, "&CLIENTS")
		&& is_allowed(sptr, ACL_CLIENTS))
		return 0;
#endif
	if (*chptr->chname == '&' && !strcmp(chptr->chname, "&OPER")
		&& IsAnOper(sptr))
		return 0;

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->chptr == chptr)
			break;

	if ((banned = match_modeid(CHFL_BAN, sptr, chptr)))
	{
		if (match_modeid(CHFL_EXCEPTION, sptr, chptr))
		{
			banned = NULL;
		}
		else if (lp == NULL) /* not invited */
		{
			return (ERR_BANNEDFROMCHAN);
		}
	}

	if ((chptr->mode.mode & MODE_INVITEONLY)
	    && !match_modeid(CHFL_INVITE, sptr, chptr)
	    && (lp == NULL))
		return (ERR_INVITEONLYCHAN);

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (chptr->mode.limit && (chptr->users >= chptr->mode.limit))
	{
		/* ->reop is set when there are no chanops on the channel,
		** so we allow people matching +R to join no matter limit,
		** so they can get reopped --B. */
		if (chptr->reop > 0 && match_modeid(CHFL_REOPLIST, sptr, chptr))
			return 0;
		if (lp == NULL)
			return (ERR_CHANNELISFULL);
		else
			limit = 1;
	}

	if (banned)
	{
		sendto_channel_butone(&me, &me, chptr,
			":%s NOTICE %s :%s carries an invitation from %s"
			" (overriding%s ban on %s!%s@%s).",
			ME, chptr->chname, sptr->name, lp->who,
			limit ? " channel limit and" : "",
			banned->value.alist->nick,
			banned->value.alist->user,
			banned->value.alist->host);
	}
	else if (limit)
	{
		sendto_channel_butone(&me, &me, chptr,
			":%s NOTICE %s :%s carries an invitation from %s"
			" (overriding channel limit).", ME, chptr->chname,
			sptr->name, lp->who);
	}
	return 0;
}

/*
** Remove bells and commas from channel name
*/

int	clean_channelname(char *cn)
{
	int flag = 0;

	while (*cn)
	{
		if (*cn == '\007' || *cn == ' ' || (!flag && *cn == ','))
		{
			*cn = '\0';
			return 0;
		}
#ifdef JAPANESE
		/* Japanese channel names can have comma in their name, but
		** only between "\033$B" (begin) and "\033(B" (end) markers.
		** So we mark it (using flag) for above check. --Beeth */
		if (cn[0] == '\033'
			&& (cn[1] == '$' || cn[1] == '(')
			&& cn[2] == 'B')
		{
			flag = (cn[1] == '$') ? 1 : 0;
			cn += 2;
		}
#endif
		cn++;
	}
	/* If flag is 1 here, Japanese channel name is incomplete! */
	return flag;
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
static	int	check_channelmask(aClient *sptr, aClient *cptr, char *chname)
{
	char	*s;

	if (*chname == '&' && IsServer(cptr))
		return -1;
	s = get_channelmask(chname);
	if (!s)
		return 0;
	s++;
	if (*s == '\0'	/* ':' was last char, thus empty mask --B. */
		|| match(s, ME) || (IsServer(cptr) && match(s, cptr->name)))
	{
		if (MyClient(sptr))
                       sendto_one(sptr, replies[ERR_BADCHANMASK], ME,
                               BadTo(sptr->name), chname);
		return -1;
	}
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static	aChannel *get_channel(aClient *cptr, char *chname, int flag)
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
#ifdef JAPANESE
#if 0
		/* XXX-JP: I think I know why it is here, but it
		** seems it is completely unneeded. */
		if (check_channelmask(cptr, cptr, chname) == -1)
			return NULL;
#endif
#endif
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
#ifdef JAPANESE
		chptr->flags = 0;
		if (jp_chname(chname))
			chptr->flags = FLAGS_JP;
#endif
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
	    }
	return chptr;
}

/*
 * add_invite():
 * 	sptr:	who invites
 *	cptr	who gets invitation
 *	chptr	what channel
 */
static	void	add_invite(aClient *sptr, aClient *cptr, aChannel *chptr)
{

	/*
	assert(sptr!=NULL);
	assert(cptr!=NULL);
	assert(chptr!=NULL);
	*/

	del_invite(cptr, chptr);

	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	{
		del_invite(cptr, cptr->user->invited->chptr);
	}

	/*
	 * add client to channel invite list
	 */
	{
		Reg	Link	*inv;

		inv = make_link();
		inv->value.cptr = cptr;
		inv->next = chptr->invites;
		chptr->invites = inv;
		istat.is_useri++;
	}
	/*
	 * add channel to the end of the client invite list
	 */
	{
		Reg	invLink	*inv, **tmp;
		char	who[NICKLEN+USERLEN+HOSTLEN+3];
		int	len;

		for (tmp = &(cptr->user->invited); *tmp; tmp = &((*tmp)->next))
			;
		inv = make_invlink();
		(*tmp) = inv;
		inv->chptr = chptr;
		inv->next = NULL;
		len = sprintf(who, "%s!%s@%s", sptr->name,
			sptr->user->username, sptr->user->host);
		inv->who = (char *)MyMalloc(len + 1);
		istat.is_banmem += len;
		strcpy(inv->who, who);
		istat.is_invite++;
	}
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void	del_invite(aClient *cptr, aChannel *chptr)
{
	{
		Reg	Link	**inv, *tmp;

		for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		{
			if (tmp->value.cptr == cptr)
			{
				*inv = tmp->next;
				free_link(tmp);
				istat.is_invite--;
				break;
			}
		}
	}
	{
		Reg	invLink	**inv, *tmp;

		for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		{
			if (tmp->chptr == chptr)
			{
				*inv = tmp->next;
				istat.is_banmem -= (strlen(tmp->who)+1);
				free(tmp->who);
				free_invlink(tmp);
				istat.is_useri--;
				break;
			}
		}
	}
}

/*
**  The last user has left the channel, free data in the channel block,
**  and eventually the channel block itself.
*/
static	void	free_channel(aChannel *chptr)
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
			istat.is_banmem -= BanLen(obtmp->value.alist);
			istat.is_bans--;
			free_bei(obtmp->value.alist);
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
			MyFree(chptr);
	    }
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int	m_join(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static	char	jbuf[BUFSIZE];
	Reg	Link	*lp;
	Reg	aChannel *chptr;
	Reg	char	*name, *key = NULL;
	int	i, tmplen, flags = 0;
	char	*p = NULL, *p2 = NULL;

	/* This is the only case we get JOIN over s2s link. --B. */
	/* It could even be its own command. */
	if (IsServer(cptr))
	{
		if (parv[1][0] == '0' && parv[1][1] == '\0')
		{
			if (sptr->user->channel == NULL)
				return 0;
			while ((lp = sptr->user->channel))
			{
				chptr = lp->value.chptr;
				sendto_channel_butserv(chptr, sptr,
						PartFmt,
						parv[0], chptr->chname,
						key ? key : "");
				remove_user_from_channel(sptr, chptr);
			}
			sendto_match_servs(NULL, cptr, ":%s JOIN 0 :%s",
				sptr->user->uid, key ? key : parv[0]);
		}
		else
		{
			/* Well, technically this is an error.
			** Let's ignore it for now. --B. */
		}
		return 0;
	}
	/* These should really be assert()s. */
	if (!sptr || !sptr->user)
		return 0;

	*jbuf = '\0';
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
		if (*name == '0' && !atoi(name))
		{
			(void)strcpy(jbuf, "0");
			i = 1;
			continue;
		}
		if (clean_channelname(name) == -1)
		{
			sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
				ME, BadTo(parv[0]), name);
			continue;
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
				sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
					ME, BadTo(parv[0]), name);
				continue;
			}
			if (*name == '!' && (*(name+1) == '#' ||
					     *(name+1) == '!'))
			{
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
				sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
					ME, BadTo(parv[0]), name);
				continue;
			}
			else if (chptr)
			{
				/* joining a !channel using the short name */
				if (hash_find_channels(name+1, chptr))
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
		    (*name == '+' && (*(name+1) == '#' || *(name+1) == '!')) ||
		    (*name == '!' && IsChannelName(name+1)))
		{
			sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
				ME, BadTo(parv[0]), name);
			continue;
		}
		tmplen = strlen(name);
		if (i + tmplen + 2 /* comma and \0 */
			>= sizeof(jbuf) )
		{
			break;
		}
		if (*jbuf)
		{
			jbuf[i++] = ',';
		}
		(void)strcpy(jbuf + i, name);
		i += tmplen;
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
						key ? key : "");
				remove_user_from_channel(sptr, chptr);
			}
			sendto_match_servs(NULL, cptr, ":%s JOIN 0 :%s",
				sptr->user->uid, key ? key : parv[0]);
			continue;
		}

		/* Weren't those names just cleaned? --B. */
		if (clean_channelname(name) == -1)
			continue;

		/* Get chptr for given name. Do not create channel yet.
		** Can return NULL. */
		chptr = get_channel(sptr, name, !CREATE);

		if (chptr && IsMember(sptr, chptr))
		{
			continue;
		}

		if (MyConnect(sptr) && !(chptr && IsQuiet(chptr)) &&
			sptr->user->joined >= MAXCHANNELSPERUSER)
		{
			sendto_one(sptr, replies[ERR_TOOMANYCHANNELS],
				   ME, BadTo(parv[0]), name);
			/* can't return, need to send the info everywhere */
			continue;
		}

		if (!strncmp(name, "\x23\x1f\x02\xb6\x03\x34\x63\x68\x02\x1f",
			     10))
		{
			sptr->exitc = EXITC_VIRUS;
			return exit_client(sptr, sptr, &me, "Virus Carrier");
		}

		if (!chptr)
		{
			/* Oh well, create the channel. */
			chptr = get_channel(sptr, name, CREATE);
		}

		if (!chptr)
		{
			/* Should NEVER happen. */
			sendto_flag(SCH_ERROR, "Could not create channel!");
			sendto_one(sptr, "%s *** %s :Could not create channel!",
				ME, BadTo(parv[0]));
			continue;
		}

		if ((i = can_join(sptr, chptr, key)))
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
		if (UseModes(name) &&
			(*name != '#' || !IsSplit()) &&
		    (!IsRestricted(sptr) || (*name == '&')) && !chptr->users &&
		    !(chptr->history && *chptr->chname == '!'))
		{
			if (*name == '!')
				flags |= CHFL_UNIQOP|CHFL_CHANOP;
			else
				flags |= CHFL_CHANOP;
		}
		/* Complete user entry to the new channel */
		add_user_to_channel(chptr, sptr, flags);
		/* Notify all users on the channel */
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
			parv[0], chptr->chname);

		del_invite(sptr, chptr);
		if (chptr->topic[0] != '\0')
		{
			sendto_one(sptr, replies[RPL_TOPIC], ME,
				BadTo(parv[0]), chptr->chname, chptr->topic);
#ifdef TOPIC_WHO_TIME
			if (chptr->topic_t > 0)
			{
				sendto_one(sptr, replies[RPL_TOPIC_WHO_TIME],
					ME, BadTo(parv[0]),
					chptr->chname, IsAnonymous(chptr) ?
					"anonymous!anonymous@anonymous." :
					chptr->topic_nuh,
					chptr->topic_t);
			}
#endif
		}

		names_channel(cptr, sptr, parv[0], chptr, 1);
		if (IsAnonymous(chptr) && !IsQuiet(chptr))
		{
			sendto_one(sptr, ":%s NOTICE %s :Channel %s has the anonymous flag set.", ME, chptr->chname, chptr->chname);
			sendto_one(sptr, ":%s NOTICE %s :Be aware that anonymity on IRC is NOT securely enforced!", ME, chptr->chname);
		}
		/*
	        ** notify other servers
		*/
		if (get_channelmask(name) || *chptr->chname == '!' /* compat */
#ifdef JAPANESE
			/* sendto_match_servs_v() is checking the same
			** and NOT sending things out. --B. */
			|| !jp_valid(NULL, chptr, NULL)
#endif
			)
		{
			sendto_match_servs_v(chptr, cptr, SV_UID,
				":%s NJOIN %s :%s%s", me.serv->sid, name,
				flags & CHFL_UNIQOP ? "@@" : 
				flags & CHFL_CHANOP ? "@" : "",
				sptr->user ? sptr->user->uid : parv[0]);
		}
		else if (*chptr->chname != '&')
		{
			sendto_serv_v(cptr, SV_UID, ":%s NJOIN %s :%s%s",
				me.serv->sid, name,
				flags & CHFL_UNIQOP ? "@@" : 
				flags & CHFL_CHANOP ? "@" : "",
				sptr->user ? sptr->user->uid : parv[0]);
		}
	}
	return 2;
}

/*
** m_njoin
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel members and modes
*/
int	m_njoin(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *name, *target;
	char mbuf[3] /* "ov" */;
	char uidbuf[BUFSIZE], *u;
	char *p = NULL;
	int chop, cnt = 0;
	aChannel *chptr = NULL;
	aClient *acptr;
	int maxlen;

	/* get channel pointer */
	if (!IsChannelName(parv[1]))
	{
		sendto_one(sptr, replies[ERR_NOSUCHCHANNEL],
			ME, BadTo(parv[0]), parv[1]);
		return 0;
	}
	if (check_channelmask(sptr, cptr, parv[1]) == -1)
	{
		sendto_flag(SCH_DEBUG, "received NJOIN for %s from %s",
			parv[1], get_client_name(cptr, TRUE));
		return 0;
	}
	/* Use '&me', because NJOIN is, unlike JOIN, always for
	** remote clients, see get_channel() what's that for. --B. */
	chptr = get_channel(&me, parv[1], CREATE);
	/* assert(chptr != NULL); */

	/* Hack for creating empty channels and locking them.
	** This also allows for getting MODEs for such channels (otherwise
	** they'd get ignored). We need that to prevent desynch, especially
	** after we (re)started.
	** This requires that single dot cannot be used as a name of
	** remote clients that can join channels. --B. */
	if (parv[2][0] == '.' && parv[2][1] == '\0')
	{
		/* If we have clients on a channel, it cannot be
		** locked, can it? --B. */
		if (chptr->users == 0 && chptr->history == 0)
		{
			chptr->history = timeofday + (*chptr->chname == '!' ?
				LDELAYCHASETIMELIMIT : DELAYCHASETIMELIMIT);
			istat.is_hchan++;
			istat.is_hchanmem += sizeof(aChannel) +
				strlen(chptr->chname);
		}
		/* There cannot be anything else in this NJOIN. */
		return 0;
	}

	*uidbuf = '\0'; u = uidbuf;
 	/* 17 comes from syntax ": NJOIN  :,@@+\r\n\0" */ 
	maxlen = BUFSIZE - 17 - strlen(parv[0]) - strlen(parv[1]) - NICKLEN;
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
				if (*(target+2) == '+')
				{
					strcpy(mbuf, "ov");
					chop = CHFL_UNIQOP| \
						CHFL_CHANOP|CHFL_VOICE;
					name = target + 3;
				}
				else
				{
					strcpy(mbuf, "o");
					chop = CHFL_UNIQOP|CHFL_CHANOP;
					name = target + 2;
				}
			}
			else
			{
				if (*(target+1) == '+')
				{
					strcpy(mbuf, "ov");
					chop = CHFL_CHANOP|CHFL_VOICE;
					name = target+2;
				}
				else
				{
					strcpy(mbuf, "o");
					chop = CHFL_CHANOP;
					name = target+1;
				}
			}
		}
		else if (*target == '+')
		{
			strcpy(mbuf, "v");
			chop = CHFL_VOICE;
			name = target+1;
		}
		else
		{
			name = target;
		}
		/* find user */
		if (!(acptr = find_person(name, NULL)) &&
			!(acptr = find_uid(name, NULL)))
		{
			/* shouldn't this be an error? --B. */
			continue;
		}
		/* is user who we think? */
		if (acptr->from != cptr)
		{
			/* shouldn't this be a squit-level error? --B. */
			continue;
		}
		/* make sure user isn't already on channel */
		if (IsMember(acptr, chptr))
		{
			if (IsBursting(sptr))
			{
				sendto_flag(SCH_ERROR, "NJOIN protocol error"
					" from %s (%s already on %s)",
					get_client_name(cptr, TRUE),
					acptr->name, chptr->chname);
				sendto_one(cptr, "ERROR :NJOIN protocol error"
					" (%s already on %s)",
					acptr->name, chptr->chname);
			}
			else
			{
				sendto_flag(SCH_CHAN, "Fake: %s JOIN %s",
					acptr->name, chptr->chname);
			}
			/* ignore such join anyway */
			continue;
		}
		/* add user to channel */
		add_user_to_channel(chptr, acptr, UseModes(parv[1]) ? chop :0);

		/* build buffer for NJOIN and UID capable servers */

		/* send it out if too big to fit buffer */
		if (u-uidbuf >= maxlen)
		{
			*u = '\0';
			sendto_match_servs_v(chptr, cptr, SV_UID,
				":%s NJOIN %s :%s",
				sptr->serv->sid, parv[1], uidbuf);
			*uidbuf = '\0'; u = uidbuf;
		}

		if (u != uidbuf)
		{
			*u++ = ',';
		}

		/* Copy the modes. */
		for (; target < name; target++)
		{
			*u++ = *target;
		}

		target = acptr->user ? acptr->user->uid : acptr->name;
		while (*target)
		{
			*u++ = *target++;
		}

		/* send join to local users on channel */
		/* Little syntax trick. Put ":" before channel name if it is
		** not burst, so clients can use it for discriminating normal
		** join from netjoin. 2.10.x is using NJOIN only during
		** burst, but 2.11 always. Hence we check for EOB from 2.11
		** to know what kind of NJOIN it is. --B. */
		sendto_channel_butserv(chptr, acptr, ":%s JOIN %s%s", acptr->name, (
#ifdef JAPANESE
			/* XXX-JP: explain why jp-patch had that! */
			IsServer(sptr) ||
#endif
			IsBursting(sptr)) ? "" : ":", chptr->chname);
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
				strcat(modebuf, mbuf);
				cnt += strlen(mbuf);
				if (*parabuf)
				    {
					strcat(parabuf, " ");
				    }
				strcat(parabuf, acptr->name);
				if (mbuf[1])
				    {
					strcat(parabuf, " ");
					strcat(parabuf, acptr->name);
				    }
				break;
			case 2:
				sendto_channel_butserv(chptr, &me,
					       ":%s MODE %s +%s%c %s %s",
						       sptr->name, chptr->chname,
						       modebuf, mbuf[0],
						       parabuf, acptr->name);
				if (mbuf[1])
				    {
					strcpy(modebuf, mbuf+1);
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
						       sptr->name, chptr->chname,
						       modebuf, parabuf);
				cnt = 0;
			    }
		    }
	    }
	/* send eventual MODE leftover */
	if (cnt)
		sendto_channel_butserv(chptr, &me, ":%s MODE %s +%s %s",
				       sptr->name, chptr->chname, modebuf, parabuf);

	/* send NJOIN */
	*u = '\0';
	if (uidbuf[0])
	{
		sendto_match_servs_v(chptr, cptr, SV_UID, ":%s NJOIN %s :%s",
			sptr->serv->sid, parv[1], uidbuf);
	}

	return 0;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_part(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg	aChannel *chptr;
	char	*p = NULL, *name, *comment = "";
	int	size;

	*buf = '\0';

	parv[1] = canonize(parv[1]);
	comment = (BadPtr(parv[2])) ? "" : parv[2];
	if (strlen(comment) > TOPICLEN)
		comment[TOPICLEN] = '\0';

	/*
	** Broadcasted to other servers is ":nick PART #chan,#chans :comment",
	** so we must make sure buf does not contain too many channels or later
	** they get truncated! "10" comes from all fixed chars: ":", " PART "
	** and ending "\r\n\0". We could subtract strlen(comment)+2 here too, 
	** but it's not something we care, is it? :->
	** Btw: if we ever change m_part to have UID as source, fix this! --B.
	*/
	size = BUFSIZE - strlen(parv[0]) - 10;
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
		/*
		**  Remove user from the old channel (if any)
		*/
		if (!get_channelmask(name) && (*chptr->chname != '!')
#ifdef JAPANESE
			/* jp_valid here because in else{} there's
			** sendto_match_servs() which ignores such
			** channels when sending stuff. --B. */
			&& jp_valid(NULL, chptr, NULL)	/* XXX-JP: why not 
							jp_valid(NULL, chptr, name)?
							because user cannot be on 
							jp-named channel which is not 
							flagged JP? --B. */
#endif
			)
		{	/* channel:*.mask */
			if (*name != '&')
			{
				/* We could've decreased size by 1 when
				** calculating it, but I left it like that
				** for the sake of clarity. --B. */
				if (strlen(buf) + strlen(name) + 1
					> size)
				{
					/* Anyway, if it would not fit in the
					** buffer, send it right away. --B */
					sendto_serv_butone(cptr, PartFmt,
						sptr->user->uid, buf, comment);
					*buf = '\0';
				}
				if (*buf)
					(void)strcat(buf, ",");
				(void)strcat(buf, name);
			}
		}
		else
			sendto_match_servs(chptr, cptr, PartFmt,
				   	   sptr->user->uid, name, comment);
		sendto_channel_butserv(chptr, sptr, PartFmt,
				       parv[0], chptr->chname, comment);
		remove_user_from_channel(sptr, chptr);
	}
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, sptr->user->uid, buf, comment);
	return 4;
}

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int	m_kick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *who;
	aChannel *chptr;
	int	chasing = 0, penalty = 0;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
	char	*tmp, *tmp2;
	char	*sender;
	char	nbuf[BUFSIZE+1];
	int	clen, maxlen;

	if (IsServer(sptr))
		sendto_flag(SCH_NOTICE, "KICK from %s for %s %s",
			    parv[0], parv[1], parv[2]);
	if (BadPtr(parv[3]))
	{
		comment = "no reason";
	}
	else
	{
		comment = parv[3];
		if (strlen(comment) > (size_t) TOPICLEN)
			comment[TOPICLEN] = '\0';
	}

	if (IsServer(sptr))
	{
		sender = sptr->serv->sid;
	}
	else if (sptr->user)
	{
		sender = sptr->user->uid;
	}
	else
	{
		sender = sptr->name;
	}

	/* we'll decrease it for each channel later */
	maxlen = BUFSIZE - MAX(strlen(sender), strlen(sptr->name))
		- strlen(comment) - 10; /* ":", " KICK ", " " and " :" */

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

		clen = maxlen - strlen(name) - 1; /* for comma, see down */
		nbuf[0] = '\0';
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
					chptr->chname, who->name, comment);

				/* Nick buffer to kick out. */
				/* as we need space for ",nick", we should add
				** 1 on the left side; instead we subtracted 1
				** on the right side, before the loop. */
				if (strlen(nbuf) + (who->user ? UIDLEN :
					strlen(who->name)) >= clen)
				{
					sendto_match_servs_v(chptr, cptr,
						SV_UID, ":%s KICK %s %s :%s",
						sender, name, nbuf, comment);
					nbuf[0] = '\0';
				}
				if (*nbuf)
				{
					strcat(nbuf, ",");
				}
				strcat(nbuf, who->user ? who->user->uid :
					who->name);

				/* kicking last one out may destroy chptr */
				if (chptr->users == 1)
				{
					if (*nbuf)
					{
						sendto_match_servs_v(chptr, 
							cptr, SV_UID,
							":%s KICK %s %s :%s",
							sender, name,
							nbuf, comment);
						nbuf[0] = '\0';
					}
				}
				remove_user_from_channel(who, chptr);
				penalty += 2;
				if (MyPerson(sptr) &&
					/* penalties, obvious */
					(penalty >= MAXPENALTY
					/* Stop if user kicks himself out
					** of channel --B. */
					|| who == sptr))
				{
					break;
				}
			    }
			else
				sendto_one(sptr,
					   replies[ERR_USERNOTINCHANNEL],
					   ME, BadTo(parv[0]), user, name);
		    } /* loop on parv[2] */
		MyFree(tmp);
		/* finish sending KICK for given channel */
		if (*nbuf)
		{
			sendto_match_servs_v(chptr, cptr, SV_UID,
				":%s KICK %s %s :%s", sender, name,
				nbuf, comment);
		}
	    } /* loop on parv[1] */

	return penalty;
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = channels list
**	parv[2] = topic text
*/
int	m_topic(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aChannel *chptr = NullChn;
	char	*topic = NULL, *name, *p = NULL;
	int	penalty = 1;
	
	parv[1] = canonize(parv[1]);

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		if (!IsChannelName(name))
		{
			sendto_one(sptr, replies[ERR_NOTONCHANNEL], ME,
				parv[0], name);
			continue;
		}
		if (!UseModes(name))
		{
			sendto_one(sptr, replies[ERR_NOCHANMODES], ME, 
				parv[0], name);
			continue;
		}
		chptr = find_channel(name, NullChn);
		if (!chptr)
		{
			sendto_one(sptr, replies[ERR_NOSUCHCHANNEL], ME,
				parv[0], name);
			return penalty;
		}
		if (!IsMember(sptr, chptr))
		{
			sendto_one(sptr, replies[ERR_NOTONCHANNEL], ME,
				parv[0], name);
			continue;
		}

		/* should never be true at this point --B. */
		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (parc > 2)
			topic = parv[2];

		if (!topic)  /* only asking  for topic  */
		{
			if (chptr->topic[0] == '\0')
			{
				sendto_one(sptr, replies[RPL_NOTOPIC], ME,
					parv[0], chptr->chname);
			}
			else
			{
				sendto_one(sptr, replies[RPL_TOPIC], ME,
					parv[0], chptr->chname, chptr->topic);
#ifdef TOPIC_WHO_TIME
				if (chptr->topic_t > 0)
				sendto_one(sptr, replies[RPL_TOPIC_WHO_TIME],
					ME, BadTo(parv[0]), chptr->chname,
					IsAnonymous(chptr) ?
					"anonymous!anonymous@anonymous." :
					chptr->topic_nuh, chptr->topic_t);
#endif
			}
		}
		else if ((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
			 is_chan_op(sptr, chptr))
		{	/* setting a topic */
			strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
#ifdef TOPIC_WHO_TIME
			sprintf(chptr->topic_nuh, "%s!%s@%s", sptr->name,
				sptr->user->username, sptr->user->host);
			chptr->topic_t = timeofday;
#endif
			sendto_match_servs(chptr, cptr,":%s TOPIC %s :%s",
					   sptr->user->uid, chptr->chname,
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
int	m_invite(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	aChannel *chptr;

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, replies[ERR_NOSUCHNICK], ME, BadTo(parv[0]), parv[1]);
		return 1;
	    }
	if (clean_channelname(parv[2]) == -1)
		return 1;
	if (check_channelmask(sptr, acptr->user->servp->bcptr, parv[2]))
		return 1;
	if (*parv[2] == '&' && !MyClient(acptr))
		return 1;
	chptr = find_channel(parv[2], NullChn);

#ifdef JAPANESE
	if (!jp_valid(acptr->from, chptr, parv[2]))
	{
		sendto_one(sptr, replies[ERR_BADCHANMASK], ME,
			chptr ? chptr->chname : parv[2]);
		return 1;
	}
#endif
	if (!chptr && parv[2][0] == '!')
	{
		/* Try to find !channel using shortname */
		chptr = hash_find_channels(parv[2] + 1, NULL);
	}

	if (!chptr)
	{
		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
				  parv[0], parv[1], parv[2]);
		if (MyConnect(sptr))
		{
        	        sendto_one(sptr, replies[RPL_INVITING], ME, BadTo(parv[0]),
	                           acptr->name, parv[2]);
			if (acptr->user->flags & FLAGS_AWAY)
					send_away(sptr, acptr);
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
			send_away(sptr, acptr);
	}

	if (MyConnect(acptr))
		if (chptr && /* (chptr->mode.mode & MODE_INVITEONLY) && */
		    sptr->user && is_chan_op(sptr, chptr))
			add_invite(sptr, acptr, chptr);

	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 2;
}


/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int	m_list(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aChannel *chptr;
	char	*name, *p = NULL;
	int	rlen = 0;

	if (parc > 2 &&
	    hunt_server(cptr, sptr, ":%s LIST %s %s", 2, parc, parv))
		return 10;
	
	if (BadPtr(parv[1]))
	{
		Link *lp;
		int listedchannels = 0;
		int maxsendq = 0;
		
		if (!sptr->user)
		{
			sendto_one(sptr, replies[RPL_LISTEND], ME,
				   BadTo(parv[0]));
			return 2;
		}
		
#ifdef LIST_ALIS_NOTE
		if (MyConnect(sptr))
		{
			sendto_one(sptr, ":%s NOTICE %s :%s", ME, parv[0],
				LIST_ALIS_NOTE);
		}
#endif
		/* Keep 10% of sendQ free
		 * Note: Definition of LIST command prevents obtaining
		 * of complete LIST from remote server, if this
		 * behaviour is changed, MyConnect() check needs to be added
		 * here and within following loops as well. - jv
		 */
		maxsendq = (int) ((float) get_sendq(sptr, 0) * (float) 0.9);
		
		/* First, show all +s/+p channels user is on */
		for (lp = sptr->user->channel; lp; lp = lp->next)
		{
			chptr = lp->value.chptr;
			if (SecretChannel(chptr) || HiddenChannel(chptr))
			{
				sendto_one(sptr, replies[RPL_LIST], ME,
					   BadTo(parv[0]), chptr->chname,
					   chptr->users, chptr->topic);
				listedchannels++;

				if (DBufLength(&sptr->sendQ) > maxsendq)
				{
					sendto_one(sptr,
						replies[ERR_TOOMANYMATCHES],
						ME, BadTo(parv[0]), "LIST");
					goto end_of_list;
				}
			}
		}

		/* Second, show all visible channels; +p channels are not
		 * reported if user is not their member - jv.
		 */
		for (chptr = channel; chptr; chptr = chptr->nextch)
		{
			if (!chptr->users ||    /* empty locked channel */
			    SecretChannel(chptr) || HiddenChannel(chptr))
			{
				continue;
			}
			sendto_one(sptr, replies[RPL_LIST], ME, BadTo(parv[0]),
				chptr->chname, chptr->users,
				chptr->topic);

			listedchannels++;

			if (DBufLength(&sptr->sendQ) > maxsendq)
			{
				sendto_one(sptr, replies[ERR_TOOMANYMATCHES],
					ME, BadTo(parv[0]), "LIST");

				break;
			}
		
		}
		
end_of_list:;
#ifdef LIST_ALIS_NOTE
		/* Send second notice if we listed more than 24 channels
		 * - usual height of irc client in text mode.
		 */
		if (MyConnect(sptr) && (listedchannels > 24))
		{
			sendto_one(sptr, ":%s NOTICE %s :%s", ME, parv[0],
				LIST_ALIS_NOTE);
		}
#endif
	
	}
	else
	{
		parv[1] = canonize(parv[1]);
		for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
		{
			chptr = find_channel(name, NullChn);
			if (chptr && ShowChannel(sptr, chptr) && sptr->user)
			{
				rlen += sendto_one(sptr, replies[RPL_LIST],
						   ME, BadTo(parv[0]), chptr->chname,
						   chptr->users, chptr->topic);
				if (!MyConnect(sptr) && rlen > CHREPLLEN)
					break;
			}
			if (*name == '!')
			{
				chptr = NULL;
				while ((chptr = hash_find_channels(name + 1,
					chptr)))
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
		sendto_one(sptr, replies[ERR_TOOMANYMATCHES], ME,
			BadTo(parv[0]), "LIST");
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
	Reg 	Link	*lp = NULL;
	Reg	aClient	*acptr;
	int 	pxlen, ismember = 0, nlen, maxlen;
	char 	*pbuf = buf;
	int	showusers = 1;
	
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
		pxlen += 4;  /* '[=|*|@]' ' + ' ' + ':'  */
		
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
				if (strchr(acptr->name, '.'))
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
int	m_names(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
			if (clean_channelname(name) == -1)
				continue;
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

		sent = sent < 2 ? 2 : (sent * MAXCHANNELSPERUSER) / MAXPENALTY;	
		return sent < 2 ? 2 : sent;
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
		if (strchr(acptr->name, '.'))
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
/* consider reoping an opless channel */
static int	reop_channel(time_t now, aChannel *chptr, int reopmode)
{
	Link *lp, op;

	if (IsSplit() || chptr->reop == 0)
	{
		/* Should never happen. */
		sendto_flag(SCH_DEBUG, "reop_channel should not happen");
		return 0;
	}

	op.value.chptr = NULL;
	/* Why do we wait until CD expires? --B. */
	if (now - chptr->history > DELAYCHASETIMELIMIT)
	{
		int idlelimit1 = 0, idlelimit2 = 0;

		if (reopmode != CHFL_REOPLIST)
		{
			/*
			** This selects random idle limits in the range
			** from CHECKFREQ to 4*CHECKFREQ
			*/
			idlelimit1 = CHECKFREQ + myrand() % (2*CHECKFREQ);
			idlelimit2 = idlelimit1 + CHECKFREQ +
				myrand() % (2*CHECKFREQ);
		}

		for (lp = chptr->members; lp; lp = lp->next)
		{
			/* not restricted */
			if (IsRestricted(lp->value.cptr))
			{
				continue;
			}
			if (lp->flags & CHFL_CHANOP)
			{
				chptr->reop = 0;
				return 0;
			}
			/* Our client */
			if (!MyConnect(lp->value.cptr))
			{
				continue;
			}
			/* matching +R list */
			if (reopmode == CHFL_REOPLIST &&
				NULL == match_modeid(CHFL_REOPLIST,
				lp->value.cptr, chptr))
			{
				continue;
			}
			/* If +R list or channel reop is heavily overdue,
			** don't care about idle. Find the least idle client.
			*/
			if (reopmode == CHFL_REOPLIST ||
				now - chptr->reop > 7*LDELAYCHASETIMELIMIT)
			{
				if (op.value.cptr == NULL ||
					lp->value.cptr->user->last >
					op.value.cptr->user->last)
				{
					op.value.cptr = lp->value.cptr;
					continue;
				}
				continue;
			}
			/* else hidden in above continue,
			** not to indent too much :> --B. */

			/* Channel reop is not heavily overdue. So pick
			** a client, which is a bit idle, but not too much.
			** Find the least idle client possible, though.
			*/
			if (now - lp->value.cptr->user->last >= idlelimit1 &&
				now - lp->value.cptr->user->last < idlelimit2 &&
				(op.value.cptr == NULL ||
				lp->value.cptr->user->last >
					op.value.cptr->user->last))
			{
				op.value.cptr = lp->value.cptr;
			}
		}
		if (op.value.cptr == NULL)
		{
			return 0;
		}
		sendto_channel_butone(&me, &me, chptr,
			":%s NOTICE %s :Enforcing channel mode +%c (%d)", ME,
			chptr->chname, reopmode == CHFL_REOPLIST ? 'R' : 'r',
			now - chptr->reop);
		op.flags = MODE_ADD|MODE_CHANOP;
		change_chan_flag(&op, chptr);
		sendto_match_servs(chptr, NULL, ":%s MODE %s +o %s",
			ME, chptr->chname, op.value.cptr->name);
		sendto_channel_butserv(chptr, &me, ":%s MODE %s +o %s",
			ME, chptr->chname, op.value.cptr->name);
		chptr->reop = 0;
		ircstp->is_reop++;
		return 1;
	}
	return 0;
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
time_t	collect_channel_garbage(time_t now)
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

	for (; chptr; chptr = chptr->nextch)
	{
		if (chptr->users == 0)
		{
			curh_nb++;
		}
		else
		{
			Link	*tmp;

			cur_nb++;

			if (IsSplit())
			{
				if (chptr->reop > 0)
				{
					/* Extend reop */
					chptr->reop += CHECKFREQ;
				}
				continue;
			}
			if (chptr->reop == 0 || chptr->reop > now)
			{
				continue;
			}
			for (tmp = chptr->mlist; tmp; tmp = tmp->next)
			{
				if (tmp->flags == CHFL_REOPLIST)
				{
					break;
				}
			}
			if (tmp && tmp->flags == CHFL_REOPLIST)
			{
				r_cnt += reop_channel(now, chptr, CHFL_REOPLIST);
			}
			else if (chptr->mode.mode & MODE_REOP)
			{
				r_cnt += reop_channel(now, chptr, !CHFL_REOPLIST);
			}
		}
	}
	if (cur_nb > max_nb)
	{
		max_nb = cur_nb;
	}

	if (r_cnt > 0)
	{
		sendto_flag(SCH_CHAN, "Re-opped %u channel(s).", r_cnt);
	}

	/*
	** check for huge netsplits, if so, garbage collection is not really
	** done but make sure there aren't too many channels kept for
	** history - krys
	** This is dubious, but I'll leave it for now, until better split
	** detection. --B.
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
			sendto_flag(SCH_NOTICE,
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
	sendto_flag(SCH_NOTICE,
		   "Channel garbage: live %u (max %u), hist %u (removed %u)%s",
		    cur_nb - 1, max_nb - 1, curh_nb, del - istat.is_hchan,
		    (split) ? " split detected" : "");
#endif
	/* Check again after CHECKFREQ seconds */
	return (time_t) (now + CHECKFREQ);
}

