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
EXTERN aClient *next_client __P((Reg aClient *next, Reg char *ch));
EXTERN int hunt_server __P((aClient *cptr, aClient *sptr, char *command,
			    int server, int parc, char *parv[]));
EXTERN int do_nick_name __P((char *nick, int server));
EXTERN int ereject_user __P((aClient *, char *, char *));
EXTERN int register_user __P((aClient *, aClient *, char *, char *));
EXTERN char *canonize __P((char *buffer));
EXTERN int m_nick __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_private __P((aClient *cptr, aClient *sptr, int parc,
			  char *parv[]));
EXTERN int m_notice __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN int m_who __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_whois __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_user __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_quit __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_kill __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_away __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_ping __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_pong __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_oper __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_pass __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_userhost __P((aClient *cptr, aClient *sptr, int parc,
			   char *parv[]));
EXTERN int m_ison __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_umode __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN void send_umode __P((aClient *cptr, aClient *sptr, int old,
			    int sendmask, char *umode_buf));
EXTERN void send_umode_out __P((aClient *cptr, aClient *sptr, int old));
#undef EXTERN
