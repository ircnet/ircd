/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_conf_ext.h
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
    defined in iauth/a_conf.c.
 */

/*  External definitions for global variables.
 */
#ifndef A_CONF_C
extern u_int	debuglevel;
extern AnInstance *instances;
#endif /* A_CONF_C */

/*  External definitions for global functions.
 */
#ifndef A_CONF_C
# define EXTERN extern
#else /* A_CONF_C */
# define EXTERN
#endif /* A_CONF_C */

EXTERN char *conf_read __P((char *));
EXTERN int conf_match __P((u_int, AnInstance *));
EXTERN void conf_ircd();

#undef EXTERN
