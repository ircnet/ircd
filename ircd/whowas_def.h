/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas_def.h
 *   Copyright (C) 1990  Markku Savela
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

/*
** WHOWAS structure moved here from whowas.c
*/
typedef struct aname {
	anUser	*ww_user;
	aClient	*ww_online;
	time_t	ww_logout;
	char	ww_nick[NICKLEN+1];
	char	ww_info[REALLEN+1];
} aName;

typedef struct alock {
	time_t	logout;
	char	nick[NICKLEN];
} aLock;
