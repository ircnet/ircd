/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas_ext.h
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
    defined in ircd/whowas.c.
 */

/*  External definitions for global variables.
 */
#ifndef WHOWAS_C
extern int ww_index, ww_size;
extern int lk_index, lk_size;
#endif /* WHOWAS_C */

/*  External definitions for global functions.
 */
#ifndef WHOWAS_C
#define EXTERN extern
#else /* WHOWAS_C */
#define EXTERN
#endif /* WHOWAS_C */
EXTERN void add_history __P((Reg aClient *cptr, Reg aClient *nodelay));
EXTERN aClient *get_history __P((char *nick, time_t timelimit));
EXTERN int find_history __P((char *nick, time_t timelimit));
EXTERN void off_history __P((Reg aClient *cptr));
EXTERN void initwhowas();
EXTERN int m_whowas __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN void count_whowas_memory __P((int *wwu, int *wwa, u_long *wwam,
				     int *wwuw));
#undef EXTERN
