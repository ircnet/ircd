/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_debug_ext.h
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
    defined in irc/c_debug.c.
 */

/*  External definitions for global variables.
 */
#ifndef C_DEBUG_C
extern char debug_id[];
extern struct stats ircst, *ircstp;
#endif /* C_DEBUG_C */

/*  External definitions for global functions.
 */
#ifndef C_DEBUG_C
#define EXTERN extern
#else /* C_DEBUG_C */
#define EXTERN
#endif /* C_DEBUG_C */
#ifdef DEBUGMODE
#if ! USE_STDARG
EXTERN void debug __P((int level, char *form, char *p1, char *p2, char *p3,
		       char *p4, char *p5, char *p6, char *p7, char *p8,
		       char *p9, char *p10));
#else
EXTERN void debug __P((int level, char *form, ...));
#endif
#else
EXTERN void debug();
#endif
#undef EXTERN
