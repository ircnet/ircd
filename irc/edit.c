/************************************************************************
 *   IRC - Internet Relay Chat, irc/edit.c
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
static  char rcsid[] = "@(#)$Id: edit.c,v 1.2 1997/09/03 17:45:37 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define EDIT_C
#include "c_externs.h"
#undef EDIT_C

#define FROM_START 0
#define FROM_END   1
#define RELATIVE   2

static int esc=0;
static int literal=0;

int do_char(ch)
char ch;
{
	static	int	first_time=0;

	if (!first_time) {
		toggle_ins();
		toggle_ins();
		first_time=1;
#ifdef DOCURSES
		if (termtype == CURSES_TERM)
			refresh();
#endif
	}
	if (esc == 1) {
		do_after_esc(ch);
		return tulosta_viimeinen_rivi();
	}
	switch (ch)
	{
	case '\000':		/* NULL */
		break;
	case '\001':		/* ^A */
		bol();			/* beginning of line */
		break;
	case '\002':		/* ^B */
		back_ch();		/* backward char */
		break;
	case '\003':		/* ^C */
		rev_line();		/* reverse line */
		break;
	case '\004':		/* ^D */
		del_ch_right();		/* delete char from right */
		break;
	case '\005':		/* ^E */
		eol();			/* end of line */
		break;
	case '\006':		/* ^F */
		forw_ch();		/* forward char */
		break;
	case '\007':		/* ^G */
		add_ch(ch);		/* bell */
		break;
	case '\010':		/* ^H */
		del_ch_left();		/* delete char to left */
		break;
	case '\011':		/* TAB */
		toggle_ins();		/* toggle insert mode */
		break;
	case '\012':		/* ^J */
		send_this_line();	/* send this line */
		break;
	case '\013':		/* ^K */
		kill_eol();		/* kill to end of line */
		break;
	case '\014':		/* ^L */
		refresh_screen();	/* refresh screen */
		write_statusline();
		break;
	case '\015':		/* ^M */
		send_this_line();	/* send this line */
		break;
	case '\016':		/* ^N */
		next_in_history();	/* next in history */
		break;
	case '\017':		/* ^O */
		break;
	case '\020':		/* ^P */
		previous_in_history();	/* previous in history */
		break;
	case '\021':		/* ^Q */
		break;
	case '\022':		/* ^R */
	case '\023':		/* ^S */
	case '\024':		/* ^T */
		break;
	case '\025':		/* ^U */
		kill_whole_line();	/* kill whole line */
		break;
	case '\026':		/* ^V */
		literal_next();		/* literal next */
		break;
	case '\027':		/* ^W */
		del_word_left();        /* delete word left */
		break;
	case '\030':		/* ^X */
		break;
	case '\031':		/* ^Y */
		yank();			/* yank */
		break;
	case '\032':		/* ^Z */
		suspend_irc(0);		/* suspend irc */
		break;
	case '\033':		/* ESC */
		got_esc();
		break;
	case '\177':		/* DEL */
		del_ch_left();		/* delete char to left */
		break;
	default:
		add_ch(ch);
		break;
	}
	return tulosta_viimeinen_rivi();
}

void bol()
{
	set_position(0, FROM_START);
}

void eol()
{
	set_position(0, FROM_END);
	set_position(1, RELATIVE);
}

void back_ch()
{
	set_position(-1, RELATIVE);
}

void forw_ch()
{
	set_position(1, RELATIVE);
}

void rev_line()
{
	int	i1, i2, i3, i4;

	i4 = get_position();
	set_position(0, FROM_START);
	i1 = get_position();
	set_position(0, FROM_END);
	i1 = get_position()-i1;
	set_position(i4, FROM_START);

	for (i2 = 0; i2 > i1/2; i2++) {
		i3 = get_char(i2);
		set_char(i2, get_char(i1-i2-1));
		set_char(i1-i2-1, i3);
	}
}

void del_ch_right()
{
	int	i1, i2, i3;

	i1 = get_position();

	if (!get_char(i1))
		return;			/* last char in line */
	set_position(0, FROM_END);
	i2 = get_position();
	for (i3 = i1; i3 < i2; i3++)
		set_char(i3, get_char(i3+1));
	set_char(i3, 0);
	set_position(i1, FROM_START);
}

void del_ch_left()
{
	int	i1, i2, i3;

	i1 = get_position();

	if (!i1)
		return;			/* first pos in line */
	set_position(0, FROM_END);
	i2 = get_position();
	for (i3 = i1-1; i3 < i2; i3++)
		set_char(i3, get_char(i3+1));
	set_char(i3, 0);
	set_position(i1, FROM_START);
	set_position(-1, RELATIVE);
}

RETSIGTYPE suspend_irc(s)
int s;
{
#ifdef SIGTSTP
	signal(SIGTSTP, suspend_irc);
# ifdef DOCURSES
                if (termtype == CURSES_TERM) {
                        echo();
                        nocrmode();
                } 
# endif /* DOCURSES */
# ifdef DOTERMCAP
                if (termtype == TERMCAP_TERM)
                        io_off();
# endif /* DOTERMCAP */
# ifdef SIGSTOP
	kill(getpid(), SIGSTOP);
# endif /* SIGSTOP */
# ifdef DOCURSES
                if (termtype == CURSES_TERM) {
                        /* initscr(); */
                        noecho();
                        crmode();
                        clear();
                        refresh();
                }
# endif /* DOCURSES */
# ifdef DOTERMCAP
                if (termtype == TERMCAP_TERM) {
                        io_on(1);
                        clearscreen();
                }
# endif /* DOTERMCAP */
		write_statusline();
#else /* || */
# if !defined(SVR3)
	tstp(); 
# endif
#endif /* || */
}

void got_esc()
{
	esc = 1;
}

void do_after_esc(ch)
char ch;
{
	if (literal) {
		literal = 0;
		add_ch(ch);
		return;
	}
	esc = 0;
	switch (ch)
	{
	case 'b':
		word_back();
		break;
	case 'd':
		del_word_right();
		break;
	case 'f':
		word_forw();
		break;
	case 'y':
		yank();
		break;
	case '\177':
		del_word_left();
		break;
	default:
		break;
	}
}

void refresh_screen()
{
#ifdef DOCURSES
	if (termtype == CURSES_TERM) {
		clearok(curscr, TRUE);
		refresh();
	}
#endif
}

void add_ch(ch)
int	ch;
{
	int	i1, i2, i3;

	if (in_insert_mode()) {
		i1 = get_position();
		set_position(0, FROM_END);
		i2 = get_position();
		for (i3 = i2; i3 >= 0; i3--)
			set_char(i1+i3+1, get_char(i3+i1));
		set_char(i1, ch);
		set_position(i1, FROM_START);
		set_position(1, RELATIVE);
	} else {
		i1 = get_position();
		set_char(i1, ch);
		set_position(i1, FROM_START);
		set_position(1, RELATIVE);
	}
}

void literal_next()
{
	got_esc();
	literal=1;
}

void word_forw()
{
	int	i1,i2;

	i1 = get_position();

	while ((i2 = get_char(i1)))
		if ((i2 == (int)' ') || (i2 == (int)'\t') ||
		    (i2 == (int)'_') || (i2 == (int)'-'))
			i1++;
		else
			break;
	while ((i2 = get_char(i1)))
		if ((i2 == (int)' ') || (i2 == (int)'\t') ||
		    (i2 == (int)'_') || (i2 == (int)'-'))
			break;
		else
			i1++;
	set_position(i1, FROM_START);
}

void word_back()
{
	int	i1,i2;

	i1 = get_position();
	if (i1 != 0)
		i1--;

	while ((i2 = get_char(i1)))
		if ((i2 == (int)' ') || (i2 == (int)'\t') ||
		    (i2 == (int)'_') || (i2 == (int)'-'))
			i1--;
		else
			break;
	while ((i2 = get_char(i1)))
		if ((i2 == (int)' ') || (i2 == (int)'\t') ||
		    (i2 == (int)'_') || (i2 == (int)'-'))
			break;
		else
			i1--;
	if (i1 <= 0)
		i1 = 0;
	else
		i1++;
	set_position(i1, FROM_START);
}

void del_word_left()
{
	int	i1, i2, i3, i4;

	i1 = get_position();
	word_back();
	i2 = get_position();
	set_position(0, FROM_END);
	i3 = get_position();
	for(i4 = i2; i4 <= i3 - (i1 - i2); i4++)
		set_char(i4, get_char(i4 + (i1 - i2)));
	for(; i4 <= i3; i4++)
		set_char(i4, (int)'\0');
	set_position(i2, FROM_START);
}

void del_word_right()
{
	int	i1, i2, i3, i4;

	i2 = get_position();
	word_forw();
	i1 = get_position();
	set_position(0, FROM_END);
	i3 = get_position();
	for(i4 = i2; i4 <= i3 - (i1 - i2); i4++)
		set_char(i4, get_char(i4 + (i1 - i2)));
	for(; i4 <= i3; i4++)
		set_char(i4, (int)'\0');
	set_position(i2, FROM_START);
}


