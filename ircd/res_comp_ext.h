/************************************************************************
 *   IRC - Internet Relay Chat, ircd/res_comp_ext.h
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
    defined in ircd/res_comp.c.
 */

/*  External definitions for global functions.
 */
#ifndef RES_COMP_C
#define EXTERN extern
#else /* RES_COMP_C */
#define EXTERN
#endif /* RES_COMP_C */
EXTERN int ircd_dn_expand __P((const u_char *msg, const u_char *eomorig,
			       const u_char *comp_dn, char *exp_dn,
			       int length));
EXTERN int ircd_dn_comp __P((const char *exp_dn, u_char *comp_dn, int length,
			     u_char **dnptrs, u_char **lastdnptr));
EXTERN int __ircd_dn_skipname __P((const u_char *comp_dn, const u_char *eom));
EXTERN u_int16_t ircd_getshort __P((register const u_char *msgp));
EXTERN u_int32_t ircd_getlong __P((register const u_char *msgp));
EXTERN void ircd__putshort __P((register u_int16_t s, register u_char *msgp));
EXTERN void ircd__putlong __P((register u_int32_t l, register u_char *msgp));
#ifdef  NEXT
EXTERN u_int16_t res_getshort __P((register const u_char *msgp));
#endif /* NEXT */
#undef EXTERN
