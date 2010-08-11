/************************************************************************
 *   IRC - Internet Relay Chat, ircd/ircd.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static const volatile char rcsid[] = "@(#)$Id: ircd.c,v 1.165 2010/08/11 17:39:00 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define IRCD_C
#include "s_externs.h"
#undef IRCD_C

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */

static	void	open_debugfile(void), setup_signals(void), io_loop(void);

#if defined(USE_IAUTH)
static	RETSIGTYPE	s_slave(int s);
#endif

istat_t	istat;
iconf_t iconf;
char	**myargv;
int	rehashed = 0;
int	portnum = -1;		    /* Server port number, listening this */
char	*configfile = IRCDCONF_PATH;	/* Server configuration file */
int	debuglevel = -1;		/* Server debug level */
int	bootopt = BOOT_PROT|BOOT_STRICTPROT;	/* Server boot option flags */
int	serverbooting = 1;
int	firstrejoindone = 0;		/* Server rejoined the network after
					   start */
char	*debugmode = "";		/*  -"-    -"-   -"-   -"- */
char	*sbrk0;				/* initial sbrk(0) */
char	*tunefile = IRCDTUNE_PATH;
volatile static	int	dorehash = 0,
			dorestart = 0,
			restart_iauth = 0;

#ifdef DELAY_CLOSE
time_t	nextdelayclose = 0;	/* time for next delayed close */
#endif
time_t	nextconnect = 1;	/* time for next try_connections call */
time_t	nextgarbage = 1;        /* time for next collect_channel_garbage call*/
time_t	nextping = 1;		/* same as above for check_pings() */
time_t	nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t	nextexpire = 1;		/* next expire run on the dns cache */
time_t	nextiarestart = 1;	/* next time to check if iauth is alive */
time_t	nextpreference = 1;	/* time for next calculate_preference call */
#ifdef TKLINE
time_t	nexttkexpire = 0;	/* time for next tkline_expire call */
#endif

aClient *ListenerLL = NULL;	/* Listeners linked list */

RETSIGTYPE s_die(int s)
{
	sendto_serv_v(NULL, SV_UID, ":%s SDIE", me.serv->sid);
#ifdef	USE_SYSLOG
	(void)syslog(LOG_CRIT, "Server Killed By SIGTERM");
	(void)closelog();
#endif
	logfiles_close();
	ircd_writetune(tunefile);
	flush_connections(me.fd);
#ifdef  UNIXPORT
	{
		aClient *acptr;
		char unixpath[256];
		for (acptr = ListenerLL; acptr; acptr = acptr->next)
		{
			if (IsUnixSocket(acptr))
			{
				sprintf(unixpath, "%s/%d",
				    acptr->confs->value.aconf->host,
				    acptr->confs->value.aconf->port);
				(void)unlink(unixpath);
			}
		}
	}
#endif
	exit(-1);
}

#if defined(USE_IAUTH)
static	RETSIGTYPE	s_slave(int s)
{
# if POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = s_slave;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
# else
	(void)signal(SIGUSR1, s_slave);
# endif
	restart_iauth = 1;
}
#endif

static RETSIGTYPE s_rehash(int s)
{
#if POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
#else
	(void)signal(SIGHUP, s_rehash);	/* sysV -argv */
#endif
	if (dorehash >= 1)
		dorehash = 2;
	else
		dorehash = 1;
}

void	restart(char *mesg)
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Restarting Server because: %s (%u)", mesg,
		     (u_int)((char *)sbrk((size_t)0)-sbrk0));
#endif
	sendto_flag(SCH_NOTICE, "Restarting server because: %s (%u)", mesg,
		    (u_int)((char *)sbrk((size_t)0)-sbrk0));
	server_reboot();
}

RETSIGTYPE s_restart(int s)
{
#if POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = s_restart;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGINT);
	(void)sigaction(SIGINT, &act, NULL);
#else

	(void)signal(SIGHUP, SIG_DFL);	/* sysV -argv */
#endif
	if (bootopt & BOOT_TTY)
	{
		fprintf(stderr, "Caught SIGINT, terminating...\n");
		exit(-1);
	}

	dorestart = 1;
}

void	server_reboot(void)
{
	Reg	int	i;

	sendto_flag(SCH_NOTICE, "Aieeeee!!!  Restarting server... (%u)",
		    (u_int)((char *)sbrk((size_t)0)-sbrk0));

	Debug((DEBUG_NOTICE,"Restarting server..."));
	flush_connections(me.fd);
	/*
	** fd 0 must be 'preserved' if either the -d or -i options have
	** been passed to us before restarting.
	*/
#ifdef USE_SYSLOG
	(void)closelog();
#endif
	logfiles_close();
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);
	if (!(bootopt & (BOOT_TTY|BOOT_DEBUG)))
	{
		(void)close(2);
		(void)close(1);
	}
	if ((bootopt & BOOT_CONSOLE) || isatty(0))
		(void)close(0);
	ircd_writetune(tunefile);
	if (!(bootopt & BOOT_INETD))
	    {
		(void)execv(IRCD_PATH, myargv);
#ifdef USE_SYSLOG
		/* Have to reopen since it has been closed above */
		
		openlog(mybasename(myargv[0]), LOG_PID|LOG_NDELAY, LOG_FACILITY);
		syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", IRCD_PATH,
		       myargv[0]);
		closelog();
#endif
		Debug((DEBUG_FATAL,"Couldn't restart server: %s",
		       strerror(errno)));
	    }
	exit(-1);
}


/*
** try_connections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
static	time_t	try_connections(time_t currenttime)
{
	Reg	aConfItem *aconf;
	Reg	aClient *cptr;
	aConfItem **pconf;
	int	confrq;
	time_t	next = 0;
	aClass	*cltmp;
	aConfItem *con_conf = NULL;
	int	allheld = 1;
#ifdef DISABLE_DOUBLE_CONNECTS
	int	i;
#endif

	if ((bootopt & BOOT_STANDALONE))
		return 0;

	Debug((DEBUG_NOTICE,"Connection check at   : %s",
		myctime(currenttime)));
	for (aconf = conf; aconf; aconf = aconf->next )
	{
		/* not a C-line */
		if (!(aconf->status & (CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER)))
			continue;

		/* not a candidate for AC */
		if (aconf->port <= 0)
			continue;

		cltmp = Class(aconf);
		/* not a candidate for AC */
		if (MaxLinks(cltmp) == 0)
			continue;

		/* minimize next to lowest hold time of all AC-able C-lines */
		if (next > aconf->hold || next == 0)
			next = aconf->hold;

		/* skip conf if the use of it is on hold until future. */
		if (aconf->hold > currenttime)
			continue;

		/* at least one candidate not held for future, good */
		allheld = 0;

		/* see if another link in this conf is allowed */
		if (Links(cltmp) >= MaxLinks(cltmp))
			continue;
		
		/* next possible check after connfreq secs for this C-line */
		confrq = get_con_freq(cltmp);
		aconf->hold = currenttime + confrq;

		/* is this server already connected? */
		cptr = find_name(aconf->name, (aClient *)NULL);
		if (!cptr)
			cptr = find_mask(aconf->name, (aClient *)NULL);

		/* matching client already exists, no AC to it */
		if (cptr)
			continue;

		/* no such server, check D-lines */
		if (find_denied(aconf->name, Class(cltmp)))
			continue;

#ifdef DISABLE_DOUBLE_CONNECTS
		/* Much better would be traversing only unknown
		** connections, but this requires another global
		** variable, adding and removing from there in
		** proper places etc. Some day. --B. */
		for (i = highest_fd; i >= 0; i--)
		{
			if (!(cptr = local[i]) ||
				cptr->status > STAT_UNKNOWN)
			{
				continue;
			}
			/* an unknown traveller we have */
			if (
#ifndef INET6
				cptr->ip.s_addr == aconf->ipnum.s_addr
#else
				!memcmp(cptr->ip.s6_addr,
					aconf->ipnum.s6_addr, 16)
#endif
			)
			{
				/* IP the same. Coincidence? Maybe.
				** Do not cause havoc with double connect. */
				break;
			}
			cptr = NULL;
		}
		if (cptr)
		{
			sendto_flag(SCH_SERVER, "AC to %s postponed", aconf->name);
			continue;
		}
#endif
		/* we have a candidate! */

		/* choose the best. */
		if (!con_conf ||
		     (con_conf->pref > aconf->pref && aconf->pref >= 0) ||
		     (con_conf->pref == -1 &&
		      Class(cltmp) > ConfClass(con_conf)))
		{
			con_conf = aconf;
		}
		/* above is my doubt: if we always choose best connection
		** and it always fails connecting, we may never try another,
		** even "worse"; what shall we do? --Beeth */
	}
	if (con_conf)
	{
		if (con_conf->next)  /* are we already last? */
		{
			for (pconf = &conf; (aconf = *pconf);
			     pconf = &(aconf->next))
				/* put the current one at the end and
				 * make sure we try all connections
				 */
				if (aconf == con_conf)
					*pconf = aconf->next;
			(*pconf = con_conf)->next = 0;
		}

		/* "Penalty" for being the best, so in next call of
		 * try_connections() other servers have chance. --B. */
		con_conf->hold += get_con_freq(Class(con_conf));

		if (iconf.aconnect == 0 || iconf.aconnect == 2 && 
				timeofday - iconf.split > DELAYCHASETIMELIMIT)
		{
			sendto_flag(SCH_NOTICE,
				"Connection to %s deferred. Autoconnect "
				"administratively disabled", con_conf->name);
		}
		else if (connect_server(con_conf, (aClient *)NULL,
				   (struct hostent *)NULL) == 0)
		{
			sendto_flag(SCH_NOTICE,
				    "Connection to %s[%s] activated.",
				    con_conf->name, con_conf->host);
		}
	}
	else
	if (allheld == 0)	/* disable AC only when some C: got checked */
	{
		/* No suitable conf for AC was found, so why bother checking
		** again? If some server quits, it'd get reenabled --B. */
		next = 0;
	}
	Debug((DEBUG_NOTICE,"Next connection check : %s", myctime(next)));
	return (next);
}

/*
 * calculate preference value based on accumulated stats.
 */
time_t calculate_preference(time_t currenttime)
{
	aConfItem *aconf;
	aCPing	*cp;
	double	f, f2;

	for (aconf = conf; aconf; aconf = aconf->next)
	{
		/* not a C-line */
		if (!(aconf->status & (CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER)))
			continue;

		/* not a candidate for AC */
		if (aconf->port <= 0)
			continue;

		/* send (udp) pings for all AC-able C-lines, we'll use it to
		** calculate preferences */
		send_ping(aconf);

		if (!(cp = aconf->ping) || !cp->seq || !cp->recvd)
		{
			aconf->pref = -1;
		}
		else
		{
			f = (double)cp->recvd / (double)cp->seq;
			f2 = pow(f, (double)20.0);
			if (f2 < (double)0.001)
				f = (double)0.001;
			else
				f = f2;
			f2 = (double)cp->ping / (double)cp->recvd;
			f = f2 / f;
			if (f > 100000.0)
				f = 100000.0;
			aconf->pref = (u_int) (f * (double)100.0);
		}
	}
	return currenttime + 60;
}

/* Checks all clients against KILL lines. (And remove them, if found.)
** Only MAXDELAYEDKILLS at a time or all, if not defined.
** Returns 1, if still work to do, 0 if finished.
*/
static	int	delayed_kills(time_t currenttime)
{
	static	time_t	dk_rehashed = 0;	/* time of last rehash we're processing */
	static	int	dk_lastfd;		/* fd we last checked */
	static	int	dk_checked;		/* # clients we checked */
	static	int	dk_killed;		/* # clients we killed */
	Reg	aClient	*cptr;
	Reg	int	i, j;

	if (dk_rehashed == 0)
	{
		dk_rehashed = currenttime;
		dk_checked = 0;
		dk_killed = 0;
		dk_lastfd = highest_fd;
	}
#ifdef MAXDELAYEDKILLS
	/* checking only this many clients each time */
	j = dk_lastfd - MAXDELAYEDKILLS + 1;
	if (j < 0)
#endif
		j = 0;

	for (i = dk_lastfd; i >= j; i--)
	{
		int	kflag = 0;
		char	*reason = NULL;

		if (!(cptr = local[i]) || !IsPerson(cptr))
		{
			/* for K:lines we're interested only in local,
			** fully registered clients */
			if (j > 0)
				j--;
			continue;
		}

		dk_checked++;
		kflag = find_kill(cptr, 0, &reason);

		/* If the client is a user and a KILL line was found
		** to be active, close this connection. */
		if (kflag == -1)
		{
			char buf[100];

			dk_killed++;
			sendto_flag(SCH_NOTICE,
				"Kill line active for %s",
				get_client_name(cptr, FALSE));
				cptr->exitc = EXITC_KLINE;
			if (!BadPtr(reason))
				sprintf(buf, "Kill line active: %.80s",
						reason);
			(void)exit_client(cptr, cptr, &me, (reason) ?
					  buf : "Kill line active");
		}
	}
	dk_lastfd = i;	/* from which fd to start next time */
	Debug((DEBUG_DEBUG, "DelayedKills killed %d and counting...",
		dk_killed));

	if (dk_lastfd < 0)
	{
		sendto_flag(SCH_NOTICE, "DelayedKills checked %d killed %d "
			"in %d sec", dk_checked, dk_killed,
			currenttime - dk_rehashed);
		dk_rehashed = 0;
		if (rehashed == 2)
		{
			/* there was rehash queued, start again */
			return 1;
		}
		return 0;
	}
	return rehashed;
}

static	time_t	check_pings(time_t currenttime)
{
#ifdef TIMEDKLINES
	static	time_t	lkill = 0;
#endif
	Reg	aClient	*cptr;
	Reg	int	kflag = 0;
	aClient *bysptr = NULL;
	int	ping = 0, i;
	time_t	oldest = 0, timeout;
	char	*reason = NULL;

	for (i = highest_fd; i >= 0; i--)
	    {
		if (!(cptr = local[i]) || IsListener(cptr))
			continue;

#ifdef TIMEDKLINES
		kflag = 0;
		reason = NULL;
		/* 
		** Once per TIMEDKLINES seconds.
		** (1 minute is minimum resolution in K-line field)
		*/
		if ((currenttime - lkill > TIMEDKLINES)
			&& IsPerson(cptr) && !IsKlineExempt(cptr))
		{
			kflag = find_kill(cptr, 1, &reason);
		}
#endif
		ping = IsRegistered(cptr) ? cptr->ping : ACCEPTTIMEOUT;
		Debug((DEBUG_DEBUG, "c(%s) %d p %d k %d a %d",
			cptr->name, cptr->status, ping, kflag,
			currenttime - cptr->lasttime));
		/*
		 * Ok, so goto's are ugly and can be avoided here but this code
		 * is already indented enough so I think its justified. -avalon
		 */
		if (!kflag && IsRegistered(cptr) &&
		    (ping >= currenttime - cptr->lasttime))
			goto ping_timeout;
		/*
		 * If the server hasnt talked to us in 2*ping seconds
		 * and it has a ping time, then close its connection.
		 * If the client is a user and a KILL line was found
		 * to be active, close this connection too.
		 */
		if (kflag ||
		    ((currenttime - cptr->lasttime) >= (2 * ping) &&
		     (cptr->flags & FLAGS_PINGSENT)) ||
		    (!IsRegistered(cptr) &&
		     (currenttime - cptr->firsttime) >= ping))
		    {
			if (!IsRegistered(cptr) && 
			    (DoingDNS(cptr) || DoingAuth(cptr) ||
			     DoingXAuth(cptr)))
			    {
				if (cptr->authfd >= 0)
				    {
					(void)close(cptr->authfd);
					cptr->authfd = -1;
					cptr->count = 0;
					*cptr->buffer = '\0';
				    }
				Debug((DEBUG_NOTICE, "%s/%c%s timeout %s",
				       (DoingDNS(cptr)) ? "DNS" : "dns",
				       (DoingXAuth(cptr)) ? "X" : "x",
				       (DoingAuth(cptr)) ? "AUTH" : "auth",
				       get_client_name(cptr,TRUE)));
				del_queries((char *)cptr);
				ClearAuth(cptr);
#if defined(USE_IAUTH)
				if (DoingDNS(cptr) || DoingXAuth(cptr))
				    {
					if (DoingDNS(cptr) &&
					    (iauth_options & XOPT_EXTWAIT))
					    {
						/* iauth wants more time */
						sendto_iauth("%d d", cptr->fd);
						ClearDNS(cptr);
						cptr->lasttime = currenttime;
						continue;
					    }
					if (DoingXAuth(cptr) &&
					    (iauth_options & XOPT_NOTIMEOUT))
					    {
						cptr->exitc = EXITC_AUTHTOUT;
						sendto_iauth("%d T", cptr->fd);
						exit_client(cptr, cptr, &me,
						     "Authentication Timeout");
						continue;
					    }
					sendto_iauth("%d T", cptr->fd);
					SetDoneXAuth(cptr);
				    }
#endif
				ClearDNS(cptr);
				ClearXAuth(cptr);
				ClearWXAuth(cptr);
				cptr->firsttime = currenttime;
				cptr->lasttime = currenttime;
				continue;
			    }
			if (IsServer(cptr) || IsConnecting(cptr) ||
			    IsHandshake(cptr))
			{
				if (cptr->serv && cptr->serv->byuid[0])
				{
					bysptr = find_uid(cptr->serv->byuid,
							NULL);
				}
				/* we are interested only in *remote* opers */
				if (bysptr && !MyConnect(bysptr))
				{
					sendto_one(bysptr, ":%s NOTICE %s :"
						"No response from %s, closing"
						" link", ME, bysptr->name,
						get_client_name(cptr, FALSE));
				}
				sendto_flag(SCH_NOTICE,
					    "No response from %s closing link",
					    get_client_name(cptr, FALSE));
			}
			/*
			 * this is used for KILL lines with time restrictions
			 * on them - send a message to the user being killed
			 * first.
			 */
			if (kflag && IsPerson(cptr))
			    {
				char buf[100];

				sendto_flag(SCH_NOTICE,
					    "Kill line active for %s",
					    get_client_name(cptr, FALSE));
				cptr->exitc = EXITC_KLINE;
				if (!BadPtr(reason))
					sprintf(buf, "Kill line active: %.80s",
						reason);
				(void)exit_client(cptr, cptr, &me, (reason) ?
						  buf : "Kill line active");
			    }
			else
			    {
				cptr->exitc = EXITC_PING;
				(void)exit_client(cptr, cptr, &me,
						  "Ping timeout");
			    }
			continue;
		    }
		else if (IsRegistered(cptr) &&
			 (cptr->flags & FLAGS_PINGSENT) == 0)
		    {
			/*
			 * if we havent PINGed the connection and we havent
			 * heard from it in a while, PING it to make sure
			 * it is still alive.
			 */
			cptr->flags |= FLAGS_PINGSENT;
			/* not nice but does the job */
			cptr->lasttime = currenttime - ping;
			sendto_one(cptr, "PING :%s", me.name);
		    }
ping_timeout:
		timeout = cptr->lasttime + ping;
		while (timeout <= currenttime)
			timeout += ping;
		if (timeout < oldest || !oldest)
			oldest = timeout;
	    }
#ifdef TIMEDKLINES
	if (currenttime - lkill > 60)
		lkill = currenttime;
#endif
	if (!oldest || oldest < currenttime)
		oldest = currenttime + PINGFREQUENCY;
	if (oldest < currenttime + 30)
		oldest += 30;
	Debug((DEBUG_NOTICE,"Next check_ping() call at: %s, %d %d %d",
		myctime(oldest), ping, oldest, currenttime));
	return (oldest);
}


static	void	setup_me(aClient *mp)
{
	struct	passwd	*p;

	p = getpwuid(getuid());
	strncpyzt(mp->username, (p) ? p->pw_name : "unknown",
		  sizeof(mp->username));
	(void)get_my_name(mp, mp->sockhost, sizeof(mp->sockhost)-1);
	/* I think we need no hostp, especially fake one --B.  */
	mp->hostp = NULL;
	if (mp->serv->namebuf[0] == '\0')
		strncpyzt(mp->serv->namebuf, mp->sockhost, sizeof(mp->serv->namebuf));
	if (me.info == DefInfo)
		me.info = mystrdup("IRCers United");
	mp->lasttime = mp->since = mp->firsttime = time(NULL);
	mp->hopcount = 0;
	mp->authfd = -1;
	mp->auth = mp->username;
	mp->confs = NULL;
	mp->flags = 0;
	mp->acpt = mp->from = mp;
	mp->next = NULL;
	mp->user = NULL;
	mp->fd = -1;
	SetMe(mp);
	mp->serv->snum = find_server_num (ME);
	/* we don't fill our own IP -> 0 as ip lenght */
	(void) make_user(mp,0);
	istat.is_users++;	/* here, cptr->next is NULL, see make_user() */
	mp->user->flags |= FLAGS_OPER;
	mp->serv->up = mp;
	mp->serv->maskedby = mp;
	mp->serv->version |= SV_UID;
	mp->user->server = find_server_string(mp->serv->snum);
	strncpyzt(mp->user->username, (p) ? p->pw_name : "unknown",
		  sizeof(mp->user->username));
	(void) strcpy(mp->user->host, mp->name);
	SetEOB(mp);
	istat.is_eobservers = 1;

	(void)add_to_client_hash_table(mp->name, mp);
	(void)add_to_sid_hash_table(mp->serv->sid, mp);
	strncpyzt(mp->serv->verstr, PATCHLEVEL, sizeof(mp->serv->verstr));
	setup_server_channels(mp);
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static	void	bad_command(void)
{
  (void)printf(
	 "Usage: ircd [-a] [-b] [-c]%s [-h servername] [-q] [-i]"
	 "[-T [tunefile]] [-p (strict|on|off)] [-s] [-v] [-t] %s\n",
#ifdef CMDLINE_CONFIG
	 " [-f config]",
#else
	 "",
#endif
#ifdef DEBUGMODE
	 " [-x loglevel]"
#else
	 ""
#endif
	 );
  (void)printf("Server not started\n\n");
  exit(-1);
}

int	main(int argc, char *argv[])
{
	uid_t	uid, euid;

	sbrk0 = (char *)sbrk((size_t)0);
	uid = getuid();
	euid = geteuid();

#ifdef	CHROOTDIR
	ircd_res_init();
	if (chdir(ROOT_PATH)!=0)
	{
		perror("chdir");
		(void)fprintf(stderr,"%s: Cannot chdir: %s.\n", IRCD_PATH,
			ROOT_PATH);
		exit(5);
	}
	if (chroot(ROOT_PATH)!=0)
	    {
		perror("chroot");
		(void)fprintf(stderr,"%s: Cannot chroot: %s.\n", IRCD_PATH,
			      ROOT_PATH);
		exit(5);
	    }
#endif /*CHROOTDIR*/

#ifdef	ZIP_LINKS
	if (zlib_version[0] == '0')
	    {
		fprintf(stderr, "zlib version 1.0 or higher required\n");
		exit(1);
	    }
	if (zlib_version[0] != ZLIB_VERSION[0])
	    {
        	fprintf(stderr, "incompatible zlib version\n");
		exit(1);
	    }
	if (strcmp(zlib_version, ZLIB_VERSION) != 0)
	    {
		fprintf(stderr, "warning: different zlib version\n");
	    }
#endif

	myargv = argv;
	(void)umask(077);                /* better safe than sorry --SRB */
	bzero((char *)&me, sizeof(me));

	make_server(&me);
	register_server(&me);

	version = make_version();	/* Generate readable version string */

	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	while (--argc > 0 && (*++argv)[0] == '-')
	    {
		char	*p = argv[0]+1;
		int	flag = *p++;

		if (flag == '\0' || *p == '\0')
		{
			if (argc > 1 && argv[1][0] != '-')
			{
				p = *++argv;
				argc -= 1;
			}
			else
			{
				p = "";
			}
		}

		switch (flag)
		    {
                    case 'a':
			bootopt |= BOOT_AUTODIE;
			break;
		    case 'b':
			bootopt |= BOOT_BADTUNE;
			break;
		    case 'c':
			bootopt |= BOOT_CONSOLE;
			break;
		    case 'q':
			bootopt |= BOOT_QUICK;
			break;
#ifdef CMDLINE_CONFIG
		    case 'f':
                        (void)setuid((uid_t)uid);
			configfile = p;
			break;
#endif
		    case 'h':
			if (*p == '\0')
				bad_command();
			strncpyzt(me.serv->namebuf, p, sizeof(me.serv->namebuf));
			break;
		    case 'i':
			bootopt |= BOOT_INETD|BOOT_AUTODIE;
		        break;
		    case 'p':
			if (!strcmp(p, "strict"))
				bootopt |= BOOT_PROT|BOOT_STRICTPROT;
			else if (!strcmp(p, "on"))
				bootopt |= BOOT_PROT;
			else if (!strcmp(p, "off"))
				bootopt &= ~(BOOT_PROT|BOOT_STRICTPROT);
			else if (!strcmp(p, "standalone"))
				bootopt |= BOOT_STANDALONE;
			else
				bad_command();
			break;
		    case 's':
			bootopt |= BOOT_NOIAUTH;
			break;
		    case 't':
#ifdef DEBUGMODE
                        (void)setuid((uid_t)uid);
#endif
			bootopt |= BOOT_TTY;
			break;
		    case 'T':
			tunefile = p;
			break;
		    case 'v':
			(void)printf("ircd %s %s\n\tzlib %s\n\tircd.conf delimiter %c\n\t%s #%s\n",
				     version, serveropts,
#ifndef	ZIP_LINKS
				     "not used",
#else
				     zlib_version,
#endif
					IRCDCONF_DELIMITER,
				     creation, generation);
			  exit(0);
		    case 'x':
#ifdef	DEBUGMODE
                        (void)setuid((uid_t)uid);
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			bootopt |= BOOT_DEBUG;
			break;
#else
			(void)fprintf(stderr,
				"%s: DEBUGMODE must be defined for -x y\n",
				myargv[0]);
			exit(0);
#endif
		    default:
			bad_command();
		    }
	    }

	if (strlen(tunefile) > 1023 || strlen(mybasename(tunefile)) > 42)
	{
		fprintf(stderr, "Too long tune filename\n");
		exit(-1);
	}
	if (argc > 0)
		bad_command(); /* This exits out */

#ifndef IRC_UID
	if ((uid != euid) && !euid)
	    {
		(void)fprintf(stderr,
			"ERROR: do not run ircd setuid root. Make it setuid a\
 normal user.\n");
		exit(-1);
	    }
#endif

#if !defined(CHROOTDIR)
	(void)setuid((uid_t)euid);
# if defined(IRC_UID) && defined(IRC_GID)
	if ((int)getuid() == 0)
	    {
		/* run as a specified user */
		(void)fprintf(stderr,"WARNING: running ircd with uid = %d\n",
			IRC_UID);
		(void)fprintf(stderr,"         changing to gid %d.\n",IRC_GID);
		(void)setgid(IRC_GID);
		(void)setuid(IRC_UID);
	    } 
# endif
#endif /*CHROOTDIR/UID/GID*/

#if defined(USE_IAUTH)
	/* At this point, we just check whether iauth is there. Real start
	 * is done in init_sys(). */
	if ((bootopt & BOOT_NOIAUTH) == 0)
		switch (vfork())
		    {
		case -1:
			fprintf(stderr, "%s: Unable to fork!", myargv[0]);
			exit(-1);
		case 0:
			close(0); close(1); close(3);
			if (execl(IAUTH_PATH, IAUTH, "-X", NULL) < 0)
				_exit(-1);
		default:
		    {
			int rc;
			
			(void)wait(&rc);
			if (rc != 0)
			    {
				fprintf(stderr,
					"%s: error: unable to find \"%s\".\n",
					myargv[0], IAUTH_PATH);
				exit(-1);
			    }
		    }
		    }
#endif

	setup_signals();

	/* didn't set debuglevel */
	/* but asked for debugging output to tty */
#ifdef DEBUGMODE

	if ((debuglevel < 0) &&  (bootopt & BOOT_TTY))
	    {
		(void)fprintf(stderr,
			"you specified -t without -x. use -x <n>\n");
		exit(-1);
	    }

#endif
	timeofday = time(NULL);
	initanonymous();
	initstats();
	initruntimeconf();
	ircd_readtune(tunefile);
	motd = NULL;
	read_motd(IRCDMOTD_PATH);
	inithashtables();
	initlists();
	initclass();
	initwhowas();
	timeofday = time(NULL);
	open_debugfile();
	timeofday = time(NULL);
	(void)init_sys();

#ifdef USE_SYSLOG
	openlog(mybasename(myargv[0]), LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	timeofday = time(NULL);
	if (initconf(bootopt) == -1)
	    {
		Debug((DEBUG_FATAL, "Couldn't open configuration file %s",
			configfile));
		(void)fprintf(stderr,
			"Couldn't open configuration file %s (%s)\n",
			 configfile,strerror(errno));
		
		exit(-1);
	    }
	else
	    {
		aClient *acptr = NULL;
		int i;

                for (i = 0; i <= highest_fd; i++)
                    {   
			if (!(acptr = local[i]))
				continue;
			if (IsListener(acptr))
				break;
			acptr = NULL;
		    }
		/* exit if there is nothing to listen to */
		if (acptr == NULL && !(bootopt & BOOT_INETD))
		{
			fprintf(stderr,
			"Fatal Error: No working P-line in ircd.conf\n");
			exit(-1);
		}
		/* Is there an M-line? */
		if (!find_me())
		{
			fprintf(stderr,
			"Fatal Error: No M-line in ircd.conf.\n");
			exit(-1);
		}
		if ((i=check_servername(ME)))
		{
			fprintf(stderr,
			"Fatal Error: %s.\n", check_servername_errors[i-1][1]);
			exit(-1);
		}
		if (!me.serv->sid)
		{
			fprintf(stderr,
			"Fatal Error: No SID specified in ircd.conf\n");
			exit(-1);
		}
		if (!sid_valid(me.serv->sid))
		{
			fprintf(stderr,
			"Fatal Error: Invalid sid %s specified in ircd.conf\n",
				me.serv->sid);
			exit(-1);
		}
		if (!networkname)
		{
			fprintf(stderr,
			"Warning: Network name is not set in ircd.conf\n");
		}
		isupport = make_isupport();	/* Generate RPL_ISUPPORT (005) numerics */
	    }

	setup_me(&me);
	check_class();
	ircd_writetune(tunefile);
	if (bootopt & BOOT_INETD)
	    {
		aClient	*tmp;
		aConfItem *aconf;

		tmp = make_client(NULL);
		make_server(tmp);
		register_server(tmp);

		tmp->fd = 0;
		tmp->flags = FLAGS_LISTEN;
		tmp->acpt = tmp;
		tmp->from = tmp;
	        tmp->firsttime = time(NULL);

                SetMe(tmp);

                (void)strcpy(tmp->serv->namebuf, "*");

                if (inetport(tmp, 0, "0.0.0.0", 0, 1))
                        tmp->fd = -1;
		if (tmp->fd == 0) 
		    {
			aconf = make_conf();
			aconf->status = CONF_LISTEN_PORT;
			aconf->clients++;
	                aconf->next = conf;
        	        conf = aconf;

                        tmp->confs = make_link();
        	        tmp->confs->next = NULL;
	               	tmp->confs->value.aconf = aconf;
	                add_fd(tmp->fd, &fdas);
	                add_fd(tmp->fd, &fdall);
	                set_non_blocking(tmp->fd, tmp);
		    }
		else
		    exit(5);
	    }
	
	Debug((DEBUG_NOTICE,"Server ready..."));
#ifdef USE_SYSLOG
	syslog(LOG_NOTICE, "Server Ready: v%s (%s #%s)", version, creation,
	       generation);
#endif
	printf("Server %s (%s) version %s starting%s%s", ME, me.serv->sid,
		version, (bootopt & BOOT_TTY) ? " in foreground mode." : ".",
#ifdef DEBUGMODE
		"(DEBUGMODE)\n"
#else
		"\n"
#endif
		);

	timeofday = time(NULL);
	mysrand(timeofday);
	
	/* daemonize() closes 0,1,2 -- make sure you don't have any fd open */
	daemonize();	
	logfiles_open();
	write_pidfile();
	dbuf_init();
	
	serverbooting = 0;
	
	while (1)
		io_loop();
}


static	void	io_loop(void)
{
	static	time_t	delay = 0;
	int maxs = 4;

	if (timeofday >= nextpreference)
		nextpreference = calculate_preference(timeofday);
	/*
	** We only want to connect if a connection is due,
	** not every time through.  Note, if there are no
	** active C lines, this call to Tryconnections is
	** made once only; it will return 0. - avalon
	*/
	if (nextconnect && timeofday >= nextconnect)
		nextconnect = try_connections(timeofday);
#ifdef DELAY_CLOSE
	/* close all overdue delayed fds */
	if (nextdelayclose && timeofday >= nextdelayclose)
		nextdelayclose = delay_close(-1);
#endif
#ifdef TKLINE
	/* expire tklines */
	if (nexttkexpire && timeofday >= nexttkexpire)
		nexttkexpire = tkline_expire(0);
#endif
	/*
	** Every once in a while, hunt channel structures that
	** can be freed. Reop channels while at it, too.
	*/
	if (timeofday >= nextgarbage)
		nextgarbage = collect_channel_garbage(timeofday);
	/*
	** DNS checks. One to timeout queries, one for cache expiries.
	*/
	if (timeofday >= nextdnscheck)
		nextdnscheck = timeout_query_list(timeofday);
	if (timeofday >= nextexpire)
		nextexpire = expire_cache(timeofday);
	/*
	** take the smaller of the two 'timed' event times as
	** the time of next event (stops us being late :) - avalon
	** WARNING - nextconnect can return 0!
	*/
	if (nextconnect)
		delay = MIN(nextping, nextconnect);
	else
		delay = nextping;
#ifdef DELAY_CLOSE
	if (nextdelayclose)
		delay = MIN(nextdelayclose, delay);
#endif
	delay = MIN(nextdnscheck, delay);
	delay = MIN(nextexpire, delay);
	delay = MIN(nextpreference, delay);
	delay -= timeofday;
	/*
	** Adjust delay to something reasonable [ad hoc values]
	** (one might think something more clever here... --msa)
	** We don't really need to check that often and as long
	** as we don't delay too long, everything should be ok.
	** waiting too long can cause things to timeout...
	** i.e. PINGS -> a disconnection :(
	** - avalon
	*/
	if (delay < 1)
		delay = 1;
	else
		delay = MIN(delay, TIMESEC);

	/*
	** First, try to drain traffic from servers and listening sockets.
	** Give up either if there's no traffic or too many iterations.
	*/
	while (maxs--)
		if (read_message(0, &fdas, 0))
			flush_fdary(&fdas);
		else
			break;

	Debug((DEBUG_DEBUG, "delay for %d", delay));
	/*
	** Second, deal with _all_ clients but only try to empty sendQ's for
	** servers.  Other clients are dealt with below..
	*/
	if (read_message(1, &fdall, 1) == 0 && delay > 1)
	    {
		/*
		** Timed out (e.g. *NO* traffic at all).
		** Try again but also check to empty sendQ's for all clients.
		*/
		(void)read_message(delay - 1, &fdall, 0);
	    }
	timeofday = time(NULL);

	Debug((DEBUG_DEBUG ,"Got message(s)"));
	/*
	** ...perhaps should not do these loops every time,
	** but only if there is some chance of something
	** happening (but, note that conf->hold times may
	** be changed elsewhere--so precomputed next event
	** time might be too far away... (similarly with
	** ping times) --msa
	*/
	if (timeofday >= nextping)
	    {
		nextping = check_pings(timeofday);
		if (rehashed > 0)
		{
			rehashed = delayed_kills(timeofday);
		}
	    }

	if (dorestart)
		restart("Caught SIGINT");
	if (dorehash > 0)
	    {	/* Only on signal, not on oper /rehash */
		ircd_writetune(tunefile);
		(void)rehash(&me, &me, 1);
		dorehash--;
	    }
	if (restart_iauth || timeofday >= nextiarestart)
	    {
		start_iauth(restart_iauth);
		restart_iauth = 0;
		nextiarestart = timeofday + 15;
	    }
	/*
	** Flush output buffers on all connections now if they
	** have data in them (or at least try to flush)
	** -avalon
	*/
	flush_connections(me.fd);

#ifdef	DEBUGMODE
	checklists();
#endif

}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by IRCDDBG_PATH.
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else don't waste the fd.
 */
static	void	open_debugfile(void)
{
#ifdef	DEBUGMODE
	int	fd;

	if (debuglevel >= 0)
	    {
		(void)printf("isatty = %d ttyname = %#x\n",
			isatty(2), (u_int)ttyname(2));
		if (!(bootopt & BOOT_TTY)) /* leave debugging output on fd 2 */
		    {
			(void)truncate(IRCDDBG_PATH, 0);
			if ((fd = open(IRCDDBG_PATH,O_WRONLY|O_CREAT,0600))<0)
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);
			if (fd != 2)
			    {
				(void)dup2(fd, 2);
				(void)close(fd); 
			    }
		    }
		Debug((DEBUG_FATAL, "Debug: File <%s> Level: %d at %s",
			( (!(bootopt & BOOT_TTY)) ? IRCDDBG_PATH :
			(isatty(2) && ttyname(2)) ? ttyname(2) : "FD2-Pipe"),
			debuglevel, myctime(time(NULL))));
	    }
#endif
	return;
}

static	void	setup_signals(void)
{
#if POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaddset(&act.sa_mask, SIGALRM);
# ifdef	SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, NULL);
# endif
	(void)sigaction(SIGPIPE, &act, NULL);
	act.sa_handler = dummy;
	(void)sigaction(SIGALRM, &act, NULL);
	act.sa_handler = s_rehash;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = s_restart;
	(void)sigaddset(&act.sa_mask, SIGINT);
	(void)sigaction(SIGINT, &act, NULL);
	act.sa_handler = s_die;
	(void)sigaddset(&act.sa_mask, SIGTERM);
	(void)sigaction(SIGTERM, &act, NULL);
# if defined(USE_IAUTH)
	act.sa_handler = s_slave;
# else
	act.sa_handler = SIG_IGN;
# endif
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
# if defined(USE_IAUTH)
	act.sa_handler = SIG_IGN;
#  ifdef SA_NOCLDWAIT
        act.sa_flags = SA_NOCLDWAIT;
#  else
        act.sa_flags = 0;
#  endif
	(void)sigaddset(&act.sa_mask, SIGCHLD);
	(void)sigaction(SIGCHLD, &act, NULL);
# endif
	
# if defined(__FreeBSD__)	
	/* Don't core after detaching from gdb on fbsd */

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigaddset(&act.sa_mask, SIGTRAP);
	(void)sigaction(SIGTRAP,&act,NULL);
# endif /* __FreeBSD__ */

#else /* POSIX_SIGNALS */

# ifndef	HAVE_RELIABLE_SIGNALS
	(void)signal(SIGPIPE, dummy);
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# else /* HAVE_RELIABLE_SIGNALS */
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, SIG_IGN);
#  endif
	(void)signal(SIGPIPE, SIG_IGN);
	
# endif /* HAVE_RELIABLE_SIGNALS */
	(void)signal(SIGALRM, dummy);
	(void)signal(SIGHUP, s_rehash);
	(void)signal(SIGTERM, s_die);
	(void)signal(SIGINT, s_restart);
# if defined(USE_IAUTH)
	(void)signal(SIGUSR1, s_slave);
	(void)signal(SIGCHLD, SIG_IGN);
# else
	(void)signal(SIGUSR1, SIG_IGN);
# endif
	
# if defined(__FreeBSD__)
	/* don't core after detaching from gdb on fbsd */
	(void)signal(SIGTRAP, SIG_IGN);
# endif /* __FreeBSD__ */

#endif /* POSIX_SIGNAL */

#ifdef RESTARTING_SYSTEMCALLS
	/*
	** At least on Apollo sr10.1 it seems continuing system calls
	** after signal is the default. The following 'siginterrupt'
	** should change that default to interrupting calls.
	*/
	(void)siginterrupt(SIGALRM, 1);
#endif
}

/*
 * Called from bigger_hash_table(), s_die(), server_reboot(),
 * main(after initializations), grow_history(), rehash(io_loop) signal.
 */
void ircd_writetune(char *filename)
{
	int fd;
	char buf[100];

	if (!filename || !*filename)
		return;

	(void)truncate(filename, 0);
	if ((fd = open(filename, O_CREAT|O_WRONLY, 0600)) >= 0)
	    {
		(void)sprintf(buf, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n", ww_size,
			       lk_size, _HASHSIZE, _CHANNELHASHSIZE,
			       _SIDSIZE, poolsize, _UIDSIZE);
		if (write(fd, buf, strlen(buf)) == -1)
			sendto_flag(SCH_ERROR,
				    "Failed (%d) to write tune file: %s.",
				    errno, mybasename(filename));
		else
			sendto_flag(SCH_NOTICE, "Updated %s.",
				    mybasename(filename));
		close(fd);
	    }
	else
		sendto_flag(SCH_ERROR, "Failed (%d) to open tune file: %s.",
			    errno, mybasename(filename));
}

/*
 * Called only from main() at startup.
 */
void ircd_readtune(char *filename)
{
	int fd, t_data[7];
	char buf[100];

	memset(buf, 0, sizeof(buf));
	if ((fd = open(filename, O_RDONLY)) != -1)
	    {
		read(fd, buf, 100);	/* no panic if this fails.. */
		if (sscanf(buf, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n", &t_data[0],
                           &t_data[1], &t_data[2], &t_data[3],
                           &t_data[4], &t_data[5], &t_data[6]) != 7)
		    {
			close(fd);
			if (bootopt & BOOT_BADTUNE)
				return;
			else
			    {
				fprintf(stderr,
					"ircd tune file %s: bad format\n",
					filename);
				exit(1);
			    }
		    }

		/*
		** Initiate the tune-values after successfully
		** reading the tune-file.
		*/
		ww_size = t_data[0];
		lk_size = t_data[1];
		_HASHSIZE = t_data[2];
#ifdef USE_HOSTHASH
		_HOSTNAMEHASHSIZE = t_data[2]; /* hostname has always same size
						  as the client hash */
#endif
#ifdef USE_IPHASH
		_IPHASHSIZE = t_data[2];
#endif
		_CHANNELHASHSIZE = t_data[3];
		_SIDSIZE = t_data[4];
		poolsize = t_data[5];
		_UIDSIZE = t_data[6];

		/*
		** the lock array only grows if the whowas array grows,
		** I don't think it should be initialized with a lower
		** size since it will never adjust unless whowas array does.
		*/
		if (lk_size < ww_size)
			lk_size = ww_size;
		close(fd);
	    }
}

