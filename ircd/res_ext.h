/************************************************************************
 *   IRC - Internet Relay Chat, ircd/res_ext.h
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
    defined in ircd/res.c.
 */

/*  External definitions for global functions.
 */
#ifndef RES_C
#define EXTERN extern
#else /* RES_C */
#define EXTERN
#endif /* RES_C */
extern int init_resolver __P((int op));
EXTERN time_t timeout_query_list __P((time_t now));
EXTERN void del_queries __P((char *cp));
EXTERN struct hostent *gethost_byname __P((char *name, Link *lp));
EXTERN struct hostent *gethost_byname_type __P((char *name, Link *lp, 
						int type));
EXTERN struct hostent *gethost_byaddr __P((char *addr, Link *lp));
EXTERN struct hostent *get_res __P((char *lp));
EXTERN time_t expire_cache __P((time_t now));
EXTERN void flush_cache();
EXTERN int m_dns __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN u_long cres_mem __P((aClient *sptr, char *nick));
#undef EXTERN
