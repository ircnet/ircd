/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_struct_def.h
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


typedef struct AuthData anAuthData;

#define INBUFSIZE 4096		/* I/O buffer size */
#define MAXI 16			/* maximum number of instances */
#define BDSIZE ((MAXI + 7) / 8)	/* bit data size */

struct AuthData
{
	/* the following are set by a_io.c and may be read by modules */
	char	user[USERLEN+1];	/* username */
	char	passwd[PASSWDLEN+1];	/* password */
	char	host[HOSTLEN+1];	/* hostname */
	char	itsip[HOSTLEN+1];	/* client ip */
	u_short	itsport;		/* client port */
	char	ourip[HOSTLEN+1];	/* our ip */
	u_short	ourport;		/* our port */
	u_int	state;			/* state (general) */

	/* the following are set by modules */
	char	*authuser;		/* authenticated username */
	u_char	authfrom;		/* where we got authuser from */

	/* the following are for use by a_io.c only */
	char	idone[BDSIZE];		/* keeping track of instances' work */
	u_char	ileft;			/* time saver, anything left? */

	/* the following are shared by a_io.c & modules */
	char	*inbuffer;		/* input buffer */
	u_int	buflen;			/* length of data in buffer */
	int	rfd, wfd;		/* fd's */
	AnInstance	*instance;	/* the module instanciation working */
	u_int	mod_status;		/* used by the module only! */
	time_t	timeout;		/* timeout */
};

#define	A_ACTIVE	0x0001	/* entry is active */
#define	A_START		0x0002	/* go through modules from beginning */
#define	A_DONE		0x0004	/* nothing left to be done */
#define	A_IGNORE	0x0010	/* ignore subsequent messages from ircd */
#define	A_LATE		0x0080	/* ircd is no longer waiting for a reply */

#define	A_GOTU		0x0100	/* got username (from ircd) */
#define	A_GOTP		0x0200	/* got password (from ircd) */
#define	A_GOTH		0x0400	/* got hostname (from ircd) */
#define	A_NOH		0x0800	/* no hostname available */

#define A_UNIX		0x1000	/* authuser is suitable for use by ircd */
#define A_DENY		0x8000	/* connection should be denied access */

#define SetBit(v,n)	v[n/8] |=  (1 << (n % 8))
#define UnsetBit(v,n)	v[n/8] &= ~(1 << (n % 8))
#define CheckBit(v,n)	(v[n/8] & (1 << (n % 8)))
