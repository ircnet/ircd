/************************************************************************
 *   IRC - Internet Relay Chat, ircd/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 03 Jun 1990
 * Added chname initialization...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() to channel.c
 */

/* -- Jto -- 10 May 1990
 * Added #include <sys.h>
 * Changed memset(xx,0,yy) into bzero(xx,yy)
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: list.c,v 1.4 1997/09/03 17:45:51 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define LIST_C
#include "s_externs.h"
#undef LIST_C

#ifdef	DEBUGMODE
static	struct	liststats {
	int	inuse;
} cloc, crem, users, servs, links, classs, aconfs;

#endif

anUser	*usrtop = NULL;
aServer	*svrtop = NULL;

int	numclients = 0;

void	initlists()
{
#ifdef	DEBUGMODE
	bzero((char *)&cloc, sizeof(cloc));
	bzero((char *)&crem, sizeof(crem));
	bzero((char *)&users, sizeof(users));
	bzero((char *)&servs, sizeof(servs));
	bzero((char *)&links, sizeof(links));
	bzero((char *)&classs, sizeof(classs));
	bzero((char *)&aconfs, sizeof(aconfs));
#endif
}

void	outofmemory()
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	sendto_flag(SCH_NOTICE, "Ouch!!! Out of memory...");
	restart("Out of Memory");
}

#ifdef	DEBUGMODE
void	checklists()
{
	aServer	*sp;
	anUser	*up;

	for (sp = svrtop; sp; sp = sp->nexts)
		if (sp->bcptr->serv != sp)
			Debug((DEBUG_ERROR, "svrtop: %#x->%#x->%#x != %#x",
				sp, sp->bcptr, sp->bcptr->serv, sp));
	
	for (up = usrtop; up; up = up->nextu)
		if (up->bcptr->user != up)
			Debug((DEBUG_ERROR, "usrtop: %#x->%#x->%#x != %#x",
				up, up->bcptr, up->bcptr->user, up));
}
#endif
	
/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient	*make_client(from)
aClient	*from;
{
	Reg	aClient *cptr = NULL;
	Reg	unsigned size = CLIENT_REMOTE_SIZE;

	/*
	 * Check freelists first to see if we can grab a client without
	 * having to call malloc.
	 */
	if (!from)
		size = CLIENT_LOCAL_SIZE;

	if (!(cptr = (aClient *)MyMalloc(size)))
		outofmemory();
	bzero((char *)cptr, (int)size);

#ifdef	DEBUGMODE
	if (size == CLIENT_LOCAL_SIZE)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->hnext = NULL;
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	    {
		cptr->since = cptr->lasttime = cptr->firsttime = timeofday;
		cptr->confs = NULL;
		cptr->sockhost[0] = '\0';
		cptr->buffer[0] = '\0';
		cptr->authfd = -1;
		cptr->auth = cptr->username;
		cptr->exitc = EXITC_UNDEF;
#ifdef	ZIP_LINKS
		cptr->zip = NULL;
#endif
	    }
	return (cptr);
}

void	free_client(cptr)
aClient	*cptr;
{
	MyFree((char *)cptr);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
anUser	*make_user(cptr)
aClient *cptr;
{
	Reg	anUser	*user;

	user = cptr->user;
	if (!user)
	    {
		user = (anUser *)MyMalloc(sizeof(anUser));
#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->away = NULL;
		user->refcnt = 1;
		user->joined = 0;
		user->flags = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->uwas = NULL;
		cptr->user = user;
		user->nextu = NULL;
		user->prevu = NULL;
		user->servp = NULL;
		user->bcptr = cptr;
		if (cptr->next)	/* the only cptr->next == NULL is me */
			istat.is_users++;
	    }
	return user;
}

aServer	*make_server(cptr)
aClient	*cptr;
{
	Reg	aServer	*serv = cptr->serv, *sp, *spp = NULL;

	if (!serv)
	    {
		serv = (aServer *)MyMalloc(sizeof(aServer));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		serv->user = NULL;
		serv->userlist = NULL;
		serv->snum = -1;
		*serv->by = '\0';
		*serv->tok = '\0';
		serv->stok = 0;
		serv->up = NULL;
		serv->refcnt = 1;
		serv->nexts = NULL;
		cptr->serv = serv;

		for (sp = svrtop; sp; spp = sp, sp = sp->nexts)
			if (spp && ((spp->ltok) + 1 < sp->ltok))
				break;
		serv->prevs = spp;
		if (spp)
		    {
			serv->ltok = spp->ltok + 1;
			spp->nexts = serv;
		    }
		else
		    {	/* Me, myself and I alone */
			svrtop = serv;
			serv->ltok = 1;
		    }

		if (sp)
		    {
			serv->nexts = sp;
			sp->prevs = serv;
		    }
		serv->bcptr = cptr;
		SPRINTF(serv->tok, "%d", serv->ltok);
	    }
	return cptr->serv;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void	free_user(user, cptr)
Reg	anUser	*user;
aClient	*cptr;
{
	aServer *serv;

	if (--user->refcnt <= 0)
	    {
		if ((serv = user->servp))
		    {
			user->servp = NULL; /* to avoid some impossible loop */
			free_server(serv, cptr);
		    }
		if (user->away)
		    {
			istat.is_away--;
			istat.is_awaymem -= (strlen(user->away) + 1);
			MyFree((char *)user->away);
		    }
		/*
		 * sanity check
		 */
		if (user->joined || user->refcnt < 0 ||
		    user->invited || user->channel || user->uwas ||
		    user->bcptr || user->prevu || user->nextu)
		    {
			char buf[512];
			/*too many arguments for dumpcore() and sendto_flag()*/
			SPRINTF(buf, "%#x %#x %#x %#x %d %d %#x %#x %#x (%s)",
				user, user->invited, user->channel, user->uwas,
				user->joined, user->refcnt,
				user->prevu, user->nextu, user->bcptr,
				(user->bcptr) ? user->bcptr->name :"none");
#ifdef DEBUGMODE
			dumpcore("%#x user (%s!%s@%s) %s",
				 cptr, cptr ? cptr->name : "<noname>",
				 user->username, user->host, buf);
#else
			sendto_flag(SCH_ERROR,
				    "* %#x user (%s!%s@%s) %s *",
				    cptr, cptr ? cptr->name : "<noname>",
				    user->username, user->host, buf);
#endif
		    }
		MyFree((char *)user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	    }
}

void	free_server(serv, cptr)
aServer	*serv;
aClient	*cptr;
{
	if (--serv->refcnt <= 0)
	    {
		if (serv->refcnt < 0 ||	serv->prevs || serv->nexts ||
		    serv->bcptr || serv->userlist || serv->user)
		    {
			char buf[512];
			SPRINTF(buf, "%d %#x %#x %#x %#x %#x (%s)",
				serv->refcnt, serv->prevs, serv->nexts,
				serv->userlist, serv->user, serv->bcptr,
				(serv->bcptr) ? serv->bcptr->name : "none");
#ifdef DEBUGMODE
			dumpcore("%#x server %s %s",
				 cptr, cptr ? cptr->name : "<noname>", buf);
			servs.inuse--;
#else
			sendto_flag(SCH_ERROR, "* %#x server %s %s *",
				    cptr, cptr ? cptr->name : "<noname>", buf);
#endif
		    }
		MyFree((char *)serv);
	    }
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 * remove client **AND** _related structures_ from lists,
 * *free* them too. -krys
 */
void	remove_client_from_list(cptr)
Reg	aClient	*cptr;
{
	checklist();
	if (cptr->hopcount == 0) /* is there another way, at this point? */
		istat.is_localc--;
	else
		istat.is_remc--;
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else
	    {
		client = cptr->next;
		client->prev = NULL;
	    }
	if (cptr->next)
		cptr->next->prev = cptr->prev;

	if (cptr->user)
	    {
		istat.is_users--;
		/*
		** has to be removed from the list of aUser structures,
		** be careful of user->servp->userlist
		*/
		if (cptr->user->servp
		    && cptr->user->servp->userlist == cptr->user)
			if (cptr->user->nextu
			    && cptr->user->nextu->servp == cptr->user->servp)
				cptr->user->servp->userlist =cptr->user->nextu;
			else
				cptr->user->servp->userlist = NULL;
		if (cptr->user->nextu)
			cptr->user->nextu->prevu = cptr->user->prevu;
		if (cptr->user->prevu)
			cptr->user->prevu->nextu = cptr->user->nextu;
		if (usrtop == cptr->user)
		    {
			usrtop = cptr->user->nextu;
			usrtop->prevu = NULL;
		    }
		cptr->user->prevu = NULL;
		cptr->user->nextu = NULL;
		
		/* decrement reference counter, and eventually free it */
		cptr->user->bcptr = NULL;
		(void)free_user(cptr->user, cptr);
	    }

	if (cptr->serv)
	    {
		if (cptr->serv->userlist)
#ifdef DEBUGMODE
			dumpcore("%#x server %s %#x",
				 cptr, cptr ? cptr->name : "<noname>",
				 cptr->serv->userlist);
#else
			sendto_flag(SCH_ERROR, "* %#x server %s %#x *",
				    cptr, cptr ? cptr->name : "<noname>",
				    cptr->serv->userlist);
#endif

		/* has to be removed from the list of aServer structures */
		if (cptr->serv->nexts)
			cptr->serv->nexts->prevs = cptr->serv->prevs;
		if (cptr->serv->prevs)
			cptr->serv->prevs->nexts = cptr->serv->nexts;
		if (svrtop == cptr->serv)
			svrtop = cptr->serv->nexts;
		cptr->serv->prevs = NULL;
		cptr->serv->nexts = NULL;
		
		if (cptr->serv->user)
		    {
			free_user(cptr->serv->user, cptr);
			cptr->serv->user = NULL;
		    }

		/* decrement reference counter, and eventually free it */
		cptr->serv->bcptr = NULL;
		free_server(cptr->serv, cptr);
	    }

	if (cptr->service)
		/*
		** has to be removed from the list of aService structures,
		** no reference counter for services, thus this part of the
		** code can safely be included in free_service()
		*/
		free_service(cptr);

#ifdef	DEBUGMODE
	if (cptr->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif

	(void)free_client(cptr);
	numclients--;
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void	add_client_to_list(cptr)
aClient	*cptr;
{
	/*
	 * since we always insert new clients to the top of the list,
	 * this should mean the "me" is the bottom most item in the list.
	 */
	if (cptr->from == cptr)
		istat.is_localc++;
	else
		istat.is_remc++;
	if (cptr->user)
		istat.is_users++;
	cptr->next = client;
	client = cptr;
	if (cptr->next)
		cptr->next->prev = cptr;
	numclients++;
	return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link	*find_user_link(lp, ptr)
Reg	Link	*lp;
Reg	aClient *ptr;
{
	if (ptr)
		for (; lp; lp = lp->next)
			if (lp->value.cptr == ptr)
				return (lp);
	return NULL;
}

Link  *find_channel_link(lp, ptr)
Reg   Link    *lp;
Reg   aChannel *ptr; 
{ 
	if (ptr)
		for (; lp; lp = lp->next)
			if (lp->value.chptr == ptr)
				return (lp);
	return NULL;    
}

Link	*make_link()
{
	Reg	Link	*lp;

	lp = (Link *)MyMalloc(sizeof(Link));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	lp->flags = 0;
	return lp;
}

void	free_link(lp)
Reg	Link	*lp;
{
	MyFree((char *)lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}


aClass	*make_class()
{
	Reg	aClass	*tmp;

	tmp = (aClass *)MyMalloc(sizeof(aClass));
#ifdef	DEBUGMODE
	classs.inuse++;
#endif
	return tmp;
}

void	free_class(tmp)
Reg	aClass	*tmp;
{
	MyFree((char *)tmp);
#ifdef	DEBUGMODE
	classs.inuse--;
#endif
}

aConfItem	*make_conf()
{
	Reg	aConfItem *aconf;

	aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));

#ifdef	DEBUGMODE
	aconfs.inuse++;
#endif
	istat.is_conf++;
	istat.is_confmem += sizeof(aConfItem);

	bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	aconf->clients = aconf->port = 0;
	aconf->next = NULL;
	aconf->host = aconf->passwd = aconf->name = NULL;
	aconf->ping = NULL;
	aconf->status = CONF_ILLEGAL;
	aconf->pref = -1;
	aconf->hold = time(NULL);
	Class(aconf) = NULL;
	return (aconf);
}

void	delist_conf(aconf)
aConfItem	*aconf;
{
	if (aconf == conf)
		conf = conf->next;
	else
	    {
		aConfItem	*bconf;

		for (bconf = conf; aconf != bconf->next; bconf = bconf->next)
			;
		bconf->next = aconf->next;
	    }
	aconf->next = NULL;
}

void	free_conf(aconf)
aConfItem *aconf;
{
	del_queries((char *)aconf);

	istat.is_conf--;
	istat.is_confmem -= aconf->host ? strlen(aconf->host)+1 : 0;
	istat.is_confmem -= aconf->passwd ? strlen(aconf->passwd)+1 : 0;
	istat.is_confmem -= aconf->name ? strlen(aconf->name)+1 : 0;
	istat.is_confmem -= aconf->ping ? sizeof(*aconf->ping) : 0;
	istat.is_confmem -= sizeof(aConfItem);

	MyFree(aconf->host);
	if (aconf->passwd)
		bzero(aconf->passwd, strlen(aconf->passwd));
	if (aconf->ping)
		MyFree((char *)aconf->ping);
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	MyFree((char *)aconf);
#ifdef	DEBUGMODE
	aconfs.inuse--;
#endif
	return;
}

#ifdef	DEBUGMODE
void	send_listinfo(cptr, name)
aClient	*cptr;
char	*name;
{
	int	inuse = 0, mem = 0, tmp = 0;

	sendto_one(cptr, ":%s %d %s :Local: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, inuse += cloc.inuse,
		   tmp = cloc.inuse * CLIENT_LOCAL_SIZE);
	mem += tmp;
	sendto_one(cptr, ":%s %d %s :Remote: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name,
		   crem.inuse, tmp = crem.inuse * CLIENT_REMOTE_SIZE);
	mem += tmp;
	inuse += crem.inuse;
	sendto_one(cptr, ":%s %d %s :Users: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, users.inuse,
		   tmp = users.inuse * sizeof(anUser));
	mem += tmp;
	inuse += users.inuse,
	sendto_one(cptr, ":%s %d %s :Servs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, servs.inuse,
		   tmp = servs.inuse * sizeof(aServer));
	mem += tmp;
	inuse += servs.inuse,
	sendto_one(cptr, ":%s %d %s :Links: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, links.inuse,
		   tmp = links.inuse * sizeof(Link));
	mem += tmp;
	inuse += links.inuse,
	sendto_one(cptr, ":%s %d %s :Classes: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, classs.inuse,
		   tmp = classs.inuse * sizeof(aClass));
	mem += tmp;
	inuse += classs.inuse,
	sendto_one(cptr, ":%s %d %s :Confs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, aconfs.inuse,
		   tmp = aconfs.inuse * sizeof(aConfItem));
	mem += tmp;
	inuse += aconfs.inuse,
	sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
		   me.name, RPL_STATSDEBUG, name, inuse, mem);
}
#endif


void	add_fd(fd, ary)
int	fd;
FdAry	*ary;
{
	Debug((DEBUG_DEBUG,"add_fd(%d,%#x)", fd, ary));
	if (fd >= 0)
		ary->fd[++(ary->highest)] = fd;
}


int	del_fd(fd, ary)
int	fd;
FdAry	*ary;
{
	int	i;

	Debug((DEBUG_DEBUG,"del_fd(%d,%#x)", fd, ary));
	if ((ary->highest == -1) || (fd < 0))
		return -1;
	for (i = 0; i <= ary->highest; i++)
		if (ary->fd[i] == fd)
			break;
	if (i < ary->highest)
	    {
		ary->fd[i] = ary->fd[ary->highest--];
		return 0;
	    }
	else if (i > ary->highest)
		return -1;
	ary->highest--;
	return 0;
}


#ifdef	HUB
void	add_active(fd, ary)
int	fd;
FdAry	*ary;
{
	del_fd(fd, ary);

	if (ary->highest == MAXCONNECTIONS/(HUB+1))
	    {
		bcopy((char *)ary->fd, (char *)&ary->fd[1],
			sizeof(int) * (MAXCONNECTIONS/(HUB + 1) - 1));
		*ary->fd = fd;
	    }
	else
		ary->fd[++(ary->highest)] = fd;
}


void	decay_activity()
{
	aClient	*cptr;
	int	i;

	for (i = highest_fd; i >= 0; i--)
		if ((cptr = local[i]) && IsPerson(cptr))
		    {
			if (cptr->ract)
				cptr->ract--;
			if (cptr->sact)
				cptr->sact--;
		    }
}


static	time_t	sorttime;

int	sort_active(a1, a2)
const void	*a1, *a2;
{
	aClient	*acptr = local[*(int *)a1], *bcptr = local[*(int *)a2];

	/*
	** give preference between a flooded client and a non-flooded client
	** to the non-flooded client.
	*/
	if (acptr->since > sorttime)
		if (bcptr->since > sorttime)
			return 0;
		else
			return -1;
	else if (bcptr->since > sorttime)
		return 1;

	/*
	** if one client has a partial message in its receive buffer, give
	** it preference over the other.
	*/
	if (DBufLength(&acptr->recvQ) && !DBufLength(&bcptr->recvQ))
		return 1;
	else if (DBufLength(&bcptr->recvQ) && !DBufLength(&acptr->recvQ))
		return -1;

	return local[*(int *)a1]->priority - local[*(int *)a2]->priority;
}

void	build_active()
{
	aClient	*cptr;
	FdAry	*ap = &fdaa;
	int	i;

	sorttime = timeofday;
	/*
	** first calculate priority...
	*/
	for (i = highest_fd; i >= 0; i--)
	    {
		if (!(cptr = local[i]))
			continue;
		if (!IsPerson(cptr))
			cptr->priority = 0;
		else
			cptr->priority = cptr->ract + cptr->sact;
	    }

	/*
	** then generate active array
	*/
	ap->highest = -1;
	for (i = highest_fd; i >= 0; i--)
		if (local[i])
			add_fd(i, ap);

	qsort(ap->fd, ap->highest+1, sizeof(*ap->fd), sort_active);

	if (ap->highest >= MAXCONNECTIONS/(HUB+1))
		ap->highest = MAXCONNECTIONS/(HUB+1)-1;
}
#endif
