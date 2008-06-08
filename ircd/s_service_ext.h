/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_service_ext.h
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
    defined in ircd/s_service.c.
 */

/*  External definitions for global variables.
 */
#ifndef S_SERVICE_C
extern	aService	*svctop;
#endif

/*  External definitions for global functions.
 */
#ifndef S_SERVICE_C
#define EXTERN extern
#else /* S_SERVICE_C */
#define EXTERN
#endif /* S_SERVICE_C */
EXTERN aService *make_service (aClient *cptr);
EXTERN void free_service (aClient *cptr);
#ifdef	USE_SERVICES
EXTERN void check_services_butone (long action, aServer *servp,
				       aClient *cptr, char *fmt, ...);
EXTERN void check_services_num (aClient *sptr, char *umode);
EXTERN aConfItem *find_conf_service (aClient *cptr, int type,
					 aConfItem *aconf);
EXTERN int m_servset (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
#endif /* USE_SERVICES */
EXTERN int m_service (aClient *cptr, aClient *sptr, int parc,
			  char *parv[]);
EXTERN int m_servlist (aClient *cptr, aClient *sptr, int parc,
			   char *parv[]);
EXTERN int m_squery (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
#undef EXTERN
