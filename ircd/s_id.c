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
static  char rcsid[] = "@(#)$Id: s_id.c,v 1.9 1999/07/27 11:26:37 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_ID_C
#include "s_externs.h"
#undef S_ID_C

/*
 * channel IDs
 */
#define CHIDNB 36

static unsigned char id_alphabet[CHIDNB+1] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890A";

static unsigned int alphabet_id[256] =
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	  /* 0 */ 35, /* 1 */ 26, /* 2 */ 27, /* 3 */ 28, /* 4 */ 29,
	  /* 5 */ 30, /* 6 */ 31, /* 7 */ 32, /* 8 */ 33, /* 9 */ 34,
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

/* ltoid: base 10 -> base 36 conversion */
static char *
ltoid(l)
time_t l;
{
    static char idrpl[CHIDLEN+1];
    char c = CHIDLEN-1;

    idrpl[CHIDLEN] = '\0';
    do
	{
	    idrpl[c] = id_alphabet[1 + l % CHIDNB];
	    l /= CHIDNB;
	}
    while (c-- > 0);
    return (char *) idrpl;
}

/* idtol: base 36 -> base 10 conversion */
static unsigned long
idtol(id)
char *id;
{
    unsigned long l = 0;
    char c = CHIDLEN-1;

    l = alphabet_id[*id++];
    while (c-- > 0)
	l = l * CHIDNB + alphabet_id[*id++];
    return l;
}

/* get_chid: give the current id */
char *
get_chid()
{
    return ltoid(time(NULL));
}

/* close_chid: is the ID in the close future? (written for CHIDLEN == 5) */
int
close_chid(id)
char *id;
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
    if (id_alphabet[1 + alphabet_id[current]] == id[1])
	    return 1;
    if (id[0] == current &&
	idtol(id) >= (timeofday % (u_int) pow(CHIDNB, CHIDLEN)))
	    return 1;
    return 0;
}

aChannel *idcache = NULL;

/* cache_chid: add a channel to the list of cached names */
void
cache_chid(chptr)
aChannel *chptr;
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
	    MyFree((char *)chptr);
	    return;
	}

    chptr->nextch = idcache;
    idcache = chptr;
    istat.is_cchan++;
    istat.is_cchanmem -= sizeof(aChannel) + strlen(chptr->chname);
}

/* check_chid: checks if a (short) channel name is in the cache
 *	returns: 0 if not, 1 if yes
 */
int
check_chid(name)
char *name;
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
void
collect_chid()
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
		    MyFree((char *)del);
		}
	    else
		    chptr = &((*chptr)->nextch);
	}
}

/* checks wether the ID is valid */
int
cid_ok(name)
char *name;
{
    int l = 1;

    while (l <= CHIDLEN)
	{
	    if (alphabet_id[name[l]] == -1)
		    return 0;
	    l += 1;
	}
    if (l != CHIDLEN+1)
	    return 0;
    return 1;
}
