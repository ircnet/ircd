/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_log_def.h
 *   Copyright (C) 1998 Christophe Kalt
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

#if defined(IAUTH_DEBUG)
# define DebugLog(x)	sendto_log x
#else
# define DebugLog(x)	;
#endif

#define	ALOG_FLOG	0x01	/* file log */
#define	ALOG_IRCD	0x02	/* notice sent to ircd (then sent to &AUTH) */

#define	ALOG_DCONF	0x000100	/* debug: configuration file */
#define	ALOG_DMISC	0x000200	/* debug: misc stuff */
#define	ALOG_DIO	0x000400	/* debug: IO stuff */
#define	ALOG_DSPY	0x001000	/* debug: show ircd stream */
#define	ALOG_DIRCD	0x002000	/* debug: errors reported by ircd */

#define	ALOG_D931	0x010000	/* debug: module rfc931 */
#define	ALOG_DSOCKS	0x020000	/* debug: module socks */
#define	ALOG_DSOCKSC	0x040000	/* debug: module socks cache */
#define	ALOG_DPIPE	0x080000	/* debug: module pipe */
#define	ALOG_DLHEX	0x100000	/* debug: module pipe */

#define	ALOG_DALL	0x1F3700	/* any debug flag */
