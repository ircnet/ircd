/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_version.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: c_version.c,v 1.3 1998/12/13 00:02:35 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define C_VERSION_C
#include "c_externs.h"
#undef C_VERSION_C

char *intro = "Internet Relay Chat v%s";
char *version;
char *infotext[] =
    {
	"Original code written by Jarkko Oikarinen <jto@tolsun.oulu.fi>",
	"Copyright 1988,1989,1990 University of Oulu, Computing Center",
        "                         and Jarkko Oikarinen",
	0,
    };

char *HEADEROLD = 
"*Internet Relay Chat* Type /help to get help * Client v%s *                 ";

char *IRCHEADER =
" *IRC Client v%s* %10.10s on %10.10s */help for help*      ";

