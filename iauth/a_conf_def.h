/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_conf_def.h
 *   Copyright (C) 1998 Christophe Kalt
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

typedef struct Module aModule;
typedef struct Instance AnInstance;
typedef struct Target aTarget;

struct Module
{
    char	*name;			/* module name */
    char	*(*init)(AnInstance *);	/* instance initialization */
    void	(*release)(AnInstance *);/* instance releasing >UNUSED< */
    void	(*stats)(AnInstance *);	/* send instance stats to ircd */
    int		(*start)(u_int);	/* start authentication */
    int		(*work)(u_int);		/* called whenever something has to be
					 * done (incoming data, timeout..) */
    int		(*timeout)(u_int);	/* called when timeout is reached */
    void	(*clean)(u_int);	/* finish/abort: cleanup*/
};

struct Instance
{
    AnInstance	*nexti;
    aModule	*mod;			/* module */
    char	*opt;			/* options read from file */
    char	*popt;			/* options to send to ircd */
    void	*data;			/* private data: stats, ... */
    aTarget	*address;
    aTarget	*hostname;
};

struct Target
{
    char	*value;
    aTarget	*nextt;
};
