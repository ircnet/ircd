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
EXTERN void start_auth __P((Reg aClient *cptr));
EXTERN void send_authports __P((aClient *cptr));
EXTERN void read_authports __P((Reg aClient *cptr));
#undef EXTERN
