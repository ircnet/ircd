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
EXTERN int m_version (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
EXTERN int m_squit (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int check_version (aClient *cptr);
EXTERN int m_smask (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_server (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
EXTERN int m_server_estab (aClient *cptr, char *sid, char *versionbuf);
EXTERN int m_info (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_links (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_summon (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
EXTERN int m_stats (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_users (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_error (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_help (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_lusers (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
EXTERN int m_connect (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
EXTERN int m_wallops (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
EXTERN int m_time (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_admin (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_trace (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_motd (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_close (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_eob (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_eoback (aClient *, aClient *, int, char **);
EXTERN int m_map (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN char *find_server_string (int snum);
EXTERN int find_server_num (char *sname);
#if defined(OPER_REHASH) || defined(LOCOP_REHASH)
EXTERN int m_rehash (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
#endif /* OPER_REHASH || LOCOP_REHASH */
#if defined(OPER_RESTART) || defined(LOCOP_RESTART)
EXTERN int m_restart (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
#endif /* OPER_RESTART || LOCOP_RESTART */
#if defined(OPER_DIE) || defined(LOCOP_DIE)
EXTERN int m_die (aClient *cptr, aClient *sptr, int parc, char *parv[]);
#endif /* OPER_DIE || LOCOP_DIE */
#if defined(OPER_SET) || defined(LOCOP_SET)
EXTERN int m_set(aClient *cptr, aClient *sptr, int parc, char *parv[]);
#endif /* OPER_SET || LOCOP_SET */

void	add_server_to_tree(aClient *cptr);
void	remove_server_from_tree(aClient *cptr);
void	do_emulated_eob(aClient *sptr);
int	check_servername(char *);
#undef EXTERN
