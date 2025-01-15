/************************************************************************
 *   IRC - Internet Relay Chat, common/match_ext.h
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
    defined in common/match.c.
 */

/*  External definitions for global variables.
 */
#ifndef MATCH_C
extern unsigned char tolowertab[];
extern unsigned char touppertab[];
extern unsigned char char_atribs[];
#endif /* MATCH_C */

/*  External definitions for global functions.
 */
#ifndef MATCH_C
#define EXTERN extern
#else /* MATCH_C */
#define EXTERN
#endif /* MATCH_C */
EXTERN int match (char *mask, char *name);
EXTERN char *collapse (char *pattern);
EXTERN int mycmp (char *s1, char *s2);
EXTERN int myncmp (char *str1, char *str2, int n);
EXTERN int isvalidusername (char *username);
EXTERN int match_ipmask(char *mask, char *ip);
#undef EXTERN
