/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_zip_ext.h
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
    defined in ircd/s_zip.c.
 */

/*  External definitions for global functions.
 */
#ifndef S_ZIP_C
#define EXTERN extern
#else /* S_ZIP_C */
#define EXTERN
#endif /* S_ZIP_C */
#ifdef	ZIP_LINKS
EXTERN int zip_init __P((aClient *cptr));
EXTERN void zip_free __P((aClient *cptr));
EXTERN char *unzip_packet __P((aClient *cptr, char *buffer, int *length));
EXTERN char *zip_buffer __P((aClient *cptr, char *buffer, int *length,
			     int flush));
#endif /* ZIP_LINKS */
#undef EXTERN
