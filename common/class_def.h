/************************************************************************
 *   IRC - Internet Relay Chat, common/class_def.h
 *   Copyright (C) 1990 Darren Reed
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

typedef struct Class {
	int	class;
	int	conFreq;
	int	pingFreq;
	int	maxLinks;
	long	maxSendq;
	int	maxHLocal;
	int	maxUHLocal;
	int	maxHGlobal;
	int	maxUHGlobal;
	int	links;
	struct Class *next;
} aClass;

#define	Class(x)	((x)->class)
#define	ConFreq(x)	((x)->conFreq)
#define	PingFreq(x)	((x)->pingFreq)
#define	MaxLinks(x)	((x)->maxLinks)
#define	MaxSendq(x)	((x)->maxSendq)
#define	MaxHLocal(x)	((x)->maxHLocal)
#define	MaxUHLocal(x)	((x)->maxUHLocal)
#define	MaxHGlobal(x)	((x)->maxHGlobal)
#define	MaxUHGlobal(x)	((x)->maxUHGlobal)
#define	Links(x)	((x)->links)
#define	IncSendq(x)	MaxSendq(x) = (int)((float)MaxSendq(x) * 1.1)

#define	ConfLinks(x)	(Class(x)->links)
#define	ConfMaxLinks(x)	(Class(x)->maxLinks)
#define	ConfClass(x)	(Class(x)->class)
#define	ConfConFreq(x)	(Class(x)->conFreq)
#define	ConfPingFreq(x)	(Class(x)->pingFreq)
#define	ConfSendq(x)	(Class(x)->maxSendq)
#define	ConfMaxHLocal(x)	(Class(x)->maxHLocal)
#define	ConfMaxUHLocal(x)	(Class(x)->maxUHLocal)
#define	ConfMaxHGlobal(x)	(Class(x)->maxHGlobal)
#define	ConfMaxUHGlobal(x)	(Class(x)->maxUHGlobal)

#define	FirstClass() 	classes
#define	NextClass(x)	((x)->next)
