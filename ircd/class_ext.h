/************************************************************************
 *   IRC - Internet Relay Chat, ircd/class_ext.h
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
    defined in ircd/class.c.
 */

/*  External definitions for global variables.
 */
#ifndef CLASS_C
extern aClass *classes;
#endif /* CLASS_C */

/*  External definitions for global functions.
 */
#ifndef CLASS_C
#define EXTERN extern
#else /* CLASS_C */
#define EXTERN
#endif /* CLASS_C */
EXTERN int get_conf_class __P((aConfItem *aconf));
EXTERN int get_client_class __P((aClient *acptr));
EXTERN int get_client_ping __P((aClient *acptr));
EXTERN int get_con_freq __P((aClass *clptr));
EXTERN void add_class __P((int class, int ping, int confreq, int maxli,
			   long sendq, int hlocal, int uhlocal,
			   int hglobal, int uhglobal));
EXTERN aClass *find_class __P((int cclass));
EXTERN void check_class();
EXTERN void initclass();
EXTERN void report_classes __P((aClient *sptr, char *to));
EXTERN long get_sendq __P((aClient *cptr));
#undef EXTERN
