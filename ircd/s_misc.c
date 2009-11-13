/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static const volatile char rcsid[] = "@(#)$Id: s_misc.c,v 1.119 2009/11/13 20:08:11 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_MISC_C
#include "s_externs.h"
#undef S_MISC_C

static	void	exit_one_client (aClient *, aClient *, aClient *, const char *);
static	void	exit_server(aClient *, aClient *, aClient *, const char *,
			const char *);


static	char	*months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static	char	*weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

/*
 * stats stuff
 */
struct	stats	ircst, *ircstp = &ircst;

char	*date(time_t clock) 
{
	static	char	buf[80], plus;
	Reg	struct	tm *lt, *gm;
	struct	tm	gmbuf;
	int	minswest;

	if (!clock) 
		time(&clock);
	gm = gmtime(&clock);
	bcopy((char *)gm, (char *)&gmbuf, sizeof(gmbuf));
	gm = &gmbuf;
	lt = localtime(&clock);

	minswest = (gm->tm_hour - lt->tm_hour) * 60 
		    + (gm->tm_min - lt->tm_min);	
	if (lt->tm_yday != gm->tm_yday)
	    {
		if ((lt->tm_yday > gm->tm_yday 
		    && lt->tm_year == gm->tm_year) 
		    || (lt->tm_yday < gm->tm_yday 
		    && lt->tm_year != gm->tm_year)) 
		    {
			minswest -= 24 * 60;
		    }
		else
		    {
			minswest += 24 * 60;
		    }
	    }

	plus = (minswest > 0) ? '-' : '+';
	if (minswest < 0)
		minswest = -minswest;

	(void)sprintf(buf, "%s %s %d %d -- %02d:%02d %c%02d:%02d",
		weekdays[lt->tm_wday], months[lt->tm_mon],lt->tm_mday,
		lt->tm_year + 1900, lt->tm_hour, lt->tm_min,
		plus, minswest/60, minswest%60);

	return buf;
}

/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int	check_registered_user(aClient *sptr)
{
	if (!IsRegisteredUser(sptr))
	    {
		sendto_one(sptr, replies[ERR_NOTREGISTERED], ME, "*");
		return -1;
	    }
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int	check_registered(aClient *sptr)
{
	if (!IsRegistered(sptr))
	    {
		sendto_one(sptr, replies[ERR_NOTREGISTERED], ME, "*");
		return -1;
	    }
	return 0;
}

/*
** check_registered_service cancels message, if 'x' is not
** a registered service.
*/
int	check_registered_service(aClient *sptr)
{
	if (!IsService(sptr))
	    {
		sendto_one(sptr, replies[ERR_NOTREGISTERED], ME, "*");
		return -1;
	    }
	return 0;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "name[user@ip#.port]" if 'showip' is true;
**	  "name[username@sockethost]", if name and sockhost are different and
**	  showip is false; else
**	  "name".
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/

char	*get_client_name(aClient *sptr, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (MyConnect(sptr))
	    {
#ifdef UNIXPORT
		if (IsUnixSocket(sptr))
		    {
			if (showip)
				sprintf(nbuf, "%s[%s]",
					sptr->name, sptr->sockhost);
			else
				sprintf(nbuf, "%s[%s]",
					sptr->name, me.sockhost);
		    }
		else
#endif
		    {
			if (showip)
				(void)sprintf(nbuf, "%s[%.*s@%s]",
					sptr->name, USERLEN,
					(!(sptr->flags & FLAGS_GOTID)) ? "" :
					sptr->auth, sptr->user ? sptr->user->sip :
#ifdef INET6 
					      inetntop(AF_INET6,
						       (char *)&sptr->ip,
						       ipv6string, sizeof(ipv6string))
#else
					      inetntoa((char *)&sptr->ip)
#endif
					);
			else
			    {
				if (mycmp(sptr->name, sptr->sockhost))
					/* Show username for clients and
					 * ident for others.
					 */
					sprintf(nbuf, "%s[%.*s@%s]",
						sptr->name, USERLEN,
						IsPerson(sptr) ?
							sptr->user->username :
							sptr->auth,
						IsPerson(sptr) ? sptr->user->host :
						sptr->sockhost);
				else
					return sptr->name;
			    }
		    }
		return nbuf;
	    }
	return sptr->name;
}

char	*get_client_host(aClient *cptr)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (!MyConnect(cptr))
		return cptr->name;
	if (!cptr->user)
		return get_client_name(cptr, TRUE);
#ifdef UNIXPORT
	if (IsUnixSocket(cptr))
		sprintf(nbuf, "%s[%s]", cptr->name, ME);
	else
#endif
		(void)sprintf(nbuf, "%s[%-.*s@%-.*s]",
			cptr->name, USERLEN,
			(!(cptr->flags & FLAGS_GOTID)) ? "" : cptr->auth,
			HOSTLEN, cptr->user->sip);
	return nbuf;
}

/*
 * Form sockhost such that if the host is of form user@host, only the host
 * portion is copied.
 */
void	get_sockhost(aClient *cptr, char *host)
{
	Reg	char	*s;

	if (!cptr || !host)
	{
		/* however unlikely this is, don't risk */
		return;
	}
	if ((s = (char *)index(host, '@')))
	{
		s++;
	}
	else
	{
		s = host;
	}
	strncpyzt(cptr->sockhost, s, sizeof(cptr->sockhost));
	Debug((DEBUG_DNS,"get_sockhost %s",s));
}

/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */
char	*my_name_for_link(char *name, int count)
{
	static	char	namebuf[HOSTLEN];
	Reg	char	*start = name;

	if (count <= 0 || count > 5)
		return start;

	while (count-- && name)
	    {
		name++;
		name = (char *)index(name, '.');
	    }
	if (!name)
		return start;

	namebuf[0] = '*';
	(void)strncpy(&namebuf[1], name, HOSTLEN - 1);
	namebuf[HOSTLEN - 1] = '\0';

	return namebuf;
}

/*
 * Goes thru the list of locally connected servers (except cptr),
 * check if my neighbours can see the server "server" (or if it is hidden
 * by a hostmask)
 * Returns the number of marked servers
 */
int	mark_blind_servers (aClient *cptr, aClient *server)
{
	Reg	int		i, j = 0;
	Reg	aClient		*acptr;
	Reg	aConfItem	*aconf;
	
	for (i = fdas.highest; i >= 0; i--)
	{
		if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr))
			continue;
		if (acptr == cptr->from || IsMe(acptr))
		{
			acptr->flags &= ~FLAGS_HIDDEN;
			continue;
		}
		if (((aconf = acptr->serv->nline) &&
			!match(my_name_for_link(ME, aconf->port), server->name)))
		{
			acptr->flags |= FLAGS_HIDDEN;
			j++;
		}
		else
		{
			acptr->flags &= ~FLAGS_HIDDEN;
		}
	}
	return j;
}

/*
** exit_server(): Removes all dependent servers and clients, and
** sends the right messages to the right client/servers.
**
** We will send all SQUITs to &servers, and QUITs to local users.
** We only send 1 SQUIT to a 2.11 servers.
** We send all SQUITs to a 2.10 servers that can see it, or QUITs otherwise.
**
** Argument:
**	cptr: The real server to SQUIT.
**	acptr: One of the depended servers to SQUIT.
**	from: Originator of SQUIT.
**	comment: The original comment for the SQUIT. (Only for cptr itself.)
**	comment2: The comment for (S)QUIT reasons for the rest.
*/
static	void	exit_server(aClient *cptr, aClient *acptr, aClient *from,
			const char *comment, const char *comment2)
{
	aClient	*acptr2;
	int	flags;

	/* Remove all the servers recursively. */
	while (acptr->serv->down)
	{
		exit_server(cptr, acptr->serv->down, from, comment, comment2);
	}
	/* Here we should send "Received SQUIT" for last server,
	** but exit_client() is doing (well, almost) this --Beeth */

	/* This server doesn't have any depedent servers anymore, only
	** users/services left. */

	flags = FLAGS_SPLIT;

	/*
	** We'll mark all servers that can't see that server as hidden.
	** If we found any, we'll also mark all users on that server hidden.
	** If a user is marked hidden, and we try to send it to a currently
	** marked server, the server can't see that user's server.
	** Note that a 2.11 can see it, so we don't have to send the QUITs
	** to it.
	*/
	if (mark_blind_servers(cptr, acptr))
	{
		flags |= FLAGS_HIDDEN;
	}

	/* Quit all users and services. */
	while (GotDependantClient(acptr))
	{
		acptr2 = acptr->prev;
		acptr2->flags |= flags;
		exit_one_client(cptr->from, acptr2, from, comment2);
	}

	/* Make sure we only send the last SQUIT to a 2.11 server. */
	if (acptr == cptr)
	{
		acptr->flags |= FLAGS_SQUIT;
	}
	if (!IsMasked(acptr))
	{
		sendto_flag(SCH_SERVER,
			"Received SQUIT %s from %s (%s)", acptr->name,
			from->name,
			acptr == cptr ? comment : comment2);
	}
	exit_one_client(cptr->from, acptr, from,
		acptr == cptr ? comment : comment2);
	
	return;
}

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
**
**	Parameters:
**
**	aClient *cptr
** 		The local client originating the exit or NULL, if this
** 		exit is generated by this server for internal reasons.
** 		This will not get any of the generated messages.
**	aClient *sptr
**		Client exiting
**	aClient *from
**		Client firing off this Exit, never NULL!
**	char	*comment
**		Reason for the exit
*/
int	exit_client(aClient *cptr, aClient *sptr, aClient *from,
		const char *comment)
{
	char	comment1[HOSTLEN + HOSTLEN + 2];

	if (MyConnect(sptr))
	{
		if (sptr->flags & FLAGS_KILLED)
		{
			sendto_flag(SCH_NOTICE, "Killed: %s.",
				    get_client_name(sptr, TRUE));
			sptr->exitc = EXITC_KILL;
		}

		sptr->flags |= FLAGS_CLOSING;
#if (defined(FNAME_USERLOG) || defined(FNAME_CONNLOG) \
     || defined(USE_SERVICES)) \
    || (defined(USE_SYSLOG) && (defined(SYSLOG_USERS) || defined(SYSLOG_CONN)))
		if (IsPerson(sptr))
		{
# if defined(FNAME_USERLOG) || defined(USE_SERVICES) || \
	(defined(USE_SYSLOG) && defined(SYSLOG_USERS))
			sendto_flog(sptr, EXITC_REG, sptr->user->username,
				    sptr->user->host);
# endif
# if defined(CLIENTS_CHANNEL) && (CLIENTS_CHANNEL_LEVEL & CCL_QUIT)
			sendto_flag(SCH_CLIENT, "%s %s %s %s QUIT %c"
#  if (CLIENTS_CHANNEL_LEVEL & CCL_QUITINFO)
				" :%s"
#  endif
				, sptr->user->uid, sptr->name,
				sptr->user->username, sptr->user->host,
				sptr->exitc
#  if (CLIENTS_CHANNEL_LEVEL & CCL_QUITINFO)
				, comment
#  endif
				);
# endif
		}
		else if (!IsService(sptr))
		{
# if defined(FNAME_CONNLOG) || defined(USE_SERVICES) || \
	(defined(USE_SYSLOG) && defined(SYSLOG_CONN))
			if (sptr->exitc == '\0' || sptr->exitc == EXITC_REG)
			{
				sptr->exitc = EXITC_UNDEF;
			}
			sendto_flog(sptr, sptr->exitc,
				    sptr->user && sptr->user->username ?
				    sptr->user->username : "",
#ifdef UNIXPORT
				    (IsUnixSocket(sptr)) ? me.sockhost :
#endif
				    ((sptr->hostp) ? sptr->hostp->h_name :
				     sptr->sockhost));
# endif
		}
#endif
		if (MyConnect(sptr))
		{
			if (IsPerson(sptr))
			{
				istat.is_myclnt--;
			}
			else if (IsServer(sptr))
			{
				istat.is_myserv--;
			}
			else if (IsService(sptr))
			{
				istat.is_myservice--;
			}
			else
			{
				istat.is_unknown--;
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
			/* Send SQUIT message to 2.11 servers to tell them
			 * the squit reason for rebroadcast on the other side
			 * - jv
			 */
			if (IsServer(sptr))
			{
				sendto_one(sptr, ":%s SQUIT %s :%s",
					me.serv->sid, sptr->serv->sid,
					comment);
			}

			if (cptr != NULL && sptr != cptr)
			{
				sendto_one(sptr, "ERROR :Closing Link: "
					"%s %s (%s)",
					get_client_name(sptr,FALSE),
					cptr->name, comment);
			}
			else
			{
				sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
					get_client_name(sptr,FALSE), comment);
			}

			if (sptr->auth != sptr->username)
			{
				istat.is_authmem -= strlen(sptr->auth) + 1;
				istat.is_auth -= 1;
				MyFree(sptr->auth);
				sptr->auth = sptr->username;
			}
		}
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		** now, I think it harms big client servers... - krys
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
		close_connection(sptr);

	} /* if (MyConnect(sptr) */

 	if (IsServer(sptr))
 	{
		/* Remove all dependent servers and clients. */
		if (!IsMasked(sptr))
		{
			sprintf(comment1, "%s %s", sptr->serv->up->name,
				sptr->name);
		}
		else
		{
			/* It was a masked server, the squit reason should
			** give the right quit reason for clients. */
			strncpyzt(comment1, comment, sizeof(comment1));
		}
		/* cptr != sptr means non-local server */
		if (cptr != sptr && 
			nextconnect == 0 && find_conf_name(sptr->name,
			(CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER)))
		{
			/* try AC */
			nextconnect = timeofday + HANGONRETRYDELAY;
		}
		exit_server(sptr, sptr, from, comment, comment1);
		check_split();
		if ((cptr == sptr))
		{
			/* It serves no purpose. --B.
			sendto_flag(SCH_SERVER, "Sending SQUIT %s (%s)",
				cptr->name, comment);
			*/
			return FLUSH_BUFFER;
		}
		return 0;
 	}
	
	/*
	** Try to guess from comment if the client is exiting
	** normally (KILL or issued QUIT), or if it is splitting
	** It requires comment for splitting users to be
	** "server.some.where splitting.some.where"
	*/
	comment1[0] = '\0';
	if ((sptr->flags & FLAGS_KILLED) == 0)
	{
		if (comment[0] == '"')
		{
			/* definitely user quit, see m_quit */
			sptr->flags |= FLAGS_QUIT;
		}
		else
		{
		        const char *c = comment;
			int i = 0;
			while (*c && *c != ' ')
				if (*c++ == '.')
					i++;
			if (*c++ && i)
			{
			        i = 0;
				while (*c && *c != ' ')
					if (*c++ == '.')
						i++;
				if (!i || *c)
					sptr->flags |= FLAGS_QUIT;
			}
			else
			{
				sptr->flags |= FLAGS_QUIT;
			}
		}

		if (sptr == cptr && !(sptr->flags & FLAGS_QUIT))
		{
			/*
			** This will avoid nick delay to be abused by
			** letting local users put a comment looking
			** like a server split.
			*/
			strncpyzt(comment1, comment, HOSTLEN + HOSTLEN);
			strcat(comment1, " ");
			sptr->flags |= FLAGS_QUIT;
		}
	}
	
	exit_one_client(cptr, sptr, from, (*comment1) ? comment1 : comment);
	/* XXX: we probably should not call it every client exit */
	/* checking every server quit should suffice --B. */
	/* check_split(); */
	return cptr == sptr ? FLUSH_BUFFER : 0;
    }

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static	void	exit_one_client(aClient *cptr, aClient *sptr, aClient *from,
	const char *comment)
{
	Reg	aClient *acptr;
	Reg	int	i;
	Reg	Link	*lp;
	invLink		*ilp;

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
	{
		sendto_flag(SCH_ERROR,
			    "ERROR: tried to exit me! : %s", comment);
		return;	/* ...must *never* exit self!! */
	}
	else if (IsServer(sptr))
	{
		/*
		** Old sendto_serv_but_one() call removed because we now
		** need to send different names to different servers
		** (domain name matching)
		*/
		if (!IsMasked(sptr))
		{
			istat.is_serv--;
		}
		if (!IsBursting(sptr))
		{
			istat.is_eobservers--;
		}
	 	for (i = fdas.highest; i >= 0; i--)
		{
			if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr) ||
			    acptr == cptr || IsMe(acptr))
			{
				continue;
			}
			if (!(sptr->flags & FLAGS_SQUIT))
			{
				/* Make sure we only send the last SQUIT
				** to a 2.11. */
				continue;
			}
			if ((acptr->flags & FLAGS_HIDDEN) &&
				!IsMasked(sptr))
			{
				/* We need a special SQUIT reason, so
				** the remote server can send the
				** right quit message. */
				sendto_one(acptr, ":%s SQUIT %s :%s %s",
					sptr->serv->up->serv->sid,
					sptr->serv->sid,
					sptr->serv->up->name,
					sptr->name);
			}
			else
			{
				sendto_one(acptr, ":%s SQUIT %s :%s",
					sptr->serv->up->serv->sid,
					sptr->serv->sid, comment);
			}
		}
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_SQUIT, sptr->serv, sptr,
				      ":%s SQUIT %s :%s", from->name,
				      sptr->name, comment);
#endif
		del_from_sid_hash_table(sptr->serv);
		remove_server_from_tree(sptr);
		/* remove server from svrtop */
		unregister_server(sptr);
	}
	else if (!IsPerson(sptr) && !IsService(sptr))
	{
				    /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty hole in the server... --msa
				    */
		; /* Nothing */
	}
	else if (sptr->name[0] && !IsService(sptr)) /* clean with QUIT... */
	{
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
		{
			if ((sptr->flags & FLAGS_SPLIT) == 0)
			{
				sendto_serv_butone(cptr, ":%s QUIT :%s",
						   sptr->user->uid, comment);
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_QUIT|
						      SERVICE_WANT_RQUIT, 
						      (sptr->user) ?
						      sptr->user->servp
						      : NULL, cptr,
						      ":%s QUIT :%s",
						      sptr->name, comment);
#endif
			}
			else
			{
				if (sptr->flags & FLAGS_HIDDEN)
					/* joys of hostmasking */
					for (i = fdas.highest; i >= 0; i--)
					{
						if (!(acptr =local[fdas.fd[i]])
						    || acptr == cptr
						    || IsMe(acptr))
							continue;
						if (acptr->flags & FLAGS_HIDDEN)
							sendto_one(acptr,
								":%s QUIT :%s",
								sptr->user->uid,
								comment);
					}
#ifdef	USE_SERVICES
				check_services_butone(SERVICE_WANT_QUIT, 
					      (sptr->user) ? sptr->user->servp
						      : NULL, cptr,
						      ":%s QUIT :%s",
						      sptr->name, comment);
#endif
			}
		}
#ifdef USE_SERVICES
		else
		{
			/* Send QUIT to services which desire such as well.
			** Services with both _QUIT and _KILL will get both
			** for now --jv
			*/
			check_services_butone(SERVICE_WANT_QUIT, 
					     (sptr->user) ? sptr->user->servp
						      : NULL, cptr,
						      ":%s QUIT :%s",
						      sptr->name, comment);

		}
#endif
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user)
		{
			if (IsInvisible(sptr))
			{
				istat.is_user[1]--;
				sptr->user->servp->usercnt[1]--;
			}
			else
			{
				istat.is_user[0]--;
				sptr->user->servp->usercnt[0]--;
			}
			if (IsAnOper(sptr))
			{
				sptr->user->servp->usercnt[2]--;
				istat.is_oper--;
			}
			sendto_common_channels(sptr, ":%s QUIT :%s",
						sptr->name, comment);

			if (!(acptr = cptr ? cptr : sptr->from))
				acptr = sptr;
			while ((lp = sptr->user->channel))
			{
				/*
				** Mark channels from where remote chop left,
				** this will eventually lock the channel.
				** close_connection() has already been called,
				** it makes MyConnect == False - krys
				*/
				if (sptr != cptr)
				{
					if (*lp->value.chptr->chname == '!')
					{
						if (!(sptr->flags &FLAGS_QUIT))
							lp->value.chptr->history = timeofday + LDELAYCHASETIMELIMIT;
					}
					else if (
#ifndef BETTER_CDELAY
						 !(sptr->flags & FLAGS_QUIT) &&
#endif
						 is_chan_op(sptr, lp->value.chptr))
					{
						lp->value.chptr->history = timeofday + DELAYCHASETIMELIMIT;
					}
				}
				if (IsAnonymous(lp->value.chptr) &&
				    !IsQuiet(lp->value.chptr))
				{
					sendto_channel_butserv(lp->value.chptr, sptr, ":%s PART %s :None", sptr->name, lp->value.chptr->chname);
				}
				remove_user_from_channel(sptr,lp->value.chptr);
			}

			/* Clean up invitefield */
			while ((ilp = sptr->user->invited))
			{
				del_invite(sptr, ilp->chptr);
				/* again, this is all that is needed */
			}

			/* remove from uid hash table */
			if (sptr->user)
			{
				del_from_uid_hash_table(sptr->user->uid, sptr);
			}

			/* Add user to history */
#ifndef BETTER_NDELAY
			add_history(sptr, (sptr->flags & FLAGS_QUIT) ?
				    &me : NULL);
#else
			add_history(sptr, (sptr == cptr) ? &me : NULL);
#endif
			off_history(sptr);
#ifdef USE_HOSTHASH
			del_from_hostname_hash_table(sptr->user->host,
						     sptr->user);
#endif
#ifdef USE_IPHASH
			del_from_ip_hash_table(sptr->user->sip, sptr->user);
#endif
		    }
	    }
	else if (sptr->name[0] && IsService(sptr))
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((sptr->flags & FLAGS_KILLED) == 0)
		{
			/*
			** A service quitting is annoying, It has to be sent
			** to connected servers depending on 
			** sptr->service->dist
			*/
			for (i = fdas.highest; i >= 0; i--)
			    {
				if (!(acptr = local[fdas.fd[i]])
				    || !IsServer(acptr) || acptr == cptr
				    || IsMe(acptr))
				{
					continue;
				}
				if (match(sptr->service->dist, acptr->name) && 
					match(sptr->service->dist, acptr->serv->sid))
				{
					continue;
				}
				sendto_one(acptr, ":%s QUIT :%s", sptr->name,
					   comment);
			}
		}
#ifdef	USE_SERVICES
		check_services_butone(SERVICE_WANT_SERVICE, NULL, NULL,
				      ":%s QUIT :%s", sptr->name, comment);
#endif
		/* MyConnect(sptr) is always FALSE here */
		if (cptr == sptr)
		{
			sendto_flag(SCH_NOTICE, "Service %s disconnected",
				    get_client_name(sptr, TRUE));
		}
		sendto_flag(SCH_SERVICE, "Received QUIT %s from %s (%s)",
			    sptr->name, from->name, comment);
		istat.is_service--;
	    }

	/* Remove sptr from the client list */
	if (del_from_client_hash_table(sptr->name, sptr) != 1)
	{
		Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
			sptr, sptr->name,
			sptr->from ? sptr->from->sockhost : "??host",
			sptr->from, sptr->next, sptr->prev, sptr->fd,
			sptr->status, sptr->user));
	}
	remove_client_from_list(sptr);
	return;
}

void	checklist(void)
{
	Reg	aClient	*acptr;
	Reg	int	i,j;

	if (!(bootopt & BOOT_AUTODIE))
		return;
	for (j = i = 0; i <= highest_fd; i++)
		if (!(acptr = local[i]))
			continue;
		else if (IsClient(acptr))
			j++;
	if (!j)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_WARNING,"ircd exiting: autodie");
#endif
		exit(0);
	    }
	return;
}

void	initstats(void)
{
	bzero((char *)&istat, sizeof(istat));
	istat.is_serv = 1;
	istat.is_localc = 1;	/* me */
	istat.is_m_users_t = timeofday;
	istat.is_m_myclnt_t = timeofday;
	istat.is_l_myclnt_t = timeofday;
	bzero((char *)&ircst, sizeof(ircst));
}

void	initruntimeconf(void)
{
	memset((char *)&iconf, 0, sizeof(iconf));
	iconf.aconnect = 1; /* default to ON */
	iconf.split = 1; /* ircd starts in split-mode */
	iconf.caccept = 2; /* accept clients when no split */

	/* Defaults set in config.h */
	iconf.split_minservers = DEFAULT_SPLIT_SERVERS;
	iconf.split_minusers = DEFAULT_SPLIT_USERS;

	if ((bootopt & BOOT_STANDALONE))
	{
		/* standalone mode */
		iconf.split = 3;
	}
}

void	tstats(aClient *cptr, char *name)
{
	Reg	aClient	*acptr;
	Reg	int	i;
	Reg	struct stats	*sp;
	struct	stats	tmp;

	sp = &tmp;
	bcopy((char *)ircstp, (char *)sp, sizeof(*sp));
	for (i = 0; i < MAXCONNECTIONS; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsServer(acptr))
		    {
			sp->is_sbs += acptr->sendB;
			sp->is_sbr += acptr->receiveB;
			sp->is_sti += timeofday - acptr->firsttime;
			sp->is_sv++;
		    }
		else if (IsClient(acptr))
		    {
			sp->is_cbs += acptr->sendB;
			sp->is_cbr += acptr->receiveB;
			sp->is_cti += timeofday - acptr->firsttime;
			sp->is_cl++;
		    }
		else if (IsUnknown(acptr))
			sp->is_ni++;
	    }

	sendto_one(cptr, ":%s %d %s :accepts %lu refused %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_ac, sp->is_ref);
	sendto_one(cptr, ":%s %d %s :unknown: commands %lu prefixes %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_unco, sp->is_unpf);
	sendto_one(cptr, ":%s %d %s :nick collisions %lu saves %lu, unknown closes %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_kill, sp->is_save, sp->is_ni);
	sendto_one(cptr, ":%s %d %s :wrong direction %lu empty %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_wrdi, sp->is_empt);
	sendto_one(cptr, ":%s %d %s :users without servers %lu ghosts N/A",
		   ME, RPL_STATSDEBUG, name, sp->is_nosrv);
	sendto_one(cptr, ":%s %d %s :numerics seen %lu mode fakes %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_num, sp->is_fake);
	sendto_one(cptr, ":%s %d %s :auth: successes %lu fails %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_asuc, sp->is_abad);
	sendto_one(cptr,":%s %d %s :local connections %lu udp packets %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_loc, sp->is_udpok);
	sendto_one(cptr,":%s %d %s :udp errors %lu udp dropped %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_udperr, sp->is_udpdrop);
	sendto_one(cptr,
   ":%s %d %s :link checks %lu passed %lu 15s/%lu 30s dropped %luSq/%luYg/%luFl",
		   ME, RPL_STATSDEBUG, name, sp->is_ckl, sp->is_cklq,
		   sp->is_cklok, sp->is_cklQ, sp->is_ckly, sp->is_cklno);
	if (sp->is_wwcnt)
		sendto_one(cptr, ":%s %d %s :whowas turnover %lu/%lu/%lu [%lu]",
			   ME, RPL_STATSDEBUG, name, sp->is_wwmt,
			   (u_int) (sp->is_wwt / sp->is_wwcnt), sp->is_wwMt,
			   KILLCHASETIMELIMIT);
	if (sp->is_lkcnt)
		sendto_one(cptr, ":%s %d %s :ndelay turnover %lu/%lu/%lu [%lu]",
			   ME, RPL_STATSDEBUG, name, sp->is_lkmt,
			   (u_int) (sp->is_lkt / sp->is_lkcnt), sp->is_lkMt,
			   DELAYCHASETIMELIMIT);
	sendto_one(cptr, ":%s %d %s :abuse protections %u strict %u", ME,
		   RPL_STATSDEBUG, name, (bootopt & BOOT_PROT) ? 1 : 0,
		   (bootopt & BOOT_STRICTPROT) ? 1 : 0);
#ifdef DELAY_CLOSE
	sendto_one(cptr, ":%s %d %s :delay close %lu total %lu",
		ME, RPL_STATSDEBUG, name, istat.is_delayclosewait,
		istat.is_delayclose);
#endif
	sendto_one(cptr, ":%s %d %s :local channels reops %d remote %d",
		ME, RPL_STATSDEBUG, name, sp->is_reop, sp->is_rreop);
	sendto_one(cptr, ":%s %d %s :Client - Server",
		   ME, RPL_STATSDEBUG, name);
	sendto_one(cptr, ":%s %d %s :connected %lu %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_cl, sp->is_sv);
	sendto_one(cptr, ":%s %d %s :bytes sent %llu %llu",
		   ME, RPL_STATSDEBUG, name,
		   sp->is_cbs, sp->is_sbs);
	sendto_one(cptr, ":%s %d %s :bytes recv %llu %llu",
		   ME, RPL_STATSDEBUG, name,
		   sp->is_cbr, sp->is_sbr);
	sendto_one(cptr, ":%s %d %s :time connected %lu %lu",
		   ME, RPL_STATSDEBUG, name, sp->is_cti, sp->is_sti);
#if defined(USE_IAUTH)
	report_iauth_stats(cptr, name);
#endif
}

aMotd		*motd = NULL;
time_t		motd_mtime;

void	read_motd(char *filename)
{
	int fd, len;
	register aMotd *temp, *last;
	struct stat Sb;
	char line[80];
	register char *tmp;
	
	if ((fd = open(filename, O_RDONLY)) == -1)
		return;
	if (fstat(fd, &Sb) == -1)
	    {
		close(fd);
		return;
	    }
	if (Sb.st_mtime <= motd_mtime)
	{
		close(fd);
		return;
	}
	motd_mtime = Sb.st_mtime;
	for(;motd != NULL;motd=last)
	    {
		last = motd->next;
		MyFree(motd->line);
		MyFree(motd);
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	last = NULL;
	while ((len=dgets(fd, line, sizeof(line)-1)) > 0)
	    {
		if ((tmp = strchr(line, '\n')) != NULL)
			*tmp = (char) 0;
		else if ((tmp = strchr(line, '\r')) != NULL)
			*tmp = (char) 0;
		else
			line[len] = '\0';
		temp = (aMotd *)MyMalloc(sizeof(aMotd));
		temp->line = mystrdup(line);
		temp->next = NULL;
		       if (!motd)
			motd = temp;
		else
			last->next = temp;
		last = temp;
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	close(fd);
}

void	check_split(void)
{
	if ((bootopt & BOOT_STANDALONE))
		return;

	if (istat.is_eobservers < iconf.split_minservers ||
	    istat.is_user[0] + istat.is_user[1] < iconf.split_minusers)
	{
		/* Split detected */
		if (!IsSplit())
		{
			sendto_flag(SCH_NOTICE,
				"Network split detected, split mode activated");
			iconf.split = timeofday;
		}
	}
	else
	{
		/* End of split */
		if (IsSplit())
		{
			sendto_flag(SCH_NOTICE,
				"Network rejoined, split mode deactivated");
			iconf.split = 0;
			if (!firstrejoindone)
			{
				firstrejoindone = 1;
				activate_delayed_listeners();
#ifdef CACCEPT_DEFAULT
				iconf.caccept = CACCEPT_DEFAULT;
#endif
			}
		}
	}
}

/* Some day play with better random functions (configure) etc. --B. */
int	myrand(void)
{
	return rand();
}

void	mysrand(unsigned int seed)
{
	srand(seed);
}
