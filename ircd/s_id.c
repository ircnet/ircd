/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_id.c
 *   Copyright (C) 1998  Christophe Kalt
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
static const volatile char rcsid[] = "@(#)$Id: s_id.c,v 1.29 2005/02/08 02:47:11 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_ID_C
#include "s_externs.h"
#undef S_ID_C

static unsigned char id_alphabet[CHIDNB+1] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static signed int alphabet_id[256] =
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
          /* 0 */ 26, /* 1 */ 27, /* 2 */ 28, /* 3 */ 29, /* 4 */ 30,
          /* 5 */ 31, /* 6 */ 32, /* 7 */ 33, /* 8 */ 34, /* 9 */ 35,
	  -1, -1, -1, -1, -1, -1, -1,
	  /* A */ 0, /* B */ 1, /* C */ 2, /* D */ 3, /* E */ 4, /* F */ 5,
	  /* G */ 6, /* H */ 7, /* I */ 8, /* J */ 9, /* K */ 10, /* L */ 11, 
	  /* M */ 12, /* N */ 13, /* O */ 14, /* P */ 15, /* Q */ 16, 
	  /* R */ 17, /* S */ 18, /* T */ 19, /* U */ 20, /* V */ 21, 
	  /* W */ 22, /* X */ 23, /* Y */ 24, /* Z */ 25,
	  -1, -1, -1, -1, -1, -1,
	  /* a */ 0, /* b */ 1, /* c */ 2, /* d */ 3, /* e */ 4, /* f */ 5,
	  /* g */ 6, /* h */ 7, /* i */ 8, /* j */ 9, /* k */ 10, /* l */ 11,
	  /* m */ 12, /* n */ 13, /* o */ 14, /* p */ 15, /* q */ 16,
	  /* r */ 17, /* s */ 18, /* t */ 19, /* u */ 20, /* v */ 21,
	  /* w */ 22, /* x */ 23, /* y */ 24, /* z */ 25, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1 };

/* 
** ltoid(long l, int n)
** base 10 -> base 36 conversion
** Converts l, to a string of n chars, zero ('A') filled. The first chars are
** dropped if it's longer.
** Returns a pointer to a static string, or NULL.
*/
char	*ltoid(long l, int n)
{
static	char	idrpl[UIDLEN+1]; /* Currently nothing longer should be used. */
	int	i = n - 1;

	if (n > sizeof(idrpl))
	{
		/* This should not happen. */
		return NULL;
	}

	idrpl[n] = '\0';
	do
	{
		idrpl[i] = id_alphabet[l % CHIDNB];
		l /= CHIDNB;
	} while (i-- > 0);

	return (char *) idrpl;
}

/*
** idtol(char *id, int n)
** base 36 -> base 10 conversion
** Converts the first n char from string id, to a number, and return that.
*/
long	idtol(char *id, int n)
{
	long	l = 0;

	if (!id)
	{
		/* Whatever. */
		return 0;
	}

	while (n-- && *id)
	{
		l = l * CHIDNB + alphabet_id[(unsigned char )*id++];
	}

	return l;
}

/* get_chid: give the current id */
char	*get_chid(void)
{
	return ltoid((long)time(NULL), CHIDLEN);
}

/* close_chid: is the ID in the close future? (written for CHIDLEN == 5) */
int	close_chid(char *id)
{
    static time_t last = 0;
    static char current;
    char *curid;

    if (timeofday - last > 900 || id[0] == current)
	{
	    last = timeofday;
	    curid = get_chid();
	    current = curid[0];
	}
    if (id_alphabet[1 + alphabet_id[(unsigned char)current]] == id[1])
	    return 1;
    if (id[0] == current &&
	idtol(id, CHIDLEN) >= (timeofday % (u_int) pow(CHIDNB, CHIDLEN)))
	    return 1;
    return 0;
}

aChannel *idcache = NULL;

/* cache_chid: add a channel to the list of cached names */
void	cache_chid(aChannel *chptr)
{
    /*
    ** caching should be limited to the minimum,
    ** for memory reasons, but most importantly for
    ** user friendly-ness.
    ** Is the logic here right, tho? 
    */
    if (chptr->history == 0 ||
	(timeofday - chptr->history) >LDELAYCHASETIMELIMIT+DELAYCHASETIMELIMIT)
	{
	    MyFree(chptr);
	    return;
	}

    chptr->nextch = idcache;
    idcache = chptr;
    istat.is_cchan++;
    istat.is_cchanmem += sizeof(aChannel) + strlen(chptr->chname);
}

/* check_chid: checks if a (short) channel name is in the cache
 *	returns: 0 if not, 1 if yes
 */
int	check_chid(char *name)
{
    aChannel *chptr = idcache;

    while (chptr)
	{
	    if (!strcasecmp(name, chptr->chname+1+CHIDLEN))
		    return 1;
	    chptr = chptr->nextch;
	}
    return 0;
}

/* collect_chid: remove expired entries from the cache */	    
void	collect_chid(void)
{
    aChannel **chptr = &idcache, *del;

    while (*chptr)
	{
	    if (close_chid((*chptr)->chname) == 0)
		{
		    del = *chptr;
		    *chptr = del->nextch;
		    istat.is_cchan--;
		    istat.is_cchanmem -= sizeof(aChannel) +strlen(del->chname);
		    MyFree(del);
		}
	    else
		    chptr = &((*chptr)->nextch);
	}
}

/* 
** checks wether the ID is valid
** It checks n chars of name to be a valid char for an ID.
** This skips the first char for now.  It's used in !channels, where the first
** char is a '!', and for UID's, where the first char should be a number.
** returns 1 when it's valid, 0 when not.
*/
int	cid_ok(char *name, int n)
{
	int	i;

	for (i = 1; i <= n; i++)
	{
		if (alphabet_id[(unsigned char)name[i]] == -1)
		{
			return 0;
		}
	}
	return 1;
}

/*
 * unique user IDs
 */

/* 
** sid_valid(char *sid)
**
** Check that the sid is a valid sid.  The first char should be a number in
** range of [0-9], the rest should be a char in the range of [0-9A-Z].
** It returns 1 if it's a valid sid, 0 if not.
*/
int	sid_valid(char *sid)
{
	if (isdigit(sid[0]) && strlen(sid) == SIDLEN)
	{
		return cid_ok(sid, SIDLEN - 1);
	}
	return 0;
}

#if 0
/* we don't need it */
static char sid[SIDLEN+1];

void	init_sid(char *conf)
{
	if (!conf || (strlen(conf) != SIDLEN) || !sid_valid(conf))
	{
		exit(0); /* ick.. -syrk */
	}
	strcpy(sid, conf);
}
#endif

char	*next_uid(void)
{
static	char	uid[UIDLEN+1+5];	/* why +5? --Beeth */
static	long	curr_cid = 0;
static	int	needfinduid = 0;

	do
	{
		sprintf(uid, "%s%s", me.serv->sid, ltoid(curr_cid, UIDLEN-SIDLEN));
		curr_cid++;

#if CHIDNB == 36 && UIDLEN == 9 && SIDLEN == 4
/* MAXCID = CHIDNB ^ (UIDLEN - SIDLEN) */
# define MAXCID 60466176
#else
# error Fix MAXCID!
#endif
		if (curr_cid > MAXCID)
		{
			/* 
			** We used all CIDs, restart from 0.
			** At a rate of 1 client per second we have 700 days before
			** reusing CID.
			** Note: after we increase UIDLEN to 12 this uptime would
			** have to be almost 90 thousands years!
			*/
			curr_cid = 0;
			needfinduid = 1;
		}
	}
	while (needfinduid && find_uid(uid, NULL) != NULL);
	return uid;
}

/*
 * check_uid
 *	various sanity checks to ensure that a UID is valid.
 * returns 1 when it's valid, 0 when not.
 */
int	check_uid(char *uid, char *sid)
{
	int retval = 0;

	if (isdigit(uid[0]) && strlen(uid) == UIDLEN)
	{
		retval = cid_ok(uid, UIDLEN - 1);
	}
	if (retval == 1)
	{
		/* compare SID part of UID with source of message --B. */
		retval = (strncasecmp(uid, sid, SIDLEN) == 0);
	}
	return retval;
}

