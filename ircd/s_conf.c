/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf.c
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

/* -- avalon -- 20 Feb 1992
 * Reversed the order of the params for attach_conf().
 * detach_conf() and attach_conf() are now the same:
 * function_conf(aClient *, aConfItem *)
 */

/* -- Jto -- 20 Jun 1990
 * Added gruner's overnight fix..
 */

/* -- Jto -- 16 Jun 1990
 * Moved matches to ../common/match.c
 */

/* -- Jto -- 03 Jun 1990
 * Added Kill fixes from gruner@lan.informatik.tu-muenchen.de
 * Added jarlek's msgbase fix (I still don't understand it... -- Jto)
 */

/* -- Jto -- 13 May 1990
 * Added fixes from msa:
 * Comments and return value to init_conf()
 */

/*
 * -- Jto -- 12 May 1990
 *  Added close() into configuration file (was forgotten...)
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: s_conf.c,v 1.10 1997/07/15 04:35:47 kalt Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/wait.h>
#ifdef __hpux
# ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
# endif
#endif
#if defined(PCS) || defined(AIX) || defined(DYNIXPTX) || defined(SVR3)
#include <time.h>
#endif
#ifdef	R_LINES
#include <signal.h>
#endif

#include "h.h"

static	int	check_time_interval __P((char *, char *));
static	int	lookup_confhost __P((aConfItem *));

aConfItem	*conf = NULL;

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void	det_confs_butmask(cptr, mask)
aClient	*cptr;
int	mask;
{
	Reg	Link	*tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			(void)detach_conf(cptr, tmp->value.aconf);
	    }
}

/*
 * find the first (best) I line to attach.
 */
int	attach_Iline(cptr, hp, sockhost)
aClient *cptr;
Reg	struct	hostent	*hp;
char	*sockhost;
{
	Reg	aConfItem	*aconf;
	Reg	char	*hname;
	Reg	int	i;
	static	char	uhost[HOSTLEN+USERLEN+3];
	static	char	fullname[HOSTLEN+1];

	for (aconf = conf; aconf; aconf = aconf->next)
	    {
		if ((aconf->status != CONF_CLIENT) &&
		    (aconf->status != CONF_RCLIENT))
			continue;
		if (aconf->port && aconf->port != cptr->acpt->port)
			continue;
		if (!aconf->host || !aconf->name)
			goto attach_iline;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			     hname = hp->h_aliases[i++])
			    {
				(void)strncpy(fullname, hname,
					sizeof(fullname)-1);
				add_local_domain(fullname,
						 HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				      sockhost, fullname));
				if (index(aconf->name, '@'))
				    {
					(void)strcpy(uhost, cptr->username);
					(void)strcat(uhost, "@");
				    }
				else
					*uhost = '\0';
				(void)strncat(uhost, fullname,
					sizeof(uhost) - strlen(uhost));
				if (!match(aconf->name, uhost))
					goto attach_iline;
			    }

		if (index(aconf->host, '@'))
		    {
			strncpyzt(uhost, cptr->username, sizeof(uhost));
			(void)strcat(uhost, "@");
		    }
		else
			*uhost = '\0';
		(void)strncat(uhost, sockhost, sizeof(uhost) - strlen(uhost));
		if (match(aconf->host, uhost))
			continue;
attach_iline:
		if (index(uhost, '@'))
			cptr->flags |= FLAGS_DOID;
		if (aconf->status & CONF_RCLIENT)
			SetRestricted(cptr);
		get_sockhost(cptr, uhost);
		if ((i = attach_conf(cptr, aconf)) < -1)
			find_bounce(cptr, ConfClass(aconf));
		return i;
	    }
	return -1;
}

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem	*count_cnlines(lp)
Reg	Link	*lp;
{
	Reg	aConfItem	*aconf, *cline = NULL, *nline = NULL;

	for (; lp; lp = lp->next)
	    {
		aconf = lp->value.aconf;
		if (!(aconf->status & CONF_SERVER_MASK))
			continue;
		if ((aconf->status == CONF_CONNECT_SERVER ||
		     aconf->status == CONF_ZCONNECT_SERVER) && !cline)
			cline = aconf;
		else if (aconf->status == CONF_NOCONNECT_SERVER && !nline)
			nline = aconf;
	    }
	return nline;
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int	detach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg	Link	**lp, *tmp;

	lp = &(cptr->confs);

	while (*lp)
	    {
		if ((*lp)->value.aconf == aconf)
		    {
			if ((aconf) && (Class(aconf)))
			    {
				if (aconf->status & CONF_CLIENT_MASK)
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
       				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
		 		    {
					free_class(Class(aconf));
					Class(aconf) = NULL;
				    }
			     }
			if (aconf && !--aconf->clients && IsIllegal(aconf))
				free_conf(aconf);
			tmp = *lp;
			*lp = tmp->next;
			free_link(tmp);
			istat.is_conflink--;
			return 0;
		    }
		else
			lp = &((*lp)->next);
	    }
	return -1;
}

static	int	is_attached(aconf, cptr)
aConfItem *aconf;
aClient *cptr;
{
	Reg	Link	*lp;

	for (lp = cptr->confs; lp; lp = lp->next)
		if (lp->value.aconf == aconf)
			break;

	return (lp) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
int	attach_conf(cptr, aconf)
aConfItem *aconf;
aClient *cptr;
{
	Reg	Link	*lp;

	if (is_attached(aconf, cptr))
		return 1;
	if (IsIllegal(aconf))
		return -1;
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR | CONF_CLIENT |
			      CONF_RCLIENT)))
	    {
		if (aconf->clients >= ConfMaxLinks(aconf) &&
		    ConfMaxLinks(aconf) > 0)
			return -3;    /* Use this for printing error message */
		if (ConfConFreq(aconf) > 0)	/* special limit per host */
		    {
			Reg	aClient	*acptr;
			Reg	int	i, cnt = 0;

			for (i = highest_fd; i >= 0; i--)
				if ((acptr = local[i]) && (cptr != acptr) &&
				    !bcmp((char *)&cptr->ip,
					  (char *)&acptr->ip,
					  sizeof(cptr->ip)))
					cnt++;
			if (cnt >= ConfConFreq(aconf))
				return -4;	/* for error message */
		    }
		if (ConfConFreq(aconf) < 0) { /* special limit per user@host */
			Reg     aClient *acptr;
			Reg     int     i, cnt = 0;

			for (i = highest_fd; i >= 0; i--)
				if ((acptr = local[i]) && (cptr != acptr) &&
				    !bcmp((char *)&cptr->ip,(char *)&acptr->ip,
					  sizeof(cptr->ip))
				    && !strncasecmp(acptr->auth,
						    cptr->auth, USERLEN))
					cnt--;
			if (cnt <= ConfConFreq(aconf))
				return -5;      /* for error message */
		}
	    }

	lp = make_link();
	istat.is_conflink++;
	lp->next = cptr->confs;
	lp->value.aconf = aconf;
	cptr->confs = lp;
	aconf->clients++;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem *find_admin()
    {
	Reg	aConfItem	*aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
    }

aConfItem *find_me()
    {
	Reg	aConfItem	*aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;
	
	return (aconf);
    }

/*
 * attach_confs
 *  Attach a CONF line to a client if the name passed matches that for
 * the conf file (for non-C/N lines) or is an exact match (C/N lines
 * only).  The difference in behaviour is to stop C:*::* and N:*::*.
 */
aConfItem *attach_confs(cptr, name, statmask)
aClient	*cptr;
char	*name;
int	statmask;
{
	Reg	aConfItem	*tmp;
	aConfItem	*first = NULL;
	int	len = strlen(name);
  
	if (!name || len > HOSTLEN)
		return NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0) &&
		    tmp->name && !match(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
			 (tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
			 tmp->name && !mycmp(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *attach_confs_host(cptr, host, statmask)
aClient *cptr;
char	*host;
int	statmask;
{
	Reg	aConfItem *tmp;
	aConfItem *first = NULL;
	int	len = strlen(host);
  
	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & CONF_SERVER_MASK) == 0 &&
		    (!tmp->host || match(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
	       	    (tmp->status & CONF_SERVER_MASK) &&
	       	    (tmp->host && mycmp(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
aConfItem *find_conf_exact(name, user, host, statmask)
char	*name, *host, *user;
int	statmask;
{
	Reg	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];

	SPRINTF(userhost, "%s@%s", user, host);

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (!(tmp->status & statmask) || !tmp->name || !tmp->host ||
		    mycmp(tmp->name, name))
			continue;
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** socket host) matches *either* host or name field
		** of the configuration.
		*/
		if (match(tmp->host, userhost))
			continue;
		if (tmp->status & (CONF_OPERATOR|CONF_LOCOP))
		    {
			if (tmp->clients < MaxLinks(Class(tmp)))
				return tmp;
			else
				continue;
		    }
		else
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf_name(name, statmask)
char	*name;
int	statmask;
{
	Reg	aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf(lp, name, statmask)
char	*name;
Link	*lp;
int	statmask;
{
	Reg	aConfItem *tmp;
	int	namelen = name ? strlen(name) : 0;
  
	if (namelen > HOSTLEN)
		return (aConfItem *) 0;

	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if ((tmp->status & statmask) &&
		    (((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
	 	     tmp->name && !mycmp(tmp->name, name)) ||
		     ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0 &&
		     tmp->name && !match(tmp->name, name))))
			return tmp;
	    }
	return NULL;
}

/*
 * Added for new access check    meLazy
 */
aConfItem *find_conf_host(lp, host, statmask)
Reg	Link	*lp;
char	*host;
Reg	int	statmask;
{
	Reg	aConfItem *tmp;
	int	hostlen = host ? strlen(host) : 0;
  
	if (hostlen > HOSTLEN || BadPtr(host))
		return (aConfItem *)NULL;
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (tmp->status & statmask &&
		    (!(tmp->status & CONF_SERVER_MASK || tmp->host) ||
	 	     (tmp->host && !match(tmp->host, host))))
			return tmp;
	    }
	return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem *find_conf_ip(lp, ip, user, statmask)
char	*ip, *user;
Link	*lp;
int	statmask;
{
	Reg	aConfItem *tmp;
	Reg	char	*s;
  
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (!(tmp->status & statmask))
			continue;
		s = index(tmp->host, '@');
		*s = '\0';
		if (match(tmp->host, user))
		    {
			*s = '@';
			continue;
		    }
		*s = '@';
		if (!bcmp((char *)&tmp->ipnum, ip, sizeof(struct in_addr)))
			return tmp;
	    }
	return NULL;
}

/*
 * find_conf_entry
 *
 * - looks for a match on all given fields.
 */
aConfItem *find_conf_entry(aconf, mask)
aConfItem *aconf;
u_int	mask;
{
	Reg	aConfItem *bconf;

	for (bconf = conf, mask &= ~CONF_ILLEGAL; bconf; bconf = bconf->next)
	    {
		if (!(bconf->status & mask) || (bconf->port != aconf->port))
			continue;

		if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
		    (BadPtr(aconf->host) && !BadPtr(bconf->host)))
			continue;
		if (!BadPtr(bconf->host) && mycmp(bconf->host, aconf->host))
			continue;

		if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
		    (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
			continue;
		if (!BadPtr(bconf->passwd) &&
		    mycmp(bconf->passwd, aconf->passwd))
			continue;

		if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
		    (BadPtr(aconf->name) && !BadPtr(bconf->name)))
			continue;
		if (!BadPtr(bconf->name) && mycmp(bconf->name, aconf->name))
			continue;
		break;
	    }
	return bconf;
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int	rehash(cptr, sptr, sig)
aClient	*cptr, *sptr;
int	sig;
{
	Reg	aConfItem **tmp = &conf, *tmp2 = NULL;
	Reg	aClass	*cltmp;
	Reg	aClient	*acptr;
	Reg	int	i;
	int	ret = 0;

	if (sig == 1)
	    {
		sendto_flag(SCH_NOTICE,
			    "Got signal SIGHUP, reloading ircd.conf file");
#ifdef	ULTRIX
		if (fork() > 0)
			exit(0);
		write_pidfile();
#endif
	    }

	for (i = 0; i <= highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		    {
			/*
			 * Nullify any references from client structures to
			 * this host structure which is about to be freed.
			 * Could always keep reference counts instead of
			 * this....-avalon
			 */
			acptr->hostp = NULL;
#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
			if (find_restrict(acptr))
			    {
				sendto_flag(SCH_NOTICE,
					    "Restricting %s, closing lp",
					    get_client_name(acptr,FALSE));
				acptr->exitc = EXITC_RLINE;
				if (exit_client(cptr,acptr,&me,"R-lined") ==
				    FLUSH_BUFFER)
					ret = FLUSH_BUFFER;
			    }
#endif
		    }

	while ((tmp2 = *tmp))
		if (tmp2->clients || tmp2->status & CONF_LISTEN_PORT)
		    {
			/*
			** Configuration entry is still in use by some
			** local clients, cannot delete it--mark it so
			** that it will be deleted when the last client
			** exits...
			*/
			if (!(tmp2->status & (CONF_LISTEN_PORT|CONF_CLIENT)))
			    {
				*tmp = tmp2->next;
				tmp2->next = NULL;
			    }
			else
				tmp = &tmp2->next;
			tmp2->status |= CONF_ILLEGAL;
		    }
		else
		    {
			*tmp = tmp2->next;
			free_conf(tmp2);
	    	    }

	/*
	 * We don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	for (cltmp = NextClass(FirstClass()); cltmp; cltmp = NextClass(cltmp))
		MaxLinks(cltmp) = -1;

	if (sig != 2)
		flush_cache();
	(void) initconf(0);
	close_listeners();

	/*
	 * flush out deleted I and P lines although still in use.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
		if (!(tmp2->status & CONF_ILLEGAL))
			tmp = &tmp2->next;
		else
		    {
			*tmp = tmp2->next;
			tmp2->next = NULL;
			if (!tmp2->clients)
				free_conf(tmp2);
		    }
#ifdef CACHED_MOTD
	read_motd(MPATH);
#endif
	rehashed = 1;
	return ret;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be the file direct or one end
 * of a pipe from m4.
 */
int	openconf()
{
#ifdef	M4_PREPROC
	int	pi[2], i;

	if (pipe(pi) == -1)
		return -1;
	switch(vfork())
	{
	case -1 :
		return -1;
	case 0 :
		(void)close(pi[0]);
		if (pi[1] != 1)
		    {
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		(void)dup2(1,2);
		for (i = 3; i < MAXCONNECTIONS; i++)
			if (local[i])
				(void) close(i);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp("m4", "m4", "ircd.m4", configfile, 0);
		report_error("Error executing m4 %s:%s", &me);
		_exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	return open(configfile, O_RDONLY);
#endif
}
extern char *getfield();

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

int 	initconf(opt)
int	opt;
{
	static	char	quotes[9][2] = {{'b', '\b'}, {'f', '\f'}, {'n', '\n'},
					{'r', '\r'}, {'t', '\t'}, {'v', '\v'},
					{'\\', '\\'}, { 0, 0}};
	Reg	char	*tmp, *s;
	int	fd, i;
	char	line[512], c[80], *tmp2 = NULL;
	int	ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", configfile));
	if ((fd = openconf()) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return -1;
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
	    {
		line[i] = '\0';
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1) > 0)
			if ((tmp = (char *)index(c, '\n')))
			    {
				*tmp = 0;
				break;
			    }
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		    {
			if (*tmp == '\\')
			    {
				for (i = 0; quotes[i][0]; i++)
					if (quotes[i][0] == *(tmp+1))
					    {
						*tmp = quotes[i][1];
						break;
					    }
				if (!quotes[i][0])
					*tmp = *(tmp+1);
				if (!*(tmp+1))
					break;
				else
					for (s = tmp; (*s = *(s+1)); s++)
						;
			    }
			else if (*tmp == '#')
			    {
				*tmp = '\0';
				break;	/* Ignore the rest of the line */
			    }
		    }
		if (!*line || line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		/* Could we test if it's conf line at all?	-Vesa */
		if (line[1] != ':')
		    {
                        Debug((DEBUG_ERROR, "Bad config line: %s", line));
                        continue;
                    }
		if (aconf)
			free_conf(aconf);
		aconf = make_conf();

		if (tmp2)
			MyFree(tmp2);
		tmp2 = NULL;
		tmp = getfield(line);
		if (!tmp)
			continue;
		switch (*tmp)
		{
			case 'A': /* Name, e-mail address of administrator */
			case 'a': /* of this server. */
				aconf->status = CONF_ADMIN;
				break;
			case 'B': /* Name of alternate servers */
			case 'b':
				aconf->status = CONF_BOUNCE;
				break;
			case 'C': /* Server where I should try to connect */
			  	  /* in case of lp failures             */
				ccount++;
				aconf->status = CONF_CONNECT_SERVER;
				break;
			case 'c':
				ccount++;
				aconf->status = CONF_ZCONNECT_SERVER;
				break;
			case 'H': /* Hub server line */
			case 'h':
				aconf->status = CONF_HUB;
				break;
			case 'I': /* Just plain normal irc client trying  */
			          /* to connect me */
				aconf->status = CONF_CLIENT;
				break;
			case 'i' : /* Restricted client */
				aconf->status = CONF_RCLIENT;
				break;
			case 'K': /* Kill user line on irc.conf           */
				aconf->status = CONF_KILL;
				break;
			case 'k':
				aconf->status = CONF_OTHERKILL;
				break;
			/* Operator. Line should contain at least */
			/* password and host where connection is  */
			case 'L': /* guaranteed leaf server */
			case 'l':
				aconf->status = CONF_LEAF;
				break;
			/* Me. Host field is name used for this host */
			/* and port number is the number of the port */
			case 'M':
			case 'm':
				aconf->status = CONF_ME;
				break;
			case 'N': /* Server where I should NOT try to     */
			case 'n': /* connect in case of lp failures     */
				  /* but which tries to connect ME        */
				++ncount;
				aconf->status = CONF_NOCONNECT_SERVER;
				break;
			case 'O':
				aconf->status = CONF_OPERATOR;
				break;
			/* Local Operator, (limited privs --SRB) */
			case 'o':
				aconf->status = CONF_LOCOP;
				break;
			case 'P': /* listen port line */
			case 'p':
				aconf->status = CONF_LISTEN_PORT;
				break;
			case 'Q': /* a server that you don't want in your */
			case 'q': /* network. USE WITH CAUTION! */
				aconf->status = CONF_QUARANTINED_SERVER;
				break;
#ifdef R_LINES
			case 'R': /* extended K line */
			case 'r': /* Offers more options of how to restrict */
				aconf->status = CONF_RESTRICT;
				break;
#endif
			case 'S': /* Service. Same semantics as   */
			case 's': /* CONF_OPERATOR                */
				aconf->status = CONF_SERVICE;
				break;
#if 0
			case 'U': /* Uphost, ie. host where client reading */
			case 'u': /* this should connect.                  */
			/* This is for client only, I must ignore this */
			/* ...U-line should be removed... --msa */
				break;
#endif
			case 'V': /* Server link version requirements */
				aconf->status = CONF_VER;
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
		    default:
			Debug((DEBUG_ERROR, "Error in config file: %s", line));
			break;
		    }
		if (IsIllegal(aconf))
			continue;

		for (;;) /* Fake loop, that I can use break here --msa */
		    {
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			aconf->port = atoi(tmp);
			if (aconf->status == CONF_CONNECT_SERVER)
				DupString(tmp2, tmp);
			if (aconf->status == CONF_ZCONNECT_SERVER)
				DupString(tmp2, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			Class(aconf) = find_class(atoi(tmp));
			break;
		    }
		istat.is_confmem += aconf->host ? strlen(aconf->host)+1 : 0;
		istat.is_confmem += aconf->passwd ? strlen(aconf->passwd)+1 :0;
		istat.is_confmem += aconf->name ? strlen(aconf->name)+1 : 0;

		/*
		** Bounce line fields are mandatory
		*/
		if (aconf->status == CONF_BOUNCE && aconf->port == 0)
			continue;
		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS)
		    {
			add_class(atoi(aconf->host), atoi(aconf->passwd),
				  atoi(aconf->name), aconf->port,
				  tmp ? atoi(tmp) : 0);
			continue;
		    }
		/*
                ** associate each conf line with a class by using a pointer
                ** to the correct class record. -avalon
                */
		if (aconf->status & (CONF_CLIENT_MASK|CONF_LISTEN_PORT))
		    {
			if (Class(aconf) == 0)
				Class(aconf) = find_class(0);
			if (MaxLinks(Class(aconf)) < 0)
				Class(aconf) = find_class(0);
		    }
		if (aconf->status & (CONF_LISTEN_PORT|CONF_CLIENT))
		    {
			aConfItem *bconf;

			if ((bconf = find_conf_entry(aconf, aconf->status)))
			    {
				delist_conf(bconf);
				bconf->status &= ~CONF_ILLEGAL;
				if (aconf->status == CONF_CLIENT)
				    {
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
					bconf->class->links += bconf->clients;
				    }
				free_conf(aconf);
				aconf = bconf;
			    }
			else if (aconf->host &&
				 aconf->status == CONF_LISTEN_PORT)
				(void)add_listener(aconf);
		    }
		if (aconf->status & (CONF_SERVER_MASK|CONF_SERVICE))
			if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
			    !aconf->host || index(aconf->host, '*') ||
			     index(aconf->host,'?') || !aconf->name)
				continue;

		if (aconf->status &
		    (CONF_SERVER_MASK|CONF_LOCOP|CONF_OPERATOR|CONF_SERVICE))
			if (!index(aconf->host, '@') && *aconf->host != '/')
			    {
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				SPRINTF(newhost, "*@%s", aconf->host);
				MyFree(aconf->host);
				aconf->host = newhost;
				istat.is_confmem += 2;
			    }
		if (aconf->status & CONF_SERVER_MASK)
		    {
			if (BadPtr(aconf->passwd))
				continue;
			else if (!(opt & BOOT_QUICK))
				(void)lookup_confhost(aconf);
		    }
		if (aconf->status & (CONF_CONNECT_SERVER | CONF_ZCONNECT_SERVER))
		    {
			aconf->ping = (aCPing *)MyMalloc(sizeof(aCPing));
			bzero((char *)aconf->ping, sizeof(*aconf->ping));
			istat.is_confmem += sizeof(*aconf->ping);
			if (tmp2 && index(tmp2, '.'))
				aconf->ping->port = atoi(index(tmp2, '.') + 1);
			else
				aconf->ping->port = aconf->port;
			if (tmp2)
			    {
				MyFree(tmp2);
				tmp2 = NULL;
			    }
				
		    }
		/*
		** Name cannot be changed after the startup.
		** (or could be allowed, but only if all links are closed
		** first).
		** Configuration info does not override the name and port
		** if previously defined. Note, that "info"-field can be
		** changed by "/rehash".
		*/
		if (aconf->status == CONF_ME)
		    {
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (ME[0] == '\0' && aconf->host[0])
				strncpyzt(ME, aconf->host,
					  sizeof(ME));
			if (aconf->port)
				setup_ping(aconf);
		    }
		(void)collapse(aconf->host);
		(void)collapse(aconf->name);
		Debug((DEBUG_NOTICE,
		      "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port,
		      aconf->class ? ConfClass(aconf) : 0));

		aconf->next = conf;
		conf = aconf;
		aconf = NULL;
	    }
	if (aconf)
		free_conf(aconf);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	check_class();
	nextping = nextconnect = timeofday;
	return 0;
}

/*
 * lookup_confhost
 *   Do (start) DNS lookups of all hostnames in the conf line and convert
 * an IP addresses in a.b.c.d number for to IP#s.
 */
static	int	lookup_confhost(aconf)
Reg	aConfItem	*aconf;
{
	Reg	char	*s;
	Reg	struct	hostent *hp;
	Link	ln;

	if (BadPtr(aconf->host) || BadPtr(aconf->name))
		goto badlookup;
	if ((s = index(aconf->host, '@')))
		s++;
	else
		s = aconf->host;
	/*
	** Do name lookup now on hostnames given and store the
	** ip numbers in conf structure.
	*/
	if (!isalpha(*s) && !isdigit(*s))
		goto badlookup;

	/*
	** Prepare structure in case we have to wait for a
	** reply which we get later and store away.
	*/
	ln.value.aconf = aconf;
	ln.flags = ASYNC_CONF;

	if (isdigit(*s))
		aconf->ipnum.s_addr = inetaddr(s);
	else if ((hp = gethost_byname(s, &ln)))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
			sizeof(struct in_addr));

	if (aconf->ipnum.s_addr == -1)
		goto badlookup;
	return 0;
badlookup:
	if (aconf->ipnum.s_addr == -1)
		bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	Debug((DEBUG_ERROR,"Host/server name error: (%s) (%s)",
		aconf->host, aconf->name));
	return -1;
}

int	find_kill(cptr, doall, comment)
aClient	*cptr;
int	doall;
char	**comment;
{
	static char	reply[256];
	char *host, *name, *ident, *check;
	aConfItem *tmp;
	int	now;

	if (!cptr->user)
		return 0;

	host = cptr->sockhost;
	name = cptr->user->username;
	ident = cptr->auth;

	if (strlen(host)  > (size_t) HOSTLEN ||
            (name ? strlen(name) : 0) > (size_t) HOSTLEN)
		return (0);

	*reply = '\0';

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (!doall && (BadPtr(tmp->passwd) || !isdigit(*tmp->passwd)))
			continue;
		if (!(tmp->status & (CONF_KILL | CONF_OTHERKILL)))
			continue;
		if (tmp->status == CONF_KILL)
			check = name;
		else
			check = ident;
 		if (tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
 		    (!check || match(tmp->name, check) == 0) &&
		    (!tmp->port || (tmp->port == cptr->acpt->port)))
		    {
			now = 0;
			if (!BadPtr(tmp->passwd) && isdigit(*tmp->passwd) &&
			    !(now = check_time_interval(tmp->passwd, reply)))
				continue;
			if (now == ERR_YOUWILLBEBANNED)
				tmp = NULL;
			break;
		    }
	    }

	if (*reply)
		sendto_one(cptr, reply, ME, now, cptr->name);
	else if (tmp)
		sendto_one(cptr, ":%s %d %s :%s", ME,
			   ERR_YOUREBANNEDCREEP, cptr->name,
			   BadPtr(tmp->passwd) ?
			   "You are not welcome to this server" : tmp->passwd);

	if (tmp && !BadPtr(tmp->passwd))
		*comment = tmp->passwd;

 	return (tmp ? -1 : 0);
}

/*
 * For type stat, check if both name and host masks match.
 * Return -1 for match, 0 for no-match.
 */
int	find_two_masks(name, host, stat)
char	*name, *host;
int	stat;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((tmp->status == stat) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
 		    (match(tmp->name, name) == 0))
			break;
 	return (tmp ? -1 : 0);
}

/*
 * For type stat, check if name matches and any char from key matches
 * to chars in passwd field.
 * Return -1 for match, 0 for no-match.
 */
int	find_conf_flags(name, key, stat)
char	*name, *key;
int	stat;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((tmp->status == stat) && tmp->passwd && tmp->name &&
 		    (match(tmp->name, name) == 0) &&
		    strpbrk(key, tmp->passwd))
			break;
 	return (tmp ? -1 : 0);
}

#ifdef R_LINES
/* find_restrict works against host/name and calls an outside program 
 * to determine whether a client is allowed to connect.  This allows 
 * more freedom to determine who is legal and who isn't, for example
 * machine load considerations.  The outside program is expected to 
 * return a reply line where the first word is either 'Y' or 'N' meaning 
 * "Yes Let them in" or "No don't let them in."  If the first word 
 * begins with neither 'Y' or 'N' the default is to let the person on.
 * It returns a value of 0 if the user is to be let through -Hoppie
 */
int	find_restrict(cptr)
aClient	*cptr;
{
	aConfItem *tmp;
	char	reply[80], temprpl[80];
	char	*rplhold = reply, *host, *name, *s;
	char	rplchar = 'Y';
	int	pi[2], rc = 0, n;

	if (!cptr->user)
		return 0;
	name = cptr->user->username;
	host = cptr->sockhost;
	Debug((DEBUG_INFO, "R-line check for %s[%s]", name, host));

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (tmp->status != CONF_RESTRICT ||
		    (tmp->host && host && match(tmp->host, host)) ||
		    (tmp->name && name && match(tmp->name, name)))
			continue;

		if (BadPtr(tmp->passwd))
			continue;

		if (pipe(pi) == -1)
		    {
			report_error("Error creating pipe for R-line %s:%s",
				     &me);
			return 0;
		    }
		switch (rc = vfork())
		{
		case -1 :
			report_error("Error forking for R-line %s:%s", &me);
			return 0;
		case 0 :
		    {
			Reg	int	i;

			(void)close(pi[0]);
			for (i = 2; i < MAXCONNECTIONS; i++)
				if (i != pi[1])
					(void)close(i);
			if (pi[1] != 2)
				(void)dup2(pi[1], 2);
			(void)dup2(2, 1);
			if (pi[1] != 2 && pi[1] != 1)
				(void)close(pi[1]);
			(void)execlp(tmp->passwd, tmp->passwd, name, host,
				     cptr->username, 0);
			_exit(-1);
		    }
		default :
			(void)close(pi[1]);
			break;
		}
		*reply = '\0';
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		while ((n = dgets(pi[0], temprpl, sizeof(temprpl)-1)) > 0)
		    {
			temprpl[n] = '\0';
			if ((s = (char *)index(temprpl, '\n')))
			      *s = '\0';
			if (strlen(temprpl) + strlen(reply) < sizeof(reply)-2)
				SPRINTF(rplhold,"%s %s", rplhold, temprpl);
			else
			    {
				sendto_flag(SCH_ERROR,
					    "R-line %s/%s: reply too long!",
					    name, host);
				break;
			    }
		    }
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		(void)close(pi[0]);
		(void)kill(rc, SIGKILL); /* cleanup time */
		(void)wait(0);

		rc = 0;
		while (*rplhold == ' ')
			rplhold++;
		rplchar = *rplhold; /* Pull out the yes or no */
		while (*rplhold != ' ')
			rplhold++;
		while (*rplhold == ' ')
			rplhold++;
		(void)strcpy(reply,rplhold);
		rplhold = reply;

		if ((rc = (rplchar == 'n' || rplchar == 'N')))
			break;
	    }
	if (rc)
	    {
		sendto_one(cptr, ":%s %d %s :Restriction: %s",
			   ME, ERR_YOUREBANNEDCREEP, cptr->name, reply);
		return -1;
	    }
	return 0;
}
#endif


/*
** check against a set of time intervals
*/

static	int	check_time_interval(interval, reply)
char	*interval, *reply;
{
	struct tm *tptr;
 	char	*p;
 	int	perm_min_hours, perm_min_minutes,
 		perm_max_hours, perm_max_minutes;
 	int	now, perm_min, perm_max;

	tptr = localtime(&timeofday);
 	now = tptr->tm_hour * 60 + tptr->tm_min;

	while (interval)
	    {
		p = (char *)index(interval, ',');
		if (p)
			*p = '\0';
		if (sscanf(interval, "%2d%2d-%2d%2d",
			   &perm_min_hours, &perm_min_minutes,
			   &perm_max_hours, &perm_max_minutes) != 4)
		    {
			if (p)
				*p = ',';
			return(0);
		    }
		if (p)
			*(p++) = ',';
		perm_min = 60 * perm_min_hours + perm_min_minutes;
		perm_max = 60 * perm_max_hours + perm_max_minutes;
           	/*
           	** The following check allows intervals over midnight ...
           	*/
		if ((perm_min < perm_max)
		    ? (perm_min <= now && now <= perm_max)
		    : (perm_min <= now || now <= perm_max))
		    {
			(void)sprintf(reply,
				":%%s %%d %%s :%s %d:%02d to %d:%02d.",
				"You are not allowed to connect from",
				perm_min_hours, perm_min_minutes,
				perm_max_hours, perm_max_minutes);
			return(ERR_YOUREBANNEDCREEP);
		    }
		if ((perm_min < perm_max)
		    ? (perm_min <= now + 5 && now + 5 <= perm_max)
		    : (perm_min <= now + 5 || now + 5 <= perm_max))
		    {
			(void)sprintf(reply, ":%%s %%d %%s :%d minute%s%s",
				perm_min-now,(perm_min-now)>1?"s ":" ",
				"and you will be denied for further access");
			return(ERR_YOUWILLBEBANNED);
		    }
		interval = p;
	    }
	return(0);
}

/*
** find_bounce
**	send a bounce numeric to a client.
**	if cptr is NULL, class is considered to be a fd (ugly, isn't it?)
*/
void	find_bounce(cptr, class)
aClient *cptr;
int	class;
    {
	Reg	aConfItem	*aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
	    {
		if (aconf->status != CONF_BOUNCE)
			continue;

		if (cptr == NULL)
			/*
			** early rejection,
			** connection class and hostname are unknown
			*/
			if (atoi(aconf->host) == -1)
			    {
				char rpl[BUFSIZE];
				
				SPRINTF(rpl, rpl_str(RPL_BOUNCE,"unknown"),
					aconf->name, aconf->port);
				strcat(rpl, "\r\n");
				send(class, rpl, strlen(rpl), 0);
				return;
			    }
			else
				continue;

		/* cptr != NULL */
		/*
		** "too many" type rejection, class is known.
		** check if B line is for a class #,
		** and if it is for a hostname.
		*/
		if (isdigit(*aconf->host))
		    {
			if (class != atoi(aconf->host))
				continue;
		    }
		else
			if (match(aconf->host, cptr->sockhost))
				continue;

		sendto_one(cptr, rpl_str(RPL_BOUNCE, cptr->name), aconf->name,
			   aconf->port);
		return;
	    }
	
    }

