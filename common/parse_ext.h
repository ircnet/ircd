/************************************************************************
 *   IRC - Internet Relay Chat, common/parse_ext.h
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
    defined in common/parse.c.
 */

/*  External definitions for global variables.
 */
#ifndef PARSE_C
extern struct Message msgtab[];
#endif /* PARSE_C */

/*  External definitions for global functions.
 */
#ifndef PARSE_C
#define EXTERN extern
#else /* PARSE_C */
#define EXTERN
#endif /* PARSE_C */
EXTERN aClient *find_client (char *name, Reg aClient *cptr);
EXTERN aClient *find_uid (char *uid, Reg aClient *cptr);
EXTERN aClient *find_sid (char *sid, Reg aClient *cptr);
EXTERN aClient *find_service (char *name, Reg aClient *cptr);
EXTERN aClient *find_server (char *name, Reg aClient *cptr);
EXTERN aClient *find_mask (char *name, aClient *cptr);
EXTERN aClient *find_name (char *name, aClient *cptr);
EXTERN aClient *find_matching_client (char *mask);
EXTERN aClient *find_target (char *name, aClient *cptr);
EXTERN aClient *find_userhost (char *user, char *host, aClient *cptr,
				   int *count);
EXTERN aClient *find_person (char *name, aClient *cptr);
EXTERN int parse (aClient *cptr, char *buffer, char *bufend);
EXTERN char *getfield (char *irc_newline);
EXTERN int m_nop(aClient *, aClient *, int, char **);
EXTERN int m_nopriv(aClient *, aClient *, int, char **);
EXTERN int m_unreg(aClient *, aClient *, int, char **);
EXTERN int m_reg(aClient *, aClient *, int, char **);
#undef EXTERN
