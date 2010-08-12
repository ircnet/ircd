/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
static const volatile char rcsid[] = "@(#)$Id: s_user.c,v 1.280 2010/08/12 16:29:30 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_USER_C
#include "s_externs.h"
#undef S_USER_C

static void	save_user (aClient *, aClient *, char *);

static char buf[BUFSIZE], buf2[BUFSIZE];

static int user_modes[]	     = { FLAGS_OPER, 'o',
				 FLAGS_LOCOP, 'O',
				 FLAGS_INVISIBLE, 'i',
				 FLAGS_WALLOP, 'w',
				 FLAGS_RESTRICT, 'r',
				 FLAGS_AWAY, 'a',
				 0, 0 };

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**
** Parameters:
**	aClient *next	First client to check
**	char	*ch	Search string (may include wilds)
**	      
*/
aClient	*next_client(aClient *next, char *ch)
{
	Reg	aClient	*tmp = next;

	next = find_client(ch, tmp);
	if (tmp && tmp->prev == next)
		return NULL;
	if (next != tmp)
		return next;
	for ( ; next; next = next->next)
		if (!match(ch,next->name) || !match(next->name,ch))
			break;
	return next;
}

/*
** hunt_server
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
*/
int	hunt_server(aClient *cptr, aClient *sptr, char *command, int server,
		int parc, char *parv[])
{
	aClient *acptr;

	/*
	** Assume it's me, if no server
	*/
	if (parc <= server || BadPtr(parv[server]) ||
	    match(ME, parv[server]) == 0 ||
	    match(parv[server], ME) == 0)
		return (HUNTED_ISME);
	/*
	** These are to pickup matches that would cause the following
	** message to go in the wrong direction while doing quick fast
	** non-matching lookups.
	*/
	if ((acptr = find_client(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	/* Match SID */
	if (!acptr && (acptr = find_sid(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	/* Match *.masked.servers */
	if (!acptr && (acptr = find_server(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	/* Remote services@servers */
	if (!acptr && (acptr = find_service(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr)
		for (acptr = client, (void)collapse(parv[server]);
		     (acptr = next_client(acptr, parv[server]));
		     acptr = acptr->next)
		    {
			if (acptr->from == sptr->from && !MyConnect(acptr))
				continue;
			/*
			 * Fix to prevent looping in case the parameter for
			 * some reason happens to match someone from the from
			 * link --jto
			 */
			if (IsRegistered(acptr) && (acptr != cptr))
				break;
		    }
	 if (acptr)
	    {
		if (!IsRegistered(acptr))
			return HUNTED_ISME;
		if (IsMe(acptr) || MyClient(acptr) || MyService(acptr))
			return HUNTED_ISME;
		if (match(acptr->name, parv[server]))
			parv[server] = acptr->name;
		if (IsService(sptr)
		    && (IsServer(acptr->from)
			&& match(sptr->service->dist,acptr->name) != 0
			&& match(sptr->service->dist,acptr->serv->sid) != 0))
		    {
			sendto_one(sptr, replies[ERR_NOSUCHSERVER], ME, BadTo(parv[0]), 
				   parv[server]);
			return(HUNTED_NOSUCH);
		    }
		sendto_one(acptr, command, parv[0],
			   parv[1], parv[2], parv[3], parv[4],
			   parv[5], parv[6], parv[7], parv[8]);
		return(HUNTED_PASS);
	    } 
	sendto_one(sptr, replies[ERR_NOSUCHSERVER], ME, BadTo(parv[0]), parv[server]);
	return(HUNTED_NOSUCH);
}

/*
** 'do_nick_name' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (0, if
**	nickname is illegal)
**
**  Nickname characters are in range
**	'A'..'}', '_', '-', '0'..'9'
**  anything outside the above set will terminate nickname.
**  In addition, the first character cannot be '-' or a digit.
**  Finally forbid the use of "anonymous" because of possible
**  abuses related to anonymous channnels. -kalt
**
*/

int	do_nick_name(char *nick, int server)
{
	Reg	char	*ch;

	if (*nick == '-') /* first character '-' */
		return 0;

	if (isdigit(*nick) && !server) /* first character in [0..9] */
		return 0;

	if (strcasecmp(nick, "anonymous") == 0)
		return 0;

#ifdef MINLOCALNICKLEN
	if (!server && nick[MINLOCALNICKLEN-1] == '\0')
		return 0;
#endif

	for (ch = nick; *ch && (ch-nick) < (server?NICKLEN:LOCALNICKLEN); ch++)
	{
		if (!isvalidnick(*ch))
		{
			break;
		}
	}

	*ch = '\0';

	return (ch - nick);
}


/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
*/
char	*canonize(char *buffer)
{
	static	char	cbuf[BUFSIZ];
	Reg	char	*s, *t, *cp = cbuf;
	Reg	int	l = 0;
	char	*p = NULL, *p2;

	*cp = '\0';

	for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
	    {
		if (l)
		    {
			for (p2 = NULL, t = strtoken(&p2, cbuf, ","); t;
			     t = strtoken(&p2, NULL, ","))
				if (!mycmp(s, t))
					break;
				else if (p2)
					p2[-1] = ',';
		    }
		else
			t = NULL;
		if (!t)
		    {
			if (l)
				*(cp-1) = ',';
			else
				l = 1;
			(void)strcpy(cp, s);
			if (p)
				cp += (p - s);
		    }
		else if (p2)
			p2[-1] = ',';
	    }
	return cbuf;
}

/*
** register_user
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the UNICK message is propagated.
*/

int	register_user(aClient *cptr, aClient *sptr, char *nick, char *username)
{
	Reg	aConfItem *aconf;
	aClient	*acptr;
	anUser	*user = sptr->user;
	char	*parv[3];
#ifndef NO_PREFIX
	char	prefix;
#endif
	int	i;
#ifdef XLINE
	static char savedusername[USERLEN+1];

	strncpyzt(savedusername, username, USERLEN+1);
#endif

	user->last = timeofday;
	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

	if (MyConnect(sptr))
	{
		char *reason = NULL;
#ifdef RESTRICT_USERNAMES
		char *lbuf = NULL;
#endif
#if defined(USE_IAUTH)
		static time_t last = 0;
		static u_int count = 0;
#endif
#ifdef XLINE
		aConfItem *xtmp;

		/* Just for clarification, so there's less confusion:
		   X-lines read from config have their fields stored
		   in aConf in the following fields:
		   host, passwd, name, name2, name3, source_ip.
		   (see s_conf.c/initconf(), look for XLINE);
		   these are used to check against USER and NICK
		   commands parameters during registration:
		   USER 1st 2nd 3rd :4th
		   NICK 5th
		   additionally user ip and/or hostname are
		   being matched against X-line last field
		   (conveniently kept in aconf->source_ip). --B. */

		for (xtmp = conf; xtmp; xtmp = xtmp->next)
		{
			if (xtmp->status != CONF_XLINE)
				continue;
			if (!BadPtr(xtmp->host) && 
				match(xtmp->host, username))
				continue;
			if (!BadPtr(xtmp->passwd) && 
				match(xtmp->passwd, sptr->user2))
				continue;
			if (!BadPtr(xtmp->name) && 
				match(xtmp->name, sptr->user3))
				continue;
			if (!BadPtr(xtmp->name2) && 
				match(xtmp->name2, sptr->info))
				continue;
			if (!BadPtr(xtmp->name3) && 
				match(xtmp->name3, nick))
				continue;
			if (!BadPtr(xtmp->source_ip) &&
				(match(xtmp->source_ip, (sptr->hostp ?
				sptr->hostp->h_name : sptr->sockhost)) &&
				match(xtmp->source_ip, sptr->user->sip) &&
				strchr(xtmp->source_ip, '/') && 
				match_ipmask(xtmp->source_ip, sptr, 0)))
				continue;
			SetXlined(sptr);
			break;
		}

		if (IsXlined(sptr))
		{
			sptr->exitc = EXITC_XLINE;
			return exit_client(cptr, sptr, &me,
				XLINE_EXIT_REASON);
		}
#endif

#if defined(USE_IAUTH)
		if (iauth_options & XOPT_EARLYPARSE && DoingXAuth(cptr))
		{
			cptr->flags |= FLAGS_WXAUTH;
			/* fool check_pings() and give iauth more time! */
			cptr->firsttime = timeofday;
			cptr->lasttime = timeofday;
			strncpyzt(sptr->user->username, username, USERLEN+1);
			if (sptr->passwd[0])
				sendto_iauth("%d P %s", sptr->fd, sptr->passwd);
			sendto_iauth("%d U %s", sptr->fd, sptr->user->username);
			return 1;
		}
		if (!DoneXAuth(sptr) && (iauth_options & XOPT_REQUIRED))
		{
			if (iauth_options & XOPT_NOTIMEOUT)
			{
				count += 1;
				if (timeofday - last > 300)
				    {
					sendto_flag(SCH_AUTH, 
	    "iauth may be not running! (refusing new user connections)");
					last = timeofday;
				    }
				sptr->exitc = EXITC_AUTHFAIL;
			}
			else
				sptr->exitc = EXITC_AUTHTOUT;
			return exit_client(cptr, cptr, &me,
				"Authentication failure! - no iauth?");
		}
		if (timeofday - last > 300 && count)
		{
			sendto_flag(SCH_AUTH, "%d users rejected.", count);
			count = 0;
		}

		/* this should not be needed, but there's a bug.. -kalt */
		/* haven't seen any notice like this, ever.. no bug no more? */
		if (*cptr->username == '\0')
		{
			sendto_flag(SCH_AUTH,
				    "Ouch! Null username for %s (%d %X)",
				    get_client_name(cptr, TRUE), cptr->fd,
				    cptr->flags);
			sendto_iauth("%d E Null username [%s] %X", cptr->fd,
				     get_client_name(cptr, TRUE), cptr->flags);
			return exit_client(cptr, sptr, &me,
					   "Fatal Bug - Try Again");
		}
#endif
#ifdef RESTRICT_USERNAMES
		/*
		** Do not allow special chars in the username.
		*/
		if ((sptr->flags & FLAGS_GOTID) &&
			! (*sptr->username == '-' ||
			index(sptr->username, '@')))
		{
			/* got ident and it is not OTHER
			 * (which could be encrypted), so check
			 * this one for validity */
			lbuf = sptr->username;
		}
		else
		{
			/* either no ident or ident OTHER */
			lbuf = username;
		}
		if (!isvalidusername(lbuf))
		{
			ircstp->is_ref++;
			sendto_flag(SCH_LOCAL, "Invalid username:  %s@%s.",
				lbuf, sptr->sockhost);
			sptr->exitc = EXITC_REF;
			return exit_client(cptr, sptr, &me, "Invalid username");
		}
#endif
		/*
		** the following insanity used to be after check_client()
		** but check_client()->attach_Iline() now needs to know the
		** username for global u@h limits.
		** moving this shit here shouldn't be a problem. -krys
		** what a piece of $#@!.. restricted can only be known
		** *after* attach_Iline(), so it matters and I have to move
		** come of it back below.  so global u@h limits really suck.
		*/
#ifndef	NO_PREFIX
		/*
		** ident is fun.. ahem
		** prefixes used:
		** 	none	I line with ident
		**	^	I line with OTHER type ident
		**	~	I line, no ident
		** 	+	i line with ident
		**	=	i line with OTHER type ident
		**	-	i line, no ident
		*/
		if (!(sptr->flags & FLAGS_GOTID))
			prefix = '~';
		else
			if (*sptr->username == '-' ||
			    index(sptr->username, '@'))
				prefix = '^';
			else
				prefix = '\0';

		/* OTHER type idents have '-' prefix (from s_auth.c),       */
		/* and they are not supposed to be used as userid (rfc1413) */
		/* @ isn't valid in usernames (m_user()) */
		if (sptr->flags & FLAGS_GOTID && *sptr->username != '-' &&
		    index(sptr->username, '@') == NULL)
			strncpyzt(buf2, sptr->username, USERLEN+1);
		else /* No ident, or unusable ident string */
		     /* because username may point to user->username */
			strncpyzt(buf2, username, USERLEN+1);

		if (prefix)
		{
			*user->username = prefix;
			strncpy(&user->username[1], buf2, USERLEN);
		}
		else
			strncpy(user->username, buf2, USERLEN+1);
		user->username[USERLEN] = '\0';
		/* eos */
#else
		strncpyzt(user->username, username, USERLEN+1);
#endif

		if (sptr->exitc == EXITC_AREF || sptr->exitc == EXITC_AREFQ)
		{
			if (sptr->exitc == EXITC_AREF)
				sendto_flag(SCH_LOCAL,
					    "Denied connection from %s.",
					    get_client_host(sptr));
			return exit_client(cptr, cptr, &me,
			         sptr->reason ? sptr->reason : "Denied access");
		}
		if ((i = check_client(sptr)))
		{
			struct msg_set { char shortm; char *longm; };
#define EXIT_MSG_COUNT 8
			static struct msg_set exit_msg[EXIT_MSG_COUNT] = {
			{ EXITC_BADPASS, "Bad password" },
			{ EXITC_GUHMAX, "Too many user connections (global)" },
			{ EXITC_GHMAX, "Too many host connections (global)" },
			{ EXITC_LUHMAX, "Too many user connections (local)" },
			{ EXITC_LHMAX, "Too many host connections (local)" },
			{ EXITC_YLINEMAX, "Too many connections" },
			{ EXITC_NOILINE, "Unauthorized connection" },
			{ EXITC_FAILURE, "Connect failure" } };

			if (i > -1)
			{
				i = -1;
			}
			i += EXIT_MSG_COUNT;
			if (i > EXIT_MSG_COUNT)
			{
				i = EXIT_MSG_COUNT;
			}
#undef EXIT_MSG_COUNT

			ircstp->is_ref++;
			sptr->exitc = exit_msg[i].shortm;
			if (exit_msg[i].shortm != EXITC_BADPASS)
			{
				sendto_flag(SCH_LOCAL, "%s from %s.",
					exit_msg[i].longm,
					get_client_host(sptr));
			}
			return exit_client(cptr, cptr, &me, exit_msg[i].longm);
		}
#ifndef	NO_PREFIX
		if (IsRestricted(sptr))
		{
			if (!(sptr->flags & FLAGS_GOTID))
				prefix = '-';
			else
				if (*sptr->username == '-' ||
				    index(sptr->username, '@'))
					prefix = '=';
				else
					prefix = '+';
			*user->username = prefix;
			strncpy(&user->username[1], buf2, USERLEN);
			user->username[USERLEN] = '\0';
		}
#endif

		aconf = sptr->confs->value.aconf;
#ifdef UNIXPORT
		if (IsUnixSocket(sptr))
		{
			strncpyzt(user->host, me.sockhost, HOSTLEN+1);
		}
		else
#endif
		{
			if (IsConfNoResolveMatch(aconf))
			{
				/* sockhost contains resolved hostname (if any),
				 * which we'll use in match_modeid to match. */
				strncpyzt(user->host, user->sip,
					HOSTLEN+1);
			}
			else
			{
				strncpyzt(user->host, sptr->sockhost,
					HOSTLEN+1);
			}
		}

		/* hmpf, why that? --B. */
		/* so already authenticated clients don't have password
		** kept in ircd memory? --B. */
		bzero(sptr->passwd, sizeof(sptr->passwd));
		/*
		 * following block for the benefit of time-dependent K:-lines
		 * how come "time-dependant"? I don't see it. --B.
		 */
		if (!IsKlineExempt(sptr) && find_kill(sptr, 0, &reason))
		{
			sendto_flag(SCH_LOCAL, "K-lined %s@%s.",
				    user->username, sptr->sockhost);
			ircstp->is_ref++;
			sptr->exitc = EXITC_KLINE;
			if (reason)
				sprintf(buf, "K-lined: %.80s", reason);
			return exit_client(cptr, sptr, &me,
				(reason) ? buf : "K-lined");
		}
	}
	else
	{
		/* Received from other server in UNICK */
		strncpyzt(user->username, username, USERLEN+1);
	}

	SetClient(sptr);
	if (!MyConnect(sptr))
	{
		acptr = find_server(user->server, NULL);
		if (acptr && acptr->from != cptr)
		{
			/* I think this is not possible anymore. --B. */
			sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
				ME, user->uid, ME, user->server,
				acptr->from->name, acptr->from->sockhost);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(cptr, sptr, &me,
				"UNICK server wrong direction");
		}
		send_umode(NULL, sptr, 0, SEND_UMODES, buf);
	}
	/* below !MyConnect, as it can remove user */
	if (IsInvisible(sptr))		/* Can be initialized in m_user() */
	{
		istat.is_user[1]++;	/* Local and server defaults +i */
		user->servp->usercnt[1]++;
	}
	else
	{
		user->servp->usercnt[0]++;
		istat.is_user[0]++;
	}
	
	check_split();
	
	if ((istat.is_user[1] + istat.is_user[0]) > istat.is_m_users)
	{

		istat.is_m_users = istat.is_user[1] + istat.is_user[0];
		if (timeofday - istat.is_m_users_t >= CLCHSEC)
		{
			sendto_flag(SCH_NOTICE, 
				"New highest global client connection: %d",
				istat.is_m_users);
			istat.is_m_users_t = timeofday;
		}
	}

	if (MyConnect(sptr))
	{
		char **isup;
		
		istat.is_unknown--;
		istat.is_myclnt++;
		if (istat.is_myclnt > istat.is_m_myclnt)
		{

			istat.is_m_myclnt = istat.is_myclnt;
			if (timeofday - istat.is_m_myclnt_t >= CLCHSEC)
			{
				sendto_flag(SCH_NOTICE,
					"New highest local client "
					"connection: %d", istat.is_m_myclnt);
				istat.is_m_myclnt_t = timeofday;
			}
		}
		if (istat.is_myclnt % CLCHNO == 0 &&
			istat.is_myclnt != istat.is_l_myclnt)
		{
			sendto_flag(SCH_NOTICE,
				"Local %screase from %d to %d clients "
				"in %d seconds",
				istat.is_myclnt>istat.is_l_myclnt?"in":"de",
				istat.is_l_myclnt, istat.is_myclnt,
				timeofday - istat.is_l_myclnt_t);
			istat.is_l_myclnt_t = timeofday;
			istat.is_l_myclnt = istat.is_myclnt;
		}
		strcpy(sptr->user->uid, next_uid());
		if (nick[0] == '0' && nick[1] == '\0')
		{
			strncpyzt(nick, sptr->user->uid, UIDLEN + 1);
			(void)strcpy(sptr->name, nick);
			(void)add_to_client_hash_table(nick, sptr);
		}
# if defined(CLIENTS_CHANNEL) && (CLIENTS_CHANNEL_LEVEL & CCL_CONN)
		sendto_flag(SCH_CLIENT, "%s %s %s %s CONN %s"
# if (CLIENTS_CHANNEL_LEVEL & CCL_CONNINFO)
#  ifdef XLINE
         " %s %s %s"
#  endif
			" :%s"
# endif
			, user->uid, nick, user->username,
			user->host, user->sip
# if (CLIENTS_CHANNEL_LEVEL & CCL_CONNINFO)
#  ifdef XLINE
         , savedusername, sptr->user2, sptr->user3
#  endif
			, sptr->info
# endif
			);
#endif
		sprintf(buf, "%s!%s@%s", nick, user->username, user->host);
		add_to_uid_hash_table(sptr->user->uid, sptr);
		sptr->exitc = EXITC_REG;
		sendto_one(sptr, replies[RPL_WELCOME], ME, BadTo(nick), buf);
		/* This is a duplicate of the NOTICE but see below...*/
		sendto_one(sptr, replies[RPL_YOURHOST], ME, BadTo(nick),
			   get_client_name(&me, FALSE), version);
		sendto_one(sptr, replies[RPL_CREATED], ME, BadTo(nick), creation);
		sendto_one(sptr, replies[RPL_MYINFO], ME, BadTo(parv[0]),
			   ME, version);
		
		isup = isupport;
		while (*isup)
		{
			sendto_one(sptr,replies[RPL_ISUPPORT], ME,
			BadTo(parv[0]),	*isup);
			isup++;
		}
		
		sendto_one(sptr, replies[RPL_YOURID], ME, BadTo(parv[0]),
			sptr->user->uid);
		(void)m_lusers(sptr, sptr, 1, parv);
		(void)m_motd(sptr, sptr, 1, parv);
		if (IsRestricted(sptr))
			sendto_one(sptr, replies[ERR_RESTRICTED], ME, BadTo(nick));
		if (IsConfNoResolve(sptr->confs->value.aconf))
		{
			sendto_one(sptr, ":%s NOTICE %s :Due to an administrative"
				" decision, your hostname is not shown.",
				ME, nick);
		}
		else if (IsConfNoResolveMatch(sptr->confs->value.aconf))
		{
			sendto_one(sptr, ":%s NOTICE %s :Due to an administrative"
				" decision, your hostname is not shown,"
				" but still matches channel MODEs.",
				ME, nick);
		}
		send_umode(sptr, sptr, 0, ALL_UMODES, buf);
		nextping = timeofday;
		
#ifdef SPLIT_CONNECT_NOTICE
		if (IsSplit())
		{
			sendto_one(sptr, ":%s NOTICE %s :%s", ME, nick,
				   SPLIT_CONNECT_NOTICE);
		}
#endif
	}

	for (i = fdas.highest; i >= 0; i--)
	{
		/* Find my leaf servers and feed the new client to them */
		if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr) ||
			acptr == cptr || IsMe(acptr))
		{
			continue;
		}
		sendto_one(acptr,
				":%s UNICK %s %s %s %s %s %s :%s",
				user->servp->sid, nick, user->uid,
				user->username, user->host, user->sip,
				(*buf) ? buf : "+", sptr->info);
	}	/* for(my-leaf-servers) */
#ifdef	USE_SERVICES
#if 0
	check_services_butone(SERVICE_WANT_NICK, user->servp, NULL,
			      "NICK %s :%d", nick, sptr->hopcount+1);
	check_services_butone(SERVICE_WANT_USER, user->servp, sptr,
			      ":%s USER %s %s %s :%s", nick, user->username, 
			      user->host, user->server, sptr->info);
	if (MyConnect(sptr))	/* all modes about local users */
		send_umode(NULL, sptr, 0, ALL_UMODES, buf);
	check_services_butone(SERVICE_WANT_UMODE, user->servp, sptr,
			      ":%s MODE %s :%s", nick, nick, buf);
#endif
	if (MyConnect(sptr))	/* all modes about local users */
		send_umode(NULL, sptr, 0, ALL_UMODES, buf);
	check_services_num(sptr, buf);
#endif
#ifdef USE_HOSTHASH
	add_to_hostname_hash_table(user->host, user);
#endif
#ifdef USE_IPHASH
	add_to_ip_hash_table(user->sip, user);
#endif
	return 1;
}

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
*/
int	m_nick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	int	delayed = 0;
	char	nick[NICKLEN+2], *user, *host;
	Link	*lp = NULL;
	int	allowednicklen;

	if (MyConnect(cptr) && IsUnknown(cptr) &&
		IsConfServeronly(cptr->acpt->confs->value.aconf))
	{
		sendto_flag(SCH_LOCAL, "User connection to server-only P-line "
			"from %s", get_client_host(cptr));
		find_bounce(cptr, -1, -1);
		return exit_client(cptr, cptr, &me, "Server only port");
	}
	if (IsService(sptr))
   	    {
		sendto_one(sptr, replies[ERR_ALREADYREGISTRED], ME, BadTo(parv[0]));
		return 1;
	    }

	/* local clients' nick size can be LOCALNICKLEN max */
	allowednicklen = MyConnect(sptr) ? LOCALNICKLEN : NICKLEN;
	strncpyzt(nick, parv[1], allowednicklen + 1);

	if (IsServer(cptr))
	{
		if (parc != 2)
		{
			char buf[BUFSIZE];
			int k;

badparamcountkills:
			sendto_flag(SCH_NOTICE,
				"Bad NICK param count (%d) for %s from %s via %s",
				parc, parv[1], sptr->name,
				get_client_name(cptr, FALSE));
			buf[0] = '\0';
			for (k = 1; k < parc; k++)
			{
				strcat(buf, " ");
				strcat(buf, parv[k]);
			}
			sendto_flag(SCH_ERROR,
				"Bad NICK param count (%d) from %s via %s: %s NICK%s",
				parc, sptr->name, 
				get_client_name(cptr, FALSE),
				parv[0], buf[0] ? buf : "");
			return 0;
		}
		else /* parc == 2 */
		{
			/* :old NICK new */
			user = sptr->user->username;
			host = sptr->user->host;
		}
	}
	else
	{
		if (sptr->user)
		{
			user = sptr->user->username;
			host = sptr->user->host;
		}
		else
			user = host = "";
	}
	/* Only local unregistered clients, I hope. --B. */
	if (MyConnect(sptr) && IsUnknown(sptr)
		&& nick[0] == '0' && nick[1] == '\0')
	{
		/* Allow registering with nick "0", this will be
		** overwritten in register_user() */
#ifdef DISABLE_NICK0_REGISTRATION
		sendto_one(sptr, replies[ERR_ERRONEOUSNICKNAME], ME,
			BadTo(parv[0]), nick);
		return 2;
#else
		goto nickkilldone;
#endif
	}

	if (MyPerson(sptr))
	{
		if (!strcmp(sptr->name, nick))
		{
			/*
			** This is just ':old NICK old' type thing.
			** Just forget the whole thing here. There is
			** no point forwarding it to anywhere.
			*/
			return 2; /* NICK Message ignored */
		}
		/* "NICK 0" or "NICK UID" received */
		if ((nick[0] == '0' && nick[1] == '\0') ||
		    !mycmp(nick, sptr->user->uid))
		{
			/* faster equivalent of
			** !strcmp(sptr->name, sptr->user->uid) */
			if (isdigit(sptr->name[0]))
			{
				/* user nick is already an UID */
				return 2; /* NICK message ignored */
			}
			else
			{
				/* user changing his nick to UID */
				strncpyzt(nick, sptr->user->uid, UIDLEN + 1);
				goto nickkilldone;
			}
		}
	}
	if (IsServer(cptr) && isdigit(nick[0]) 
		&& !strncasecmp(me.serv->sid, nick, SIDLEN))
	{
		/* Remote server send us remote user changing his nick
		** to uid-like with our! sid. Burn the bastard. --B. */
		sendto_flag(SCH_KILL, "Bad SID Nick Prefix: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
                sendto_serv_butone(NULL, ":%s KILL %s :%s (%s[%s] != %s)",
                                   me.name, sptr->user->uid, me.name,
                                   sptr->name, sptr->from->name,
                                   get_client_name(cptr, TRUE));
                sptr->flags |= FLAGS_KILLED;
                (void) exit_client(NULL, sptr, &me, "Fake SID Nick Prefix");
                return exit_client(NULL, cptr, &me, "Fake SID Nick Prefix");
	}
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * we have to KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick, IsServer(cptr)) == 0 ||
		strncmp(nick, parv[1], allowednicklen))
	{
		sendto_one(sptr, replies[ERR_ERRONEOUSNICKNAME], ME, BadTo(parv[0]),
			   parv[1]);

		if (IsServer(cptr))
		    {
			ircstp->is_kill++;
			sendto_flag(SCH_KILL, "Bad Nick: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
			if (sptr != cptr && sptr->user) /* bad nick change */
			    {
				sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
					   ME, sptr->user->uid, ME, parv[1],
					   nick, cptr->name);
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					ME, sptr->user->uid, ME,
					get_client_name(cptr, FALSE),
					parv[0], user, host);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			    }
		    }
		return 2;
	}

	if (!(acptr = find_client(nick, NULL)))
	    {
		aClient	*acptr2;

		if (IsServer(cptr) || !(bootopt & BOOT_PROT))
			goto nickkilldone;
		if ((acptr2 = get_history(nick, (long)(KILLCHASETIMELIMIT))) &&
		    !MyConnect(acptr2))
			/*
			** Lock nick for KCTL so one cannot nick collide
			** (due to kill chase) people who recently changed
			** their nicks. --Beeth
			*/
			delayed = 1;
		else
			/*
			** Let ND work
			*/
			delayed = find_history(nick, (long)(DELAYCHASETIMELIMIT));
		if (!delayed)
			goto nickkilldone;  /* No collisions, all clear... */
	    }
	/*
	** If acptr == sptr, then we have a client doing a nick
	** change between *equivalent* nicknames as far as server
	** is concerned (user is changing the case of his/her
	** nickname or somesuch)
	*/
	if (acptr == sptr)
	{
		/*
		** Allows change of case in his/her nick
		*/
		goto nickkilldone; /* -- go and process change */
	}
	/*
	** Note: From this point forward it can be assumed that
	** acptr != sptr (points to different client structures).
	*/
	/*
	** If the older one is "non-person", the new entry is just
	** allowed to overwrite it. Just silently drop non-person,
	** and proceed with the nick. This should take care of the
	** "dormant nick" way of generating collisions...
	*/
	if (acptr && IsUnknown(acptr) && MyConnect(acptr))
	    {
		(void) exit_client(acptr, acptr, &me, "Overridden");
		goto nickkilldone;
	    }
	/*
	** Decide, we really have a nick collision and deal with it
	*/
	if (!IsServer(cptr))
	    {
		/*
		** NICK is coming from local client connection. Just
		** send error reply and ignore the command.
		*/
		sendto_one(sptr, replies[(delayed) ? ERR_UNAVAILRESOURCE
						   : ERR_NICKNAMEINUSE],
					 ME, BadTo(parv[0]), nick);
		return 2; /* NICK message ignored */
	    }
	/*
	** NICK was coming from a server connection. Means that the same
	** nick is registered for different users by different server.
	** This is either a race condition (two users coming online about
	** same time, or net reconnecting) or just two net fragments becoming
	** joined and having same nicks in use. We cannot have TWO users with
	** same nick--purge this NICK from the system with a KILL... >;)
	**
	** Since 2.11, we SAVE both users, not KILL.
	*/

	/*
	** This seemingly obscure test (sptr == cptr) differentiates
	** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
	*/
	if (sptr == cptr)
	{
		/* 2.11+ do not introduce clients with NICK. --B. */
		goto badparamcountkills;
	}

	/*
	** Since it's not a new client, it must have an UID here,
	** so SAVE them both.
	*/
	/* assert(sptr->user!=NULL); assert(acptr->user!=NULL); */
	{
		char	path[BUFSIZE];

		/* Save acptr */
		sprintf(path, "(%s@%s[%s](%s) <- %s@%s[%s])",
			acptr->user->username,	acptr->user->host, 
			acptr->from->name, acptr->name,	user, host, cptr->name);
		save_user(NULL, acptr, path);

		/* Save sptr */
		sprintf(path, "(%s@%s[%s] <- %s@%s[%s](%s))",
			acptr->user->username, acptr->user->host,
			acptr->from->name, user, host, cptr->name, sptr->name);
		save_user(NULL, sptr, path);

		/* Everything is done */
		ircstp->is_save++;
		return 2;
	}

nickkilldone:
	if (IsServer(sptr))
	{
		/* 2.11+ do not introduce clients with NICK. --B. */
		goto badparamcountkills;
	}
	else if (sptr->name[0])		/* NICK received before, changing */
	{
		if (MyConnect(sptr))
		{
			if (!IsPerson(sptr))    /* Unregistered client */
				return 2;       /* Ignore new NICKs */
			/*
			** Restricted clients cannot change nicks
			** with exception of changing nick from SAVEd UID.
			** We could check for this earlier, so we would not
			** do all those checks, just return error. --Beeth
			*/
			if (IsRestricted(sptr) && !isdigit(*parv[0]))
			{
				sendto_one(sptr, replies[ERR_RESTRICTED],
					ME, BadTo(parv[0]));
				return 2;
			}
			/* Can the user speak on all channels? */
			for (lp = sptr->user->channel; lp; lp = lp->next)
				if (can_send(sptr, lp->value.chptr) &&
				    !IsQuiet(lp->value.chptr))
					break;
			/* If (lp), we give bigger penalty at the end. */
#ifdef DISABLE_NICKCHANGE_WHEN_BANNED
			if (lp)
			{
				sendto_one(sptr, replies[ERR_CANNOTSENDTOCHAN],
					ME, BadTo(parv[0]),
					lp->value.chptr->chname);
				return 2;
			}
#endif
		}
		/*
		** Client just changing his/her nick. If he/she is
		** on a channel, send note of change to all clients
		** on that channel. Propagate notice to other servers.
		*/
		sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
#if defined(CLIENTS_CHANNEL) && (CLIENTS_CHANNEL_LEVEL & CCL_NICK)
		if (MyConnect(sptr))
			sendto_flag(SCH_CLIENT, "%s %s %s %s NICK %s",
				sptr->user->uid, parv[0],
				sptr->user->username, sptr->user->host, nick);
#endif
		if (sptr->user) /* should always be true.. */
		{
			add_history(sptr, sptr);
#ifdef	USE_SERVICES
			check_services_butone(SERVICE_WANT_NICK,
					      sptr->user->servp, sptr,
					      ":%s NICK :%s", parv[0], nick);
#endif
		}
		else
			sendto_flag(SCH_NOTICE,
				    "Illegal NICK change: %s -> %s from %s",
				    parv[0], nick, get_client_name(cptr,TRUE));
		sendto_serv_butone(cptr, ":%s NICK :%s", sptr->user->uid, nick);
		if (sptr->name[0])
			(void)del_from_client_hash_table(sptr->name, sptr);
		(void)strcpy(sptr->name, nick);
	}
	else
	{
		/* Client setting NICK the first time */

		/* This had to be copied here to avoid problems.. */
		(void)strcpy(sptr->name, nick);
		if (sptr->user)
		{
			/*
			** USER already received, now we have NICK.
			** *NOTE* For servers "NICK" *must* precede the
			** user message (giving USER before NICK is possible
			** only for local client connection!). register_user
			** may reject the client and call exit_client for it
			** --must test this and exit m_nick too!!!
			*/
			if (register_user(cptr, sptr, nick,
					  sptr->user->username)
			    == FLUSH_BUFFER)
				return FLUSH_BUFFER;
		}
		/* If we returned from register_user, then the user registered
		** and sptr->name is not "0" anymore; add_to_client_hash_table
		** (few lines down) would be called second time for that client
		** because register_user does that for "NICK 0" after copying
		** UID onto client nick.
		** OTOH, if no USER was yet received, isdigit is true only for
		** client trying to register with "NICK 0". As we would have
		** returned without adding "0" to client hash table anyway,
		** we can safely return here. --B. */
		if (isdigit(sptr->name[0]))
		{
			return 3;
		}
	}
	/* Registered client doing "NICK 0" already has UID in sptr->name.
	** If sptr->name is "0" here, it means there was no "USER" (thanks
	** to that fiction's "isdigit" check above after register_user), so
	** do not add_to_client_hash_table with nick "0", let register_user
	** fill in UID first and add that to client hash table. --B. */
	if (sptr->name[0] != '0' || sptr->name[1] != '\0')
	{
		/* Finally set new nick name. */
		(void)add_to_client_hash_table(nick, sptr);
	}
	if (lp)
		return 15;
	else
		return 3;
}

/*
** m_unick
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2] = uid
**	parv[3] = username (login name, account)
**	parv[4] = client host name
**	parv[5] = client ip
**	parv[6] = users mode
**	parv[7] = users real name info
*/
int	m_unick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*uid, nick[NICKLEN+2], *user, *host, *realname;

	strncpyzt(nick, parv[1], NICKLEN+1);
	uid = parv[2];
	user = parv[3];
	host = parv[4];
	realname = parv[7];

	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * and KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick, 1) == 0 || strcmp(nick, parv[1]))
	{
		sendto_one(sptr, replies[ERR_ERRONEOUSNICKNAME], ME, BadTo(parv[0]),
			parv[1]);
		
		ircstp->is_kill++;
		sendto_flag(SCH_KILL, "Bad UNick: %s From: %s %s", parv[1],
			parv[0], get_client_name(cptr, FALSE));
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])", ME, uid, ME,
			parv[1], nick, cptr->name);
		return 2;
	}

	/*
	** Check validity of UID.
	*/
	if (!check_uid(uid, sptr->serv->sid))
	{
		/* This is so bad, that I really don't want to deal with it! */
		sendto_ops_butone(NULL, ME,
			"Bad UID (%s) from %s",
			uid, get_client_name(cptr, FALSE));
		return exit_client(cptr, cptr, &me, "Bad UID");
	}

	/*
	** Check against UID collisions,
	** they should never happen, but never say never.
	*/
	acptr = find_uid(uid, NULL);
	if (acptr)
	{
		/* This is so bad, that I really don't want to deal with it! */
		sendto_ops_butone(NULL, ME,
			"UID collision for %s from %s",
			uid, get_client_name(cptr, FALSE));
		return exit_client(cptr, cptr, &me, "UID collision");
	}

	/*
	** Check against NICK collisions,
	** and if possible save the two users.
	*/
	acptr = find_client(nick, NULL);
	if (acptr)
	    {
		/*
		** If the older one is "non-person", the new entry is just
		** allowed to overwrite it. Just silently drop non-person,
		** and proceed with the unick.
		*/
		if (IsUnknown(acptr) && MyConnect(acptr))
		{
			(void) exit_client(acptr, acptr, &me, "Overridden");
		}
		else
		/*
		** Ouch, this new client is trying to take an already
		** existing nickname..
		*/
		if (*acptr->user->uid)
		    {
			char	path[BUFSIZE];

			/* both users have a UID, save them */

			/* Save the other user.  Send it to all servers. */
			sprintf(path, "(%s@%s)%s <- (%s@%s)%s",
				acptr->user->username, acptr->user->host,
				acptr->user->server, user, host, cptr->name);
			save_user(NULL, acptr, path);

			/*
			** We need to send a SAVE for the new user back.
			** It isn't introduced yet, so we can't just call
			** save_user().
			*/
			sendto_one(cptr,
				":%s SAVE %s :%s (%s@%s)%s <- (%s@%s)%s", 
				me.serv->sid, uid, ME, 
				acptr->user->username, acptr->user->host,
				acptr->user->server, user, host, cptr->name);

			/* Just introduce him with the uid to the rest. */
			strcpy(nick, uid);
			ircstp->is_save++;
		    }
		else
		    {
			/*
			** sadly, both have to be killed because:
			** - we cannot rely on cptr do kill acptr
			**   as the new user introduced here may have changed
			**   nicknames before the collision happens on cptr
			** - sending a kill to cptr means nick chasing will
			**   get to the new user introduced here.
			*/
			sendto_one(acptr,
				   replies[ERR_NICKCOLLISION], ME, BadTo(acptr->name),
				   acptr->name, user, host);
			sendto_flag(SCH_KILL,
			    "Nick collision on %s (%s@%s)%s <- (%s@%s)%s",
				    acptr->name,
				    (acptr->user)?acptr->user->username:"???",
				    (acptr->user) ? acptr->user->host : "???",
				    acptr->from->name,
				    user, host, get_client_name(cptr, FALSE));
			ircstp->is_kill++;
			sendto_serv_butone(NULL, 
				   ":%s KILL %s :%s ((%s@%s)%s <- (%s@%s)%s)",
				   ME, acptr->name, ME,
				   (acptr->user) ? acptr->user->username:"???",
				   (acptr->user) ? acptr->user->host : "???",
				   acptr->from->name, user, host,
				   get_client_name(cptr, FALSE));
			acptr->flags |= FLAGS_KILLED;
			return exit_client(NULL, acptr, &me, "Nick collision");
		    }
	    }

	/*
	** the following is copied from m_user() as we need only
	** few things from it here, as we assume (should we?) it
	** is all ok since UNICK can come only from other server
	*/
	acptr = make_client(cptr);
	add_client_to_list(acptr);
	(void)make_user(acptr, strlen(parv[5]));
	/* more corrrect is this, but we don't yet have ->mask, so...
	acptr->user->servp = find_server_name(sptr->serv->mask->serv->snum);
	... just remember to change it one day --Beeth */
	acptr->user->servp = sptr->serv;
	sptr->serv->refcnt++;
	acptr->user->server = find_server_string(sptr->serv->snum);
	if (strlen(realname) > REALLEN)
	{
		realname[REALLEN] = '\0';
	}
	acptr->info = mystrdup(realname);
	strncpyzt(acptr->user->username, parv[3], USERLEN+1);
	strncpyzt(acptr->user->host, host, sizeof(acptr->user->host));
	reorder_client_in_list(acptr);

	/*
	** NOTE: acptr->name has to be set *after* calling m_user();
	** else it will already introduce the client which
	** can't have the uid set yet.
	** the extended m_nick() does things the other way around..
	** I hope this will not cause trouble sometime later. -kalt
	** You could do things like m_nick(), but you then you should
	** stop it from making a call to register_user().
	*/
	/* The nick is already checked for size, and is a local var. */
	strcpy(acptr->name, nick);
	add_to_client_hash_table(nick, acptr);
	/* we don't have hopcount in unick, we take it from sptr */
	acptr->hopcount = sptr->hopcount;
	/* The client is already killed if the uid is too long. */
	strcpy(acptr->user->uid, uid);
	strcpy(acptr->user->sip, parv[5]);
	add_to_uid_hash_table(uid, acptr);
	{
	    char	*pv[4];
	    
	    pv[0] = ME;
	    pv[1] = acptr->name;
	    pv[2] = parv[6];
	    pv[3] = NULL;
	    m_umode(NULL, acptr, 3, pv);
	}
	register_user(cptr, acptr, nick, user);
	return 0;
}

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/

static	int	m_message(aClient *cptr, aClient *sptr, int parc,
		char *parv[], int notice)
{
	Reg	aClient	*acptr;
	Reg	char	*s;
	aChannel *chptr;
	char	*nick, *server, *p, *cmd, *user, *host;
	int	count = 0, penalty = 0, syntax = 0;

	cmd = notice ? "NOTICE" : "PRIVMSG";

	if (MyConnect(sptr))
		parv[1] = canonize(parv[1]);
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	     nick = strtoken(&p, NULL, ","), penalty++)
	    {
		/*
		** restrict destination list to MAXPENALTY/2 recipients to
		** solve SPAM problem --Yegg 
		*/ 
		if (2*penalty >= MAXPENALTY) {
		    if (!notice)
			    sendto_one(sptr, replies[ERR_TOOMANYTARGETS],
				       ME, BadTo(parv[0]),
				       "Too many",nick,"No Message Delivered");
		    continue;      
		}   
		/*
		** nickname addressed?
		*/
		if (((IsServer(cptr) || IsService(cptr))
			&& (acptr = find_uid(nick, NULL))) || 
			(acptr = find_person(nick, NULL)))
		    {
			if (!notice && MyConnect(sptr) &&
			    acptr->user && (acptr->user->flags & FLAGS_AWAY))
				send_away(sptr, acptr);
			sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
					  parv[0], cmd, acptr->name, parv[2]);
			continue;
		    }
		/*
		** channel msg?
		*/
		if ((IsPerson(sptr) || IsService(sptr) || IsServer(sptr)) &&
		    (chptr = find_channel(nick, NullChn)))
		    {
			if (can_send(sptr, chptr) == 0 || IsServer(sptr))
				sendto_channel_butone(cptr, sptr, chptr,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			else if (!notice)
				sendto_one(sptr, replies[ERR_CANNOTSENDTOCHAN],
					   ME, BadTo(parv[0]), nick);
			continue;
		    }
	
		/*
		** the following two cases allow masks in NOTICEs
		** (for OPERs only)
		**
		** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		**
		** NOTE: 2.11 changed syntax from $/#-mask to $$/$#-mask
		*/
		if (IsAnOper(sptr))
		{
			if (*nick == '$')
			{
				if (*(nick+1) == '$')
				{
					syntax = MATCH_SERVER;
				}
				else if (*(nick+1) == '#')
				{
					syntax = MATCH_HOST;
				}
			}
		}
		if (syntax)
		{
			if (!(s = (char *)rindex(nick, '.')))
			{
				sendto_one(sptr, replies[ERR_NOTOPLEVEL],
					   ME, BadTo(parv[0]), nick);
				continue;
			}
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			{
				sendto_one(sptr, replies[ERR_WILDTOPLEVEL],
					ME, BadTo(parv[0]), nick);
				continue;
			}
			sendto_match_butone(
				IsServer(cptr) ? cptr : NULL, 
				sptr, nick + 2, syntax,
				":%s %s %s :%s", parv[0],
				cmd, nick, parv[2]);
			continue;
		}	/* syntax */

		/*
		** nick!user@host addressed?
		*/
		if ((user = (char *)index(nick, '!')) &&
		    (host = (char *)index(nick, '@')))
		    {
			*user = '\0';
			*host = '\0';
			if ((acptr = find_person(nick, NULL)) &&
			    !mycmp(user+1, acptr->user->username) &&
			    !mycmp(host+1, acptr->user->host))
			    {
				sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
						  parv[0], cmd, nick, parv[2]);
				*user = '!';
				*host = '@';
				continue;
			    }
			*user = '!';
			*host = '@';
		    }

		/*
		** user[%host]@server addressed?
		*/
		if ((server = (char *)index(nick, '@')) &&
		    (acptr = find_server(server + 1, NULL)))
		    {
			/*
			** Not destined for a user on me :-(
			*/
			if (!IsMe(acptr))
			    {
				sendto_one(acptr,":%s %s %s :%s", parv[0],
					   cmd, nick, parv[2]);
				continue;
			    }
			*server = '\0';

			if ((host = (char *)index(nick, '%')))
				*host++ = '\0';

			/*
			** Look for users which match the destination host
			** (no host == wildcard) and if one and one only is
			** found connected to me, deliver message!
			*/
			acptr = find_userhost(nick, host, NULL, &count);
			if (server)
				*server = '@';
			if (host)
				*--host = '%';
			if (acptr)
			    {
				if (count == 1)
					sendto_prefix_one(acptr, sptr,
							  ":%s %s %s :%s",
					 		  parv[0], cmd,
							  nick, parv[2]);
				else if (!notice)
					sendto_one(sptr, replies[ERR_TOOMANYTARGETS],
						   ME, BadTo(parv[0]), "Duplicate", nick,
						   "No Message Delivered");
				continue;
			    }
		    }
		else if ((host = (char *)index(nick, '%')))
		    {
			/*
			** user%host addressed?
			*/
			*host++ = '\0';
			acptr = find_userhost(nick, host, NULL, &count);
			*--host = '%';
			if (acptr)
			    {
				if (count == 1)
					sendto_prefix_one(acptr, sptr,
							  ":%s %s %s :%s",
					 		  parv[0], cmd,
							  nick, parv[2]);
				else if (!notice)
					sendto_one(sptr, replies[ERR_TOOMANYTARGETS],
						   ME, BadTo(parv[0]), "Duplicate", nick,
						   "No Message Delivered");
				continue;
			    }
		    }
		if (!notice)
			sendto_one(sptr, replies[ERR_NOSUCHNICK], ME, BadTo(parv[0]),
				   nick);
	    }
    return penalty;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

int	m_private(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int	m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 1);
}

/*
** who_one
**	sends one RPL_WHOREPLY to sptr concerning acptr & repchan
*/
static	void	who_one(aClient *sptr, aClient *acptr, aChannel *repchan,
		Link *lp)
{
	char	status[5];
	int	i = 0;

	if (acptr->user->flags & FLAGS_AWAY)
		status[i++] = 'G';
	else
		status[i++] = 'H';
	if (IsAnOper(acptr))
		status[i++] = '*';
	if ((repchan != NULL) && (lp == NULL))
		lp = find_user_link(repchan->members, acptr);
	if (lp != NULL)
	    {
		if (lp->flags & CHFL_CHANOP)
			status[i++] = '@';
		else if (lp->flags & CHFL_VOICE)
			status[i++] = '+';
	    }
	status[i] = '\0';
	sendto_one(sptr, replies[RPL_WHOREPLY], ME, BadTo(sptr->name),
		   (repchan) ? (repchan->chname) : "*", acptr->user->username,
		   acptr->user->host, acptr->user->server, acptr->name,
		   status, acptr->hopcount, acptr->user->servp->sid, acptr->info);
}


/*
** who_channel
**	lists all users on a given channel
*/
static	void	who_channel(aClient *sptr, aChannel *chptr, int oper)
{
	Reg	Link	*lp;
	int	member;

	if (!IsAnonymous(chptr))
	{
		member = IsMember(sptr, chptr);
		if (member || !SecretChannel(chptr))
		{
			for (lp = chptr->members; lp; lp = lp->next)
			{
				if (oper && !IsAnOper(lp->value.cptr))
				{
					continue;
				}
				if (IsInvisible(lp->value.cptr) && !member)
				{
					continue;
				}
				who_one(sptr, lp->value.cptr, chptr, lp);
			}
		}
	}
	else if ((lp = find_user_link(chptr->members, sptr)))
	{
		who_one(sptr, lp->value.cptr, chptr, lp);
	}
}

/*
** who_find
**	lists all (matching) users.
**	CPU intensive, but what can be done?
**	
**	Reduced CPU load - 05/2001
*/
static	void	who_find(aClient *sptr, char *mask, int oper)
{
	aChannel *chptr = NULL;
	Link	*lp,*lp2;
	aClient	*acptr;
	
	/* first, show INvisible matching users on common channels */
	if (sptr->user) /* service can request who as well */
	for (lp = sptr->user->channel; lp ;lp = lp->next)
	{
		chptr = lp->value.chptr;
		if (IsAnonymous(chptr))
			continue;
		for (lp2 = chptr->members; lp2 ;lp2 = lp2->next)
		{
			acptr = lp2->value.cptr;
			
			if (!IsInvisible(acptr)
			    || (acptr->flags & FLAGS_HIDDEN))
			{
				continue;
			}
			
			if (oper && !IsAnOper(acptr))
			{
				continue;
			}
			
			/* Mark user with FLAGS_HIDDEN to prevent multiple
			 * checking.
			 */
			
			acptr->flags |= FLAGS_HIDDEN;
			if (!mask ||
			     match(mask, acptr->name) == 0 ||
			     match(mask, acptr->user->username) == 0 ||
			     match(mask, acptr->user->host) == 0 ||
			     match(mask, acptr->user->server) == 0 ||
			     match(mask, acptr->info) == 0)
				who_one(sptr, acptr, chptr, NULL);
		
		}
	}

	for (acptr = client; acptr; acptr = acptr->next)
	{
			
		if (!IsPerson(acptr))
			continue;
		
		/* clear the flag */
		if (acptr->flags & FLAGS_HIDDEN)
		{
			acptr->flags &= ~FLAGS_HIDDEN;
			continue;
		}
		
		/* allow local opers to see matching clients
		 * on _LOCAL_ server and show the user himself */
		if (IsInvisible(acptr) && (acptr != sptr)
		    && !(MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
		   )
		{
			continue;
		}
		
		/* we wanted only opers */
		if (oper && !IsAnOper(acptr))
		{
			continue;
		}

		/*
		** This is brute force solution, not efficient...? ;(
		** Show entry, if no mask or any of the fields match
		** the mask. --msa
		*/
		if (!mask ||
		     match(mask, acptr->name) == 0 ||
		     match(mask, acptr->user->username) == 0 ||
		     match(mask, acptr->user->host) == 0 ||
		     match(mask, acptr->user->server) == 0 ||
		     match(mask, acptr->info) == 0)
			who_one(sptr, acptr, NULL, NULL);
	}
	
}


/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
int	m_who(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aChannel *chptr;
	int	oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int	penalty = 0;
	char	*p, *mask, *channame;

	if (parc < 2)
	{
		who_find(sptr, NULL, oper);
		sendto_one(sptr, replies[RPL_ENDOFWHO], ME, BadTo(parv[0]), "*");
		/* it was very CPU intensive */
		return MAXPENALTY;
	}

	/* get rid of duplicates */
	parv[1] = canonize(parv[1]);

	for (p = NULL, mask = strtoken(&p, parv[1], ",");
	    mask && penalty <= MAXPENALTY;
		mask = strtoken(&p, NULL, ","))
	{ 
		channame = NULL;
		penalty += 1;

		/* find channel user last joined, we might need it later */
		if (sptr->user && sptr->user->channel)
			channame = sptr->user->channel->value.chptr->chname;

		if (clean_channelname(mask) == -1)
			/* maybe we should tell user? --B. */
			continue;

		/*
		** We can never have here !mask 
		** or *mask == '\0', since it would be equal
		** to parc == 1, that is 'WHO' and/or would not
		** pass through above for loop.
		*/
		if (mask[1] == '\0' && mask[0] == '0')
		{
			/*
			** 'WHO 0' - do who_find() later
			*/
			mask = NULL;
			channame = NULL;
		}
		else if (mask[1] == '\0' && mask[0] == '*')
		{
			/*
			** 'WHO *'
			** If user was on any channel, list the one
			** he joined last.
			*/
			mask = NULL;
		}
		else
		{
			/*
			** Try if mask was channelname and if yes, do
			** who_channel, else if mask was nick, do who_one.
			** Else do horrible who_find()
			*/
			channame = mask;
		}
		
		if (IsChannelName(channame))
		{
			chptr = find_channel(channame, NULL);
			if (chptr)
			{
				who_channel(sptr, chptr, oper);
			}
			else
			{
				/*
				** 'WHO #nonexistant'.
				*/
				penalty += 1;
			}
		}
		else 
		{
			aClient	*acptr = NULL;

			if (mask)
			{
				/*
				** Here mask can be NULL. It doesn't matter,
				** since find_client would return NULL.
				** Just saving one function call. ;)
				*/
				acptr = find_client(mask, NULL);
				if (acptr && !IsClient(acptr))
				{
					acptr = NULL;
				}
			}
			if (acptr)
			{
				/* We found client, so send WHO for it */
				who_one(sptr, acptr, NULL, NULL);
			}
			else
			{
				/*
				** All nice chances lost above.
				** We must hog our server with that.
				*/
				
				/* simplify mask */
				(void)collapse(mask);

				who_find(sptr, mask, oper);
				penalty += MAXPENALTY;
			}
		}
		sendto_one(sptr, replies[RPL_ENDOFWHO], ME, BadTo(parv[0]),
			   BadPtr(mask) ?  "*" : mask);
	}
	return penalty;
}

/* send_whois() is used by m_whois() to send whois reply to sptr, for acptr */
static	void	send_whois(aClient *sptr, aClient *acptr)
{
	static anUser UnknownUser =
	    {
		NULL,	/* channel */
		NULL,   /* invited */
		NULL,	/* uwas */
		NULL,	/* away */
		0,	/* last */
		1,      /* refcount */
		0,	/* joined */
		0,	/* flags */
		NULL,	/* servp */
		0, NULL, NULL,	/* hashc, uhnext, bcptr */
		"<Unknown>",	/* user */
		"0",		/* uid */
		"<Unknown>",	/* host */
		"<Unknown>",	/* server */
	    };
	Link	*lp;
	anUser	*user;
	aChannel *chptr;
	aClient *a2cptr;
	int len, mlen;
	char *name;

	user = acptr->user ? acptr->user : &UnknownUser;
	name = (!*acptr->name) ? "?" : acptr->name;

	a2cptr = find_server(user->server, NULL);

	sendto_one(sptr, replies[RPL_WHOISUSER], ME, BadTo(sptr->name),
		   name, user->username, user->host, acptr->info);

	mlen = strlen(ME) + strlen(sptr->name) + 6 + strlen(name);

	for (len = 0, *buf = '\0', lp = user->channel; lp; lp = lp->next)
	    {
		chptr = lp->value.chptr;
		if ((!IsAnonymous(chptr) || acptr == sptr) &&
		    ShowChannel(sptr, chptr))
		    {
			if (len + strlen(chptr->chname)
			    > (size_t) BUFSIZE - 4 - mlen)
			    {
				sendto_one(sptr, ":%s %d %s %s :%s", ME,
					   RPL_WHOISCHANNELS, sptr->name, name,
					   buf);
				*buf = '\0';
				len = 0;
			    }
			if (is_chan_op(acptr, chptr))
				*(buf + len++) = '@';
			else if (has_voice(acptr, chptr))
				*(buf + len++) = '+';
			if (len)
				*(buf + len) = '\0';
			(void)strcpy(buf + len, chptr->chname);
			len += strlen(chptr->chname);
			(void)strcat(buf + len, " ");
			len++;
		    }
	    }
	if (buf[0] != '\0')
		sendto_one(sptr, replies[RPL_WHOISCHANNELS], ME, BadTo(sptr->name), name,
			   buf);

	sendto_one(sptr, replies[RPL_WHOISSERVER], ME, BadTo(sptr->name),
		   name, user->server,
		   a2cptr ? a2cptr->info:"*Not On This Net*");

	if (user->flags & FLAGS_AWAY)
		send_away(sptr, user->bcptr);

	if (IsAnOper(acptr))
		sendto_one(sptr, replies[RPL_WHOISOPERATOR], ME, BadTo(sptr->name), name);

	if (acptr->user && MyConnect(acptr))
		sendto_one(sptr, replies[RPL_WHOISIDLE], ME, BadTo(sptr->name),
			   name, (long)(timeofday - user->last)
#ifdef WHOIS_SIGNON_TIME
			, (long)acptr->firsttime
#endif
			);
}

/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
int	m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Link	*lp;
	aClient *acptr;
	aChannel *chptr;
	char	*nick, *tmp, *tmp2;
	char	*p = NULL;
	int	found = 0;

    	if (parc < 2)
	    {
		sendto_one(sptr, replies[ERR_NONICKNAMEGIVEN], ME, BadTo(parv[0]));
		return 1;
	    }

	if (parc > 2)
	    {
		if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1,parc,parv) !=
		    HUNTED_ISME)
			return 3;
		parv[1] = parv[2];
	    }

	tmp = mystrdup(parv[1]);

	for (tmp2 = canonize(tmp); (nick = strtoken(&p, tmp2, ",")); 
		tmp2 = NULL)
	    {
		int	invis, showperson, member, wilds;

		found &= 0x0f;	/* high/boolean, low/counter */
		(void)collapse(nick);
		wilds = (index(nick, '?') || index(nick, '*'));
		/*
		 * We're no longer allowing remote users to generate
		 * requests with wildcard, nor local users with more
		 * than one wildcard target per command.
		 * Max 3 targets per command allowed.
		 */
		if ((wilds && (!MyConnect(sptr) || p)) || found++ > 3)
			break;

		if (!wilds)
		    {
			acptr = hash_find_client(nick, (aClient *)NULL);
			if (!acptr || !IsPerson(acptr))
				sendto_one(sptr,
					   replies[ERR_NOSUCHNICK], ME, BadTo(parv[0]),
					   nick);
			else
				send_whois(sptr, acptr);
			continue;
		    }

		for (acptr = client; (acptr = next_client(acptr, nick));
		     acptr = acptr->next)
		    {
			if (IsServer(acptr) || IsService(acptr))
				continue;
			/*
			 * I'm always last :-) and acptr->next == NULL!!
			 */
			if (IsMe(acptr))
				break;
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 * - if wildcards are being used don't send a reply if
			 *   the querier isnt any common channels and the
			 *   client in question is invisible and wildcards are
			 *   in use (allow exact matches only);
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */
			invis = (acptr->user) ?
				(acptr->user->flags & FLAGS_INVISIBLE) : 0;
			member = (acptr->user && acptr->user->channel) ? 1 : 0;
			showperson = (wilds && !invis && !member) || !wilds;
			for (lp = (acptr->user) ? acptr->user->channel : NULL;
			     lp; lp = lp->next)
			    {
				chptr = lp->value.chptr;
				if (IsAnonymous(chptr))
					continue;
				member = IsMember(sptr, chptr);
				if (invis && !member)
					continue;
				if (member || (!invis && PubChannel(chptr)))
				    {
					showperson = 1;
					break;
				    }
				if (!invis && HiddenChannel(chptr) &&
				    !SecretChannel(chptr))
					showperson = 1;
			    }
			if (!showperson)
				continue;

			found |= 0x10;

			send_whois(sptr, acptr);
		    }
		if (!(found & 0x10))
		    {
			if (strlen(nick) > (size_t) NICKLEN)
				nick[NICKLEN] = '\0';
			sendto_one(sptr, replies[ERR_NOSUCHNICK], ME, BadTo(parv[0]),
				   nick);
		    }
		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, replies[RPL_ENDOFWHOIS], ME, BadTo(parv[0]), parv[1]);

	MyFree(tmp);

	return 2;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = user modes
**	parv[3] = unused
**	parv[4] = real name info
**
** NOTE: As of 2.11.1 we no longer call m_user() internally. --B.
*/
int	m_user(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
#define	UFLAGS	(FLAGS_INVISIBLE|FLAGS_WALLOP|FLAGS_RESTRICT)
	struct umodes_arr_s
	{
		char umode;
		int flag;
	} umodes_arr[] =
	{ {'i', FLAGS_INVISIBLE},
	  {'r', FLAGS_RESTRICT},
	  {'w', FLAGS_WALLOP},
	  {'\0', 0}
	};

	char	*username, *umodes, *server, *realname;
	anUser	*user;
	char	ipbuf[BUFSIZE];
	int	what,i;
	char 	*s;

	if (MyConnect(cptr) && IsUnknown(cptr) &&
		IsConfServeronly(cptr->acpt->confs->value.aconf))
	{
		sendto_flag(SCH_LOCAL, "User connection to server-only P-line "
			"from %s", get_client_host(cptr));
		find_bounce(cptr, -1, -1);
		return exit_client(cptr, cptr, &me, "Server only port");
	}
	/* Reject new USER */
	if (IsServer(sptr) || IsService(sptr) || sptr->user)
	    {
		sendto_one(sptr, replies[ERR_ALREADYREGISTRED], ME, BadTo(parv[0]));
		return 1;
   	    }
	if ((username = (char *)index(parv[1],'@')))
		*username = '\0'; 

	/* Copy parameters into better documenting variables */

	username = parv[1];
	umodes   = parv[2];
	server   = parv[3];
	realname = parv[4];
	
#ifdef INET6
	inetntop(AF_INET6, (char *)&sptr->ip, ipbuf, sizeof(ipbuf));
#else
	strcpy(ipbuf, (char *)inetntoa((char *)&sptr->ip));
#endif
	user = make_user(sptr, strlen(ipbuf));
	strcpy(user->sip, ipbuf);

	user->servp = me.serv;
	me.serv->refcnt++;
#ifdef	DEFAULT_INVISIBLE
	SetInvisible(sptr);
#endif
	/* parse desired user modes sent in USER */
	/* rfc behaviour - bits */
	if (isdigit(*umodes))
	{
		for (s = umodes+1; *s; s++)
			if (!isdigit(*s))
				break;
		if (*s == '\0')
			/* allows only umodes specified in UFLAGS - see above */
			sptr->user->flags |= (UFLAGS & atoi(umodes));
	}
	else	/* new behaviour */
	{
		/* 0 here is intentional. User MUST specify + or -,
		 * as we don't want to restrict (broken) clients which send
		 * their hostname in mode field (and happen to have r there).
		 * - jv
		 */
		what = 0;
		for (s = umodes; *s; s++)
		{
			switch (*s)
			{
				case '+':
					what = MODE_ADD;
					continue;
				case '-':
					what = MODE_DEL;
					continue;
				default:
					break;
			}
			/* If mode does not start with - or +, don't bother. */
			if (what == 0)
				break;
			for (i = 0; umodes_arr[i].umode != '\0'; i++)
			{
				if (*s == umodes_arr[i].umode)
				{
					if (what == MODE_ADD)
					{
						sptr->user->flags |=
							umodes_arr[i].flag;
					}
					if (what == MODE_DEL)
					{
						sptr->user->flags &=
							~(umodes_arr[i].flag);
					}
				}
			}
		}
	}
	user->server = find_server_string(me.serv->snum);
	
	reorder_client_in_list(sptr);
	if (sptr->info != DefInfo)
		MyFree(sptr->info);
	if (strlen(realname) > REALLEN)
		realname[REALLEN] = '\0';
	sptr->info = mystrdup(realname);
#ifdef XLINE
	sptr->user2 = mystrdup(umodes);
	sptr->user3 = mystrdup(server);
#endif
	if (sptr->name[0]) /* NICK already received, now we have USER... */
	{
		return register_user(cptr, sptr, sptr->name, username);
	}
	else
	{
		strncpyzt(sptr->user->username, username, USERLEN+1);
	}
	return 2;
}

/* Fear www proxy abusers... aliased to QUIT, muhaha --B. */
int	m_post(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	sendto_flag(SCH_LOCAL, "Denied http-post connection from %s.",
		cptr->sockhost);
	return m_quit(cptr, sptr, parc, parv);
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static	char comment[TOPICLEN+1];

	if (IsServer(sptr))
		return 0;

	if (MyConnect(sptr))
	{
		(void) snprintf(comment, TOPICLEN, "\"%s",
			(parc > 1 && parv[1]) ? parv[1] : "");
		(void) strcat(comment, "\"");
	}
	else
	{
		(void) snprintf(comment, TOPICLEN + 1, "%s",
			(parc > 1 && parv[1]) ? parv[1] : "");
	}
	return exit_client(cptr, sptr, sptr, comment);
}

/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim
**	parv[2] = kill path
*/
int	m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr = NULL;
	char	*inpath = cptr->name;
	char	*user, *path, *killer;
	int	chasing = 0;

	if (!is_allowed(sptr, ACL_KILL))
		return m_nopriv(cptr, sptr, parc, parv);

	user = parv[1];
	path = parv[2];

	if (IsAnOper(cptr) && strlen(path) > (size_t) TOPICLEN)
		path[TOPICLEN] = '\0';

	/* first, _if coming from a server_ check for kill on UID */
	if (IsServer(cptr))
		acptr = find_uid(user, NULL);
	if (acptr == NULL)
		acptr = find_client(user, NULL);
	if (acptr == NULL)
	    {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = get_history(user, (long)KILLCHASETIMELIMIT)))
		    {
			if (!IsServer(sptr))
				sendto_one(sptr, replies[ERR_NOSUCHNICK], 
					   ME, BadTo(parv[0]), user);
			return 1;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   ME, parv[0], user, acptr->name);
		chasing = 1;
	    }
	if (!MyConnect(acptr) && !is_allowed(cptr, ACL_KILLREMOTE))
	    {
		return m_nopriv(cptr, sptr, parc, parv);
	    }
	if (IsServer(acptr) || IsMe(acptr))
	    {
		sendto_flag(SCH_ERROR, "%s tried to KILL server %s: %s %s %s",
			    sptr->name, acptr->name, parv[0], parv[1], parv[2]);
		sendto_one(sptr, replies[ERR_CANTKILLSERVER], ME, BadTo(parv[0]),
			   acptr->name);
		return 1;
	    }
	if (!IsServer(cptr))
	    {
		/*
		** The kill originates from this server, initialize path.
		** (In which case the 'path' may contain user suplied
		** explanation ...or some nasty comment, sigh... >;-)
		**
		**	...!operhost!oper
		**	...!operhost!oper (comment)
		*/
#ifdef UNIXPORT
		if (IsUnixSocket(cptr)) /* Don't use get_client_name syntax */
			inpath = ME;
		else
#endif
			inpath = cptr->sockhost;
		sprintf(buf, "%s%s (%s)",
			cptr->name, IsOper(sptr) ? "" : "(L)", path);
		path = buf;
	    }
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/
	if (IsService(acptr))
	{
		sendto_flag(SCH_KILL, "Received KILL message for %s[%s]. "
			"From %s Path: %s!%s", acptr->name, 
			isdigit(acptr->service->servp->sid[0]) ?
			acptr->service->servp->sid : "2.10", parv[0], inpath,
			path);
	}
	else
	{
		sendto_flag(SCH_KILL, "Received KILL message for "
			"%s!%s@%s[%s/%s]. From %s Path: %s!%s",
			acptr->name, acptr->user->username, acptr->user->host,
			acptr->user->servp->bcptr->name, 
			isdigit(acptr->user->servp->sid[0]) ?
			acptr->user->servp->sid : "2.10", parv[0], inpath,
			path);
	}
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
	{
		if (IsService(acptr))
		{
			syslog(LOG_DEBUG, "KILL From %s For %s[%s] Path %s!%s",
				parv[0], acptr->name, 
				isdigit(acptr->service->servp->sid[0]) ?
				acptr->service->servp->sid : "2.10",
				inpath, path);
		}
		else
		{
			syslog(LOG_DEBUG, "KILL From %s For %s!%s@%s[%s/%s] "
				"Path %s!%s", parv[0], acptr->name, 
				acptr->user->username, acptr->user->host,
				acptr->user->servp->bcptr->name, 
				isdigit(acptr->user->servp->sid[0]) ?
				acptr->user->servp->sid : "2.10",
				inpath, path);
		}
	}
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
	    {
		if (acptr->user)
		    {
			sendto_serv_v(cptr, SV_UID, ":%s KILL %s :%s!%s",
				      parv[0], acptr->user->uid, inpath, path);
		    }
		else
			sendto_serv_butone(cptr, ":%s KILL %s :%s!%s",
					   parv[0], acptr->name, inpath, path);
		if (chasing && !IsClient(cptr))
			sendto_one(cptr, ":%s KILL %s :%s!%s",
				   ME, acptr->name, inpath, path);
		acptr->flags |= FLAGS_KILLED;
	    }
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_KILL, NULL, sptr, 
			      ":%s KILL %s :%s!%s", parv[0], acptr->name,
			      inpath, path);
#endif

	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_prefix_one(acptr, sptr,":%s KILL %s :%s!%s",
				  parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents exit_one_client from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	if (MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
	    {
		acptr->exitc = EXITC_KILL;
		sprintf(buf2, "Local Kill by %s (%s)", sptr->name, parv[2]);
	    }
	else
	    {
		if ((killer = index(path, ' ')))
		    {
			while (killer > path && *killer != '!')
				killer--;
			if (killer != path)
				killer++;
		    }
		else
			killer = path;
		sprintf(buf2, "Killed (%s)", killer);
	    }
	return exit_client(cptr, acptr, sptr, buf2);
}

/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *	    Not currently really working, I don't like this
 *	    call at all...
 *
 *	    ...trying to make it work. I don't like it either,
 *	      but perhaps it's worth the load it causes to net.
 *	      This requires flooding of the whole net like NICK,
 *	      USER, MODE, etc messages...  --msa
 ***********************************************************************/

/*
** m_away
**	parv[0] = sender prefix
**	parv[1] = away message
*/
int	m_away(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg	char	*away, *awy2 = parv[1];
	int	len;

	away = sptr->user->away;

	if (parc < 2 || !*awy2)	/* Marking as not away */
	    {
		if (away)
		    {
			istat.is_away--;
			istat.is_awaymem -= (strlen(away) + 1);
			MyFree(away);
			sptr->user->away = NULL;
		    }
		if (sptr->user->flags & FLAGS_AWAY)
			sendto_serv_butone(cptr, ":%s MODE %s :-a",
				sptr->user->uid, parv[0]);
		/* sendto_serv_butone(cptr, ":%s AWAY", parv[0]); */
		if (MyConnect(sptr))
			sendto_one(sptr, replies[RPL_UNAWAY], ME, BadTo(parv[0]));
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_AWAY, NULL, sptr,
				      ":%s AWAY", parv[0]);
#endif
		sptr->user->flags &= ~FLAGS_AWAY;
		return 1;
	    }

	/* Marking as away */

	if ((len = strlen(awy2)) > (size_t) TOPICLEN)
	    {
		len = TOPICLEN;
		awy2[TOPICLEN] = '\0';
	    }
	len++;
	/* sendto_serv_butone(cptr, ":%s AWAY :%s", parv[0], awy2); */
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_AWAY, NULL, sptr,
			      ":%s AWAY :%s", parv[0], awy2);
#endif

	if (away)
	    {
		istat.is_awaymem -= (strlen(away) + 1);
		away = (char *)MyRealloc(away, len);
		istat.is_awaymem += len;
	    }
	else
	    {
		istat.is_away++;
		istat.is_awaymem += len;
		away = (char *)MyMalloc(len);
		sendto_serv_butone(cptr, ":%s MODE %s :+a",
			sptr->user->uid, parv[0]);
	    }

	sptr->user->flags |= FLAGS_AWAY;
	if (MyConnect(sptr))
	    {
		sptr->user->away = away;
		(void)strcpy(away, awy2);
		sendto_one(sptr, replies[RPL_NOWAWAY], ME, BadTo(parv[0]));
	    }
	return 2;
}

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*origin, *destination;

	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */

	acptr = find_client(origin, NULL);
	if (!acptr)
		acptr = find_server(origin, NULL);
	if (!acptr || acptr != sptr)
		origin = cptr->name;
	if (!BadPtr(destination) && match(destination, ME) != 0)
	{
		if ((acptr = find_server(destination, NULL)))
			sendto_one(acptr, ":%s PING %s :%s", parv[0],
				origin, destination);
	    	else
		{
			sendto_one(sptr, replies[ERR_NOSUCHSERVER],
				ME, BadTo(parv[0]), destination);
			return 1;
		}
	}
	else
		sendto_one(sptr, ":%s PONG %s :%s", ME,
			(destination) ? destination : ME, parv[1]);
	return 1;
}

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, replies[ERR_NOORIGIN], ME, BadTo(parv[0]));
		return 1;
	    }

	origin = parv[1];
	destination = parv[2];

	sptr->flags &= ~FLAGS_PINGSENT;
	if (destination)
	{
		acptr = find_target(destination, cptr);
	}
	else
	{
		acptr = &me;
	}
	if (!acptr)
	{
		sendto_one(sptr, replies[ERR_NOSUCHSERVER], ME, BadTo(parv[0]),
			   destination);
		return 2;	
	}
	if (!IsMe(acptr))
	{
		if (!(MyClient(sptr) && mycmp(origin, sptr->name)))
			sendto_one(acptr,":%s PONG %s %s",
					   parv[0], origin, destination);
		return 2;
	}
	return 1;
}


/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
int	m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *aconf;
	char	*name, *password, *encr;
	char	*logstring = NULL;

	name = parv[1];
	password = parv[2];

	if (IsAnOper(sptr))
	{
		if (MyConnect(sptr))
			sendto_one(sptr, replies[RPL_YOUREOPER], ME, BadTo(parv[0]));
		return 1;
	}
	if (!(aconf = find_Oline(name, sptr)))
	{
		sendto_one(sptr, replies[ERR_NOOPERHOST], ME, BadTo(parv[0]));
		return 1;
	}
	if (aconf->clients >= MaxLinks(Class(aconf)))
	{
		sendto_one(sptr, ":%s %d %s :Too many opers", ME, ERR_NOOPERHOST, BadTo(parv[0]));
		return 1;
	}
#ifdef CRYPT_OPER_PASSWORD
	/* pass whole aconf->passwd as salt, let crypt() deal with it */

	if (password && aconf->passwd)
	    {
		extern	char *crypt();

		encr = crypt(password, aconf->passwd);
		if (encr == NULL)
		    {
			sendto_flag(SCH_ERROR, "crypt() returned NULL");
			sendto_one(sptr,replies[ERR_PASSWDMISMATCH], ME,BadTo(parv[0]));
			return 3;
		    }
	    }
	else
		encr = "";
#else
	encr = password;
#endif  /* CRYPT_OPER_PASSWORD */

	if ((aconf->status & CONF_OPS) &&
	    StrEq(encr, aconf->passwd) && !attach_conf(sptr, aconf))
	{
		int old = (sptr->user->flags & ALL_UMODES);
		char *s;

		s = index(aconf->host, '@');
		*s++ = '\0';
#ifndef	NO_OPER_REMOTE
		if (aconf->flags & ACL_LOCOP)
#else
		if ((match(s,me.sockhost) && !IsLocal(sptr)) ||
		    aconf->flags & ACL_LOCOP)
#endif
			SetLocOp(sptr);
		else
			SetOper(sptr);
		*--s =  '@';
		sendto_flag(SCH_NOTICE, "%s (%s@%s) is now operator (%c)",
			    parv[0], sptr->user->username, sptr->user->host,
			   IsOper(sptr) ? 'o' : 'O');
		send_umode_out(cptr, sptr, old);
 		sendto_one(sptr, replies[RPL_YOUREOPER], ME, BadTo(parv[0]));
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->servp, 
				      sptr, ":%s MODE %s :+%c", parv[0],
				      parv[0], IsOper(sptr) ? 'o' : 'O');
#endif
		if (IsAnOper(sptr))
		{
			istat.is_oper++;
			sptr->user->servp->usercnt[2]++;
		}
		logstring = "";
	}	
	else /* Wrong password or attach_conf() failed */
	{
		(void)detach_conf(sptr, aconf);
		if (!StrEq(encr, aconf->passwd))
			sendto_one(sptr,replies[ERR_PASSWDMISMATCH], ME, BadTo(parv[0]));
		else
			sendto_one(sptr,":%s %d %s :Too many connections",
				ME, ERR_PASSWDMISMATCH, BadTo(parv[0]));
#ifdef FAILED_OPERLOG
		sendto_flag(SCH_NOTICE, "FAILED OPER attempt by %s!%s@%s",
			parv[0], sptr->user->username, sptr->user->host);
		logstring = "FAILED ";
#endif
	}

	if (logstring)
	{
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
		syslog(LOG_INFO, "%sOPER (%s) by (%s!%s@%s) [%s@%s]",
			logstring,
			name, parv[0], sptr->user->username, sptr->user->host,
			sptr->auth,
#ifdef UNIXPORT
			IsUnixSocket(sptr) ? sptr->sockhost :
#endif
#ifdef INET6
                       inet_ntop(AF_INET6, (char *)&sptr->ip, ipv6string,
			       sizeof(ipv6string))
#else
                       inetntoa((char *)&sptr->ip)
#endif
		       );
#endif /* defined(USE_SYSLOG) && defined(SYSLOG_OPER) */

#ifdef FNAME_OPERLOG
	      {
		int     logfile;

		/*
		 * This conditional makes the logfile active only after
		 * it's been created - thus logging can be turned off by
		 * removing the file.
		 *
		 * stop NFS hangs...most systems should be able to open a
		 * file in 3 seconds. -avalon (curtesy of wumpus)
		 */
		(void)alarm(3);
		if (IsPerson(sptr) &&
		    (logfile = open(FNAME_OPERLOG, O_WRONLY|O_APPEND
#ifdef LOGFILES_ALWAYS_CREATE
					|O_CREAT, S_IRUSR|S_IWUSR
#endif
			)) != -1)
		{
			(void)alarm(0);
		  	sprintf(buf, "%s %sOPER (%s) by (%s!%s@%s)"
				     " [%s@%s]\n",
			 	myctime(timeofday),
				logstring,
				name, parv[0],
				sptr->user->username, sptr->user->host,
				sptr->auth,
#ifdef UNIXPORT
				IsUnixSocket(sptr) ? sptr->sockhost :
#endif
#ifdef INET6
				inetntop(AF_INET6, (char *)&sptr->ip,
					ipv6string, sizeof(ipv6string))
#else
				inetntoa((char *)&sptr->ip)
#endif
				);
			(void)alarm(3);
		  	(void)write(logfile, buf, strlen(buf));
		  	(void)alarm(0);
		  	(void)close(logfile);
		}
		(void)alarm(0);
		/* Modification by pjg */
	      }
#endif /* FNAME_OPERLOG */
	
	} /* logstring != NULL */
	
	return 3;
    }

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
**	parv[2] = protocol & server versions (server only)
**	parv[3] = server id & options (server only)
**	parv[4] = (optional) link options (server only)                  
*/
int	m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *password = parv[1];

	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	if (cptr->user || cptr->service)
	{
		/* If we have one of these structures allocated, it means
		** that PASS was issued after USER or SERVICE. No need to
		** copy PASS parameters to info field, then. */
		return 1;
	}
	/* Temporarily store PASS pwd *parameters* into info field.
	** This will be used as version in m_server() and cleared
	** in m_user(). */
	if (parc > 2 && parv[2])
	    {
		strncpyzt(buf, parv[2], 15); 
		if (parc > 3 && parv[3])
		    {
			strcat(buf, " ");
			strncat(buf, parv[3], 100);
			if (parc > 4 && parv[4])
			    {
				strcat(buf, " ");
				strncat(buf, parv[4], 5);
			    }
		    }
		if (cptr->info != DefInfo)
			MyFree(cptr->info);
		cptr->info = mystrdup(buf);
	    }
	return 1;
}

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int	m_userhost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*p = NULL;
	aClient	*acptr;
	Reg	char	*s;
	Reg	int	i, len;
	int	idx = 1;

	(void)sprintf(buf, replies[RPL_USERHOST], ME, BadTo(parv[0]));
	len = strlen(buf);
	*buf2 = '\0';

	for (i = 5, s = strtoken(&p, parv[idx], " "); i && s; i--)
	     {
		if ((acptr = find_person(s, NULL)))
		    {
			if (*buf2)
				(void)strcat(buf, " ");
			sprintf(buf2, "%s%s=%c%s@%s", acptr->name,
				IsAnOper(acptr) ? "*" : "",
				(acptr->user->flags & FLAGS_AWAY) ? '-' : '+',
				acptr->user->username, acptr->user->host);
			(void)strncat(buf, buf2, sizeof(buf) - len);
			len += strlen(buf2);
			if (len > BUFSIZE - (NICKLEN + 5 + HOSTLEN + USERLEN))
			    {
				sendto_one(sptr, "%s", buf);
				(void)sprintf(buf, replies[RPL_USERHOST],
					     ME, BadTo(parv[0]));
				len = strlen(buf);
				*buf2 = '\0';
			    }
		    }
		s = strtoken(&p, (char *)NULL, " ");
		if (!s && parv[++idx])
		    {
			p = NULL;
			s = strtoken(&p, parv[idx], " ");
		    }
	    }
	sendto_one(sptr, "%s", buf);
	return 1;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

int	m_ison(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg	aClient *acptr;
	Reg	char	*s, **pav = parv;
	Reg	int	len = 0, i;
	char	*p = NULL;

	(void)sprintf(buf, replies[RPL_ISON], ME, BadTo(*parv));
	len = strlen(buf);

	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
		if ((acptr = find_person(s, NULL)))
		    {
			i = strlen(acptr->name);
			if (len + i > sizeof(buf) - 4)	
			{
				/* leave room for " \r\n\0" */
				break;
			}
			(void) strcpy(buf + len, acptr->name);
			len += i;
			(void) strcpy(buf + len++, " ");
		    }
	sendto_one(sptr, "%s", buf);
	return 1;
}

/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender (can be NULL, see below..)
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int	m_umode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg	int	flag;
	Reg	int	*s;
	Reg	char	**p, *m;
	aClient	*acptr = NULL;
	int	what, setflags, penalty = 0;

	what = MODE_ADD;

	if (cptr && !(acptr = find_person(parv[1], NULL)))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, replies[ERR_NOSUCHCHANNEL], ME, BadTo(parv[0]),
				   parv[1]);
		return 1;
	    }
	if (cptr == NULL)
		/* internal call which has to be handled in a special way */
		acptr = sptr;

	if ((cptr != NULL) &&
	    ((IsServer(sptr) || sptr != acptr || acptr->from != sptr->from)))
	    {
		if (IsServer(cptr))
			sendto_ops_butone(NULL, ME,
				  "MODE for User %s From %s!%s", parv[1],
				  get_client_name(cptr, FALSE), sptr->name);
		else
			sendto_one(sptr, replies[ERR_USERSDONTMATCH], ME, BadTo(parv[0]));
			return 1;
	    }
 
	if (parc < 3)
	    {
		m = buf;
		*m++ = '+';
		for (s = user_modes; (flag = *s) && (m - buf < BUFSIZE - 4);
		     s += 2)
			if (sptr->user->flags & flag)
				*m++ = (char)(*(s+1));
		*m = '\0';
		sendto_one(sptr, replies[RPL_UMODEIS], ME, BadTo(parv[0]), buf);
		return 1;
	    }

	/* find flags already set for user */
	setflags = 0;
	for (s = user_modes; (flag = *s); s += 2)
		if (sptr->user->flags & flag)
			setflags |= flag;

	/*
	 * parse mode change string(s)
	 */
	for (p = &parv[2]; p && *p; p++ )
		for (m = *p; *m; m++)
			switch(*m)
			{
			case '+' :
				what = MODE_ADD;
				break;
			case '-' :
				what = MODE_DEL;
				break;	
			/* we may not get these,
			 * but they shouldnt be in default
			 */
			case ' ' :
			case '\n' :
			case '\r' :
			case '\t' :
				break;
			case 'a' : /* fall through case */
				/* users should use the AWAY message */
				if (cptr && !IsServer(cptr))
					break;
				if (what == MODE_DEL && sptr->user->away)
				    {
					istat.is_away--;
					istat.is_awaymem -= (strlen(sptr->user->away) + 1);
					MyFree(sptr->user->away);
					sptr->user->away = NULL;
#ifdef  USE_SERVICES
				check_services_butone(SERVICE_WANT_AWAY,
						      sptr->user->servp, sptr,
						      ":%s AWAY", parv[0]);
#endif
				    }
#ifdef  USE_SERVICES
				if (what == MODE_ADD)
				check_services_butone(SERVICE_WANT_AWAY,
						      sptr->user->servp, sptr,
						      ":%s AWAY :", parv[0]);
#endif
			default :
				for (s = user_modes; (flag = *s); s += 2)
					if (*m == (char)(*(s+1)))
				    {
					if (what == MODE_ADD)
						sptr->user->flags |= flag;
					else
						sptr->user->flags &= ~flag;	
					penalty += 1;
					break;
				    }
				if (flag == 0 && MyConnect(sptr))
					sendto_one(sptr, replies[ERR_UMODEUNKNOWNFLAG],
						ME, BadTo(parv[0]), *m);
				break;
			}
	if (cptr)
	    {
		/* stop users making themselves operators too easily */
		if (!(setflags & FLAGS_OPER) && IsOper(sptr) &&
		    !IsServer(cptr))
			ClearOper(sptr);
		if (!(setflags & FLAGS_LOCOP) && IsLocOp(sptr))
			ClearLocOp(sptr);
		/* but once they are, set their status */
		SetClient(sptr);
		if ((setflags & FLAGS_RESTRICT) &&
		    !IsRestricted(sptr))
		    {
			sendto_one(sptr, replies[ERR_RESTRICTED], ME, BadTo(parv[0]));
			SetRestricted(sptr);
			/* Can't return; here since it could mess counters */
		    }
		if ((setflags & (FLAGS_OPER|FLAGS_LOCOP)) && !IsAnOper(sptr) &&
		    MyConnect(sptr))
			det_confs_butmask(sptr, CONF_CLIENT);

		/*
		 * compare new flags with old flags and send string which
		 * will cause servers to update correctly.
		 */
		if (!IsInvisible(sptr) && (setflags & FLAGS_INVISIBLE))
		    {
			istat.is_user[1]--;
			istat.is_user[0]++;
			sptr->user->servp->usercnt[1]--;
			sptr->user->servp->usercnt[0]++;
		    }
		if (IsInvisible(sptr) && !(setflags & FLAGS_INVISIBLE))
		    {
			istat.is_user[1]++;
			istat.is_user[0]--;
			sptr->user->servp->usercnt[1]++;
			sptr->user->servp->usercnt[0]--;
		    }
		send_umode_out(cptr, sptr, setflags);
	    }

	/* update counters */
	if (IsOper(sptr) && !(setflags & FLAGS_OPER))
	    {
		istat.is_oper++;
		sptr->user->servp->usercnt[2]++;
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->servp,
				      sptr, ":%s MODE %s :+o", parv[0],
				      parv[0]);
#endif
	    }
	else if (!IsOper(sptr) && (setflags & FLAGS_OPER))
	    {
		istat.is_oper--;
		sptr->user->servp->usercnt[2]--;
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->servp,
				      sptr, ":%s MODE %s :-o", parv[0],
				      parv[0]);
#endif
	    }
	else if (MyConnect(sptr) && !IsLocOp(sptr) && (setflags & FLAGS_LOCOP))
	    {
		istat.is_oper--;
		sptr->user->servp->usercnt[2]--;
#ifdef USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->servp,
				      sptr, ":%s MODE %s :-O", parv[0],
				      parv[0]);
#endif
	    }

	return penalty;
}
	
/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void	send_umode(aClient *cptr, aClient *sptr, int old, int sendmask,
		char *umode_buf)
{
	Reg	int	*s, flag;
	Reg	char	*m;
	int	what = MODE_NULL;

	if (!sptr->user)
		return;
	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (sptr->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (s = user_modes; (flag = *s); s += 2)
	    {
		if (MyClient(sptr) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(sptr->user->flags & flag))
		    {
			if (what == MODE_DEL)
				*m++ = *(s+1);
			else
			    {
				what = MODE_DEL;
				*m++ = '-';
				*m++ = *(s+1);
			    }
		    }
		else if (!(flag & old) && (sptr->user->flags & flag))
		    {
			if (what == MODE_ADD)
				*m++ = *(s+1);
			else
			    {
				what = MODE_ADD;
				*m++ = '+';
				*m++ = *(s+1);
			    }
		    }
	    }
	*m = '\0';
	if (*umode_buf && cptr)
		sendto_one(cptr, ":%s MODE %s :%s",
			   sptr->name, sptr->name, umode_buf);
}

/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
void	send_umode_out(aClient *cptr, aClient *sptr, int old)
{
	Reg	int	i;
	Reg	aClient	*acptr;

	send_umode(NULL, sptr, old, SEND_UMODES, buf);

	if (*buf)
		for (i = fdas.highest; i >= 0; i--)
		    {
			if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr))
				continue;
			if (acptr == cptr || acptr == sptr)
				continue;
			sendto_one(acptr, ":%s MODE %s :%s",
				   sptr->user->uid, sptr->name, buf);
		    }

	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
#ifdef USE_SERVICES
	/* buf contains all modes for local users, and iow only for remotes */
	if (*buf)
		check_services_butone(SERVICE_WANT_UMODE, sptr->user->servp,
				      sptr, ":%s MODE %s :%s", sptr->name,
				      sptr->name, buf);
#endif
}

/*
** save_user() added 990618 by Christope Kalt
**
** This will save the user sptr, and put the nick he's currently using in
** nick delay.
** It will send SAVE to the servers that can deal with it, and just a nick
** change to the rest.
** It will adjust the path, to include the link we got the SAVE message from.
** For internal calls, set cptr to NULL.
*/
static	void	save_user(aClient *cptr, aClient *sptr, char *path)
{
	if (MyConnect(sptr))
	{
		sendto_one(sptr, replies[RPL_SAVENICK], cptr ? cptr->name : ME,
			   sptr->name, sptr->user->uid);
#if defined(CLIENTS_CHANNEL) && (CLIENTS_CHANNEL_LEVEL & CCL_NICK)
		sendto_flag(SCH_CLIENT, "%s %s %s %s NICK %s",
			sptr->user->uid, sptr->name, sptr->user->username,
			sptr->user->host, sptr->user->uid);
#endif
	}
	
	sendto_common_channels(sptr, ":%s NICK :%s",
			       sptr->name, sptr->user->uid);
	add_history(sptr, NULL);
#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_NICK, sptr->user->servp, sptr,
			      ":%s NICK :%s", sptr->name, sptr->user->uid);
#endif
	sendto_serv_v(cptr, SV_UID, ":%s SAVE %s :%s%c%s", 
		cptr ? cptr->serv->sid : me.serv->sid, sptr->user->uid, 
		cptr ? cptr->name : ME, cptr ? '!' : ' ', path);
	sendto_flag(SCH_SAVE, "Received SAVE message for %s. Path: %s!%s",
		    sptr->name, cptr ? cptr->name : ME, path);
	del_from_client_hash_table(sptr->name, sptr);
	strcpy(sptr->name, sptr->user->uid);
	add_to_client_hash_table(sptr->name, sptr);
}

/*
** m_save() added 990618 by Christope Kalt
**	parv[0] = sender prefix
**	parv[1] = saved user
**	parv[2] = save path
*/
int	m_save(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char *path = (parc > 2) ? parv[2] : "*no-path*";
	
	if (parc < 2)
	{
		sendto_flag(SCH_ERROR, "Save with not enough parameters "
			"from %s", cptr->name);
		return 1;
	}

	/* need sanity checks here -syrk */
	acptr = find_uid(parv[1], NULL);
	if (acptr && strcasecmp(acptr->name, acptr->user->uid))
	{
		save_user(cptr, acptr, path);
		ircstp->is_save++;
	}

	return 0;
}

/*
** Given client cptr and function decide access.
** Return 1 for OK, 0 for forbidden.
*/

int	is_allowed(aClient *cptr, long function)
{
	Link	*tmp;

	/* We cannot judge not our clients. Yet. */
	if (!MyConnect(cptr) || IsServer(cptr))
		return 1;

	/* minimal control, but nothing else service can do anyway. */
	if (IsService(cptr))
	{
		if (function == ACL_TKLINE &&
			(cptr->service->wants & SERVICE_WANT_TKLINE))
			return 1;
		if (function == ACL_KLINE &&
			(cptr->service->wants & SERVICE_WANT_KLINE))
			return 1;
		return 0;
	}

	for (tmp = cptr->confs; tmp; tmp = tmp->next)
	{
		if (tmp->value.aconf->status & CONF_OPERATOR)
			break;
	}

	/* no O: conf found */
	if (!tmp)
		return 0;

	/* check access */
	if ((tmp->value.aconf->flags & function))
		return 1;

	return 0;
}

void send_away(aClient *sptr, aClient *acptr)
{
	if (acptr->user->away)
	{
		sendto_one(sptr, replies[RPL_AWAY], ME, sptr->name,
			acptr->name, acptr->user->away);
	}
	else
	{
#ifdef AWAY_MOREINFO
		/* Building buffer and using it instead of "Gone" would be
		 * a better code, but a bit slower; just rememeber about this
		 * one when ever changing RPL_AWAY --B. */
		sendto_one(sptr, ":%s 301 %s %s :"
			"Gone, for more info use WHOIS %s %s",
			ME, sptr->name, acptr->name,
			acptr->name, acptr->name);
#else
		sendto_one(sptr, replies[RPL_AWAY], ME, sptr->name,
			acptr->name, "Gone");
#endif
	}
}
