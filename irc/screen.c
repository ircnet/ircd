/************************************************************************
 *   IRC - Internet Relay Chat, irc/screen.c
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

char screen_id[] = "screen.c v2.0 (c) 1988 University of Oulu, Computing\
 Center and Jarkko Oikarinen";

#include <stdio.h>
#include <curses.h>
#include "struct.h"
#include "common.h"
#include "irc.h"

#define	SBUFSIZ 240

#define	FROM_START 0
#define	FROM_END   1
#define	RELATIVE   2

#define	HIST_SIZ 1000

static	char	last_line[SBUFSIZ+1];
static	char	yank_buffer[SBUFSIZ+1];
static	char	history[HIST_SIZ][SBUFSIZ+1];
static	int	position = 0;
static	int	pos_in_history = 0;

int	insert = 1;	/* default to insert mode */
			/* I want insert mode, thazwhat emacs does ! //jkp */

int get_disp();
void record_line();
void clear_last_line();

extern int termtype;

int get_char(pos)
int pos;
{
    if (pos>=SBUFSIZ || pos<0)
	return 0;
    return (int)last_line[pos];
}

void set_char(pos, ch)
int pos, ch;
{
    if (pos<0 || pos>=SBUFSIZ)
	return;
    if (ch<0)
	ch=0;
    last_line[pos]=(char)ch;
}

int get_yank_char(pos)
int pos;
{
    if (pos>=SBUFSIZ || pos<0)
	return 0;
    return (int)yank_buffer[pos];
}

void set_yank_char(pos, ch)
int pos, ch;
{
    if (pos<0 || pos>=SBUFSIZ)
	return;
    if (ch<0)
	ch=0;
    yank_buffer[pos]=(char)ch;
}

void set_position(disp, from)
int disp, from;
{
    int i1;

    switch (from) {
    case FROM_START:
	position=disp;
	break;
    case RELATIVE:
	position+=disp;
	break;
    case FROM_END:
	for (i1=0; get_char(i1); i1++);
	position=i1-1;
	break;
    default:
	position=0;
	break;
    }
}

int get_position()
{
    return position;
}

void toggle_ins()
{
    insert = !insert;
#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      standout();
      if (insert)
	mvaddstr(LINES-2, 75, "INS");
      else
	mvaddstr(LINES-2, 75, "OWR");
      standend();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM)
      put_insflag(insert);
#endif
}

int in_insert_mode()
{
    return insert;
}

void send_this_line()
{
    record_line();
    sendit(last_line);
    clear_last_line();
    bol();
    tulosta_viimeinen_rivi();
#ifdef DOCURSES
    if (termtype == CURSES_TERM)
      refresh();
#endif
}

void record_line()
{
    static int place=0;
    int i1;

    for(i1=0; i1<SBUFSIZ; i1++)
	history[place][i1]=get_char(i1);
    place++;
    if (place==HIST_SIZ)
	place=0;
    pos_in_history=place;
}
    
void clear_last_line()
{
    int i1;

    for(i1=0; i1<SBUFSIZ; i1++)
	set_char(i1,(int)'\0');
}

void kill_eol()
{
    int i1, i2, i3;

    i1=get_position();
    set_position(0, FROM_END);
    i2=get_position();
    for(i3=0; i3<SBUFSIZ; i3++)
	set_yank_char(i3,(int)'\0');
    for(i3=0; i3<=(i2-i1); i3++) {
	set_yank_char(i3,get_char(i1+i3));
	set_char(i1+i3, 0);
    }
    set_position(i1, FROM_START);
}

void next_in_history()
{
    int i1;

    pos_in_history++;
    if (pos_in_history==HIST_SIZ)
	pos_in_history=0;
    clear_last_line();

    for (i1 = 0; history[pos_in_history][i1]; i1++) 
	set_char(i1, history[pos_in_history][i1]);

    set_position(0, FROM_START);
}

void previous_in_history()
{
    int i1;

    pos_in_history--;
    if (pos_in_history<0)
	pos_in_history=HIST_SIZ-1;
    clear_last_line();
    for (i1=0; history[pos_in_history][i1]; i1++) 
	set_char(i1, history[pos_in_history][i1]);

    set_position(0, FROM_START);
}

void kill_whole_line()
{
    clear_last_line();
    set_position(0, FROM_START);
}

void yank()
{
    int i1, i2, i3;
    
    i1=get_position();
    i2=0;
    while (get_yank_char(i2))
	i2++;
    
    for(i3=SBUFSIZ-1; i3>=i1+i2; i3--)
	set_char(i3, get_char(i3-i2));
    for(i3=0; i3<i2; i3++)
	set_char(i1+i3, get_yank_char(i3));
}

int tulosta_viimeinen_rivi()
{
    static int paikka=0;
    int i1, i2, i3;

    i1=get_position();
    /* taytyyko siirtaa puskuria */
    if (i1<(get_disp(paikka)+10) && paikka) {
	paikka--;
	i2=get_disp(paikka);
    } else if (i1>(get_disp(paikka)+70)) {
	paikka++;
	i2=get_disp(paikka);
    } else {
	i2=get_disp(paikka);
    }

#ifdef DOCURSES
    if (termtype == CURSES_TERM) {
      move(LINES-1,0);
      for(i3=0; i3<78; i3++)
        if (get_char(i2+i3))
	  mvaddch(LINES-1, i3, get_char(i2+i3));
      clrtoeol();
      move(LINES-1, i1-get_disp(paikka));
      refresh();
    }
#endif
#ifdef DOTERMCAP
    if (termtype == TERMCAP_TERM) {
      tcap_move(-1, 0);
      for(i3=0; i3<78; i3++)
        if (get_char(i2+i3))
	  tcap_putch(LINES-1, i3, get_char(i2+i3));
      clear_to_eol();
      tcap_move(-1, i1-get_disp(paikka));
      refresh();
    }
#endif
    return (i1-get_disp(paikka));
}

int get_disp(paikka)
int paikka;
{
    static int place[]={0,55,110,165,220};

    if (paikka>4 || paikka<0)
	return 0;
    return place[paikka];
}
