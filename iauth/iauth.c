/************************************************************************
 *   IRC - Internet Relay Chat, iauth/iauthd.c
 *   Copyright (C) 1998 Christophe Kalt
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
static  char rcsid[] = "@(#)$Id: iauth.c,v 1.6 1998/12/13 00:34:18 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define IAUTH_C
#include "a_externs.h"
#undef IAUTH_C

static int do_log = 0;

RETSIGTYPE dummy(s)
int s;
{
	/* from common/bsd.c */
#ifndef HAVE_RELIABLE_SIGNALS
	(void)signal(SIGALRM, dummy);
	(void)signal(SIGPIPE, dummy);
# ifndef HPUX	/* Only 9k/800 series require this, but don't know how to.. */
#  ifdef SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# endif
#else
# if POSIX_SIGNALS
	struct  sigaction       act;

	act.sa_handler = dummy;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGALRM);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
#  ifdef SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
#  endif
	(void)sigaction(SIGALRM, &act, (struct sigaction *)NULL);
	(void)sigaction(SIGPIPE, &act, (struct sigaction *)NULL);
#  ifdef SIGWINCH
	(void)sigaction(SIGWINCH, &act, (struct sigaction *)NULL);
#  endif
# endif
#endif
}

RETSIGTYPE s_log(s)
int s;
{
# if POSIX_SIGNALS
        struct  sigaction act;

        act.sa_handler = s_log;
        act.sa_flags = 0;
        (void)sigemptyset(&act.sa_mask);
        (void)sigaddset(&act.sa_mask, SIGUSR2);
        (void)sigaction(SIGUSR2, &act, NULL);
# else  
        (void)signal(SIGUSR2, s_log);
# endif
        do_log = 1;
}

void
init_signals()
{
	/* from ircd/ircd.c setup_signals() */
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
/*
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
*/
	act.sa_handler = s_log;
	(void)sigaddset(&act.sa_mask, SIGUSR2);
	(void)sigaction(SIGUSR2, &act, NULL);
#else
# ifndef	HAVE_RELIABLE_SIGNALS
	(void)signal(SIGPIPE, dummy);
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# else
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, SIG_IGN);
#  endif
	(void)signal(SIGPIPE, SIG_IGN);
# endif
	(void)signal(SIGALRM, dummy);   
/*
	(void)signal(SIGHUP, s_rehash);
	(void)signal(SIGTERM, s_die); 
	(void)signal(SIGINT, s_restart);
*/
	(void)signal(SIGUSR2, s_log);
#endif
}

int	main(argc, argv)
int	argc;
char	*argv[];
{
	time_t	delay = 0;

	/*setup_signals();*/

	if (argc == 2 && !strcmp(argv[1], "-X"))
		exit(0);

	if (isatty(0))
	    {
		(void)printf("iauth %s\n", make_version());
		if (argc == 3 && !strcmp(argv[1], "-c"))
		    {
			(void)printf("\nReading \"%s\"\n\n", argv[2]);
			conf_read(argv[2]);
		    }
		else
		    {
#if defined(INET6)
			(void)printf("\t+INET6\n");
#endif
#if defined(IAUTH_DEBUG)
			(void)printf("\t+IAUTH_DEBUG\n");
#endif
#if defined(USE_POLL)
			(void)printf("\t+USE_POLL\n");
#endif
		    }
		exit(0);
	    }

	init_signals();
	init_syslog();
	init_filelogs();
	sendto_log(ALOG_DMISC, LOG_NOTICE, "Daemon starting (%s%s).",
		   make_version(),
#if defined(IAUTH_DEBUG)
		   "+debug"
#else
		   ""
#endif
		   );
	init_io();
	conf_read(NULL);
	conf_ircd();

#if defined(IAUTH_DEBUG)
	if (debuglevel & ALOG_DIRCD)
		sendto_ircd("G 1");
	else
#endif
		sendto_ircd("G 0");

	while (1)
	    {
		loop_io();

		if (do_log)
		    {
			sendto_log(ALOG_IRCD|ALOG_DMISC, LOG_INFO,
				   "Got SIGUSR2, reinitializing log file(s).");
			init_filelogs();
			do_log = 0;
		    }
	    }
}
