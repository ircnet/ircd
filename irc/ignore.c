/************************************************************************
 *   IRC - Internet Relay Chat, irc/ignore.c
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

char ignore_id[]="ignore.c v2.0 (c) 1988, 1989 Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "irc.h"

anIgnore *ignore = (anIgnore *) 0;
char ibuf[80];

int kill_ignore();
int add_ignore();

void do_ignore(user, temp)
char *user, *temp;
{
  char *ch, *wild = "*";
  anIgnore *iptr;
  char *apu = user, *uh;
  int status;
  if ((user == (char *) 0) || (*user == '\0')) {

    putline("*** Current ignore list entries:");
    for (iptr = ignore; iptr; iptr = iptr->next) {
      sprintf(ibuf,"    Ignoring %s messages from user %s!%s", 
	      (iptr->flags == IGNORE_TOTAL) ? "all" :
	      (iptr->flags == IGNORE_PRIVATE) ? "private" : "public", 
	      iptr->user, iptr->from);
      putline(ibuf);
    }
    putline("*** End of ignore list entries");
    return;
  }
  while (apu && *apu) {
    ch = apu;
    if (*ch == '+') {
      ch++;
      status = IGNORE_PUBLIC;
    }
    else if (*ch == '-') {
      ch++;
      status = IGNORE_PRIVATE;
    }
    else 
      status = IGNORE_TOTAL;
    if ((apu = index(ch, ',')))
      *(apu++) = '\0';
    if ((uh = index(ch, '!')))
      *uh++ = '\0';
    else if ((uh = index(ch, '@')))
      *uh++ = '\0';
    else
      uh = wild;
    if (!*ch)
      ch = wild;
    if ((iptr = find_ignore(ch, (anIgnore *)NULL, uh))) {
      sprintf(ibuf,"*** Ignore removed: user %s!%s",
      iptr->user, iptr->from);
      putline(ibuf);
      kill_ignore(iptr);
    } else {
      if (strlen(ch) > (size_t) NICKLEN)
	ch[NICKLEN] = '\0';
      if (add_ignore(ch, status, uh) >= 0) {
	sprintf(ibuf,"*** Ignore %s messages from user %s!%s", 
		(status == IGNORE_TOTAL) ? "all" :
		 (status == IGNORE_PRIVATE) ? "private" : "public", ch, uh);
	putline(ibuf);
      } else
	putline("Fatal Error: Cannot allocate memory for ignore buffer");
    }
  }
}    

anIgnore *find_ignore(user, para, fromhost)
char *user, *fromhost;
anIgnore *para;
{
  anIgnore *iptr;
  for (iptr = ignore; iptr; iptr=iptr->next)
    if ((matches(iptr->user, user) == 0) &&
	(matches(iptr->from, fromhost)==0))
      break;

  return iptr ? iptr : para;
}

int kill_ignore(iptr)
anIgnore *iptr;
{
  anIgnore *i2ptr, *i3ptr = (anIgnore *) 0;
  for (i2ptr = ignore; i2ptr; i2ptr = i2ptr->next) {
    if (i2ptr == iptr)
      break;
    i3ptr = i2ptr;
  }
  if (i2ptr) {
    if (i3ptr)
      i3ptr->next = i2ptr->next;
    else
      ignore = i2ptr->next;
    free(i2ptr);
    return (1);
  }
  return (-1);
}

int add_ignore(ch, status, fromhost)
char *ch, *fromhost;
int status;
{
  anIgnore *iptr;
  iptr = (anIgnore *) malloc(sizeof (anIgnore));
  if (iptr == (anIgnore *) 0)
    return(-1);
  strncpyzt(iptr->user, ch, sizeof(iptr->user));
  strncpyzt(iptr->from, fromhost, sizeof(iptr->from));
  iptr->next = ignore;
  ignore = iptr;
  iptr->flags = status;
  return(1);
}
