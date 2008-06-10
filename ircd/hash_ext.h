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
extern int _UIDSIZE;
extern int _CHANNELHASHSIZE;
extern int _SIDSIZE;
#ifdef USE_HOSTHASH
extern int _HOSTNAMEHASHSIZE;
#endif
#ifdef USE_IPHASH
extern int _IPHASHSIZE;
#endif
#endif /* HASH_C */

/*  External definitions for global functions.
 */
#ifndef HASH_C
#define EXTERN extern
#else /* HASH_C */
#define EXTERN
#endif /* HASH_C */
EXTERN void inithashtables(void);
EXTERN int add_to_client_hash_table (char *name, aClient *cptr);
EXTERN int add_to_uid_hash_table (char *uid, aClient *cptr);
EXTERN int add_to_channel_hash_table (char *name, aChannel *chptr);
EXTERN int add_to_sid_hash_table (char *sid, aClient *cptr);
EXTERN int del_from_client_hash_table (char *name, aClient *cptr);
EXTERN int del_from_uid_hash_table (char *uid, aClient *cptr);
EXTERN int del_from_channel_hash_table (char *name, aChannel *chptr);
EXTERN int del_from_sid_hash_table (aServer *sptr);
EXTERN aClient *hash_find_client (char *name, aClient *cptr);
EXTERN aClient *hash_find_uid (char *uid, aClient *cptr);
EXTERN aClient *hash_find_server (char *server, aClient *cptr);
EXTERN aChannel *hash_find_channel (char *name, aChannel *chptr);
EXTERN aChannel *hash_find_channels (char *name, aChannel *chptr);
EXTERN aClient *hash_find_sid (char *sid, aClient *cptr);
#ifdef USE_HOSTHASH
EXTERN int add_to_hostname_hash_table (char *hostname, anUser *user);
EXTERN int del_from_hostname_hash_table (char *hostname, anUser *user);
EXTERN anUser *hash_find_hostname (char *hostname, anUser *user);
#endif
#ifdef USE_IPHASH
EXTERN int add_to_ip_hash_table (char *ip, anUser *user);
EXTERN int del_from_ip_hash_table (char *ip, anUser *user);
EXTERN anUser *hash_find_ip (char *ip, anUser *user);
#endif
EXTERN int m_hash (aClient *cptr, aClient *sptr, int parc, char *parv[]);

#undef EXTERN
