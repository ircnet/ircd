/************************************************************************
 *   IRC - Internet Relay Chat, include/whowas.h
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

#ifndef	__whowas_include__
#define __whowas_include__

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

/*
** add_history
**	Add the currently defined name of the client to history.
**	usually called before changing to a new name (nick).
**	Client must be a fully registered user (specifically,
**	the user structure must have been allocated).
*/
void	add_history __P((aClient *, aClient *));

/*
** off_history
**	This must be called when the client structure is about to
**	be released. History mechanism keeps pointers to client
**	structures and it must know when they cease to exist. This
**	also implicitly calls AddHistory.
*/
void	off_history __P((aClient *));

/*
** get_history
**	Return the current client that was using the given
**	nickname within the timelimit. Returns NULL, if no
**	one found...
*/
aClient	*get_history __P((char *, time_t));
					/* Nick name */
					/* Time limit in seconds */

/*
** find_history
**      Return 1 if a user was using the given nickname within
**      the timelimit. Returns NULL, if none found...
*/
int	find_history __P((char *, time_t));

int	m_whowas __P((aClient *, aClient *, int, char *[]));

/*
** for debugging...counts related structures stored in whowas array.
*/
void	count_whowas_memory __P((int *, int *, u_long *, int *));

#endif /* __whowas_include__ */
