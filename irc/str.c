/************************************************************************
 *   IRC - Internet Relay Chat, irc/str.c
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
static  char rcsid[] = "@(#)$Id: str.c,v 1.2 1997/09/03 17:45:43 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define STR_C
#include "c_externs.h"
#undef STR_C

char * center(buf,str,len)
char *buf, *str;
int len;
{
  char i,j,k;
  if ((i = strlen(str)) > len) {
    buf[len-1] = '\0';
    for(len--; len > 0; len--) buf[len-1] = str[len-1];
    return(buf);
  }
  j = (len-i)/2;
  for (k=0; k<j; k++) buf[k] = ' ';
  buf[k] = '\0';
  strcat(buf,str);
  for (k=j+i; k<len; k++) buf[k] = ' ';
  buf[len] = '\0';
  return (buf);
}

/* William Wisner <wisner@b.cc.umich.edu>, 16 March 1989 */
char *
real_name(user)
     struct passwd *user;
{
  char *bp, *cp;
  static char name[REALLEN+1];

  bp = user->pw_gecos;
  cp = name;

  name[REALLEN] = '\0';
  do {
    switch(*bp) {
    case '&':
      *cp = '\0';
      strncat(name, user->pw_name, REALLEN-strlen(name));
      name[REALLEN] = '\0';
      *cp = toupper(*cp);
      cp = index(name, '\0');
      bp++;
      break;
    case ',':
      *bp = *cp = '\0';
      break;
    case '\0':
      *cp = *bp;
      break;
    default:
      *cp++ = *bp++;
    }
  } while (*bp != '\0' && strlen(name) < (size_t) REALLEN);
  return(name);
}

