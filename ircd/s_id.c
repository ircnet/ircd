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
static  char rcsid[] = "@(#)$Id: s_id.c,v 1.3 1998/10/08 17:35:30 kalt Exp $";
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
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, /* 0 */ 34, /* 1 */ 25, /* 2 */ 26, 
	  /* 3 */ 27, /* 4 */ 28, /* 5 */ 29, /* 6 */ 30, /* 7 */ 31, 
	  /* 8 */ 32, /* 9 */ 33, -1, -1, -1, -1, -1, -1, -1, /* A */ 0, 
	  /* B */ 1, /* C */ 2, -1, /* E */ 3, /* F */ 4, /* G */ 5, 
	  /* H */ 6, /* I */ 7, /* J */ 8, /* K */ 9, /* L */ 10, 
	  /* M */ 11, /* N */ 12, /* O */ 13, /* P */ 14, /* Q */ 15, 
	  /* R */ 16, /* S */ 17, /* T */ 18, /* U */ 19, /* V */ 20, 
	  /* W */ 21, /* X */ 22, /* Y */ 23, /* Z */ 24, -1, -1, 
	  -1, -1, -1, -1, /* a */ 0, /* b */ 1, /* c */ 2, -1, /* e */ 3, 
	  /* f */ 4, /* g */ 5, /* h */ 6, /* i */ 7, /* j */ 8, /* k */ 9, 
	  /* l */ 10, /* m */ 11, /* n */ 12, /* o */ 13, /* p */ 14, 
	  /* q */ 15, /* r */ 16, /* s */ 17, /* t */ 18, /* u */ 19, 
	  /* v */ 20, /* w */ 21, /* x */ 22, /* y */ 23, /* z */ 24, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
	  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

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

    l = alphabet_id[*id++];
    l = l * CHIDNB + alphabet_id[*id++];
    l = l * CHIDNB + alphabet_id[*id++];
    l = l * CHIDNB + alphabet_id[*id++];
    l = l * CHIDNB + alphabet_id[*id++];
    return l;
}

/* get_chid: give the current id */
char *
get_chid()
{
    return ltoid(time(NULL));
}

/* close_chid: is the ID in the close future? */
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
	idtol(id) >= (timeofday % (CHIDNB*CHIDNB*CHIDNB*CHIDNB*CHIDNB)))
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
