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
EXTERN void intr(void);
EXTERN void myloop (int sock);
EXTERN void do_cmdch (char *ptr, char *temp);
EXTERN void do_quote (char *ptr, char *temp);
EXTERN void do_query (char *ptr, char *temp);
EXTERN void do_mypriv (char *buf1, char *buf2);
EXTERN void do_myqpriv (char *buf1, char *buf2);
EXTERN void do_mytext (char *buf1, char *temp);
EXTERN void do_unkill (char *buf, char *temp);
EXTERN void do_bye (char *buf, char *tmp);
EXTERN void do_kill (char *buf1, char *tmp);
EXTERN void do_kick (char *buf1, char *tmp);
EXTERN void do_away (char *buf1, char *tmp);
EXTERN void do_server (char *buf, char *tmp);
EXTERN void sendit (char *line);
EXTERN char *mycncmp (char *str1, char *str2);
EXTERN void do_clear (char *buf, char *temp);
EXTERN void putline (char *line);
EXTERN int unixuser(void);
EXTERN void do_log (char *ptr, char *temp);
EXTERN char *last_to_me (char *sender);
EXTERN char *last_from_me (char *recipient);
#ifdef GETPASS
EXTERN do_oper (char *ptr, char *xtra);
#endif
EXTERN void do_channel (char *ptr, char *xtra);
EXTERN void write_statusline(void);
EXTERN RETSIGTYPE quit_intr (int s);
#undef EXTERN
