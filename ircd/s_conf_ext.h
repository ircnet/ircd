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
#ifdef TKLINE
extern aConfItem *tkconf;
#endif
extern char *networkname;
#endif /* S_CONF_C */

/*  External definitions for global functions.
 */
#ifndef S_CONF_C
#define EXTERN extern
#else /* S_CONF_C */
#define EXTERN
#endif /* S_CONF_C */
EXTERN void det_confs_butmask (aClient *cptr, int mask);
EXTERN int match_ipmask (char *mask, aClient *cptr,
	int maskwithusername);
EXTERN int attach_Iline (aClient *cptr, Reg struct hostent *hp,
			     char *sockhost);
EXTERN aConfItem *count_cnlines (Reg Link *lp);
EXTERN int detach_conf (aClient *cptr, aConfItem *aconf);
EXTERN int attach_conf (aClient *cptr, aConfItem *aconf);
EXTERN aConfItem *find_admin(void);
EXTERN aConfItem *find_me(void);
EXTERN aConfItem *attach_confs (aClient *cptr, char *name, int statmask);
EXTERN aConfItem *attach_confs_host (aClient *cptr, char *host,
					 int statmask);
EXTERN aConfItem *find_conf_exact (char *name, char *user, char *host,
				       int statmask);
EXTERN aConfItem *find_Oline (char *name, aClient *cptr);
EXTERN aConfItem *find_conf_name (char *name, int statmask);
EXTERN aConfItem *find_conf (Link *lp, char *name, int statmask);
EXTERN aConfItem *find_conf_host (Reg Link *lp, char *host,
				      Reg int statmask);
EXTERN aConfItem *find_conf_host_sid (Reg Link *lp, char *host, char *sid,
				      Reg int statmask);
EXTERN aConfItem *find_conf_ip (Link *lp, char *ip, char *user,
				    int statmask);
EXTERN aConfItem *find_conf_entry (aConfItem *aconf, u_int mask);
EXTERN int rehash (aClient *cptr, aClient *sptr, int sig);
EXTERN int openconf(void);
EXTERN int initconf (int opt);
EXTERN int find_kill (aClient *cptr, int timedklines, char **comment);
EXTERN int find_two_masks (char *name, char *host, int stat);
EXTERN int find_conf_flags (char *name, char *key, int stat);
EXTERN int find_restrict (aClient *cptr);
EXTERN void find_bounce (aClient *cptr, int class, int fd);
EXTERN aConfItem *find_denied (char *name, int class);
EXTERN char *iline_flags_to_string(long flags);
EXTERN long iline_flags_parse(char *string);
EXTERN char *pline_flags_to_string(long flags);
EXTERN long pline_flags_parse(char *string);
# ifdef	INET6
EXTERN char *ipv6_convert (char *orig);
# endif
#ifdef TKLINE
EXTERN int m_tkline(aClient *, aClient *, int, char **);
EXTERN int m_untkline(aClient *, aClient *, int, char **);
EXTERN time_t tkline_expire(int);
#endif
#ifdef KLINE
EXTERN int m_kline(aClient *, aClient *, int, char **);
#endif
#undef EXTERN
