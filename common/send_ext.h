/************************************************************************
 *   IRC - Internet Relay Chat, common/send_ext.h
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
    defined in common/send.c.
 */

/*  External definitions for global functions.
 */
#ifndef SEND_C
#define EXTERN extern
#else /* SEND_C */
#define EXTERN
#endif /* SEND_C */
EXTERN int send_queued __P((aClient *to));
#if ! USE_STDARG
EXTERN int sendto_one();
#else /* USE_STDARG */
EXTERN int vsendto_one (aClient *to, char *pattern, va_list va);
EXTERN int sendto_one (aClient *to, char *pattern, ...);
#endif /* USE_STDARG */
#ifndef CLIENT_COMPILE
EXTERN void flush_connections __P((int fd));
EXTERN void setup_svchans();
EXTERN void sendto_flog __P((aClient *cptr, char *msg, time_t duration,
			     char *username, char *hostname));
#if ! USE_STDARG
EXTERN void sendto_channel_butone();
EXTERN void sendto_serv_butone();
EXTERN void sendto_serv_v();
EXTERN void sendto_serv_notv();
EXTERN void sendto_common_channels();
EXTERN void sendto_channel_butserv();
EXTERN void sendto_match_servs();
EXTERN void sendto_match_servs_v();
EXTERN void sendto_match_butone();
EXTERN void sendto_ops_butone();
EXTERN void sendto_prefix_one();
EXTERN void sendto_flag();
#else /* USE_STDARG */
EXTERN void sendto_channel_butone (aClient *one, aClient *from,
				   aChannel *chptr, char *pattern, ...);
EXTERN void sendto_serv_butone (aClient *one, char *pattern, ...);
EXTERN void sendto_serv_v (aClient *one, int ver, char *pattern, ...);
EXTERN void sendto_serv_notv (aClient *one, int ver, char *pattern, ...);
EXTERN void sendto_common_channels (aClient *user, char *pattern, ...);
EXTERN void sendto_channel_butserv (aChannel *chptr, aClient *from,
				    char *pattern, ...);
EXTERN void sendto_match_servs (aChannel *chptr, aClient *from,
				char *format, ...);
EXTERN void sendto_match_servs_v (aChannel *chptr, aClient *from, int ver,
				char *format, ...);
EXTERN void sendto_match_butone (aClient *one, aClient *from, char *mask,
				 int what, char *pattern, ...);
EXTERN void sendto_ops_butone (aClient *one, aClient *from, char *pattern,
			       ...);
EXTERN void sendto_prefix_one (aClient *to, aClient *from, char *pattern,
			       ...);
EXTERN void sendto_flag (u_int chan, char *pattern, ...);
#endif /* USE_STDARG */
#endif /* CLIENT_COMPILE */
#undef EXTERN
