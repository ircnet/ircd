/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_conf.c
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

char conf_id[] = "conf.c v2.0 (c) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";

#include <stdio.h>
#include "struct.h"
#include "common.h"
#include "sys.h"

extern char *getfield();

initconf(host, passwd, myname, port)
char	*host, *passwd, *myname;
int	*port;
{
	FILE	*fd;
	char	line[256], *tmp;

	if ((fd = fopen(CONFIGFILE,"r")) == NULL)
		return /* (-1) */ ;
	while (fgets(line,255,fd)) {
		if (line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		switch (*getfield(line))
		{
		case 'C':   /* Server where I should try to connect */
		case 'c':   /* in case of link failures             */
		case 'I':   /* Just plain normal irc client trying  */
		case 'i':   /* to connect me */
		case 'N':   /* Server where I should NOT try to     */
		case 'n':   /* connect in case of link failures     */
			          /* but which tries to connect ME        */
		case 'O':   /* Operator. Line should contain at least */
		case 'o':   /* password and host where connection is  */
			          /* allowed from */
		case 'M':   /* Me. Host field is name used for this host */
		case 'm':   /* and port number is the number of the port */
		case 'a':
		case 'A':
		case 'k':
		case 'K':
		case 'q':
		case 'Q':
		case 'l':
		case 'L':
		case 'y':
		case 'Y':
		case 'h':
		case 'H':
		case 'p':
		case 'P':
			break;
		case 'U':   /* Uphost, ie. host where client reading */
		case 'u':   /* this should connect.                  */
			if (!(tmp = getfield(NULL)))
				break;
			strncpyzt(host, tmp, HOSTLEN);
			if (!(tmp = getfield(NULL)))
				break;
			strncpyzt(passwd, tmp, PASSWDLEN);
			if (!(tmp = getfield(NULL)))
				break;
			strncpyzt(myname, tmp, HOSTLEN);
			if (!(tmp = getfield(NULL)))
				break;
			if ((*port = atoi(tmp)) == 0)
				debug(DEBUG_ERROR,
				      "Error in config file, bad port field");
			break;    
		default:
/*      debug(DEBUG_ERROR, "Error in config file: %s", line); */
			break;
		}
	}
}
