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
static  char rcsid[] = "@(#)$Id: s_service.c,v 1.17 1997/10/08 20:20:03 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_SERVICE_C
#include "s_externs.h"
#undef S_SERVICE_C

aService	*svctop = NULL;

aService	*make_service(cptr)
aClient	*cptr;
{
	Reg	aService	*svc = cptr->service;

	if (svc)
		return svc;

	cptr->service = svc = (aService *)MyMalloc(sizeof(*svc));
	bzero((char *)svc, sizeof(*svc));
	svc->bcptr = cptr;
	if (svctop)
		svctop->prevs = svc;
	svc->nexts = svctop;
	svc->prevs = NULL; /* useless */
	svctop = svc;
	return svc;
}


void	free_service(cptr)
aClient	*cptr;
{
	Reg	aService	*serv;

	if ((serv = cptr->service))
	    {
		if (serv->nexts)
			serv->nexts->prevs = serv->prevs;
		if (serv->prevs)
			serv->prevs->nexts = serv->nexts;
		if (svctop == serv)
			svctop = serv->nexts;
		if (serv->servp)
			free_server(serv->servp, cptr);
		if (serv->server)
			MyFree(serv->server);
		MyFree((char *)serv);
		cptr->service = NULL;
	    }
}


static	aClient *best_service(name, cptr)
char	*name;
aClient *cptr;
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
				acptr = bcptr;
				break;
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
#if ! USE_STDARG
void	check_services_butone(action, server, cptr, fmt, p1, p2, p3, p4,
			      p5, p6, p7, p8)
long	action;
aClient	*cptr; /* shouldn't this be named sptr? */
char	*fmt, *server;
void	*p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
#else
void	check_services_butone(long action, char *server, aClient *cptr, char *fmt, ...)
#endif
{
	char nbuf[NICKLEN + USERLEN + HOSTLEN + 3] = "";
	Reg	aClient	*acptr;
	Reg	int	i;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]) || !IsService(acptr) ||
		    (cptr && acptr == cptr->from))
			continue;
		/*
		** found a (local) service, check if action matches what's
		** wanted AND if it comes from a server matching the dist
		*/
		if ((acptr->service->wants & action)
		    && (!server || !match(acptr->service->dist, server)))
			if ((acptr->service->wants & SERVICE_WANT_PREFIX) && 
			    cptr && IsRegisteredUser(cptr) &&
			    (action & SERVICE_MASK_PREFIX))
			    {
#if USE_STDARG
				char	buf[2048];
				va_list	va;
				va_start(va, fmt);
				vsprintf(buf, fmt+3, va);
				va_end(va);
#endif
				sprintf(nbuf, "%s!%s@%s", cptr->name,
					cptr->user->username,cptr->user->host);

#if ! USE_STDARG
				sendto_one(acptr, fmt, nbuf, p2, p3, p4, p5,
					   p6, p7, p8);
#else
				sendto_one(acptr, ":%s%s", nbuf, buf);
#endif
			    }
			else
			    {
#if ! USE_STDARG
				sendto_one(acptr, fmt, p1, p2, p3, p4, p5,
					   p6, p7, p8);
#else
				va_list	va;
				va_start(va, fmt);
				vsendto_one(acptr, fmt, va);
				va_end(va);
#endif
			    }
	    }
	return;
}

/*
** sendnum_toone
**	send the NICK + USER + UMODE for sptr to cptr according to wants
*/
static void	sendnum_toone (cptr, wants, sptr, umode)
aClient *cptr, *sptr;
char   *umode;
int	wants;
{

	if (!*umode)
		umode = "+";

	if (wants & SERVICE_WANT_EXTNICK)
		/* extended NICK syntax */
		sendto_one(cptr, "NICK %s %d %s %s %s %s :%s",
			   (wants & SERVICE_WANT_NICK) ? sptr->name : ".",
			   sptr->hopcount + 1,
			   (wants & SERVICE_WANT_USER) ? sptr->user->username
			   : ".",
			   (wants & SERVICE_WANT_USER) ? sptr->user->host :".",
			   (wants & SERVICE_WANT_USER) ?
			   ((wants & SERVICE_WANT_TOKEN) ?
			    sptr->user->servp->tok : sptr->user->server) : ".",
			   (wants & SERVICE_WANT_UMODE) ? umode : "+",
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
				   (wants & SERVICE_WANT_TOKEN)?
				   sptr->user->servp->tok : sptr->user->server,
				   sptr->info);
		if (wants & SERVICE_WANT_UMODE)
			sendto_one(cptr, ":%s MODE %s %s", prefix, sptr->name,
				   umode);
	    }
}

/*
** check_services_num
**	check all local services to eventually send NICK + USER + UMODE
**	for new client sptr
*/
void	check_services_num(sptr, umode)
aClient *sptr;
char   *umode;
{
	Reg	aClient	*acptr;
	Reg	int	i;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]) || !IsService(acptr))
			continue;
		/*
		** found a (local) service, check if action matches what's
		** wanted AND if it comes from a server matching the dist
		*/
		if ((acptr->service->wants & SERVICE_MASK_NUM)
		    && !match(acptr->service->dist, sptr->user->server))
			sendnum_toone(acptr, acptr->service->wants, sptr,
				      umode);
	    }
}


aConfItem *find_conf_service(cptr, type, aconf)
aClient	*cptr;
aConfItem *aconf;
int	type;
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
				SPRINTF(uhost, "%s@%s", cptr->username, s);
				if (match(tmp->host, uhost) == 0)
					return tmp;
			    }
		SPRINTF(uhost, "%s@%s", cptr->username, cptr->sockhost);
		if (match(tmp->host, uhost) == 0)
			return tmp;
	    }
	return aconf;
}
#endif


/*
** m_service
**
**	parv[0] = sender prefix
**	parv[1] = service name
**	parv[2] = server token
**	parv[3] = distribution code
**	parv[4] = service type
**	parv[5] = hopcount
**	parv[6] = info
*/
int	m_service(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aClient	*acptr = NULL;
	Reg	aService *svc;
#ifdef  USE_SERVICES
	Reg	aConfItem *aconf;
#endif
	aServer	*sp = NULL;
	char	*dist, *server = NULL, *info, *stok;
	int	type, metric = 0, i;
	char	*mlname;

	if (sptr->user)
	    {
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED, parv[0]));
		return 1;
	    }

	if (parc < 7 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[6] == '\0')
	    {
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS,
			   BadPtr(parv[0]) ? "*" : parv[0]), "SERVICE");
		return 1;
	    }

	/* Copy parameters into better documenting variables */

	/*
	 * Change the sender's origin.
	 */
	if (IsServer(cptr))
	    {
		sptr = make_client(cptr);
		add_client_to_list(sptr);
		strncpyzt(sptr->name, parv[1], sizeof(sptr->name));
		server = parv[2];
		metric = atoi(parv[5]);
		sp = find_tokserver(atoi(server), cptr, NULL);
		if (!sp)
		    {
			sendto_flag(SCH_ERROR,
                       	    "ERROR: SERVICE:%s without SERVER:%s from %s",
				    sptr->name, server,
				    get_client_name(cptr, FALSE));
			return exit_client(NULL, sptr, &me, "No Such Server");
		    }
		if (match(parv[3], ME))
		    {
			sendto_flag(SCH_ERROR,
                       	    "ERROR: SERVICE:%s DIST:%s from %s", sptr->name,
				    parv[3], get_client_name(cptr, FALSE));
			return exit_client(NULL, sptr, &me,
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

	dist = parv[3];
	type = atoi(parv[4]);
	info = parv[6];

#ifdef	USE_SERVICES
	if (!IsServer(cptr))
	    {
		metric = 0;
		server = ME;
		sp = me.serv;
		if (!do_nick_name(parv[1]))
		    {
			sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME,
				   parv[0]));
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

		strncpyzt(sptr->name, parv[1], sizeof(sptr->name));
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
		sendto_one(sptr, rpl_str(RPL_YOURESERVICE, sptr->name),
			   sptr->name);
		sendto_one(sptr, rpl_str(RPL_YOURHOST, sptr->name),
                           get_client_name(&me, FALSE), version);
		sendto_one(sptr, rpl_str(RPL_MYINFO, sptr->name), ME, version);
		sendto_flag(SCH_NOTICE, "Service %s connected",
			    get_client_name(sptr, TRUE));
		istat.is_unknown--;
		istat.is_myservice++;
	    }
#endif

	istat.is_service++;
	svc = make_service(sptr);
	SetService(sptr);
	svc->servp = sp;
	sp->refcnt++;
	svc->server = mystrdup(sp->bcptr->name);
	strncpyzt(svc->dist, dist, HOSTLEN);
	strncpyzt(sptr->info, info, REALLEN);
	svc->wants = 0;
	svc->type = type;
	sptr->hopcount = metric;
	(void)add_to_client_hash_table(sptr->name, sptr);

#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_SERVICE, NULL, sptr,
			      "SERVICE %s %s %s %d %d :%s", sptr->name,
			      server, dist, type, metric, info);
#endif
	sendto_flag(SCH_SERVICE, "Received SERVICE %s from %s (%s %d %s)",
		    sptr->name, get_client_name(cptr, TRUE), dist, metric,
		    info);

	for (i = fdas.highest; i >= 0; i--)
	    {
		if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr) ||
		    acptr == cptr)
			continue;
		if (match(dist, acptr->name))
			continue;
		mlname = my_name_for_link(ME, acptr->serv->nline->port);
		if (*mlname == '*' && match(mlname, sptr->service->server)== 0)
			stok = me.serv->tok;
		else
			stok = sp->tok;
		sendto_one(acptr, "SERVICE %s %s %s %d %d :%s", sptr->name,
			   stok, dist, type, metric+1, info);
	    }
	return 0;
}


/*
** Returns list of all matching services.
** parv[1] - string to match names against
** parv[2] - type of service
*/
int	m_servlist(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aService *sp;
	Reg	aClient *acptr;
	char	*mask = BadPtr(parv[1]) ? "*" : parv[1];
	int	type = 0;

	if (parc > 2)
		type = BadPtr(parv[2]) ? 0 : atoi(parv[2]);
	for (sp = svctop; sp; sp = sp->nexts)
		if  ((acptr = sp->bcptr) && (!type || type == sp->type) &&
		     (match(mask, acptr->name) == 0))
			sendto_one(sptr, rpl_str(RPL_SERVLIST, parv[0]),
				   acptr->name, sp->server, sp->dist,
				   sp->type, acptr->hopcount, acptr->info);
	sendto_one(sptr, rpl_str(RPL_SERVLISTEND, parv[0]), mask, type);
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
int	m_servset(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
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
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES, parv[0]));
		return 1;
	    }
	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
			   "SERVSET");
		return 1;
	    }
	if (sptr->service->wants)
		return 1;

	/* check against configuration */
	sptr->service->wants = atoi(parv[1]) & sptr->service->type;
	/* check that service is global for some requests */
	if (strcmp(sptr->service->dist, "*"))
		sptr->service->wants &= ~SERVICE_MASK_GLOBAL;
	/* allow options */
	sptr->service->wants |= (atoi(parv[1]) & ~SERVICE_MASK_ALL);
	/* send accepted SERVSET */
	sendto_one(sptr, ":%s SERVSET %s :%d", sptr->name, sptr->name,
		   sptr->service->wants);

	if (parc < 3 ||
	    ((burst = sptr->service->wants & atoi(parv[2])) == 0))
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
	if (burst & SERVICE_WANT_SERVER)
	    {
		int	split;
		
		for (acptr = &me; acptr; acptr = acptr->prev)
		    {
			if (!IsServer(acptr) && !IsMe(acptr))
				continue;
			if (match(sptr->service->dist, acptr->name))
				continue;
			split = (MyConnect(acptr) &&
				 mycmp(acptr->name, acptr->sockhost));
			if (split)
				sendto_one(sptr,":%s SERVER %s %d %s :[%s] %s",
					   acptr->serv->up, acptr->name,
					   acptr->hopcount+1,
					   acptr->serv->tok,
					   acptr->sockhost, acptr->info);
			else
				sendto_one(sptr, ":%s SERVER %s %d %s :%s",
					   acptr->serv->up, acptr->name,
					   acptr->hopcount+1,
					   acptr->serv->tok,
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
					  acptr->user->server))
					continue;
				if (burst & SERVICE_WANT_UMODE)
					send_umode(NULL, acptr, 0, SEND_UMODES,
						   buf);
				sendnum_toone(sptr, burst, acptr, buf);
			    }
			else if (IsService(acptr))
			    {
				if (!(burst & SERVICE_WANT_SERVICE))
					continue;
				if (match(sptr->service->dist,
					  acptr->service->server))
					continue;
				sendto_one(sptr, "SERVICE %s %s %s %d %d :%s",
					   acptr->name, acptr->service->server,
					   acptr->service->dist,
					   acptr->service->type,
					   acptr->hopcount + 1, acptr->info);
			    }
		    }
	    }
	
	if (burst & (SERVICE_WANT_CHANNEL|SERVICE_WANT_VCHANNEL|SERVICE_WANT_MODE))
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
		    }
	    }
	return 0;
}
#endif


/*
** Send query to service.
** parv[1] - string to match name against
** parv[2] - string to send to service
*/
int	m_squery(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;

	if (parc <= 2)
	    {
		if (parc == 1)
			sendto_one(sptr, err_str(ERR_NORECIPIENT, parv[0]),
				   "SQUERY");
		else if (parc == 2 || BadPtr(parv[1]))
			sendto_one(sptr, err_str(ERR_NOTEXTTOSEND, parv[0]));
		return 1;
	    }

	if ((acptr = best_service(parv[1], NULL)))
		if (MyConnect(acptr) &&
		    (acptr->service->wants & SERVICE_WANT_PREFIX))
			sendto_one(acptr, ":%s!%s@%s SQUERY %s :%s", parv[0],
				   sptr->user->username, sptr->user->host,
				   acptr->name, parv[2]);
		else
			sendto_one(acptr, ":%s SQUERY %s :%s",
				   parv[0], acptr->name, parv[2]);
	else
		sendto_one(sptr, err_str(ERR_NOSUCHSERVICE, parv[0]), parv[1]);
	return 2;
}
