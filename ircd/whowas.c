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
static const volatile char rcsid[] = "@(#)$Id: whowas.c,v 1.12 2004/11/19 15:10:08 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define WHOWAS_C
#include "s_externs.h"
#undef WHOWAS_C

static aName *was;
int ww_index = 0, ww_size = MAXCONNECTIONS * 2;
static aLock *locked;
int lk_index = 0, lk_size = MAXCONNECTIONS * 2;

static void grow_whowas(void)
{
	int osize = ww_size;

	Debug((DEBUG_ERROR, "grow_whowas ww:%d, lk:%d, #%d, %#x/%#x",
		   ww_size, lk_size, numclients, was, locked));
	ww_size = (int) ((float) numclients * 1.1);
	was = (aName *) MyRealloc((char *) was, sizeof(*was) * ww_size);
	bzero((char *) (was + osize), sizeof(*was) * (ww_size - osize));
	Debug((DEBUG_ERROR, "grow_whowas %#x", was));
	ircd_writetune(tunefile);
}

static void grow_locked(void)
{
	int osize = lk_size;

	lk_size = ww_size;
	locked = (aLock *) MyRealloc((char *) locked, sizeof(*locked) * lk_size);
	bzero((char *) (locked + osize), sizeof(*locked) * (lk_size - osize));
}

/*
** add_history
**	Add the currently defined name of the client to history.
**	usually called before changing to a new name (nick).
**	Client must be a fully registered user (specifically,
**	the user structure must have been allocated).
**	if nodelay is NULL, then the nickname will be subject to NickDelay
*/
void add_history(aClient *cptr, aClient *nodelay)
{
	Reg aName *np;
	Reg Link *uwas;

	cptr->user->refcnt++;

	np = &was[ww_index];

	if ((np->ww_online && (np->ww_online != &me)) && !(np->ww_user && np->ww_user->uwas))
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
		Reg Link **old_uwas;

		old_uwas = &(np->ww_user->uwas);
		/* (*old_uwas) should NEVER happen to be NULL. -krys */
		while ((*old_uwas)->value.i != ww_index)
			old_uwas = &((*old_uwas)->next);
		uwas = *old_uwas;
		*old_uwas = uwas->next;
		free_link(uwas);
		free_user(np->ww_user);
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
				istat.is_wwawaysmem -= strlen(np->ww_user->away) + 1;
			}
		}
		free_user(np->ww_user);
	}

	if (np->ww_logout != 0)
	{
		int elapsed = timeofday - np->ww_logout;

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
			strcpy(locked[lk_index].nick, np->ww_nick);
			locked[lk_index++].logout = np->ww_logout;
			if ((lk_index == lk_size) && (lk_size != ww_size))
			{
				grow_locked();
			}
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

	strncpyzt(np->ww_nick, cptr->name, NICKLEN + 1);
	strncpyzt(np->ww_info, cptr->info, REALLEN + 1);

	ww_index++;
	if ((ww_index == ww_size) && (numclients > ww_size))
		grow_whowas();
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
aClient *get_history(char *nick, time_t timelimit)
{
	Reg aName *wp, *wp2;

	wp = wp2 = &was[ww_index];
	timelimit = timeofday - timelimit;

	do
	{
		if (wp == was)
		{
			wp = was + ww_size;
		}
		wp--;
		if (wp->ww_logout < timelimit)
		{
			/* no point in checking more, only old or unused 
			 * entry's left. */
			return NULL;
		}
		if (wp->ww_online == &me)
		{
			/* This one is offline */
			continue;
		}
		if (wp->ww_online && !mycmp(nick, wp->ww_nick))
		{
			return wp->ww_online;
		}
	} while (wp != wp2);

	return (NULL);
}

/*
** find_history
**      Returns 1 if a user was using the given nickname within
**   the timelimit and it's locked. Returns 0, if none found...
*/
int find_history(char *nick, time_t timelimit)
{
	Reg aName *wp, *wp2;
	Reg aLock *lp, *lp2;

	wp = wp2 = &was[ww_index];
#ifdef RANDOM_NDELAY
	timelimit = timeofday - timelimit - (lk_index % 60);
#else
	timelimit = timeofday - timelimit;
#endif

	do
	{
		if (wp == was)
		{
			wp = was + ww_size;
		}
		wp--;
		if (wp->ww_logout < timelimit)
		{
			return 0;
		}
		/* wp->ww_online == NULL means it's locked */
		if ((!wp->ww_online) && (!mycmp(nick, wp->ww_nick)))
		{
			return 1;
		}
	} while (wp != wp2);

	lp = lp2 = &locked[lk_index];
	do
	{
		if (lp == locked)
		{
			lp = locked + lk_size;
		}
		lp--;
		if (lp->logout < timelimit)
		{
			return 0;
		}
		if (!mycmp(nick, lp->nick))
		{
			return 1;
		}
	} while (lp != lp2);

	return (0);
}

/*
** off_history
**	This must be called when the client structure is about to
**	be released. History mechanism keeps pointers to client
**	structures and it must know when they cease to exist. This
**	also implicitly calls AddHistory.
*/
void off_history(aClient *cptr)
{
	Reg Link *uwas;

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

void initwhowas(void)
{
	Reg int i;

	was = (aName *) MyMalloc(sizeof(*was) * ww_size);

	for (i = 0; i < ww_size; i++)
		bzero((char *) &was[i], sizeof(aName));
	locked = (aLock *) MyMalloc(sizeof(*locked) * lk_size);
	for (i = 0; i < lk_size; i++)
		bzero((char *) &locked[i], sizeof(aLock));

	ircstp->is_wwmt = ircstp->is_lkmt = DELAYCHASETIMELIMIT * DELAYCHASETIMELIMIT;
	return;
}


/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int m_whowas(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	Reg aName *wp, *wp2 = NULL;
	Reg int j = 0;
	Reg anUser *up = NULL;
	int max = -1;
	/*
	 * 2014-04-19  Kurt Roeckx
	 *  * whowas.c/m_whowas(): Initialize p to NULL for call to strtoken()
	 */
	char *p = NULL, *nick, *s;

	if (parc < 2)
	{
		sendto_one(sptr, replies[ERR_NONICKNAMEGIVEN], ME, BadTo(parv[0]));
		return 1;
	}
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr, sptr, ":%s WHOWAS %s %s :%s", 3, parc, parv))
			return 3;

	parv[1] = canonize(parv[1]);
	if (!MyConnect(sptr))
		max = MIN(max, 20);

	for (s = parv[1]; (nick = strtoken(&p, s, ",")); s = NULL)
	{
		wp = wp2 = &was[(ww_index ? ww_index : ww_size) - 1];
		j = 0;

		do {
			if (mycmp(nick, wp->ww_nick) == 0)
			{
				up = wp->ww_user;
				sendto_one(sptr, replies[RPL_WHOWASUSER],
						   ME, BadTo(parv[0]), wp->ww_nick, up->username,
						   up->host, wp->ww_info);
				sendto_one(sptr, replies[RPL_WHOISSERVER],
						   ME, BadTo(parv[0]), wp->ww_nick, up->server,
						   myctime(wp->ww_logout));
				j++;
			}
			if (max > 0 && j >= max)
				break;
			if (wp == was)
				wp = &was[ww_size - 1];
			else
				wp--;
		} while (wp != wp2);

		if (up == NULL)
		{
			if (strlen(nick) > (size_t) NICKLEN)
				nick[NICKLEN] = '\0';
			sendto_one(sptr, replies[ERR_WASNOSUCHNICK], ME, BadTo(parv[0]),
					   nick);
		}
		else
			up = NULL;

		if (p)
			p[-1] = ',';
	}
	sendto_one(sptr, replies[RPL_ENDOFWHOWAS], ME, BadTo(parv[0]), parv[1]);
	return 2;
}


/*
** for debugging...counts related structures stored in whowas array.
*/
void count_whowas_memory(int *wwu, int *wwa, u_long *wwam, int *wwuw)
{
	Reg anUser *tmp;
	Reg Link *tmpl;
	Reg int i, j;
	int u = 0, a = 0, w = 0;
	u_long am = 0;

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
					am += (strlen(tmp->away) + 1);
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
