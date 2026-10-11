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

typedef struct ConfKV aConfKV;

struct ConfKV
{
    char	*key;
    char	*value;
    aConfKV	*next;
};


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

	/* Optional global (module-wide) lifecycle hooks */
	int (*ginit)(AnInstance *);     /* initialize persistent resources */
	void (*gtick)(AnInstance *);    /* invoked periodically */
	int (*gwork)(AnInstance *);     /* handle global events (if any) */
	void (*grelease)(AnInstance *); /* cleanup persistent resources */
};

struct Instance
{
    AnInstance	*nexti;
    u_char	in;			/* instance number */
    aModule	*mod;			/* module */
    char	*opt;			/* options read from file */
    char	*popt;			/* options to send to ircd */
    void	*data;			/* private data: stats, ... */
    aTarget	*address;
    aTarget	*hostname;
    u_int	timeout;
    u_int	port;
	u_char	wait_for_reg;   /* wait until client sent NICK/USER
 							   (and possibly CAP/AUTHENTICATE) */
	u_char	skip_if_sasl;   /* skip module if SASL authentication succeeded. */
	u_char  wait_for_ident; /* wait until ident lookup completes */
	u_char 	skip_if_ident;  /* skip module if we got an ident reply */
    char	*reason;		/* reject reason */
    aConfKV	*module_kv;		/* module-specific key/value pairs */
    u_char	delayed;		/* delayed execution mode */
};

struct Target
{
    char	*value;
    char	yes;
    aTarget	*nextt;
};
