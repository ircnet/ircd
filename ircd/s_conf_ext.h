/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf_ext.h
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
    defined in ircd/s_conf.c.
 */

/*  External definitions for global variables.
 */
#ifndef S_CONF_C
extern aConfItem *conf, *kconf;
#endif /* S_CONF_C */

/*  External definitions for global functions.
 */
#ifndef S_CONF_C
#define EXTERN extern
#else /* S_CONF_C */
#define EXTERN
#endif /* S_CONF_C */
EXTERN void det_confs_butmask __P((aClient *cptr, int mask));
EXTERN int attach_Iline __P((aClient *cptr, Reg struct hostent *hp,
			     char *sockhost));
EXTERN aConfItem *count_cnlines __P((Reg Link *lp));
EXTERN int detach_conf __P((aClient *cptr, aConfItem *aconf));
EXTERN int attach_conf __P((aClient *cptr, aConfItem *aconf));
EXTERN aConfItem *find_admin();
EXTERN aConfItem *find_me();
EXTERN aConfItem *attach_confs __P((aClient *cptr, char *name, int statmask));
EXTERN aConfItem *attach_confs_host __P((aClient *cptr, char *host,
					 int statmask));
EXTERN aConfItem *find_conf_exact __P((char *name, char *user, char *host,
				       int statmask));
EXTERN aConfItem *find_Oline __P((char *name, aClient *cptr));
EXTERN aConfItem *find_conf_name __P((char *name, int statmask));
EXTERN aConfItem *find_conf __P((Link *lp, char *name, int statmask));
EXTERN aConfItem *find_conf_host __P((Reg Link *lp, char *host,
				      Reg int statmask));
EXTERN aConfItem *find_conf_ip __P((Link *lp, char *ip, char *user,
				    int statmask));
EXTERN aConfItem *find_conf_entry __P((aConfItem *aconf, u_int mask));
EXTERN int rehash __P((aClient *cptr, aClient *sptr, int sig));
EXTERN int openconf();
EXTERN int initconf __P((int opt));
EXTERN int find_kill __P((aClient *cptr, int doall, char **comment));
EXTERN int find_two_masks __P((char *name, char *host, int stat));
EXTERN int find_conf_flags __P((char *name, char *key, int stat));
EXTERN int find_restrict __P((aClient *cptr));
EXTERN void find_bounce __P((aClient *cptr, int class, int fd));
EXTERN aConfItem *find_denied __P((char *name, int class));
# ifdef	INET6
EXTERN char *ipv6_convert __P((char *orig));
# endif
#undef EXTERN
