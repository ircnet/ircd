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

#ifdef ENABLE_CIDR_LIMITS
struct _patricia_tree_t;
#endif

typedef struct Class {
	int	class;
	int	conFreq;
	int	pingFreq;
	int	maxLinks;
	int	maxSendq;
	int	maxBSendq;
	int	maxHLocal;
	int	maxUHLocal;
	int	maxHGlobal;
	int	maxUHGlobal;
	int	links;
#ifdef ENABLE_CIDR_LIMITS
	int	cidr_len;
	int	cidr_amount;
	struct _patricia_tree_t *ip_limits;
#endif
	struct Class *next;
} aClass;

#define	Class(x)	((x)->class)
#define	ConFreq(x)	((x)->conFreq)
#define	PingFreq(x)	((x)->pingFreq)
#define	MaxLinks(x)	((x)->maxLinks)
#define	MaxSendq(x)	((x)->maxSendq)
#define	MaxBSendq(x)	((x)->maxBSendq)
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
#ifdef ENABLE_CIDR_LIMITS
#define	MaxCidrAmount(x)	((x)->cidr_amount)
#define	CidrLen(x)	((x)->cidr_len)
#define	CidrTree(x)	((x)->ip_limits)
#define	ConfMaxCidrAmount(x)	(Class(x)->cidr_amount)
#define	ConfCidrLen(x)	(Class(x)->cidr_len)
#define	ConfCidrTree(x)	(Class(x)->ip_limits)
#endif

#define	FirstClass() 	classes
#define	NextClass(x)	((x)->next)
