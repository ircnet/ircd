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

char c_bsd_id[] = "c_bsd.c v2.0 (c) 1988 University of Oulu, Computing Center\
 and Jarkko Oikarinen";

#include "struct.h"
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#else
# include "res/inet.h"
#endif
#ifndef AUTOMATON
#include <curses.h>
#endif
#include "common.h"
#include "sys.h"
#include "sock.h"	/* If FD_ZERO isn't defined up to this point, */
			/* define it (BSD4.2 needs this) */
#include "h.h"
#include "irc.h"

#ifdef AUTOMATON
#ifdef DOCURSES
#undef DOCURSES
#endif
#ifdef DOTERMCAP
#undef DOTERMCAP
#endif
#endif /* AUTOMATON */

#define	STDINBUFSIZE (0x80)

extern	aClient	me;
extern	int	termtype;
extern	int	QuitFlag;

int	client_init(host, portnum, cptr)
char	*host;
int	portnum;
aClient	*cptr;
{
	int	sock;
	static	struct	hostent *hp;
	static	struct	sockaddr_in server;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(1);
	}
	server.sin_family = AF_INET;
 
	if (isdigit(*host))
		server.sin_addr.s_addr = inetaddr(host);
	else { 
		hp = gethostbyname(host);
		if (hp == 0) {
			fprintf(stderr, "%s: unknown host\n", host);
			exit(2);
		}
		bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	}
	server.sin_port = htons(portnum);
	if (connect(sock, (SAP)&server, sizeof(server)) == -1) {
		perror("irc");
	 	exit(1);
	}

	cptr->acpt = cptr;
	cptr->port = server.sin_port;
	cptr->ip.s_addr = server.sin_addr.s_addr;
	return(sock);
}

void client_loop(sock)
int	sock;
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
			move (-1, i);
#endif
#ifdef HPUX
		if (select(32, (int *)&ready, 0, 0, NULL) < 0) {
#else
		if (select(32, &ready, 0, 0, NULL) < 0) {
#endif
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
					move(-1, i);
#endif
			}
		}
#endif /* AUTOMATON */
	} while (1);
}
