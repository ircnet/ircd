/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_debug.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static  char rcsid[] = "@(#)$Id: c_debug.c,v 1.4 2002/07/29 21:36:07 chopin Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define C_DEBUG_C
#include "c_externs.h"
#undef C_DEBUG_C

struct	stats	ircst, *ircstp = &ircst;

#ifdef DEBUGMODE
void	debug(int level, char *form, ...)
{
  if (debuglevel >= 0)
    if (level <= debuglevel) {
      char buf[512];
      va_list va;
      va_start(va, form);
      (void)vsprintf(buf, form, va);
      va_end(va);
      putline(buf);
    }
}
#else /* do nothing */
void	debug()
{
}
#endif
