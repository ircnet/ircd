/************************************************************************
 *   IRC - Internet Relay Chat, irc/irc_ext.h
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
    defined in irc/irc.c.
 */

/*  External definitions for global variables.
 */
#ifndef IRC_C
extern struct Command commands[];
extern char irc_id[];
extern aChannel *channel;
extern aClient me, *client;
extern anUser meUser;
extern FILE *logfile;
extern char buf[];
extern char *querychannel;
extern int portnum, termtype;
extern int debuglevel;
extern int unkill_flag, cchannel;
extern int QuitFlag;
#if defined(HPUX) || defined(SVR3) || defined(SVR4)
extern char logbuf[]; 
#endif
#ifdef MAIL50
extern int ucontext, perslength;
extern char persname[];
extern struct itmlst null_list[];
#endif
#endif /* IRC_C */

/*  External definitions for global functions.
 */
#ifndef IRC_C
#define EXTERN extern
#else /* IRC_C */
#define EXTERN
#endif /* IRC_C */
EXTERN void intr();
EXTERN void myloop __P((int sock));
EXTERN void do_cmdch __P((char *ptr, char *temp));
EXTERN void do_quote __P((char *ptr, char *temp));
EXTERN void do_query __P((char *ptr, char *temp));
EXTERN void do_mypriv __P((char *buf1, char *buf2));
EXTERN void do_myqpriv __P((char *buf1, char *buf2));
EXTERN void do_mytext __P((char *buf1, char *temp));
EXTERN void do_unkill __P((char *buf, char *temp));
EXTERN void do_bye __P((char *buf, char *tmp));
EXTERN void do_kill __P((char *buf1, char *tmp));
EXTERN void do_kick __P((char *buf1, char *tmp));
EXTERN void do_away __P((char *buf1, char *tmp));
EXTERN void do_server __P((char *buf, char *tmp));
EXTERN void sendit __P((char *line));
EXTERN char *mycncmp __P((char *str1, char *str2));
EXTERN void do_clear __P((char *buf, char *temp));
EXTERN void putline __P((char *line));
EXTERN int unixuser();
EXTERN void do_log __P((char *ptr, char *temp));
EXTERN char *last_to_me __P((char *sender));
EXTERN char *last_from_me __P((char *recipient));
#ifdef GETPASS
EXTERN do_oper __P((char *ptr, char *xtra));
#endif
EXTERN void do_channel __P((char *ptr, char *xtra));
EXTERN void write_statusline();
EXTERN RETSIGTYPE quit_intr __P((int s));
#undef EXTERN
