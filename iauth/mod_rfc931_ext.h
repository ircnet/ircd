/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_rfc931_ext.h
 *   Copyright (C) 1998 Christophe Kalt
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
    defined in iauth/mod_rfc931.c.
 */

/*  External definitions for global variables.
 */
#ifndef MOD_RFC931_C
extern aModule Module_rfc931;
#endif /* MOD_RFC931_C */

/*  External definitions for global functions.
 */
#ifndef MOD_RFC931_C
# define EXTERN extern
#else /* MOD_RFC931_C */
# define EXTERN
#endif /* MOD_RFC931_C */

/*
EXTERN int rfc931_start __P((u_int));
EXTERN int rfc931_work __P((u_int));
EXTERN int rfc931_timeout __P((u_int));
EXTERN void rfc931_clean __P((u_int));
*/

#undef EXTERN
