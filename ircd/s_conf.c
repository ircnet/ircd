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
static  char rcsid[] = "@(#)$Id: s_conf.c,v 1.56 2002/07/29 22:38:50 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_CONF_C
#include "s_externs.h"
#undef S_CONF_C

static	int	check_time_interval __P((char *, char *));
static	int	lookup_confhost __P((aConfItem *));

aConfItem	*conf = NULL;
aConfItem	*kconf = NULL;
char		*networkname = NULL;

/* Parse I-lines flags from string.
 * D - Restricted, if no DNS
 * I - Restricted, if no ident
 * R - Restricted.
 * E - Kline exempt
 */
long iline_flags_parse(char *string)
{
	long tmp = 0;
	
	if (!string)
	{
		return 0;
	}
	
	if (index(string,'D'))
	{
		tmp |= CFLAG_RNODNS;
	}
	if (index(string,'I'))
	{
		tmp |= CFLAG_RNOIDENT;
	}
	if (index(string,'R'))
	{
		tmp |= CFLAG_RESTRICTED;
	}
	
	if (index(string,'E'))
	{
		tmp |= CFLAG_KEXEMPT;
	}

	return tmp;
}

/* convert iline flags to human readable string */
char *iline_flags_to_string(long flags)
{
	static char ifsbuf[BUFSIZE];
	char *s = ifsbuf;
	
	if (flags & CFLAG_RNODNS)
	{
		*s++ = 'D';
	}

	if (flags & CFLAG_RNOIDENT)
	{
		*s++ = 'I';
	}
	
	if (flags & CFLAG_RESTRICTED)
	{
		*s++ = 'R';
	}

	if (flags & CFLAG_KEXEMPT)
	{
		*s++ = 'E';
	}
	*s++ = '\0';
	
	return ifsbuf;
}

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
 * Match address by #IP bitmask (10.11.12.128/27)
 * Now should work for IPv6 too.
 * returns -1 on error, 0 on match, 1 when NO match.
 */
int    match_ipmask(mask, cptr)
char   *mask;
aClient *cptr;
{
	int	m;
	char	*p;
	struct  IN_ADDR addr;
	char	dummy[128];
	u_long	lmask;
#ifdef	INET6
	int	j;
#endif
 
	strncpyzt(dummy, mask, sizeof(dummy));
	mask = dummy;
	if ((p = index(mask, '@')))
	{
		*p = '\0';
		if (match(mask, cptr->username))
			return 1;
		mask = p + 1;
	}
	if (!(p = index(mask, '/')))
		goto badmask;
	*p = '\0';
	
	if (sscanf(p + 1, "%d", &m) != 1)
	{
		goto badmask;
	}
#ifndef	INET6
	if (m < 0 || m > 32)
		goto badmask;
	if (!m)
		return 0;       /* x.x.x.x/0 always matches */
	lmask = htonl((u_long)0xffffffffL << (32 - m));
	addr.s_addr = inetaddr(mask);
	return ((addr.s_addr ^ cptr->ip.s_addr) & lmask) ? 1 : 0;
#else
	if (m < 0 || m > 128)
		goto badmask;

	if (inetpton(AF_INET6, mask, (void *)addr.s6_addr) != 1)
	{
		return -1;
	}

	/* Make sure that the ipv4 notation still works. */
	if (IN6_IS_ADDR_V4MAPPED(&addr) && m < 96)
	{
		m += 96;
	}

	j = m & 0x1F;	/* number not mutliple of 32 bits */
	m >>= 5;	/* number of 32 bits */

	if (m && memcmp((void *)(addr.s6_addr), 
		(void *)(cptr->ip.s6_addr), m << 2))
		return 1;

	if (j)
	{
		lmask = htonl((u_long)0xffffffffL << (32 - j));
		if ((((u_int32_t *)(addr.s6_addr))[m] ^
			((u_int32_t *)(cptr->ip.s6_addr))[m]) & lmask)
			return 1;
	}

	return 0;
#endif
badmask:
	sendto_flag(SCH_ERROR, "Ignoring bad mask: %s", mask);
	return -1;
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
				strncpyzt(fullname, hname,
					sizeof(fullname));
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
		if (strchr(aconf->host, '/'))		/* 1.2.3.0/24 */
		    {
			if (match_ipmask(aconf->host, cptr))
				continue;
                } else if (match(aconf->host, uhost))	/* 1.2.3.* */
			continue;
		if (*aconf->name == '\0' && hp)
		    {
			strncpyzt(uhost, hp->h_name, sizeof(uhost));
			add_local_domain(uhost, sizeof(uhost) - strlen(uhost));
		    }
attach_iline:
		if (aconf->status & CONF_RCLIENT)
		{
			SetRestricted(cptr);
		}
		
		if (IsConfRestricted(aconf))
		{
			SetRestricted(cptr);
		}
		
		if (!hp && IsConfRNoDNS(aconf))
		{
			SetRestricted(cptr);
		}
		
		if (IsConfKlineExempt(aconf))
		{
			SetKlineExempt(cptr);
		}
		if (!(cptr->flags & FLAGS_GOTID) && IsConfRNoIdent(aconf))
		{
			SetRestricted(cptr);
		}
		get_sockhost(cptr, uhost);
		if ((i = attach_conf(cptr, aconf)) < -1)
			find_bounce(cptr, ConfClass(aconf), -1);
		return i;
	    }
	find_bounce(cptr, 0, -2);
	return -2; /* used in register_user() */
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
	aConfItem **aconf2,*aconf3;

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
			{
				/* Remove the conf entry from the Conf linked list */
				for (aconf2 = &conf; (aconf3 = *aconf2); )
				{
					if (aconf3 == aconf)
					{
						*aconf2 = aconf3->next;
						aconf3->next = NULL;
						free_conf(aconf);
					}
					else
					{
						aconf2 = &aconf3->next;
					}
				}
			}
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
	    }

	if ((aconf->status & (CONF_CLIENT | CONF_RCLIENT)))
	{
		int hcnt = 0, ucnt = 0;
		int ghcnt = 0, gucnt = 0;
		anUser *user = NULL;
		/* check on local/global limits per host and per user@host */

		/*
		** local limits first to save CPU if any is hit.
		**	host check is done on the IP address.
		**	user check is done on the IDENT reply.
		*/
		if (ConfMaxHLocal(aconf) > 0 || ConfMaxUHLocal(aconf) > 0 ||
		    ConfMaxHGlobal(aconf) > 0 || ConfMaxUHGlobal(aconf) > 0 )
		{
			for ((user = hash_find_hostname(cptr->sockhost, NULL));
			     user; user = user->hhnext)
			{
				if (!mycmp(cptr->sockhost, user->host))
				{
					ghcnt++;
					if (MyConnect(user->bcptr))
					{
						hcnt++;
						if (!mycmp(user->bcptr->auth,
							   cptr->auth))
						{
							ucnt++;
							gucnt++;
						}
					}
					else
					{
						if (!mycmp(user->username,
							   cptr->user->username
							   ))
						{
							gucnt++;
						}
					}
					if (ConfMaxUHLocal(aconf) > 0 &&
					    ucnt >= ConfMaxUHLocal(aconf))
					{
						return -5;
					}
	
					if (ConfMaxHLocal(aconf) > 0 &&
					    hcnt >= ConfMaxHLocal(aconf))
					{
						return -4;
					}
					if (ConfMaxUHGlobal(aconf) > 0 &&
					    gucnt >= ConfMaxUHGlobal(aconf))
					{
						return -7;
					}
					if (ConfMaxHGlobal(aconf) > 0 &&
					     ghcnt >= ConfMaxHGlobal(aconf))
					{
						return -6;
					}
				}
			}
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
		if (IsIllegal(tmp))
			continue;
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

/*
 * find an O-line which matches the hostname and has the same "name".
 */
aConfItem *find_Oline(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];
	char	userip[USERLEN+HOSTLEN+3];

	SPRINTF(userhost, "%s@%s", cptr->username, cptr->sockhost);
	SPRINTF(userip, "%s@%s", cptr->username, 
#ifdef INET6
		(char *)inetntop(AF_INET6, (char *)&cptr->ip, mydummy,
			MYDUMMY_SIZE)
#else
		(char *)inetntoa((char *)&cptr->ip)
#endif
	);


	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (!(tmp->status & (CONF_OPS)) || !tmp->name || !tmp->host ||
			mycmp(tmp->name, name))
			continue;
		/*
		** Accept if the *real* hostname matches the host field or
		** the ip does.
		*/
		if (match(tmp->host, userhost) && match(tmp->host, userip) &&
			(!strchr(tmp->host, '/') 
			|| match_ipmask(tmp->host, cptr)))
			continue;
		if (tmp->clients < MaxLinks(Class(tmp)))
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
		if (IsIllegal(tmp))
			continue;
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
		if (IsIllegal(tmp))
			continue;
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
		if (IsIllegal(tmp))
			continue;
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
		if (IsIllegal(tmp))
			continue;
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
		if (!bcmp((char *)&tmp->ipnum, ip, sizeof(struct IN_ADDR)))
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
		logfiles_close();
		logfiles_open();
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

	tmp = &kconf;
	while ((tmp2 = *tmp))
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

	if (sig == 2)
		flush_cache();
	(void) initconf(0);
	close_listeners();

	/*
	 * Flush *unused* config entries.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
		if (!(tmp2->status & CONF_ILLEGAL) || tmp2->clients)
			tmp = &tmp2->next;
		else
		{
			*tmp = tmp2->next;
			tmp2->next = NULL;
			free_conf(tmp2);
		}
	
#ifdef CACHED_MOTD
	read_motd(IRCDMOTD_PATH);
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
	int ret;
#ifdef	M4_PREPROC
	int	pi[2], i;

	if (pipe(pi) == -1)
		return -1;
	switch(vfork())
	{
	case -1 :
		if (serverbooting)
		{
			fprintf(stderr,
			"Fatal Error: Unable to fork() m4 (%s)",
			strerror(errno));
		}
		return -1;
	case 0 :
		(void)close(pi[0]);
		if (pi[1] != 1)
		    {
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		/* If the server is booting, stderr is still open and
		 * user should receive error message */
		if (!serverbooting)
		{
			(void)dup2(1,2);
		}
		for (i = 3; i < MAXCONNECTIONS; i++)
			if (local[i])
				(void) close(i);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp("m4", "m4", IRCDM4_PATH, configfile, 0);
		if (serverbooting)
		{
			fprintf(stderr,"Fatal Error: Error executing m4 (%s)",
				strerror(errno));
		}
		report_error("Error executing m4 %s:%s", &me);
		_exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	if ((ret = open(configfile, O_RDONLY)) == - 1)
	{
		if (serverbooting)
		{
			fprintf(stderr,
			"Fatal Error: Can not open configuration file %s (%s)",
			configfile,strerror(errno));
		}
	}
	return ret;
#endif
}

/*
** char *ipv6_convert(char *orig)
** converts the original ip address to an standard form
** returns a pointer to a string.
*/

#ifdef	INET6
char	*ipv6_convert(orig)
char	*orig;
{
	char	*s, *t, *buf = NULL;
	int	i, j;
	int	len = 1;	/* for the '\0' in case of no @ */
	struct	in6_addr addr;
	char	dummy[MYDUMMY_SIZE];

	if ((s = strchr(orig, '@')))
	    {
		*s = '\0';
		len = strlen(orig) + 2;	/* +2 for '@' and '\0' */
		buf = (char *)MyMalloc(len);
		(void *)strcpy(buf, orig);
		buf[len - 2] = '@';
		buf[len - 1] = '\0'; 
		*s = '@';
		orig = s + 1;
	    }

	if ((s = strchr(orig, '/')))
	    {
		*s = '\0';
		s++;
	    }

	i = inetpton(AF_INET6, orig, addr.s6_addr);

	if (i > 0)
	    {
		t = inetntop(AF_INET6, addr.s6_addr, dummy, MYDUMMY_SIZE);
	    }
	
	j = len - 1;
	if (!((i > 0) && t))
		t = orig;

	len += strlen(t);
	buf = (char *)MyRealloc(buf, len);
	strcpy(buf + j, t);

	if (s)
	    {
		*(s-1) = '/'; /* put the '/' back, not sure it's needed tho */ 
		j = len;
		len += strlen(s) + 1;
		buf = (char *)MyRealloc(buf, len);
		buf[j - 1] = '/';
		strcpy(buf + j, s);
	    }

	return buf;
}
#endif

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
	char	line[512], c[80], *tmp2 = NULL, *tmp3 = NULL, *tmp4 = NULL;
	int	ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", configfile));
	if ((fd = openconf()) == -1)
	    {
#if defined(M4_PREPROC) && !defined(USE_IAUTH)
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
		if (line[1] != IRCDCONF_DELIMITER)
		    {
                        Debug((DEBUG_ERROR, "Bad config line: %s", line));
                        continue;
                    }
		if (aconf)
			free_conf(aconf);
		aconf = make_conf();

		if (tmp2)
			MyFree(tmp2);
		tmp3 = tmp4 = NULL;
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
			case 'D': /* auto connect restrictions */
			case 'd':
				aconf->status = CONF_DENY;
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
#ifdef	INET6
			if (aconf->status & 
				(CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER
				|CONF_CLIENT|CONF_RCLIENT|CONF_KILL
				|CONF_OTHERKILL|CONF_NOCONNECT_SERVER
				|CONF_OPERATOR|CONF_LOCOP|CONF_LISTEN_PORT
				|CONF_SERVICE))
				aconf->host = ipv6_convert(tmp);
			else
				DupString(aconf->host, tmp);
#else
			DupString(aconf->host, tmp);
#endif
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			aconf->port = 0;
			if (sscanf(tmp, "0x%x", &aconf->port) != 1 ||
			    aconf->port == 0)
				aconf->port = atoi(tmp);
			if (aconf->status == CONF_CONNECT_SERVER)
				DupString(tmp2, tmp);
			if (aconf->status == CONF_ZCONNECT_SERVER)
				DupString(tmp2, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			Class(aconf) = find_class(atoi(tmp));
			/* the following are only used for Y: */
			if ((tmp3 = getfield(NULL)) == NULL)
				break;
			tmp4 = getfield(NULL);
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
			if (atoi(aconf->host) >= 0)
				add_class(atoi(aconf->host),
					  atoi(aconf->passwd),
					  atoi(aconf->name), aconf->port,
					  tmp ? atoi(tmp) : 0,
					  tmp3 ? atoi(tmp3) : 1,
					  (tmp3 && index(tmp3, '.')) ?
					  atoi(index(tmp3, '.') + 1) : 1,
 					  tmp4 ? atoi(tmp4) : 1,
					  (tmp4 && index(tmp4, '.')) ?
					  atoi(index(tmp4, '.') + 1) : 1);
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
		if (aconf->status & (CONF_LISTEN_PORT|CONF_CLIENT|CONF_RCLIENT))
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
		if ((aconf->status & (CONF_CLIENT|CONF_RCLIENT)))
		{
			/* Parse I-line flags */
			if (tmp3)
			{
				aconf->flags = iline_flags_parse(tmp3);
			}
			else
			{
				aconf->flags = 0L;
			}
		}
		
		if (aconf->status & CONF_SERVICE)
			aconf->port &= SERVICE_MASK_ALL;
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
			if (me.info != DefInfo)
				MyFree(me.info);
			me.info = MyMalloc(REALLEN+1);
			strncpyzt(me.info, aconf->name, REALLEN+1);
			if (ME[0] == '\0' && aconf->host[0])
				strncpyzt(ME, aconf->host,
					  sizeof(ME));
			if (me.serv->sid[0] == '\0' && tmp && *tmp)
				strncpyzt(me.serv->sid, tmp,
					sizeof(me.serv->sid));
						
			if (aconf->port)
				setup_ping(aconf);
		    }
		
		if (aconf->status == CONF_ADMIN)
		{
			if (!networkname && tmp && *tmp)
			{
				if (strlen(tmp) < HOSTLEN)
				{
					DupString(networkname,tmp);
				}
			}
		}
		
		(void)collapse(aconf->host);
		(void)collapse(aconf->name);
		Debug((DEBUG_NOTICE,
		      "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port,
		      aconf->class ? ConfClass(aconf) : 0));

		if (aconf->status & (CONF_KILL|CONF_OTHERKILL))
		    {
			aconf->next = kconf;
			kconf = aconf;
		    }
		else
		    {
			aconf->next = conf;
			conf = aconf;
		    }
		aconf = NULL;
	    }
	if (aconf)
		free_conf(aconf);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	(void)close(fd);
#if defined(M4_PREPROC) && !defined(USE_IAUTH)
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

#ifdef INET6
	if(inetpton(AF_INET6, s, aconf->ipnum.s6_addr))
		;
#else
	if (isdigit(*s))
		aconf->ipnum.s_addr = inetaddr(s);
#endif
	else if ((hp = gethost_byname(s, &ln)))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
			sizeof(struct IN_ADDR));
#ifdef	INET6
	else
	{
		bcopy(minus_one, aconf->ipnum.s6_addr, IN6ADDRSZ);
		goto badlookup;
	}

#else
	if (aconf->ipnum.s_addr == -1)
		goto badlookup;
#endif

	return 0;

badlookup:
#ifdef INET6
	if (AND16(aconf->ipnum.s6_addr) == 255)
#else
	if (aconf->ipnum.s_addr == -1)
#endif
		bzero((char *)&aconf->ipnum, sizeof(struct IN_ADDR));
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
	char *host, *ip, *name, *ident, *check;
	aConfItem *tmp;
	int	now;

	if (!cptr->user)
		return 0;
	
	if (IsKlineExempt(cptr))
	{
		return 0;
	}

	host = cptr->sockhost;
#ifdef INET6
	ip = (char *) inetntop(AF_INET6, (char *)&cptr->ip, mydummy,
			       MYDUMMY_SIZE);
#else
	ip = (char *) inetntoa((char *)&cptr->ip);
#endif
	if (!strcmp(host, ip))
		ip = NULL; /* we don't have a name for the ip# */
	name = cptr->user->username;
	if (IsRestricted(cptr) && name[0] == '+')
	{
		/*
		** since we added '+' at the begining of valid
		** ident response, remove it here for kline
		** comparison --Beeth
		*/
		name++;
	}
	ident = cptr->auth;

	if (strlen(host)  > (size_t) HOSTLEN ||
            (name ? strlen(name) : 0) > (size_t) HOSTLEN)
		return (0);

	*reply = '\0';

	for (tmp = kconf; tmp; tmp = tmp->next)
	    {
		if (!doall && (BadPtr(tmp->passwd) || !isdigit(*tmp->passwd)))
			continue;
		if (!(tmp->status & (CONF_KILL | CONF_OTHERKILL)))
			continue; /* should never happen with kconf */
		if (!tmp->host || !tmp->name)
			continue;
		if (tmp->status == CONF_KILL)
			check = name;
		else
			check = ident;
		/* host & IP matching.. */
		if (!ip) /* unresolved */
		    {
			if (strchr(tmp->host, '/'))
			    {
				if (match_ipmask((*tmp->host == '=') ?
						 tmp->host+1: tmp->host, cptr))
					continue;
			    }
			else          
				if (match((*tmp->host == '=') ? tmp->host+1 :
					  tmp->host, host))
					continue;
		    }
		else if (*tmp->host == '=') /* numeric only */
			continue;
		else /* resolved */
			if (strchr(tmp->host, '/'))
			    {
				if (match_ipmask(tmp->host, cptr))
					continue;
			    }
			else
				if (match(tmp->host, ip) &&
				    match(tmp->host, host))
					continue;
		
		/* user & port matching */
		if ((!check || match(tmp->name, check) == 0) &&
		    (!tmp->port || (tmp->port == cptr->acpt->port)))   
		    {
#ifdef TIMEDKLINES
			now = 0;
			if (!BadPtr(tmp->passwd) && isdigit(*tmp->passwd) &&
			    !(now = check_time_interval(tmp->passwd, reply)))
				continue;
			if (now == ERR_YOUWILLBEBANNED)
				tmp = NULL;
#endif
			break;
		    }
	    }

	if (tmp && !BadPtr(tmp->passwd))
	{
		*comment = tmp->passwd;
	}
	else
	{
		*comment = NULL;
	}
	if (*reply)
	{
		/* TIMEDKLINES */
		sendto_one(cptr, reply, ME, now, cptr->name);
	}
	else if (tmp)
	{
		sendto_one(cptr, replies[ERR_YOUREBANNEDCREEP], 
			ME, cptr->name,
			BadPtr(tmp->name) ? "*" : tmp->name,
			BadPtr(tmp->host) ? "*" : tmp->host,
			*comment ? ":" : "",
			*comment ? *comment : "");
	}

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
	int l;

	if (index(key, '/') == NULL)
		return 0;
	l = ((char *)index(key, '/') - key) + 1;
	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((tmp->status == stat) && tmp->passwd && tmp->name &&
		    (strncasecmp(key, tmp->passwd, l) == 0) &&
		    (match(tmp->name, name) == 0) &&
		    (strpbrk(key + l, tmp->passwd + l)))
			break;
 	return (tmp ? -1 : 0);
}

#ifdef TIMEDKLINES
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
#endif /* TIMEDKLINES */

/*
** find_bounce
**	send a bounce numeric to a client.
**	fd is optional, and only makes sense if positive and when cptr is NULL
**	fd == -1 : not fd, class is a class number.
**	fd == -2 : not fd, class isn't a class number.
*/
void	find_bounce(cptr, class, fd)
aClient *cptr;
int	class, fd;
    {
	Reg	aConfItem	*aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
	    {
		if (aconf->status != CONF_BOUNCE)
			continue;

		if (fd >= 0)
			/*
			** early rejection,
			** connection class and hostname are unknown
			*/
			if (*aconf->host == '\0')
			    {
				char rpl[BUFSIZE];
				
				SPRINTF(rpl, replies[RPL_BOUNCE], ME, "unknown",
					aconf->name, aconf->port);
				strcat(rpl, "\r\n");
#ifdef INET6
				sendto(fd, rpl, strlen(rpl), 0, 0, 0);
#else
				send(fd, rpl, strlen(rpl), 0);
#endif
				return;
			    }
			else
				continue;

		/* fd < 0 */
		/*
		** "too many" type rejection, class is known.
		** check if B line is for a class #,
		** and if it is for a hostname.
		*/
		if (fd != -2 &&
		    !strchr(aconf->host, '.') && isdigit(*aconf->host))
		    {
			if (class != atoi(aconf->host))
				continue;
		    }
		else
			if (strchr(aconf->host, '/'))
			    {
				if (match_ipmask(aconf->host, cptr))
					continue;
			    }
			else if (match(aconf->host, cptr->sockhost))
				continue;

		sendto_one(cptr, replies[RPL_BOUNCE], ME, BadTo(cptr->name), aconf->name,
			   aconf->port);
		return;
	    }
	
    }

/*
** find_denied
**	for a given server name, make sure no D line matches any of the
**	servers currently present on the net.
*/
aConfItem *
find_denied(name, class)
    char *name;
    int class;
{
    aConfItem	*aconf;

    for (aconf = conf; aconf; aconf = aconf->next)
	{
	    if (aconf->status != CONF_DENY)
		    continue;
	    if (!aconf->name)
		    continue;
	    if (match(aconf->name, name) && aconf->port != class)
		    continue;
	    if (isdigit(*aconf->passwd))
		{
		    aConfItem	*aconf2;
		    int		ck = atoi(aconf->passwd);

		    for (aconf2 = conf; aconf2; aconf2 = aconf2->next)
			{
			    if (aconf2->status != CONF_NOCONNECT_SERVER)
				    continue;
			    if (!aconf2->class || ConfClass(aconf2) != ck)
				    continue;
			    if (find_client(aconf2->host, NULL))
				    return aconf2;
			}
		}
	    if (aconf->host)
		{
		    aServer	*asptr;

		    for (asptr = svrtop; asptr; asptr = asptr->nexts)
			    if (aconf->host &&
				!match(aconf->host, asptr->bcptr->name))
				    return aconf;
		}
	}
    return NULL;
}
