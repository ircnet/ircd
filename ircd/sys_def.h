/************************************************************************
 *   IRC - Internet Relay Chat, ircd/sys_def.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
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

#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE) && \
    !defined(CHKCONF_COMPILE) && defined(DO_DEBUG_MALLOC)
# define	free(x)		MyFree(x)
#else
# define	MyFree(x)       if ((x) != NULL) free(x)
#endif

#define	SETSOCKOPT(fd, o1, o2, p1, o3)	setsockopt(fd, o1, o2, (char *)p1,\
						   (SOCK_LEN_TYPE) sizeof(o3))

#define	GETSOCKOPT(fd, o1, o2, p1, p2)	getsockopt(fd, o1, o2, (char *)p1,\
						   (SOCK_LEN_TYPE *)p2)
