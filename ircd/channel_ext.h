/************************************************************************
 *   IRC - Internet Relay Chat, ircd/channel_ext.h
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
    defined in ircd/channel.c.
 */

/*  External definitions for global variables.
 */
#ifndef CHANNEL_C
extern aChannel *channel;
#endif /* CHANNEL_C */

/*  External definitions for global functions.
 */
#ifndef CHANNEL_C
#define EXTERN extern
#else /* CHANNEL_C */
#define EXTERN
#endif /* CHANNEL_C */
EXTERN void remove_user_from_channel __P((aClient *sptr, aChannel *chptr));
EXTERN int is_chan_op __P((aClient *cptr, aChannel *chptr));
EXTERN int has_voice __P((aClient *cptr, aChannel *chptr));
EXTERN int can_send __P((aClient *cptr, aChannel *chptr));
EXTERN aChannel *find_channel __P((Reg char *chname, Reg aChannel *chptr));
EXTERN void setup_server_channels __P((aClient *mp));
EXTERN void channel_modes __P((aClient *cptr, Reg char *mbuf, Reg char *pbuf,
			       aChannel *chptr));
EXTERN void send_channel_modes __P((aClient *cptr, aChannel *chptr));
EXTERN int m_mode __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN void clean_channelname __P((Reg char *cn));
EXTERN void del_invite __P((aClient *cptr, aChannel *chptr));
EXTERN int m_join __P((Reg aClient *cptr, Reg aClient *sptr, int parc,
		       char *parv[]));
EXTERN int m_part __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_kick __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int count_channels __P((aClient *sptr));
EXTERN int m_topic __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_invite __P((aClient *cptr, aClient *sptr, int parc,
			 char *parv[]));
EXTERN int m_list __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN int m_names __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
EXTERN void send_user_joins __P((aClient *cptr, aClient *user));
EXTERN time_t collect_channel_garbage __P((time_t now));
#undef EXTERN
