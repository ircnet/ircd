/*
 *   IRC - Internet Relay Chat, ircd/class.c
 *   Copyright (C) 1990 Darren Reed
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
static const volatile char rcsid[] = "@(#)$Id: class.c,v 1.28 2008/06/22 16:09:07 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define CLASS_C
#include "s_externs.h"
#undef CLASS_C
#ifdef ENABLE_CIDR_LIMITS
#include "patricia_ext.h"
#endif

#define BAD_CONF_CLASS		-1
#define BAD_PING		-2
#define BAD_CLIENT_CLASS	-3

aClass	*classes;

int	get_conf_class(aConfItem *aconf)
{
	if ((aconf) && Class(aconf))
		return (ConfClass(aconf));

	Debug((DEBUG_DEBUG,"No Class For %s",
	      (aconf) ? aconf->name : "*No Conf*"));

	return (BAD_CONF_CLASS);

}

static	int	get_conf_ping(aConfItem *aconf)
{
	if ((aconf) && Class(aconf))
		return (ConfPingFreq(aconf));

	Debug((DEBUG_DEBUG,"No Ping For %s",
	      (aconf) ? aconf->name : "*No Conf*"));

	return (BAD_PING);
}



int	get_client_class(aClient *acptr)
{
	Reg	Link	*tmp;
	Reg	aClass	*cl;
	int	retc = BAD_CLIENT_CLASS;

	if (acptr && !IsMe(acptr)  && (acptr->confs))
		for (tmp = acptr->confs; tmp; tmp = tmp->next)
		    {
			if (!tmp->value.aconf ||
			    !(cl = tmp->value.aconf->class))
				continue;
			retc = Class(cl);
			break;
		    }

	Debug((DEBUG_DEBUG,"Returning Class %d For %s",retc,acptr->name));

	return (retc);
}

int	get_client_ping(aClient *acptr)
{
	int	ping = 0, ping2;
	aConfItem	*aconf;
	Link	*link;

	link = acptr->confs;

	if (link)
		while (link)
		    {
			aconf = link->value.aconf;
			if (aconf->status & (CONF_CLIENT|CONF_CONNECT_SERVER|
					     CONF_NOCONNECT_SERVER|
					     CONF_ZCONNECT_SERVER))
			    {
				ping2 = get_conf_ping(aconf);
				if ((ping2 != BAD_PING) && ((ping > ping2) ||
				    !ping))
					ping = ping2;
			     }
			link = link->next;
		    }
	else
	    {
		ping = PINGFREQUENCY;
		Debug((DEBUG_DEBUG,"No Attached Confs"));
	    }
	if (ping <= 0)
		ping = PINGFREQUENCY;
	Debug((DEBUG_DEBUG,"Client %s Ping %d", acptr->name, ping));
	return (ping);
}

int	get_con_freq(aClass *clptr)
{
	if (clptr)
		return (MAX(60, ConFreq(clptr)));
	else
		return (CONNECTFREQUENCY);
}

/*
 * When adding a class, check to see if it is already present first.
 * if so, then update the information for that class, rather than create
 * a new entry for it and later delete the old entry.
 * if no present entry is found, then create a new one and add it in
 * immeadiately after the first one (class 0).
 */
void	add_class(int class, int ping, int confreq, int maxli, int sendq,
		int bsendq, int hlocal, int uhlocal, int hglobal, int uhglobal
#ifdef ENABLE_CIDR_LIMITS
		, char *cidrlen_s
#endif
      )
{
	aClass *t, *p;
#ifdef ENABLE_CIDR_LIMITS
	char *tmp;
	int cidrlen = 0, cidramount = 0;

	if(cidrlen_s)
	{
		if((tmp = index(cidrlen_s, '/')))
		{
			*tmp++ = '\0';

			cidramount = atoi(cidrlen_s);
			cidrlen = atoi(tmp);
		}
	}
#endif

	t = find_class(class);
	if ((t == classes) && (class != 0))
	    {
		p = (aClass *)make_class();
		NextClass(p) = NextClass(t);
		NextClass(t) = p;
		MaxSendq(p) = QUEUELEN;
#ifdef ENABLE_CIDR_LIMITS
		CidrLen(p) = 0;
		p->ip_limits = NULL;
#endif
		istat.is_class++;
	    }
	else
		p = t;
	Debug((DEBUG_DEBUG,
"Add Class %d: p %x t %x - cf: %d pf: %d ml: %d sq: %d.%d ml: %d.%d mg: %d.%d",
		class, p, t, confreq, ping, maxli, sendq, bsendq, hlocal, uhlocal,
	       hglobal, uhglobal));
	Class(p) = class;
	ConFreq(p) = confreq;
	PingFreq(p) = ping;
	MaxLinks(p) = maxli;
	if (sendq)
		MaxSendq(p) = sendq;
	MaxBSendq(p) = bsendq ? bsendq : 0;
	MaxHLocal(p) = hlocal;
	MaxUHLocal(p) = uhlocal;
	MaxHGlobal(p) = hglobal;
	MaxUHGlobal(p) = uhglobal;

#ifdef ENABLE_CIDR_LIMITS
	if (cidrlen > 0 && CidrLen(p) == 0 && p->ip_limits == NULL)
	{
		CidrLen(p) = cidrlen;
#  ifdef INET6
		p->ip_limits = (struct _patricia_tree_t *) patricia_new(128);
#  else
		p->ip_limits = (struct _patricia_tree_t *) patricia_new(32);
#  endif
	}
	if (CidrLen(p) != cidrlen)
	{
		/* Hmpf, sendto_somewhere maybe to warn? --B. */
		Debug((DEBUG_NOTICE, 
			"Cannot change cidrlen on the fly (class %d)",
			Class(p)));
	}
	if (CidrLen(p) > 0)
		MaxCidrAmount(p) = cidramount;
#endif
	if (p != t)
		Links(p) = 0;
}

aClass	*find_class(int cclass)
{
	aClass *cltmp;

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
		if (Class(cltmp) == cclass)
			return cltmp;
	return classes;
}

void	check_class(void)
{
	Reg	aClass	*cltmp, *cltmp2;

	Debug((DEBUG_DEBUG, "Class check:"));

	for (cltmp2 = cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp2))
	    {
		Debug((DEBUG_DEBUG,
			"Class %d : CF: %d PF: %d ML: %d LI: %d SQ: %ld",
			Class(cltmp), ConFreq(cltmp), PingFreq(cltmp),
			MaxLinks(cltmp), Links(cltmp), MaxSendq(cltmp)));
		if (MaxLinks(cltmp) < 0)
		    {
			NextClass(cltmp2) = NextClass(cltmp);
			if (Links(cltmp) <= 0)
			{
				free_class(cltmp);
				istat.is_class--;
			}
		    }
		else
			cltmp2 = cltmp;
	    }
}

void	initclass(void)
{
	classes = (aClass *)make_class();
	istat.is_class++;

	Class(FirstClass()) = 0;
	ConFreq(FirstClass()) = CONNECTFREQUENCY;
	PingFreq(FirstClass()) = PINGFREQUENCY;
	MaxLinks(FirstClass()) = MAXIMUM_LINKS;
	MaxSendq(FirstClass()) = QUEUELEN;
	MaxBSendq(FirstClass()) = 0;
	Links(FirstClass()) = 0;
	NextClass(FirstClass()) = NULL;
	MaxHLocal(FirstClass()) = 1;
	MaxUHLocal(FirstClass()) = 1;
	MaxHGlobal(FirstClass()) = 1;
	MaxUHGlobal(FirstClass()) = 1;
#ifdef ENABLE_CIDR_LIMITS
	CidrLen(FirstClass()) = 0;
	FirstClass()->ip_limits = NULL;
#endif
}

void	report_classes(aClient *sptr, char *to)
{
	Reg	aClass	*cltmp;
	char	tmp[64] = "";

	for (cltmp = FirstClass(); cltmp; cltmp = NextClass(cltmp))
	{
#ifdef ENABLE_CIDR_LIMITS
		if (MaxCidrAmount(cltmp) > 0 && CidrLen(cltmp) > 0)
			/* leading space is important */
			snprintf(tmp, sizeof(tmp), " %d/%d",
				MaxCidrAmount(cltmp), CidrLen(cltmp));
		else
			tmp[0] = '\0';
#endif
		sendto_one(sptr, replies[RPL_STATSYLINE], ME, BadTo(to), 'Y',
			Class(cltmp), PingFreq(cltmp), ConFreq(cltmp),
			MaxLinks(cltmp), MaxSendq(cltmp), MaxBSendq(cltmp),
			MaxHLocal(cltmp), MaxUHLocal(cltmp),
			MaxHGlobal(cltmp), MaxUHGlobal(cltmp), Links(cltmp), tmp);
	}
}

int	get_sendq(aClient *cptr, int bursting)
{
	Reg	int	sendq = QUEUELEN;
	Reg	Link	*tmp;
	Reg	aClass	*cl;

	if (cptr->serv && cptr->serv->nline)
		sendq = bursting && MaxBSendq(cptr->serv->nline->class) ?
			MaxBSendq(cptr->serv->nline->class) :
			MaxSendq(cptr->serv->nline->class);
	else if (cptr && !IsMe(cptr)  && (cptr->confs))
		for (tmp = cptr->confs; tmp; tmp = tmp->next)
		    {
			if (!tmp->value.aconf ||
			    !(cl = tmp->value.aconf->class))
				continue;
			sendq = bursting && MaxBSendq(cl) ?
				MaxBSendq(cl) : MaxSendq(cl);
			break;
		    }
	return sendq;
}

