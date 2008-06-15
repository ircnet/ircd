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
static const volatile char rcsid[] = "@(#)$Id: hash.c,v 1.57 2008/06/15 00:57:37 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define HASH_C
#include "s_externs.h"
#undef HASH_C

static	aHashEntry	*clientTable = NULL;
static	aHashEntry	*uidTable = NULL;
static	aHashEntry	*channelTable = NULL;
static	aHashEntry	*sidTable = NULL;
#ifdef USE_HOSTHASH
static	aHashEntry	*hostnameTable = NULL;
#endif
#ifdef USE_IPHASH
static	aHashEntry	*ipTable = NULL;
#endif
static	unsigned int	*hashtab = NULL;
static	int	clhits = 0, clmiss = 0, clsize = 0;
static	int	uidhits = 0, uidmiss = 0, uidsize = 0;
static	int	chhits = 0, chmiss = 0, chsize = 0;
static	int	sidhits = 0, sidmiss = 0, sidsize = 0;
static  int     cnhits = 0, cnmiss = 0 ,cnsize = 0;
static	int	iphits = 0, ipmiss = 0, ipsize = 0;
int	_HASHSIZE = 0;
int	_UIDSIZE = 0;
int	_CHANNELHASHSIZE = 0;
int	_SIDSIZE = 0;
int     _HOSTNAMEHASHSIZE = 0;
int	_IPHASHSIZE = 0;

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
 * The order shown above is just one instant of the server.
 */

/*
 * hash_nick_name
 *
 * this function must be *quick*.  Thus there should be no multiplication
 * or division or modulus in the inner loop.  subtraction and other bit
 * operations allowed.
 */
static	u_int	hash_nick_name(char *nname, u_int *store)
{
	Reg	u_char	*name = (u_char *)nname;
	Reg	u_char	ch;
	Reg	u_int	hash = 1;

	for (; (ch = *name); name++)
	{
		hash <<= 4;
		hash += hashtab[(int)ch];
	}
	/*
	if (hash < 0)
		hash = -hash;
	*/
	if (store)
		*store = hash;
	hash %= _HASHSIZE;
	return (hash);
}

/*
 * hash_uid
 *
 * this function must be *quick*.  Thus there should be no multiplication
 * or division or modulus in the inner loop.  subtraction and other bit
 * operations allowed.
 */
static	u_int	hash_uid(char *uid, u_int *store)
{
	Reg	u_char	ch;
	Reg	u_int	hash = 1;

	for (; (ch = *uid); uid++)
	{
		hash <<= 4;
		hash ^= hashtab[(int)ch];
	}
	if (store)
	{
		*store = hash;
	}
	hash %= _UIDSIZE;
	return (hash);
}

/*
** hash_sid
*/
static	u_int	hash_sid(char *sid, u_int *store)
{
	Reg	u_char	ch;
	Reg	u_int	hash = 1;

	for (; (ch = *sid); sid++)
	{
		hash <<= 4;
		hash += hashtab[(int)ch];
	}
	if (store)
	{
		*store = hash;
	}
	hash %= _SIDSIZE;
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
static	u_int	hash_channel_name(char *hname, u_int *store, int shortname)
{
	Reg	u_char	*name = (u_char *)hname;
	Reg	u_char	ch;
	Reg	int	i = 30;
	Reg	u_int	hash = 5;

	if (*name == '!' && shortname == 0)
		name += 1 + CHIDLEN;
	for (; (ch = *name) && --i; name++)
	{
		hash <<= 4;
		hash += hashtab[(u_int)ch] + (i << 1);
	}
	if (store)
		*store = hash;
	hash %= _CHANNELHASHSIZE;
	return (hash);
}

#ifdef USE_HOSTHASH
/*
 * hash_host_name
 */
static	u_int	hash_host_name(char *hname, u_int *store)
{

	Reg	u_char	*name = (u_char *)hname;
	Reg	u_int	hash = 0;
	
	for (; *name; name++)
	{
		hash = 31 * hash + hashtab[*name];
	}
	
	if (store)
		*store = hash;
	hash %= _HOSTNAMEHASHSIZE;
	return (hash);
}
#endif

#ifdef USE_IPHASH
/*
 * hash_host_name
 */
static	u_int	hash_ip(char *hip, u_int *store)
{

	Reg	u_char	*ip = (u_char *)hip;
	Reg	u_int	hash = 0;
	
	for (; *ip; ip++)
	{
		hash = 31 * hash + hashtab[*ip];
	}
	
	if (store)
		*store = hash;
	hash %= _IPHASHSIZE;
	return (hash);
}
#endif

/* bigger prime
 *
 * given a positive integer, return a prime number that's larger
 *
 * 13feb94 gbl
 */
static	int	bigger_prime(int size)
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
		for (trial = 3; trial <= sq ; trial += 2)
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
static	void	clear_client_hash_table(int size)
{
	_HASHSIZE = bigger_prime(size);
	clhits = 0;
	clmiss = 0;
	clsize = 0;
	if (!clientTable)
		clientTable = (aHashEntry *)MyMalloc(_HASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)clientTable, sizeof(aHashEntry) * _HASHSIZE);
	Debug((DEBUG_DEBUG, "Client Hash Table Init: %d (%d)",
		_HASHSIZE, size));
}

static	void	clear_uid_hash_table(int size)
{
	_UIDSIZE = bigger_prime(size);
	uidhits = 0;
	uidmiss = 0;
	uidsize = 0;
	if (!uidTable)
		uidTable = (aHashEntry *)MyMalloc(_UIDSIZE *
						  sizeof(aHashEntry));
	bzero((char *)uidTable, sizeof(aHashEntry) * _UIDSIZE);
	Debug((DEBUG_DEBUG, "uid Hash Table Init: %d (%d)", _UIDSIZE, size));
}

static	void	clear_channel_hash_table(int size)
{
	_CHANNELHASHSIZE = bigger_prime(size);
	chmiss = 0;
	chhits = 0;
	chsize = 0;
	if (!channelTable)
		channelTable = (aHashEntry *)MyMalloc(_CHANNELHASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)channelTable, sizeof(aHashEntry) * _CHANNELHASHSIZE);
	Debug((DEBUG_DEBUG, "Channel Hash Table Init: %d (%d)",
		_CHANNELHASHSIZE, size));
}


static	void	clear_sid_hash_table(int size)
{
	_SIDSIZE = bigger_prime(size);
	sidhits = 0;
	sidmiss = 0;
	sidsize = 0;
	if (!sidTable)
		sidTable = (aHashEntry *)MyMalloc(_SIDSIZE *
						     sizeof(aHashEntry));
	bzero((char *)sidTable, sizeof(aHashEntry) * _SIDSIZE);
	Debug((DEBUG_DEBUG, "Sid Hash Table Init: %d (%d)", _SIDSIZE, size));
}

#ifdef USE_HOSTHASH
static	void	clear_hostname_hash_table(int size)
{
	_HOSTNAMEHASHSIZE = bigger_prime(size);
	cnhits = 0;
	cnmiss = 0;
	cnsize = 0;
	if (!hostnameTable)
		hostnameTable = (aHashEntry *)MyMalloc(_HOSTNAMEHASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)hostnameTable, sizeof(aHashEntry) * _HOSTNAMEHASHSIZE);
	Debug((DEBUG_DEBUG, "Hostname Hash Table Init: %d (%d)",
		_HOSTNAMEHASHSIZE, size));
}
#endif

#ifdef USE_IPHASH
static	void	clear_ip_hash_table(int size)
{
	_IPHASHSIZE = bigger_prime(size);
	iphits = 0;
	ipmiss = 0;
	ipsize = 0;
	if (!ipTable)
		ipTable = (aHashEntry *)MyMalloc(_IPHASHSIZE *
						     sizeof(aHashEntry));
	bzero((char *)ipTable, sizeof(aHashEntry) * _IPHASHSIZE);
	Debug((DEBUG_DEBUG, "IP Hash Table Init: %d (%d)",
		_IPHASHSIZE, size));
}
#endif


void	inithashtables(void)
{
	Reg int i;

	clear_client_hash_table((_HASHSIZE) ? _HASHSIZE : HASHSIZE);
	clear_uid_hash_table((_UIDSIZE) ? _UIDSIZE : UIDSIZE);
	clear_channel_hash_table((_CHANNELHASHSIZE) ? _CHANNELHASHSIZE
                                 : CHANNELHASHSIZE);
	clear_sid_hash_table((_SIDSIZE) ? _SIDSIZE : SIDSIZE);
#ifdef USE_HOSTHASH
	clear_hostname_hash_table((_HOSTNAMEHASHSIZE) ? _HOSTNAMEHASHSIZE : 
				   HOSTNAMEHASHSIZE);
#endif
#ifdef USE_IPHASH
	clear_ip_hash_table((_IPHASHSIZE) ? _IPHASHSIZE : IPHASHSIZE);
#endif

	/*
	 * Moved multiplication out from the hashfunctions and into
	 * a pre-generated lookup table. Should save some CPU usage
	 * even on machines with a fast mathprocessor.  -- Core
	 */
	hashtab = (u_int *) MyMalloc(256 * sizeof(u_int));
	for (i = 0; i < 256; i++)
		hashtab[i] = tolower((char)i) * 109;
}

static	void	bigger_hash_table(int *size, aHashEntry *table, int new)
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
	ircd_writetune(tunefile);
	table = (aHashEntry *)MyMalloc(sizeof(*table) * new);
	bzero((char *)table, sizeof(*table) * new);

	if (otab == channelTable)
	    {
		Debug((DEBUG_ERROR, "Channel Hash Table from %d to %d (%d)",
			    osize, new, chsize));
		sendto_flag(SCH_HASH, "Channel Hash Table from %d to %d (%d)",
			osize, new, chsize);
		chmiss = 0;
		chhits = 0;
		chsize = 0;
		channelTable = table;
		for (chptr = channel; chptr; chptr = chptr->nextch)
			(void)add_to_channel_hash_table(chptr->chname, chptr);
		MyFree(otab);
	    }
	else if (otab == clientTable)
	    {
		int	i;
		aClient	*next;
		Debug((DEBUG_ERROR, "Client Hash Table from %d to %d (%d)",
			    osize, new, clsize));
		sendto_flag(SCH_HASH, "Client Hash Table from %d to %d (%d)",
			    osize, new, clsize);
		clmiss = 0;
		clhits = 0;
		clsize = 0;
		clientTable = table;

		for (i = 0; i < osize; i++)
		    {
			for (cptr = (aClient *)otab[i].list; cptr;
				cptr = next)
			    {
				next = cptr->hnext;
				(void)add_to_client_hash_table(cptr->name, 
					cptr);
			    }
		    }
		MyFree(otab);
	    }
	else if (otab == uidTable)
	    {
		Debug((DEBUG_ERROR, "uid Hash Table from %d to %d (%d)",
			    osize, new, uidsize));
		sendto_flag(SCH_HASH, "uid Hash Table from %d to %d (%d)",
			    osize, new, uidsize);
		uidmiss = 0;
		uidhits = 0;
		uidsize = 0;
		uidTable = table;
		for (cptr = client; cptr; cptr = cptr->next)
			if (cptr->user)
				cptr->user->uhnext = NULL;
		for (cptr = client; cptr; cptr = cptr->next)
			if (cptr->user && cptr->user->uid[0])
				add_to_uid_hash_table(cptr->user->uid, cptr);
		MyFree(otab);
	    }
#ifdef USE_HOSTHASH
	else if (otab == hostnameTable)
	    {
		int	i;
		anUser	*next,*user;
		Debug((DEBUG_ERROR, "Hostname Hash Table from %d to %d (%d)",
			    osize, new, clsize));
		sendto_flag(SCH_HASH, "Hostname Hash Table from %d to %d (%d)",
			    osize, new, clsize);
		cnmiss = 0;
		cnhits = 0;
		cnsize = 0;
		hostnameTable = table;

		for (i = 0; i < osize; i++)
		    {
			for (user = (anUser *)otab[i].list; user;
				user = next)
			    {
				next = user->hhnext;
				(void)add_to_hostname_hash_table(user->host,
					user);
			    }
		    }
		MyFree(otab);
	    }
#endif
	else if (otab == sidTable)
	{
		Debug((DEBUG_ERROR, "sid Hash Table from %d to %d (%d)",
			osize, new, sidsize));
		sendto_flag(SCH_HASH, "sid Hash Table from %d to %d (%d)",
			osize, new, sidsize);
		sidmiss = 0;
		sidhits = 0;
		sidsize = 0;
		sidTable = table;
		for (sptr = svrtop; sptr; sptr = sptr->nexts)
		{
			if (sptr->version & SV_UID)
			{
				(void) add_to_sid_hash_table(sptr->sid,
					sptr->bcptr);
			}
		}
		MyFree(otab);
	}
#ifdef USE_IPHASH
	else if (otab == ipTable)
	{
		int	i;
		anUser	*next,*user;
		Debug((DEBUG_ERROR, "IP Hash Table from %d to %d (%d)",
			    osize, new, clsize));
		sendto_flag(SCH_HASH, "IP Hash Table from %d to %d (%d)",
			    osize, new, clsize);
		ipmiss = 0;
		iphits = 0;
		ipsize = 0;
		ipTable = table;

		for (i = 0; i < osize; i++)
		    {
			for (user = (anUser *)otab[i].list; user;
				user = next)
			    {
				next = user->iphnext;
				(void)add_to_ip_hash_table(user->sip,
					user);
			    }
		    }
		MyFree(otab);
	}
#endif
    
	return;
}

/*
 * add_to_client_hash_table
 */
int	add_to_client_hash_table(char *name, aClient *cptr)
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
 * add_to_uid_hash_table
 */
int	add_to_uid_hash_table(char *uid, aClient *cptr)
{
	Reg	u_int	hashv;

	hashv = hash_uid(uid, &cptr->user->hashv);
	cptr->user->uhnext = (aClient *)uidTable[hashv].list;
	uidTable[hashv].list = (void *)cptr;
	uidTable[hashv].links++;
	uidTable[hashv].hits++;
	uidsize++;
	if (uidsize > _UIDSIZE)
		bigger_hash_table(&_UIDSIZE, uidTable, 0);
	return 0;
}

/*
 * add_to_channel_hash_table
 */
int	add_to_channel_hash_table(char *name, aChannel *chptr)
{
	Reg	u_int	hashv;

	hashv = hash_channel_name(name, &chptr->hashv, 0);
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
** add_to_sid_hash_table
*/
int	add_to_sid_hash_table(char *sid, aClient *cptr)
{
	Reg	u_int	hashv;

	hashv = hash_sid(sid, &cptr->serv->sidhashv);
	cptr->serv->sidhnext = (aServer *)sidTable[hashv].list;
	sidTable[hashv].list = (void *)cptr->serv;
	sidTable[hashv].links++;
	sidTable[hashv].hits++;
	sidsize++;
	if (sidsize > _SIDSIZE)
	{
		bigger_hash_table(&_SIDSIZE, sidTable, 0);
	}
	return 0;
}

#ifdef USE_HOSTHASH
/*
 * add_to_hostname_hash_table
 */
int	add_to_hostname_hash_table(char *hostname, anUser *user)
{
	Reg	u_int	hashv;

	hashv = hash_host_name(hostname, &user->hhashv);
	user->hhnext = (anUser *)hostnameTable[hashv].list;
	hostnameTable[hashv].list = (void *)user;
	hostnameTable[hashv].links++;
	hostnameTable[hashv].hits++;
	cnsize++;
	if (cnsize > _HOSTNAMEHASHSIZE)
		bigger_hash_table(&_HOSTNAMEHASHSIZE, hostnameTable, 0);
	return 0;
}
#endif

#ifdef USE_IPHASH
/*
 * add_to_ip_hash_table
 */
int	add_to_ip_hash_table(char *ip, anUser *user)
{
	Reg	u_int	hashv;

	hashv = hash_ip(ip, &user->iphashv);
	user->iphnext = (anUser *)ipTable[hashv].list;
	ipTable[hashv].list = (void *)user;
	ipTable[hashv].links++;
	ipTable[hashv].hits++;
	ipsize++;
	if (ipsize > _IPHASHSIZE)
		bigger_hash_table(&_IPHASHSIZE, ipTable, 0);
	return 0;
}
#endif

/*
 * del_from_client_hash_table
 */
int	del_from_client_hash_table(char *name, aClient *cptr)
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
 * del_from_uid_hash_table
 */
int	del_from_uid_hash_table(char *uid, aClient *cptr)
{
	Reg	aClient	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = cptr->user->hashv;
	hashv %= _UIDSIZE;
	for (tmp = (aClient *)uidTable[hashv].list; tmp;
	     tmp = tmp->user->uhnext)
	    {
		if (tmp == cptr)
		    {
			if (prev)
				prev->user->uhnext = tmp->user->uhnext;
			else
				uidTable[hashv].list=(void *)tmp->user->uhnext;
			tmp->user->uhnext = NULL;
			if (uidTable[hashv].links > 0)
			    {
				uidTable[hashv].links--;
				uidsize--;
				return 1;
			    }
			else
			    {
				sendto_flag(SCH_ERROR, "id-hash table failure");
				Debug((DEBUG_ERROR, "id-hash table failure")); 
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
int	del_from_channel_hash_table(char *name, aChannel *chptr)
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
** del_from_sid_hash_table
*/
int	del_from_sid_hash_table(aServer *sptr)
{
	Reg	aServer	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = sptr->sidhashv;
	hashv %= _SIDSIZE;
	for (tmp = (aServer *)sidTable[hashv].list; tmp; tmp = tmp->sidhnext)
	{
		if (tmp == sptr)
		{
			if (prev)
			{
				prev->sidhnext = tmp->sidhnext;
			}
			else
			{
				sidTable[hashv].list = (void *)tmp->sidhnext;
			}
			tmp->sidhnext = NULL;
			if (sidTable[hashv].links > 0)
			{
				sidTable[hashv].links--;
				sidsize--;
				return 1;
			}
			else
			{
                                sendto_flag(SCH_ERROR, "sid-hash failure");
				return -1;
			}
		}
		prev = tmp;
	}
	return 0;
}

#ifdef USE_HOSTHASH
/*
 * del_from_hostname_hash_table
 */
int	del_from_hostname_hash_table(char *hostname, anUser *user)
{
	Reg	anUser	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = user->hhashv;
	hashv %= _HOSTNAMEHASHSIZE;
	for (tmp = (anUser *)hostnameTable[hashv].list; tmp; tmp = tmp->hhnext)
	    {
		if (tmp == user)
		    {
			if (prev)
				prev->hhnext = tmp->hhnext;
			else
				hostnameTable[hashv].list = (void *)tmp->hhnext;
			tmp->hhnext = NULL;
			if (hostnameTable[hashv].links > 0)
			    {
				hostnameTable[hashv].links--;
				cnsize--;
				return 1;
			    }
			else
			    {
				sendto_flag(SCH_ERROR, "hn-hash table failure");
				Debug((DEBUG_ERROR, "hn-hash table failure")); 
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
#endif
#ifdef USE_IPHASH
/*
 * del_from_ip_hash_table
 */
int	del_from_ip_hash_table(char *ip, anUser *user)
{
	Reg	anUser	*tmp, *prev = NULL;
	Reg	u_int	hashv;

	hashv = user->iphashv;
	hashv %= _IPHASHSIZE;
	for (tmp = (anUser *)ipTable[hashv].list; tmp; tmp = tmp->iphnext)
	    {
		if (tmp == user)
		    {
			if (prev)
				prev->iphnext = tmp->iphnext;
			else
				ipTable[hashv].list = (void *)tmp->iphnext;
			tmp->iphnext = NULL;
			if (ipTable[hashv].links > 0)
			    {
				ipTable[hashv].links--;
				ipsize--;
				return 1;
			    }
			else
			    {
				sendto_flag(SCH_ERROR, "ip-hash table failure");
				Debug((DEBUG_ERROR, "ip-hash table failure")); 
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
#endif


/*
 * hash_find_client
 */
aClient	*hash_find_client(char *name, aClient *cptr)
{
	Reg	aClient	*tmp;
	Reg	aClient	*prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_nick_name(name, &hv);
	tmp3 = &clientTable[hashv];

	/*
	 * Got the bucket, now search the chain.
	 */
	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	{
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
			/* I think this is useless concern --Beeth
			if (prv)
			    {
				aClient *tmp2;

				tmp2 = (aClient *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnext = tmp->hnext;
				tmp->hnext = tmp2;
			    }
			*/
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_client possible loop");
			break;
		}
	}
	clmiss++;
	return (cptr);
}

/*
 * hash_find_uid
 */
aClient	*hash_find_uid(char *uid, aClient *cptr)
{
	Reg	aClient	*tmp;
	Reg	aClient	*prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_uid(uid, &hv);
	tmp3 = &uidTable[hashv];

	/*
	 * Got the bucket, now search the chain.
	 */
	for (tmp = (aClient *)tmp3->list; tmp;
	     prv = tmp, tmp = tmp->user->uhnext)
	{
		if (hv == tmp->user->hashv && mycmp(uid, tmp->user->uid) == 0)
		    {
			uidhits++;
			/*
			 * If the member of the hashtable we found isnt at
			 * the top of its chain, put it there.  This builds
			 * a most-frequently used order into the chains of
			 * the hash table, giving speadier lookups on those
			 * nicks which are being used currently.  This same
			 * block of code is also used for channels and
			 * servers for the same performance reasons.
			 */
#if 0
			if (prv)
			    {
				aClient *tmp2;

				tmp2 = (aClient *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->user->uhnext = tmp->user->uhnext;
				tmp->user->uhnext = tmp2;
			    }
#endif
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_uid possible loop");
			break;
		}
	}
	uidmiss++;
	return (cptr);
}

/*
 * hash_find_server
 */
aClient	*hash_find_server(char *server, aClient *cptr)
{
	Reg	aClient	*tmp, *prv = NULL;
	Reg	char	*t;
	Reg	char	ch;
	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_nick_name(server, &hv);
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	    {
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		if (hv == tmp->hashv && mycmp(server, tmp->name) == 0)
		    {
			clhits++;
			/*
			if (prv)
			    {
				aClient *tmp2;

				tmp2 = (aClient *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnext = tmp->hnext;
				tmp->hnext = tmp2;
			    }
			*/
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_server possible loop");
			break;
		}
	    }
	t = ((char *)server + strlen(server)) - 1;
	/*
	 * Whats happening in this next loop ? Well, it takes a name like
	 * foo.bar.edu and proceeds to search for *.edu and then *.bar.edu.
	 * This is for checking full server names against masks although
	 * it isn't often done this way in lieu of using match().
	 */
	for (;;)
	    {
		while (t > server && (*t != '.'))
			t--;
		if (t == server)
			break;
		t--;
		if (*t == '*')
			break;
		ch = *t;
		*t = '*';
		/*
	 	 * Don't need to check IsServer() here since nicknames can't
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
 */
aChannel	*hash_find_channel(char *name, aChannel *chptr)
{
	Reg	aChannel	*tmp, *prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_channel_name(name, &hv, 0);
	tmp3 = &channelTable[hashv];

	for (tmp = (aChannel *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnextch)
	{
		if (hv == tmp->hashv && mycmp(name, tmp->chname) == 0)
		    {
			chhits++;
			/*
			if (prv)
			    {
				register aChannel *tmp2;

				tmp2 = (aChannel *)tmp3->list;
				tmp3->list = (void *)tmp;
				prv->hnextch = tmp->hnextch;
				tmp->hnextch = tmp2;
			    }
			*/
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_channel possible loop");
			break;
		}
	}
	chmiss++;
	return chptr;
}

/*
 * hash_find_channels
 *
 * look up matches for !?????name instead of a real match.
 */
aChannel	*hash_find_channels(char *name, aChannel *chptr)
{
	aChannel	*tmp;
	u_int	hashv, hv;

	if (chptr == NULL)
	    {
		aHashEntry	*tmp3;

		hashv = hash_channel_name(name, &hv, 1);
		tmp3 = &channelTable[hashv];
		chptr = (aChannel *) tmp3->list;
	    }
	else
	    {
		hv = chptr->hashv;
		chptr = chptr->hnextch;
	    }

	if (chptr == NULL)
		return NULL;
	for (tmp = chptr; tmp; tmp = tmp->hnextch)
		if (hv == tmp->hashv && *tmp->chname == '!' &&
		    mycmp(name, tmp->chname + CHIDLEN + 1) == 0)
		    {
			chhits++;
			return (tmp);
		    }
	chmiss++;
	return NULL;
}


/*
** hash_find_sid
*/
aClient	*hash_find_sid(char *sid, aClient *cptr)
{
	Reg     aServer *tmp;
	Reg     aServer *prv = NULL;
	Reg     aHashEntry      *tmp3;
	u_int   hashv, hv;
	int	count = 0;

	hashv = hash_sid(sid, &hv);
	tmp3 = &sidTable[hashv];

	for (tmp = (aServer *)tmp3->list; tmp; prv = tmp, tmp = tmp->sidhnext)
	{
		if (hv == tmp->sidhashv && mycmp(sid, tmp->sid) == 0)
		{
			sidhits++;
			return (tmp->bcptr);
		}
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_sid possible loop");
			break;
		}
	}
	sidmiss++;
	return (cptr);
}

#ifdef USE_HOSTHASH
/*
 * hash_find_hostname
 */
anUser	*hash_find_hostname(char *hostname, anUser *user)
{
	Reg	anUser	*tmp, *prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_host_name(hostname, &hv);
	tmp3 = &hostnameTable[hashv];

	for (tmp = (anUser *)tmp3->list; tmp; prv = tmp, tmp = tmp->hhnext)
	{
		if (hv == tmp->hhashv && !mycmp(hostname, tmp->host))
		    {
			cnhits++;
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_hostname possible loop");
			break;
		}
	}
	cnmiss++;
	return user;
}
#endif

#ifdef USE_IPHASH
/*
 * hash_find_ip
 */
anUser	*hash_find_ip(char *ip, anUser *user)
{
	Reg	anUser	*tmp, *prv = NULL;
	Reg	aHashEntry	*tmp3;
	u_int	hashv, hv;
	int	count = 0;

	hashv = hash_ip(ip, &hv);
	tmp3 = &ipTable[hashv];

	for (tmp = (anUser *)tmp3->list; tmp; prv = tmp, tmp = tmp->iphnext)
	{
		if (hv == tmp->iphashv && !mycmp(ip, tmp->sip))
		    {
			iphits++;
			return (tmp);
		    }
		if (count++ > 21142)
		{
			sendto_flag(SCH_ERROR, "hash_find_ip possible loop");
			break;
		}
	}
	ipmiss++;
	return user;
}
#endif


/*
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 */
struct HashTable_s
{
	char hash;
	char *hashname;
	aHashEntry **table;
	int *hits;
	int *miss;
	int *nentries;
	int *size;
	u_int (*hashfunc)(char *name, u_int *store);
};

#if defined(DEBUGMODE) || defined(HASHDEBUG)
static	void	show_hash_bucket(aClient *sptr, struct HashTable_s *HashTables,
	int shash, int bucket)
{
	int j = 1;
	aHashEntry *htab, *tab;
	aClient *acptr;
	anUser *auptr;
	aServer *asptr;
	aChannel *chptr;
	
	htab = *(HashTables[shash].table);
	tab = &htab[bucket];
	
	/* Nothing in bucket, skip */
	if (!tab->links)
	{
		sendto_one(sptr, ":%s NOTICE %s :Hash bucket %d is empty",
				ME, sptr->name, bucket);
		return;
	}
	if (htab == clientTable)
	{
		acptr = (aClient *) tab->list;
		while (acptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s",
			 	ME, sptr->name, bucket, j, acptr->name);	
	
			j++;
			acptr = acptr->hnext;
		}
	}
	else if (htab == uidTable)
	{
		acptr = (aClient *) tab->list;
		while (acptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s (%s)",
			 	ME, sptr->name, bucket, j, acptr->user->uid,
			 	acptr->name);
			j++;
			acptr = acptr->user->uhnext;
		}
	}
	else if (htab == channelTable)
	{
		chptr = (aChannel *) tab->list;
		while (chptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s",
			 	ME, sptr->name, bucket, j, chptr->chname);
			j++;
			chptr = chptr->hnextch;
		}

	}
	else if (htab == sidTable)
	{
		asptr = (aServer *) tab->list;
		while (asptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s (%s)",
			 	ME, sptr->name, bucket, j, asptr->sid,
			 	asptr->bcptr->name);
			j++;
			asptr = asptr->sidhnext;
		}
	
	}
#ifdef USE_HOSTHASH
	else if (htab == hostnameTable)
	{
		auptr = (anUser *) tab->list;
		while (auptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s (%s)",
			 	ME, sptr->name, bucket, j, auptr->host,
				auptr->bcptr->name);	
	
			j++;
			auptr = auptr->hhnext;
		}
	
	}
#endif
#ifdef USE_IPHASH
	else if (htab == ipTable)
	{
		auptr = (anUser *) tab->list;
		while (auptr)
		{
			sendto_one(sptr,
			 	":%s NOTICE %s :Bucket %d entry %d - %s (%s)",
			 	ME, sptr->name, bucket, j, auptr->sip,
				auptr->bcptr->name);	
	
			j++;
			auptr = auptr->iphnext;
		}
	}
#endif

	return;
}
#endif /* DEBUGMODE || HASHDEBUG */

int	m_hash(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aHashEntry *hashtab = NULL, *tab;
	int shash = -1, i, l;
	int deepest = 0 , deeplink = 0, totlink = 0, mosthits = 0, mosthit = 0;
	int tothits = 0, used = 0, used_now = 0, link_pop[11];
	
	struct HashTable_s HashTables[] =
	{
		{'c', "client", &clientTable, &clhits, &clmiss, &clsize,
			&_HASHSIZE, hash_nick_name},
		{'u', "UID", &uidTable, &uidhits, &uidmiss, &uidsize, &_UIDSIZE,
			hash_uid},
		{'C', "channel", &channelTable, &chhits, &chmiss, &sidsize,
			&_CHANNELHASHSIZE, NULL},
		{'S', "SID", &sidTable, &sidhits, &sidmiss, &sidsize, &_SIDSIZE,
			hash_sid },
#ifdef USE_HOSTHASH
		{'h', "hostname", &hostnameTable, &cnhits, &cnmiss, &cnsize,
			&_HOSTNAMEHASHSIZE, hash_host_name},
#endif
#ifdef USE_IPHASH
		{'i', "ip", &ipTable, &iphits, &ipmiss, &ipsize,
			&_IPHASHSIZE, hash_ip},
#endif
		{0, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
	};

	if (!is_allowed(sptr, ACL_HAZH))
		return m_nopriv(cptr, sptr, parc, parv);
	
	if (parc < 2)
	{
		sendto_one(sptr, ":%s NOTICE %s: Syntax: HAZH <hash> [command]"
				,ME, sptr->name);
		i = 0;
		while (HashTables[i].hash)
		{
			sendto_one(sptr, ":%s NOTICE %s:  %c - %s hash ",
				ME, sptr->name, HashTables[i].hash,
				HashTables[i].hashname);
			i++;
		}
		sendto_one(sptr, ":%s NOTICE %s: Commands: ",
				ME, sptr->name);
		sendto_one(sptr, ":%s NOTICE %s: hash <string> - hashes given "
				"string with appropriate hashing function",
				ME, sptr->name);
#if defined(DEBUGMODE) || defined(HASHDEBUG)
		sendto_one(sptr, ":%s NOTICE %s: show <number> - dumps bucket"
				" of given hash", ME, sptr->name);
		sendto_one(sptr, ":%s NOTICE %s: list [number] - dumps entire"
				" hash, displays buckets with more nodes than"
				" number if given", ME, sptr->name);
		
#endif

		return 2;
	}

	/* select hash */
	for (i = 0; HashTables[i].hashname != NULL; i++)
	{
		if (parv[1][0] == HashTables[i].hash)
		{
			shash = i;
			hashtab = *(HashTables[i].table);
			break;
		}
	}
	
	if (shash == -1)
	{
		return 1;
	}
#if defined(DEBUGMODE) || defined(HASHDEBUG)
	if (parc > 2)
	{
		/* dumps whole hash, a bit suicidal (check for sendq!) */
		if (!strcasecmp(parv[2], "list"))
		{
			int j = 1;
			/* Display buckets with more than j members */
			if (parc > 3)
			{
				j = atoi(parv[3]);
			}
			for (i = 0; i < *(HashTables[shash].size); i++)
			{
				if ((&hashtab[i])->links < j)
				{
					continue;
				}
				show_hash_bucket(sptr, HashTables, shash, i);
			}
			return 1;
		}
	}
#endif	
	if (parc > 3)
	{
#if defined(DEBUGMODE) || defined(HASHDEBUG)
		/* Print hash entry */
		if (!strcasecmp(parv[2], "show") && (i = atoi(parv[3])))
		{
			show_hash_bucket(sptr, HashTables, shash, i);
			return 1;
		}
#endif
		/* Hash given string with appropriate hashing function */
		if (!strcasecmp(parv[2], "hash"))
		{
			int hval;
			
			/* Server hash doesn't have hashfunc
			 * Also make an exception for channel hashfunc
			 * which takes 3 params
			 */
			if (!HashTables[shash].hashfunc &&
				(HashTables[shash].hash != 'C'))
			{
				sendto_one(sptr, ":%s NOTICE %s :Hash function"
						" unavailable", ME, parv[0]);
				return 2;
			}
			if (HashTables[shash].hash != 'C')
			{
				hval = HashTables[shash].hashfunc(parv[3],
								  NULL);
			}
			else
			{
				hval = hash_channel_name(parv[3], NULL, 0);
			}
			sendto_one(sptr, ":%s NOTICE %s :Hash value of %s"
				" using %s hash function is %d ", ME, parv[0],
				parv[3], HashTables[shash].hashname, hval);
			return 1;
		}
	}

	/* Create hash statistics */
	memset(link_pop, 0, sizeof(link_pop));
	for (i = 0; i < *(HashTables[shash].size); i++)
	{
		tab = &hashtab[i];
		l = tab->links;
		/* How populated buckets are... */
		if (l > 0)
		{
			if (l < 10)
			{
				link_pop[l]++;
			}
			else
			{
				link_pop[10]++;
			}
			used_now++;
			totlink += l;
			if (l > deepest)
			{
				deepest = l;
				deeplink = i;
			}
		}
		else
		{
			link_pop[0]++;
		}
		
		l = tab->hits;
		if (l)
		{
			used++;
			tothits += l;
			if (l > mosthits)
			{
				mosthits = l;
				mosthit = i;
			}
		}
	}
	
	sendto_one(sptr,"NOTICE %s :%s hash statistics",
			parv[0], HashTables[shash].hashname);
	sendto_one(sptr,"NOTICE %s :---------------------------------------",
			parv[0]);
        sendto_one(sptr,"NOTICE %s :Entries Hashed: %d NonEmpty: %d of %d",
                   parv[0], totlink, used_now, *(HashTables[shash].size));
        sendto_one(sptr,"NOTICE %s :Hash Ratio (av. depth): %f %%Full: %f",
                  parv[0], (float)((1.0 * totlink) / (1.0 * used_now)),
                  (float)((1.0 * used_now) / (1.0 * *(HashTables[shash].size))));
        sendto_one(sptr,"NOTICE %s :Deepest Link: %d Links: %d",
                   parv[0], deeplink, deepest);
        sendto_one(sptr,"NOTICE %s :Total Hits: %d Unhit: %d Av Hits: %f",
                   parv[0], tothits, *(HashTables[shash].size) - used,
                   (float)((1.0 * (float)tothits) / (1.0 * (float)used)));
        sendto_one(sptr,"NOTICE %s :Entry Most Hit: %d Hits: %d",
                   parv[0], mosthit, mosthits);
        sendto_one(sptr,"NOTICE %s :hits %d miss %d entries %d size %d",
                   parv[0], *(HashTables[shash].hits),
			    *(HashTables[shash].miss),
			    *(HashTables[shash].nentries),
			    *(HashTables[shash].size));

	for (i = 0; i < 10; i++)
	{
	        sendto_one(sptr,"NOTICE %s :Nodes with %d entries: %d",
                   parv[0],i,link_pop[i]);
	
	}
	        sendto_one(sptr,"NOTICE %s :Nodes with >10 entries: %d",
                   parv[0],link_pop[10]);

        return 1;

}

