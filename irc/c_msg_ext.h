/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_msg_ext.h
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
    defined in irc/c_msg.c.
 */

/*  External definitions for global variables.
 */
#ifndef C_MSG_C
#ifndef lint
extern char c_msg_id[];
#endif
extern char mybuf[];
#endif /* C_MSG_C */

/*  External definitions for global functions.
 */
#ifndef C_MSG_C
#define EXTERN extern
#else /* C_MSG_C */
#define EXTERN
#endif /* C_MSG_C */
EXTERN void m_die(void);
EXTERN int m_mode (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_wall (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_wallops (aClient *sptr, aClient *cptr, int parc,
			  char *parv[]);
EXTERN int m_ping (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_pong (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_nick (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN void m_away (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_server (aClient *sptr, aClient *cptr, int parc,
			 char *parv[]);
EXTERN int m_topic (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_join (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_part (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN void m_version (aClient *sptr, aClient *cptr, int parc,
			   char *parv[]);
EXTERN void m_bye(void);
EXTERN int m_quit (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_kill (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN void m_info (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN void m_squit (aClient *sptr, aClient *cptr, int parc,
			 char *parv[]);
EXTERN void m_newwhoreply (char *channel, char *username, char *host,
			       char *nickname, char *away, char *realname);
EXTERN void m_newnamreply (aClient *sptr, aClient *cptr, int parc,
			       char *parv[]);
EXTERN void m_linreply (aClient *sptr, aClient *cptr, int parc,
			    char *parv[]);
EXTERN int m_private (aClient *sptr, aClient *cptr, int parc,
			  char *parv[]);
EXTERN int m_kick (aClient *sptr, aClient *cptr, int parc, char *parv[]);
EXTERN int m_notice (aClient *sptr, aClient *cptr, int parc,
			 char *parv[]);
EXTERN int m_invite (aClient *sptr, aClient *cptr, int parc,
			 char *parv[]);
EXTERN int m_error (aClient *sptr, aClient *cptr, int parc, char *parv[]);
#undef EXTERN
