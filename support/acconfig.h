/* $Id: acconfig.h,v 1.7 1998/09/24 17:34:47 kalt Exp $ */

/* Define if zlib package must be used for compilation/linking. */
#undef USE_ZLIB

/* Define if ncurses library must be used for compilation/linking. */
#undef USE_NCURSES

/* Define if cursesX library must be used for compilation/linking. */
#undef USE_CURSESX

/* Define if curses library must be used for compilation/linking. */
#undef USE_CURSES

/* Define if termcap library must be used for compilation/linking. */
#undef USE_TERMCAP

/* Define if the second argument of waitpid must be an "union wait *" instead
   of an "int *". */
#undef USE_UNION_WAIT

/* Define if int8_t, u_int8_t, int16_t, u_int16_t, int32_t, u_int32_t, u_char,
 * u_short, u_int, u_long are not known types. */
#undef int8_t
#undef u_int8_t
#undef int16_t
#undef u_int16_t
#undef int32_t
#undef u_int32_t
#undef u_char
#undef u_short
#undef u_int
#undef u_long

/* Define if memcmp is not 8-bit clean. */
#undef MEMCMP_BROKEN

/* Define if the operating system is AIX 3.2.  */
#undef AIX_3_2

/* Define if the operating system is Solaris 2.x (SunOS 5.x).  */
#undef SOLARIS_2

/* Define if the operating system is Solaris 2.3 (SunOS 5.3).  */
#undef SOLARIS_2_3

/* Define if the operating system is Solaris 2.[0-2] (SunOS 5.[0-2]).  */
#undef SOLARIS_2_0_2_1_2_2

/* Define if <netdb.h> contains bad __const usages (Linux). */
#undef BAD___CONST_NETDB_H

/* Define if sys_errlist is declared in stdio.h or errno.h. */
#undef SYS_ERRLIST_DECLARED

/* Define if sys_nerr is declared in stdio.h or errno.h. */
#undef SYS_NERR_DECLARED

/* Define if errno is declared in errno.h. */
#undef ERRNO_DECLARED

/* Define if h_errno is declared in errno.h or netdb.h. */
#undef H_ERRNO_DECLARED

/* Define if poll(2) must be used instead of select(2). */
/* Note: some systems (e.g. linux 2.0.x) have a non-working poll() */
#undef USE_POLL

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

/* Define as the resolver configuration file. */
#undef IRC_RESCONF
