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
EXTERN void remove_user_from_channel (aClient *sptr, aChannel *chptr);
EXTERN int is_chan_op (aClient *cptr, aChannel *chptr);
EXTERN int has_voice (aClient *cptr, aChannel *chptr);
EXTERN int can_send (aClient *cptr, aChannel *chptr);

#ifdef JAPANESE
EXTERN char *get_channelmask (char *);
EXTERN int jp_valid (aClient *, aChannel *, char *);
#else
#define get_channelmask(x) rindex((x), ':')
#endif

EXTERN aChannel *find_channel (Reg char *chname, Reg aChannel *chptr);
EXTERN void setup_server_channels (aClient *mp);
EXTERN void channel_modes (aClient *cptr, Reg char *mbuf, Reg char *pbuf,
			       aChannel *chptr);
EXTERN void send_channel_modes (aClient *cptr, aChannel *chptr);
EXTERN void send_channel_members (aClient *cptr, aChannel *chptr);
EXTERN int m_mode (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int clean_channelname (Reg char *cn);
EXTERN void del_invite (aClient *cptr, aChannel *chptr);
EXTERN int m_join (Reg aClient *cptr, Reg aClient *sptr, int parc,
		       char *parv[]);
EXTERN int m_njoin (Reg aClient *cptr, Reg aClient *sptr, int parc,
		        char *parv[]);
EXTERN int m_part (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_kick (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_topic (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_invite (aClient *cptr, aClient *sptr, int parc,
			 char *parv[]);
EXTERN int m_list (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN int m_names (aClient *cptr, aClient *sptr, int parc, char *parv[]);
EXTERN time_t collect_channel_garbage (time_t now);
#undef EXTERN
