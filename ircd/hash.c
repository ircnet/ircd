/************************************************************************
 *   IRC - Internet Relay Chat, ircd/hash.c
 *   Copyright (C) 1991 Darren Reed
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
static  char rcsid[] = "@(#)$Id: hash.c,v 1.9 1998/05/25 20:44:20 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define HASH_C
#include "s_externs.h"
#undef HASH_C

static	aHashEntry	*clientTable = NULL;
static	aHashEntry	*channelTable = NULL;
static	aHashEntry	*serverTable = NULL;
static	unsigned int	*hashtab = NULL;
static	int	clhits = 0, clmiss = 0, clsize = 0;
static	int	chhits = 0, chmiss = 0, chsize = 0;
static	int	svsize = 0;
int	_HASHSIZE = 0;
int	_CHANNELHASHSIZE = 0;
int	_SERVERSIZE = 0;

/*
 * Hashing.
 *
 *   The server uses a chained hash table to provide quick and efficient
 * hash table mantainence (providing the hash function works evenly over
 * the input range).  The hash table is thus not susceptible to problems
 * of filling all the buckets or the need to rehash.
 *    It is expected that the hash table would look somehting like this
 * during use:
 *                   +-----+    +-----+    +-----+   +-----+
 *                ---| 224 |----| 225 |----| 226 |---| 227 |---
 *                   +-----+    +-----+    +-----+   +-----+
 *                      |          |          |
 *                   +-----+    +-----+    +-----+
 *                   |  A  |    |  C  |    |  D  |
 *                   +-----+    +-----+    +-----+
 *                      |
 *                   +-----+
 *                   |  B  |
 *                   +-----+
 *
 * A - GOPbot, B - chang, C - hanuaway, D - *.mu.OZ.AU
 *
 * The order shown above is just one instant of the server.  Each time a
 * lookup is made on an entry in the hash table and it is found, the entry
 * is moved to the top of the chain.
 */

/*
 * hash_nick_name
 *
 * this function must be *quick*.  Thus there should be no multiplication
 * or division or modulus in the inner loop.  subtraction and other bit
 * operations allowed.
 */
static	u_int	hash_nick_name(nname, store)
char	*nname;
int	*store;
{
	Reg	u_char	*name = (u_char *)nname;
	Reg	u_char	ch;
	Reg	u_int	hash = 1;

	for (; (ch = *name); name++)
	{
		hash <<= 1;
		hash += hashtab[(int)ch];
	}
	/*
	if (hash < 0)
		hash = -hash;
	*/
	*store = hash;
	hash %= _HASHSIZE;
	return (hash);
}

/*
 * hash_channel_name
 *
 * calculate a hash value on at most the first 30 characters of the channel
 * name. Most names are short than this or dissimilar in this range. There
 * is little or no point hashing on a full channel name which maybe 255 chars
 * long.
 */
static	u_int	hash_channel_name(hname, store)
char	*hname;
int	*store;
{
	Reg	u_char	*name = (u_char *)hname;
	Reg	u_char	ch;
	Reg	int	i = 30;
	Reg	u_int	hash = 5;

	if (*name == '!')
		name += 1 + CHIDLEN;
	for (; (ch = *name) && --i; name++)
	{
		hash <<= 1;
		hash += hashtab[(u_int)ch] + (i << 1);
	}
	*store = hash;
	hash %= _CHANNELHASHSIZE;
	return (hash);
}

/* bigger prime
 *
 * given a positive integer, return a prime number that's larger
 *
 * 13feb94 gbl
 */
static	int	bigger_prime(size)
int	size;
{
	int	trial, failure, sq;

	if (size < 0)
		return -1;

	if (size < 4)
		return size;

	if (size % 2 == 0)      /* Make sure it's odd because... */
		size++;
	
	for ( ; ; size += 2)    /* ...no point checking even numbers - Core */
	    {
		failure = 0;
		sq = (int)sqrt((double)size);
		for (trial = 2; trial <= sq ; trial++)
		    {
			if ((size % trial) == 0)
			    {
				failure = 1;
				break;
			    }
		    }
		if (!failure)
			return size;
	    }
	/* return -1; */	/* Never reached */
}

/*
 * clear_*_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
static	void	clear_client_hash_table(size)
int	size;
{
	_HASHSIZE = bigger_prime(size);
	clhits = 0;
	clmiss = 0;
	if (!clientTable)
		clientTable = (aHashEntry *)MyMalloc(_HASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)clientTable, sizeof(aHashEntry) * _HASHSIZE);
	Debug((DEBUG_DEBUG, "Client Hash Table Init: %d (%d)",
		_HASHSIZE, size));
}

static	void	clear_channel_hash_table(size)
int	size;
{
	_CHANNELHASHSIZE = bigger_prime(size);
	chmiss = 0;
	chhits = 0;
	if (!channelTable)
		channelTable = (aHashEntry *)MyMalloc(_CHANNELHASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)channelTable, sizeof(aHashEntry) * _CHANNELHASHSIZE);
	Debug((DEBUG_DEBUG, "Channel Hash Table Init: %d (%d)",
		_CHANNELHASHSIZE, size));
}

static	void	clear_server_hash_table(size)
int	size;
{
	_SERVERSIZE = bigger_prime(size);
	if (!serverTable)
		serverTable = (aHashEntry *)MyMalloc(_SERVERSIZE *
						     sizeof(aHashEntry));
	bzero((char *)serverTable, sizeof(aHashEntry) * _SERVERSIZE);
	Debug((DEBUG_DEBUG, "Server Hash Table Init: %d (%d)",
		_SERVERSIZE, size));
}

void	inithashtables()
{
	Reg int i;

	clear_client_hash_table((_HASHSIZE) ? _HASHSIZE : HASHSIZE);
	clear_channel_hash_table((_CHANNELHASHSIZE) ? _CHANNELHASHSIZE
                                 : CHANNELHASHSIZE);
	clear_server_hash_table((_SERVERSIZE) ? _SERVERSIZE : SERVERSIZE);

	/*
	 * Moved multiplication out from the hashfunctions and into
	 * a pre-generated lookup table. Should save some CPU usage
	 * even on machines with a fast mathprocessor.  -- Core
	 */
	hashtab = (u_int *) MyMalloc(256 * sizeof(u_int));
	for (i = 0; i < 256; i++)
		hashtab[i] = tolower((char)i) * 109;
}

static	void	bigger_hash_table(size, table, new)
int	*size;
aHashEntry	*table;
int	new;
{
	Reg	aClient	*cptr;
	Reg	aChannel *chptr;
	Reg	aServer	*sptr;
	aHashEntry	*otab = table;
	int	osize = *size;

	while (!new || new <= osize)
		if (!new)
		    {
			new = osize;
			new = bigger_prime(1 + (int)((float)new * 1.30));
		    }
		else
			new = bigger_prime(1 + new);

	Debug((DEBUG_NOTICE, "bigger_h_table(*%#x = %d,%#x,%d)",
		size, osize, table, new));

	*size = new;
	MyFree((char *)table);
	table = (aHashEntry *)MyMalloc(sizeof(*table) * new);
	bzero((char *)table, sizeof(*table) * new);

	if (otab == channelTable)
	    {
		Debug((DEBUG_ERROR, "Channel Hash Table from %d to %d (%d)",
			    osize, new, chsize));
		chmiss = 0;
		chhits = 0;
		chsize = 0;
		channelTable = table;
		for (chptr = channel; chptr; chptr = chptr->nextch)
			chptr->hnextch = NULL;
		for (chptr = channel; chptr; chptr = chptr->nextch)
			(void)add_to_channel_hash_table(chptr->chname, chptr);
		sendto_flag(SCH_HASH, "Channel Hash Table from %d to %d (%d)",
			    osize, new, chsize);
	    }
	else if (otab == clientTable)
	    {
		Debug((DEBUG_ERROR, "Client Hash Table from %d to %d (%d)",
			    osize, new, clsize));
		sendto_flag(SCH_HASH, "Client Hash Table from %d to %d (%d)",
			    osize, new, clsize);
		clmiss = 0;
		clhits = 0;
		clsize = 0;
		clientTable = table;
		for (cptr = client; cptr; cptr = cptr->next)
			cptr->hnext = NULL;
		for (cptr = client; cptr; cptr = cptr->next)
			(void)add_to_client_hash_table(cptr->name, cptr);
	    }
	else if (otab == serverTable)
	    {
		Debug((DEBUG_ERROR, "Server Hash Table from %d to %d (%d)",
			    osize, new, svsize));
		sendto_flag(SCH_HASH, "Server Hash Table from %d to %d (%d)",
			    osize, new, svsize);
		svsize = 0;
		serverTable = table;
		for (sptr = svrtop; sptr; sptr = sptr->nexts)
			sptr->shnext = NULL;
		for (sptr = svrtop; sptr; sptr = sptr->nexts)
			(void)add_to_server_hash_table(sptr, sptr->bcptr);
	    }
	ircd_writetune(tunefile);
	return;
}


/*
 * add_to_client_hash_table
 */
int	add_to_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg	u_int	hashv;

	hashv = hash_nick_name(name, &cptr->hashv);
	cptr->hnext = (aClient *)clientTable[hashv].list;
	clientTable[hashv].list = (void *)cptr;
	clientTable[hashv].links++;
	clientTable[hashv].hits++;
	clsize++;
	if (clsize > _HASHSIZE)
		bigger_hash_table(&_HASHSIZE, clientTable, 0);
	return 0;
}

/*
 * add_to_channel_hash_table
 */
int	add_to_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	Reg	u_int	hashv;

	hashv = hash_channel_name(name, &chptr->hashv);
	chptr->hnextch = (aChannel *)channelTable[hashv].list;
	channelTable[hashv].list = (void *)chptr;
	channelTable[hashv].links++;
	channelTable[hashv].hits++;
	chsize++;
	if (chsize > _CHANNELHASHSIZE)
		bigger_hash_table(&_CHANNELHASHSIZE, channelTable, 0);
	return 0;
}

/*
 * add_to_server_hash_table
 */
int	add_to_server_hash_table(sptr, cptr)
aServer	*sptr;
aClient	*cptr;
{
	Reg	u_int	hashv;

	Debug((DEBUG_DEBUG, "Add %s token %d/%d/%s cptr %#x to server table",
		sptr->bcptr->name, sptr->stok, sptr->ltok, sptr->tok, cptr));
	hashv = sptr->stok * 15053;
	hashv %= _SERVERSIZE;
	sptr->shnext = (aServer *)serverTable[hashv].list;
	serverTable[hashv].list = (void *)sptr;
	serverTable[hashv].links++;
	serverTable[hashv].hits++;
	svsize++;
	if (svsize > _SERVERSIZE)
		bigger_hash_table(&_SERVERSIZE, serverTable, 0);
	return 0;
}

/*
 * del_from_client_hash_table
 */
int	del_from_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg	aClient	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = cptr->hashv;
	hashv %= _HASHSIZE;
	for (tmp = (aClient *)clientTable[hashv].list; tmp; tmp = tmp->hnext)
	    {
		if (tmp == cptr)
		    {
			if (prev)
				prev->hnext = tmp->hnext;
			else
				clientTable[hashv].list = (void *)tmp->hnext;
			tmp->hnext = NULL;
			if (clientTable[hashv].links > 0)
			    {
				clientTable[hashv].links--;
				clsize--;
				return 1;
			    }
			else
			    {
				sendto_flag(SCH_ERROR, "cl-hash table failure");
				Debug((DEBUG_ERROR, "cl-hash table failure")); 
				/*
				 * Should never actually return from here and
				 * if we do it is an error/inconsistency in the
				 * hash table.
				 */
				return -1;
			    }
		    }
		prev = tmp;
	    }
	return 0;
}

/*
 * del_from_channel_hash_table
 */
int	del_from_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	Reg	aChannel	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = chptr->hashv;
	hashv %= _CHANNELHASHSIZE;
	for (tmp = (aChannel *)channelTable[hashv].list; tmp;
	     tmp = tmp->hnextch)
	    {
		if (tmp == chptr)
		    {
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv].list=(void *)tmp->hnextch;
			tmp->hnextch = NULL;
			if (channelTable[hashv].links > 0)
			    {
				channelTable[hashv].links--;
				chsize--;
				return 1;
			    }
			else
			    {
                                sendto_flag(SCH_ERROR, "ch-hash table failure");
				return -1;
			    }
		    }
		prev = tmp;
	    }
	return 0;
}


/*
 * del_from_server_hash_table
 */
int	del_from_server_hash_table(sptr, cptr)
aServer	*sptr;
aClient	*cptr;
{
	Reg	aServer	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = sptr->stok * 15053;
	hashv %= _SERVERSIZE;
	for (tmp = (aServer *)serverTable[hashv].list; tmp; tmp = tmp->shnext)
	    {
		if (tmp == sptr)
		    {
			if (prev)
				prev->shnext = tmp->shnext;
			else
				serverTable[hashv].list = (void *)tmp->shnext;
			tmp->shnext = NULL;
			if (serverTable[hashv].links > 0)
			    {
				serverTable[hashv].links--;
				svsize--;
				return 1;
			    }
			else
			    {
                                sendto_flag(SCH_ERROR, "se-hash table failure");
				return -1;
			    }
		    }
		prev = tmp;
	    }
	return 0;
}


/*
 * hash_find_client
 */
aClient	*hash_find_client(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg	aClient	*tmp;
	Reg	aClient	*prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;

	hashv = hash_nick_name(name, &hv);
	tmp3 = &clientTable[hashv];

	/*
	 * Got the bucket, now search the chain.
	 */
	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
		if (hv == tmp->hashv && mycmp(name, tmp->name) == 0)
		    {
			clhits++;
			/*
			 * If the member of the hashtable we found isnt at
			 * the top of its chain, put it there.  This builds
			 * a most-frequently used order into the chains of
			 * the hash table, giving speadier lookups on those
			 * nicks which are being used currently.  This same
			 * block of code is also used for channels and
			 * servers for the same performance reasons.
			 */
			if (prv)
			    {
				aClient *tmp2;

				tmp2 = (aClient *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnext = tmp->hnext;
				tmp->hnext = tmp2;
			    }
			return (tmp);
		    }
	clmiss++;
	return (cptr);
}

/*
 * hash_find_server
 */
aClient	*hash_find_server(server, cptr)
char	*server;
aClient *cptr;
{
	Reg	aClient	*tmp, *prv = NULL;
	Reg	char	*t;
	Reg	char	ch;
	aHashEntry	*tmp3;
	u_int	hashv, hv;

	hashv = hash_nick_name(server, &hv);
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	    {
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		if (hv == tmp->hashv && mycmp(server, tmp->name) == 0)
		    {
			clhits++;
			if (prv)
			    {
				aClient *tmp2;

				tmp2 = (aClient *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnext = tmp->hnext;
				tmp->hnext = tmp2;
			    }
			return (tmp);
		    }
	    }
	t = ((char *)server + strlen(server));
	/*
	 * Whats happening in this next loop ? Well, it takes a name like
	 * foo.bar.edu and proceeds to earch for *.edu and then *.bar.edu.
	 * This is for checking full server names against masks although
	 * it isnt often done this way in lieu of using match().
	 */
	for (;;)
	    {
		t--;
		for (; t >= server; t--)
			if (*(t+1) == '.')
				break;
		if (t < server || *t == '*')
			break;
		ch = *t;
		*t = '*';
		/*
	 	 * Dont need to check IsServer() here since nicknames cant
		 * have *'s in them anyway.
		 */
		if (((tmp = hash_find_client(t, cptr))) != cptr)
		    {
			*t = ch;
			return (tmp);
		    }
		*t = ch;
	    }
	clmiss++;
	return (cptr);
}

/*
 * hash_find_channel
 *
 * If the name doesn't begin with #, or & or -, then we're looking for
 * -????name instead of a real match.
 */
aChannel	*hash_find_channel(name, chptr)
char	*name;
aChannel *chptr;
{
	Reg	aChannel	*tmp, *prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv, exact;

	hashv = hash_channel_name(name, &hv);
	tmp3 = &channelTable[hashv];

	if (IsChannelName(name))
		exact = 1;
	else
		exact = 0;
	for (tmp = (aChannel *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnextch)
		if (hv == tmp->hashv &&
		    ((exact == 1 && mycmp(name, tmp->chname) == 0) ||
		     (exact == 0 && *tmp->chname == '!' &&
		      mycmp(name, tmp->chname + CHIDLEN + 1) == 0)))
		    {
			chhits++;
			if (prv)
			    {
				register aChannel *tmp2;

				tmp2 = (aChannel *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnextch = tmp->hnextch;
				tmp->hnextch = tmp2;
			    }
			return (tmp);
		    }
	chmiss++;
	return chptr;
}

/*
 * hash_find_stoken
 */
aServer	*hash_find_stoken(tok, cptr, dummy)
int	tok;
aClient *cptr;
void	*dummy;
{
	Reg	aServer	*tmp, *prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;

	hv = hashv = tok * 15053;
	hashv %= _SERVERSIZE;
	tmp3 = &serverTable[hashv];

	for (tmp = (aServer *)tmp3->list; tmp; prv = tmp, tmp = tmp->shnext)
		if (tmp->stok == tok && tmp->bcptr->from == cptr)
		    {
			if (prv)
			    {
				Reg	aServer	*tmp2;

				tmp2 = (aServer *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->shnext = tmp->shnext;
				tmp->shnext = tmp2;
			    }
			return (tmp);
		    }
	return (aServer *)dummy;
}

/*
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 */

int	m_hash(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#ifdef DEBUGMODE
	register	int	l, i;
	register	aHashEntry	*tab;
	int	deepest = 0, deeplink = 0, showlist = 0, tothits = 0;
	int	mosthit = 0, mosthits = 0, used = 0, used_now = 0, totlink = 0;
	int	link_pop[10], size = _HASHSIZE;
	char	ch;
	aHashEntry	*table;

	if (parc > 1) {
		ch = *parv[1];
		if (islower(ch))
			table = clientTable;
		else {
			table = channelTable;
			size = _CHANNELHASHSIZE;
		}
		if (ch == 'L' || ch == 'l')
			showlist = 1;
	} else {
		ch = '\0';
		table = clientTable;
	}

	for (i = 0; i < 10; i++)
		link_pop[i] = 0;
	for (i = 0; i < size; i++) {
		tab = &table[i];
		l = tab->links;
		if (showlist)
		    sendto_one(sptr,
			   "NOTICE %s :Hash Entry:%6d Hits:%7d Links:%6d",
			   parv[0], i, tab->hits, l);
		if (l > 0) {
			if (l < 10)
				link_pop[l]++;
			else
				link_pop[9]++;
			used_now++;
			totlink += l;
			if (l > deepest) {
				deepest = l;
				deeplink = i;
			}
		}
		else
			link_pop[0]++;
		l = tab->hits;
		if (l) {
			used++;
			tothits += l;
			if (l > mosthits) {
				mosthits = l;
				mosthit = i;
			}
		}
	}
	switch((int)ch)
	{
	case 'V' : case 'v' :
	    {
		register	aClient	*acptr;
		int	bad = 0, listlength = 0;

		for (acptr = client; acptr; acptr = acptr->next) {
			if (hash_find_client(acptr->name,acptr) != acptr) {
				if (ch == 'V')
				sendto_one(sptr, "NOTICE %s :Bad hash for %s",
					   parv[0], acptr->name);
				bad++;
			}
			listlength++;
		}
		sendto_one(sptr,"NOTICE %s :List Length: %d Bad Hashes: %d",
			   parv[0], listlength, bad);
	    }
	case 'P' : case 'p' :
		for (i = 0; i < 10; i++)
			sendto_one(sptr,"NOTICE %s :Entires with %d links : %d",
			parv[0], i, link_pop[i]);
		return (2);
	case 'r' :
	    {
		Reg	aClient	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
		clear_client_hash_table(_HASHSIZE);
		for (acptr = client; acptr; acptr = acptr->next)
			(void)add_to_client_hash_table(acptr->name, acptr);
		break;
	    }
	case 'R' :
	    {
		Reg	aChannel	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Channel List.", parv[0]);
		clear_channel_hash_table(_CHANNELHASHSIZE);
		for (acptr = channel; acptr; acptr = acptr->nextch)
			(void)add_to_channel_hash_table(acptr->chname, acptr);
		break;
	    }
	case 'H' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   hash_channel_name(parv[2]));
		return (2);
	case 'h' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
				   hash_nick_name(parv[2]));
		return (2);
	case 'n' :
	    {
		aClient	*tmp;
		int	max;

		if (parc <= 2 || !IsAnOper(sptr))
			return (1);
		l = atoi(parv[2]) % _HASHSIZE;
		if (parc > 3)
			max = atoi(parv[3]) % _HASHSIZE;
		else
			max = l;
		for (;l <= max; l++)
			for (i = 0, tmp = (aClient *)clientTable[l].list; tmp;
			     i++, tmp = tmp->hnext)
			    {
				if (parv[1][2] == '1' && tmp != tmp->from)
					continue;
				sendto_one(sptr,"NOTICE %s :Node: %d #%d %s",
					   parv[0], l, i, tmp->name);
			    }
		return (2);
	    }
	case 'N' :
	    {
		aChannel *tmp;
		int	max;

		if (parc <= 2 || !IsAnOper(sptr))
			return (1);
		l = atoi(parv[2]) % _CHANNELHASHSIZE;
		if (parc > 3)
			max = atoi(parv[3]) % _CHANNELHASHSIZE;
		else
			max = l;
		for (;l <= max; l++)
			for (i = 0, tmp = (aChannel *)channelTable[l].list; tmp;
			     i++, tmp = tmp->hnextch)
				sendto_one(sptr,"NOTICE %s :Node: %d #%d %s",
					   parv[0], l, i, tmp->chname);
		return (2);
	    }
	case 'S' :
#else
	if (parc>1&&!strcmp(parv[1],"sums")){
#endif
	sendto_one(sptr, "NOTICE %s :[SBSDC] [SUSER]", parv[0]);
	sendto_one(sptr, "NOTICE %s :[SSERV] [IRCDC]", parv[0]);
	sendto_one(sptr, "NOTICE %s :[CHANC] [SMISC]", parv[0]);
	sendto_one(sptr, "NOTICE %s :[HASHC] [VERSH]", parv[0]);
	sendto_one(sptr, "NOTICE %s :[MAKEF] HOSTID", parv[0]);
#ifndef	DEBUGMODE
	}
#endif
	    return 2;
#ifdef	DEBUGMODE
	case 'z' :
	    {
		if (parc <= 2 || !IsAnOper(sptr))
			return 1;
		l = atoi(parv[2]);
		if (l < 256)
			return 1;
		bigger_hash_table(&_HASHSIZE, clientTable, l);
		sendto_one(sptr, "NOTICE %s :HASHSIZE now %d", parv[0], l);
		break;
	    }
	case 'Z' :
	    {
		if (parc <= 2 || !IsAnOper(sptr))
			return 1;
		l = atoi(parv[2]);
		if (l < 256)
			return 1;
		bigger_hash_table(&_CHANNELHASHSIZE, channelTable, l);
		sendto_one(sptr, "NOTICE %s :CHANNELHASHSIZE now %d",
			   parv[0], l);
		break;
	    }
	default :
		break;
	}
	sendto_one(sptr,"NOTICE %s :Entries Hashed: %d NonEmpty: %d of %d",
		   parv[0], totlink, used_now, size);
	if (!used_now)
		used_now = 1;
	sendto_one(sptr,"NOTICE %s :Hash Ratio (av. depth): %f %Full: %f",
		  parv[0], (float)((1.0 * totlink) / (1.0 * used_now)),
		  (float)((1.0 * used_now) / (1.0 * size)));
	sendto_one(sptr,"NOTICE %s :Deepest Link: %d Links: %d",
		   parv[0], deeplink, deepest);
	if (!used)
		used = 1;
	sendto_one(sptr,"NOTICE %s :Total Hits: %d Unhit: %d Av Hits: %f",
		   parv[0], tothits, size-used,
		   (float)((1.0 * (float)tothits) / (1.0 * (float)used)));
	sendto_one(sptr,"NOTICE %s :Entry Most Hit: %d Hits: %d",
		   parv[0], mosthit, mosthits);
	sendto_one(sptr,"NOTICE %s :Client hits %d miss %d",
		   parv[0], clhits, clmiss);
	sendto_one(sptr,"NOTICE %s :Channel hits %d miss %d",
		   parv[0], chhits, chmiss);
	return 2;
#endif
}


