/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_bsd.c
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

/* -- Jto -- 07 Jul 1990
 * Added jlp@hamblin.byu.edu's debugtty fix
 */

/* -- Armin -- Jun 18 1990
 * Added setdtablesize() for more socket connections
 * (sequent OS Dynix only) -- maybe select()-call must be changed ...
 */

/* -- Jto -- 13 May 1990
 * Added several fixes from msa:
 *   Better error messages
 *   Changes in check_access
 * Added SO_REUSEADDR fix from zessel@informatik.uni-kl.de
 */

#ifndef lint
static const volatile char rcsid[] = "@(#)$Id: s_bsd.c,v 1.188 2011/01/20 14:26:56 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_BSD_C
#include "s_externs.h"
#undef S_BSD_C

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET	0x7f
#endif

aClient	*local[MAXCONNECTIONS];
FdAry	fdas, fdall;
int	highest_fd = 0, readcalls = 0, udpfd = -1, resfd = -1, adfd = -1;
time_t	timeofday;
static	struct	SOCKADDR_IN	mysk;
static	void	polludp(void);

static	struct	SOCKADDR *connect_inet (aConfItem *, aClient *, int *);
static	int	completed_connection (aClient *);
static	int	check_init (aClient *, char *);
static	int	check_ping (char *, int);
static	void	do_dns_async (void);
static	int	set_sock_opts (int, aClient *);
#ifdef	UNIXPORT
static	struct	SOCKADDR *connect_unix (aConfItem *, aClient *, int *);
static	aClient	*add_unixconnection (aClient *, int);
static	char	unixpath[256];
#endif
static	char	readbuf[READBUF_SIZE];

#define	CFLAG	(CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER)
#define	NFLAG	CONF_NOCONNECT_SERVER

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
** Note: size is the max we can append to hname!
*/

void	add_local_domain(char *hname, size_t size)
{
#ifdef RES_INIT
	/* some return plain hostname with ending dot, whoops. */
	if (hname[strlen(hname)-1] == '.')
	{
		hname[strlen(hname)-1] = '\0';
		size++;
	}
	/* try to fix up unqualified names */
	if (!index(hname, '.'))
	{
		if (!(ircd_res.options & RES_INIT))
		{
			Debug((DEBUG_DNS,"ircd_res_init()"));
			ircd_res_init();
		}
		/* Enough space in hname to append defdname? */
		/* "2" is dot and ending \0 */ 
		if (ircd_res.defdname[0] &&
			strlen(ircd_res.defdname) + 2 <= size)
		{
			/* no need for strncat with above check */
			(void)strcat(hname, ".");
			(void)strcat(hname, ircd_res.defdname);
		}
	}
#endif
	return;
}

/*
** Cannot use perror() within daemon. stderr is closed in
** ircd and cannot be used. And, worse yet, it might have
** been reassigned to a normal connection...
*/

/*
** report_error
**	This a replacement for perror(). Record error to log and
**	also send a copy to all *LOCAL* opers online.
**
**	text	is a *format* string for outputting error. It must
**		contain only two '%s', the first will be replaced
**		by the sockhost from the cptr, and the latter will
**		by strerror(errno).
**
**	cptr	if not NULL, is the *LOCAL* client associated with
**		the error.
*/
void	report_error(char *text, aClient *cptr)
{
	Reg	int	errtmp = errno; /* debug may change 'errno' */
	Reg	char	*host;
	int	err;
	SOCK_LEN_TYPE len = sizeof(err);
	char	fmbuf[BUFSIZE+1];
	aClient *bysptr = NULL;
	
	extern	char	*strerror(int);

	host = (cptr) ? get_client_name(cptr, FALSE) : "";

	Debug((DEBUG_ERROR, text, host, strerror(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (cptr && !IsMe(cptr) && cptr->fd >= 0)
		if (!GETSOCKOPT(cptr->fd, SOL_SOCKET, SO_ERROR, &err, &len))
			if (err)
				errtmp = err;
#endif
	sendto_flag(SCH_ERROR, text, host, strerror(errtmp));
	if (cptr && (IsConnecting(cptr) || IsHandshake(cptr)) &&
		cptr->serv && cptr->serv->byuid[0])
	{
		bysptr = find_uid(cptr->serv->byuid, NULL);
		if (bysptr && !MyConnect(bysptr))
		{
			fmbuf[0] = '\0';
			strcpy(fmbuf, ":%s NOTICE %s :");
			strncat(fmbuf, text, BUFSIZE-strlen(fmbuf));
			sendto_one(bysptr, fmbuf, ME, bysptr->name,
				host, strerror(errtmp));
		}
	}
#ifdef USE_SYSLOG
	syslog(LOG_WARNING, text, host, strerror(errtmp));
#endif
	if (serverbooting)
	{
		fprintf(stderr,text,host,strerror(errtmp));
		fprintf(stderr,"\n");
	}
	return;
}

/*
 * inetport
 *
 * Create a socket in the AF_INET domain, bind it to the port given in
 * 'port' and listen to it. If 'ip' has a value, use it as vif to listen.
 * Connections are accepted to this socket depending on the IP# mask given
 * by 'ipmask'.  Returns the fd of the socket created or -1 on error.
 */
int	inetport(aClient *cptr, char *ip, char *ipmask, int port, int dolisten)
{
	static	struct SOCKADDR_IN server;
	int	ad[4];
	SOCK_LEN_TYPE len = sizeof(server);
	char	ipname[20];

	/* broken config? why allow such broken line to live?
	** XXX: fix initconf()? --B. */
	if (!ipmask)
	{
		sendto_flag(SCH_ERROR, "Invalid P-line");
		return -1;
	}
	ad[0] = ad[1] = ad[2] = ad[3] = 0;

	/*
	 * do it this way because building ip# from separate values for each
	 * byte requires endian knowledge or some nasty messing. Also means
	 * easy conversion of "*" to 0.0.0.0 or 134.* to 134.0.0.0 :-)
	 */
	(void)sscanf(ipmask, "%d.%d.%d.%d", &ad[0], &ad[1], &ad[2], &ad[3]);
	if (ad[0]>>8 || ad[1]>>8 || ad[2]>>8 || ad[3]>>8)
	{
		sendto_flag(SCH_ERROR, "Invalid ipmask %s", ipmask);
		return -1;
	}
	(void)sprintf(ipname, "%d.%d.%d.%d", ad[0], ad[1], ad[2], ad[3]);

	(void)sprintf(cptr->sockhost, "%-.42s.%u", ip ? ip : ME,
		      (unsigned int)port);
	DupString(cptr->auth, ipname);
	/*
	 * At first, open a new socket
	 */
	if (cptr->fd == -1)
		cptr->fd = socket(AFINET, SOCK_STREAM, 0);
	if (cptr->fd < 0)
	    {
		report_error("opening stream socket %s:%s", cptr);
		return -1;
	    }
	else if (cptr->fd >= MAXCLIENTS)
	    {
		sendto_flag(SCH_ERROR,
			    "No more connections allowed (%s)", cptr->name);
		(void)close(cptr->fd);
		return -1;
	    }
	(void)set_sock_opts(cptr->fd, cptr);
#if defined (__CYGWIN32__)
	/* Can anyone explain why setting nonblock here works and does not
	** in add_listener after we return from inetport()? --B. */
	(void)set_non_blocking(cptr->fd, cptr);
#endif
	/*
	 * Bind a port to listen for new connections if port is non-null,
	 * else assume it is already open and try get something from it.
	 */
	if (port)
	    {
		server.SIN_FAMILY = AFINET;
#ifdef INET6
		if (!ip || (!isxdigit(*ip) && *ip != ':'))
			server.sin6_addr = in6addr_any;
		else
			if(!inetpton(AF_INET6, ip, server.sin6_addr.s6_addr))
				bcopy(minus_one, server.sin6_addr.s6_addr,
				      IN6ADDRSZ);
#else
		if (!ip || !isdigit(*ip))
			server.sin_addr.s_addr = INADDR_ANY;
		else
			server.sin_addr.s_addr = inetaddr(ip);
#endif
		server.SIN_PORT = htons(port);
		/*
		 * Try 10 times to bind the socket with an interval of 20
		 * seconds. Do this so we don't have to keep trying manually
		 * to bind. Why ? Because a port that has closed often lingers
		 * around for a short time.
		 * This used to be the case.  Now it no longer is.
		 * Could cause the server to hang for too long - avalon
		 */
		if (bind(cptr->fd, (SAP)&server, sizeof(server)) == -1)
		    {
			report_error("binding stream socket %s:%s", cptr);
			(void)close(cptr->fd);
			return -1;
		    }
	    }

	if (getsockname(cptr->fd, (struct SOCKADDR *)&server, &len))
	    {
		report_error("getsockname failed for %s:%s",cptr);
		(void)close(cptr->fd);
		return -1;
	    }

	if (cptr == &me) /* KLUDGE to get it work... */
	    {
		char	buf[1024];

		(void)sprintf(buf, replies[RPL_MYPORTIS], ME, "*",
			ntohs(server.SIN_PORT));
		(void)write(0, buf, strlen(buf));
	    }

	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
#ifdef INET6
	bcopy(server.sin6_addr.s6_addr, cptr->ip.s6_addr, IN6ADDRSZ);
#else
	cptr->ip.s_addr = server.sin_addr.s_addr; /* broken on linux at least*/
#endif
	cptr->port = port;
	local[cptr->fd] = cptr;
	if (dolisten)
	{
		listen(cptr->fd, LISTENQUEUE);
	}

	return 0;
}

/*
 * add_listener
 *
 * Create a new client which is essentially the stub like 'me' to be used
 * for a socket that is passive (listen'ing for connections to be accepted).
 */
int	add_listener(aConfItem *aconf)
{
	aClient	*cptr;

	cptr = make_client(NULL);
	cptr->flags = FLAGS_LISTEN;
	cptr->acpt = cptr;
	cptr->from = cptr;
	cptr->firsttime = time(NULL);
	cptr->name = ME;
	SetMe(cptr);
	
	
	cptr->confs = make_link();
	cptr->confs->next = NULL;
	cptr->confs->value.aconf = aconf;

	open_listener(cptr);
	/* Add to linked list */
	if (ListenerLL)
	{
		ListenerLL->prev = cptr;
	}
	cptr->next = ListenerLL;
	cptr->prev = NULL;
	ListenerLL = cptr;
	
	return 0;
}

#ifdef	UNIXPORT
/*
 * unixport
 *
 * Create a socket and bind it to a filename which is comprised of the path
 * (directory where file is placed) and port (actual filename created).
 * Set directory permissions as rwxr-xr-x so other users can connect to the
 * file which is 'forced' to rwxrwxrwx (different OS's have different need of
 * modes so users can connect to the socket).
 */
int	unixport(aClient *cptr, char *path, int port)
{
	struct sockaddr_un un;
	struct stat buf;

	if ((cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	    {
		report_error("error opening unix domain socket %s:%s", cptr);
		return -1;
	    }
	else if (cptr->fd >= MAXCLIENTS)
	    {
		sendto_flag(SCH_ERROR,
			    "No more connections allowed (%s)", path);
		(void)close(cptr->fd);
		return -1;
	    }

	un.sun_family = AF_UNIX;
	(void)mkdir(path, 0755);
	sprintf(unixpath, "%s/%d", path, port);
	get_sockhost(cptr, unixpath);
	if (stat(unixpath, &buf)==0)
	{
		report_error("unix domain socket %s:%s", cptr);
		(void)close(cptr->fd);
		return -1;
	}
	strncpyzt(un.sun_path, unixpath, sizeof(un.sun_path));
	errno = 0;

	if (bind(cptr->fd, (SAP)&un, strlen(unixpath)+2) == -1)
	    {
		report_error("error binding unix socket %s:%s", cptr);
		(void)close(cptr->fd);
		return -1;
	    }
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	(void)listen(cptr->fd, LISTENQUEUE);
	(void)chmod(path, 0755);
	(void)chmod(unixpath, 0777);
	SetUnixSock(cptr);
	cptr->port = 0;
	local[cptr->fd] = cptr;

	return 0;
}
#endif

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections.  Unix sockets have
 * the path to the socket unlinked for cleanliness.
 */
void	close_listeners(void)
{
	aClient	*acptr, *bcptr;
	aConfItem *aconf;

	/*
	 * close all 'extra' listening ports we have and unlink the file
	 * name if it was a unix socket.
	 */
	for (acptr = ListenerLL; acptr; acptr = bcptr)
	{
		aconf = acptr->confs->value.aconf;
		bcptr = acptr->next; /* might get deleted by close_connection
				      */

		if (IsIllegal(aconf))
		{
#ifdef	UNIXPORT
			if (IsUnixSocket(acptr))
			{
				sprintf(unixpath, "%s/%d",
					aconf->host, aconf->port);
				(void)unlink(unixpath);
			}
#endif
			if (aconf->clients > 0)
			{
				close_client_fd(acptr);
			}
			else
			{
				close_connection(acptr);
			}
		}
	}
}

/* Opens listening socket on given listener */
void	open_listener(aClient *cptr)
{
	aConfItem *aconf;
	int dolisten = 1;

	aconf = cptr->confs->value.aconf;
	
	if (!IsListener(cptr) || cptr->fd > 0)
	{
		return;
	}
	
#ifdef	UNIXPORT
	if (*aconf->host == '/')
	{
		if (unixport(cptr, aconf->host, aconf->port))
		{
			cptr->fd = -1;
		}
	}
	else
#endif
	{
		if (IsConfDelayed(aconf) && !firstrejoindone)
		{
			dolisten = 0;
			SetListenerInactive(cptr);
		}
		if (inetport(cptr, aconf->host, aconf->name, aconf->port,
			dolisten))
		{
			/* to allow further inetport calls */
			cptr->fd = -1;
		}
	}

	if (cptr->fd >= 0)
	{
		add_fd(cptr->fd, &fdas);
		add_fd(cptr->fd, &fdall);
		set_non_blocking(cptr->fd, cptr);
	}
}

/* Reopens listening sockets on all listeners */
void	reopen_listeners(void)
{
	aClient *acptr;
	aConfItem *aconf;
	for (acptr = ListenerLL; acptr; acptr = acptr->next)
	{
		aconf = acptr->confs->value.aconf;
		if (!IsIllegal(aconf) && acptr->fd < 0)
		{
			open_listener(acptr);
		}
	}
}

void	activate_delayed_listeners(void)
{
	int cnt = 0;
	aClient *acptr;

	for (acptr = ListenerLL; acptr; acptr = acptr->next)
	{
		if (IsListenerInactive(acptr))
		{
			listen(acptr->fd, LISTENQUEUE);
			ClearListenerInactive(acptr);
			cnt++;
		}
	}

	if (cnt > 0)
	{
		sendto_flag(SCH_NOTICE, "%d listeners activated", cnt);
	}
}

void	start_iauth(int rcvdsig)
{
#if defined(USE_IAUTH)
	static time_t last = 0;
	static char first = 1;
	int sp[2], fd, val;
	static pid_t iauth_pid = 0;

	if ((bootopt & BOOT_NOIAUTH) != 0)
		return;
	if (rcvdsig == 2)
	{
		sendto_flag(SCH_AUTH, "Killing iauth...");
		if (iauth_pid)
			kill(iauth_pid, SIGTERM);
		iauth_pid = 0;
	}
	else
	if (adfd >= 0)
	    {
		if (rcvdsig)
			sendto_flag(SCH_AUTH,
			    "iauth is already running, restart canceled");
		return;
	    }
	if ((time(NULL) - last) > 90 || rcvdsig)
	    {
		sendto_flag(SCH_AUTH, "Starting iauth...");
		last = time(NULL);
		read_iauth(); /* to reset olen */
		iauth_spawn += 1;
	    }
	else
		return;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0)
	    {
		sendto_flag(SCH_ERROR, "socketpair() failed!");
		sendto_flag(SCH_AUTH, "Failed to restart iauth!");
	    }
	adfd = sp[0];
	set_non_blocking(sp[0], NULL);
	set_non_blocking(sp[1], NULL); /* less to worry about in iauth */
	val = IAUTH_BUFFER;
	if (setsockopt(sp[0], SOL_SOCKET, SO_SNDBUF, (void *) &val,
	    sizeof(val)) < 0)
			sendto_flag(SCH_AUTH,
			    "IAUTH_BUFFER too big for sp0 sndbuf, using default");
	if (setsockopt(sp[1], SOL_SOCKET, SO_SNDBUF, (void *) &val,
	    sizeof(val)) < 0)
			sendto_flag(SCH_AUTH,
			    "IAUTH_BUFFER too big for sp1 sndbuf, using default");
	if (setsockopt(sp[0], SOL_SOCKET, SO_RCVBUF, (void *) &val,
	    sizeof(val)) < 0)
			sendto_flag(SCH_AUTH,
			    "IAUTH_BUFFER too big for sp0 rcvbuf, using default");
	if (setsockopt(sp[1], SOL_SOCKET, SO_RCVBUF, (void *) &val,
	    sizeof(val)) < 0)
			sendto_flag(SCH_AUTH,
			    "IAUTH_BUFFER too big for sp1 rcvbuf, using default");
	switch ((iauth_pid = vfork()))
	    {
		case -1:
			sendto_flag(SCH_ERROR, "vfork() failed!");
			sendto_flag(SCH_AUTH, "Failed to restart iauth!");
			close(sp[0]); close(sp[1]);
			adfd = -1;
			return;
		case 0:
			for (fd = 0; fd < MAXCONNECTIONS; fd++)
				if (fd != sp[1])
					(void)close(fd);
			if (sp[1] != 0)
			    {
				(void)dup2(sp[1], 0);
				close(sp[1]);
			    }
			if (execl(IAUTH_PATH, IAUTH, NULL) < 0)
				_exit(-1); /* should really not happen.. */
		default:
			close(sp[1]);
	    }

	if (first)
		first = 0;
	else
	{
		int i;
		aClient *cptr;
		char	abuf[BUFSIZ];	/* size of abuf in vsendto_iauth */
		/* 20 is biggest possible ending "%d O\n\0", which means
		** 16-digit fd -- very unlikely :> */
		char	*e = abuf + BUFSIZ - 20;
		char	*s = abuf;

		/* Build abuf to send big buffer once (or twice) to iauth,
		** which goes faster than many consecutive small writes.
		** BitKoenig claims it saves metadata overhead on Linux and
		** does not harm other systems --B. */
		for (i = 0; i <= highest_fd; i++)
		{
			if (!(cptr = local[i]))
				continue;
			if (IsServer(cptr) || IsService(cptr))
				continue;
			
			/* if not enough room in abuf, send whatever we have
			** now and start writing from begin of abuf again. */
			if (s > e)
			{
				/* sendto_iauth() appends "\n", so we
				** remove last one */
				*(s - 1) = '\0';	
				sendto_iauth(abuf);
				s = abuf;
			}
			/* A little trick: we sprintf onto s and move s (which 
			** points inside abuf) forward, at the end of s (number
			** of bytes returned by sprintf). This makes s always 
			** point to the end of things written on abuf, which 
			** allows both next sprintf at the end (no strcat!) and
			** removing last \n when needed. */
			s += sprintf(s, "%d O\n", i);
		}
		/* send the rest */
		if (s != abuf)
		{
			*(s - 1) = '\0';
			sendto_iauth(abuf);
		}
	}
#endif
}

/*
 * init_sys
 */
void	init_sys(void)
{
	Reg	int	fd;

#ifdef RLIMIT_FD_MAX
	struct rlimit limit;

	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	    {
		if (limit.rlim_max < MAXCONNECTIONS)
		    {
			(void)fprintf(stderr, "ircd fd table is too big\n");
			(void)fprintf(stderr, "Hard Limit: %d IRC max: %d\n",
				      (int) limit.rlim_max, MAXCONNECTIONS);
			(void)fprintf(stderr,
				      "Fix MAXCONNECTIONS and recompile.\n");
			exit(-1);
		    }
		limit.rlim_cur = limit.rlim_max; /* make soft limit the max */
		if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
		    {
			(void)fprintf(stderr, "error setting max fd's to %d\n",
				      (int) limit.rlim_cur);
			exit(-1);
		    }
	    }
	/* Let's assume that if it has define for fd limit, then
	   it can do core limits, too. --B. */
	if (!getrlimit(RLIMIT_CORE, &limit))
	{
		limit.rlim_cur = limit.rlim_max;
		if (limit.rlim_cur != RLIM_INFINITY)
		{
			(void)fprintf(stderr, "warning: max core"
				" is not unlimited\n");
		}
		if (setrlimit(RLIMIT_CORE, &limit) == -1)
		{
			(void)fprintf(stderr, "error setting max core to %d\n",
				(int) limit.rlim_cur);
			exit(-1);
		}
	}
#endif
#if !defined(USE_POLL)
# ifdef sequent
#  ifndef	DYNIXPTX
	int	fd_limit;

	fd_limit = setdtablesize(MAXCONNECTIONS + 1);
	if (fd_limit < MAXCONNECTIONS)
	    {
		(void)fprintf(stderr,"ircd fd table too big\n");
		(void)fprintf(stderr,"Hard Limit: %d IRC max: %d\n",
			fd_limit, MAXCONNECTIONS);
		(void)fprintf(stderr,"Fix MAXCONNECTIONS\n");
		exit(-1);
	    }
#  endif
# endif
#endif /* USE_POLL */

#if defined(PCS) || defined(DYNIXPTX) || defined(SVR3)
	char	logbuf[BUFSIZ];

	(void)setvbuf(stderr,logbuf,_IOLBF,sizeof(logbuf));
#else
# if defined(HPUX)
	(void)setvbuf(stderr, NULL, _IOLBF, 0);
# else
#  if !defined(SVR4)
	(void)setlinebuf(stderr);
#  endif
# endif
#endif

	bzero((char *)&fdas, sizeof(fdas));
	bzero((char *)&fdall, sizeof(fdall));
	fdas.highest = fdall.highest = -1;
	/* we need stderr open, don't close() it, daemonize() will do it */
	/* after ircdwatch restarts ircd, we no longer have stderr, FIXME */
	local[0] = local[1] = local[2] = NULL;
	for (fd = 3; fd < MAXCONNECTIONS; fd++)
	{
		local[fd] = NULL;
		(void)close(fd);
	}
}

void	daemonize(void)
{
#ifdef TIOCNOTTY
	int fd;
#endif

	if (bootopt & BOOT_TTY)	/* debugging is going to a tty */
		goto init_dgram;

	(void)fclose(stdout);
	(void)close(1);

	if (!(bootopt & BOOT_DEBUG))
	{
		(void)fclose(stderr);
		(void)close(2);
	}

	if (((bootopt & BOOT_CONSOLE) || isatty(0)) &&
	    !(bootopt & BOOT_INETD))
	    {
		if (fork())
			exit(0);
#ifdef TIOCNOTTY
		if ((fd = open("/dev/tty", O_RDWR)) >= 0)
		    {
			(void)ioctl(fd, TIOCNOTTY, (char *)NULL);
			(void)close(fd);
		    }
#endif
#if defined(HPUX) || defined(SVR4) || defined(DYNIXPTX) || \
    defined(_POSIX_SOURCE) || defined(SGI)
		(void)setsid();
#elif defined (__CYGWIN32__) || defined(__APPLE__)
    		(void)setpgrp();
#else
		(void)setpgrp(0, (int)getpid());
#endif
		(void)fclose(stdin);
		(void)close(0);
	    }
init_dgram:
	resfd = init_resolver(0x1f);

	start_iauth(0);

}


void	write_pidfile(void)
{
	int fd;
	char buff[20];
	(void)truncate(IRCDPID_PATH, 0);
	if ((fd = open(IRCDPID_PATH, O_CREAT|O_WRONLY, 0600))>=0)
	    {
		bzero(buff, sizeof(buff));
		(void)sprintf(buff,"%5d\n", (int)getpid());
		if (write(fd, buff, strlen(buff)) == -1)
		{
			Debug((DEBUG_NOTICE,"Error writing to pid file %s",
			      IRCDPID_PATH));
		}
		(void)close(fd);
		return;
	    }
# ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE,"Error opening pid file %s",
			IRCDPID_PATH));
# endif
}
		
/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static	int	check_init(aClient *cptr, char *sockn)
{
	struct	SOCKADDR_IN sk;
	SOCK_LEN_TYPE len = sizeof(struct SOCKADDR_IN);

#ifdef	UNIXPORT
	if (IsUnixSocket(cptr))
	    {
		strncpyzt(sockn, cptr->acpt->sockhost, HOSTLEN+1);
		get_sockhost(cptr, sockn);
		return 0;
	    }
#endif

	/* If descriptor is a tty, special checking... */
	if (isatty(cptr->fd))
	    {
		strncpyzt(sockn, me.sockhost, HOSTLEN);
		bzero((char *)&sk, sizeof(struct SOCKADDR_IN));
	    }
	else if (getpeername(cptr->fd, (SAP)&sk, &len) == -1)
	    {
		report_error("connect failure: %s %s", cptr);
		return -1;
	    }
#ifdef INET6
	inetntop(AF_INET6, (char *)&sk.sin6_addr, sockn, INET6_ADDRSTRLEN);
	Debug((DEBUG_DNS,"sockn %x",sockn));
	Debug((DEBUG_DNS,"sockn %s",sockn));
#else
	(void)strcpy(sockn, (char *)inetntoa((char *)&sk.sin_addr));
#endif
	bcopy((char *)&sk.SIN_ADDR, (char *)&cptr->ip, sizeof(struct IN_ADDR));
	cptr->port = ntohs(sk.SIN_PORT);

	return 0;
}

/*
 * Ordinary client access check. Look for conf lines which have the same
 * status as the flags passed.
 *  0 = Success
 * -1 = Bad socket.
 * -2 = Access denied
 */
int	check_client(aClient *cptr)
{
	char	sockname[HOSTLEN+1];
	Reg	struct	hostent *hp = NULL;
	Reg	int	i;
 
#ifdef INET6
	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
		cptr->name, inet_ntop(AF_INET6, (char *)&cptr->ip, ipv6string,
				      sizeof(ipv6string))));
#else
	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
		cptr->name, inetntoa((char *)&cptr->ip)));
#endif

	if (check_init(cptr, sockname))
		return -1;

#ifdef UNIXPORT
	if (!IsUnixSocket(cptr))
#endif
		hp = cptr->hostp;
	/*
	 * Verify that the host to ip mapping is correct both ways and that
	 * the ip#(s) for the socket is listed for the host.
	 */
	if (hp)
	    {
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!bcmp(hp->h_addr_list[i], (char *)&cptr->ip,
				  sizeof(struct IN_ADDR)))
				break;
		if (!hp->h_addr_list[i])
		    {
#ifdef INET6
			sendto_flag(SCH_ERROR,
				    "IP# Mismatch: %s != %s[%08x%08x%08x%08x]",
				    inetntop(AF_INET6, (char *)&cptr->ip,
					      ipv6string,sizeof(ipv6string)), hp->h_name,
				    ((unsigned long *)hp->h_addr)[0],
				    ((unsigned long *)hp->h_addr)[1],
				    ((unsigned long *)hp->h_addr)[2],
				    ((unsigned long *)hp->h_addr)[3]); 
#else
			sendto_flag(SCH_ERROR, "IP# Mismatch: %s != %s[%08x]",
				    inetntoa((char *)&cptr->ip), hp->h_name,
				    *((unsigned long *)hp->h_addr));
#endif
			hp = NULL;
			cptr->hostp = NULL;
		    }
	    }

	if ((i = attach_Iline(cptr, hp, sockname)))
	    {
		Debug((DEBUG_DNS,"ch_cl: access denied: %s[%s]",
			cptr->name, sockname));
		return i;
	    }

	Debug((DEBUG_DNS, "ch_cl: access ok: %s[%s]",
		cptr->name, sockname));

#ifdef NO_OPER_REMOTE
	if (
#ifdef UNIXPORT
		IsUnixSocket(cptr) ||
#endif
#ifdef INET6
		IN6_IS_ADDR_LOOPBACK(&cptr->ip) ||
		/* If s6_addr32 was standard, we could just compare them,
		 * not memcmp. --B. */
		!memcmp(cptr->ip.s6_addr, mysk.sin6_addr.s6_addr, 16)
#else
		inetnetof(cptr->ip) == IN_LOOPBACKNET ||
		cptr->ip.S_ADDR == mysk.SIN_ADDR.S_ADDR
#endif
		)
	{
		ircstp->is_loc++;
		cptr->flags |= FLAGS_LOCAL;
	}
#endif /* NO_OPER_REMOTE */
	return 0;
}

/*
 * check_server_init(), check_server()
 *	check access for a server given its name (passed in cptr struct).
 *	Must check for all C/N lines which have a name which matches the
 *	name given and a host which matches. A host alias which is the
 *	same as the server name is also acceptable in the host field of a
 *	C/N line.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int	check_server_init(aClient *cptr)
{
	Reg	char	*name;
	Reg	aConfItem *c_conf = NULL, *n_conf = NULL;
	struct	hostent	*hp = NULL;
	Link	*lp;

	name = cptr->name;
	Debug((DEBUG_DNS, "sv_cl: check access for %s[%s]",
		name, cptr->sockhost));

	if (IsUnknown(cptr) && !attach_confs(cptr, name, CFLAG|NFLAG))
	    {
		Debug((DEBUG_DNS,"No C/N lines for %s", name));
		return -1;
	    }
	lp = cptr->confs;
	/*
	 * We initiated this connection so the client should have a C and N
	 * line already attached after passing through the connec_server()
	 * function earlier.
	 */
	if (IsConnecting(cptr) || IsHandshake(cptr))
	    {
		c_conf = find_conf(lp, name, CFLAG);
		n_conf = find_conf(lp, name, NFLAG);
		if (!c_conf || !n_conf)
		    {
			sendto_flag(SCH_ERROR, "Connecting Error: %s[%s]",
				   name, cptr->sockhost);
			det_confs_butmask(cptr, 0);
			return -1;
		    }
	    }
#ifdef	UNIXPORT
	if (IsUnixSocket(cptr))
	    {
		if (!c_conf)
			c_conf = find_conf(lp, name, CFLAG);
		if (!n_conf)
			n_conf = find_conf(lp, name, NFLAG);
	    }
#endif

	/*
	** If the servername is a hostname, either an alias (CNAME) or
	** real name, then check with it as the host. Use gethostbyname()
	** to check for servername as hostname.
	*/
	if (!cptr->hostp
#ifdef UNIXPORT
		&& !IsUnixSocket(cptr)
#endif
		)
	    {
		Reg	aConfItem *aconf;

		aconf = count_cnlines(lp);
		if (aconf)
		    {
			Reg	char	*s;
			Link	lin;

			/*
			** Do a lookup for the CONF line *only* and not
			** the server connection else we get stuck in a
			** nasty state since it takes a SERVER message to
			** get us here and we can't interrupt that very
			** well.
			*/
			lin.value.aconf = aconf;
			lin.flags = ASYNC_CONF;
			nextdnscheck = 1;
			if ((s = index(aconf->host, '@')))
				s++;
			else
				s = aconf->host;
			Debug((DEBUG_DNS,"sv_ci:cache lookup (%s)",s));
			hp = gethost_byname(s, &lin);
		    }
	    }
	return check_server(cptr, hp, c_conf, n_conf);
}

int	check_server(aClient *cptr, struct hostent *hp,
		aConfItem *c_conf, aConfItem *n_conf)
{
	Reg	char	*name;
	char	abuff[HOSTLEN+USERLEN+2];
	char	sockname[HOSTLEN+1], fullname[HOSTLEN+1];
	Link	*lp = cptr->confs;
	int	i;

	if (check_init(cptr, sockname))
		return -2;

check_serverback:
	if (hp)
	    {
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!bcmp(hp->h_addr_list[i], (char *)&cptr->ip,
				  sizeof(struct IN_ADDR)))
				break;
		if (!hp->h_addr_list[i])
		    {
#ifdef INET6
			sendto_flag(SCH_ERROR,
				    "IP# Mismatch: %s != %s[%08x%08x%08x%08x]",
				    inetntop(AF_INET6, (char *)&cptr->ip,
					      ipv6string, sizeof(ipv6string)),hp->h_name,
				    ((unsigned long *)hp->h_addr)[0],
				    ((unsigned long *)hp->h_addr)[1],
				    ((unsigned long *)hp->h_addr)[2],
				    ((unsigned long *)hp->h_addr)[3]); 
#else
			sendto_flag(SCH_ERROR, "IP# Mismatch: %s != %s[%08x]",
				    inetntoa((char *)&cptr->ip), hp->h_name,
				    *((unsigned long *)hp->h_addr));
#endif
			hp = NULL;
		    }
	    }
	else if (cptr->hostp)
	    {
		hp = cptr->hostp;
		goto check_serverback;
	    }

	if (hp)
		/*
		 * if we are missing a C or N line from above, search for
		 * it under all known hostnames we have for this ip#.
		 */
		for (i=0,name = hp->h_name; name ; name = hp->h_aliases[i++])
		    {
			strncpyzt(fullname, name, sizeof(fullname));
			add_local_domain(fullname, HOSTLEN-strlen(fullname));
			Debug((DEBUG_DNS, "sv_cl: gethostbyaddr: %s->%s",
				sockname, fullname));
			sprintf(abuff, "%s@%s", cptr->username, fullname);
			if (!c_conf)
				c_conf = find_conf_host(lp, abuff, CFLAG);
			if (!n_conf)
				n_conf = find_conf_host(lp, abuff, NFLAG);
			if (c_conf && n_conf)
			    {
				get_sockhost(cptr, fullname);
				break;
			    }
		    }
	name = cptr->name;

	/*
	 * Check for C and N lines with the hostname portion the ip number
	 * of the host the server runs on. This also checks the case where
	 * there is a server connecting from 'localhost'.
	 */
	if (IsUnknown(cptr) && (!c_conf || !n_conf))
	    {
		sprintf(abuff, "%s@%s", cptr->username, sockname);
		if (!c_conf)
			c_conf = find_conf_host(lp, abuff, CFLAG);
		if (!n_conf)
			n_conf = find_conf_host(lp, abuff, NFLAG);
	    }
	/*
	 * Attach by IP# only if all other checks have failed.
	 * It is quite possible to get here with the strange things that can
	 * happen when using DNS in the way the irc server does. -avalon
	 */
	if (!hp)
	    {
		if (!c_conf)
			c_conf = find_conf_ip(lp, (char *)&cptr->ip,
					      cptr->username, CFLAG);
		if (!n_conf)
			n_conf = find_conf_ip(lp, (char *)&cptr->ip,
					      cptr->username, NFLAG);
	    }
	else
		for (i = 0; hp->h_addr_list[i]; i++)
		    {
			if (!c_conf)
				c_conf = find_conf_ip(lp, hp->h_addr_list[i],
						      cptr->username, CFLAG);
			if (!n_conf)
				n_conf = find_conf_ip(lp, hp->h_addr_list[i],
						      cptr->username, NFLAG);
		    }
	/*
	 * detach all conf lines that got attached by attach_confs()
	 */
	det_confs_butmask(cptr, 0);
	/*
	 * if no C or no N lines, then deny access
	 */
	if (!c_conf || !n_conf)
	    {
		get_sockhost(cptr, sockname);
		Debug((DEBUG_DNS, "sv_cl: access denied: %s[%s@%s] c %x n %x",
			name, cptr->auth, cptr->sockhost,
			c_conf, n_conf));
		return -1;
	    }
	/*
	 * attach the C and N lines to the client structure for later use.
	 */
	(void)attach_confs(cptr, name, CONF_HUB|CONF_LEAF);
	(void)attach_conf(cptr, n_conf);
	(void)attach_conf(cptr, c_conf);
	if (IsIllegal(n_conf) || IsIllegal(c_conf))
	{
		sendto_flag(SCH_DEBUG, "Illegal class!");
		return -2;
	}
	if (!n_conf->host || !c_conf->host)
	{
		sendto_flag(SCH_DEBUG, "Null host in class!");
		return -2;
	}

	if (
#ifdef INET6
		AND16(c_conf->ipnum.s6_addr) == 255
#else
		c_conf->ipnum.s_addr == -1
#endif
#ifdef UNIXPORT
		&& !IsUnixSocket(cptr)
#endif
		)
	{
		bcopy((char *)&cptr->ip, (char *)&c_conf->ipnum,
			sizeof(struct IN_ADDR));
	}
#ifdef UNIXPORT
	if (!IsUnixSocket(cptr))
#endif
		get_sockhost(cptr, c_conf->host);

	Debug((DEBUG_DNS,"sv_cl: access ok: %s[%s]",
		name, cptr->sockhost));
	return 0;
}

/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
static	int completed_connection(aClient *cptr)
{
	aConfItem *aconf;

	SetHandshake(cptr);
	
	aconf = find_conf(cptr->confs, cptr->name, CFLAG);
	if (!aconf)
	    {
		sendto_flag(SCH_NOTICE,
			    "Lost C-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	if (!BadPtr(aconf->passwd))
		sendto_one(cptr, "PASS %s %s IRC|%s %s%s%s", aconf->passwd,
			pass_version, serveropts,
			(bootopt & BOOT_STRICTPROT) ? "P" : "",
#ifdef JAPANESE
			"j",
#else
			"",
#endif
#ifdef ZIP_LINKS
			(aconf->status == CONF_ZCONNECT_SERVER) ? "Z" :
#endif
			""
			);

	aconf = find_conf(cptr->confs, cptr->name, CONF_NOCONNECT_SERVER);
	if (!aconf)
	    {
		sendto_flag(SCH_NOTICE,
			    "Lost N-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	sendto_one(cptr, "SERVER %s 1 %s :%s",
		   my_name_for_link(ME, aconf->port), me.serv->sid, me.info);
	if (!IsDead(cptr))
	    {
		start_auth(cptr);
#if defined(USE_IAUTH)
		/*
		** This could become a bug.. but I don't think iauth needs the
		** hostname/aliases in this case. -kalt
		*/
		sendto_iauth("%d d", cptr->fd);
#endif
	    }

	return (IsDead(cptr)) ? -1 : 0;
}

/*
 * Closes FD in aClient structures.
 */
void close_client_fd(aClient *cptr)
{
	int i;
	
#ifdef SO_LINGER
	struct 	linger	sockling;

	sockling.l_onoff = 0;
	/*
	 * 2014-04-19  Kurt Roeckx
	 * s_bsd.c/close_client_fd(): Initialize the complete linger structure
	 */
	sockling.l_linger = 0;
#endif

	if (cptr->authfd >= 0)
	{
#ifdef	SO_LINGER
		if (cptr->exitc == EXITC_PING)
			if (SETSOCKOPT(cptr->authfd, SOL_SOCKET, SO_LINGER,
				       &sockling, sockling))
				report_error("setsockopt(SO_LINGER) %s:%s",
					     cptr);
#endif
		(void)close(cptr->authfd);

		cptr->authfd = -1;
	}

	if ((i = cptr->fd) >= 0)
	{
		flush_connections(i);
		if (IsServer(cptr) || IsListener(cptr))
		{	
			del_fd(i, &fdas);
#ifdef	ZIP_LINKS
			/*
			** the connection might have zip data (even if
			** FLAGS_ZIP is not set)
			*/
			zip_free(cptr);
#endif
		}
		else if (IsClient(cptr))
		{
#ifdef	SO_LINGER
			if (cptr->exitc == EXITC_PING)
				if (SETSOCKOPT(i, SOL_SOCKET, SO_LINGER,
					       &sockling, sockling))
					report_error("setsockopt(SO_LINGER) %s:%s",
						     cptr);
#endif
		 }
		del_fd(i, &fdall);
		local[i] = NULL;
		(void)close(i);

		cptr->fd = -1;
		DBufClear(&cptr->sendQ);
		DBufClear(&cptr->recvQ);
		bzero(cptr->passwd, sizeof(cptr->passwd));
	}
}

/*
** close_connection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
void	close_connection(aClient *cptr)
{
	aConfItem *aconf;

	if (IsServer(cptr))
	{
		ircstp->is_sv++;
		ircstp->is_sbs += cptr->sendB;
		ircstp->is_sbr += cptr->receiveB;
		ircstp->is_sti += timeofday - cptr->firsttime;
	}
	else if (IsClient(cptr))
	{
		ircstp->is_cl++;
		ircstp->is_cbs += cptr->sendB;
		ircstp->is_cbr += cptr->receiveB;
		ircstp->is_cti += timeofday - cptr->firsttime;
	}
	else
	{
		ircstp->is_ni++;
	}

	/*
	 * remove outstanding DNS queries.
	 */
	del_queries((char *)cptr);
	/*
	 * If the server connection has been up for a long amount of time,
	 * schedule a 'quick' reconnect, else reset the next-connect cycle.
	 */
	if (IsServer(cptr) &&
		(aconf = find_conf_exact(cptr->name, cptr->username,
		cptr->sockhost, CFLAG)))
	{
		/*
		 * Reschedule a faster reconnect, if this was a automatically
		 * connected configuration entry. (Note that if we have had
		 * a rehash in between, the status has been changed to
		 * CONF_ILLEGAL). But only do this if it was a "good" link.
		 */
		aconf->hold = timeofday;
		aconf->hold += (aconf->hold - cptr->since > HANGONGOODLINK) ?
				HANGONRETRYDELAY : ConfConFreq(aconf);
		/* nextconnect could be 0 */
		if (nextconnect > aconf->hold || nextconnect == 0)
		{
			nextconnect = aconf->hold;
		}
	}
	
	if (nextconnect == 0 && (IsHandshake(cptr) || IsConnecting(cptr)))
	{
		nextconnect = timeofday + HANGONRETRYDELAY;
	}

	if (cptr->fd >= 0)
	{
#if defined(USE_IAUTH)
		if (!IsListener(cptr) && !IsConnecting(cptr))
		{
			/* iauth doesn't know about listening FD nor
			 * cancelled outgoing connections.
			 */
			sendto_iauth("%d D", cptr->fd);
		}
#endif
		/*
		 * clean up extra sockets from P-lines which have been
		 * discarded.
		 */
		if ((cptr->acpt != &me) && !(IsListener(cptr)))
		{
			aconf = cptr->acpt->confs->value.aconf;
			
			if (aconf->clients > 0)
			{
				aconf->clients--;
			}
			
			if (!aconf->clients && IsIllegal(aconf))
			{
				close_connection(cptr->acpt);
			}
		}
	}
	
	close_client_fd(cptr);

	/* Remove from Listener Linked list */
	if (IsListener(cptr) && cptr->confs && cptr->confs->value.aconf &&
		IsIllegal(cptr->confs->value.aconf) &&
		cptr->confs->value.aconf->clients <= 0)
	{
		if (cptr->prev)
		{
			cptr->prev->next = cptr->next;
		}
		else
		{
			/* we were 1st */
			ListenerLL = cptr->next;
		}
		if (cptr->next)
		{
			cptr->next->prev = cptr->prev;
		}
	}

	det_confs_butmask(cptr, 0);
	cptr->from = NULL; /* ...this should catch them! >:) --msa */
	return;
}

/*
** set_sock_opts
*/
static	int	set_sock_opts(int fd, aClient *cptr)
{
	int	opt, ret = 0;
#ifdef SO_REUSEADDR
	opt = 1;
	if (SETSOCKOPT(fd, SOL_SOCKET, SO_REUSEADDR, &opt, opt) < 0)
		report_error("setsockopt(SO_REUSEADDR) %s:%s", cptr);
#endif
#if  defined(SO_DEBUG) && defined(DEBUGMODE) && 0
/* Solaris 2.x with SO_DEBUG writes to syslog by default */
#if !defined(SOLARIS_2) || defined(USE_SYSLOG)
	opt = 1;
	if (SETSOCKOPT(fd, SOL_SOCKET, SO_DEBUG, &opt, opt) < 0)
		report_error("setsockopt(SO_DEBUG) %s:%s", cptr);
#endif /* SOLARIS_2 */
#endif
#if defined(SO_USELOOPBACK) && !defined(__CYGWIN32__)
	opt = 1;
	if (SETSOCKOPT(fd, SOL_SOCKET, SO_USELOOPBACK, &opt, opt) < 0)
		report_error("setsockopt(SO_USELOOPBACK) %s:%s", cptr);
#endif
#ifdef	SO_RCVBUF
	opt = 8192;
	if (SETSOCKOPT(fd, SOL_SOCKET, SO_RCVBUF, &opt, opt) < 0)
		report_error("setsockopt(SO_RCVBUF) %s:%s", cptr);
#endif
#ifdef	SO_SNDBUF
# ifdef	_SEQUENT_
/* seems that Sequent freezes up if the receving buffer is a different size
 * to the sending buffer (maybe a tcp window problem too).
 */
# endif
	opt = 8192;
	if (SETSOCKOPT(fd, SOL_SOCKET, SO_SNDBUF, &opt, opt) < 0)
		report_error("setsockopt(SO_SNDBUF) %s:%s", cptr);
# ifdef	SO_SNDLOWAT
	/*
	 * Setting the low water mark should improve performence by avoiding
	 * early returns from select()/poll().  It shouldn't delay sending
	 * data, provided that io_loop() combines read_message() and
	 * flush_fdary/connections() calls properly. -kalt
	 * This call isn't always implemented, even when defined.. so be quiet
	 * about errors. -kalt
	 */
	opt = 8192;
	SETSOCKOPT(fd, SOL_SOCKET, SO_SNDLOWAT, &opt, opt);
# endif
#endif
#if defined(IP_OPTIONS) && defined(IPPROTO_IP) && !defined(AIX) && \
    !defined(INET6)
	/*
	 * Mainly to turn off and alert us to source routing, here.
	 * Method borrowed from Wietse Venema's TCP wrapper.
	 */
	{
	    if (!IsListener(cptr)
#ifdef UNIXPORT
		&& !IsUnixSocket(cptr)
#endif
		)
		{
		    u_char	opbuf[256], *t = opbuf;
		    char	*s = readbuf;

		    opt = sizeof(opbuf);
		    if (GETSOCKOPT(fd, IPPROTO_IP, IP_OPTIONS, t, &opt) == -1)
			    report_error("getsockopt(IP_OPTIONS) %s:%s", cptr);
		    else if (opt > 0)
			{
			    for (; opt > 0; opt--, s+= 3)
				    (void)sprintf(s, " %02x", *t++);
			    *s = '\0';
			    sendto_flag(SCH_NOTICE,
					"Connection %s with IP opts%s",
					get_client_name(cptr, TRUE), readbuf);
			    Debug((DEBUG_NOTICE,
				   "Connection %s with IP opts%s",
				   get_client_name(cptr, TRUE), readbuf));
			    ret = -1;
			}
		}
	}
#endif
	return ret;
}

int	get_sockerr(aClient *cptr)
{
	int errtmp = errno, err = 0;
	SOCK_LEN_TYPE len = sizeof(err);

#ifdef	SO_ERROR
	if (cptr->fd >= 0)
		if (!GETSOCKOPT(cptr->fd, SOL_SOCKET, SO_ERROR, &err, &len))
			if (err)
				errtmp = err;
#endif
	return errtmp;
}

/*
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
void	set_non_blocking(int fd, aClient *cptr)
{
	int	res, nonb = 0;

	/*
	** NOTE: consult ALL your relevant manual pages *BEFORE* changing
	**	 these ioctl's.  There are quite a few variations on them,
	**	 as can be seen by the PCS one.  They are *NOT* all the same.
	**	 Heed this well. - Avalon.
	*/
#ifdef NBLOCK_POSIX
	nonb |= O_NONBLOCK;
#endif
#ifdef NBLOCK_BSD
	nonb |= O_NDELAY;
#endif
#ifdef NBLOCK_SYSV
	/* This portion of code might also apply to NeXT.  -LynX */
	res = 1;

	if (ioctl (fd, FIONBIO, &res) < 0)
		report_error("ioctl(fd,FIONBIO) failed for %s:%s", cptr);
#else
	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		report_error("fcntl(fd, F_GETFL) failed for %s:%s",cptr);
	else if (fcntl(fd, F_SETFL, res | nonb) == -1)
		report_error("fcntl(fd, F_SETL, nonb) failed for %s:%s",cptr);
#endif
	return;
}

#ifdef	CLONE_CHECK
/* 
 * check_clones
 * adapted by jecete 4 IRC Ptnet
 */
static  int     check_clones(aClient *cptr)
{
	struct abacklog {
		struct  IN_ADDR ip;
		time_t  PT;
		struct abacklog *next;
	};
	static		struct abacklog *backlog = NULL;
	register	struct abacklog **blscn = &backlog,
					*blptr;
	register	int count = 0;

	/* First, ditch old entries */
	while (*blscn != NULL)
	    {
		if ((*blscn)->PT+CLONE_PERIOD < timeofday)
		    {
			blptr= *blscn;
			*blscn=blptr->next;
			MyFree(blptr);
		    }
		else
			blscn = &(*blscn)->next;
	    }
	/* Now add new item to the list */
	blptr = (struct abacklog *) MyMalloc(sizeof(struct abacklog));
#ifdef INET6
	bcopy(cptr->ip.s6_addr, blptr->ip.s6_addr, IN6ADDRSZ);
#else
	blptr->ip.s_addr = cptr->ip.s_addr;
#endif
	blptr->PT = timeofday;
	blptr->next = backlog;
	backlog = blptr;
	
	/* Count the number of entries from the same host */
	blptr = backlog;
	while (blptr != NULL)
	    {
#ifdef INET6
		if (bcmp(blptr->ip.s6_addr, cptr->ip.s6_addr, IN6ADDRSZ) == 0)
#else
		if (blptr->ip.s_addr == cptr->ip.s_addr)
#endif
			count++;
		blptr = blptr->next;
	    }
       return (count);
}
#endif

/* moved from inner blocks of add_connection to get rid of gotos */
void	add_connection_refuse(int fd, aClient *acptr, int delay)
{
#if !defined(DELAY_CLOSE)
	delay = 0;
#endif
	if (!delay)
	{
		(void)close(fd);
	}
	ircstp->is_ref++;
	acptr->fd = -2;
	free_client(acptr);
}

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yet since it doesnt have a name.
 */
aClient	*add_connection(aClient *cptr, int fd)
{
	Link	lin;
	aClient *acptr;
	aConfItem *aconf = NULL;
	acptr = make_client(NULL);

	aconf = cptr->confs->value.aconf;
	acptr->acpt = cptr;

	/* Removed preliminary access check. Full check is performed in
	 * m_server and m_user instead. Also connection time out help to
	 * get rid of unwanted connections.
	 */
	if (isatty(fd)) /* If descriptor is a tty, special checking... */
		get_sockhost(acptr, cptr->sockhost);
	else
	    {
		struct	SOCKADDR_IN addr;
		SOCK_LEN_TYPE len = sizeof(struct SOCKADDR_IN);

		if (getpeername(fd, (SAP)&addr, &len) == -1)
		    {
#if defined(linux)
			if (errno != ENOTCONN)
#endif
				report_error("Failed in connecting to %s :%s",
					     cptr);
			add_connection_refuse(fd, acptr, 0);
			return NULL;
		    }
		/* don't want to add "Failed in connecting to" here.. */
		if (aconf && IsIllegal(aconf))
		{
			add_connection_refuse(fd, acptr, 0);
			return NULL;
		}
		/* Copy ascii address to 'sockhost' just in case. Then we
		 * have something valid to put into error messages...
		 */
#ifdef INET6
		inetntop(AF_INET6, (char *)&addr.sin6_addr, ipv6string,
			  sizeof(ipv6string));
		get_sockhost(acptr, (char *)ipv6string);
#else
		get_sockhost(acptr, (char *)inetntoa((char *)&addr.sin_addr));
#endif
		bcopy ((char *)&addr.SIN_ADDR, (char *)&acptr->ip,
			sizeof(struct IN_ADDR));
		acptr->port = ntohs(addr.SIN_PORT);

#ifdef	CLONE_CHECK
		if (check_clones(acptr) > CLONE_MAX)
		{
			sendto_flag(SCH_LOCAL, "Rejecting connection from %s.",
				acptr->sockhost);
			acptr->exitc = EXITC_CLONE;
			sendto_flog(acptr, EXITC_CLONE, "", acptr->sockhost);
#ifdef DELAY_CLOSE
			nextdelayclose = delay_close(fd);
#else
			(void)send(fd, "ERROR :Too rapid connections from your "
				"host\r\n", 46, 0);
#endif
			/* If DELAY_CLOSE is not defined, delay will
			** be changed to 0 inside. --B. */
			add_connection_refuse(fd, acptr, 1);
			return NULL;
		}
#endif
		lin.flags = ASYNC_CLIENT;
		lin.value.cptr = acptr;
		lin.next = NULL;
#ifdef INET6
		Debug((DEBUG_DNS, "lookup %s",
		       inet_ntop(AF_INET6, (char *)&addr.sin6_addr,
				 ipv6string, sizeof(ipv6string))));
#else
		Debug((DEBUG_DNS, "lookup %s",
		       inetntoa((char *)&addr.sin_addr)));
#endif
		acptr->hostp = gethost_byaddr((char *)&acptr->ip, &lin);
		if (!acptr->hostp)
			SetDNS(acptr);
		nextdnscheck = 1;
	    }

	acptr->fd = fd;
	set_non_blocking(acptr->fd, acptr);
	if (set_sock_opts(acptr->fd, acptr) == -1)
	{
		add_connection_refuse(fd, acptr, 0);
		return NULL;
	}
	if (aconf)
		aconf->clients++;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = acptr;
	add_fd(fd, &fdall);
	add_client_to_list(acptr);
	start_auth(acptr);
#if defined(USE_IAUTH)
	if (!isatty(fd) && !DoingDNS(acptr))
	    {
		int i = 0;
		
		while (acptr->hostp->h_aliases[i])
			sendto_iauth("%d A %s", acptr->fd,
				     acptr->hostp->h_aliases[i++]);
		if (acptr->hostp->h_name)
			sendto_iauth("%d N %s",acptr->fd,acptr->hostp->h_name);
		else if (acptr->hostp->h_aliases[0])
			sendto_iauth("%d n", acptr->fd);
	    }
#endif
	return acptr;
}

#ifdef	UNIXPORT
static	aClient	*add_unixconnection(aClient *cptr, int fd)
{
	aClient *acptr;
	aConfItem *aconf = NULL;

	acptr = make_client(NULL);

	/* Copy ascii address to 'sockhost' just in case. Then we
	 * have something valid to put into error messages...
	 */
	get_sockhost(acptr, me.sockhost);
	aconf = cptr->confs->value.aconf;
	if (aconf)
	    {
		if (IsIllegal(aconf))
		    {
			ircstp->is_ref++;
			acptr->fd = -2;
			free_client(acptr);
			(void)close(fd);
			return NULL;
		    }
		else
			aconf->clients++;
	    }
	acptr->fd = fd;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = acptr;
	add_fd(fd, &fdall);
	acptr->acpt = cptr;
	SetUnixSock(acptr);
	bcopy((char *)&me.ip, (char *)&acptr->ip, sizeof(struct IN_ADDR));

	add_client_to_list(acptr);
	set_non_blocking(acptr->fd, acptr);
	(void)set_sock_opts(acptr->fd, acptr);
# if defined(USE_IAUTH)
	/*
	** iauth protocol and iauth itself should be extended to alllow
	** dealing with this type of connection.
	*/
	sendto_iauth("%d O", acptr->fd);
	SetDoneXAuth(acptr);
# endif
	return acptr;
}
#endif

/*
** read_listener
**
** Accept incoming connections, extracted from read_message() 98/12 -kalt
** Up to LISTENER_MAXACCEPT connections will be accepted in one run.
*/
static	void	read_listener(aClient *cptr)
{
	int fdnew, max = LISTENER_MAXACCEPT;
	aClient	*acptr;

	while (max--)
	    {
		/*
		** There may be many reasons for error return, but in otherwise
		** correctly working environment the probable cause is running
		** out of file descriptors (EMFILE, ENFILE or others?). The
		** man pages for accept don't seem to list these as possible,
		** although it's obvious that it may happen here.
		** Thus no specific errors are tested at this point, just
		** assume that connections cannot be accepted until some old
		** is closed first.
		*/
		if ((fdnew = accept(cptr->fd, NULL, NULL)) < 0)
		    {
			if (errno != EWOULDBLOCK)
				report_error("Cannot accept connection %s:%s",
					     cptr);
			break;
		    }
		ircstp->is_ac++;
		if (fdnew >= MAXCLIENTS)
		    {
			ircstp->is_ref++;
			sendto_flag(SCH_ERROR, "All connections in use. (%s)",
				    get_client_name(cptr, TRUE));
			find_bounce(NULL, 0, fdnew);
			(void)send(fdnew, "ERROR :All connections in use\r\n",
				   32, 0);
			(void)close(fdnew);
			continue;
		    }

		/* Can cptr->confs->value.aconf be NULL? --B. */
		if ((iconf.caccept == 0 ||
			/*
			 * 2011-01-20  Piotr Kucharski
			 *  * s_bsd.c/read_listener(), s_serv.c/report_listeners(): use IsSplit()
			 *    instead of iconf.split==1 (reported by BR).
			 */
			(iconf.caccept == 2 && IsSplit()))
			&& cptr->confs->value.aconf != NULL
			&& IsConfDelayed(cptr->confs->value.aconf))
		{
			/* Should we bother sendto_flag(SCH_ERROR...? --B. */
#ifdef CACCEPT_DELAYED_CLOSE
			nextdelayclose = delay_close(fdnew);
			ircstp->is_ref++;
#else
			(void)send(fdnew, "ERROR :All client connections are "
				"temporarily refused.\r\n", 56, 0);
			(void)close(fdnew);
#endif
			continue;
		}

#ifdef	UNIXPORT
		if (IsUnixSocket(cptr))
			acptr = add_unixconnection(cptr, fdnew);
		else
#endif
		acptr = add_connection(cptr, fdnew);

		if (acptr == NULL)
		{
			continue;
		}
		nextping = timeofday; /* isn't this abusive? -kalt */
		istat.is_unknown++;

		/* Notice on connect. */
		sendto_one(acptr, replies[RPL_HELLO], ME, HELLO_MSG);
	    }
}

/*
** client_packet
**
** Process data from receive buffer to client.
** Extracted from read_packet() 960804/291p3/Vesa
*/
static	int	client_packet(aClient *cptr)
{
	Reg	int	dolen = 0;

	while (DBufLength(&cptr->recvQ) && !NoNewLine(cptr) &&
	       ((cptr->status < STAT_UNKNOWN) ||
		(cptr->since - timeofday < MAXPENALTY)))
	    {
		/*
		** If it has become registered as a Service or Server
		** then skip the per-message parsing below.
		*/
		if (IsService(cptr) || IsServer(cptr))
		    {
			dolen = dbuf_get(&cptr->recvQ, readbuf,
					 sizeof(readbuf));
			if (dolen <= 0)
				break;
			dolen = dopacket(cptr, readbuf, dolen);
			if (dolen == 2 && cptr->since == cptr->lasttime)
				cptr->since += 5;
			if (dolen)
				return dolen;
			break;
		    }
		dolen = dbuf_getmsg(&cptr->recvQ, readbuf,
				    sizeof(readbuf));
		/*
		** Devious looking...whats it do ? well..if a client
		** sends a *long* message without any CR or LF, then
		** dbuf_getmsg fails and we pull it out using this
		** loop which just gets the next 512 bytes and then
		** deletes the rest of the buffer contents.
		** -avalon
		*/
		while (dolen <= 0)
		    {
			if (dolen < 0)
				return exit_client(cptr, cptr, &me,
						   "dbuf_getmsg fail");
			if (DBufLength(&cptr->recvQ) < 510)
			    {	/* hmm? */
				cptr->flags |= FLAGS_NONL;
				break;
			    }
			dolen = dbuf_get(&cptr->recvQ, readbuf, 511);
			if (dolen > 0 && DBufLength(&cptr->recvQ))
				DBufClear(&cptr->recvQ);
		    }

		/* Is it okay not to test for other return values? -krys */
		if (dolen > 0 &&
		    (dopacket(cptr, readbuf, dolen) == FLUSH_BUFFER))
			return FLUSH_BUFFER;
	    }
	return 1;
}

/*
** read_packet
**
** Read a 'packet' of data from a connection and process it.  Read in 8k
** chunks to give a better performance rating (for server connections).
** Do some tricky stuff for client connections to make sure they don't do
** any flooding >:-) -avalon
*/
static	int	read_packet(aClient *cptr, int msg_ready)
{
	Reg	int	length = 0, done;

	if (msg_ready &&
	    !(IsPerson(cptr) && DBufLength(&cptr->recvQ) > 6090))
	    {
		errno = 0;
#ifdef INET6
		length = recvfrom(cptr->fd, readbuf, sizeof(readbuf), 0, 0, 0);
#else
		length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);
#endif
#if defined(DEBUGMODE) && defined(DEBUG_READ)
		if (length > 0)
			Debug((DEBUG_READ,
				"recv = %d bytes from %d[%s]:[%*.*s]\n",
				length, cptr->fd, cptr->name, length, length,
				readbuf));
#endif

		Debug((DEBUG_DEBUG, "Received %d(%d-%s) bytes from %d %s",
			length, errno, strerror(errno),
			cptr->fd, get_client_name(cptr, TRUE)));
		cptr->lasttime = timeofday;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT|FLAGS_NONL);
		/*
		 * If not ready, fake it so it isnt closed
		 */
		if (length == -1 &&
			((errno == EWOULDBLOCK) || (errno == EAGAIN)))
			return 1;
		if (length <= 0)
			return length;
	    }
	else if (msg_ready)
		return exit_client(cptr, cptr, &me, "EOF From Client");

	/*
	** For server connections, we process as many as we can without
	** worrying about the time of day or anything :)
	*/
	if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr) ||
	    IsService(cptr))
	    {
		if (length > 0)
		    {
			done = dopacket(cptr, readbuf, length);
			if (done && done != 2)
				return done;
		    }
	    }
	else
	    {
		/*
		** Before we even think of parsing what we just read, stick
		** it on the end of the receive queue and do it when its
		** turn comes around.
		*/
		/* why no poolsize increase here like in send? --B. */
		if (length && dbuf_put(&cptr->recvQ, readbuf, length) < 0)
			return exit_client(cptr, cptr, &me, "dbuf_put fail");

		if (IsPerson(cptr) &&
		    DBufLength(&cptr->recvQ) > CLIENT_FLOOD
		    && !is_allowed(cptr, ACL_CANFLOOD))
		    {
			cptr->exitc = EXITC_FLOOD;
			return exit_client(cptr, cptr, &me, "Excess Flood");
		    }

		return client_packet(cptr);
	    }
	return 1;
}


/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 * 
 * Don't ever use ZERO for delay, unless you mean to poll and then
 * you have to have sleep/wait somewhere else in the code.--msa
 * Actually, ZERO is NOT ZERO anymore.. see below -kalt
 */
int	read_message(time_t delay, FdAry *fdp, int ro)
{
#if !defined(USE_POLL)
# define SET_READ_EVENT( thisfd )	FD_SET( thisfd, &read_set)
# define SET_WRITE_EVENT( thisfd )	FD_SET( thisfd, &write_set)
# define CLR_READ_EVENT( thisfd )	FD_CLR( thisfd, &read_set)
# define CLR_WRITE_EVENT( thisfd )	FD_CLR( thisfd, &write_set)
# define TST_READ_EVENT( thisfd )	FD_ISSET( thisfd, &read_set)
# define TST_WRITE_EVENT( thisfd )	FD_ISSET( thisfd, &write_set)

	fd_set	read_set, write_set;
	int	highfd = -1;
#else
/* most of the following use pfd */
# define POLLSETREADFLAGS	(POLLIN|POLLRDNORM)
# define POLLREADFLAGS		(POLLSETREADFLAGS|POLLHUP|POLLERR)
# define POLLSETWRITEFLAGS	(POLLOUT|POLLWRNORM)
# define POLLWRITEFLAGS		(POLLOUT|POLLWRNORM|POLLHUP|POLLERR)

# define SET_READ_EVENT( thisfd ){  CHECK_PFD( thisfd );\
				   pfd->events |= POLLSETREADFLAGS;}
# define SET_WRITE_EVENT( thisfd ){ CHECK_PFD( thisfd );\
				   pfd->events |= POLLSETWRITEFLAGS;}

# define CLR_READ_EVENT( thisfd )	pfd->revents &= ~POLLSETREADFLAGS
# define CLR_WRITE_EVENT( thisfd )	pfd->revents &= ~POLLSETWRITEFLAGS
# define TST_READ_EVENT( thisfd )	pfd->revents & POLLREADFLAGS
# define TST_WRITE_EVENT( thisfd )	pfd->revents & POLLWRITEFLAGS

# define CHECK_PFD( thisfd ) 			\
	if ( pfd->fd != thisfd ) {		\
		pfd = &poll_fdarray[nbr_pfds++];\
		pfd->fd     = thisfd;		\
		pfd->events = 0;		\
		pfd->revents = 0;		\
	}

	struct pollfd   poll_fdarray[MAXCONNECTIONS];
	struct pollfd * pfd     = poll_fdarray;
	struct pollfd * res_pfd = NULL;
	struct pollfd * udp_pfd = NULL;
	struct pollfd * ad_pfd = NULL;
	aClient	 * authclnts[MAXCONNECTIONS];	/* mapping of auth fds to client ptrs */
	int	   nbr_pfds = 0;
#endif

	aClient	*cptr;
	int	nfds, ret = 0;
	struct	timeval	wait;
	time_t	delay2 = delay;
	int	res, length, fd, i;
	int	auth;
	int	write_err = 0;

	for (res = 0;;)
	    {
#if !defined(USE_POLL)
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
#else
		/* set up such that CHECK_FD works */
		nbr_pfds = 0;
		pfd 	 = poll_fdarray;
		pfd->fd  = -1;
		res_pfd  = NULL;
		udp_pfd  = NULL;
		ad_pfd = NULL;
#endif	/* USE_POLL */
		auth = 0;

#if defined(USE_POLL)
		if ( auth == 0 )
			bzero((char *) authclnts, sizeof( authclnts ));
#endif
		for (i = fdp->highest; i >= 0; i--)
		    {
			fd = fdp->fd[i];
			if (!(cptr = local[fd]))
				continue;
			Debug((DEBUG_L11, "fd %d cptr %#x %d %#x %s",
				fd, cptr, cptr->status, cptr->flags,
				get_client_name(cptr,TRUE)));
			/* authentication fd's */
			if (DoingAuth(cptr))
			    {
				auth++;
				SET_READ_EVENT(cptr->authfd);
				Debug((DEBUG_NOTICE,"auth on %x %d", cptr,
					fd));
				if (cptr->flags & FLAGS_WRAUTH)
					SET_WRITE_EVENT(cptr->authfd);
#if defined(USE_POLL)
				authclnts[cptr->authfd] = cptr;
#else
				if (cptr->authfd > highfd)
					highfd = cptr->authfd;
#endif
			    }
			/*
			** if any of these is true, data won't be parsed
			** so no need to check for anything!
			*/
#if defined(USE_IAUTH)
			if (DoingDNS(cptr) || DoingAuth(cptr) ||
			    WaitingXAuth(cptr) ||
			    (DoingXAuth(cptr) &&
			     !(iauth_options & XOPT_EARLYPARSE)))
#else
			if (DoingDNS(cptr) || DoingAuth(cptr))
#endif
				continue;
#if !defined(USE_POLL)
			if (fd > highfd)
				highfd = fd;
#endif
			if (IsListener(cptr))
			    {
				if (
#ifdef LISTENER_DELAY
					/* Checking for new connections is only
					** done up to once per LD seconds. */
					timeofday >= cptr->lasttime + 
						LISTENER_DELAY && 
#endif
					ro == 0)
				    {
					SET_READ_EVENT( fd );
				    }
				else if (delay2 > 1)
					delay2 = 1;
				continue;
			    }
			
			/*
			** This is very approximate, it should take
			** cptr->since into account. -kalt
			*/
			if (DBufLength(&cptr->recvQ) && delay2 > 2)
				delay2 = 1;

			if (IsRegisteredUser(cptr))
			    {
				if (cptr->since - timeofday < MAXPENALTY+1)
					SET_READ_EVENT( fd );
			    }
			else if (DBufLength(&cptr->recvQ) < 4088)
				SET_READ_EVENT( fd );

			/*
			** If we have anything in the sendQ, check if there is
			** room to write data.
			*/
			if (DBufLength(&cptr->sendQ) || 
#ifdef	ZIP_LINKS
			    ((cptr->flags & FLAGS_ZIP) &&
			     (cptr->zip->outcount > 0)) ||
#endif
			    IsConnecting(cptr))
			{
				if (IsServer(cptr) || IsConnecting(cptr) ||
					ro == 0)
				{
					SET_WRITE_EVENT(fd);
				}
			}
		    }

		if (udpfd >= 0)
		    {
			SET_READ_EVENT(udpfd);
#if !defined(USE_POLL)
			if (udpfd > highfd)
				highfd = udpfd;
#else
			udp_pfd = pfd;
#endif
		    }
		if (resfd >= 0)
		    {
			SET_READ_EVENT(resfd);
#if !defined(USE_POLL)
			if (resfd > highfd)
				highfd = resfd;
#else
			res_pfd = pfd;
#endif			
		    }
#if defined(USE_IAUTH)
		if (adfd >= 0)
		    {
			SET_READ_EVENT(adfd);
# if ! USE_POLL
			if (adfd > highfd)
				highfd = adfd;
# else
			ad_pfd = pfd;
# endif			
		    }
#endif
		Debug((DEBUG_L11, "udpfd %d resfd %d adfd %d", udpfd, resfd,
		       adfd));
#if !defined(USE_POLL)
		Debug((DEBUG_L11, "highfd %d", highfd));
#endif
		
		wait.tv_sec = MIN(delay2, delay);
		wait.tv_usec = (delay == 0) ? 200000 : 0;
#if !defined(USE_POLL)
		nfds = select(highfd + 1, (SELECT_FDSET_TYPE *)&read_set,
			      (SELECT_FDSET_TYPE *)&write_set, 0, &wait);
#else
		nfds = poll( poll_fdarray, nbr_pfds,
			     wait.tv_sec * 1000 + wait.tv_usec/1000 );
#endif
		ret = nfds;
		if (nfds == -1 && errno == EINTR)
			return -1;
		else if (nfds >= 0)
			break;
#if !defined(USE_POLL)
		report_error("select %s:%s", &me);
#else
		report_error("poll %s:%s", &me);
#endif
		res++;
		if (res > 5)
			restart("too many select()/poll() errors");
		sleep(10);
		timeofday = time(NULL);
	    } /* for(res=0;;) */
	
	timeofday = time(NULL);
	if (nfds > 0 &&
#if !defined(USE_POLL)
	    resfd >= 0 &&
#else
	    (pfd = res_pfd) &&
#endif
	    TST_READ_EVENT(resfd))
	    {
		CLR_READ_EVENT(resfd);
		nfds--;
		do_dns_async();
	    }
	if (nfds > 0 &&
#if !defined(USE_POLL)
	    udpfd >= 0 &&
#else
	    (pfd = udp_pfd) &&
#endif
	    TST_READ_EVENT(udpfd))
	    {
		CLR_READ_EVENT(udpfd);
		nfds--;
		polludp();
	    }
#if defined(USE_IAUTH)
	if (nfds > 0 &&
# if ! USE_POLL
	    adfd >= 0 &&
# else
	    (pfd = ad_pfd) &&
# endif
	    TST_READ_EVENT(adfd))
	    {
		CLR_READ_EVENT(adfd);
		nfds--;
		read_iauth();
	    }
#endif

#if !defined(USE_POLL)
	for (i = fdp->highest; i >= 0; i--)
#else
	for (pfd = poll_fdarray, i = 0; i < nbr_pfds; i++, pfd++ )
#endif
	    {
#if !defined(USE_POLL)
		fd = fdp->fd[i];
		if (!(cptr = local[fd]))
			continue;
#else
		fd = pfd->fd;
		if ((cptr = authclnts[fd]))
		    {
#endif
			/*
			 * check for the auth fd's
			 */
			if (auth > 0 && nfds > 0
#if !defined(USE_POLL)
			    && cptr->authfd >= 0
#endif
			    )
			    {
				auth--;
				if (TST_WRITE_EVENT(cptr->authfd))
				    {
					nfds--;
					send_authports(cptr);
				    }
				else if (TST_READ_EVENT(cptr->authfd))
				    {
					nfds--;
					read_authports(cptr);
				    }
				continue;
			    }
#if defined(USE_POLL)
		    }
		fd = pfd->fd;
		if (!(cptr = local[fd]))
			continue;
#else
		fd = cptr->fd;
#endif
		/*
		 * accept connections
		 */
		if (TST_READ_EVENT(fd) && IsListener(cptr) &&
			!IsListenerInactive(cptr))
		    {
			CLR_READ_EVENT(fd);
			cptr->lasttime = timeofday;
			read_listener(cptr);
			continue;
		    }
		if (IsMe(cptr))
			continue;
		if (TST_WRITE_EVENT(fd))
		    {
			write_err = 0;
			/*
			** ...room for writing, empty some queue then...
			*/
			if (IsConnecting(cptr))
				write_err = completed_connection(cptr);
			if (!write_err)
				(void)send_queued(cptr);
			if (IsDead(cptr) || write_err)
			    {
deadsocket:
				if (TST_READ_EVENT(fd))
					CLR_READ_EVENT(fd);
				if (cptr->exitc == EXITC_SENDQ)
				{
					(void)exit_client(cptr,cptr,&me,
						"Max SendQ exceeded");
				}
				else
				{
					/* Keep (primary) error or it will not
					 * be possible to discriminate socket
					 * error from mbuf error. --B. */
					if (cptr->exitc == EXITC_REG)
						cptr->exitc = EXITC_ERROR;
					(void)exit_client(cptr, cptr, &me,
						strerror(get_sockerr(cptr)));
				}
				continue;
			    }
		    }
		length = 1;	/* for fall through case */
		if (!NoNewLine(cptr) || TST_READ_EVENT(fd))
		    {
			if (!DoingAuth(cptr))
				length = read_packet(cptr, TST_READ_EVENT(fd));
		    }
		readcalls++;
		if (length == FLUSH_BUFFER)
			continue;
		else if (length > 0)
			flush_connections(cptr->fd);
		if (IsDead(cptr))
			goto deadsocket;
		if (length > 0)
			continue;
		
		/* Ghost! Unknown users are tagged in parse() since 2.9.
		 * Let's not drop the uplink but just the ghost's message.
		 */
		if (length == -3)
			continue;
		
		/*
		** NB: This following section has been modified to *expect*
		**     cptr to be valid (ie if (length == FLUSH_BUFFER) is
		**     above and stays there). - avalon 24/9/94
		*/
		/*
		** ...hmm, with non-blocking sockets we might get
		** here from quite valid reasons, although.. why
		** would select report "data available" when there
		** wasn't... so, this must be an error anyway...  --msa
		** actually, EOF occurs when read() returns 0 and
		** in due course, select() returns that fd as ready
		** for reading even though it ends up being an EOF. -avalon
		*/
		Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
		       cptr->fd, errno, length));
		
		if (IsServer(cptr) || IsHandshake(cptr))
		    {
			int timeconnected = timeofday - cptr->firsttime;
			
			if (length == 0)
				sendto_flag(SCH_NOTICE,
		     "Server %s closed the connection (%d, %2d:%02d:%02d)",
					     get_client_name(cptr, FALSE),
					     timeconnected / 86400,
					     (timeconnected % 86400) / 3600,
					     (timeconnected % 3600)/60, 
					     timeconnected % 60);
			else	/* this must be for -1 */
			    {
				report_error("Lost connection to %s:%s",cptr);
				sendto_flag(SCH_NOTICE,
			     "%s had been connected for %d, %2d:%02d:%02d",
					     get_client_name(cptr, FALSE),
					     timeconnected / 86400,
					     (timeconnected % 86400) / 3600,
					     (timeconnected % 3600)/60, 
					     timeconnected % 60);
			    }
		    }
		(void)exit_client(cptr, cptr, &me, length >= 0 ?
				  "EOF From client" :
				  strerror(get_sockerr(cptr)));
	    } /* for(i) */
	return ret;
}

/*
 * connect_server
 */
int	connect_server(aConfItem *aconf, aClient *by, struct hostent *hp)
{
	Reg	struct	SOCKADDR *svp;
	Reg	aClient *cptr, *c2ptr;
	Reg	char	*s;
	int	i, len;

#ifdef INET6
	Debug((DEBUG_NOTICE,"Connect to %s[%s] @%s",
	       aconf->name, aconf->host,
	       inet_ntop(AF_INET6, (char *)&aconf->ipnum, ipv6string,
			 sizeof(ipv6string))));
#else
	Debug((DEBUG_NOTICE,"Connect to %s[%s] @%s",
	       aconf->name, aconf->host,
	       inetntoa((char *)&aconf->ipnum)));
#endif

	if ((c2ptr = find_server(aconf->name, NULL)))
	    {
		sendto_flag(SCH_NOTICE, "Server %s already present from %s",
			    aconf->name, get_client_name(c2ptr, TRUE));
		if (by && IsPerson(by) && !MyClient(by))
		  sendto_one(by,
			     ":%s NOTICE %s :Server %s already present from %s",
			     ME, by->name, aconf->name,
			     get_client_name(c2ptr, TRUE));
		return -1;
	    }

	/*
	 * If we don't know the IP# for this host and it is a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	if (!aconf->ipnum.S_ADDR && *aconf->host != '/')
	    {
		Link    lin;

		lin.flags = ASYNC_CONNECT;
		lin.value.aconf = aconf;
		nextdnscheck = 1;
		s = (char *)index(aconf->host, '@');
		s++; /* should NEVER be NULL */
#ifdef INET6
		if (!inetpton(AF_INET6, s, aconf->ipnum.s6_addr))
#else
		if ((aconf->ipnum.s_addr = inetaddr(s)) == -1)
#endif
		    {
#ifdef INET6
			bzero(aconf->ipnum.s6_addr, IN6ADDRSZ);
#else
			aconf->ipnum.s_addr = 0;
#endif
			hp = gethost_byname(s, &lin);
			Debug((DEBUG_NOTICE, "co_sv: hp %x ac %x na %s ho %s",
				hp, aconf, aconf->name, s));
			if (!hp)
				return 0;
			bcopy(hp->h_addr, (char *)&aconf->ipnum,
				sizeof(struct IN_ADDR));
		    }
	    }
	cptr = make_client(NULL);
	if ((make_server(cptr))==NULL)
	{
		free_client(cptr);
		return -1;
	}
	cptr->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strncpyzt(cptr->serv->namebuf, aconf->name, sizeof(cptr->serv->namebuf));
	strncpyzt(cptr->sockhost, aconf->host, HOSTLEN+1);

#ifdef	UNIXPORT
	if (*aconf->host == '/') /* (/ starts a 2), Unix domain -- dl*/
		svp = connect_unix(aconf, cptr, &len);
	else
#endif
	svp = connect_inet(aconf, cptr, &len);

	if (!svp)
	    {
free_server:
		if (cptr->fd >= 0)
			(void)close(cptr->fd);
		cptr->fd = -2;
		/* make_server() sets ->bcptr, clear it now or free_server()
		** complains. --B. */
		cptr->serv->bcptr = NULL;
		free_server(cptr->serv);
		free_client(cptr);
		return -1;
	    }

	set_non_blocking(cptr->fd, cptr);
	(void)set_sock_opts(cptr->fd, cptr);
	(void)signal(SIGALRM, dummy);
	(void)alarm(4);
	if (connect(cptr->fd, (SAP)svp, len) < 0 && errno != EINPROGRESS)
	    {
		i = errno; /* other system calls may eat errno */
		(void)alarm(0);
		report_error("Connect to host %s failed: %s",cptr);
		if (by && IsPerson(by) && !MyClient(by))
		  sendto_one(by,
			     ":%s NOTICE %s :Connect to host %s failed.",
			     ME, by->name, cptr->name);
		errno = i;
		if (errno == EINTR)
			errno = ETIMEDOUT;
		goto free_server;
	    }
	(void)alarm(0);

	/* Attach config entries to client here rather than in
	 * completed_connection. This to avoid null pointer references
	 * when name returned by gethostbyaddr matches no C lines
	 * (could happen in 2.6.1a when host and servername differ).
	 * No need to check access and do gethostbyaddr calls.
	 * There must at least be one as we got here C line...  meLazy
	 */
	(void)attach_confs_host(cptr, aconf->host, CFLAG|NFLAG);

	if (!find_conf_host(cptr->confs, aconf->host, NFLAG) ||
	    !find_conf_host(cptr->confs, aconf->host, CFLAG))
	    {
      		sendto_flag(SCH_NOTICE,
			    "Host %s is not enabled for connecting:no C/N-line",
			    aconf->host);
		if (by && IsPerson(by) && !MyClient(by))
		  sendto_one(by,
			     ":%s NOTICE %s :Connect to host %s failed.",
			     ME, by->name, cptr->name);
		det_confs_butmask(cptr, 0);
		goto free_server;
	    }
	/*
	** The socket has been connected or connect is in progress.
	*/
	if (by && IsPerson(by))
	    {
		(void)strcpy(cptr->serv->by, by->name);
		if (by->uid[0])
		{
			strcpy(cptr->serv->byuid, by->uid);
		}
		cptr->serv->user = by->user;
		by->user->refcnt++;
	    }
	else
		(void)strcpy(cptr->serv->by, "AutoConn.");
	cptr->serv->up = &me;
	cptr->serv->maskedby = cptr;
	cptr->serv->nline = aconf;
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	add_fd(cptr->fd, &fdall);
	local[cptr->fd] = cptr;
	cptr->acpt = &me;
	SetConnecting(cptr);

	get_sockhost(cptr, aconf->host);
	add_client_to_list(cptr);
	nextping = timeofday;
	istat.is_unknown++;

	return 0;
}

static	struct	SOCKADDR *connect_inet(aConfItem *aconf, aClient *cptr,
	int *lenp)
{
	static	struct	SOCKADDR_IN	server;
	struct SOCKADDR_IN outip;
	
	Reg	struct	hostent	*hp;
	aClient	*acptr;
	int	i;

	/*
	 * Might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */
	cptr->fd = socket(AFINET, SOCK_STREAM, 0);
	if (cptr->fd >= MAXCLIENTS)
	    {
		sendto_flag(SCH_NOTICE,
			    "No more connections allowed (%s)", cptr->name);
		return NULL;
	    }
	bzero((char *)&server, sizeof(server));
	server.SIN_FAMILY = AFINET;
	get_sockhost(cptr, aconf->host);
	
	if (!BadPtr(aconf->source_ip))
	{
		memset(&outip, 0, sizeof(outip));
		outip.SIN_PORT = 0;
		outip.SIN_FAMILY = AFINET;
#ifdef INET6
		if (!inetpton(AF_INET6, aconf->source_ip, outip.sin6_addr.s6_addr))
#else
		if ((outip.sin_addr.s_addr = inetaddr(aconf->source_ip)) == -1)
#endif
		{
			sendto_flag(SCH_ERROR, "Invalid source IP (%s) in C:line",
				aconf->source_ip);
			memcpy(&outip, &mysk, sizeof(mysk));
		}
	}
	else
	{
		memcpy(&outip, &mysk, sizeof(mysk));		
	}
	if (cptr->fd == -1)
	    {
		report_error("opening stream socket to server %s:%s", cptr);
		return NULL;
	    }
	/*
	** Bind to a local IP# (with unknown port - let unix decide) so
	** we have some chance of knowing the IP# that gets used for a host
	** with more than one IP#.
	** With VIFs, M:line defines outgoing IP# and initialises mysk.
	*/
	if (bind(cptr->fd, (SAP)&outip, sizeof(outip)) == -1)
	    {
		report_error("error binding to local port for %s:%s", cptr);
		return NULL;
	    }
	/*
	 * By this point we should know the IP# of the host listed in the
	 * conf line, whether as a result of the hostname lookup or the ip#
	 * being present instead. If we don't know it, then the connect fails.
	 */
#ifdef INET6
	if (isdigit(*aconf->host) && (AND16(aconf->ipnum.s6_addr) == 255))
		if (!inetpton(AF_INET6, aconf->host,aconf->ipnum.s6_addr))
			bcopy(minus_one, aconf->ipnum.s6_addr, IN6ADDRSZ);
	if (AND16(aconf->ipnum.s6_addr) == 255)
#else
	if (isdigit(*aconf->host) && (aconf->ipnum.s_addr == -1))
		aconf->ipnum.s_addr = inetaddr(aconf->host);
	if (aconf->ipnum.s_addr == -1)
#endif
	    {
		hp = cptr->hostp;
		if (!hp)
		    {
			Debug((DEBUG_FATAL, "%s: unknown host", aconf->host));
			return NULL;
		    }
		bcopy(hp->h_addr, (char *)&aconf->ipnum,
		      sizeof(struct IN_ADDR));
 	    }
	bcopy((char *)&aconf->ipnum, (char *)&server.SIN_ADDR,
		sizeof(struct IN_ADDR));
	bcopy((char *)&aconf->ipnum, (char *)&cptr->ip,
		sizeof(struct IN_ADDR));
	cptr->port = (aconf->port > 0) ? aconf->port : portnum;
	server.SIN_PORT = htons(cptr->port);
	/*
	 * Look for a duplicate IP#,port pair among already open connections
	 * (This caters for unestablished connections).
	 */
	for (i = highest_fd; i >= 0; i--)
		if ((acptr = local[i]) &&
		    !bcmp((char *)&cptr->ip, (char *)&acptr->ip,
			  sizeof(cptr->ip)) && server.SIN_PORT == acptr->port)
			return NULL;
	*lenp = sizeof(server);
	return	(struct SOCKADDR *)&server;
}

#ifdef	UNIXPORT
/* connect_unix
 *
 * Build a socket structure for cptr so that it can connet to the unix
 * socket defined by the conf structure aconf.
 */
static	struct	SOCKADDR *connect_unix(aConfItem *aconf, aClient *cptr,
	int *lenp)
{
	static	struct	sockaddr_un	sock;

	if ((cptr->fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	    {
		report_error("Unix domain connect to host %s failed: %s", cptr);
		return NULL;
	    }
	else if (cptr->fd >= MAXCLIENTS)
	    {
		sendto_flag(SCH_NOTICE,
			    "No more connections allowed (%s)", cptr->name);
		return NULL;
	    }

	get_sockhost(cptr, aconf->host);
	strncpyzt(sock.sun_path, aconf->host + 2, sizeof(sock.sun_path));
	sock.sun_family = AF_UNIX;
	*lenp = strlen(sock.sun_path) + 2;

	SetUnixSock(cptr);
	return (struct sockaddr *)&sock;
}
#endif

/*
 * The following section of code performs summoning of users to irc.
 */
#if defined(ENABLE_SUMMON) || defined(USERS_SHOWS_UTMP) 
int	utmp_open(void)
{
#ifdef O_NOCTTY
	return (open(UTMP, O_RDONLY|O_NOCTTY));
#else
	return (open(UTMP, O_RDONLY));
#endif
}

int	utmp_read(int fd, char *name, char *line, char *host, int hlen)
{
	struct	utmp	ut;
	while (read(fd, (char *)&ut, sizeof (struct utmp))
	       == sizeof (struct utmp))
	    {
		strncpyzt(name, ut.ut_name, 9);
		strncpyzt(line, ut.ut_line, 10);
#ifdef USER_PROCESS
#  if defined(HPUX) || defined(AIX)
		strncpyzt(host,(ut.ut_host[0]) ? (ut.ut_host) : ME, 16);
#  else
		strncpyzt(host, ME, 9);
#  endif
		if (ut.ut_type == USER_PROCESS)
			return 0;
#else
		strncpyzt(host, (ut.ut_host[0]) ? (ut.ut_host) : ME,
			hlen);
		if (ut.ut_name[0])
			return 0;
#endif
	    }
	return -1;
}

int	utmp_close(int fd)
{
	return(close(fd));
}

#ifdef ENABLE_SUMMON
void	summon(aClient *who, char *namebuf, char *linebuf, char *chname)
{
	static	char	wrerr[] = "NOTICE %s :Write error. Couldn't summon.";
	int	fd;
	char	line[512];
	struct	tm	*tp;

	tp = localtime(&timeofday);
	if (strlen(linebuf) > (size_t) 9)
	    {
		sendto_one(who,"NOTICE %s :Serious fault in SUMMON.",
			   who->name);
		sendto_one(who,
			   "NOTICE %s :linebuf too long. Inform Administrator",
			   who->name);
		return;
	    }
	/*
	 * Following line added to prevent cracking to e.g. /dev/kmem if
	 * UTMP is for some silly reason writable to everyone...
	 */
	if ((linebuf[0] != 't' || linebuf[1] != 't' || linebuf[2] != 'y')
	    && (linebuf[0] != 'c' || linebuf[1] != 'o' || linebuf[2] != 'n')
	    && (linebuf[0] != 'p' || linebuf[1] != 't' || linebuf[2] != 's')
#ifdef HPUX
	    && (linebuf[0] != 'p' || linebuf[1] != 't' || linebuf[2] != 'y' ||
		linebuf[3] != '/')
#endif
	    )
	    {
		sendto_one(who,
		      "NOTICE %s :Looks like mere mortal souls are trying to",
			   who->name);
		sendto_one(who,"NOTICE %s :enter the twilight zone... ",
			   who->name);
		Debug((0, "%s (%s@%s, nick %s, %s)",
		      "FATAL: major security hack. Notify Administrator !",
		      who->username, who->user->host,
		      who->name, who->info));
		return;
	    }

	sprintf(line,"/dev/%s", linebuf);
	(void)alarm(5);
#ifdef	O_NOCTTY
	if ((fd = open(line, O_WRONLY | O_NDELAY | O_NOCTTY)) == -1)
#else
	if ((fd = open(line, O_WRONLY | O_NDELAY)) == -1)
#endif
	    {
		(void)alarm(0);
		sendto_one(who,
			   "NOTICE %s :%s seems to have disabled summoning...",
			   who->name, namebuf);
		return;
	    }
#if !defined(O_NOCTTY) && defined(TIOCNOTTY)
	(void)ioctl(fd, TIOCNOTTY, NULL);
#endif
	(void)alarm(0);
	(void)sprintf(line,"\n\r\007Message from IRC_Daemon@%s at %d:%02d\n\r",
			ME, tp->tm_hour, tp->tm_min);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		(void)alarm(0);
		(void)close(fd);
		sendto_one(who, wrerr, who->name);
		return;
	    }
	(void)alarm(0);
	(void)strcpy(line, "ircd: You are being summoned to Internet Relay \
Chat on\n\r");
	(void)alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		(void)alarm(0);
		(void)close(fd);
		sendto_one(who, wrerr, who->name);
		return;
	    }
	(void)alarm(0);
	sprintf(line, "ircd: Channel %s, by %s@%s (%s) %s\n\r", chname,
		who->user->username, who->user->host, who->name, who->info);
	(void)alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		(void)alarm(0);
		(void)close(fd);
		sendto_one(who, wrerr, who->name);
		return;
	    }
	(void)alarm(0);
	(void)strcpy(line,"ircd: Respond with irc\n\r");
	(void)alarm(5);
	if (write(fd, line, strlen(line)) != strlen(line))
	    {
		(void)alarm(0);
		(void)close(fd);
		sendto_one(who, wrerr, who->name);
		return;
	    }
	(void)close(fd);
	(void)alarm(0);
	sendto_one(who, replies[RPL_SUMMONING], ME, BadTo(who->name), namebuf);
	return;
}
#  endif
#endif /* ENABLE_SUMMON */

/*
** find the real hostname for the host running the server (or one which
** matches the server's name) and its primary IP#.  Hostname is stored
** in the client structure passed as a pointer.
*/
void	get_my_name(aClient *cptr, char *name, int len)
{
	static	char tmp[HOSTLEN+1];
	struct	hostent	*hp;
	char	*cname = cptr->name;
	aConfItem	*aconf;
#ifdef HAVE_GETIPNODEBYNAME
	int	error_num1, error_num2;
	struct	hostent	*hp1, *hp2;
#endif

	/*
	** Setup local socket structure to use for binding to.
	*/
	bzero((char *)&mysk, sizeof(mysk));
	mysk.SIN_FAMILY = AFINET;
	mysk.SIN_PORT = 0;
	
	if ((aconf = find_me())->passwd && isdigit(*aconf->passwd))
#ifdef INET6
		if(!inetpton(AF_INET6, aconf->passwd, mysk.sin6_addr.s6_addr))
			bcopy(minus_one, mysk.sin6_addr.s6_addr, IN6ADDRSZ);
#else
		mysk.sin_addr.s_addr = inetaddr(aconf->passwd);
#endif

	if (gethostname(name, len) == -1)
		return;
	name[len] = '\0';

	add_local_domain(name, len - strlen(name));

	/*
	** If hostname gives another name than cname, then check if there is
	** a CNAME record for cname pointing to hostname. If so accept
	** cname as our name.   meLazy
	*/
	if (BadPtr(cname))
		return;
#ifdef HAVE_GETIPNODEBYNAME
	hp1 = getipnodebyname(cname, AF_INET6, AI_DEFAULT, &error_num1);
	hp2 = getipnodebyname(name, AF_INET6, AI_DEFAULT, &error_num2);
	if (! error_num1) hp=hp1; else hp=hp2;
	if ((! error_num1) || (! error_num2)) 
#else
	if ((hp = gethostbyname(cname)) || (hp = gethostbyname(name)))
#endif
	    {
		char	*hname;
		int	i = 0;

		for (hname = hp->h_name; hname; hname = hp->h_aliases[i++])
  		    {
			strncpyzt(tmp, hname, sizeof(tmp));
			add_local_domain(tmp, sizeof(tmp) - strlen(tmp));

			/*
			** Copy the matching name over and store the
			** 'primary' IP# as 'myip' which is used
			** later for making the right one is used
			** for connecting to other hosts.
			*/
			if (!mycmp(ME, tmp))
				break;
 		    }
		if (mycmp(ME, tmp))
			strncpyzt(name, hp->h_name, len);
		else
			strncpyzt(name, tmp, len);
#if 0
/* If someone puts IP in M:, fine, use it as outgoing ip, but using
resolved M: name for outgoing ip is... troublesome. It can resolve to IP of
some other host or 127.0.0.1 (in which case we'd be getting strange errors
from connect()); if left empty, OS will decide itself what IP to use; 
if someone wants to control this, use M: or C: adequate fields. --B. */
		if (BadPtr(aconf->passwd))
			bcopy(hp->h_addr, (char *)&mysk.SIN_ADDR,
			      sizeof(struct IN_ADDR));
#endif
		Debug((DEBUG_DEBUG,"local name is %s",
				get_client_name(&me,TRUE)));
	    }
#ifdef HAVE_GETIPNODEBYNAME
	freehostent(hp1);
	freehostent(hp2);
#endif
	return;
}

/*
** setup a UDP socket and listen for incoming packets
*/
int	setup_ping(aConfItem *aconf)
{
	struct	SOCKADDR_IN	from;
	int	on = 1;

	if (udpfd != -1)
		return udpfd;
	bzero((char *)&from, sizeof(from));
	if (aconf->passwd && isdigit(*aconf->passwd))
#ifdef INET6
	    {
		if (!inetpton(AF_INET6, aconf->passwd,from.sin6_addr.s6_addr))
			bcopy(minus_one, from.sin6_addr.s6_addr, IN6ADDRSZ);
	    }
#else
	  from.sin_addr.s_addr = inetaddr(aconf->passwd);
#endif
	else
#ifdef INET6
	  from.SIN_ADDR = in6addr_any;
#else
	  from.sin_addr.s_addr = htonl(INADDR_ANY); /* hmmpf */
#endif
	from.SIN_PORT = htons((u_short) aconf->port);
	from.SIN_FAMILY = AFINET;

	if ((udpfd = socket(AFINET, SOCK_DGRAM, 0)) == -1)
	    {
		Debug((DEBUG_ERROR, "socket udp : %s", strerror(errno)));
		return -1;
	    }
	if (SETSOCKOPT(udpfd, SOL_SOCKET, SO_REUSEADDR, &on, on) == -1)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "setsockopt udp fd %d : %m", udpfd);
#endif
		Debug((DEBUG_ERROR, "setsockopt so_reuseaddr : %s",
			strerror(errno)));
		(void)close(udpfd);
		return udpfd = -1;
	    }
	on = 0;
	(void) SETSOCKOPT(udpfd, SOL_SOCKET, SO_BROADCAST, &on, on);
	if (bind(udpfd, (SAP)&from, sizeof(from))==-1)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "bind udp.%d fd %d : %m",
		       ntohs(from.SIN_PORT), udpfd);
#endif
		Debug((DEBUG_ERROR, "bind : %s", strerror(errno)));
		(void)close(udpfd);
		return udpfd = -1;
	    }
	if (fcntl(udpfd, F_SETFL, FNDELAY)==-1)
	    {
		Debug((DEBUG_ERROR, "fcntl fndelay : %s", strerror(errno)));
		(void)close(udpfd);
		return udpfd = -1;
	    }
	Debug((DEBUG_INFO, "udpfd = %d, port %d", udpfd,ntohs(from.SIN_PORT)));
	return udpfd;
}


void	send_ping(aConfItem *aconf)
{
	Ping	pi;
	struct	SOCKADDR_IN	sin;
	aCPing	*cp = aconf->ping;

#ifdef INET6
	if (!aconf->ipnum.s6_addr || AND16(aconf->ipnum.s6_addr) == 255 || !cp->port)
#else
	if (!aconf->ipnum.s_addr || aconf->ipnum.s_addr == -1 || !cp->port)
#endif
		return;
	if (aconf->class->conFreq == 0) /* avoid flooding */
		return;
	pi.pi_cp = aconf;
	pi.pi_id = htonl(PING_CPING);
	pi.pi_seq = cp->lseq++;
	cp->seq++;
	/*
	 * Only recognise stats from the last 20 minutes as significant...
	 * Try and fake sliding along a "window" here.
	 */
	if (cp->seq > 1 && cp->seq * aconf->class->conFreq > 1200)
	    {
		if (cp->recvd)
		    {
			cp->ping -= (cp->ping / cp->recvd);
			if (cp->recvd == cp->seq)
				cp->recvd--;
		    }
		else
			cp->ping = 0;
		cp->seq--;
	    }

	bzero((char *)&sin, sizeof(sin));
#ifdef INET6
	bcopy(aconf->ipnum.s6_addr, sin.sin6_addr.s6_addr, IN6ADDRSZ);
#else
	sin.sin_addr.s_addr = aconf->ipnum.s_addr;
#endif
	sin.SIN_PORT = htons(cp->port);
	sin.SIN_FAMILY = AFINET;
	(void)gettimeofday(&pi.pi_tv, NULL);
#ifdef INET6
	Debug((DEBUG_SEND,"Send ping to %s,%d fd %d, %d bytes",
	       inet_ntop(AF_INET6, (char *)&aconf->ipnum, ipv6string, sizeof(ipv6string)),
	       cp->port, udpfd, sizeof(pi)));
#else
	Debug((DEBUG_SEND,"Send ping to %s,%d fd %d, %d bytes",
	       inetntoa((char *)&aconf->ipnum),
	       cp->port, udpfd, sizeof(pi)));
#endif
	(void)sendto(udpfd, (char *)&pi, sizeof(pi), 0,(SAP)&sin,sizeof(sin));
}

static	int	check_ping(char *buf, int len)
{
	Ping	pi;
	aConfItem	*aconf;
	struct	timeval	tv;
	double	d;
	aCPing	*cp = NULL;
	u_long	rtt;

	(void)gettimeofday(&tv, NULL);

	if (len < sizeof(pi) + 8)
		return -1;

	bcopy(buf, (char *)&pi, sizeof(pi));	/* ensure nice byte align. */

	for (aconf = conf; aconf; aconf = aconf->next)
		if (pi.pi_cp == aconf && (cp = aconf->ping))
			break;
	if (!aconf || match(aconf->name, buf + sizeof(pi)))
		return -1;

	cp->recvd++;
	cp->lrecvd++;
	rtt = ((tv.tv_sec - pi.pi_tv.tv_sec) * 1000 +
		(tv.tv_usec - pi.pi_tv.tv_usec) / 1000);
	cp->ping += rtt;
	cp->rtt += rtt;
	if (cp->rtt > 1000000)
	    {
		cp->ping = (cp->rtt /= cp->lrecvd);
		cp->recvd = cp->lrecvd = 1;
		cp->seq = cp->lseq = 1;
	    }
	d = (double)cp->recvd / (double)cp->seq;
	d = pow(d, (double)20.0);
	d = (double)cp->ping / (double)cp->recvd / d;
	if (d > 10000.0)
		d = 10000.0;
	aconf->pref = (int) (d * 100.0);

	return 0;
}

/*
 * max # of pings set to 15/sec.
 */
static	void	polludp(void)
{
	static	time_t	last = 0;
	static	int	cnt = 0, mlen = 0, lasterr = 0;
	Reg	char	*s;
	struct	SOCKADDR_IN	from;
	Ping	pi;
	int	n;
	SOCK_LEN_TYPE fromlen = sizeof(from);

	/*
	 * find max length of data area of packet.
	 */
	if (!mlen)
	{
		mlen = sizeof(readbuf) - strlen(ME) - strlen(version);
		mlen -= 6;
		if (mlen < 0)
			mlen = 0;
	}
	Debug((DEBUG_DEBUG,"udp poll"));

	memset(&from, 0, fromlen);
	n = recvfrom(udpfd, readbuf, mlen, 0, (SAP)&from, &fromlen);
	if (n == -1)
	    {
		ircstp->is_udperr++;
		if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
			return;
		else
		    {
#if 0
/* seems to create more confusion than it's worth */
			char buf[100];

			sprintf(buf, "udp port recvfrom() from %s to %%s: %%s",
#ifdef INET6
				from.sin6_addr.s6_addr
#else
				from.sin_addr.s_addr
#endif
				== 0 ? "unknown" :
#ifdef INET6
				inetntop(AF_INET6, (char *)&from.sin6_addr,
					ipv6string, sizeof(ipv6string))
#else
				inetntoa((char *)&from.sin_addr)
#endif
				);
			report_error(buf, &me);
#endif /* confusion */
			return;
		    }
	    }

	if (timeofday == last)
	    {
		if (++cnt > 14)
		    {
			if (timeofday > lasterr + 30)
			    {
				sendto_flag(SCH_NOTICE,
				    "udp packet dropped: %d bytes from %s.%d",
#ifdef INET6
					    n, inetntop(AF_INET6,
					 (char *)&from.sin6_addr, ipv6string,
							 sizeof(ipv6string)),
#else
					    n,inetntoa((char *)&from.sin_addr),
#endif
					    ntohs(from.SIN_PORT));
				lasterr = timeofday;
			    }
			ircstp->is_udpdrop++;
			return;
		    }
	    }
	else
		cnt = 0, last = timeofday;

#ifdef INET6
	Debug((DEBUG_NOTICE, "udp (%d) %d bytes from %s,%d", cnt, n,
	       inet_ntop(AF_INET6, (char *)&from.sin6_addr, ipv6string,
			 sizeof(ipv6string)),
	       ntohs(from.SIN_PORT)));
#else
	Debug((DEBUG_NOTICE, "udp (%d) %d bytes from %s,%d", cnt, n,
	       inetntoa((char *)&from.sin_addr),
	       ntohs(from.SIN_PORT)));
#endif

	readbuf[n] = '\0';
	ircstp->is_udpok++;
	if (n  < 8)
		return;

	bcopy(s = readbuf, (char *)&pi, MIN(n, sizeof(pi)));
	pi.pi_id = ntohl(pi.pi_id);
	Debug((DEBUG_INFO, "\tpi_id %#x pi_seq %d pi_cp %#x",
		pi.pi_id, pi.pi_seq, pi.pi_cp));

	if ((pi.pi_id == (PING_CPING|PING_REPLY) ||
	     pi.pi_id == (PING_CPING|(PING_REPLY << 24))) && n >= sizeof(pi))
	    {
		check_ping(s, n);
		return;
	    }
	else if (pi.pi_id & PING_REPLY)
		return;
	/*
	 * attach my name and version for the reply
	 */
	pi.pi_id |= PING_REPLY;
	pi.pi_id = htonl(pi.pi_id);
	bcopy((char *)&pi, s, MIN(n, sizeof(pi)));
	s += n;
	(void)strcpy(s, ME);
	s += strlen(s)+1;
	(void) strcpy(s, version);
	s += strlen(s);
	(void)sendto(udpfd, readbuf, s-readbuf, 0, (SAP)&from ,sizeof(from));
	return;
}

/*
 * do_dns_async
 *
 * Called when the fd returned from init_resolver() has been selected for
 * reading.
 */
static	void	do_dns_async(void)
{
	static	Link	ln;
	aClient	*cptr;
	aConfItem	*aconf;
	struct	hostent	*hp;
	int	bytes, pkts;

	pkts = 0;

	do {
		ln.flags = -1;
		hp = get_res((char *)&ln);

		Debug((DEBUG_DNS,"%#x = get_res(%d,%#x)", hp, ln.flags,
			ln.value.cptr));

		switch (ln.flags)
		{
		case ASYNC_NONE :
			/*
			 * no reply was processed that was outstanding or
			 * had a client still waiting.
			 */
			break;
		case ASYNC_CLIENT :
			if ((cptr = ln.value.cptr))
			    {
				del_queries((char *)cptr);
				ClearDNS(cptr);
				cptr->hostp = hp;
#if defined(USE_IAUTH)
				if (hp)
				    {
					int i = 0;

					while (hp->h_aliases[i])
						sendto_iauth("%d A %s",
							     cptr->fd,
							hp->h_aliases[i++]);
					if (hp->h_name)
						sendto_iauth("%d N %s",
							cptr->fd, hp->h_name);
					else if (hp->h_aliases[0])
						sendto_iauth("%d n", cptr->fd);
				    }
				else
					sendto_iauth("%d d", cptr->fd);
				if (iauth_options & XOPT_EXTWAIT)
					cptr->lasttime = timeofday;
#endif
			    }
			break;
		case ASYNC_CONNECT :
			aconf = ln.value.aconf;
			if (hp && aconf)
			    {
				bcopy(hp->h_addr, (char *)&aconf->ipnum,
				      sizeof(struct IN_ADDR));
				(void)connect_server(aconf, NULL, hp);
			    }
			else
				sendto_flag(SCH_ERROR,
					    "Connect to %s failed: host lookup",
					   (aconf) ? aconf->host : "unknown");
			break;
		case ASYNC_CONF :
			aconf = ln.value.aconf;
			if (hp && aconf)
				bcopy(hp->h_addr, (char *)&aconf->ipnum,
				      sizeof(struct IN_ADDR));
			break;
		default :
			break;
		}
		pkts++;
		if (ioctl(resfd, FIONREAD, &bytes) == -1)
			bytes = 0;
	} while ((bytes > 0) && (pkts < 10));
}

#ifdef DELAY_CLOSE
/*
 * Delaying close(2) on too rapid connections reduces cpu
 * and bandwidth usage. Not mentioning disk space, if you
 * log such crap.
 *
 * Based on Ari `DLR' Heikkinen <aheikin@dlr.pspt.fi> irce0.9.1
 * by Piotr `Beeth' Kucharski <chopin@42.pl>
 *
 * Note: calling with fd == -2 closes all delayed fds.
 */

time_t	delay_close(int fd)
{
	struct	fdlog
	{
		struct	fdlog	*next;
		int	fd;
		time_t	time;
	};
	static	struct	fdlog	*first = NULL, *last = NULL;
	struct	fdlog	*next = first, *tmp;
	int	tmpdel = 0;

	if (fd == -2)
	{
		/* special case used in m_close() -- close all! */
		tmpdel = istat.is_delayclosewait;
	}
	else 
	/* Make sure we don't delay close() on too many fds. If we have
	 * kept more than half (possibly) available fds for clients
	 * waiting to be closed, release quarter oldest of them.
	 * >>1, >>2 are faster equivalents of /2, /4 --B. */
	if (istat.is_delayclosewait > (MAXCLIENTS-istat.is_localc) >> 1)
	{
		tmpdel = istat.is_delayclosewait >> 2;
	}

	while ((tmp = next))
	{
		if (tmp->time < timeofday || tmpdel-- > 0)
		{
			next = tmp->next;
			(void)send(fd, "ERROR :Too rapid connections "
				"from your host\r\n", 46, 0);
			close(tmp->fd);
			MyFree(tmp);
			istat.is_delayclosewait--;
			if (!next)
			{
				last = NULL;
			}
			first = next;
		}
		else
		{
			/* This linked list has entries chronologically
			 * sorted, so if tmp is not old enough, all next
			 * would also be not old enough to close. */
			break;
		}
	}

	if (fd >= 0)
	{
		/* set socket in nonblocking state (just in case) */
		set_non_blocking(fd, NULL);

		/* disallow further receives */
		shutdown(fd, SHUT_RD);

		/* create a new entry with fd and time of close */
		tmp = (struct fdlog *)MyMalloc(sizeof(*tmp));
		tmp->next = NULL;
		tmp->fd = fd;
		tmp->time = timeofday + DELAY_CLOSE;
		istat.is_delayclosewait++;
		istat.is_delayclose++;

		/* then add it to the list */
		if (last)
		{
			last->next = tmp;
			last = tmp;
		}
		else
		{
			first = last = tmp;
		}
	}

	return first ? first->time : 0;
}
#endif
