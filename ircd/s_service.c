/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_service.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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
static const volatile char rcsid[] = "@(#)$Id: s_service.c,v 1.69 2010/08/12 01:08:02 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_SERVICE_C
#include "s_externs.h"
#undef S_SERVICE_C

aService	*svctop = NULL;

aService	*make_service(aClient *cptr)
{
	Reg	aService	*svc = cptr->service;

	if (svc)
		return svc;

	cptr->service = svc = (aService *)MyMalloc(sizeof(*svc));
	bzero((char *)svc, sizeof(*svc));
	cptr->name = svc->namebuf;
	svc->bcptr = cptr;
	if (svctop)
		svctop->prevs = svc;
	svc->nexts = svctop;
	svc->prevs = NULL; /* useless */
	svctop = svc;
	return svc;
}


void	free_service(aClient *cptr)
{
	aService	*serv;

	if ((serv = cptr->service))
	{
		if (serv->nexts)
			serv->nexts->prevs = serv->prevs;
		if (serv->prevs)
			serv->prevs->nexts = serv->nexts;
		if (svctop == serv)
			svctop = serv->nexts;
		/* It's just the pointer, not even allocated in m_service.
		 * Why would someone want to destroy that struct here?
		 * So far commenting it out. --B.
		if (serv->servp)
			free_server(serv->servp, cptr);
		 */
		/* this is ok, ->server is a string. */
		if (serv->server)
			MyFree(serv->server);
		MyFree(serv);
		cptr->service = NULL;
	}
}


static	aClient *best_service(char *name, aClient *cptr)
{
	Reg	aClient	*acptr = NULL;
	Reg	aClient	*bcptr;
	Reg	aService *sp;
	int	len = strlen(name);

	if (!index(name, '@') || !(acptr = find_service(name, cptr)))
		for (sp = svctop; sp; sp = sp->nexts)
			if ((bcptr = sp->bcptr) &&
			    !myncmp(name, bcptr->name, len))
			    {
				if (!acptr || bcptr->hopcount < acptr->hopcount)
				{
					acptr = bcptr;
				}
			    }
	return (acptr ? acptr : cptr);
}
 

#ifdef	USE_SERVICES
/*
** check_services_butone
**	check all local services except `cptr', and send `fmt' according to:
**	action	type on notice
**	server	origin
*/
void	check_services_butone(long action, aServer *servp, aClient *cptr,
		char *fmt, ...)
/* shouldn't cptr be named sptr? */
{
	char	nbuf[NICKLEN + USERLEN + HOSTLEN + 3];
	Reg	aService *sp;

	*nbuf = '\0';
	for (sp = svctop; sp; sp = sp->nexts)
	{
		if (!MyConnect(sp->bcptr) ||
		    (cptr && sp->bcptr == cptr->from))
		{
			continue;
		}
		/*
		** found a (local) service, check if action matches what's
		** wanted AND if it comes from a server matching the dist
		*/
		if ((sp->wants & action)
		    && (!servp || !match(sp->dist, servp->bcptr->name)
			|| !match(sp->dist, servp->sid)))
		{
			if ((sp->wants & (SERVICE_WANT_PREFIX|SERVICE_WANT_UID))
			    && cptr && IsRegisteredUser(cptr) &&
			    (action & SERVICE_MASK_PREFIX))
			{
				char	buf[2048];
				va_list	va;
				va_start(va, fmt);
				(void)va_arg(va, char *);
				vsprintf(buf, fmt+3, va);
				va_end(va);
				if ((sp->wants & SERVICE_WANT_UID))
					sendto_one(sp->bcptr, ":%s%s", 
						cptr->user ? cptr->user->uid :
						cptr->name, buf);
				else
					sendto_one(sp->bcptr, ":%s!%s@%s%s",
						cptr->name, cptr->user->username,
						cptr->user->host, buf);
			}
			else
			{
				va_list	va;
				va_start(va, fmt);
				vsendto_one(sp->bcptr, fmt, va);
				va_end(va);
			}
		}
	}
	return;
}

/*
** sendnum_toone
**	send the NICK + USER + UMODE for sptr to cptr according to wants
*/
static	void	sendnum_toone(aClient *cptr, int wants, aClient *sptr,
			char *umode)
{

	if (!*umode)
		umode = "+";

	if ((wants & SERVICE_WANT_UID) && sptr->user)
		sendto_one(cptr, ":%s UNICK %s %s %s %s %s %s :%s",
			sptr->user->servp->sid,
			(wants & SERVICE_WANT_NICK) ? sptr->name : ".",
			sptr->user->uid,
			(wants & SERVICE_WANT_USER) ? sptr->user->username : ".",
			(wants & SERVICE_WANT_USER) ? sptr->user->host : ".",
			(wants & SERVICE_WANT_USER) ? sptr->user->sip : ".",
			(wants & (SERVICE_WANT_UMODE|SERVICE_WANT_OPER)) ? umode : "+",
			(wants & SERVICE_WANT_USER) ? sptr->info : "");
	else
	if (wants & SERVICE_WANT_EXTNICK)
		/* extended NICK syntax */
		sendto_one(cptr, "NICK %s %d %s %s %s %s :%s",
			   (wants & SERVICE_WANT_NICK) ? sptr->name : ".",
			   sptr->hopcount + 1,
			   (wants & SERVICE_WANT_USER) ? sptr->user->username
			   : ".",
			   (wants & SERVICE_WANT_USER) ? sptr->user->host :".",
			   (wants & SERVICE_WANT_USER) ?
			   ((wants & SERVICE_WANT_SID) ?
			    sptr->user->servp->sid : sptr->user->server) : ".",
			   (wants & (SERVICE_WANT_UMODE|SERVICE_WANT_OPER)) ? umode : "+",
			   (wants & SERVICE_WANT_USER) ? sptr->info : "");
	else
		/* old style NICK + USER + UMODE */
	    {
		char    nbuf[NICKLEN + USERLEN + HOSTLEN + 3];
		char    *prefix;

		if (wants & SERVICE_WANT_PREFIX)
		    {
			sprintf(nbuf, "%s!%s@%s", sptr->name,
				sptr->user->username, sptr->user->host);
			prefix = nbuf;
		    }
		else
			prefix = sptr->name;

		if (wants & SERVICE_WANT_NICK)
			sendto_one(cptr, "NICK %s :%d", sptr->name,
				   sptr->hopcount+1);
		if (wants & SERVICE_WANT_USER)
			sendto_one(cptr, ":%s USER %s %s %s :%s", prefix, 
				   sptr->user->username, sptr->user->host,
				   (wants & SERVICE_WANT_SID)?
				   sptr->user->servp->sid : sptr->user->server,
				   sptr->info);
		if (wants & (SERVICE_WANT_UMODE|SERVICE_WANT_OPER))
			sendto_one(cptr, ":%s MODE %s %s", prefix, sptr->name,
				   umode);
	    }
}

/*
** check_services_num
**	check all local services to eventually send NICK + USER + UMODE
**	for new client sptr
*/
void	check_services_num(aClient *sptr, char *umode)
{
	Reg	aService *sp;

	for (sp = svctop; sp; sp = sp->nexts)
	{
		if (!MyConnect(sp->bcptr))
		{
			continue;
		}
		/*
		** found a (local) service, check if action matches what's
		** wanted AND if it comes from a server matching the dist
		*/
		if ((sp->wants & SERVICE_MASK_NUM)
		    && (!match(sp->dist, sptr->user->server)
				|| !match(sp->dist, sptr->user->servp->sid)))
		{
			sendnum_toone(sp->bcptr, sp->wants, sptr,
				      umode);
		}
	}
}


aConfItem	*find_conf_service(aClient *cptr, int type, aConfItem *aconf)
{
	static	char	uhost[HOSTLEN+USERLEN+3];
	Reg	aConfItem *tmp;
	char	*s;
	struct	hostent	*hp;
	int	i;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockethost)
		** matches host field of the configuration, the name field
		** is the same, the type match is correct and nobody else
		** is using this S-line.
		*/
		if (!(tmp->status & CONF_SERVICE))
			continue;
		Debug((DEBUG_INFO,"service: cl=%d host (%s) name (%s) port=%d",
			tmp->clients, tmp->host, tmp->name, tmp->port));
		Debug((DEBUG_INFO,"service: host (%s) name (%s) type=%d",
			cptr->sockhost, cptr->name, type));
		if (tmp->clients || (type && tmp->port != type) ||
		    mycmp(tmp->name, cptr->name))
			continue;
	 	if ((hp = cptr->hostp))
			for (s = hp->h_name, i = 0; s; s = hp->h_aliases[i++])
			    {
				sprintf(uhost, "%s@%s", cptr->username, s);
				if (match(tmp->host, uhost) == 0)
					return tmp;
			    }
		sprintf(uhost, "%s@%s", cptr->username, cptr->sockhost);
		if (match(tmp->host, uhost) == 0)
			return tmp;
	    }
	return aconf;
}
#endif


/*
** m_service
**
**  <= 2.10 protocol:
**	parv[0] = sender prefix
**	parv[1] = service name
**	parv[2] = server token (unused on pure 2.11 network)
**	parv[3] = distribution code
**	parv[4] = service type
**	parv[5] = hopcount
**	parv[6] = info
**
**  2.11 protocol
**	parv[0] = sender prefix
**	parv[1] = service name
**	parv[2] = distribution mask
**	parv[3] = service type
**	parv[4]	= info
**
*/
int	m_service(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient	*acptr = NULL, *bcptr = NULL;
	aService *svc;
#ifdef  USE_SERVICES
	aConfItem *aconf;
#endif
	aServer	*sp = NULL;
	char	*dist, *server = NULL, *info;
	int	type, i;

	if (sptr->user)
	    {
		sendto_one(sptr, replies[ERR_ALREADYREGISTRED], ME, BadTo(parv[0]));
		return 1;
	    }

	if (parc < 5)
	{
		sendto_one(cptr, replies[ERR_NEEDMOREPARAMS], ME,
			   BadTo(parv[0]), "SERVICE");
		return 1;
	}

	/* Copy parameters into better documenting variables */
	dist = parv[2];
	type = strtol(parv[3], NULL, 0);
	info = parv[4];

	/*
	 * Change the sender's origin.
	 */
	if (IsServer(cptr))
	    {
		acptr = make_client(cptr);
		svc = make_service(acptr);
		add_client_to_list(acptr);
		strncpyzt(acptr->service->namebuf, parv[1],
			sizeof(acptr->service->namebuf));
		
		/* 2.11 protocol - :SID SERVICE ..
		 * - we know that the sptr contains the correct server */
		acptr->hopcount = sptr->hopcount;
		sp = sptr->serv;
		
		if (sp == NULL)
		{
			sendto_flag(SCH_ERROR,
                       	    "ERROR: SERVICE:%s without SERVER:%s from %s",
				    acptr->name, server,
				    get_client_name(cptr, FALSE));
			return exit_client(NULL, acptr, &me, "No Such Server");
		}
		if (match(dist, ME) && match(dist, me.serv->sid))
		{
			sendto_flag(SCH_ERROR,
                       	    "ERROR: SERVICE:%s DIST:%s from %s", acptr->name,
				    dist, get_client_name(cptr, FALSE));
			return exit_client(NULL, acptr, &me,
					   "Distribution code mismatch");
		}
	    }
#ifndef	USE_SERVICES
	else
	    {
		sendto_one(cptr, "ERROR :Server doesn't support services");
		return 1;
	    }
#endif


#ifdef	USE_SERVICES
	if (!IsServer(cptr))
	    {
		char **isup = isupport;

		svc = make_service(sptr);
		sptr->hopcount = 0;
		server = ME;
		sp = me.serv;
		if (!do_nick_name(parv[1], 0))
		    {
			sendto_one(sptr, replies[ERR_ERRONEOUSNICKNAME],
				   ME, BadTo(parv[0]), parv[1]);
			return 1;
		    }
		if (strlen(parv[1]) + strlen(server) + 2 >= (size_t) HOSTLEN)
		    {
			sendto_one(acptr, "ERROR :Servicename is too long.");
			sendto_flag(SCH_ERROR,
				    "Access for service %d (%s) denied (%s)",
				    type, parv[1], "servicename too long");
			return exit_client(cptr, sptr, &me, "Name too long");
		    }

		strncpyzt(sptr->service->namebuf, parv[1],
			sizeof(sptr->service->namebuf));
		if (!(aconf = find_conf_service(sptr, type, NULL)))
		    {
			sendto_one(sptr,
				   "ERROR :Access denied (service %d) %s",
				   type, get_client_name(sptr, TRUE));
			sendto_flag(SCH_ERROR,
				    "Access denied (service %d) %s", type,
				    get_client_name(sptr, TRUE));
			return exit_client(cptr, sptr, &me, "Not enabled");
		    }

		if (!BadPtr(aconf->passwd) &&
		    !StrEq(aconf->passwd, sptr->passwd))
		    {
			sendto_flag(SCH_ERROR,
				    "Access denied: (passwd mismatch) %s",
				    get_client_name(sptr, TRUE));
			return exit_client(cptr, sptr, &me, "Bad Password");
		    }

		(void)strcat(sptr->name, "@"), strcat(sptr->name, server);
		if (find_service(sptr->name, NULL))
		    {
			sendto_flag(SCH_ERROR, "Service %s already exists",
				    get_client_name(sptr, TRUE));
			return exit_client(cptr, sptr, &me, "Service Exists");
		    }
		attach_conf(sptr, aconf);
		sendto_one(sptr, replies[RPL_YOURESERVICE], ME, BadTo(sptr->name),
			   sptr->name);
		sendto_one(sptr, replies[RPL_YOURHOST], ME, BadTo(sptr->name),
                           get_client_name(&me, FALSE), version);
		while (*isup)
		{
			sendto_one(sptr,replies[RPL_ISUPPORT], ME,
			BadTo(sptr->name), *isup);
			isup++;
		}
		sendto_one(sptr, replies[RPL_MYINFO], ME, BadTo(sptr->name), ME, version);
		sendto_flag(SCH_NOTICE, "Service %s connected",
			    get_client_name(sptr, TRUE));
		istat.is_unknown--;
		istat.is_myservice++;
		if (istat.is_myservice > istat.is_m_myservice)
			istat.is_m_myservice = istat.is_myservice;
		
		/* local service, assign to acptr so we can use it later*/
		acptr = sptr;
	    }
#endif

	istat.is_service++;
	if (istat.is_service > istat.is_m_service)
		istat.is_m_service = istat.is_service;
	SetService(acptr);
	svc->servp = sp;
	sp->refcnt++;
	svc->server = mystrdup(sp->bcptr->name);
	strncpyzt(svc->dist, dist, HOSTLEN);
	if (acptr->info != DefInfo)
		MyFree(acptr->info);
	if (strlen(info) > REALLEN) info[REALLEN] = '\0';
	acptr->info = mystrdup(info);
	svc->wants = 0;
	svc->type = type;
	reorder_client_in_list(acptr);
	(void)add_to_client_hash_table(acptr->name, acptr);

#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_SERVICE, NULL, acptr,
			      ":%s SERVICE %s %s %d :%s",
			      sp->sid, acptr->name, dist, type, info);
#endif
	sendto_flag(SCH_SERVICE,
			"Received SERVICE %s from %s via %s (%s %d %s)",
			acptr->name, sptr->name, get_client_name(cptr, TRUE),
			dist, acptr->hopcount, info);

	for (i = fdas.highest; i >= 0; i--)
	{
		if (!(bcptr = local[fdas.fd[i]]) || !IsServer(bcptr) ||
		    bcptr == cptr)
			continue;
		if (match(dist, bcptr->name) && match(dist, bcptr->serv->sid))
			continue;

		sendto_one(bcptr, ":%s SERVICE %s %s %d :%s",
				sp->sid, acptr->name, dist, type, info);
	}
		
	return 0;
}


/*
** Returns list of all matching services.
** parv[1] - string to match names against
** parv[2] - type of service
*/
int	m_servlist(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg	aService *sp;
	Reg	aClient *acptr;
	char	*mask = BadPtr(parv[1]) ? "*" : parv[1];
	int	type = 0;

	if (parc > 2)
		type = BadPtr(parv[2]) ? 0 : strtol(parv[2], NULL, 0);
	for (sp = svctop; sp; sp = sp->nexts)
		if  ((acptr = sp->bcptr) && (!type || type == sp->type) &&
		     (match(mask, acptr->name) == 0))
			sendto_one(sptr, replies[RPL_SERVLIST], ME, BadTo(parv[0]),
				   acptr->name, sp->server, sp->dist,
				   sp->type, acptr->hopcount, acptr->info);
	sendto_one(sptr, replies[RPL_SERVLISTEND], ME, BadTo(parv[0]), mask, type);
	return 2;
}


#ifdef	USE_SERVICES
/*
** m_servset
**
**      parv[0] = sender prefix
**      parv[1] = data requested
**      parv[2] = burst requested (optional)
*/
int	m_servset(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	int burst = 0;

	if (!MyConnect(sptr))
	    {
		sendto_flag(SCH_ERROR, "%s issued a SERVSET (from %s)",
			    sptr->name, get_client_name(cptr, TRUE));
		return 1;
	    }
	if (!IsService(sptr) || (IsService(sptr) && sptr->service->wants))
	    {
		sendto_one(sptr, replies[ERR_NOPRIVILEGES], ME, BadTo(parv[0]));
		return 1;
	    }
	if (sptr->service->wants)
		return 1;

	/* check against configuration */
	sptr->service->wants = strtol(parv[1], NULL, 0) & sptr->service->type;
	/* check that service is global for some requests */
	if (strcmp(sptr->service->dist, "*"))
		sptr->service->wants &= ~SERVICE_MASK_GLOBAL;
	/* allow options */
	sptr->service->wants |= (strtol(parv[1], NULL, 0) & ~SERVICE_MASK_ALL);
	/* send accepted SERVSET */
	sendto_one(sptr, ":%s SERVSET %s :%d", sptr->name, sptr->name,
		   sptr->service->wants);

	if (parc < 3 ||
	    ((burst = sptr->service->wants & strtol(parv[2], NULL, 0)) == 0))
		return 0;

	/*
	** services can request a connect burst.
	** it is optional, because most services should not need it,
	** so let's save some bandwidth.
	**
	** tokens are NOT used. (2.8.x like burst)
	** distribution code is respected.
	** service type also respected.
	*/
	cptr->flags |= FLAGS_CBURST;
	if (burst & SERVICE_WANT_SERVER)
	    {
		int	split;
		
		for (acptr = &me; acptr; acptr = acptr->prev)
		    {
			if (!IsServer(acptr) && !IsMe(acptr))
				continue;
			if (match(sptr->service->dist, acptr->name) &&
					match(sptr->service->dist, acptr->serv->sid))
				continue;
			split = (MyConnect(acptr) &&
				 mycmp(acptr->name, acptr->sockhost));
			sendto_one(sptr, ":%s SERVER %s %d %s :%s",
				acptr->serv->up->name, acptr->name,
				acptr->hopcount+1,
				acptr->serv->sid,
				acptr->info);
		    }
	    }

	if (burst & (SERVICE_WANT_NICK|SERVICE_WANT_USER|SERVICE_WANT_SERVICE))
	    {
		char	buf[BUFSIZE] = "+";
		
		for (acptr = &me; acptr; acptr = acptr->prev)
		    {
			/* acptr->from == acptr for acptr == cptr */
			if (acptr->from == cptr)
				continue;
			if (IsPerson(acptr))
			    {
				if (match(sptr->service->dist,
					  acptr->user->server) &&
					match(sptr->service->dist,
					acptr->user->servp->sid))
					continue;
				if (burst & SERVICE_WANT_UMODE)
					send_umode(NULL, acptr, 0, SEND_UMODES,
						   buf);
				else if (burst & SERVICE_WANT_OPER)
					send_umode(NULL, acptr, 0, FLAGS_OPER,
						   buf);
				sendnum_toone(sptr, burst, acptr, buf);
			    }
			else if (IsService(acptr))
			    {
				if (!(burst & SERVICE_WANT_SERVICE))
					continue;
				if (match(sptr->service->dist,
					  acptr->service->server) &&
					match(sptr->service->dist,
					acptr->service->servp->sid))
					continue;
				sendto_one(sptr, "SERVICE %s %s %s %d %d :%s",
					   acptr->name, acptr->service->server,
					   acptr->service->dist,
					   acptr->service->type,
					   acptr->hopcount + 1, acptr->info);
			    }
		    }
	    }
	
	if (burst & (SERVICE_WANT_CHANNEL|SERVICE_WANT_VCHANNEL|SERVICE_WANT_MODE|SERVICE_WANT_TOPIC))
	    {
		char    modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
		aChannel	*chptr;
		
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (chptr->users == 0)
				continue;
			if (burst&(SERVICE_WANT_CHANNEL|SERVICE_WANT_VCHANNEL))
				sendto_one(sptr, "CHANNEL %s %d",
					   chptr->chname, chptr->users);
			if (burst & SERVICE_WANT_MODE)
			    {
				*modebuf = *parabuf = '\0';
				modebuf[1] = '\0';
				channel_modes(&me, modebuf, parabuf, chptr);
				sendto_one(sptr, "MODE %s %s", chptr->chname,
					   modebuf);
			    }
			if ((burst & SERVICE_WANT_TOPIC) && *chptr->topic)
				sendto_one(sptr, "TOPIC %s :%s",
					   chptr->chname, chptr->topic);
		    }
	    }
	sendto_one(sptr, "EOB");
	cptr->flags ^= FLAGS_CBURST;
	return 0;
}
#endif


/*
** Send query to service.
** parv[1] - string to match name against
** parv[2] - string to send to service
*/
int	m_squery(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (parc <= 2)
	    {
		if (parc == 1)
			sendto_one(sptr, replies[ERR_NORECIPIENT], ME, BadTo(parv[0]),
				   "SQUERY");
		else if (parc == 2 || BadPtr(parv[1]))
			sendto_one(sptr, replies[ERR_NOTEXTTOSEND], ME, BadTo(parv[0]));
		return 1;
	    }

	if ((acptr = best_service(parv[1], NULL)))
		if (MyConnect(acptr) &&
		    (acptr->service->wants & SERVICE_WANT_PREFIX))
			sendto_one(acptr, ":%s!%s@%s SQUERY %s :%s", parv[0],
				   sptr->user->username, sptr->user->host,
				   acptr->name, parv[2]);
		else if (MyConnect(acptr) && 
			(acptr->service->wants & SERVICE_WANT_UID))
			sendto_one(acptr, ":%s SQUERY %s :%s", sptr->user->uid,
				   acptr->name, parv[2]);
		else
			sendto_one(acptr, ":%s SQUERY %s :%s",
				   parv[0], acptr->name, parv[2]);
	else
		sendto_one(sptr, replies[ERR_NOSUCHSERVICE], ME, BadTo(parv[0]), parv[1]);
	return 2;
}

