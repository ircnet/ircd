/************************************************************************
 *   IRC - Internet Relay Chat, ircd/hash_ext.h
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
    defined in ircd/hash.c.
 */

/*  External definitions for global variables.
 */
#ifndef HASH_C
extern int _HASHSIZE;
extern int _CHANNELHASHSIZE;
extern int _SERVERSIZE;
#endif /* HASH_C */

/*  External definitions for global functions.
 */
#ifndef HASH_C
#define EXTERN extern
#else /* HASH_C */
#define EXTERN
#endif /* HASH_C */
EXTERN void inithashtables();
EXTERN int add_to_client_hash_table __P((char *name, aClient *cptr));
EXTERN int add_to_channel_hash_table __P((char *name, aChannel *chptr));
EXTERN int add_to_server_hash_table __P((aServer *sptr, aClient *cptr));
EXTERN int del_from_client_hash_table __P((char *name, aClient *cptr));
EXTERN int del_from_channel_hash_table __P((char *name, aChannel *chptr));
EXTERN int del_from_server_hash_table __P((aServer *sptr, aClient *cptr));
EXTERN aClient *hash_find_client __P((char *name, aClient *cptr));
EXTERN aClient *hash_find_server __P((char *server, aClient *cptr));
EXTERN aChannel *hash_find_channel __P((char *name, aChannel *chptr));
EXTERN aChannel *hash_find_channels __P((char *name, aChannel *chptr));
EXTERN aServer *hash_find_stoken __P((int tok, aClient *cptr, void *dummy));
EXTERN int m_hash __P((aClient *cptr, aClient *sptr, int parc, char *parv[]));
#undef EXTERN
