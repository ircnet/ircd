/************************************************************************
 *   IRC - Internet Relay Chat, include/sys.h
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

#ifndef	__sys_include__
#define __sys_include__

#ifdef ISC202
# include <net/errno.h>
#else
# include <sys/errno.h>
#endif

#include "setup.h"
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#else
# include "cdefs.h"
#endif
#include <sys/param.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#elif (!defined(BSD)) || (BSD < 199306)
# include "bitypes.h"
#endif

#ifdef	HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef	HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef	HAVE_STRINGS_H
# include <strings.h>
#else
# ifdef	HAVE_STRING_H
#  include <string.h>
# endif
#endif
#define	strcasecmp	mycmp
#define	strncasecmp	myncmp
#if defined(NOINDEX)
# define   index   strchr
# define   rindex  strrchr
#endif
#if !defined(HAVE_STRINGS_H) && !defined(HAVE_STRING_H)
extern	char	*index __P((char *, char));
extern	char	*rindex __P((char *, char));
#endif

#if defined(NEED_BCMP) || defined(NEED_BZERO)
# define	bcmp(a,b,c)	memcmp(a,b,c)
# define	bzero(a,b)	memset(a,0,b)
# define	bcopy(a,b,c)	memmove(b,a,c)
#endif

#ifdef AIX
# include <sys/select.h>
#endif
#if defined(HPUX )
# include <time.h>
#else
# include <sys/time.h>
#endif

#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE) && defined(DO_DEBUG_MALLOC)
# define	free(x)		MyFree(x)
#else
# define	MyFree(x)       if ((x) != NULL) free(x)
#endif

#ifdef NEXT
# define VOIDSIG int	/* whether signal() returns int of void */
#else
# define VOIDSIG void	/* whether signal() returns int of void */
#endif

extern	VOIDSIG	dummy();
extern	VOIDSIG s_die __P(());

#ifdef	DYNIXPTX
# define	NO_U_TYPES
#endif

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#ifdef	USE_VARARGS
# include <varargs.h>
#endif

#define	SETSOCKOPT(fd, o1, o2, p1, o3)	setsockopt(fd, o1, o2, (char *)p1,\
						   sizeof(o3))

#define	GETSOCKOPT(fd, o1, o2, p1, p2)	getsockopt(fd, o1, o2, (char *)p1,\
						   (int *)p2)

/* These are in latest versions of arpa/nameser.h in BIND */
#ifndef	HFIXEDSZ
#define HFIXEDSZ	12	/* #/bytes of fixed data in header */
#endif
#ifndef NS_NOTIFY_OP
#define NS_NOTIFY_OP	0x4	/* notify secondary of SOA change */
#endif
#ifndef INT16SZ
#define INT16SZ		2	/* for systems without 16-bit ints */
#endif
#ifndef INT32SZ
#define INT32SZ		4	/* for systems without 32-bit ints */
#endif


#endif /* __sys_include__ */
