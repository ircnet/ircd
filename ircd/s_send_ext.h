/************************************************************************
 *   IRC - Internet Relay Chat, common/send_ext.h
 *   Copyright (C) 1999 Christophe Kalt
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
    defined in ircd/s_send.c.
 */

/*  External definitions for global functions.
 */
#ifndef S_SEND_C
#define EXTERN extern
#else /* S_SEND_C */
#define EXTERN
#endif /* S_SEND_C */

EXTERN void esendto_one(aClient *orig, aClient *dest, char *imsg, char *fmt,
			...);
EXTERN void esendto_serv_butone(aClient *orig, aClient *dest, char *dname,
				char *imsg, aClient *one, char *fmt, ...);
EXTERN void esendto_channel_butone(aClient *orig, char *imsg, aClient *one,
				   aChannel *chptr, char *fmt, ...);
EXTERN void esendto_match_servs(aClient *orig, char *imsg, aChannel *chptr,
				char *fmt, ...);

#undef EXTERN
