/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_user_ext.h
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
    defined in ircd/s_user.c.
 */

/*  External definitions for global functions.
 */
#ifndef S_USER_C
#define EXTERN extern
#else /* S_USER_C */
#define EXTERN
#endif /* S_USER_C */
EXTERN aClient *next_client (Reg aClient *next, Reg char *ch);
EXTERN int hunt_server (aClient *cptr, aClient *sptr, char *command,
			    int server, int parc, char *parv[]);
EXTERN int do_nick_name (char *nick, int server);
EXTERN int register_user (aClient *, aClient *, char *, char *);
EXTERN char *canonize (char *buffer);
EXTERN int m_nick (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_unick (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_private (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
EXTERN int m_notice (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
EXTERN int m_who (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_whois (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_user (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_post (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_quit (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_kill (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_away (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_ping (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_pong (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_oper (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_pass (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_userhost (aClient *cptr, aClient *sptr, int parc,
			   char *parv[]);
EXTERN int m_ison (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_umode (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN void send_umode (aClient *cptr, aClient *sptr, int old,
			    int sendmask, char *umode_buf);
EXTERN void send_umode_out (aClient *cptr, aClient *sptr, int old);
EXTERN int m_save (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int is_allowed(aClient *, long);
EXTERN char *oline_flags_to_string(long);
EXTERN long oline_flags_parse(char *);
EXTERN void send_away(aClient *, aClient *);
#undef EXTERN
