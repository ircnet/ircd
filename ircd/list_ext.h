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
EXTERN void initlists();
EXTERN void outofmemory();
#ifdef	DEBUGMODE
EXTERN void checklists();
EXTERN void send_listinfo __P((aClient *cptr, char *name));
#endif /* DEBUGMOE */
EXTERN aClient *make_client __P((aClient *from));
EXTERN void free_client __P((aClient *cptr));
EXTERN anUser *make_user __P((aClient *cptr));
EXTERN aServer *make_server __P((aClient *cptr));
EXTERN void free_user __P((Reg anUser *user, aClient *cptr));
EXTERN void free_server __P((aServer *serv, aClient *cptr));
EXTERN void remove_client_from_list __P((Reg aClient *cptr));
EXTERN void add_client_to_list __P((aClient *cptr));
EXTERN Link *find_user_link __P((Reg Link *lp, Reg aClient *ptr));
EXTERN Link *find_channel_link __P((Reg Link *lp, Reg aChannel *ptr));
EXTERN Link *make_link();
EXTERN void free_link __P((Reg Link *lp));
EXTERN aClass *make_class();
EXTERN void free_class __P((Reg aClass *tmp));
EXTERN aConfItem *make_conf();
EXTERN void delist_conf __P((aConfItem *aconf));
EXTERN void free_conf __P((aConfItem *aconf));
EXTERN void add_fd __P((int fd, FdAry *ary));
EXTERN int del_fd __P((int fd, FdAry *ary));
#ifdef	HUB
EXTERN void add_active __P((int fd, FdAry *ary));
EXTERN void decay_activity();
EXTERN int sort_active __P((const void *a1, const void *a2));
EXTERN void build_active();
#endif /* HUB */
#undef EXTERN
