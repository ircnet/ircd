/************************************************************************
 *   IRC - Internet Relay Chat, irc/swear_ext.h
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
    defined in irc/swear.c.
 */

/*  External definitions for global variables.
 */
#ifndef SWEAR_C
#ifdef DOTERMCAP
extern char swear_id[];
extern int irc_lines, irc_columns, scroll_ok, scroll_status;
#endif
#endif /* SWEAR_C */

/*  External definitions for global functions.
 */
#ifndef SWEAR_C
#define EXTERN extern
#else /* SWEAR_C */
#define EXTERN
#endif /* SWEAR_C */
#ifdef DOTERMCAP
EXTERN void tcap_putch __P((int row, int col, char ch));
EXTERN void tcap_move __P((int row, int col));
EXTERN void clear_to_eol __P((int row, int col));
EXTERN void clearscreen();
EXTERN int io_on __P((int flag));
EXTERN int io_off();
EXTERN void scroll_ok_off();
EXTERN void scroll_ok_on();
EXTERN void put_insflag __P((int flag));
EXTERN void put_statusline();
EXTERN void tcap_putline __P((char *line));
#endif
#undef EXTERN
