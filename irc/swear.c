/************************************************************************
 *   IRC - Internet Relay Chat, irc/swear.c
 *   Copyright (C) 1990 Jarkko Oikarinen
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

char swear_id[]="swear.c v2.0 (c) 1989 Jarkko Oikarinen";

/* Curses replacement routines. Uses termcap */

#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/file.h>
#include <ctype.h>
#include <strings.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#if HPUX
#include <sys/ttold.h>
#endif

#define LLEN 60

static struct sgttyb oldtty, newtty;
static char termcapentry[1024];
static char codes[1024], *cls;

extern char *tgoto(), *getenv();
static char *termname;

static int currow = 0;
int lines, columns, scroll_ok = 0, scroll_status = 0;
extern int insert;
extern char *HEADER;

tcap_putch(row, col, ch)
int row, col;
char ch;
{
  tcap_move(row, col);
  putchar(ch);
  fflush(stdout);
}

tcap_move(row, col)
int row, col;
{
  cls = codes;
  tgetstr("cm",&cls);
  if (row < 0)
    row = lines - row;
  cls = tgoto(codes, col, row);
  printf("%s",cls);
  fflush(stdout);
}

clear_to_eol(row, col)
int row, col;
{
  tcap_move(row, col);
  cls = codes;
  tgetstr("ce", &cls);
  printf("%s",codes);
  fflush(stdout);
}

clearscreen()
{
  cls = codes;
  tgetstr("cl",&cls);
  printf("%s",codes);
  fflush(stdout);
  currow = 0;
}
  
int
io_on(flag)
int flag;
{
/*  if (ioctl(0, TIOCGETP, &oldtty) == -1) {
    perror("ioctl");
    return(-1);
  }
  newtty = oldtty;
  newtty.sg_flags &= ~ECHO;
  newtty.sg_flags |= CBREAK;
  ioctl(0, TIOCSETP, &newtty); */
  system("stty -echo cbreak");
  if (tgetent(termcapentry,termname=getenv("TERM")) != 1) {
    printf("Cannot find termcap entry !\n");
    fflush(stdout);
  }
  printf("TERMCAP=%s\n",termcapentry);
  lines = tgetnum("li");
  columns = tgetnum("co");
  return(0);
}

int
io_off()
{
  if (scroll_ok)
    scroll_ok_off();
  if (ioctl(0, TIOCSETP, &oldtty) < 0)
    return(-1);
  return(0);
}

scroll_ok_off()
{
  cls = codes;
  tgetstr("cs",&cls);
  cls = tgoto(codes, lines-1, 0);
  printf("%s",cls); 
  scroll_ok = 0;
}

scroll_ok_on()
{
  cls = codes;
  tgetstr("cm",&cls);
  cls = tgoto(codes, 0, 0);
  printf("%s",cls);
  cls = codes;
  tgetstr("cs",&cls);
  cls = tgoto(codes, lines-3, 0);
  printf("%s",cls);
  fflush(stdout);
  scroll_ok = scroll_status = 1;
}

put_insflag(flag)
int flag;
{
  flag = insert;
  tcap_move(-2, columns - 5);
    cls = codes;
    tgetstr("mr",&cls);
    printf("%s",codes);
  printf((flag) ? "INS" : "OWR");
    cls = codes;
    tgetstr("me",&cls);
    printf("%s",codes);
  fflush(stdout);
}
  
put_statusline()
{
  tcap_move (-2, 0);
    cls = codes;
    tgetstr("mr",&cls);
    printf("%s",codes);
  printf(HEADER, version);
    cls = codes;
    tgetstr("me",&cls);
    printf("%s",codes);
  fflush(stdout);
}

tcap_putline(line)
char *line;
{
  char *ptr = line, *ptr2, *newl;
  char ch='\0';
  while (ptr) {
    if (strlen(ptr) > columns-1) {
      ch = ptr[columns-1];
      ptr[columns-1] = '\0';
      ptr2 = &ptr[columns-2];
    } 
    else
      ptr2 = NULL;
    if (scroll_ok) {
      tcap_move(lines-3, 0);
    } else {
      tcap_move(currow++,0);
      if (currow > lines - 4) currow = 0;
    }
    while (newl = index(ptr,'\n'))
      *newl = '\0';
    printf("%s",ptr);
    if (scroll_ok) 
      printf("\n",ptr); 
    else {
      if (currow == 0) {
	clear_to_eol(1,0);
	clear_to_eol(2,0);
      }
      else if (currow == lines - 4) {
	clear_to_eol(lines-4,0);
	clear_to_eol(0,0);
      }
      else {
	clear_to_eol(currow+1,0);
	clear_to_eol(currow+2,0);
      }
    }
    ptr = ptr2;
    if (ptr2) {
      *ptr2++ = '+';
      *ptr2 = ch;
    }
  }
  fflush(stdout);
}
