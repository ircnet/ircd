/************************************************************************
 *   IRC - Internet Relay Chat, ircd/ircd_ext.h
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
    defined in ircd/ircd.c.
 */

/*  External definitions for global variables.
 */
#ifndef IRCD_C
extern aClient me;
extern aClient *client;
extern istat_t istat;
extern char **myargv;
extern int rehashed;
extern int portnum;
extern char *configfile;
extern int debuglevel;
extern int bootopt;
extern char *debugmode;
extern char *sbrk0;
extern char *tunefile;
extern time_t nextconnect;
extern time_t nextgarbage;
extern time_t nextping;
extern time_t nextdnscheck;
extern time_t nextexpire;
#endif /* IRCD_C */

/*  External definitions for global functions.
 */
#ifndef IRCD_C
#define EXTERN extern
#else /* IRCD_C */
#define EXTERN
#endif /* IRCD_C */
#ifdef	PROFIL
EXTERN RETSIGTYPE s_monitor __P((int s));
#endif /* PROFIL */
EXTERN RETSIGTYPE s_die __P((int s));
EXTERN void restart __P((char *mesg));
EXTERN RETSIGTYPE s_restart __P((int s));
EXTERN void server_reboot();
EXTERN time_t io_loop __P((time_t delay));
EXTERN void ircd_writetune __P((char *filename));
EXTERN void ircd_readtune __P((char *filename));
#undef EXTERN
