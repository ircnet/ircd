/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_bsd.c
 *   Copyright (C) 1990, Jarkko Oikarinen and
 *                       University of Oulu, Computing Center
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
static  char rcsid[] = "@(#)$Id: c_bsd.c,v 1.7 2003/10/18 15:31:27 q Exp $";
#endif

#include "os.h"
#include "c_defines.h"
#define C_BSD_C
#include "c_externs.h"
#undef C_BSD_C

#ifdef AUTOMATON
#ifdef DOCURSES
#undef DOCURSES
#endif
#ifdef DOTERMCAP
#undef DOTERMCAP
#endif
#endif /* AUTOMATON */

#define	STDINBUFSIZE (0x80)

int	client_init(char *host, int portnum, aClient *cptr)
{
	int	sock;
	static	struct	hostent *hp;
	static	struct	SOCKADDR_IN server;
#ifdef HAVE_GETIPNODEBYNAME
	int	error_num;
#endif

	sock = socket(AFINET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	server.SIN_FAMILY = AFINET;
 
	if (isdigit(*host))
#ifdef INET6
	    {
		if(!inetpton(AF_INET6, host, server.sin6_addr.s6_addr))
			bcopy(minus_one, server.sin6_addr.s6_addr, IN6ADDRSZ);
	    }
#else
		server.sin_addr.s_addr = inetaddr(host);
#endif
	else { 
#ifdef INET6
# ifndef HAVE_GETIPNODEBYNAME
		res_init();
		_res.options|=RES_USE_INET6;
# endif
#endif
#ifndef HAVE_GETIPNODEBYNAME
		hp = gethostbyname(host);
		if (hp == 0) {
			fprintf(stderr, "%s: unknown host\n", host);
			exit(2);
		}
#else
		hp = getipnodebyname(host, AF_INET6, AI_DEFAULT, &error_num);
		if (error_num) {
			fprintf(stderr, "%s: unknown host\n", host);
			exit(2);
		}
#endif
		bcopy(hp->h_addr, (char *)&server.SIN_ADDR, hp->h_length);
	}
	server.SIN_PORT = htons(portnum);
	if (connect(sock, (SAP)&server, sizeof(server)) == -1) {
		perror("irc");
	 	exit(1);
	}

	cptr->acpt = cptr;
	cptr->port = server.SIN_PORT;
#ifdef INET6
	bcopy(server.sin6_addr.s6_addr, cptr->ip.s6_addr, IN6ADDRSZ);
#else
	cptr->ip.s_addr = server.sin_addr.s_addr;
#endif
#ifdef	HAVE_GETIPNODEBYNAME
	freehostent(hp);
#endif
	return(sock);
}

void	client_loop(int sock)
{
	int	i = 0, size, pos;
	char	apubuf[STDINBUFSIZE+1];
	fd_set	ready;

	do {
		if (sock < 0 || QuitFlag)
			return;
		FD_ZERO(&ready);
		FD_SET(sock, &ready);
		FD_SET(0, &ready);
#ifdef DOCURSES
		if (termtype == CURSES_TERM)
			move(LINES-1,i); refresh();
#endif
#ifdef DOTERMCAP
		if (termtype == TERMCAP_TERM)
			tcap_move (-1, i);
#endif
		if (select(32, (SELECT_FDSET_TYPE *)&ready, 0, 0, NULL) < 0) {
/*      perror("select"); */
			continue;
		}
		if (FD_ISSET(sock, &ready)) {
			if ((size = read(sock, apubuf, STDINBUFSIZE)) < 0)
				perror("receiving stream packet");
			if (size <= 0) {
				close(sock);
				return;
			}
			dopacket(&me, apubuf, size);
		}
#ifndef AUTOMATON
		if (FD_ISSET(0, &ready)) {
			if ((size = read(0, apubuf, STDINBUFSIZE)) < 0) {
				putline("FATAL ERROR: End of stdin file !");
				return;
			}
			for (pos = 0; pos < size; pos++) {
				i=do_char(apubuf[pos]);
#ifdef DOCURSES
				if (termtype == CURSES_TERM)
					move(LINES-1, i);
#endif
#ifdef DOTERMCAP
				if (termtype == CURSES_TERM)
					tcap_move(-1, i);
#endif
			}
		}
#endif /* AUTOMATON */
	} while (1);
}

