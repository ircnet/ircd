/* $Id: acconfig.h,v 1.3 1997/07/22 12:40:06 kalt Exp $ */

/* Define if zlib package must be used for compilation/linking. */
#undef USE_ZLIB

/* Define if ncurses library must be used for compilation/linking. */
#undef USE_NCURSES

/* Define if curses library must be used for compilation/linking. */
#undef USE_CURSES

/* Define if termcap library must be used for compilation/linking. */
#undef USE_TERMCAP

/* Define if the second argument of waitpid must be an "union wait *" instead
   of an "int *". */
#undef USE_UNION_WAIT

/* Define if the operating system is Solaris 2.3 (SunOS 5.3).  */
#undef SOLARIS_2_3

/* Define if the operating system is Solaris 2.2 (SunOS 5.2).  */
#undef SOLARIS_2_2

/* Define if sys_errlist is declared in stdio.h or errno.h. */
#undef SYS_ERRLIST_DECLARED

/* Define if the system provides POSIX sigaction. */
#undef POSIX_SIGNALS

/* Define if the system provides reliable BSD signals through sigset instead
   of signal. */
/* #define signal sigset */

/* Define if the system provides reliable BSD signals. */
#undef BSD_RELIABLE_SIGNALS

/* Define if the system provides unreliable SystemV signals. */
#undef SYSV_UNRELIABLE_SIGNALS

/* Define if the system provides POSIX non-blocking system. */
#undef NBLOCK_POSIX

/* Define if the system provides BSD non-blocking system. */
#undef NBLOCK_BSD

/* Define if the system provides SystemV non-blocking system. */
#undef NBLOCK_SYSV

/* Define is the system can use variable arguments. */
#undef USE_STDARG

/* Define if you do not have the index function. */
#undef NOINDEX

/* Define if the strerror function must be provided by the source code. */
#undef NEED_STRERROR

/* Define if the strtoken function must be provided by the source code. */
#undef NEED_STRTOKEN

/* Define if the strtok function must be provided by the source code. */
#undef NEED_STRTOK

/* Define if the inet_addr function must be provided by the source code. */
#undef NEED_INET_ADDR

/* Define if the inet_aton function must be provided by the source code. */
#undef NEED_INET_ATON

/* Define if the inet_netof function must be provided by the source code. */
#undef NEED_INET_NETOF

/* Define if the inet_ntoa function must be provided by the source code. */
#undef NEED_INET_NTOA

/* Define if you have the memmove. */
#undef HAVE_MEMMOVE

/* Define if the bcopy function must be provided by the source code. */
#undef NEED_BCOPY

/* Define if the bzero function must be provided by the source code. */
#undef NEED_BZERO

/* Define if the bcmp function must be provided by the source code. */
#undef NEED_BCMP

/* Define if you have the getrusage function. */
#undef GETRUSAGE_2

/* Define if you have the times function. */
#undef TIMES_2
