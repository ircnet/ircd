/************************************************************************
 *   IRC - Internet Relay Chat, ircd/list_ext.h
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
    defined in ircd/list.c.
 */

/*  External definitions for global variables.
 */
#ifndef LIST_C
extern anUser *usrtop;
extern aServer *svrtop;
extern int numclients;
extern const char *DefInfo;
#endif /* LIST_C */

/*  External definitions for global functions.
 */
#ifndef LIST_C
#define EXTERN extern
#else /* LIST_C */
#define EXTERN
#endif /* LIST_C */
EXTERN void initlists(void);
EXTERN void outofmemory(void);
#ifdef	DEBUGMODE
EXTERN void checklists(void);
EXTERN void send_listinfo (aClient *cptr, char *name);
#endif /* DEBUGMOE */
EXTERN aClient *make_client (aClient *from);
EXTERN void free_client (aClient *cptr);
EXTERN anUser *make_user (aClient *cptr, int iplen);
EXTERN aServer *make_server (aClient *cptr);
EXTERN void free_user (anUser *user);
EXTERN void free_server (aServer *serv);
EXTERN void remove_client_from_list (Reg aClient *cptr);
EXTERN void reorder_client_in_list (aClient *cptr);
EXTERN void add_client_to_list (aClient *cptr);
EXTERN Link *find_user_link (Reg Link *lp, Reg aClient *ptr);
EXTERN Link *find_channel_link (Reg Link *lp, Reg aChannel *ptr);
EXTERN Link *make_link(void);
EXTERN invLink *make_invlink(void);
EXTERN void free_link (Reg Link *lp);
EXTERN void free_invlink (Reg invLink *lp);
EXTERN aClass *make_class(void);
EXTERN void free_class (Reg aClass *tmp);
EXTERN aConfItem *make_conf(void);
EXTERN void delist_conf (aConfItem *aconf);
EXTERN void free_conf (aConfItem *aconf);
EXTERN void add_fd (int fd, FdAry *ary);
EXTERN int del_fd (int fd, FdAry *ary);
#undef EXTERN
