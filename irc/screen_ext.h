/************************************************************************
 *   IRC - Internet Relay Chat, irc/screen_ext.h
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
    defined in irc/screen.c.
 */

/*  External definitions for global variables.
 */
#ifndef SCREEN_C
extern char screen_id[];
extern int insert;
#endif /* SCREEN_C */

/*  External definitions for global functions.
 */
#ifndef SCREEN_C
#define EXTERN extern
#else /* SCREEN_C */
#define EXTERN
#endif /* SCREEN_C */
EXTERN int get_char (int pos);
EXTERN void set_char (int pos, int ch);
EXTERN int get_yank_char (int pos);
EXTERN void set_yank_char (int pos, int ch);
EXTERN void set_position (int disp, int from);
EXTERN int get_position(void);
EXTERN void toggle_ins(void);
EXTERN int in_insert_mode(void);
EXTERN void send_this_line(void);
EXTERN void record_line(void);
EXTERN void clear_last_line(void);
EXTERN void kill_eol(void);
EXTERN void next_in_history(void);
EXTERN void previous_in_history(void);
EXTERN void kill_whole_line(void);
EXTERN void yank(void);
EXTERN int tulosta_viimeinen_rivi(void);
EXTERN int get_disp (int paikka);
#undef EXTERN
