/************************************************************************
 *   IRC - Internet Relay Chat, common/common_def.h
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

#define DupString(x,y) do {x = (char *)MyMalloc(strlen((char *)y) + 1);\
			   (void)strcpy((char *)x, (char *)y);\
			  } while(0)

#undef tolower
#define tolower(c) (tolowertab[(u_char)(c)])

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

#define PRINT	0x0001
#define CNTRL	0x0002
#define ALPHA	0x0004
#define PUNCT	0x0008
#define DIGIT	0x0010 
#define SPACE	0x0020
#define NVALID	0x0040
#define UVALID	0x0080

#define isalpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define isspace(c) (char_atribs[(u_char)(c)]&SPACE)
#define islower(c) ((char_atribs[(u_char)(c)]&ALPHA) && \
		    ((u_char)(c) > (u_char)0x5f))
#define isupper(c) ((char_atribs[(u_char)(c)]&ALPHA) && \
		    ((u_char)(c) < (u_char)0x60))
#define isdigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	isxdigit(c) (isdigit(c) || \
		     ((u_char)'a' <= (u_char)(c) && \
		      (u_char)(c) <= (u_char)'f') || \
		     ((u_char)'A' <= (u_char)(c) && \
		      (u_char)(c) <= (u_char)'F'))
#define isalnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define isprint(c) (char_atribs[(u_char)(c)]&PRINT)
#define isascii(c) ((u_char)(c) <= (u_char)0x7f)
#define isgraph(c) ((char_atribs[(u_char)(c)]&PRINT) && \
		    ((u_char)(c) != (u_char)0x20))
#define ispunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))
#define isvalidnick(c) (char_atribs[(u_char)(c)]&NVALID)
#define isvaliduser(c) (char_atribs[(u_char)(c)]&UVALID)

#ifdef DEBUGMODE
# define Debug(x) debug x
# define DO_DEBUG_MALLOC
#else
# define Debug(x) ;
# define LOGFILE "/dev/null"
#endif

#if defined(CHKCONF_COMPILE) || defined(CLIENT_COMPILE)
#undef	ZIP_LINKS
#endif
