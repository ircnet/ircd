/************************************************************************
 *   IRC - Internet Relay Chat, irc/help.c
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

char help_id[]="help.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "help.h"
#include "irc.h"

char helpbuf[80];

void do_help(ptr, temp)
char *ptr, *temp;
{
  struct Help *hptr;
  int count;

  if (BadPtr(ptr)) {
    sprintf(helpbuf, "*** Help: Internet Relay Chat v%s Commands:", version);
    putline(helpbuf);
    count = 0;
    for (hptr = helplist; hptr->command; hptr++) {
      sprintf(&helpbuf[count*10], "%10s", hptr->command);
      if (++count >= 6) {
	count = 0;
	putline(helpbuf);
      }
    }
    if (count)
      putline(helpbuf);
    putline("Type /HELP <command> to get help about a particular command.");
    putline("For example \"/HELP signoff\" gives you help about the");
    putline("/SIGNOFF command. To use a command you must prefix it with a");
    putline("slash or whatever your current command character is (see");
    putline("\"/HELP cmdch\"");
    putline("*** End Help");
  } else {
    for (hptr = helplist; hptr->command; hptr++) 
      if (mycncmp(ptr, hptr->command)) 
	break;

    if (hptr->command == (char *) 0) {
      putline("*** There is no help information for that command.");
      putline("*** Type \"/HELP\" to get a list of commands.");
      return;
    }
    sprintf(helpbuf, "*** Help: %s", hptr->syntax);
    putline(helpbuf);
    for (count = 0; count < 5; count++)
      if (hptr->explanation[count] && *(hptr->explanation[count])) {
	sprintf(helpbuf, "    %s", hptr->explanation[count]);
	putline(helpbuf);
      }
    putline("*** End Help");
  }
}

