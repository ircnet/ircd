/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas.c
 *   Copyright (C) 1990 Markku Savela
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

/*
 * --- avalon --- 6th April 1992
 * rewritten to scrap linked lists and use a table of structures which
 * is referenced like a circular loop. Should be faster and more efficient.
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: whowas.c,v 1.3 1997/09/03 17:46:08 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define WHOWAS_C
#include "s_externs.h"
#undef WHOWAS_C

static	aName	*was;
int	ww_index = 0, ww_size = MAXCONNECTIONS*2;
static	aLock	*locked;
int	lk_index = 0, lk_size = MAXCONNECTIONS*2;

static	void	grow_history()
{
	int	osize = ww_size;

	Debug((DEBUG_ERROR, "Whowas/grow_history ww:%d, lk:%d, #%d, %#x/%#x",
			    ww_size, lk_size, numclients, was, locked));
	ww_size = (int)((float)numclients * 1.1);
	was = (aName *)MyRealloc((char *)was, sizeof(*was) * ww_size);
	bzero((char *)(was + osize), sizeof(*was) * (ww_size - osize));
	lk_size = (int)((float)numclients * 1.1);
	locked = (aLock *)MyRealloc((char *)locked, sizeof(*locked) * lk_size);
	bzero((char *)(locked + osize), sizeof(*locked) * (lk_size - osize));
	Debug((DEBUG_ERROR, "Whowas/grow_history %#x/%#x", was, locked));
	ircd_writetune(tunefile);
}


/*
** add_history
**	Add the currently defined name of the client to history.
**	usually called before changing to a new name (nick).
**	Client must be a fully registered user (specifically,
**	the user structure must have been allocated).
*/
void	add_history(cptr, nodelay)
Reg	aClient	*cptr, *nodelay;
{
	Reg	aName	*np;
	Reg	Link	*uwas;

	cptr->user->refcnt++;

	np = &was[ww_index];

	if ((np->ww_online && (np->ww_online != &me))
	    && !(np->ww_user && np->ww_user->uwas))
#ifdef DEBUGMODE
		dumpcore("whowas[%d] %#x %#x %#x", ww_index, np->ww_online,
			 np->ww_user, np->ww_user->uwas);
#else
		sendto_flag(SCH_ERROR, "whowas[%d] %#x %#x %#x", ww_index,
			    np->ww_online, np->ww_user, np->ww_user->uwas);
#endif
	/*
	** The entry to be overwritten in was[] is still online.
	** its uwas has to be updated
	*/
	if (np->ww_online && (np->ww_online != &me))
	    {
		Reg	Link	**old_uwas;

		old_uwas = &(np->ww_user->uwas);
		/* (*old_uwas) should NEVER happen to be NULL. -krys */
		while ((*old_uwas)->value.i != ww_index)
			old_uwas = &((*old_uwas)->next);
		uwas = *old_uwas;
		*old_uwas = uwas->next;
		free_link(uwas);
		free_user(np->ww_user, np->ww_online);
		istat.is_wwuwas--;
	    }
	else if (np->ww_user)
	    {
		/*
		** Testing refcnt here is quite ugly, and unexact.
		** Nonetheless, the result is almost correct, and another
		** ugly thing in free_server() shoud make it exact.
		** The problem is that 1 anUser structure might be
		** referenced several times from whowas[] but is only counted
		** once. One knows when to add, not when to substract - krys
		*/
		if (np->ww_user->refcnt == 1)
		    {
			istat.is_wwusers--;
			if (np->ww_user->away)
			    {
				istat.is_wwaways--;
				istat.is_wwawaysmem -=strlen(np->ww_user->away)
							+ 1;
			    }
		    }
		free_user(np->ww_user, NULL);
	    }

	if (np->ww_logout != 0)
	    {
		int	elapsed = timeofday - np->ww_logout;

		/* some stats */
		ircstp->is_wwcnt++;
		ircstp->is_wwt += elapsed;
		if (elapsed < ircstp->is_wwmt)
			ircstp->is_wwmt = elapsed;
		if (elapsed > ircstp->is_wwMt)
			ircstp->is_wwMt = elapsed;

		if (np->ww_online == NULL)
		    {
			if (locked[lk_index].logout)
			    {
				elapsed = timeofday - locked[lk_index].logout;
				/* some stats first */
				ircstp->is_lkcnt++;
				ircstp->is_lkt += elapsed;
				if (elapsed < ircstp->is_lkmt)
					ircstp->is_lkmt = elapsed;
				if (elapsed > ircstp->is_lkMt)
					ircstp->is_lkMt = elapsed;
			    }

			/*
			 ** This nickname has to be locked, thus copy it to the
			 ** lock[] array.
			 */
			strncpyzt(locked[lk_index].nick, np->ww_nick, NICKLEN);
			locked[lk_index++].logout = np->ww_logout;
			if (lk_index >= lk_size)
				lk_index = 0;
		    }
	    }

	if (nodelay == cptr) /* &me is NOT a valid value, see off_history() */
	    {
		/*
		** The client is online, np->ww_online is going to point to
		** it. The client user struct has to point to this entry
		** as well for faster off_history()
		** this uwas, and the ww_online form a pair.
		*/
		uwas = make_link();
		istat.is_wwuwas++;
		/*
		** because of reallocs, one can not store a pointer inside
		** the array. store the index instead.
		*/
		uwas->value.i = ww_index;
		uwas->flags = timeofday;
		uwas->next = cptr->user->uwas;
		cptr->user->uwas = uwas;
	    }

	np->ww_logout = timeofday;
	np->ww_user = cptr->user;
	np->ww_online = (nodelay != NULL) ? nodelay : NULL;

	strncpyzt(np->ww_nick, cptr->name, NICKLEN+1);
	strncpyzt(np->ww_info, cptr->info, REALLEN+1);

	ww_index++;
	if ((ww_index == ww_size) && (numclients > ww_size))
		grow_history();
	if (ww_index >= ww_size)
		ww_index = 0;
	return;
}

/*
** get_history
**      Return the current client that was using the given
**      nickname within the timelimit. Returns NULL, if no
**      one found...
*/
aClient	*get_history(nick, timelimit)
char	*nick;
time_t	timelimit;
{
	Reg	aName	*wp, *wp2;
	Reg	int	i = 0;

	wp = wp2 = &was[ww_index];
	timelimit = timeofday - timelimit;

	do {
		if (!mycmp(nick, wp->ww_nick) && wp->ww_logout >= timelimit)
			break;
		wp++;
		if (wp == &was[ww_size])
			i = 1, wp = was;
	} while (wp != wp2);

	if (wp != wp2 || !i)
		if (wp->ww_online == &me)
			return (NULL);
		else
			return (wp->ww_online);
	return (NULL);
}

/*
** find_history
**      Returns 1 if a user was using the given nickname within
**   the timelimit. Returns 0, if none found...
*/
int	find_history(nick, timelimit)
char  *nick;
time_t        timelimit;
{
	Reg     aName   *wp, *wp2;
	Reg	aLock	*lp, *lp2;
	Reg     int     i = 0;
	
	wp = wp2 = &was[ww_index];
#ifdef RANDOM_NDELAY	
	timelimit = timeofday - timelimit - (lk_index % 60);
#else
	timelimit = timeofday - timelimit;
#endif
	
	do {
		if (!mycmp(nick, wp->ww_nick) &&
		    (wp->ww_logout >= timelimit) && (wp->ww_online == NULL))
			break;
		wp++;
		if (wp == &was[ww_size])
			i = 1, wp = was;
	} while (wp != wp2);
	if ((wp != wp2 || !i) && (wp->ww_online == NULL))
		return (1);

	lp = lp2 = &locked[lk_index];
	i = 0;
	do {
		if (!myncmp(nick, lp->nick, NICKLEN) &&
		    (lp->logout >= timelimit))
			break;
		lp++;
		if (lp == &locked[lk_size])
			i = 1, lp = locked;
	} while (lp != lp2);
	if (lp != lp2 || !i)
		return (1);

	return (0);
}

/*
** off_history
**	This must be called when the client structure is about to
**	be released. History mechanism keeps pointers to client
**	structures and it must know when they cease to exist. This
**	also implicitly calls AddHistory.
*/
void	off_history(cptr)
Reg	aClient	*cptr;
{
	Reg	Link	*uwas;

	/*
	** If the client has uwas entry/ies, there are also entries in
	** the whowas array which point back to it.
	** They have to be removed, by pairs
	*/
	while ((uwas = cptr->user->uwas))
	    {
		if (was[uwas->value.i].ww_online != cptr)
#ifdef DEBUGMODE
			dumpcore("was[%d]: %#x != %#x", uwas->value.i,
				 was[uwas->value.i].ww_online, cptr);
#else
			sendto_flag(SCH_ERROR, "was[%d]: %#x != %#x", 
				    uwas->value.i, 
				    was[uwas->value.i].ww_online, cptr);
#endif
		/*
		** using &me to make ww_online non NULL (nicknames to be
		** locked). &me can safely be used, it is constant.
		*/
		was[uwas->value.i].ww_online = &me;
		cptr->user->uwas = uwas->next;
		free_link(uwas);
		istat.is_wwuwas--;
	    }

	istat.is_wwusers++;
	if (cptr->user->away)
	    {
		istat.is_wwaways++;
		istat.is_wwawaysmem += strlen(cptr->user->away) + 1;
	    }
		
	return;
}

void	initwhowas()
{
	Reg	int	i;

	was = (aName *)MyMalloc(sizeof(*was) * ww_size);

	for (i = 0; i < ww_size; i++)
		bzero((char *)&was[i], sizeof(aName));
	locked = (aLock *)MyMalloc(sizeof(*locked) * lk_size);
	for (i = 0; i < lk_size; i++)
		bzero((char *)&locked[i], sizeof(aLock));

	ircstp->is_wwmt = ircstp->is_lkmt = DELAYCHASETIMELIMIT
						* DELAYCHASETIMELIMIT;
	return;
}


/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int	m_whowas(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg	aName	*wp, *wp2 = NULL;
	Reg	int	j = 0;
	Reg	anUser	*up = NULL;
	int	max = -1;
	char	*p, *nick, *s;

 	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN, parv[0]));
		return 1;
	    }
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr,sptr,":%s WHOWAS %s %s :%s", 3,parc,parv))
			return 3;

	parv[1] = canonize(parv[1]);
	if (!MyConnect(sptr))
		max = MIN(max, 20);

	for (s = parv[1]; (nick = strtoken(&p, s, ",")); s = NULL)
	    {
		wp = wp2 = &was[ww_index - 1];

		do {
			if (wp < was)
				wp = &was[ww_size - 1];
			if (mycmp(nick, wp->ww_nick) == 0)
			    {
				up = wp->ww_user;
				sendto_one(sptr, rpl_str(RPL_WHOWASUSER,
					   parv[0]), wp->ww_nick, up->username,
					   up->host, wp->ww_info);
				sendto_one(sptr, rpl_str(RPL_WHOISSERVER,
					   parv[0]), wp->ww_nick, up->server,
					   myctime(wp->ww_logout));
				if (up->away)
					sendto_one(sptr, rpl_str(RPL_AWAY,
						   parv[0]),
						   wp->ww_nick, up->away);
				j++;
			    }
			if (max > 0 && j >= max)
				break;
			wp--;
		} while (wp != wp2);

		if (up == NULL)
		    {
			if (strlen(parv[1]) > (size_t) NICKLEN)
				parv[1][NICKLEN] = '\0';
			sendto_one(sptr, err_str(ERR_WASNOSUCHNICK, parv[0]),
				   parv[1]);
		    }

		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS, parv[0]), parv[1]);
	return 2;
    }


/*
** for debugging...counts related structures stored in whowas array.
*/
void	count_whowas_memory(wwu, wwa, wwam, wwuw)
int	*wwu, *wwa, *wwuw;
u_long	*wwam;
{
	Reg	anUser	*tmp;
	Reg	Link	*tmpl;
	Reg	int	i, j;
	int	u = 0, a = 0, w = 0;
	u_long	am = 0;

	for (i = 0; i < ww_size; i++)
		if ((tmp = was[i].ww_user))
		{
			for (j = 0; j < i; j++)
				if (was[j].ww_user == tmp)
					break;
			if (j < i)
				continue;
			if (was[i].ww_online == NULL ||
			    was[i].ww_online == &me)
			{
				u++;
				if (tmp->away)
				{
					a++;
					am += (strlen(tmp->away)+1);
				}
			}
			else
			{
				tmpl = tmp->uwas;
				while (tmpl)
				{
					w++;
					tmpl = tmpl->next;
				}
			}
		}
	*wwu = u;
	*wwa = a;
	*wwam = am;
	*wwuw = w;

	return;
}
