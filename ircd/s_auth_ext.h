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
# if defined(USE_IAUTH)
extern u_char iauth_options;
extern u_int iauth_spawn;
# endif
#
# define EXTERN extern
#else /* S_AUTH_C */
# define EXTERN
#endif /* S_AUTH_C */

#if defined(USE_IAUTH)
EXTERN int vsendto_iauth (char *pattern, va_list va);
EXTERN int sendto_iauth (char *pattern, ...);
EXTERN void read_iauth(void);
EXTERN void report_iauth_conf (aClient *, char *);
EXTERN void report_iauth_stats (aClient *, char *);
#endif
EXTERN void start_auth (Reg aClient *cptr);
EXTERN void send_authports (aClient *cptr);
EXTERN void read_authports (Reg aClient *cptr);

#undef EXTERN
