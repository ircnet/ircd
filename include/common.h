/************************************************************************
 *   IRC - Internet Relay Chat, include/common.h
 *   Copyright (C) 1990 Armin Gruner
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

#ifndef	__common_include__
#define __common_include__

#ifdef _AIX
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#else
#include "cdefs.h"
#endif

#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

#if 0
#ifndef	HAVE_SYS_MALLOC_H
char	*malloc(), *calloc();
#else
/*#include MALLOCH*/
#endif
#endif

extern	int	match __P((char *, char *));
extern	char	*collapse __P((char *));
extern	int	mycmp __P((char *, char *));
extern	int	myncmp __P((char *, char *, int));
#ifdef NEED_STRTOK
extern	char	*strtok __P((char *, char *));
#endif
#ifdef NEED_STRTOKEN
extern	char	*strtoken __P((char **, char *, char *));
#endif
#ifdef NEED_INET_ADDR
extern unsigned long inet_addr __P((char *));
#else	/* OSF */
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#if defined(NEED_INET_NTOA) || defined(NEED_INET_NETOF)
#include <netinet/in.h>
#endif

extern char *myctime __P((time_t));
extern char *strtoken __P((char **, char *, char *));

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#ifdef	SPRINTF
#undef	SPRINTF
#endif
#ifndef USE_STDARG
#define	SPRINTF	(void) irc_sprintf
#else
#define SPRINTF (void) sprintf
#endif

#define DupString(x,y) do {x = (char *)MyMalloc(strlen((char *)y) + 1);\
			   (void)strcpy((char *)x, (char *)y);\
			  } while(0)

extern unsigned char tolowertab[];

#undef tolower
#define tolower(c) (tolowertab[(u_char)(c)])

extern unsigned char touppertab[];

#undef toupper
#define toupper(c) (touppertab[(u_char)(c)])

#undef isalpha
#undef isdigit
#undef isxdigit
#undef isalnum
#undef isprint
#undef isascii
#undef isgraph
#undef ispunct
#undef islower
#undef isupper
#undef isspace

extern unsigned char char_atribs[];

#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32

#define isalpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define isspace(c) (char_atribs[(u_char)(c)]&SPACE)
#define islower(c) ((char_atribs[(u_char)(c)]&ALPHA) && \
		    ((u_char)(c) > (u_char)0x5f))
#define isupper(c) ((char_atribs[(u_char)(c)]&ALPHA) && \
		    ((u_char)(c) < (u_char)0x60))
#define isdigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	isxdigit(c) (isdigit(c) || \
		     (u_char)'a' <= (c) && (c) <= (u_char)'f' || \
		     (u_char)'A' <= (c) && (c) <= (u_char)'F')
#define isalnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define isprint(c) (char_atribs[(u_char)(c)]&PRINT)
#define isascii(c) (((u_char)(c) >= (u_char)'\0') && \
		    ((u_char)(c) <= (u_char)0x7f))
#define isgraph(c) ((char_atribs[(u_char)(c)]&PRINT) && \
		    ((u_char)(c) != (u_char)0x32))
#define ispunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))

extern void	flush_connections __P((int));
#ifndef USE_STDARG
extern int	irc_sprintf();
#endif

/*VARARGS?*/

#endif /* __common_include__ */
