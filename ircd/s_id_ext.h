/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_id_ext.h
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
    defined in ircd/s_id.c.
 */

/*  External definitions for global variables.
 */
#ifndef S_ID_C
/* none */
#endif /* S_ID_C */

/*  External definitions for global functions.
 */
#ifndef S_ID_C
#define EXTERN extern
#else /* S_ID_C */
#define EXTERN
#endif /* S_ID_C */
EXTERN char *get_chid(void);
EXTERN int close_chid(char *);
EXTERN void cache_chid(aChannel *);
EXTERN int check_chid(char *);
EXTERN void collect_chid(void);

EXTERN void init_sid(char *);
EXTERN char *next_uid(void);
EXTERN int check_uid(char *, char *);
EXTERN char *ltoid(long l, int n);
EXTERN long idtol(char *id, int n);
EXTERN int sid_valid(char *sid);
EXTERN int cid_ok(char *name, int n);

#undef EXTERN
