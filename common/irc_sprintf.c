/*
 *   IRC - Internet Relay Chat, ircd/irc_sprintf.c
 *   Copyright (C) 2002 Piotr Kucharski
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
static  char rcsid[] = "@(#)$Id: irc_sprintf.c,v 1.1 2002/07/30 23:12:21 chopin Exp $";
#endif

#define IRC_SPRINTF_C
#include "irc_sprintf_ext.h"
#undef IRC_SPRINTF_C

/*
 * Our sprintf, only basic support (%s and %d is 95% we use), plus
 * handles %y/%Y, which return (for an aClient pointer) ->name or
 * ->uid, depending on target
 */

int irc_sprintf(aClient *target, char *buf, char *format, ...)
{
	va_list ap;
	int i;

	va_start(ap, format);
	i = vsprintf(buf, format, ap);
	va_end(ap);

	return i;
}
