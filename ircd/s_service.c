#include "struct.h"
#include "common.h"
#include "sys.h"
#include "service.h"
#include "msg.h"
#include "numeric.h"
#include "h.h"

static	aService	*svctop = NULL;

aService	*make_service(cptr)
aClient	*cptr;
{
	Reg	aService	*svc = cptr->service;

	if (svc)
		return svc;

	cptr->service = svc = (aService *)MyMalloc(sizeof(*svc));
	bzero((char *)svc, sizeof(*svc));
	if (svctop)
		svctop->prevs = svc;
	svc->nexts = svctop;
	svc->prevs = NULL;
	svc->bcptr = cptr;
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
		    {
			svctop = serv->nexts;
			serv->prevs = NULL;
		    }
		serv->prevs = NULL;
		serv->nexts = NULL;
		if (serv->servp)
			free_server(serv->servp, cptr);

		MyFree(serv);
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
	int	metric = -1;
	int	type = -1, len = strlen(name);

	if (!index(name, '@') || !(acptr = find_service(name, cptr)))
		for (sp = svctop; sp; sp = sp->nexts)
			if ((bcptr = sp->bcptr) &&
			    !myncmp(name, bcptr->name, len))
			    {
				acptr = bcptr;
				break;
			    }
	if (acptr && acptr->hopcount > 1)
		for (type = acptr->service->type, metric = acptr->hopcount,
		     sp = svctop; sp; sp = sp->nexts)
			if (sp->type == type && (bcptr = sp->bcptr) &&
			    bcptr->hopcount < metric)
				acptr = bcptr;
	return (acptr ? acptr : cptr);
}
 

#ifdef	USE_SERVICES
void	check_services_butone(action, server, cptr, fmt, p1, p2, p3, p4,
			      p5, p6, p7, p8)
long	action;
aClient	*cptr;
char	*fmt, *server;
void	*p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
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
		    && (!server || !matches(acptr->service->dist, server)))
			sendto_one(acptr, fmt, p1, p2, p3, p4, p5, p6, p7, p8);
	    }
	return;
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
				if (matches(tmp->host, uhost) == 0)
					return tmp;
			    }
		SPRINTF(uhost, "%s@%s", cptr->username, cptr->sockhost);
		if (matches(tmp->host, uhost) == 0)
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
**	parv[2] = server name
**	parv[3] = distution code
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
	char	*dist, *server = NULL, *info;
	int	type, metric = 0, i/*, tok = 1*/;

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
		if (cptr->serv->version != SV_OLD)
			sp = find_tokserver(atoi(server), cptr, NULL);
		else if ((acptr = find_server(server, NULL))) /* uhh.. */
			sp = acptr->serv;
/*
		if (sp)
			tok = sp->ltok;
*/
	    }
#ifndef	USE_SERVICES
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

		(void)strcpy(sptr->name, parv[1]);
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
		sendto_one(sptr, rpl_str(RPL_WELCOME, sptr->name), sptr->name);
		sendto_one(sptr, rpl_str(RPL_MYINFO, sptr->name), ME, version);
		sendto_flag(SCH_NOTICE, "Service %s connected",
			    get_client_name(sptr, TRUE));
		istat.is_myservice++;
	    }
#endif

	istat.is_service++;
	svc = make_service(sptr);
	SetService(sptr);
	svc->server = mystrdup(server);
	strncpyzt(svc->dist, dist, HOSTLEN);
	strncpyzt(sptr->info, info, REALLEN);
	svc->wants = 0;
	svc->type = type;
	sptr->hopcount = metric;
/*	SPRINTF(svc->tok, "%d", tok);*/
	if ((svc->servp = sp)) /* why if ? */
		sp->refcnt++;
	(void)add_to_client_hash_table(sptr->name, sptr);

#ifdef	USE_SERVICES
	check_services_butone(SERVICE_WANT_SERVICE, NULL, sptr,
				"SERVICE %s %s %s %d %d :%s", sptr->name,
				server, dist, type, metric, info);
#endif

	for (i = fdas.highest; i >= 0; i--)
	{
		if (!(acptr = local[fdas.fd[i]]) || !IsServer(acptr) ||
		    acptr == cptr)
			continue;
		if (matches(dist, acptr->name))
			continue;
		if (acptr->serv->version == SV_OLD)
			continue;
		sendto_one(acptr, "SERVICE %s %s %s %d %d :%s", sptr->name,
			   server, dist, type, metric+1, info);
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
		     (matches(mask, acptr->name) == 0))
			sendto_one(sptr, rpl_str(RPL_SERVLIST, parv[0]),
				   acptr->name, sp->server, sp->dist,
				   sp->type, acptr->hopcount, acptr->info);
	sendto_one(sptr, rpl_str(RPL_SERVLISTEND, parv[0]), mask, type);
	return 1;
}


#ifdef	USE_SERVICES
int	m_servset(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
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
	if (!sptr->service->wants)
		sptr->service->wants = atoi(parv[1]) & sptr->service->type;
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
		sendto_one(acptr, ":%s SQUERY %s :%s",
			   parv[0], acptr->name, parv[2]);
	else
		sendto_one(sptr, err_str(ERR_NOSUCHSERVICE, parv[0]), parv[1]);
	return 1;
}
