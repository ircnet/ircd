/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc_ext.h
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
    defined in ircd/s_misc.c.
 */

/*  External definitions for global variables.
 */
#ifndef S_MISC_C
extern struct stats ircst, *ircstp;
#ifdef  CACHED_MOTD
extern aMotd *motd;
extern struct tm motd_tm;
#endif /* CACHED_MOTD */
#endif /* S_MISC_C */

/*  External definitions for global functions.
 */
#ifndef S_MISC_C
#define EXTERN extern
#else /* S_MISC_C */
#define EXTERN
#endif /* S_MISC_C */
EXTERN char *date __P((time_t clock));
EXTERN int check_registered_user __P((aClient *sptr));
EXTERN int check_registered __P((aClient *sptr));
EXTERN int check_registered_service __P((aClient *sptr));
EXTERN char *get_client_name __P((aClient *sptr, int showip));
EXTERN char *get_client_host __P((aClient *cptr));
EXTERN void get_sockhost __P((Reg aClient *cptr, Reg char *host));
EXTERN char *my_name_for_link __P((char *name, Reg int count));
EXTERN int mark_blind_servers __P((aClient *cptr, char *name));
EXTERN int exit_client __P((aClient *cptr, aClient *sptr, aClient *from,
			    char *comment));
EXTERN void checklist();
EXTERN void initstats();
EXTERN void tstats __P((aClient *cptr, char *name));
#ifdef  CACHED_MOTD
EXTERN void read_motd __P((char *filename));
#endif /* CACHED_MOTD */
#undef EXTERN
