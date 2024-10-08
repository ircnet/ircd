/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_sasl_ext.h
 *   Copyright (C) 2021 IRCnet.com team
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
    defined in ircd/s_sasl.c.
 */

/*  External definitions for global functions.
 */
#define EXTERN
EXTERN int process_implicit_sasl_abort(aClient *sptr);
#undef EXTERN
