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
static  char rcsid[] = "@(#)$Id: s_user.c,v 1.63 1998/12/21 21:05:33 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_USER_C
#include "s_externs.h"
#undef S_USER_C

static char buf[BUFSIZE], buf2[BUFSIZE];

static int user_modes[]	     = { FLAGS_OPER, 'o',
				 FLAGS_LOCOP, 'O',
				 FLAGS_INVISIBLE, 'i',
				 FLAGS_WALLOP, 'w',
				 FLAGS_RESTRICTED, 'r',
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
*/
aClient *next_client(next, ch)
Reg	aClient *next;	/* First client to check */
Reg	char	*ch;	/* search string (may include wilds) */
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
int	hunt_server(cptr, sptr, command, server, parc, parv)
aClient	*cptr, *sptr;
char	*command, *parv[];
int	server, parc;
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
			&& match(sptr->service->dist,acptr->name) != 0))
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER, parv[0]), 
				   parv[server]);
			return(HUNTED_NOSUCH);
		    }
		sendto_one(acptr, command, parv[0],
			   parv[1], parv[2], parv[3], parv[4],
			   parv[5], parv[6], parv[7], parv[8]);
		return(HUNTED_PASS);
	    } 
	sendto_one(sptr, err_str(ERR_NOSUCHSERVER, parv[0]), parv[server]);
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
**  In addition, the first character cannot be '-'
**  or a Digit.
**
**  Note:
**	'~'-character should be allowed, but
**	a change should be global, some confusion would
**	result if only few servers allowed it...
*/

int	do_nick_name(nick)
char	*nick;
{
	Reg	char	*ch;

	if (*nick == '-' || isdigit(*nick)) /* first character in [0..9-] */
		return 0;

	for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
		if (!isvalid(*ch) || isspace(*ch))
			break;

	*ch = '\0';

	return (ch - nick);
}


/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
*/
char	*canonize(buffer)
char	*buffer;
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
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/

static	int	register_user(cptr, sptr, nick, username)
aClient	*cptr;
aClient	*sptr;
char	*nick, *username;
{
	Reg	aConfItem *aconf;
	aClient	*acptr;
	aServer	*sp = NULL;
	anUser	*user = sptr->user;
	short	oldstatus = sptr->status;
	char	*parv[3];
#ifndef NO_PREFIX
	char	prefix;
#endif
	int	i;

	user->last = timeofday;
	parv[0] = sptr->name;
	parv[1] = parv[2] = NULL;

	if (MyConnect(sptr))
	    {
		char *reason = NULL;

#if defined(USE_IAUTH)
		/* this should not be needed, but there's a bug.. -kalt */
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

		if (sptr->exitc == EXITC_AREF)
		    {
			char *masked = NULL, *format;

			/*
			** All this masking is rather ugly but prompted by
			** the fact that the hostnames should not be made
			** available in realtime. (first iauth module using
			** this detects open proxies)
			** Then again, if detailed information is needed,
			** the admin should check logs and/or the module
			** should be changed to send details to &AUTH.
			*/
			if (sptr->hostp)
			    {
				masked = index(sptr->hostp->h_name, '.');
				format = "Denied connection from ???%s.";
			    }
			else
			    {
				char *dot;

				masked = inetntoa((char *)&sptr->ip);
				dot = rindex(masked, '.');
				if (dot)
					*(dot+1) = '\0';
				else
					masked = NULL;
				format = "Denied connection from %s???.";
			    }
			if (masked) /* just to be safe */
				sendto_flag(SCH_LOCAL, format, masked);
			else
				sendto_flag(SCH_LOCAL, "Denied connection.");
#if defined(USE_SYSLOG) && defined(SYSLOG_CONN)
			syslog(LOG_NOTICE, "%s ( %s ): <none>@%s [%s] %c\n",
			       myctime(sptr->firsttime), " Denied  ",
			       (IsUnixSocket(sptr)) ? me.sockhost :
			       ((sptr->hostp) ? sptr->hostp->h_name :
				sptr->sockhost), sptr->auth, sptr->exitc);
#endif		    
#if defined(FNAME_CONNLOG) || defined(USE_SERVICES)
			sendto_flog(sptr, " Denied  ", 0, "<none>",
				    (IsUnixSocket(sptr)) ? me.sockhost :
				    ((sptr->hostp) ? sptr->hostp->h_name :
				    sptr->sockhost));
#endif
			return exit_client(cptr, sptr, &me, "Denied Access");
		    }
		if ((i = check_client(sptr)))
		    {
			struct msg_set { char *shortm; char *longm; };
			    
			static struct msg_set exit_msg[7] = {
			{ "G u@h max", "Too many user connections (global)" },
			{ "G IP  max", "Too many host connections (global)" },
			{ "L u@h max", "Too many user connections (local)" },
			{ "L IP  max", "Too many host connections (local)" },
			{ "   max   ", "Too many connections" },
			{ " No Auth ", "Unauthorized connection" },
			{ " Failure ", "Connect failure" } };

			i += 7;
			if (i < 0 || i > 6) /* in case.. */
				i = 6;

			ircstp->is_ref++;
			sptr->exitc = EXITC_REF;
			sendto_flag(SCH_LOCAL, "%s from %s.",
				    exit_msg[i].longm, get_client_host(sptr));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONN)
			syslog(LOG_NOTICE, "%s ( %s ): <none>@%s [%s] %c\n",
			       myctime(sptr->firsttime), exit_msg[i].shortm,
			       (IsUnixSocket(sptr)) ? me.sockhost :
			       ((sptr->hostp) ? sptr->hostp->h_name :
				sptr->sockhost), sptr->auth, sptr->exitc);
#endif		    
#if defined(FNAME_CONNLOG) || defined(USE_SERVICES)
			sendto_flog(sptr, exit_msg[i].shortm, 0, "<none>",
				    (IsUnixSocket(sptr)) ? me.sockhost :
				    ((sptr->hostp) ? sptr->hostp->h_name :
				    sptr->sockhost));
#endif
			return exit_client(cptr, sptr, &me, exit_msg[i].longm);
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
		if (IsUnixSocket(sptr))
			strncpyzt(user->host, me.sockhost, HOSTLEN+1);
		else
			strncpyzt(user->host, sptr->sockhost, HOSTLEN+1);

		if (!BadPtr(aconf->passwd) &&
		    !StrEq(sptr->passwd, aconf->passwd))
		    {
			ircstp->is_ref++;
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH, parv[0]));
			return exit_client(cptr, sptr, &me, "Bad Password");
		    }
		bzero(sptr->passwd, sizeof(sptr->passwd));
		/*
		 * following block for the benefit of time-dependent K:-lines
		 */
		if (find_kill(sptr, 1, &reason))
		    {
			/*char buf[100];*/

			sendto_flag(SCH_LOCAL, "K-lined %s@%s.",
				    user->username, sptr->sockhost);
			ircstp->is_ref++;
			sptr->exitc = EXITC_REF;
#if defined(USE_SYSLOG) && defined(SYSLOG_CONN)
			syslog(LOG_NOTICE, "%s ( K lined ): %s@%s [%s] %c\n",
			       myctime(sptr->firsttime), user->username,
			       user->host, sptr->auth, '-');
#endif		    
#if defined(FNAME_CONNLOG) || defined(USE_SERVICES)
			sendto_flog(sptr, " K lined ", 0, user->username,
				    user->host);
#endif
			if (reason)
				sprintf(buf, "K-lined: %.80s", reason);
			return exit_client(cptr, sptr, &me, (reason) ? buf :
					   "K-lined");
		    }
#ifdef R_LINES
		if (find_restrict(sptr))
		    {
			sendto_flag(SCH_LOCAL, "R-lined %s@%s.",
				    user->username, sptr->sockhost);
			ircstp->is_ref++;
			sptr->exitc = EXITC_REF;
# if defined(USE_SYSLOG) && defined(SYSLOG_CONN)
			syslog(LOG_NOTICE, "%s ( R lined ): %s@%s [%s] %c\n",
			       myctime(sptr->firsttime), user->username,
			       user->host, sptr->username, '-');
# endif		    
# if defined(FNAME_CONNLOG) || defined(USE_SERVICES)
			sendto_flog(sptr, " R lined ", 0, user->username,
				    user->host);
# endif
			return exit_client(cptr, sptr, &me , "R-lined");
		    }
#endif
		if (oldstatus == STAT_MASTER && MyConnect(sptr))
			(void)m_oper(&me, sptr, 1, parv);
/*		*user->tok = '1';
		user->tok[1] = '\0';*/
		sp = user->servp;
	    }
	else
		strncpyzt(user->username, username, USERLEN+1);
	SetClient(sptr);
	if (MyConnect(sptr))
	    {
		sprintf(buf, "%s!%s@%s", nick, user->username, user->host);
		sptr->exitc = EXITC_REG;
		sendto_one(sptr, rpl_str(RPL_WELCOME, nick), buf);
		/* This is a duplicate of the NOTICE but see below...*/
		sendto_one(sptr, rpl_str(RPL_YOURHOST, nick),
			   get_client_name(&me, FALSE), version);
		sendto_one(sptr, rpl_str(RPL_CREATED, nick), creation);
		sendto_one(sptr, rpl_str(RPL_MYINFO, parv[0]),
			   ME, version);
		(void)m_lusers(sptr, sptr, 1, parv);
		(void)m_motd(sptr, sptr, 1, parv);
		nextping = timeofday;
	    }
	else if (IsServer(cptr))
	    {
		acptr = find_server(user->server, NULL);
		if (acptr && acptr->from != cptr)
		    {
			sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
				   ME, sptr->name, ME, user->server,
				   acptr->from->name, acptr->from->sockhost);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(cptr, sptr, &me,
					   "USER server wrong direction");
		    }
	    }

	send_umode(NULL, sptr, 0, SEND_UMODES, buf);
	for (i = fdas.highest; i >= 0; i--)
	    {	/* Find my leaf servers and feed the new client to them */
		if ((acptr = local[fdas.fd[i]]) == cptr || IsMe(acptr))
			continue;
		if ((aconf = acptr->serv->nline) &&
		    !match(my_name_for_link(ME, aconf->port),
			   user->server))
			sendto_one(acptr, "NICK %s %d %s %s %s %s :%s",
				   nick, sptr->hopcount+1, 
				   user->username, user->host, 
				   me.serv->tok, (*buf) ? buf : "+",
				   sptr->info);
		else
			sendto_one(acptr, "NICK %s %d %s %s %s %s :%s",
				   nick, sptr->hopcount+1, 
				   user->username, user->host, 
				   user->servp->tok, 
				   (*buf) ? buf : "+", sptr->info);
	    }	/* for(my-leaf-servers) */
	if (MyConnect(sptr))
	    {
		if (IsRestricted(sptr))
			sendto_one(sptr, err_str(ERR_RESTRICTED, nick));
		send_umode(sptr, sptr, 0, ALL_UMODES, buf);
	    }

	if (IsInvisible(sptr))		/* Can be initialized in m_user() */
		istat.is_user[1]++;	/* Local and server defaults +i */
	else
		istat.is_user[0]++;
	if (MyConnect(sptr))
	    {
		istat.is_unknown--;
		istat.is_myclnt++;
	    }
#ifdef	USE_SERVICES
#if 0
	check_services_butone(SERVICE_WANT_NICK, user->server, NULL,
			      "NICK %s :%d", nick, sptr->hopcount+1);
	check_services_butone(SERVICE_WANT_USER, user->server, sptr,
			      ":%s USER %s %s %s :%s", nick, user->username, 
			      user->host, user->server, sptr->info);
	if (MyConnect(sptr))	/* all modes about local users */
		send_umode(NULL, sptr, 0, ALL_UMODES, buf);
	check_services_butone(SERVICE_WANT_UMODE, user->server, sptr,
			      ":%s MODE %s :%s", nick, nick, buf);
#endif
	if (MyConnect(sptr))	/* all modes about local users */
		send_umode(NULL, sptr, 0, ALL_UMODES, buf);
	check_services_num(sptr, buf);
#endif
	return 1;
    }

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
** the following are only used between since version 2.9 between servers
**	parv[2] = hopcount
**	parv[3] = username (login name, account)
**	parv[4] = client host name
**	parv[5] = server token
**	parv[6] = users mode
**	parv[7] = users real name info
*/
int	m_nick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	int	delayed = 0;
	char	nick[NICKLEN+2], *s, *user, *host;
	Link	*lp;

	if (IsService(sptr))
   	    {
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED, parv[0]));
		return 1;
	    }

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN, parv[0]));
		return 1;
	    }
	if (MyConnect(sptr) && (s = (char *)index(parv[1], '~')))
		*s = '\0';
	strncpyzt(nick, parv[1], NICKLEN+1);

	if (parc == 8 && cptr->serv)
	    {
		user = parv[3];
		host = parv[4];
	    }
	else
	    {
		if (sptr->user)
		    {
			user = sptr->username;
			host = sptr->user->host;
		    }
		else
			user = host = "";
	    }
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * and KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick) == 0 ||
	    (IsServer(cptr) && strcmp(nick, parv[1])))
	    {
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME, parv[0]),
			   parv[1]);

		if (IsServer(cptr))
		    {
			ircstp->is_kill++;
			sendto_flag(SCH_KILL, "Bad Nick: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
				   ME, parv[1], ME, parv[1],
				   nick, cptr->name);
			if (sptr != cptr) /* bad nick change */
			    {
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					ME, parv[0], ME,
					get_client_name(cptr, FALSE),
					parv[0], user, host);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			    }
		    }
		return 2;
	    }

	/*
	** Check against nick name collisions.
	**
	** Put this 'if' here so that the nesting goes nicely on the screen :)
	** We check against server name list before determining if the nickname
	** is present in the nicklist (due to the way the below for loop is
	** constructed). -avalon
	*/
	if ((acptr = find_server(nick, NULL)))
		if (MyConnect(sptr))
		    {
			sendto_one(sptr, err_str(ERR_NICKNAMEINUSE, parv[0]),
				   nick);
			return 2; /* NICK message ignored */
		    }
	/*
	** acptr already has result from previous find_server()
	*/
	if (acptr)
	    {
		/*
		** We have a nickname trying to use the same name as
		** a server. Send out a nick collision KILL to remove
		** the nickname. As long as only a KILL is sent out,
		** there is no danger of the server being disconnected.
		** Ultimate way to jupiter a nick ? >;-). -avalon
		*/
		sendto_flag(SCH_KILL,
			    "Nick collision on %s (%s@%s)%s <- (%s@%s)%s",
			    sptr->name,
			    (acptr->user) ? acptr->user->username : "???",
			    (acptr->user) ? acptr->user->host : "???",
			    acptr->from->name, user, host,
			    get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
			   ME, sptr->name, ME, acptr->from->name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	    }
	/*
	** Nick is free, and it comes from another server or
	** it has been free for a while here
	*/
	if (!(acptr = find_client(nick, NULL)) &&
	    (IsServer(cptr) || !(bootopt & BOOT_PROT) ||
	     !(delayed = find_history(nick, (long)DELAYCHASETIMELIMIT))))
		goto nickkilldone;  /* No collisions, all clear... */
	/*
	** If acptr == sptr, then we have a client doing a nick
	** change between *equivalent* nicknames as far as server
	** is concerned (user is changing the case of his/her
	** nickname or somesuch)
	*/
	if (acptr == sptr)
		if (strcmp(acptr->name, nick) != 0)
			/*
			** Allows change of case in his/her nick
			*/
			goto nickkilldone; /* -- go and process change */
		else
			/*
			** This is just ':old NICK old' type thing.
			** Just forget the whole thing here. There is
			** no point forwarding it to anywhere,
			** especially since servers prior to this
			** version would treat it as nick collision.
			*/
			return 2; /* NICK Message ignored */
	/*
	** Note: From this point forward it can be assumed that
	** acptr != sptr (point to different client structures).
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
		sendto_one(sptr, err_str((delayed) ? ERR_UNAVAILRESOURCE
						   : ERR_NICKNAMEINUSE,
					 parv[0]), nick);
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
	** The client indicated by 'acptr' is dead meat, give at least some
	** indication of the reason why we are just dropping it cold.
	*/
	sendto_one(acptr, err_str(ERR_NICKCOLLISION, acptr->name),
		   acptr->name, user, host);
	/*
	** This seemingly obscure test (sptr == cptr) differentiates
	** between "NICK new" (TRUE) and ":old NICK new" (FALSE) forms.
	*/
	if (sptr == cptr)
	    {
		sendto_flag(SCH_KILL,
			    "Nick collision on %s (%s@%s)%s <- (%s@%s)%s",
			    acptr->name,
			    (acptr->user) ? acptr->user->username : "???",
			    (acptr->user) ? acptr->user->host : "???",
			    acptr->from->name,
			    user, host, get_client_name(cptr, FALSE));
		/*
		** A new NICK being introduced by a neighbouring
		** server (e.g. message type "NICK new" received)
		*/
		ircstp->is_kill++;
		sendto_serv_butone(NULL, 
				   ":%s KILL %s :%s ((%s@%s)%s <- (%s@%s)%s)",
				   ME, acptr->name, ME,
				   (acptr->user) ? acptr->user->username:"???",
				   (acptr->user) ? acptr->user->host : "???",
				   acptr->from->name, user, host,
				   /* NOTE: Cannot use get_client_name twice
				   ** here, it returns static string pointer:
				   ** the other info would be lost
				   */
				   get_client_name(cptr, FALSE));
		acptr->flags |= FLAGS_KILLED;
		return exit_client(NULL, acptr, &me, "Nick collision");
	    }
	/*
	** A NICK change has collided (e.g. message type
	** ":old NICK new". This requires more complex cleanout.
	** Both clients must be purged from this server, the "new"
	** must be killed from the incoming connection, and "old" must
	** be purged from all outgoing connections.
	*/
	sendto_flag(SCH_KILL, "Nick change collision %s!%s@%s to %s %s <- %s",
		    sptr->name, user, host, acptr->name, acptr->from->name,
		    get_client_name(cptr, FALSE));
	ircstp->is_kill++;
	sendto_serv_butone(NULL, /* KILL old from outgoing servers */
			   ":%s KILL %s :%s (%s(%s) <- %s)",
			   ME, sptr->name, ME, acptr->from->name,
			   acptr->name, get_client_name(cptr, FALSE));
	ircstp->is_kill++;
	sendto_serv_butone(NULL, /* Kill new from incoming link */
		   ":%s KILL %s :%s (%s <- %s(%s))",
		   ME, acptr->name, ME, acptr->from->name,
		   get_client_name(cptr, FALSE), sptr->name);
	acptr->flags |= FLAGS_KILLED;
	(void)exit_client(NULL, acptr, &me, "Nick collision(new)");
	sptr->flags |= FLAGS_KILLED;
	return exit_client(cptr, sptr, &me, "Nick collision(old)");

nickkilldone:
	if (IsServer(sptr))
	    {
		/* A server introducing a new client, change source */

		sptr = make_client(cptr);
		add_client_to_list(sptr);
		if (parc > 2)
			sptr->hopcount = atoi(parv[2]);
		(void)strcpy(sptr->name, nick);
		if (parc == 8 && cptr->serv)
		    {
			char	*pv[7];

			pv[0] = sptr->name;
			pv[1] = parv[3];
			pv[2] = parv[4];
			pv[3] = parv[5];
			pv[4] = parv[7];
			pv[5] = parv[6];
			pv[6] = NULL;
			(void)add_to_client_hash_table(nick, sptr);
			return m_user(cptr, sptr, 6, pv);
		    }
	    }
	else if (sptr->name[0])		/* NICK received before, changing */
	    {
		if (MyConnect(sptr))
		{
			if (!IsPerson(sptr))    /* Unregistered client */
				return 2;       /* Ignore new NICKs */
			if (IsRestricted(sptr))
			    {
				sendto_one(sptr,
					   err_str(ERR_RESTRICTED, nick));
				return 2;
			    }
			/* is the user banned on any channel ? */
			for (lp = sptr->user->channel; lp; lp = lp->next)
				if (can_send(sptr, lp->value.chptr) ==MODE_BAN)
					break;
		}
		/*
		** Client just changing his/her nick. If he/she is
		** on a channel, send note of change to all clients
		** on that channel. Propagate notice to other servers.
		*/
		sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
		if (sptr->user) /* should always be true.. */
		    {
			add_history(sptr, sptr);
#ifdef	USE_SERVICES
			check_services_butone(SERVICE_WANT_NICK,
					      sptr->user->server, sptr,
					      ":%s NICK :%s", parv[0], nick);
#endif
		    }
		else
			sendto_flag(SCH_NOTICE,
				    "Illegal NICK change: %s -> %s from %s",
				    parv[0], nick, get_client_name(cptr,TRUE));
		sendto_serv_butone(cptr, ":%s NICK :%s", parv[0], nick);
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
	/*
	**  Finally set new nick name.
	*/
	(void)add_to_client_hash_table(nick, sptr);
	if (lp)
		return 15;
	else
		return 3;
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

static	int	m_message(cptr, sptr, parc, parv, notice)
aClient *cptr, *sptr;
char	*parv[];
int	parc, notice;
{
	Reg	aClient	*acptr;
	Reg	char	*s;
	aChannel *chptr;
	char	*nick, *server, *p, *cmd, *user, *host;
	int	count = 0, penalty = 0;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NORECIPIENT, parv[0]), cmd);
		return 1;
	    }

	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND, parv[0]));
		return 1;
	    }

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
			    sendto_one(sptr, err_str(ERR_TOOMANYTARGETS,
						     parv[0]),
				       "Too many",nick,"No Message Delivered");
		    continue;      
		}   
		/*
		** nickname addressed?
		*/
		if ((acptr = find_person(nick, NULL)))
		    {
			if (!notice && MyConnect(sptr) &&
			    acptr->user && (acptr->user->flags & FLAGS_AWAY))
				sendto_one(sptr, rpl_str(RPL_AWAY, parv[0]),
					   acptr->name,
					   (acptr->user->away) ? 
					   acptr->user->away : "Gone");
			sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
					  parv[0], cmd, nick, parv[2]);
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
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN,
					   parv[0]), nick);
			continue;
		    }
	
		/*
		** the following two cases allow masks in NOTICEs
		** (for OPERs only)
		**
		** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		*/
		if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
		    {
			if (!(s = (char *)rindex(nick, '.')))
			    {
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL,
					   parv[0]), nick);
				continue;
			    }
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			    {
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL,
					   parv[0]), nick);
				continue;
			    }
			if ((s = (char *)rindex(ME, '.')) &&
			    strcasecmp(rindex(nick, '.'), s))
			    {
				sendto_one(sptr, err_str(ERR_BADMASK,
					   parv[0]), nick);
				continue;
			    }
			sendto_match_butone(IsServer(cptr) ? cptr : NULL, 
					    sptr, nick + 1,
					    (*nick == '#') ? MATCH_HOST :
							     MATCH_SERVER,
					    ":%s %s %s :%s", parv[0],
					    cmd, nick, parv[2]);
			continue;
		    }
		
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
					sendto_one(sptr, err_str(
						   ERR_TOOMANYTARGETS,
						   parv[0]), "Duplicate", nick,
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
					sendto_one(sptr, err_str(
						   ERR_TOOMANYTARGETS,
						   parv[0]), "Duplicate", nick,
						   "No Message Delivered");
				continue;
			    }
		    }
		if (!notice)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK, parv[0]),
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

int	m_private(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int	m_notice(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	return m_message(cptr, sptr, parc, parv, 1);
}

/*
** who_one
**	sends one RPL_WHOREPLY to sptr concerning acptr & repchan
*/
static	void	who_one(sptr, acptr, repchan, lp)
aClient *sptr, *acptr;
aChannel *repchan;
Link *lp;
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
	sendto_one(sptr, rpl_str(RPL_WHOREPLY, sptr->name),
		   (repchan) ? (repchan->chname) : "*", acptr->user->username,
		   acptr->user->host, acptr->user->server, acptr->name,
		   status, acptr->hopcount, acptr->info);
}


/*
** who_channel
**	lists all users on a given channel
*/
static	void	who_channel(sptr, chptr, oper)
aClient *sptr;
aChannel *chptr;
int oper;
{
	Reg	Link	*lp;
	aChannel *channame;
	int	member;

	if (!IsAnonymous(chptr))
	    {
		member = IsMember(sptr, chptr);
		if (member || !SecretChannel(chptr))
			for (lp = chptr->members; lp; lp = lp->next)
			    {
				if (oper && !IsAnOper(lp->value.cptr))
					continue;
				if (IsInvisible(lp->value.cptr) && !member)
					continue;
				who_one(sptr, lp->value.cptr, chptr, lp);
			    }
	    }
	else if (lp = find_user_link(chptr->members, sptr))
		who_one(sptr, lp->value.cptr, chptr, lp);
}

/*
** who_find
**	lists all (matching) users.
**	CPU intensive, but what can be done?
*/
static	void	who_find(sptr, mask, oper)
aClient *sptr;
char *mask;
int oper;
{
	aChannel *chptr, *ch2ptr;
	Link	*lp;
	int	member;
	int	showperson, isinvis;
	aClient	*acptr;

	for (acptr = client; acptr; acptr = acptr->next)
	    {
		ch2ptr = NULL;
			
		if (!IsPerson(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;
		showperson = 0;
		/*
		 * Show user if they are on the same channel, or not
		 * invisible and on a non secret channel (if any).
		 * Do this before brute force match on all relevant
		 * fields since these are less cpu intensive (I
		 * hope :-) and should provide better/more shortcuts
		 * -avalon
		 */
		isinvis = IsInvisible(acptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		    {
			chptr = lp->value.chptr;
			if (IsAnonymous(chptr))
				continue;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (member || (!isinvis && PubChannel(chptr)))
			    {
				showperson = 1;
				if (!IsAnonymous(chptr) ||
				    acptr != sptr)
				    {
					ch2ptr = chptr;
					break;
				    }
			    }
			if (HiddenChannel(chptr) &&
			    !SecretChannel(chptr) && !isinvis)
				showperson = 1;
		    }
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		/*
		** This is brute force solution, not efficient...? ;( 
		** Show entry, if no mask or any of the fields match
		** the mask. --msa
		*/
		if (showperson &&
		    (!mask ||
		     match(mask, acptr->name) == 0 ||
		     match(mask, acptr->user->username) == 0 ||
		     match(mask, acptr->user->host) == 0 ||
		     match(mask, acptr->user->server) == 0 ||
		     match(mask, acptr->info) == 0))
			who_one(sptr, acptr, ch2ptr, NULL);
	    }
}

/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
int	m_who(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Link	*lp;
	aChannel *chptr, *mychannel;
	char	*channame = NULL;
	int	oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int	member, penalty = 0;
	char	*p, *mask;

	if (parc < 2)
	    {
		who_find(sptr, NULL, oper);
		sendto_one(sptr, rpl_str(RPL_ENDOFWHO, parv[0]), "*");
		return 5;
	    }

        for (p = NULL, mask = strtoken(&p, parv[1], ",");
	     mask && penalty < MAXPENALTY;
             mask = strtoken(&p, NULL, ","))
	    { 
		channame = NULL;
		mychannel = NullChn;
		clean_channelname(mask);
		if (sptr->user && (lp = sptr->user->channel))
				mychannel = lp->value.chptr;
		/*
		**  Following code is some ugly hacking to preserve the
		**  functions of the old implementation. (Also, people
		**  will complain when they try to use masks like "12tes*"
		**  and get people on channel 12 ;) --msa
		*/
		if (!mask || *mask == '\0') /* !mask always false? */
			mask = NULL;
		else if (mask[1] == '\0' && mask[0] == '*')
		    {
			mask = NULL;
			if (mychannel)
				channame = mychannel->chname;
		    }
		else if (mask[1] == '\0' && mask[0] == '0')
			/* "WHO 0" for irc.el */
			mask = NULL;
		else
			channame = mask;
		(void)collapse(mask);
		
		if (IsChannelName(channame))
		    {
			chptr = find_channel(channame, NULL);
			if (chptr)
				who_channel(sptr, chptr, oper);
			penalty += 1;
		    }
		else 
		    {
			who_find(sptr, mask, oper);
			if (mask && (int)strlen(mask) > 4)
				penalty += 3;
			else
				penalty += 5;
		    }
		sendto_one(sptr, rpl_str(RPL_ENDOFWHO, parv[0]),
			   BadPtr(mask) ?  "*" : mask);
	    }
	return penalty;
}

/* send_whois() is used by m_whois() to send whois reply to sptr, for acptr */
static void
send_whois(sptr, acptr)
aClient	*sptr, *acptr;
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
		NULL, NULL, NULL,	/* next, prev, bcptr */
		"<Unknown>",	/* user */
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

	sendto_one(sptr, rpl_str(RPL_WHOISUSER, sptr->name),
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
		sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS, sptr->name), name,
			   buf);

	sendto_one(sptr, rpl_str(RPL_WHOISSERVER, sptr->name),
		   name, user->server,
		   a2cptr ? a2cptr->info:"*Not On This Net*");

	if (user->flags & FLAGS_AWAY)
		sendto_one(sptr, rpl_str(RPL_AWAY, sptr->name), name,
			   (user->away) ? user->away : "Gone");

	if (IsAnOper(acptr))
		sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR, sptr->name), name);

	if (acptr->user && MyConnect(acptr))
		sendto_one(sptr, rpl_str(RPL_WHOISIDLE, sptr->name),
			   name, timeofday - user->last);
}

/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
int	m_whois(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Link	*lp;
	aClient *acptr;
	aChannel *chptr;
	char	*nick, *tmp;
	char	*p = NULL;
	int	found = 0;

    	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN, parv[0]));
		return 1;
	    }

	if (parc > 2)
	    {
		if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1,parc,parv) !=
		    HUNTED_ISME)
			return 3;
		parv[1] = parv[2];
	    }

	parv[1] = canonize(parv[1]);

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
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
					   err_str(ERR_NOSUCHNICK, parv[0]),
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
			sendto_one(sptr, err_str(ERR_NOSUCHNICK, parv[0]),
				   nick);
		    }
		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS, parv[0]), parv[1]);

	return 2;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
**	parv[5] = users mode (is only used internally by the server,
**		  NULL otherwise)
*/
int	m_user(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#define	UFLAGS	(FLAGS_INVISIBLE|FLAGS_WALLOP)
	char	*username, *host, *server, *realname;
	anUser	*user;

	/* Reject new USER */
	if (IsServer(sptr) || IsService(sptr) || sptr->user)
	    {
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED, parv[0]));
		return 1;
   	    }
	if (parc > 2 && (username = (char *)index(parv[1],'@')))
		*username = '\0'; 
	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "USER");
		if (IsServer(cptr))
		    {
			/* send error */
			sendto_flag(SCH_NOTICE,
				    "bad USER param count for %s from %s",
				    parv[0], get_client_name(cptr, FALSE));
			/*
			** and kill it, as there's no reason to expect more
			** USER messages about it, or we'll create a ghost.
			*/
			sendto_one(cptr,
				   ":%s KILL %s :%s (bad USER param count)",
				   ME, parv[0], ME);
			sptr->flags |= FLAGS_KILLED;
			exit_client(NULL, sptr, &me, "bad USER param count");
		    }
		return 1;
	    }

	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host     = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server   = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
	realname = (parc < 5 || BadPtr(parv[4])) ? "<bad-realname>" : parv[4];

 	user = make_user(sptr);

	if (!MyConnect(sptr))
	    {
		aClient	*acptr = NULL;
		aServer	*sp = NULL;

		if (!(sp = find_tokserver(atoi(server), cptr, NULL)))
		    {
			/*
			** Why? Why do we keep doing this?
			** s_service.c had the same kind of kludge.
			** Can't we get rid of this? - krys
			*/
			acptr = find_server(server, NULL);
			if (acptr)
				sendto_flag(SCH_ERROR,
			    "ERROR: SERVER:%s uses wrong syntax for NICK (%s)",
					    get_client_name(cptr, FALSE),
					    parv[0]);
		    }
		if (acptr)
			sp = acptr->serv;
		else if (!sp)
		    {
			sendto_flag(SCH_ERROR,
                        	    "ERROR: USER:%s without SERVER:%s from %s",
				    parv[0], server,
				    get_client_name(cptr, FALSE));
			ircstp->is_nosrv++;
			return exit_client(NULL, sptr, &me, "No Such Server");
		    }
		user->servp = sp;
		user->servp->refcnt++;

		Debug((DEBUG_DEBUG, "from %s user %s server %s -> %#x %s",
			parv[0], username, server, sp, sp->bcptr->name));
		strncpyzt(user->host, host, sizeof(user->host));
		user->server = find_server_string(sp->snum);
		goto user_finish;
	    }

	user->servp = me.serv;
	me.serv->refcnt++;
#ifndef	NO_DEFAULT_INVISIBLE
	SetInvisible(sptr);
#endif
	if (sptr->flags & FLAGS_RILINE)
		sptr->user->flags |= FLAGS_RESTRICTED;
	sptr->user->flags |= (UFLAGS & atoi(host));
	strncpyzt(user->host, host, sizeof(user->host));
	user->server = find_server_string(me.serv->snum);

user_finish:
	/* 
	** servp->userlist's are pointers into usrtop linked list.
	** Users aren't added to the top always, but only when they come
	** from a new server.
	*/
	if ((user->nextu = user->servp->userlist) == NULL)
	    {
		/* First user on this server goes to top of anUser list */
		user->nextu = usrtop;
		usrtop->prevu = user;
		usrtop = user;	/* user->prevu == usrtop->prevu == NULL */
	    } else {
		/*
		** This server already has users,
		** insert this new user in the middle of the anUser list,
		** update its neighbours..
		*/
		if (user->servp->userlist->prevu) /* previous user */
		    {
			user->prevu = user->servp->userlist->prevu;
			user->servp->userlist->prevu->nextu = user;
		    } else	/* user->servp->userlist == usrtop */
			usrtop = user; /* there is no previous user */
		user->servp->userlist->prevu = user; /* next user */
	    }
	user->servp->userlist = user;
	
	if (sptr->info != DefInfo)
		MyFree(sptr->info);
	if (strlen(realname) > REALLEN)
		realname[REALLEN] = '\0';
	sptr->info = mystrdup(realname);
#if defined(USE_IAUTH)
	if (MyConnect(sptr))
		sendto_iauth("%d U %.*s", sptr->fd, USERLEN+1, username);
#endif
	if (sptr->name[0]) /* NICK already received, now we have USER... */
	    {
		if ((parc == 6) && IsServer(cptr)) /* internal m_user() */
		    {
			char	*pv[4];

			pv[0] = ME;
			pv[1] = sptr->name;
			pv[2] = parv[5];
			pv[3] = NULL;
			m_umode(NULL, sptr, 3, pv);/*internal fake call again*/
			/* The internal m_umode does NOT propagate to 2.8
			** servers. (it can NOT since NICK/USER hasn't been
			** sent yet). See register_user()
			*/
		    }
		return register_user(cptr, sptr, sptr->name, username);
	    }
	else
		strncpyzt(sptr->user->username, username, USERLEN+1);
	return 2;
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	static	char	quitc[] = "I Quit";
	register char *comment = (parc > 1 && parv[1]) ? parv[1] : quitc;

	if (MyClient(sptr) || MyService(sptr))
		if (!strncmp("Local Kill", comment, 10) ||
		    !strncmp(comment, "Killed", 6))
			comment = quitc;
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';
	return IsServer(sptr) ? 0 : exit_client(cptr, sptr, sptr, comment);
    }

/*
** m_kill
**	parv[0] = sender prefix
**	parv[1] = kill victim
**	parv[2] = kill path
*/
int	m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*inpath = get_client_name(cptr,FALSE);
	char	*user, *path, *killer;
	int	chasing = 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "KILL");
		return 1;
	    }

	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */

	if (IsAnOper(cptr))
	    {
		if (BadPtr(path))
		    {
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
				   "KILL");
			return 1;
		    }
		if (strlen(path) > (size_t) TOPICLEN)
			path[TOPICLEN] = '\0';
	    }

	if (!(acptr = find_client(user, NULL)))
	    {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = get_history(user, (long)KILLCHASETIMELIMIT)))
		    {
			if (!IsServer(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHNICK, 
							 parv[0]), user);
			return 1;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   ME, parv[0], user, acptr->name);
		chasing = 1;
	    }
	if (!MyConnect(acptr) && IsLocOp(cptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES, parv[0]));
		return 1;
	    }
	if (IsServer(acptr) || IsMe(acptr))
	    {
		sendto_flag(SCH_ERROR, "%s tried to KILL server %s: %s %s %s",
			    sptr->name, acptr->name, parv[0], parv[1], parv[2]);
		sendto_one(sptr, err_str(ERR_CANTKILLSERVER, parv[0]),
			   acptr->name);
		return 1;
	    }

#ifdef	LOCAL_KILL_ONLY
	if (MyOper(sptr) && !MyConnect(acptr))
	    {
		sendto_one(sptr, ":%s NOTICE %s :Nick %s isnt on your server",
			   ME, parv[0], acptr->name);
		return 1;
	    }
#endif
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
		if (IsUnixSocket(cptr)) /* Don't use get_client_name syntax */
			inpath = me.sockhost;
		else
			inpath = cptr->sockhost;
		if (!BadPtr(path))
		    {
			SPRINTF(buf, "%s%s (%s)",
				cptr->name, IsOper(sptr) ? "" : "(L)", path);
			path = buf;
		    }
		else
			path = cptr->name;
	    }
	else if (BadPtr(path))
		 path = "*no-path*"; /* Bogus server sending??? */
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/
	if (IsLocOp(sptr) && !MyConnect(acptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES, parv[0]));
		return 1;
	    }
	sendto_flag(SCH_KILL,
		    "Received KILL message for %s. From %s Path: %s!%s",
		    acptr->name, parv[0], inpath, path);
#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
		syslog(LOG_DEBUG,"KILL From %s For %s Path %s!%s",
			parv[0], acptr->name, inpath, path);
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
	    {
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
		SPRINTF(buf2, "Local Kill by %s (%s)", sptr->name,
			BadPtr(parv[2]) ? sptr->name : parv[2]);
	    }
	else
	    {
		if ((killer = index(path, ' ')))
		    {
			while (*killer && *killer != '!')
				killer--;
			if (!*killer)
				killer = path;
			else
				killer++;
		    }
		else
			killer = path;
		SPRINTF(buf2, "Killed (%s)", killer);
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
int	m_away(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
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
			sendto_serv_butone(cptr, ":%s MODE %s :-a", parv[0],
					   parv[0]);
		/* sendto_serv_butone(cptr, ":%s AWAY", parv[0]); */
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_UNAWAY, parv[0]));
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
		sendto_serv_butone(cptr, ":%s MODE %s :+a", parv[0], parv[0]);
	    }

	sptr->user->flags |= FLAGS_AWAY;
	if (MyConnect(sptr))
	    {
		sptr->user->away = away;
		(void)strcpy(away, awy2);
		sendto_one(sptr, rpl_str(RPL_NOWAWAY, parv[0]));
	    }
	return 2;
}

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_ping(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN, parv[0]));
		return 1;
	    }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */

	acptr = find_client(origin, NULL);
	if (!acptr)
		acptr = find_server(origin, NULL);
	if (!acptr || acptr != sptr)
		origin = cptr->name;
	if (!BadPtr(destination) && mycmp(destination, ME) != 0)
	    {
		if ((acptr = find_server(destination, NULL)))
			sendto_one(acptr,":%s PING %s :%s", parv[0],
				   origin, destination);
	    	else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER, parv[0]),
				   destination);
			return 1;
		    }
	    }
	else
		sendto_one(sptr, ":%s PONG %s :%s", ME,
			   (destination) ? destination : ME, origin);
	return 1;
    }

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_pong(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	char	*origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN, parv[0]));
		return 1;
	    }

	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;

	if (!BadPtr(destination) && mycmp(destination, ME) != 0)
	    {
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server(destination, NULL))) {
			if (!(MyClient(sptr) && mycmp(origin, sptr->name)))
				sendto_one(acptr,":%s PONG %s %s",
					   parv[0], origin, destination);
		} else
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER, parv[0]),
				   destination);
		return 2;
	    }
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
		      destination ? destination : "*"));
#endif
	return 1;
    }


/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
int	m_oper(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	aConfItem *aconf;
	char	*name, *password, *encr;
#ifdef CRYPT_OPER_PASSWORD
	char	salt[3];
	extern	char *crypt();
#endif /* CRYPT_OPER_PASSWORD */

	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;

	if (!IsServer(cptr) && (BadPtr(name) || BadPtr(password)))
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "OPER");
		return 1;
	    }
	
	/* if message arrived from server, trust it, and set to oper */
	    
	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	    {
		/* we never get here, do we?? (counters!) -krys */
		sptr->user->flags |= FLAGS_OPER;
		sendto_serv_butone(cptr, ":%s MODE %s :+o", parv[0], parv[0]);
		if (IsMe(cptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER, parv[0]));
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->server, 
				      sptr, ":%s MODE %s :+o", parv[0], 
				      parv[0]);
#endif
		return 1;
	    }
	else if (IsAnOper(sptr))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER, parv[0]));
		return 1;
	    }
	if (!(aconf = find_conf_exact(name, sptr->username, sptr->sockhost,
				      CONF_OPS)) &&
	    !(aconf = find_conf_exact(name, sptr->username,
#ifdef INET6
				      (char *)inetntop(AF_INET6,
						       (char *)&cptr->ip,
						       mydummy, MYDUMMY_SIZE),
#else
				      (char *)inetntoa((char *)&cptr->ip),
#endif
				      CONF_OPS)))
	    {
		sendto_one(sptr, err_str(ERR_NOOPERHOST, parv[0]));
		return 1;
	    }
#ifdef CRYPT_OPER_PASSWORD
	/* use first two chars of the password they send in as salt */

	/* passwd may be NULL. Head it off at the pass... */
	salt[0] = '\0';
	if (password && aconf->passwd)
	    {
		/* Determine if MD5 or DES */
		if (strncmp(aconf->passwd, "$1$", 3))
		    {
			salt[0] = aconf->passwd[0];
			salt[1] = aconf->passwd[1];
		    }
		else
		    {
			salt[0] = aconf->passwd[3];
			salt[1] = aconf->passwd[4];
		    }
		salt[2] = '\0';
		encr = crypt(password, salt);
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
#ifdef	OPER_REMOTE
		if (aconf->status == CONF_LOCOP)
#else
		if ((match(s,me.sockhost) && !IsLocal(sptr)) ||
		    aconf->status == CONF_LOCOP)
#endif
			SetLocOp(sptr);
		else
			SetOper(sptr);
		*--s =  '@';
		sendto_flag(SCH_NOTICE, "%s (%s@%s) is now operator (%c)",
			    parv[0], sptr->user->username, sptr->user->host,
			   IsOper(sptr) ? 'o' : 'O');
		send_umode_out(cptr, sptr, old);
 		sendto_one(sptr, rpl_str(RPL_YOUREOPER, parv[0]));
#if !defined(CRYPT_OPER_PASSWORD) && (defined(FNAME_OPERLOG) ||\
    (defined(USE_SYSLOG) && defined(SYSLOG_OPER)))
		encr = "";
#endif
#if defined(USE_SYSLOG) && defined(SYSLOG_OPER)
		syslog(LOG_INFO, "OPER (%s) (%s) by (%s!%s@%s) [%s@%s]",
			name, encr,
		       parv[0], sptr->user->username, sptr->user->host,
		       sptr->auth, IsUnixSocket(sptr) ? sptr->sockhost :
#ifdef INET6
                       inet_ntop(AF_INET6, (char *)&sptr->ip), mydummy, MYDUMMY_SIZE);
#else
                       inetntoa((char *)&sptr->ip));
#endif
#endif
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
		    (logfile = open(FNAME_OPERLOG, O_WRONLY|O_APPEND)) != -1)
		{
		  (void)alarm(0);
		  SPRINTF(buf, "%s OPER (%s) (%s) by (%s!%s@%s) [%s@%s]\n",
			  myctime(timeofday), name, encr,
			  parv[0], sptr->user->username, sptr->user->host,
			  sptr->auth, IsUnixSocket(sptr) ? sptr->sockhost :
#ifdef INET6
			  inetntop(AF_INET6, (char *)&sptr->ip, mydummy,
				   MYDUMMY_SIZE));
#else
			  inetntoa((char *)&sptr->ip));
#endif
		  (void)alarm(3);
		  (void)write(logfile, buf, strlen(buf));
		  (void)alarm(0);
		  (void)close(logfile);
		}
		(void)alarm(0);
		/* Modification by pjg */
	      }
#endif
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->server, 
				      sptr, ":%s MODE %s :+%c", parv[0],
				      parv[0], IsOper(sptr) ? 'O' : 'o');
#endif
		if (IsAnOper(sptr))
			istat.is_oper++;
	    }
	else
	    {
		(void)detach_conf(sptr, aconf);
		sendto_one(sptr,err_str(ERR_PASSWDMISMATCH, parv[0]));
	    }
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
int	m_pass(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *password = parc > 1 ? parv[1] : NULL;

	if (BadPtr(password))
	    {
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "PASS");
		return 1;
	    }
	/* Temporarily store PASS pwd *parameters* into info field */
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
	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	return 1;
    }

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int	m_userhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char	*p = NULL;
	aClient	*acptr;
	Reg	char	*s;
	Reg	int	i, len;
	int	idx = 1;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
			   "USERHOST");
		return 1;
	    }

	(void)strcpy(buf, rpl_str(RPL_USERHOST, parv[0]));
	len = strlen(buf);
	*buf2 = '\0';

	for (i = 5, s = strtoken(&p, parv[idx], " "); i && s; i--)
	     {
		if ((acptr = find_person(s, NULL)))
		    {
			if (*buf2)
				(void)strcat(buf, " ");
			SPRINTF(buf2, "%s%s=%c%s@%s", acptr->name,
				IsAnOper(acptr) ? "*" : "",
				(acptr->user->flags & FLAGS_AWAY) ? '-' : '+',
				acptr->user->username, acptr->user->host);
			(void)strncat(buf, buf2, sizeof(buf) - len);
			len += strlen(buf2);
			if (len > BUFSIZE - (NICKLEN + 5 + HOSTLEN + USERLEN))
			    {
				sendto_one(sptr, "%s", buf);
				(void)strcpy(buf, rpl_str(RPL_USERHOST,
					     parv[0]));
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

int	m_ison(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aClient *acptr;
	Reg	char	*s, **pav = parv;
	Reg	int	len = 0;
	char	*p = NULL;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "ISON");
		return 1;
	    }

	(void)strcpy(buf, rpl_str(RPL_ISON, *parv));
	len = strlen(buf);

	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
		if ((acptr = find_person(s, NULL)))
		    {
			(void) strcpy(buf + len, acptr->name);
			len += strlen(acptr->name);
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
int	m_umode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	int	flag;
	Reg	int	*s;
	Reg	char	**p, *m;
	aClient	*acptr = NULL;
	int	what, setflags, penalty = 0;

	what = MODE_ADD;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]), "MODE");
		return 1;
	    }

	if (cptr && !(acptr = find_person(parv[1], NULL)))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL, parv[0]),
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
			sendto_ops_butone(NULL, &me,
				  ":%s WALLOPS :MODE for User %s From %s!%s",
				  ME, parv[1],
				  get_client_name(cptr, FALSE), sptr->name);
		else
			sendto_one(sptr, err_str(ERR_USERSDONTMATCH, parv[0]));
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
		sendto_one(sptr, rpl_str(RPL_UMODEIS, parv[0]), buf);
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
						      sptr->user->server, sptr,
						      ":%s AWAY", parv[0]);
#endif
				    }
#ifdef  USE_SERVICES
				if (what == MODE_ADD)
				check_services_butone(SERVICE_WANT_AWAY,
						      sptr->user->server, sptr,
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
					sendto_one(sptr, err_str(
						ERR_UMODEUNKNOWNFLAG, parv[0]),
						*m);
				break;
			}
	/*
	 * stop users making themselves operators too easily
	 */
	if (cptr)
	    {
		if (!(setflags & FLAGS_OPER) && IsOper(sptr) &&
		    !IsServer(cptr))
			ClearOper(sptr);
		if (!(setflags & FLAGS_LOCOP) && IsLocOp(sptr) &&
		    !IsServer(cptr))
			sptr->user->flags &= ~FLAGS_LOCOP;
		if ((setflags & FLAGS_RESTRICTED) &&
		    !(sptr->user->flags & FLAGS_RESTRICTED))
		    {
			sendto_one(sptr, err_str(ERR_RESTRICTED, parv[0]));
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
		    }
		if (IsInvisible(sptr) && !(setflags & FLAGS_INVISIBLE))
		    {
			istat.is_user[1]++;
			istat.is_user[0]--;
		    }
		send_umode_out(cptr, sptr, setflags);
	    }

	/* update counters */	   
	if (IsOper(sptr) && !(setflags & FLAGS_OPER))
	    {
		istat.is_oper++;
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->server, 
				      sptr, ":%s MODE %s :+o", parv[0],
				      parv[0]);
#endif
	    }
	else if (!IsOper(sptr) && (setflags & FLAGS_OPER))
	    {
		istat.is_oper--;
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->server,
				      sptr, ":%s MODE %s :-o", parv[0],
				      parv[0]);
#endif
	    }
	else if (MyConnect(sptr) && !IsLocOp(sptr) && (setflags & FLAGS_LOCOP))
	    {
		istat.is_oper--;
#ifdef USE_SERVICES
		check_services_butone(SERVICE_WANT_OPER, sptr->user->server,
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
void	send_umode(cptr, sptr, old, sendmask, umode_buf)
aClient *cptr, *sptr;
int	old, sendmask;
char	*umode_buf;
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
void	send_umode_out(cptr, sptr, old)
aClient *cptr, *sptr;
int	old;
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
				   sptr->name, sptr->name, buf);
		    }

	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, old, ALL_UMODES, buf);
#ifdef USE_SERVICES
	/* buf contains all modes for local users, and iow only for remotes */
	if (*buf)
		check_services_butone(SERVICE_WANT_UMODE, NULL, sptr,
				      ":%s MODE %s :%s", sptr->name,
				      sptr->name, buf);
#endif
}
