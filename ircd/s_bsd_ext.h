/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_bsd_ext.h
 *   Copyright (C) 1997 Alain Nissen
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

/*  This file contains external definitions for global variables and functions
    defined in ircd/s_bsd.c.
 */

/*  External definitions for global variables.
 */
#ifndef S_BSD_C
extern aClient *local[];
extern FdAry fdas, fdaa, fdall;
extern int highest_fd, readcalls, udpfd, resfd;
extern time_t timeofday;
#endif /* S_BSD_C */

/*  External definitions for global functions.
 */
#ifndef S_BSD_C
#define EXTERN extern
#else /* S_BSD_C */
#define EXTERN
#endif /* S_BSD_C */
EXTERN void add_local_domain __P((char *hname, int size));
EXTERN void report_error __P((char *text, aClient *cptr));
EXTERN int inetport __P((aClient *cptr, char *ip, char *ipmask, int port));
EXTERN int add_listener __P((aConfItem *aconf));
EXTERN void close_listeners();
EXTERN void init_sys();
EXTERN void write_pidfile();
EXTERN int check_client __P((Reg aClient *cptr));
EXTERN int check_server_init __P((aClient *cptr));
EXTERN int check_server __P((aClient *cptr, Reg struct hostent *hp,
			     Reg aConfItem *c_conf, Reg aConfItem *n_conf,
			     int estab));
EXTERN int hold_server __P((aClient *cptr));
EXTERN void close_connection __P((aClient *cptr));
EXTERN int get_sockerr __P((aClient *cptr));
EXTERN void set_non_blocking __P((int fd, aClient *cptr));
EXTERN aClient *add_connection __P((aClient *cptr, int fd));
EXTERN int read_message __P((time_t delay, FdAry *fdp));
EXTERN int connect_server __P((aConfItem *aconf, aClient *by,
			       struct hostent *hp));
EXTERN void get_my_name __P((aClient *cptr, char *name, int len));
EXTERN int setup_ping __P((aConfItem *aconf));
EXTERN void send_ping __P((aConfItem *aconf));
#if defined(ENABLE_SUMMON) || defined(ENABLE_USERS)
EXTERN int utmp_open()
EXTERN int utmp_read __P((int fd, char *name, char *line, char *host,
			  int hlen));
EXTERN int utmp_close(int fd);
#ifdef  ENABLE_SUMMON
EXTERN void summon __P((aClient *who, char *namebuf, char *linebuf,
			char *chname));
#endif /* ENABLE_SUMMON */
#endif /* ENABLE_SUMMON || ENABLE_USERS */
#ifdef	UNIXPORT
EXTERN int unixport __P((aClient *cptr, char *path, int port));
#endif
#undef EXTERN
