/************************************************************************
 *   IRC - Internet Relay Chat, common/support_ext.h
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
    defined in common/support.c.
 */

/*  External definitions for global functions.
 */
#ifndef SUPPORT_C
#define EXTERN extern
#else /* SUPPORT_C */
#define EXTERN
#endif /* SUPPORT_C */
EXTERN char *mystrdup __P((char *s));
#if ! HAVE_STRTOKEN
EXTERN char *strtoken __P((char **save, char *str, char *fs));
#endif /* HAVE_STRTOKEN */
#if ! HAVE_STRTOK
EXTERN char *strtok __P((char *str, char *fs));
#endif /* HAVE_STRTOK */
#if ! HAVE_STRERROR
EXTERN char *strerror __P((int err_no));
#endif /* HAVE_STRERROR */
EXTERN char *myctime __P((time_t value));
#if ! HAVE_INET_NTOA
EXTERN char *inetntoa __P((char *in));
#endif /* HAVE_INET_NTOA */
#if ! HAVE_INET_NETOF
EXTERN int inetnetof __P((struct in_addr in));
#endif /* HAVE_INET_NETOF */
#if ! HAVE_INET_ADDR
EXTERN u_long inetaddr __P((register const char *cp));
#endif /* HAVE_INET_ADDR */
#if ! HAVE_INET_ATON
EXTERN int inetaton __P((register const char *cp, struct in_addr *addr));
#endif /* HAVE_INET_ATON */
#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE)
EXTERN void dumpcore ();
#endif /* DEBUGMODE && !CLIENT_COMPILE */
#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE) && defined(DO_DEBUG_MALLOC)
EXTERN char *MyMalloc __P((size_t x));
EXTERN char *MyRealloc __P((char *x, size_t y));
EXTERN void MyFree __P((char *x));
#else /* DEBUGMODE && !CLIENT_COMPILE && !DO_DEBUG_MALLOC */
EXTERN char *MyMalloc __P((size_t x));
EXTERN char *MyRealloc __P((char *x, size_t y));
#endif /* DEBUGMODE && !CLIENT_COMPILE && !DO_DEBUG_MALLOC */
#if ! USE_STDARG
EXTERN int irc_sprintf();
#endif /* USE_STDARG */
EXTERN int dgets __P((int fd, char *buf, int num));
EXTERN char *make_version();
#if SOLARIS_2_3
EXTERN struct hostent *solaris_gethostbyname __P((const char *name));
#endif /* SOLARIS_2_3 */
#if HAVE_MEMCMP && MEMCMP_BROKEN
EXTERN int irc_memcmp __P((const __ptr_t s1, const __ptr_t s2, size_t len));
#endif /* HAVE_MEMCMP && MEMCMP_BROKEN */
#undef EXTERN
