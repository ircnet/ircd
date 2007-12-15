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
#ifdef INET6
EXTERN char ipv6string[INET6_ADDRSTRLEN];
#endif
EXTERN char *mystrdup (char *s);
#if !defined(HAVE_STRTOKEN)
EXTERN char *strtoken (char **save, char *str, char *fs);
#endif /* HAVE_STRTOKEN */
#if !defined(HAVE_STRTOK)
EXTERN char *strtok (char *str, char *fs);
#endif /* HAVE_STRTOK */
#if !defined(HAVE_STRERROR)
EXTERN char *strerror (int err_no);
#endif /* HAVE_STRERROR */
EXTERN char *myctime (time_t value);
EXTERN char *mybasename (char *);
#ifdef INET6
EXTERN char *inetntop(int af, const void *in, char *local_dummy, size_t the_size);
EXTERN int inetpton(int af, const char *src, void *dst);
#endif
#if !defined(HAVE_INET_NTOA)
EXTERN char *inetntoa (char *in);
#endif /* HAVE_INET_NTOA */
#if !defined(HAVE_INET_NETOF)
EXTERN int inetnetof (struct in_addr in);
#endif /* HAVE_INET_NETOF */
#if !defined(HAVE_INET_ADDR)
EXTERN u_long inetaddr (register const char *cp);
#endif /* HAVE_INET_ADDR */
#if !defined(HAVE_INET_ATON)
EXTERN int inetaton (register const char *cp, struct in_addr *addr);
#endif /* HAVE_INET_ATON */
#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE)
EXTERN void dumpcore (char *msg, ...);
#endif /* DEBUGMODE && !CLIENT_COMPILE */
#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE) && defined(DO_DEBUG_MALLOC)
EXTERN char *MyMalloc (size_t x);
EXTERN char *MyRealloc (char *x, size_t y);
EXTERN void MyFree (void *x);
#else /* DEBUGMODE && !CLIENT_COMPILE && !DO_DEBUG_MALLOC */
EXTERN char *MyMalloc (size_t x);
EXTERN char *MyRealloc (char *x, size_t y);
#endif /* DEBUGMODE && !CLIENT_COMPILE && !DO_DEBUG_MALLOC */
EXTERN int dgets (int fd, char *buf, int num);
EXTERN char *make_version(void);
EXTERN char **make_isupport(void);
#ifdef SOLARIS_2_3
EXTERN struct hostent *solaris_gethostbyname (const char *name);
#endif /* SOLARIS_2_3 */
#if defined(HAVE_MEMCMP) && defined(MEMCMP_BROKEN)
EXTERN int irc_memcmp (const __ptr_t s1, const __ptr_t s2, size_t len);
#endif /* HAVE_MEMCMP && MEMCMP_BROKEN */
#undef EXTERN
