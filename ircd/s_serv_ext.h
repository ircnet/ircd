/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_serv_ext.h
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
    defined in ircd/s_serv.c.
 */

/*  External definitions for global functions.
 */
#ifndef S_SERV_C
#define EXTERN extern
#else /* S_SERV_C */
#define EXTERN
#endif /* S_SERV_C */
EXTERN int m_version __P((aClient *cptr, aClient *sptr, int parc,
			  char *parv[]));
EXTERN int m_squit __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int check_version __P((aClient *cptr));
EXTERN int m_server __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN int m_server_estab __P((Reg aClient *cptr));
EXTERN int m_reconnect __P((aClient *cptr, aClient *sptr, int parc,
			    char *parv[]));
EXTERN int m_info __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_links __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_summon __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN int m_stats __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_users __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_error __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_help __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_lusers __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN int m_connect __P((aClient *cptr, aClient *sptr, int parc,
			  char *parv[]));
EXTERN int m_wallops __P((aClient *cptr, aClient *sptr, int parc,
			  char *parv[]));
EXTERN int m_time __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_admin __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_trace __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_motd __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_close __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN char *find_server_string __P((int snum));
EXTERN int find_server_num __P((char *sname));
#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
EXTERN int m_rehash __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
#endif /* OPER_REHASH || LOCOP_REHASH */
#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
EXTERN int m_restart __P((aClient *cptr, aClient *sptr, int parc,
			  char *parv[]));
#endif /* OPER_RESTART || LOCOP_RESTART */
#if defined(OPER_DIE) || defined(LOCOP_DIE)
EXTERN int m_die __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
#endif /* OPER_DIE || LOCOP_DIE */
#undef EXTERN
