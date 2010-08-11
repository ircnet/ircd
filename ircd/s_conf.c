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
static const volatile char rcsid[] = "@(#)$Id: s_conf.c,v 1.195 2010/08/11 17:16:51 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_CONF_C
#include "s_externs.h"
#undef S_CONF_C
#ifdef ENABLE_CIDR_LIMITS
#include "patricia_ext.h"
#endif

#ifdef TIMEDKLINES
static	int	check_time_interval (char *, char *);
#endif
static	int	lookup_confhost (aConfItem *);

#ifdef CONFIG_DIRECTIVE_INCLUDE
#include "config_read.c"
#endif

aConfItem	*conf = NULL;
aConfItem	*kconf = NULL;
char		*networkname = NULL;
#ifdef TKLINE
aConfItem	*tkconf = NULL;
#endif

/* Parse I-lines flags from string.
 * D - Restricted, if no DNS.
 * I - Restricted, if no ident.
 * R - Restricted.
 * E - Kline exempt.
 * N - Do not resolve hostnames (show as IP).
 * F - Fallthrough to next I:line when password not matched
 */
long	iline_flags_parse(char *string)
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
#ifdef XLINE
	if (index(string,'e'))
	{
		tmp |= CFLAG_XEXEMPT;
	}
#endif
	if (index(string,'N'))
	{
		tmp |= CFLAG_NORESOLVE;
	}
	if (index(string,'M'))
	{
		tmp |= CFLAG_NORESOLVEMATCH;
	}
	if (index(string,'F'))
	{
		tmp |= CFLAG_FALL;
	}

	return tmp;
}

/* convert iline flags to human readable string */
char	*iline_flags_to_string(long flags)
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
#ifdef XLINE
	if (flags & CFLAG_XEXEMPT)
	{
		*s++ = 'e';
	}
#endif
	if (flags & CFLAG_NORESOLVE)
	{
		*s++ = 'N';
	}
	if (flags & CFLAG_NORESOLVEMATCH)
	{
		*s++ = 'M';
	}
	if (flags & CFLAG_FALL)
	{
		*s++ = 'F';
	}
	if (s == ifsbuf)
	{
		*s++ = '-';
	}
	*s++ = '\0';
	
	return ifsbuf;
}
/* Convert P-line flags from string
 * D - delayed port
 * S - server only port
 */
long pline_flags_parse(char *string)
{
	long tmp = 0;
	if (index(string, 'D'))
	{
		tmp |= PFLAG_DELAYED;
	}
	if (index(string, 'S'))
	{
		tmp |= PFLAG_SERVERONLY;
	}
	return tmp;
}
/* Convert P-line flags from integer to string
 */
char *pline_flags_to_string(long flags)
{
	static char pfsbuf[BUFSIZE];
	char *s = pfsbuf;
	
	if (flags & PFLAG_DELAYED)
	{
		*s++ = 'D';
	}
			
	if (flags & PFLAG_SERVERONLY)
	{
		*s++ = 'S';
	}
	
	if (s == pfsbuf)
	{
		*s++ = '-';
	}

	*s++ = '\0';
	return pfsbuf;
}

/* convert oline flags to human readable string */
char	*oline_flags_to_string(long flags)
{
	static char ofsbuf[BUFSIZE];
	char *s = ofsbuf;

	if (flags & ACL_LOCOP)
		*s++ = 'L';
	if (flags & ACL_KILLREMOTE)
		*s++ = 'K';
	else if (flags & ACL_KILLLOCAL)
		*s++ = 'k';
	if (flags & ACL_SQUITREMOTE)
		*s++ ='S';
	else if (flags & ACL_SQUITLOCAL)
		*s++ ='s';
	if (flags & ACL_CONNECTREMOTE)
		*s++ ='C';
	else if (flags & ACL_CONNECTLOCAL)
		*s++ ='c';
	if (flags & ACL_CLOSE)
		*s++ ='l';
	if (flags & ACL_HAZH)
		*s++ ='h';
	if (flags & ACL_DNS)
		*s++ ='d';
	if (flags & ACL_REHASH)
		*s++ ='r';
	if (flags & ACL_RESTART)
		*s++ ='R';
	if (flags & ACL_DIE)
		*s++ ='D';
	if (flags & ACL_SET)
		*s++ ='e';
	if (flags & ACL_TKLINE)
		*s++ ='T';
	if (flags & ACL_KLINE)
		*s++ ='q';
#ifdef CLIENTS_CHANNEL
	if (flags & ACL_CLIENTS)
		*s++ ='&';
#endif
	if (flags & ACL_NOPENALTY)
		*s++ = 'P';
	if (flags & ACL_CANFLOOD)
		*s++ = 'p';
	if (flags & ACL_TRACE)
		*s++ = 't';
#ifdef ENABLE_SIDTRACE
	if (flags & ACL_SIDTRACE)
		*s++ = 'v';
#endif
	if (s == ofsbuf)
		*s++ = '-';
	*s++ = '\0';
	return ofsbuf;
}
/* convert string from config to flags */
long	oline_flags_parse(char *string)
{
	long tmp = 0;
	char *s;
	
	for (s = string; *s; s++)
	{
		switch(*s)
		{
		case 'L': tmp |= ACL_LOCOP; break;
		case 'A': tmp |= (ACL_ALL & 
			~(ACL_LOCOP|ACL_CLIENTS|ACL_NOPENALTY|ACL_CANFLOOD));
			break;
		case 'K': tmp |= ACL_KILL; break;
		case 'k': tmp |= ACL_KILLLOCAL; break;
		case 'S': tmp |= ACL_SQUIT; break;
		case 's': tmp |= ACL_SQUITLOCAL; break;
		case 'C': tmp |= ACL_CONNECT; break;
		case 'c': tmp |= ACL_CONNECTLOCAL; break;
		case 'l': tmp |= ACL_CLOSE; break;
		case 'h': tmp |= ACL_HAZH; break;
		case 'd': tmp |= ACL_DNS; break;
		case 'r': tmp |= ACL_REHASH; break;
		case 'R': tmp |= ACL_RESTART; break;
		case 'D': tmp |= ACL_DIE; break;
		case 'e': tmp |= ACL_SET; break;
		case 'T': tmp |= ACL_TKLINE; break;
		case 'q': tmp |= ACL_KLINE; break;
#ifdef CLIENTS_CHANNEL
		case '&': tmp |= ACL_CLIENTS; break;
#endif
		case 'P': tmp |= ACL_NOPENALTY; break;
		case 'p': tmp |= ACL_CANFLOOD; break;
		case 't': tmp |= ACL_TRACE; break;
#ifdef ENABLE_SIDTRACE
		case 'v': tmp |= ACL_SIDTRACE; break;
#endif
		}
	}
	if (tmp & ACL_LOCOP)
		tmp &= ~ACL_ALL_REMOTE;
#ifdef OPER_KILL
# ifndef OPER_KILL_REMOTE
	tmp &= ~ACL_KILLREMOTE;
# endif
#else
	tmp &= ~ACL_KILL;
#endif
#ifndef OPER_REHASH
	tmp &= ~ACL_REHASH;
#endif
#ifndef OPER_SQUIT
	tmp &= ~ACL_SQUIT;
#endif
#ifndef OPER_SQUIT_REMOTE
	tmp &= ~ACL_SQUITREMOTE;
#endif
#ifndef OPER_CONNECT
	tmp &= ~ACL_CONNECT;
#endif
#ifndef OPER_RESTART
	tmp &= ~ACL_RESTART;
#endif
#ifndef OPER_DIE
	tmp &= ~ACL_DIE;
#endif
#ifndef OPER_SET
	tmp &= ~ACL_SET;
#endif
#ifndef OPER_TKLINE
	tmp &= ~ACL_TKLINE;
#endif
#ifndef OPER_KLINE
	tmp &= ~ACL_KLINE;
#endif
	return tmp;
}
/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void	det_confs_butmask(aClient *cptr, int mask)
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
int    match_ipmask(char *mask, aClient *cptr, int maskwithusername)
{
	int	m;
	char	*p;
	struct  IN_ADDR addr;
	char	dummy[128];
	char	*omask;
	u_long	lmask;
#ifdef	INET6
	int	j;
#endif
 
	omask = mask;
	strncpyzt(dummy, mask, sizeof(dummy));
	mask = dummy;
	if (maskwithusername && (p = index(mask, '@')))
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
	if (!m)
		return 0;       /* x.x.x.x/0 always matches */
#ifndef	INET6
	if (m < 0 || m > 32)
		goto badmask;
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
	if (IN6_IS_ADDR_V4MAPPED(&addr))
	{
		if (m <= 32)
			m += 96;
		if (m <= 96)
			goto badmask;
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
	if (maskwithusername)
	sendto_flag(SCH_ERROR, "Ignoring bad mask: %s", omask);
	return -1;
}

/*
 * find the first (best) I line to attach.
 */

#define UHConfMatch(x, y, z)	(match((x), (index((x), '@') ? (y) : (y)+(z))))

int	attach_Iline(aClient *cptr, struct hostent *hp, char *sockhost)
{
	Reg	aConfItem	*aconf;
	char	uhost[HOSTLEN+USERLEN+2];
	char	uaddr[HOSTLEN+USERLEN+2];
	int	ulen = strlen(cptr->username) + 1; /* for '@' */
	int	retval = -2; /* EXITC_NOILINE in register_user() */

	/* We fill uaddr and uhost now, before aconf loop. */
	sprintf(uaddr, "%s@%s", cptr->username, sockhost);
	if (hp)
	{
		char	fullname[HOSTLEN+1];

		/* If not for add_local_domain, I wouldn't need this
		** fullname. Can't we add_local_domain somewhere in
		** dns code? --B. */
		strncpyzt(fullname, hp->h_name, sizeof(fullname));
		add_local_domain(fullname, HOSTLEN - strlen(fullname));
		Debug((DEBUG_DNS, "a_il: %s->%s", sockhost, fullname));
		sprintf(uhost, "%s@%s", cptr->username, fullname);
	}
	/* all uses of uhost are guarded by if (hp), so no need to zero it. */

	for (aconf = conf; aconf; aconf = aconf->next)
	{
		if ((aconf->status != CONF_CLIENT))
		{
			continue;
		}
		if (aconf->port && aconf->port != cptr->acpt->port)
		{
			continue;
		}
		/* aconf->name can be NULL with wrong I:line in the config
		** (without all required fields). If aconf->host can be NULL,
		** I don't know. Anyway, this is an error! --B. */
		if (!aconf->host || !aconf->name)
		{
			/* Try another I:line. */
			continue;
		}

		/* If anything in aconf->name... */
		if (*aconf->name)
		{
			int	namematched = 0;

			if (hp)
			{
				if (!UHConfMatch(aconf->name, uhost, ulen))
				{
					namematched = 1;
				}
			}
			/* Note: here we could do else (!hp) and try to
			** check if aconf->name is '*' or '*@*' and
			** if so, allow the client. But not doing so
			** gives us nice opportunity to distinguish
			** between '*' in aconf->name (requires DNS)
			** and empty aconf->name (matches any). --B. */

			/* Require name to match before checking addr fields. */
			if (namematched == 0)
			{
				/* Try another I:line. */
				continue;
			}
		} /* else empty aconf->name, match any hostname. */

		if (*aconf->host)
		{
#ifdef UNIXPORT
			if (IsUnixSocket(cptr) && aconf->host[0] == '/')
			{
				if (match(aconf->host, uaddr+ulen))
				{
					/* Try another I:line. */
					continue;
				}
			}
			else
#endif
			if (strchr(aconf->host, '/'))	/* 1.2.3.0/24 */
			{
				
				/* match_ipmask takes care of checking
				** possible username if aconf->host has '@' */
				if (match_ipmask(aconf->host, cptr, 1))
				{
					/* Try another I:line. */
					continue;
				}
			}
			else	/* 1.2.3.* */
			{
				if (UHConfMatch(aconf->host, uaddr, ulen))
				{
					/* Try another I:line. */
					continue;
				}
			}
		} /* else empty aconf->host, match any ipaddr */

		/* Password check, if I:line has it. If 'F' flag, try another
		** I:line, otherwise bail out and reject client. */
		if (!BadPtr(aconf->passwd) &&
			!StrEq(cptr->passwd, aconf->passwd))
		{
			if (IsConfFallThrough(aconf))
			{
				continue;
			}
			else
			{
				sendto_one(cptr, replies[ERR_PASSWDMISMATCH],
					ME, BadTo(cptr->name));
				retval = -8; /* EXITC_BADPASS */
				break;
			}
		}

		/* Various cases of +r. */
		if (IsConfRestricted(aconf) ||
			(!hp && IsConfRNoDNS(aconf)) ||
			(!(cptr->flags & FLAGS_GOTID) && IsConfRNoIdent(aconf)))
		{
			SetRestricted(cptr);
		}
		if (IsConfKlineExempt(aconf))
		{
			SetKlineExempt(cptr);
		}
#ifdef XLINE
		if (IsConfXlineExempt(aconf))
		{
			ClearXlined(cptr);
		}
#endif

		/* Copy uhost (hostname) over sockhost, if conf flag permits. */
		if (hp && !IsConfNoResolve(aconf))
		{
			get_sockhost(cptr, uhost+ulen);
		}
		/* Note that attach_conf() should not return -2. */
		if ((retval = attach_conf(cptr, aconf)) < -1)
		{
			find_bounce(cptr, ConfClass(aconf), -1);
		}
		break;
	}
	if (retval == -2)
	{
		find_bounce(cptr, 0, -2);
	}
	return retval;
}

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem	*count_cnlines(Link *lp)
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

#ifdef ENABLE_CIDR_LIMITS
static int	add_cidr_limit(aClient *cptr, aConfItem *aconf)
{
	patricia_node_t *pnode;

	if(aconf->class->cidr_amount == 0 || aconf->class->cidr_len == 0)
		return -1;

	pnode = patricia_match_ip(ConfCidrTree(aconf), &cptr->ip);

	/* doesnt exist, create and then allow */
	if(pnode == NULL)
	{
		pnode = patricia_make_and_lookup_ip(ConfCidrTree(aconf),
					&cptr->ip,
					aconf->class->cidr_len);

		if(pnode == NULL)
			return -1;

		pnode->data++;
		return 1;
	}

	if((long)pnode->data >= aconf->class->cidr_amount)
		return 0;

	pnode->data++;
	return 1;
}

static void	remove_cidr_limit(aClient *cptr, aConfItem *aconf)
{
	patricia_node_t *pnode;

	if(ConfMaxCidrAmount(aconf) == 0 || ConfCidrLen(aconf) == 0)
		return;

	pnode = patricia_match_ip(ConfCidrTree(aconf), &cptr->ip);

	if(pnode == NULL)
		return;

	pnode->data--;

	if(((unsigned long) pnode->data) == 0)
		patricia_remove(ConfCidrTree(aconf), pnode);
}
#endif /* ENABLE_CIDR_LIMITS */

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int	detach_conf(aClient *cptr, aConfItem *aconf)
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
				{
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
#ifdef ENABLE_CIDR_LIMITS
					if ((aconf->status & CONF_CLIENT))
						remove_cidr_limit(cptr, aconf);
#endif
				}

       				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
		 		    {
					free_class(Class(aconf));
					Class(aconf) = NULL;
				    }
			     }
			if (aconf && --aconf->clients <= 0 && IsIllegal(aconf))
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

static	int	is_attached(aConfItem *aconf, aClient *cptr)
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
**	Non-zero return value is used in register_user().
*/
int	attach_conf(aClient *cptr, aConfItem *aconf)
{
	Reg	Link	*lp;

	if (is_attached(aconf, cptr))
		return 1;
	if (IsIllegal(aconf))
		return -1; /* EXITC_FAILURE, hmm */
	if ((aconf->status & (CONF_OPERATOR | CONF_CLIENT )))
	    {
		if (
#ifdef YLINE_LIMITS_OLD_BEHAVIOUR
			aconf->clients >= ConfMaxLinks(aconf)
#else
			ConfLinks(aconf) >= ConfMaxLinks(aconf)
#endif
			&& ConfMaxLinks(aconf) > 0)
			return -3;    /* EXITC_YLINEMAX */
	    }

	if ((aconf->status & CONF_CLIENT))
	{
		int hcnt = 0, ucnt = 0;
		int ghcnt = 0, gucnt = 0;
		anUser *user = NULL;
		/* check on local/global limits per host and per user@host */

#ifdef ENABLE_CIDR_LIMITS
		if(!add_cidr_limit(cptr, aconf))
			return -4; /* EXITC_LHMAX */
#endif
		/*
		** local limits first to save CPU if any is hit.
		**	host check is done on the IP address.
		**	user check is done on the IDENT reply.
		*/
		if (ConfMaxHLocal(aconf) > 0 || ConfMaxUHLocal(aconf) > 0 ||
		    ConfMaxHGlobal(aconf) > 0 || ConfMaxUHGlobal(aconf) > 0 )
		{
#ifdef YLINE_LIMITS_IPHASH
			for ((user = hash_find_ip(cptr->user->sip, NULL));
			     user; user = user->iphnext)
				if (!mycmp(cptr->user->sip, user->sip))
#else
			for ((user = hash_find_hostname(cptr->sockhost, NULL));
			     user; user = user->hhnext)
				if (!mycmp(cptr->sockhost, user->host))
#endif
				{
					ghcnt++;
					if (ConfMaxHGlobal(aconf) > 0 &&
					     ghcnt >= ConfMaxHGlobal(aconf))
					{
						return -6; /* EXITC_GHMAX */
					}
					if (MyConnect(user->bcptr))
					{
						hcnt++;
						if (ConfMaxHLocal(aconf) > 0 &&
						    hcnt >= ConfMaxHLocal(aconf))
						{
							return -4; /* EXITC_LHMAX */
						}
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
						return -5; /* EXITC_LUHMAX */
					}
	
					if (ConfMaxUHGlobal(aconf) > 0 &&
					    gucnt >= ConfMaxUHGlobal(aconf))
					{
						return -7; /* EXITC_GUHMAX */
					}
				}
		}
	}


	lp = make_link();
	istat.is_conflink++;
	lp->next = cptr->confs;
	lp->value.aconf = aconf;
	cptr->confs = lp;
	cptr->ping = get_client_ping(cptr);
	aconf->clients++;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem	*find_admin(void)
{
	Reg	aConfItem	*aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
}

aConfItem	*find_me(void)
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
aConfItem	*attach_confs(aClient *cptr, char *name, int statmask)
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
aConfItem	*attach_confs_host(aClient *cptr, char *host, int statmask)
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
aConfItem	*find_conf_exact(char *name, char *user, char *host, 
		int statmask)
{
	Reg	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];

	sprintf(userhost, "%s@%s", user, host);

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
		if (tmp->status & CONF_OPERATOR)
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
aConfItem	*find_Oline(char *name, aClient *cptr)
{
	Reg	aConfItem *tmp, *tmp2 = NULL;
	char	userhost[USERLEN+HOSTLEN+3];
	char	userip[USERLEN+HOSTLEN+3];

	sprintf(userhost, "%s@%s", cptr->username, cptr->sockhost);
	sprintf(userip, "%s@%s", cptr->username, cptr->user->sip);

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
			|| match_ipmask(tmp->host, cptr, 1)))
			continue;
		if (tmp->clients < MaxLinks(Class(tmp)))
			return tmp;
		else
			tmp2 = tmp;
	    }
	return tmp2;
}


aConfItem	*find_conf_name(char *name, int statmask)
{
	Reg	aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the hostname matches name field
		** of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	    }
	return NULL;
}

aConfItem	*find_conf(Link *lp, char *name, int statmask)
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
aConfItem	*find_conf_host(Link *lp, char *host, int statmask)
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

aConfItem	*find_conf_host_sid(Link *lp, char *host, char *sid, int statmask)
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
	 	     (tmp->host && !match(tmp->host, host))) &&
			(!tmp->passwd || !tmp->passwd[0] ||
				!match(tmp->passwd, sid)) )
		{
			return tmp;
		}
	    }
	return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem	*find_conf_ip(Link *lp, char *ip, char *user, int statmask)
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
aConfItem	*find_conf_entry(aConfItem *aconf, u_int mask)
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
int	rehash(aClient *cptr, aClient *sptr, int sig)
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

	if (sig == 'a')
		start_iauth(2);	/* 2 means kill iauth first */
	if (sig == 'd')
	{
		flush_cache();
		close(resfd);
		resfd = init_resolver(0x1f);
	}
#ifdef TKLINE
	if (sig == 't')
		tkline_expire(1);
#endif
	(void) initconf(0);
	close_listeners();
	reopen_listeners();

	/*
	 * Flush *unused* config entries.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
		if (!(tmp2->status & CONF_ILLEGAL) || (tmp2->clients > 0))
			tmp = &tmp2->next;
		else
		{
			*tmp = tmp2->next;
			tmp2->next = NULL;
			free_conf(tmp2);
		}
	
	read_motd(IRCDMOTD_PATH);
	if (rehashed == 1)
	{
		/* queue another rehash for later */
		rehashed = 2;
	}
	else if (rehashed == 0)
	{
		rehashed = 1;
	}
	mysrand(timeofday);
	return ret;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be the file direct or one end
 * of a pipe from m4.
 */
int	openconf(void)
{
#ifdef	M4_PREPROC
	int	pi[2], i;
# ifdef HAVE_GNU_M4
	char	*includedir, *includedirptr;

	includedir = strdup(IRCDM4_PATH);
	includedirptr = strrchr(includedir, '/');
	if (includedirptr)
		*includedirptr = '\0';
# endif
#else
	int ret;
#endif

#ifdef	M4_PREPROC
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
		(void)execlp(M4_PATH, "m4",
#ifdef HAVE_GNU_M4
#ifdef USE_M4_PREFIXES
			"-P",
#endif
			"-I", includedir,
#endif
#ifdef INET6
			"-DINET6",
#endif
			IRCDM4_PATH, configfile, (char *) NULL);
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
	if ((ret = open(configfile, O_RDONLY)) == -1)
	{
		if (serverbooting)
		{
			fprintf(stderr,
			"Fatal Error: Can not open configuration file %s (%s)\n",
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
char	*ipv6_convert(char *orig)
{
	char	*s, *t, *buf = NULL;
	int	i, j;
	int	len = 1;	/* for the '\0' in case of no @ */
	struct	in6_addr addr;

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
		t = inetntop(AF_INET6, addr.s6_addr, ipv6string, sizeof(ipv6string));
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

int 	initconf(int opt)
{
	static	char	quotes[9][2] = {{'b', '\b'}, {'f', '\f'}, {'n', '\n'},
					{'r', '\r'}, {'t', '\t'}, {'v', '\v'},
					{'\\', '\\'}, { 0, 0}};
	Reg	char	*tmp, *s;
	int	fd, i;
	char	*tmp2 = NULL, *tmp3 = NULL, *tmp4 = NULL;
#ifdef ENABLE_CIDR_LIMITS
	char	*tmp5 = NULL;
#endif
	int	ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	char	*line;
	aConfig	*ConfigTop, *p;
	FILE	*fdn;
#else
	char	line[512], c[80];
#endif

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", configfile));
	if ((fd = openconf()) == -1)
	    {
#if defined(M4_PREPROC) && !defined(USE_IAUTH)
		(void)wait(0);
#endif
		return -1;
	    }
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	fdn = fdopen(fd, "r");
	if (fdn == NULL)
	{
		if (serverbooting)
		{
			fprintf(stderr,
			"Fatal Error: Can not open configuration file %s (%s)\n",
			configfile,strerror(errno));
		}
		return -1;
	}
	ConfigTop = config_read(fdn, 0, new_config_file(configfile, NULL, 0));
	for(p = ConfigTop; p; p = p->next)
#else
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
#endif
	{
#if defined(CONFIG_DIRECTIVE_INCLUDE)
		line = p->line;
#else
		line[i] = '\0';
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1) > 0)
			if ((tmp = (char *)index(c, '\n')))
			    {
				*tmp = 0;
				break;
			    }
#endif
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
#ifdef ENABLE_CIDR_LIMITS
		tmp5 = NULL;
#endif
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
			case 'i' : /* Restricted client */
				aconf->flags |= CFLAG_RESTRICTED;
			case 'I':
				aconf->status = CONF_CLIENT;
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
			case 'o':
				aconf->flags |= ACL_LOCOP;
			case 'O':
				aconf->status = CONF_OPERATOR;
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
			case 'V': /* Server link version requirements */
				aconf->status = CONF_VER;
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
#ifdef XLINE
			case 'X':
				aconf->status = CONF_XLINE;
				break;
#endif
		    default:
			Debug((DEBUG_ERROR, "Error in config file: %s", line));
			break;
		    }
		if (IsIllegal(aconf))
			continue;

		do
		{
			if ((tmp = getfield(NULL)) == NULL)
				break;
#ifdef	INET6
			if (aconf->status & 
				(CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER
				|CONF_CLIENT|CONF_KILL
				|CONF_OTHERKILL|CONF_NOCONNECT_SERVER
				|CONF_OPERATOR|CONF_LISTEN_PORT
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
#ifdef XLINE
			if (aconf->status == CONF_XLINE)
			{
				DupString(aconf->name2, tmp);
				if ((tmp = getfield(NULL)) == NULL)
					break;
				DupString(aconf->name3, tmp);
				if ((tmp = getfield(NULL)) == NULL)
					break;
				DupString(aconf->source_ip, tmp);
				break;
			}
#endif
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
			/* used in Y: local limits and I: and P: flags */
			if ((tmp3 = getfield(NULL)) == NULL)
				break;
			/* used in Y: global limits */
			if((tmp4 = getfield(NULL)) == NULL)
				break;
#ifdef ENABLE_CIDR_LIMITS
			tmp5 = getfield(NULL);
#endif
		} while (0); /* to use break without compiler warnings */
		istat.is_confmem += aconf->host ? strlen(aconf->host)+1 : 0;
		istat.is_confmem += aconf->passwd ? strlen(aconf->passwd)+1 :0;
		istat.is_confmem += aconf->name ? strlen(aconf->name)+1 : 0;
		istat.is_confmem += aconf->name2 ? strlen(aconf->name2)+1 : 0;
#ifdef XLINE
		istat.is_confmem += aconf->name3 ? strlen(aconf->name3)+1 : 0;
#endif
		istat.is_confmem += aconf->source_ip ? strlen(aconf->source_ip)+1 : 0;

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
					  (tmp && index(tmp, '.')) ?
					  atoi(index(tmp, '.') + 1) : 0,
					  tmp3 ? atoi(tmp3) : 1,
					  (tmp3 && index(tmp3, '.')) ?
					  atoi(index(tmp3, '.') + 1) : 1,
 					  tmp4 ? atoi(tmp4) : 1,
					  (tmp4 && index(tmp4, '.')) ?
					  atoi(index(tmp4, '.') + 1) : 1
#ifdef ENABLE_CIDR_LIMITS
					  , tmp5
#endif
				);
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
			
			/* any flags in this line? */
			if (tmp3)
			{
				aconf->flags |=
					((aconf->status == CONF_CLIENT) ?
					iline_flags_parse(tmp3) :
					pline_flags_parse(tmp3));
			}

			/* trying to find exact conf line in already existing
			 * conf, so we don't delete old one, just update it */
			if (
#ifdef FASTER_ILINE_REHASH
				(aconf->status & CONF_LISTEN_PORT) &&
#endif
				(bconf = find_conf_entry(aconf, aconf->status)))
			{
				/* we remove bconf (already existing) from conf
				 * so that we can add it back uniformly at the
				 * end of while(dgets) loop. --B. */
				delist_conf(bconf);
				bconf->status &= ~CONF_ILLEGAL;
				/* aconf is a new item, it can contain +r flag
				 * (from lowercase i:lines). In any case we
				 * don't want old flags to remain. --B. */
				bconf->flags = aconf->flags;
				/* in case class was changed */
				if (aconf->status == CONF_CLIENT &&
					aconf->class != bconf->class)
				{
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
					bconf->class->links += bconf->clients;
				}
				/* free new one, assign old one to aconf */
				free_conf(aconf);
				aconf = bconf;
			}
			else	/* no such conf line was found */
			{
				if (aconf->host &&
					aconf->status == CONF_LISTEN_PORT)
				{
					(void)add_listener(aconf);
				}
			}
		}
		if (aconf->status & CONF_SERVICE)
			aconf->port &= SERVICE_MASK_ALL;
		if (aconf->status & (CONF_SERVER_MASK|CONF_SERVICE))
		{
			char *hostptr = NULL;

			/* since it's u@h syntax, let's ignore user part
			   in checks below --B. */
			hostptr = index(aconf->host, '@');
			if (hostptr != NULL)
				hostptr++;	/* move ptr after '@' */
			else
				hostptr = aconf->host;

			if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS
				|| !hostptr || index(hostptr, '*')
				|| index(hostptr,'?') || !aconf->name)
				continue;	/* next config line */
		}

		if (aconf->status &
		    (CONF_SERVER_MASK|CONF_OPERATOR|CONF_SERVICE))
			if (!index(aconf->host, '@') && *aconf->host != '/')
			    {
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				sprintf(newhost, "*@%s", aconf->host);
				MyFree(aconf->host);
				aconf->host = newhost;
				istat.is_confmem += 2;
			    }
		if (tmp3 && (aconf->status & CONF_OPERATOR))
		{
			aconf->flags |= oline_flags_parse(tmp3);
			/* remove this when removing o: lines --B. */
			if (aconf->flags & ACL_LOCOP)
				aconf->flags &= ~ACL_ALL_REMOTE;
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
			if (tmp3)
			{
				DupString(aconf->source_ip, tmp3);
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
			if (me.serv->namebuf[0] == '\0' && aconf->host[0])
				strncpyzt(me.serv->namebuf, aconf->host,
					  sizeof(me.serv->namebuf));
			if (me.serv->sid[0] == '\0' && tmp && *tmp)
			{
				for(i = 0; i < sizeof(me.serv->sid); i++)
					me.serv->sid[i] = toupper(tmp[i]);
			}
						
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
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	config_free(ConfigTop);
#endif
	if (aconf)
		free_conf(aconf);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	(void)fclose(fdn);
#else
	(void)close(fd);
#endif
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
static	int	lookup_confhost(aConfItem *aconf)
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

int	find_kill(aClient *cptr, int timedklines, char **comment)
{
#ifdef TIMEDKLINES
	static char	reply[256];
	int		now = 0;
#endif
	char		*host, *ip, *name, *ident, *check;
	aConfItem	*tmp;
#ifdef TKLINE
	int		tklines = 1;
#endif

	if (!cptr->user)
		return 0;
	
	if (IsKlineExempt(cptr))
	{
		return 0;
	}

	host = cptr->sockhost;
#ifdef INET6
	ip = (char *) inetntop(AF_INET6, (char *)&cptr->ip, ipv6string,
			       sizeof(ipv6string));
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

#ifdef TIMEDKLINES
	*reply = '\0';
#endif

findkline:
	tmp = 
#ifdef TKLINE
		tklines ? tkconf :
#endif
		kconf;
	for (; tmp; tmp = tmp->next)
	{
#ifdef TIMEDKLINES
		if (timedklines && (BadPtr(tmp->passwd) || !isdigit(*tmp->passwd)))
			continue;
#endif
		if (!(tmp->status & (
#ifdef TKLINE
			tklines ? (CONF_TKILL | CONF_TOTHERKILL) :
#endif
			(CONF_KILL | CONF_OTHERKILL))))
			continue; /* should never happen with kconf */
		if (!tmp->host || !tmp->name)
			continue;
#ifdef TKLINE
		/* this TK already expired */
		if (tklines && tmp->hold < timeofday)
			continue;
#endif
		if (tmp->status == (
#ifdef TKLINE
			tklines ? CONF_TKILL : 
#endif
			CONF_KILL))
			check = name;
		else
			check = ident;
		/* host & IP matching.. */
		if (!ip) /* unresolved */
		    {
			if (strchr(tmp->host, '/'))
			    {
				if (match_ipmask((*tmp->host == '=') ?
						 tmp->host+1: tmp->host, cptr, 1))
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
				if (match_ipmask(tmp->host, cptr, 1))
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
#ifdef TKLINE
	if (!tmp && tklines)
	{
		tklines = 0;
		goto findkline;
	}
#endif
	if (tmp && !BadPtr(tmp->passwd))
	{
		*comment = tmp->passwd;
	}
	else
	{
		*comment = NULL;
	}
#ifdef TIMEDKLINES
	if (*reply)
	{
		sendto_one(cptr, reply, ME, now, cptr->name);
	}
	else
#endif
	if (tmp)
	{
		sendto_one(cptr, replies[ERR_YOUREBANNEDCREEP], 
			ME, cptr->name,
			BadPtr(tmp->name) ? "*" : tmp->name,
			BadPtr(tmp->host) ? "*" : tmp->host,
			*comment ? ": " : "",
			*comment ? *comment : "");
	}

	return (tmp ? -1 : 0);
}

/*
 * For type stat, check if both name and host masks match.
 * Return -1 for match, 0 for no-match.
 */
int	find_two_masks(char *name, char *host, int stat)
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
int	find_conf_flags(char *name, char *key, int stat)
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

static	int	check_time_interval(char *interval, char *reply)
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
void	find_bounce(aClient *cptr, int class, int fd)
{
	Reg	aConfItem	*aconf;

	if (fd < 0 && cptr == NULL)
	{
		/* nowhere to send error to */
		return;
	}

	for (aconf = conf; aconf; aconf = aconf->next)
	{
		if (aconf->status != CONF_BOUNCE)
		{
			continue;
		}

		if (fd >= 0)
		{
			/*
			** early rejection,
			** connection class and hostname are unknown
			*/
			if (*aconf->host == '\0')
			{
				char rpl[BUFSIZE];
				
				sprintf(rpl, replies[RPL_BOUNCE], ME, "unknown",
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
			{
				continue;
			}
		}

		/* fd < 0 */
		/*
		** "too many" type rejection, class is known.
		** check if B line is for a class #,
		** and if it is for a hostname.
		*/
		if (fd != -2 &&
		    !strchr(aconf->host, '.') &&
			(isdigit(*aconf->host) || *aconf->host == '-'))
		{
			if (class != atoi(aconf->host))
			{
				continue;
			}
		}
		else
		{
			if (strchr(aconf->host, '/'))
			{
				if (match_ipmask(aconf->host, cptr, 1))
					continue;
			}
			else if (match(aconf->host, cptr->sockhost))
			{
				continue;
			}
		}

		sendto_one(cptr, replies[RPL_BOUNCE], ME, BadTo(cptr->name),
			aconf->name, aconf->port);
		return;
	}
	
}

/*
** find_denied
**	for a given server name, make sure no D line matches any of the
**	servers currently present on the net.
*/
aConfItem	*find_denied(char *name, int class)
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
				if (find_client(aconf2->name, NULL))
					return aconf2;
			}
		}
		if (aconf->host)
		{
			aServer	*asptr;
			char	*host = aconf->host;
			int	reversed = 0;

			if (*host == '!')
			{
				host++;
				reversed = 1;
			}
			for (asptr = svrtop; asptr; asptr = asptr->nexts)
				if (!match(host, asptr->bcptr->name))
					break;

			if (!reversed && asptr)
				return aconf;
			if (reversed && !asptr)
				/* anything but NULL; tho using it may give
				** funny results in calling function */
				return conf;
		}
	}
	return NULL;
}

#ifdef TKLINE
/*
 * Parses 0w1d2h3m4s timeformat, filling in output variable in seconds.
 * Returns 0 if everything went ok.
 */
int	wdhms2sec(char *input, time_t *output)
{
#ifndef TKLINE_MULTIPLIER
#define TKLINE_MULTIPLIER 60
#endif
	int multi;
	int tmp = 0;
	char *s;

	*output = 0;

	if (!input) return 0;

	s = input;
	while (*s)
	{
		switch(tolower(*s))
		{
		case 'w':
			multi = 604800; break;
		case 'd':
			multi = 86400; break;
		case 'h':
			multi = 3600; break;
		case 'm':
			multi = 60; break;
		case 's':
			multi = 1; break;
		default:
			if (isdigit(*s))
			{
				tmp = atoi(s);
				while (isdigit(*s))
					s++;
				if (!*s)
				{
					*output += TKLINE_MULTIPLIER * tmp;
				}
				continue;
			}
			else
				return -1;
		}
		*output += multi * tmp;
		s++;
	}
	return 0;
}
#endif

#if defined(TKLINE) || defined(KLINE)
/* 
 * Adds t/kline to t/kconf.
 * If tkline already existed, its expire time is updated.
 *
 * Returns created tkline expire time.
 */
void do_kline(int tkline, char *who, time_t time, char *user, char *host, char *reason, int status)
{
	char buff[BUFSIZE];
	aClient	*acptr;
	aConfItem *aconf;
	int i, count = 0;

	buff[0] = '\0';

	/* Check if such u@h already exists in tkconf. */
	for (aconf = tkline?tkconf:kconf; aconf; aconf = aconf->next)
	{
		if (0==strcasecmp(aconf->host, host) && 
			0==strcasecmp(aconf->name, user))
		{
			aconf->hold = timeofday + time;
			break;
		}
	}
	if (aconf == NULL)
	{
		aconf = make_conf();
		aconf->next = NULL;
		aconf->status = status;
		aconf->hold = timeofday + time;
		aconf->port = 0;
		Class(aconf) = find_class(0);
		DupString(aconf->name, BadTo(user));
		DupString(aconf->host, BadTo(host));
		DupString(aconf->passwd, reason);
		istat.is_confmem += strlen(aconf->name) + 1;
		istat.is_confmem += strlen(aconf->host) + 1;
		istat.is_confmem += strlen(aconf->passwd) + 1;

		/* put on top of t/kconf */
		if (tkline)
		{
			if (tkconf)
			{
				aconf->next = tkconf;
			}
			tkconf = aconf;
			sendto_flag(SCH_TKILL, "TKLINE %s@%s (%u) by %s :%s",
				aconf->name, aconf->host, time, who, reason);
		}
		else
		{
			if (kconf)
			{
				aconf->next = kconf;
			}
			kconf = aconf;
			sendto_flag(SCH_TKILL, "KLINE %s@%s by %s :%s",
				aconf->name, aconf->host, who, reason);
		}
	}

	/* get rid of klined clients */
	for (i = highest_fd; i >= 0; i--)
	{
		if (!(acptr = local[i]) || !IsPerson(acptr)
			|| IsKlineExempt(acptr))
		{
			continue;
		}
		if (!strcmp(acptr->sockhost, acptr->user->sip))
		{
			/* unresolved */
			if (strchr(aconf->host, '/'))
			{
				if (match_ipmask(*aconf->host == '=' ?
					aconf->host + 1 : aconf->host,
					acptr, 1))
				{
					continue;
				}
			}
			else
			{
				if (match(*aconf->host == '=' ?
					aconf->host + 1 : aconf->host,
					acptr->sockhost))
				{
					continue;
				}
			}
		}
		else
		{
			/* resolved */
			if (*aconf->host == '=')
			{
				/* IP only */
				continue;
			}
			if (strchr(aconf->host, '/'))
			{
				if (match_ipmask(aconf->host, acptr, 1))
				{
					continue;
				}
			}
			else
			{
				if (match(aconf->host, acptr->user->sip)
					&& match(aconf->host,
					acptr->user->host))
				{
					continue;
				}
			}
		}
		if (match(aconf->name, aconf->status == CONF_TOTHERKILL ?
			acptr->auth : (IsRestricted(acptr) &&
			acptr->user->username[0] == '+' ?
			acptr->user->username+1 :
			acptr->user->username)) == 0)
		{
			count++;
			sendto_one(acptr, replies[ERR_YOUREBANNEDCREEP],
				ME, acptr->name, aconf->name, aconf->host,
				": ", aconf->passwd);
			sendto_flag(SCH_TKILL,
				"%sKill line active for %s", tkline?"T":"",
				get_client_name(acptr, FALSE));
			if (buff[0] == '\0')
			{
				sprintf(buff, "Kill line active: %.80s",
					aconf->passwd);
			}
			acptr->exitc = tkline ? EXITC_TKLINE : EXITC_KLINE;
			(void) exit_client(acptr, acptr, &me, buff);
		}
	}
	if (count > 4)
	{
		sendto_flag(SCH_TKILL, "%sKline reaped %d souls",
			tkline?"T":"", count);
	}

#ifdef TKLINE
	/* do next tkexpire, but not more often than once a minute */
	if (!nexttkexpire || nexttkexpire > aconf->hold)
	{
		nexttkexpire = MAX(timeofday + 60, aconf->hold);
	}
#endif
	return;
}

int	prep_kline(int tkline, aClient *cptr, aClient *sptr, int parc, char **parv)
{
	int	status = tkline ? CONF_TKILL : CONF_KILL;
	time_t	time;
	char	*user, *host, *reason;
	int	err = 0;

	/* sanity checks */
	if (tkline)
	{
		err = wdhms2sec(parv[1], &time);
#ifdef TKLINE_MAXTIME
		if (time > TKLINE_MAXTIME)
			time = TKLINE_MAXTIME;
		if (time < 0) /* overflown, must have wanted bignum :) */
			time = TKLINE_MAXTIME;
#endif
		user = parv[2];
		reason = parv[3];
	}
	else
	{
		user = parv[1];
		reason = parv[2];
	}
	host = strchr(user, '@');
	
	if (strlen(user) > USERLEN+HOSTLEN+1)
	{
		err = 1;
	}
	if (host)
	{
		*host++ = '\0';
	}
	if (!user || !host || *user == '\0' || *host == '\0' ||
		(!strcmp("*", user) && !strcmp("*", host))) {
		/* disallow all forms of bad u@h format and block *@* too */
		err = 1;
	}
	if (!err && host && strchr(host, '/') && match_ipmask(host, sptr, 0) == -1)
	{
		/* check validity of 1.2.3.0/24 or it will be spewing errors
		** for every connecting client. */
		err = 1;
	}
#ifdef KLINE
badkline:
#endif
	if (err)
	{
		/* error */
		if (!IsPerson(sptr))
		{
			sendto_one(sptr, ":%s NOTICE %s "
				":T/KLINE: Incorrect format",
				ME, parv[0]);
			return exit_client(cptr, cptr, &me,
				"T/KLINE: Incorrect format");
		}
		sendto_one(sptr, ":%s NOTICE %s :%sKLINE: Incorrect format",
			ME, parv[0], tkline?"T":"");
		return 2;
	}

	/* All seems fine. */
	if (*user == '=')
	{
		status = tkline ? CONF_TOTHERKILL : CONF_OTHERKILL;
		user++;
	}
#ifdef INET6
	host = ipv6_convert(host);
#endif
	if (strlen(reason) > TOPICLEN)
	{
		reason[TOPICLEN] = '\0';
	}

#ifdef KLINE
	if (!tkline)
	{
		int	kfd, ksize, kret;
		char	kbuf[2*BUFSIZE];
		char	*utmp, *htmp, *rtmp;

		if (!strcmp(KLINE_PATH, IRCDCONF_PATH))
		{
			sendto_flag(SCH_ERROR,
				"Invalid kline configuration file.");
			return MAXPENALTY;
		}
		utmp = strchr(user, IRCDCONF_DELIMITER);
		htmp = strchr(host, IRCDCONF_DELIMITER);
		rtmp = strchr(reason, IRCDCONF_DELIMITER);
		if (utmp || htmp || rtmp)
		{
			/* Too lazy to copy it here. --B. */
			err = 1;
			goto badkline;
		}

		kfd = open(KLINE_PATH, O_WRONLY|O_APPEND|O_NDELAY);
		if (kfd < 0)
		{
			sendto_flag(SCH_ERROR,
				"Cannot open kline configuration file.");
			return MAXPENALTY;
		}
		ksize = snprintf(kbuf, sizeof(kbuf),
			"%c%c%s%c%s%c%s%c0%c #%s%s%s%s%s#%d\n",
			(status == CONF_OTHERKILL ? 'k' : 'K'),
			IRCDCONF_DELIMITER, host, IRCDCONF_DELIMITER, reason,
			IRCDCONF_DELIMITER, user, IRCDCONF_DELIMITER,
			IRCDCONF_DELIMITER, sptr->name,
			sptr->user ? "!" : "",
			sptr->user ? sptr->user->username : "",
			sptr->user ? "@" : "",
			sptr->user ? sptr->user->host : "", (int)timeofday);
		kret = write(kfd, kbuf, ksize);
		close(kfd);
		if (kret != ksize)
		{
			sendto_flag(SCH_ERROR, "Error writing (%d!=%d) "
				"to kline configuration file.", kret, ksize);
			sendto_one(sptr, ":%s NOTICE %s :KLINE: error writing "
				"(%d!=%d) to kline configuration file",
				ME, parv[0], kret, ksize);
			return MAXPENALTY;
		}
	}
#endif /* KLINE */

	/* All parameters are now sane. Do the stuff. */
	do_kline(tkline, parv[0], time, user, host, reason, status);

	return 1;
}
#endif /* TKLINE || KLINE */

#ifdef KLINE
int	m_kline(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	if (!is_allowed(sptr, ACL_KLINE))
		return m_nopriv(cptr, sptr, parc, parv);
	return prep_kline(0, cptr, sptr, parc, parv);
}
#endif

#ifdef TKLINE
int	m_tkline(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	if (!is_allowed(sptr, ACL_TKLINE))
		return m_nopriv(cptr, sptr, parc, parv);
	return prep_kline(1, cptr, sptr, parc, parv);
}

int	m_untkline(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	aConfItem	*tmp, *prev;
	char	*user, *host;
	int	deleted = 0;
	
	if (!is_allowed(sptr, ACL_UNTKLINE))
		return m_nopriv(cptr, sptr, parc, parv);

	user = parv[1];
	host = strchr(user, '@');
	if (!host)
	{
		/* error */
		if (!IsPerson(sptr))
		{
			sendto_one(sptr, ":%s NOTICE %s "
				":UNTKLINE: Incorrect format",
				ME, parv[0]);
			return exit_client(cptr, cptr, &me,
				"UNTKLINE: Incorrect format");
		}
		sendto_one(sptr, ":%s NOTICE %s :UNTKLINE: Incorrect format",
			ME, parv[0]);
		return 2;
	}
	if (*user == '=')
	{
		user++;
	}
	*host++ = '\0';

	for (prev = tkconf, tmp = tkconf; tmp; tmp = tmp->next)
	{
		if (0==strcasecmp(tmp->host, host) && 
			0==strcasecmp(tmp->name, user))
		{
			if (tmp == tkconf)
				tkconf = tmp->next;
			else
				prev->next = tmp->next;
			free_conf(tmp);
			deleted = 1;
			break;
		}
		prev = tmp;
	}

	if (deleted)
	{
		sendto_flag(SCH_TKILL, "UNTKLINE %s@%s by %s",
			user, host, parv[0]);
	}
	return 1;
}

time_t	tkline_expire(int all)
{
	aConfItem	*tmp = NULL, *tmp2 = tkconf, *prev = tkconf;
	time_t	min = 0;
	
	while ((tmp = tmp2))
	{
		tmp2 = tmp->next;
		if (all || tmp->hold <= timeofday)
		{
			if (tmp == tkconf)
				tkconf = tmp->next;
			else
				prev->next = tmp->next;
			free_conf(tmp);
			continue;
		}
		if (min == 0 || tmp->hold < min)
		{
			min = tmp->hold;
		}
		prev = tmp;
	}
	if (min && min < nexttkexpire + 60)
		min = nexttkexpire + 60;
	return min;
}
#endif /* TKLINE */
