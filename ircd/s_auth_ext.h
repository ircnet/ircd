/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_auth_ext.h
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
    defined in ircd/s_auth.c.
 */

/*  External definitions for global functions.
 */
#ifndef S_AUTH_C
#define EXTERN extern
#else /* S_AUTH_C */
#define EXTERN
#endif /* S_AUTH_C */
# if defined(USE_IAUTH)
#  if ! USE_STDARG
EXTERN int sendto_iauth();
#  else /* USE_STDARG */
EXTERN int vsendto_iauth (char *pattern, va_list va);
EXTERN int sendto_iauth (char *pattern, ...);
#  endif
EXTERN void read_iauth();
EXTERN void report_iauth_conf __P((aClient *, char *));
# endif
EXTERN void start_auth __P((Reg aClient *cptr));
EXTERN void send_authports __P((aClient *cptr));
EXTERN void read_authports __P((Reg aClient *cptr));
#undef EXTERN
