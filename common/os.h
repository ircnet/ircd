/************************************************************************
 *   IRC - Internet Relay Chat, include/os.h
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

/*  This file contains the various system-relative "#include" lines needed for
    getting all system types, macros, external variables and functions defined.

    This file also contains definitions of types, constants, macros, external
    variables and functions that are missing in the include files of some OS,
    or need to be redefined for various reasons.
 */

#include "setup.h"

#if defined(linux) || defined(__CYGWIN32__) || defined(__GLIBC__)
# define _GNU_SOURCE 1
#endif

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_STDARG_H
# include <stdarg.h>
#else
#error You need working stdarg.
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_CTYPE_H
# include <ctype.h>
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#ifdef HAVE_VFORK_H
# include <vfork.h>
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#ifdef HAVE_UTMP_H
# include <utmp.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
# if defined(linux) && !defined(POLLRDNORM)
/* Linux 2.1.xx supports poll(), header files are not upto date yet */
#  define POLLRDNORM 0x0040
# endif
#endif

#ifdef HAVE_STROPTS_H
# include <stropts.h>
#endif

#ifdef HAVE_NETDB_H
# ifdef BAD___CONST_NETDB_H
#  ifndef __const
#   define __const
#   include <netdb.h>
#   undef __const
#  else
#   include <netdb.h>
#  endif
# else
#  include <netdb.h>
# endif
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif

#ifdef HAVE_SYS_SYSLOG_H
# include <sys/syslog.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
# ifdef HAVE_STRING_WITH_STRINGS
#  include <strings.h>
# endif
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINFO_NI_H
# include <netinfo/ni.h>
#endif

#if defined(USE_ZLIB) && !defined(CLIENT_COMPILE) && \
	!defined(CHKCONF_COMPILE) && !defined(CONTRIB_COMPILE)
# include <zlib.h>
#endif

#if defined(HAVE_DLFCN_H)
# include <dlfcn.h>
#endif

#if (defined(__FreeBSD__) ||\
     defined(__OpenBSD__) ||\
     defined(__NetBSD__)     ) && !defined(__ELF__)
# define DLSYM_NEEDS_UNDERSCORE
#endif

/*  Some special include files for a special OS. :)
 */

#ifdef ISC
# include <sys/bsdtypes.h>
# include <sys/sioctl.h>
# include <sys/stream.h>
# include <net/errno.h>
#endif

/*  Some additional system-relative defines that make the code easier.
 *
 *  Note. In fact, the C code should never use system-specific tests; as you
 *        know, numerous people worked on it in the past, so now it is
 *        difficult for me to know why such tests are used in the code. But I
 *        still hope this part of the include file will be cleaned up in
 *        further releases.                          -- Alain.Nissen@ulg.ac.be
 */

#if defined(ultrix) || defined(__ultrix) || defined(__ultrix__)
# ifdef ULTRIX
#  undef ULTRIX
# endif
# define ULTRIX
#endif

#if defined(aix) || defined(_AIX)
# ifdef AIX
#  undef AIX
# endif
# define AIX
#endif

#if defined(sgi) || defined(__sgi) || defined(__sgi__)
# ifdef SGI
#  undef SGI
# endif
# define SGI
#endif

#ifdef NeXT
# ifdef NEXT
#  undef NEXT
# endif
# define NEXT
#endif

#if defined(hpux) || defined(__hpux)
# ifdef HPUX
#  undef HPUX
# endif
# define HPUX
#endif

#if defined(_SVR4) || defined(__SVR4) || defined(__SVR4__) || defined(__svr4__)
# ifdef SVR4
#  undef SVR4
# endif
# define SVR4
#endif

#ifdef __osf__
# ifdef OSF
#  undef OSF
# endif
# define OSF
# ifndef BSD
#  define BSD 1
# endif
#endif

#if defined(sequent) || defined(__sequent) || defined(__sequent)
# ifdef _SEQUENT_
#  undef _SEQUENT_
# endif
# define _SEQUENT_
# undef BSD
# define SYSV
# define DYNIXPTX
#endif

#if defined(mips) || defined(PCS)
#undef SYSV
#endif

#ifdef MIPS
#undef BSD
#define BSD             1       /* mips only works in bsd43 environment */
#endif

#define Reg register

/*  Strings and memory functions portability problems.
 */

#if defined(HAVE_MEMCMP) && defined(MEMCMP_BROKEN)
# define memcmp irc_memcmp
#endif

#ifdef HAVE_STRCHR
# ifdef index
#  undef index
# endif
# define index strchr
#endif

#ifdef HAVE_STRRCHR
# ifdef rindex
#  undef rindex
# endif
# define rindex strrchr
#endif

#if !defined(HAVE_STRCHR) && defined(HAVE_INDEX)
# ifdef strchr
#  undef strchr
# endif
# define strchr index
#endif

#if !defined(HAVE_STRRCHR) && defined(HAVE_RINDEX)
# ifdef strrchr
#  undef strrchr
# endif
# define strrchr rindex
#endif

#ifdef HAVE_MEMCMP
# ifdef bcmp
#  undef bcmp
# endif
# define bcmp memcmp
#endif

#ifdef HAVE_MEMSET
# ifdef bzero
#  undef bzero
# endif
# define bzero(a,b) memset((a),0,(b))
#endif

#ifdef HAVE_MEMMOVE
# ifdef bcopy
#  undef bcopy
# endif
# define bcopy(a,b,c) memmove((b),(a),(c))
#endif

#if !defined(HAVE_MEMCMP) && defined(HAVE_BCMP)
# ifdef memcmp
#  undef memcmp
# endif
# define memcmp bcmp
#endif

#if !defined(HAVE_MEMCPY) && defined(HAVE_BCOPY)
# ifdef memcpy
#  undef memcpy
# endif
# define memcpy(d,s,n) bcopy((s),(d),(n))
#endif

#define	strcasecmp	mycmp
#define	strncasecmp	myncmp

/*  inet_ntoa(), inet_aton(), inet_addr() and inet_netof() portability
 *  problems.
 *
 *  The undefs and prototypes are "needed" because of the way Paul Vixie
 *  majorly screws up system's include files with Bind.  In this case, it's
 *  Bind 8.x installing /usr[/local]/include/arpa/inet.h -krys
 */

#ifdef HAVE_INET_NTOA
# ifdef inet_ntoa
#  undef inet_ntoa
extern char *inet_ntoa (struct in_addr in);
# endif
# define inetntoa(x) inet_ntoa(*(struct in_addr *)(x))
#endif
#ifdef HAVE_INET_ATON
# ifdef inet_aton
#  undef inet_aton
extern int inet_aton (const char *cp, struct in_addr *addr);
# endif
# define inetaton inet_aton
#endif
#ifdef HAVE_INET_ADDR
# ifdef inet_addr
#  undef inet_addr
extern unsigned long int inet_addr (const char *cp);
# endif
# define inetaddr inet_addr
#endif
#ifdef HAVE_INET_NETOF
# ifdef inet_netof
#  undef inet_netof
extern int inet_netof (struct in_addr in);
# endif
# define inetnetof inet_netof
#endif
#if !defined(HAVE_ARPA_INET_H)
extern unsigned long int inet_addr (const char *cp);
extern int inet_aton (const char *cp, struct in_addr *addr);
extern int inet_netof (struct in_addr in);
extern char *inet_ntoa (struct in_addr in);
#endif

/*  Signals portability problems.
 */

#ifdef HPUX
# ifndef SIGWINCH  /*pre 9.0*/
#  define SIGWINCH SIGWINDOW
# endif
#endif

#if defined(BSD_RELIABLE_SIGNALS) || defined(POSIX_SIGNALS)
#define	HAVE_RELIABLE_SIGNALS
#endif

/*  ctime portability problems.
 */

#if defined(HPUX) && defined(__STDC__)
# define ctime(x) (ctime((const time_t *)(x)))
#endif

/*  getsockopt portability problems.
 */

#ifdef apollo
# undef IP_OPTIONS  /* Defined in /usr/include/netinet/in.h but doesn't work */
#endif

/*  setlinebuf portability problems.
 */

#if defined(HPUX) && !defined(SYSV) && !defined(SVR4) || defined(__CYGWIN32__)
# define setlinebuf(x) (setvbuf((x), NULL, _IOLBF, BUFSIZ))
#endif


/*  gethostbyname portability problems.
 */

#ifdef SOLARIS_2_0_2_1_2_2
/* 
 * On Solaris 2.0, 2.1 and 2.2 (SunOS 5.0, 5.1 and 5.2) systems,
 * gethostbyname() has a bug, it always returns null in h->aliases.
 * Workaround: use the undocumented __switch_gethostbyname(...).
 */
extern struct hostent *__switch_gethostbyname (const char *name);
#define gethostbyname __switch_gethostbyname
#endif

#ifdef SOLARIS_2_3
/* 
 * On Solaris 2.3 (SunOS 5.3) systems, gethostbyname() has a bug, it always
 * returns null in h->aliases.  Workaround: use the undocumented
 * _switch_gethostbyname_r(...).
 */
extern struct hostent *_switch_gethostbyname_r (const char *name,
						    struct hostent *hp,
						    char *buf, int size,
						    int *h_errno);
#define gethostbyname solaris_gethostbyname
#endif

/*  Resolver portability problems.
 */

#ifdef __m88k__
# define __BIND_RES_TEXT
#endif

#ifndef NETDB_INTERNAL          /* defined in latest BIND's <netdb.h>   */
# define NETDB_INTERNAL  -1     /* but not in every vendors' <netdb.h>  */
#endif

/*  getrusage portability problems.
 */

#if defined(HPUX) && !defined(HAVE_GETRUSAGE)
# define getrusage(a,b) (syscall(SYS_GETRUSAGE, (a), (b)))
# define HAVE_GETRUSAGE 1
#endif

/*  select portability problems - some systems do not define FD_... macros; on
 *  some systems (for example HPUX), select uses an int * instead of an
 *  fd_set * for its 2nd, 3rd and 4th arguments.
 *
 *  Note. This test should be more portable and put in configure ... but I've
 *        no idea on how to build a test for configure that will guess if the
 *        system uses int * or fd_set * inside select(). If you've some idea,
 *        please tell it to me. :)                   -- Alain.Nissen@ulg.ac.be
 */

#if !defined(USE_POLL)
# ifndef FD_ZERO
#  define FD_ZERO(set)      (((set)->fds_bits[0]) = 0)
#  define FD_SET(s1, set)   (((set)->fds_bits[0]) |= 1 << (s1))
#  define FD_ISSET(s1, set) (((set)->fds_bits[0]) & (1 << (s1)))
#  define FD_SETSIZE        30
# endif /* FD_ZERO */
# if defined(HPUX) && (! defined(_XPG4_EXTENDED) || (defined(_XPG4_EXTENDED) && defined(__INCLUDE_FROM_TIME_H) && !defined(_XOPEN_SOURCE_EXTENDED)))
#  define SELECT_FDSET_TYPE int
# else
#  define SELECT_FDSET_TYPE fd_set
# endif
#else /* should not be here - due to irc/c_bsd.c that does not support poll */
# define SELECT_FDSET_TYPE fd_set
#endif /* USE_POLL */

/*  <sys/wait.h> POSIX.1 portability problems - HAVE_SYS_WAIT_H is defined
 *  only if <sys/wait.h> is compatible with POSIX.1 - if not included, a
 *  prototype must be supplied for wait (wait3 and waitpid are unused here).
 */

#if !defined(HAVE_SYS_WAIT_H)
# ifdef USE_UNION_WAIT
extern pid_t wait (union wait *);
# else
extern pid_t wait (int *);
# endif
#endif

/*  <sys/socket.h> portability problems - X/Open SPEC 1170 specifies that
 *  the length parameter of socket operations must be a size_t
 *  (resp. size_t *), instead of an int (resp. int *); the problem is that
 *  only a few systems (for example AIX 4.2) follow this specification; others
 *  systems still use int (resp. int *), which is not always equal to size_t
 *  (resp. size_t *).
 *
 *  And then POSIX changed it again to a socklen_t, which seems to be an
 *  unsigned int in most cases.
 *
 *  This is now attempted to be detected by configure, we fall back to
 *  socklen_t if configure failed and socklen_t was found.  Else it's using
 *  the old code.
 *
 */

#if defined(ACCEPT_TYPE_ARG3)
# define SOCK_LEN_TYPE ACCEPT_TYPE_ARG3
#elif defined(HAVE_SOCKLEN_T)
# define SOCK_LEN_TYPE socklen_t
#elif defined(AIX) && defined(_XOPEN_SOURCE_EXTENDED) && _XOPEN_SOURCE_EXTENDED
# define SOCK_LEN_TYPE size_t
#else
# define SOCK_LEN_TYPE int
#endif

/*  Stupid typo in AIX 3.2's <sys/stropts.h>.
 */

#ifdef AIX_3_2
# ifdef _IO
# undef _IO
# endif
# define _IO(x,y) (IOC_VOID|((x)<<8)|(y))
#endif

/*  These macros may be broken.
 */

#ifdef STAT_MACROS_BROKEN
# ifdef S_ISFIFO
# undef S_ISFIFO
# endif
# define S_ISFIFO(m)	(((m)&(_S_IFMT)) == (_S_IFIFO))
# ifdef S_ISDIR
# undef S_ISDIR
# endif
# define S_ISDIR(m)	(((m)&(_S_IFMT)) == (_S_IFDIR))
# ifdef S_ISCHR
# undef S_ISCHR
# endif
# define S_ISCHR(m)	(((m)&(_S_IFMT)) == (_S_IFCHR))
# ifdef S_ISBLK
# undef S_ISBLK
# endif
# define S_ISBLK(m)	(((m)&(_S_IFMT)) == (_S_IFBLK))
# ifdef S_ISREG
# undef S_ISREG
# endif
# define S_ISREG(m)	(((m)&(_S_IFMT)) == (_S_IFREG))
#endif

/*  These constants may be missing.
 */

#ifndef NULL
#define NULL ((void *)0)
#endif
#ifdef FALSE
#undef FALSE
#endif
#define FALSE (0)
#ifdef TRUE
#undef TRUE
#endif
#define TRUE (!FALSE)

/*  These macros may be missing.
 */

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

/*  These external may be missing.
 */

#if !defined(SYS_ERRLIST_DECLARED)
extern char *sys_errlist[];
#endif

#if !defined(SYS_NERR_DECLARED)
extern int sys_nerr;
#endif

#if !defined(ERRNO_DECLARED)
extern int errno;
#endif

#if !defined(H_ERRNO_DECLARED)
extern int h_errno;
#endif

/*
 *  IPv4 or IPv6 structures?
 */

#ifdef INET6

# define AND16(x) ((x)[0]&(x)[1]&(x)[2]&(x)[3]&(x)[4]&(x)[5]&(x)[6]&(x)[7]&(x)[8]&(x)[9]&(x)[10]&(x)[11]&(x)[12]&(x)[13]&(x)[14]&(x)[15])
extern unsigned char minus_one[];
# define WHOSTENTP(x) ((x)[0]|(x)[1]|(x)[2]|(x)[3]|(x)[4]|(x)[5]|(x)[6]|(x)[7]|(x)[8]|(x)[9]|(x)[10]|(x)[11]|(x)[12]|(x)[13]|(x)[14]|(x)[15])

# define	AFINET		AF_INET6
# define	SOCKADDR_IN	sockaddr_in6
# define	SOCKADDR	sockaddr
# define	SIN_FAMILY	sin6_family
# define	SIN_PORT	sin6_port
# define	SIN_ADDR	sin6_addr
# define	S_ADDR		s6_addr
# define	IN_ADDR		in6_addr

#ifndef INET6_ADDRSTRLEN   
#define INET6_ADDRSTRLEN   46
#endif

#else /* INET6 */

# define	AFINET		AF_INET
# define	SOCKADDR_IN	sockaddr_in
# define	SOCKADDR	sockaddr
# define	SIN_FAMILY	sin_family
# define	SIN_PORT	sin_port
# define	SIN_ADDR	sin_addr
# define	S_ADDR		s_addr
# define	IN_ADDR		in_addr

# define WHOSTENTP(x) (x)
#endif

/* Courtesy of AC_CHECK_TYPES autoconf macro in configure */
#if !defined(HAVE_INT16_T)
typedef short int16_t;
#endif
#if !defined(HAVE_INT32_T)
typedef int int32_t;
#endif
#if !defined(HAVE_INT8_T)
typedef char int8_t;
#endif
#if !defined(HAVE_U_CHAR)
typedef unsigned char u_char;
#endif
#if !defined(HAVE_U_INT)
typedef unsigned int u_int;
#endif
#if !defined(HAVE_U_INT32_T)
typedef unsigned int u_int32_t;
#endif
#if !defined(HAVE_U_INT16_T)
typedef unsigned short u_int16_t;
#endif
#if !defined(HAVE_U_INT8_T)
typedef unsigned char u_int8_t;
#endif
#if !defined(HAVE_U_LONG)
typedef unsigned long u_long;
#endif
#if !defined(HAVE_U_SHORT)
typedef unsigned short u_short;
#endif

