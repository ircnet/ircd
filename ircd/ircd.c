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
static  char rcsid[] = "@(#)$Id: ircd.c,v 1.41 1999/01/23 23:01:23 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define IRCD_C
#include "s_externs.h"
#undef IRCD_C

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */

static	void	open_debugfile(), setup_signals();

istat_t	istat;
char	**myargv;
int	rehashed = 0;
int	portnum = -1;		    /* Server port number, listening this */
char	*configfile = CONFIGFILE;	/* Server configuration file */
int	debuglevel = -1;		/* Server debug level */
int	bootopt = BOOT_PROT|BOOT_STRICTPROT;	/* Server boot option flags */
char	*debugmode = "";		/*  -"-    -"-   -"-   -"- */
char	*sbrk0;				/* initial sbrk(0) */
char	*tunefile = TPATH;
static	int	dorehash = 0,
		dorestart = 0,
		restart_iauth = 0;

static	char	*dpath = DPATH;

time_t	nextconnect = 1;	/* time for next try_connections call */
time_t	nextgarbage = 1;        /* time for next collect_channel_garbage call*/
time_t	nextping = 1;		/* same as above for check_pings() */
time_t	nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t	nextexpire = 1;	/* next expire run on the dns cache */

#ifdef	PROFIL
extern	etext();

RETSIGTYPE	s_monitor(s)
int s;
{
	static	int	mon = 0;
#if POSIX_SIGNALS
	struct	sigaction act;
#endif

	(void)moncontrol(mon);
	mon = 1 - mon;
#if POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
	(void)signal(SIGUSR1, s_monitor);
#endif
}
#endif

RETSIGTYPE s_die(s)
int s;
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_CRIT, "Server Killed By SIGTERM");
#endif
	ircd_writetune(tunefile);
	flush_connections(me.fd);
	exit(-1);
}

#if defined(USE_IAUTH)
RETSIGTYPE s_slave(s)
int s;
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

static RETSIGTYPE s_rehash(s)
int s;
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
	dorehash = 1;
}

void	restart(mesg)
char	*mesg;
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Restarting Server because: %s (%u)", mesg,
		     (u_int)sbrk((size_t)0)-(u_int)sbrk0);
#endif
	sendto_flag(SCH_NOTICE, "Restarting server because: %s (%u)", mesg,
		    (u_int)sbrk((size_t)0)-(u_int)sbrk0);
	server_reboot();
}

RETSIGTYPE s_restart(s)
int s;
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
	dorestart = 1;
}

void	server_reboot()
{
	Reg	int	i;

	sendto_flag(SCH_NOTICE, "Aieeeee!!!  Restarting server... (%u)",
		    (u_int)sbrk((size_t)0)-(u_int)sbrk0);

	Debug((DEBUG_NOTICE,"Restarting server..."));
	flush_connections(me.fd);
	/*
	** fd 0 must be 'preserved' if either the -d or -i options have
	** been passed to us before restarting.
	*/
#ifdef USE_SYSLOG
	(void)closelog();
#endif
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);
	if (!(bootopt & (BOOT_TTY|BOOT_DEBUG)))
		(void)close(2);
	(void)close(1);
	if ((bootopt & BOOT_CONSOLE) || isatty(0))
		(void)close(0);
	ircd_writetune(tunefile);
	if (!(bootopt & (BOOT_INETD|BOOT_OPER)))
	    {
		(void)execv(SPATH, myargv);
#ifdef USE_SYSLOG
		/* Have to reopen since it has been closed above */
		
		openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
		syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", SPATH,
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
static	time_t	try_connections(currenttime)
time_t	currenttime;
{
	static	time_t	lastsort = 0;
	Reg	aConfItem *aconf;
	Reg	aClient *cptr;
	aConfItem **pconf;
	int	confrq;
	time_t	next = 0;
	aClass	*cltmp;
	aConfItem *con_conf = NULL;
	double	f, f2;
	aCPing	*cp;

	Debug((DEBUG_NOTICE,"Connection check at   : %s",
		myctime(currenttime)));
	for (aconf = conf; aconf; aconf = aconf->next )
	    {
		/* Also when already connecting! (update holdtimes) --SRB */
		if (!(aconf->status & (CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER)))
			continue;
		/*
		** Skip this entry if the use of it is still on hold until
		** future. Otherwise handle this entry (and set it on hold
		** until next time). Will reset only hold times, if already
		** made one successfull connection... [this algorithm is
		** a bit fuzzy... -- msa >;) ]
		*/
		if ((aconf->hold > currenttime))
		    {
			if ((next > aconf->hold) || (next == 0))
				next = aconf->hold;
			continue;
		    }
		send_ping(aconf);
		if (aconf->port <= 0)
			continue;

		cltmp = Class(aconf);
		confrq = get_con_freq(cltmp);
		aconf->hold = currenttime + confrq;
		/*
		** Found a CONNECT config with port specified, scan clients
		** and see if this server is already connected?
		*/
		cptr = find_name(aconf->name, (aClient *)NULL);
		if (!cptr)
			cptr = find_mask(aconf->name, (aClient *)NULL);
		/*
		** It is not connected, scan clients and see if any matches
		** a D(eny) line.
		*/
		if (find_denied(aconf->name, Class(cltmp)))
			continue;
		/* We have a candidate, let's see if it could be the best. */
		if (!cptr && (Links(cltmp) < MaxLinks(cltmp)) &&
		    (!con_conf ||
		     (con_conf->pref > aconf->pref && aconf->pref >= 0) ||
		     (con_conf->pref == -1 &&
		      Class(cltmp) > ConfClass(con_conf))))
			con_conf = aconf;
		if ((next > aconf->hold) || (next == 0))
			next = aconf->hold;
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
		if (connect_server(con_conf, (aClient *)NULL,
				   (struct hostent *)NULL) == 0)
			sendto_flag(SCH_NOTICE,
				    "Connection to %s[%s] activated.",
				    con_conf->name, con_conf->host);
	    }
	Debug((DEBUG_NOTICE,"Next connection check : %s", myctime(next)));
	/*
	 * calculate preference value based on accumulated stats.
	 */
	if (!lastsort || lastsort < currenttime)
	    {
		for (aconf = conf; aconf; aconf = aconf->next)
			if (!(cp = aconf->ping) || !cp->seq || !cp->recvd)
				aconf->pref = -1;
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
		lastsort = currenttime + 60;
	    }
	return (next);
}


static	void	close_held(cptr)
aClient	*cptr;
{
	Reg	aClient	*acptr;
	int	i;

	for (i = highest_fd; i >= 0; i--)
		if ((acptr = local[i]) && (cptr->port == acptr->port) &&
		    (acptr != cptr) && IsHeld(acptr) &&
		    !bcmp((char *)&cptr->ip, (char *)&acptr->ip,
			  sizeof(acptr->ip)))
		    {
			(void) exit_client(acptr, acptr, &me,
					   "Reconnect Timeout");
			return;
		    }
}


static	time_t	check_pings(currenttime)
time_t	currenttime;
{
	static	time_t	lkill = 0;
	Reg	aClient	*cptr;
	Reg	int	kflag = 0;
	int	ping = 0, i, rflag = 0;
	time_t	oldest = 0, timeout;
	char	*reason;

	for (i = highest_fd; i >= 0; i--)
	    {
		if (!(cptr = local[i]) || IsListening(cptr) || IsLog(cptr) ||
		    IsHeld(cptr))
			continue;

		/*
		 * K and R lines once per minute, max.  This is the max.
		 * granularity in K-lines anyway (with time field).
		 */
		if ((currenttime - lkill > 60) || rehashed)
		    {
			if (IsPerson(cptr))
			    {
				kflag = find_kill(cptr, rehashed, &reason);
#ifdef R_LINES_OFTEN
				rflag = find_restrict(cptr);
#endif
			    }
			else
			    {
				kflag = rflag = 0;
				reason = NULL;
			    }
		    }
		ping = IsRegistered(cptr) ? get_client_ping(cptr) :
					    CONNECTTIMEOUT;
		Debug((DEBUG_DEBUG, "c(%s) %d p %d k %d r %d a %d",
			cptr->name, cptr->status, ping, kflag, rflag,
			currenttime - cptr->lasttime));
		/*
		 * Ok, so goto's are ugly and can be avoided here but this code
		 * is already indented enough so I think its justified. -avalon
		 */
		if (!kflag && !rflag && IsRegistered(cptr) &&
		    (ping >= currenttime - cptr->lasttime))
			goto ping_timeout;
		/*
		 * If the server hasnt talked to us in 2*ping seconds
		 * and it has a ping time, then close its connection.
		 * If the client is a user and a KILL line was found
		 * to be active, close this connection too.
		 */
		if (kflag || rflag ||
		    ((currenttime - cptr->lasttime) >= (2 * ping) &&
		     (cptr->flags & FLAGS_PINGSENT)) ||
		    (!IsRegistered(cptr) &&
		     (currenttime - cptr->firsttime) >= ping))
		    {
			if (IsReconnect(cptr))
			    {
				sendto_flag(SCH_ERROR,
					    "Reconnect timeout to %s",
					    get_client_name(cptr, TRUE));
				close_held(cptr);
				(void)exit_client(cptr, cptr, &me,
						  "Ping timeout");
			    }
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
				Debug((DEBUG_NOTICE, "DNS/AUTH timeout %s",
					get_client_name(cptr,TRUE)));
				del_queries((char *)cptr);
				ClearAuth(cptr);
#if defined(USE_IAUTH)
				if (DoingDNS(cptr) || DoingXAuth(cptr))
					sendto_iauth("%d T", cptr->fd);
#endif
				ClearDNS(cptr);
				ClearXAuth(cptr);
				SetAccess(cptr);
				cptr->firsttime = currenttime;
				continue;
			    }
			if (IsServer(cptr) || IsConnecting(cptr) ||
			    IsHandshake(cptr))
				sendto_flag(SCH_NOTICE,
					    "No response from %s closing link",
					    get_client_name(cptr, FALSE));
			/*
			 * this is used for KILL lines with time restrictions
			 * on them - send a messgae to the user being killed
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

#if defined(R_LINES) && defined(R_LINES_OFTEN)
			else if (IsPerson(cptr) && rflag)
			    {
				sendto_flag(SCH_NOTICE,
					   "Restricting %s, closing link.",
					   get_client_name(cptr,FALSE));
				cptr->exitc = EXITC_RLINE;
				(void)exit_client(cptr, cptr, &me,
						  "Restricting");
			    }
#endif
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
	if (currenttime - lkill > 60)
		lkill = currenttime;
	if (!oldest || oldest < currenttime)
		oldest = currenttime + PINGFREQUENCY;
	if (oldest < currenttime + 2)
		oldest += 2;
	Debug((DEBUG_NOTICE,"Next check_ping() call at: %s, %d %d %d",
		myctime(oldest), ping, oldest, currenttime));
	return (oldest);
}


static	void	setup_me(mp)
aClient	*mp;
{
	struct	passwd	*p;

	p = getpwuid(getuid());
	strncpyzt(mp->username, (p) ? p->pw_name : "unknown",
		  sizeof(mp->username));
	(void)get_my_name(mp, mp->sockhost, sizeof(mp->sockhost)-1);
	if (mp->name[0] == '\0')
		strncpyzt(mp->name, mp->sockhost, sizeof(mp->name));
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
	(void) make_server(mp);
	mp->serv->snum = find_server_num (ME);
	(void) make_user(mp);
	istat.is_users++;	/* here, cptr->next is NULL, see make_user() */
	usrtop = mp->user;
	mp->user->flags |= FLAGS_OPER;
	mp->serv->up = mp->name;
	mp->user->server = find_server_string(mp->serv->snum);
	strncpyzt(mp->user->username, (p) ? p->pw_name : "unknown",
		  sizeof(mp->user->username));
	(void) strcpy(mp->user->host, mp->name);

	(void)add_to_client_hash_table(mp->name, mp);

	setup_server_channels(mp);
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static	int	bad_command()
{
  (void)printf(
	 "Usage: ircd [-a] [-b] [-c] [-d path]%s [-h servername] [-q] [-o] [-i] [-T tunefile] [-p (strict|on|off)] [-v] %s\n",
#ifdef CMDLINE_CONFIG
	 " [-f config]",
#else
	 "",
#endif
#ifdef DEBUGMODE
	 " [-x loglevel] [-t]"
#else
	 ""
#endif
	 );
  (void)printf("Server not started\n\n");
  exit(-1);
}

int	main(argc, argv)
int	argc;
char	*argv[];
{
	uid_t	uid, euid;
	time_t	delay = 0;

	(void) myctime(time(NULL));	/* Don't ask, just *don't* ask */
	sbrk0 = (char *)sbrk((size_t)0);
	uid = getuid();
	euid = geteuid();
#ifdef	PROFIL
	(void)monstartup(0, etext);
	(void)moncontrol(1);
	(void)signal(SIGUSR1, s_monitor);
#endif

#ifdef	CHROOTDIR
	if (chdir(dpath))
	    {
		perror("chdir");
		(void)fprintf(stderr, "%s: Error in daemon path: %s.\n",
			      SPATH, dpath);
		exit(-1);
	    }
	ircd_res_init();
	if (chroot(DPATH))
	    {
		perror("chroot");
		(void)fprintf(stderr,"%s: Cannot chroot: %s.\n", SPATH, DPATH);
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
			if (argc > 1 && argv[1][0] != '-')
			    {
				p = *++argv;
				argc -= 1;
			    }
			else
				p = "";

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
		    case 'd' :
                        (void)setuid((uid_t)uid);
			dpath = p;
			break;
		    case 'o': /* Per user local daemon... */
                        (void)setuid((uid_t)uid);
			bootopt |= BOOT_OPER;
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
			strncpyzt(me.name, p, sizeof(me.name));
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
			else
				bad_command();
			break;
		    case 's':
			bootopt |= BOOT_NOIAUTH;
			break;
		    case 't':
                        (void)setuid((uid_t)uid);
			bootopt |= BOOT_TTY;
			break;
		    case 'T':
			if (*p == '\0')
				bad_command();
			tunefile = p;
			break;
		    case 'v':
			(void)printf("ircd %s %s\n\tzlib %s\n\tircd_dir: %s \n\t%s #%s\n", version, serveropts,
#ifndef	ZIP_LINKS
				     "not used",
#else
				     zlib_version,
#endif
				     dpath, creation, generation);
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

#ifndef	CHROOTDIR
	if (chdir(dpath))
	    {
		perror("chdir");
		(void)fprintf(stderr, "%s: Error in daemon path: %s.\n",
                              SPATH, dpath);
		exit(-1);
	    }
#endif
#if defined(USE_IAUTH)
	if ((bootopt & BOOT_NOIAUTH) == 0)
		switch (vfork())
		    {
		case -1:
			fprintf(stderr, "%s: Unable to fork!", myargv[0]);
			exit(-1);
		case 0:
			close(0); close(1); close(3);
			if (execl(APATH, APATH, "-X", NULL) < 0)
				_exit(-1);
		default:
		    {
			int rc;
			
			(void)wait(&rc);
			if (rc != 0)
			    {
				fprintf(stderr,
					"%s: error: unable to find \"%s\".\n",
					myargv[0], APATH);
				exit(-1);
			    }
		    }
		    }
#endif

	setup_signals();

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

	/* didn't set debuglevel */
	/* but asked for debugging output to tty */
	if ((debuglevel < 0) &&  (bootopt & BOOT_TTY))
	    {
		(void)fprintf(stderr,
			"you specified -t without -x. use -x <n>\n");
		exit(-1);
	    }

	if (argc > 0)
		bad_command(); /* This exits out */

	initstats();
	ircd_readtune(tunefile);
	timeofday = time(NULL);
#ifdef	CACHED_MOTD
	motd = NULL;
	read_motd(MPATH);
#endif
	inithashtables();
	initlists();
	initclass();
	initwhowas();
	timeofday = time(NULL);
	open_debugfile();
	timeofday = time(NULL);
	(void)init_sys();

#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	timeofday = time(NULL);
	if (initconf(bootopt) == -1)
	    {
		Debug((DEBUG_FATAL, "Failed in reading configuration file %s",
			configfile));
		/* no can do.
		(void)printf("Couldn't open configuration file %s\n",
			configfile);
		*/
		exit(-1);
	    }
	else
	    {
		aClient *acptr;
		int i;

                for (i = 0; i <= highest_fd; i++)
                    {   
                        if (!(acptr = local[i]))
                                continue;
			if (IsListening(acptr))
				break;
			acptr = NULL;
		    }
		/* exit if there is nothing to listen to */
		if (acptr == NULL)
			exit(-1);
	    }

	dbuf_init();
	setup_me(&me);
	check_class();
	ircd_writetune(tunefile);
	if (bootopt & BOOT_INETD)
	    {
		aClient	*tmp;
		aConfItem *aconf;

		tmp = make_client(NULL);

		tmp->fd = 0;
		tmp->flags = FLAGS_LISTEN;
		tmp->acpt = tmp;
		tmp->from = tmp;
	        tmp->firsttime = time(NULL);

                SetMe(tmp);

                (void)strcpy(tmp->name, "*");

                if (inetport(tmp, 0, "0.0.0.0", 0))
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
	if (bootopt & BOOT_OPER)
	    {
		aClient *tmp = add_connection(&me, 0);

		if (!tmp)
			exit(1);
		SetMaster(tmp);
		local[0] = tmp;
	    }
	else
		write_pidfile();

	Debug((DEBUG_NOTICE,"Server ready..."));
#ifdef USE_SYSLOG
	syslog(LOG_NOTICE, "Server Ready: v%s (%s #%s)", version, creation,
	       generation);
#endif
	timeofday = time(NULL);
	while (1)
		delay = io_loop(delay);
}


time_t	io_loop(delay)
time_t	delay;
{
#ifdef PREFER_SERVER
	static	time_t	nextc = 0;
#endif
#ifdef HUB
	static	time_t	lastl = 0;
#endif

	/*
	** We only want to connect if a connection is due,
	** not every time through.  Note, if there are no
	** active C lines, this call to Tryconnections is
	** made once only; it will return 0. - avalon
	*/
	if (nextconnect && timeofday >= nextconnect)
		nextconnect = try_connections(timeofday);
	/*
	** Every once in a while, hunt channel structures that
	** can be freed.
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
	delay = MIN(nextdnscheck, delay);
	delay = MIN(nextexpire, delay);
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

#if defined(PREFER_SERVER)
	(void)read_message(1, &fdas);
	Debug((DEBUG_DEBUG, "delay for %d", delay));
	if (timeofday > nextc)
	    {
		(void)read_message(delay, &fdall);
		nextc = timeofday;
	    }
	timeofday = time(NULL);
#else
	(void)read_message(delay, &fdall);
	timeofday = time(NULL);
#endif

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
		rehashed = 0;
	    }

	if (dorestart)
		restart("Caught SIGINT");
	if (dorehash)
	    {	/* Only on signal, not on oper /rehash */
		ircd_writetune(tunefile);
		(void)rehash(&me, &me, 1);
		dorehash = 0;
	    }
	if (restart_iauth)
	    {
		start_iauth(1);
		restart_iauth = 0;
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

	return delay;
}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else don't waste the fd.
 */
static	void	open_debugfile()
{
#ifdef	DEBUGMODE
	int	fd;
	aClient	*cptr;

	if (debuglevel >= 0)
	    {
		cptr = make_client(NULL);
		cptr->fd = 2;
		SetLog(cptr);
		cptr->port = debuglevel;
		cptr->flags = 0;
		cptr->acpt = cptr;
		local[2] = cptr;
		(void)strcpy(cptr->sockhost, me.sockhost);

		(void)printf("isatty = %d ttyname = %#x\n",
			isatty(2), (u_int)ttyname(2));
		if (!(bootopt & BOOT_TTY)) /* leave debugging output on fd 2 */
		    {
			(void)truncate(LOGFILE, 0);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);
			if (fd != 2)
			    {
				(void)dup2(fd, 2);
				(void)close(fd); 
			    }
			strncpyzt(cptr->name, LOGFILE, sizeof(cptr->name));
		    }
		else if (isatty(2) && ttyname(2))
			strncpyzt(cptr->name, ttyname(2), sizeof(cptr->name));
		else
			(void)strcpy(cptr->name, "FD2-Pipe");
		Debug((DEBUG_FATAL, "Debug: File <%s> Level: %d at %s",
			cptr->name, cptr->port, myctime(time(NULL))));
	    }
	else
		local[2] = NULL;
#endif
	return;
}

static	void	setup_signals()
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
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
	act.sa_handler = SIG_IGN;
#  ifdef SA_NOCLDWAIT
        act.sa_flags = SA_NOCLDWAIT;
#  else
        act.sa_flags = 0;
#  endif
	(void)sigaddset(&act.sa_mask, SIGCHLD);
	(void)sigaction(SIGCHLD, &act, NULL);
# endif

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
# endif
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
void ircd_writetune(filename)
char *filename;
{
	int fd;
	char buf[100];

	(void)truncate(filename, 0);
	if ((fd = open(filename, O_CREAT|O_WRONLY, 0600)) >= 0)
	    {
		(void)sprintf(buf, "%d\n%d\n%d\n%d\n%d\n%d\n", ww_size,
			       lk_size, _HASHSIZE, _CHANNELHASHSIZE,
			       _SERVERSIZE, poolsize);
		if (write(fd, buf, strlen(buf)) == -1)
			sendto_flag(SCH_ERROR,
				    "Failed (%d) to write tune file: %s.",
				    errno, filename);
		else
			sendto_flag(SCH_NOTICE, "Updated %s.", filename);
		close(fd);
	    }
	else
		sendto_flag(SCH_ERROR, "Failed (%d) to open tune file: %s.",
			    errno, filename);
}

/*
 * Called only from main() at startup.
 */
void ircd_readtune(filename)
char *filename;
{
	int fd, t_data[6];
	char buf[100];

	buf[0] = '\0';
	if ((fd = open(filename, O_RDONLY)) != -1)
	    {
		read(fd, buf, 100);	/* no panic if this fails.. */
		if (sscanf(buf, "%d\n%d\n%d\n%d\n%d\n%d\n", &t_data[0],
                           &t_data[1], &t_data[2], &t_data[3],
                           &t_data[4], &t_data[5]) != 6)
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
		_CHANNELHASHSIZE = t_data[3];
		_SERVERSIZE = t_data[4];
		poolsize = t_data[5];

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
