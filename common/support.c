/************************************************************************
 *   IRC - Internet Relay Chat, common/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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
static  char rcsid[] = "@(#)$Id: support.c,v 1.2 1997/04/14 15:04:07 kalt Exp $";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "patchlevel.h"
#include <signal.h>

extern	int errno; /* ...seems that errno.h doesn't define this everywhere */
#ifndef	CLIENT_COMPILE
extern	void	outofmemory();
#endif

char	*mystrdup(s)
char	*s;
{
	/* Portable strdup(), contributed by mrg, thanks!  -roy */

	char	*t;

	t = (char *) MyMalloc(strlen(s) + 1);
	if (t)
		return ((char *)strcpy(t, s));
	return NULL;
}

#ifdef NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
**
**	$Id: support.c,v 1.2 1997/04/14 15:04:07 kalt Exp $
*/

char *strtoken(save, str, fs)
char **save;
char *str, *fs;
{
    char *pos = *save;	/* keep last position across calls */
    Reg char *tmp;

    if (str)
	pos = str;		/* new string scan */

    while (pos && *pos && index(fs, *pos) != NULL)
	pos++; 		 	/* skip leading separators */

    if (!pos || !*pos)
	return (pos = *save = NULL); 	/* string contains only sep's */

    tmp = pos; 			/* now, keep position of the token */

    while (*pos && index(fs, *pos) == NULL)
	pos++; 			/* skip content of the token */

    if (*pos)
	*pos++ = '\0';		/* remove first sep after the token */
    else
	pos = NULL;		/* end of string */

    *save = pos;
    return(tmp);
}
#endif /* NEED_STRTOKEN */

#ifdef	NEED_STRTOK
/*
** NOT encouraged to use!
*/

char *strtok(str, fs)
char *str, *fs;
{
    static char *pos;

    return strtoken(&pos, str, fs);
}

#endif /* NEED_STRTOK */

#ifdef NEED_STRERROR
/*
**	strerror - return an appropriate system error string to a given errno
**
**		   argv 11/90
**	$Id: support.c,v 1.2 1997/04/14 15:04:07 kalt Exp $
*/

char *strerror(err_no)
int err_no;
{
#ifndef SYS_ERRLIST_DECLARED
	extern	char	*sys_errlist[];	 /* Sigh... hopefully on all systems */
	extern	int	sys_nerr;
#endif

	static	char	buff[40];
	char	*errp;

	errp = (err_no > sys_nerr ? (char *)NULL : sys_errlist[err_no]);

	if (errp == (char *)NULL)
	    {
		errp = buff;
		SPRINTF(errp, "Unknown Error %d", err_no);
	    }
	return errp;
}

#endif /* NEED_STRERROR */

/*
**	inetntoa  --	changed name to remove collision possibility and
**			so behaviour is gaurunteed to take a pointer arg.
**			-avalon 23/11/92
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX don't have this)
**			argv 11/90).
**	inet_ntoa --	its broken on some Ultrix/Dynix too. -avalon
**	$Id: support.c,v 1.2 1997/04/14 15:04:07 kalt Exp $
*/

char	*inetntoa(in)
char	*in;
{
	static	char	buf[16];
	Reg	u_char	*s = (u_char *)in;
	Reg	int	a,b,c,d;

	a = (int)*s++;
	b = (int)*s++;
	c = (int)*s++;
	d = (int)*s++;
	(void)sprintf(buf, "%d.%d.%d.%d", a,b,c,d );

	return buf;
}

#ifdef NEED_INET_NETOF
/*
**	inet_netof --	return the net portion of an internet number
**			argv 11/90
**	$Id: support.c,v 1.2 1997/04/14 15:04:07 kalt Exp $
**
*/

int inet_netof(in)
struct in_addr in;
{
    int addr = in.s_net;

    if (addr & 0x80 == 0)
	return ((int) in.s_net);

    if (addr & 0x40 == 0)
	return ((int) in.s_net * 256 + in.s_host);

    return ((int) in.s_net * 256 + in.s_host * 256 + in.s_lh);
}
#endif /* NEED_INET_NETOF */


#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE)
void	dumpcore(msg, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
char	*msg, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
	static	time_t	lastd = 0;
	static	int	dumps = 0;
	char	corename[12];
	time_t	now;
	int	p;

	now = time(NULL);

	if (!lastd)
		lastd = now;
	else if (now - lastd < 60 && dumps > 2)
		(void)s_die();
	if (now - lastd > 60)
	    {
		lastd = now;
		dumps = 1;
	    }
	else
		dumps++;
	p = getpid();
	if (fork()>0) {
		kill(p, 3);
		kill(p, 9);
	}
	write_pidfile();
	SPRINTF(corename, "core.%d", p);
	(void)rename("core", corename);
	Debug((DEBUG_FATAL, "Dumped core : core.%d", p));
	sendto_flag(SCH_ERROR, "Dumped core : core.%d", p);
	Debug((DEBUG_FATAL, msg, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,p11));
	sendto_flag(SCH_ERROR, msg, p1, p2, p3, p4, p5, p6, p7, p8,p9,p10,p11);
	(void)s_die();
}
#endif

#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE) && defined(DO_DEBUG_MALLOC)

static	char	*marray[100000];
static	int	mindex = 0;

#define	SZ_EX	(sizeof(char *) + sizeof(size_t) + 4)
#define	SZ_CHST	(sizeof(char *) + sizeof(size_t))
#define	SZ_CH	(sizeof(char *))
#define	SZ_ST	(sizeof(size_t))

char	*MyMalloc(x)
size_t	x;
{
	register int	i;
	register char	**s;
	char	*ret;

	ret = (char *)malloc(x + (size_t)SZ_EX);

	if (!ret)
	    {
# ifndef	CLIENT_COMPILE
		outofmemory();
# else
		perror("malloc");
		exit(-1);
# endif
	    }
	bzero(ret, (int)x + SZ_EX);
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&x, ret + SZ_ST, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)x, 4);
	Debug((DEBUG_MALLOC, "MyMalloc(%ld) = %#x", x, ret + SZ_CHST));
	for(i = 0, s = marray; *s && i < mindex; i++, s++)
		;
 	if (i < 100000)
	    {
		*s = ret;
		if (i == mindex)
			mindex++;
	    }
	return ret + SZ_CHST;
    }

char    *MyRealloc(x, y)
char	*x;
size_t	y;
    {
	register int	l;
	register char	**s;
	char	*ret, *cp;
	size_t	i;
	int	k;

	if (x != NULL)
	  {
	      x -= SZ_CHST;
	      bcopy(x, (char *)&cp, SZ_CH);
	      bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	      bcopy(x + (int)i + SZ_CHST, (char *)&k, 4);
	      if (bcmp((char *)&k, "VAVA", 4) || (x != cp))
		      dumpcore("MyRealloc %#x %d %d %#x %#x", x, y, i, cp, k);
	  }
	ret = (char *)realloc(x, y + (size_t)SZ_EX);

	if (!ret)
	    {
# ifndef	CLIENT_COMPILE
		outofmemory();
# else
		perror("realloc");
		exit(-1);
# endif
	    }
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&y, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)y, 4);
	Debug((DEBUG_NOTICE, "MyRealloc(%#x,%ld) = %#x", x, y, ret + SZ_CHST));
	for(l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
 	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
	for(l = 0, s = marray; *s && l < mindex; l++,s++)
		;
 	if (l < 100000)
	    {
		*s = ret;
		if (l == mindex)
			mindex++;
	    }
	return ret + SZ_CHST;
    }

void	MyFree(x)
char	*x;
{
	size_t	i;
	char	*j;
	u_char	k[4];
	register int	l;
	register char	**s;

	if (!x)
		return;
	x -= SZ_CHST;

	bcopy(x, (char *)&j, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + SZ_CHST + (int)i, (char *)k, 4);

	if (bcmp((char *)k, "VAVA", 4) || (j != x))
		dumpcore("MyFree %#x %ld %#x %#x", x, i, j,
			 (k[3]<<24) | (k[2]<<16) | (k[1]<<8) | k[0]);

#undef	free
	(void)free(x);
#define	free(x)	MyFree(x)
	Debug((DEBUG_MALLOC, "MyFree(%#x)",x + SZ_CHST));

	for (l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
}
#else
char	*MyMalloc(x)
size_t	x;
{
	char *ret = (char *)malloc(x);

	if (!ret)
	    {
# ifndef	CLIENT_COMPILE
		outofmemory();
# else
		perror("malloc");
		exit(-1);
# endif
	    }
	return	ret;
}

char	*MyRealloc(x, y)
char	*x;
size_t	y;
    {
	char *ret = (char *)realloc(x, y);

	if (!ret)
	    {
# ifndef CLIENT_COMPILE
		outofmemory();
# else
		perror("realloc");
		exit(-1);
# endif
	    }
	return ret;
    }
#endif


/*
** read a string terminated by \r or \n in from a fd
**
** Created: Sat Dec 12 06:29:58 EST 1992 by avalon
** Returns:
**	0 - EOF
**	-1 - error on read
**     >0 - number of bytes returned (<=num)
** After opening a fd, it is necessary to init dgets() by calling it as
**	dgets(x,y,0);
** to mark the buffer as being empty.
*/
int	dgets(fd, buf, num)
int	fd, num;
char	*buf;
{
	static	char	dgbuf[8192];
	static	char	*head = dgbuf, *tail = dgbuf;
	register char	*s, *t;
	register int	n, nr;

	/*
	** Sanity checks.
	*/
	if (head == tail)
		*head = '\0';
	if (!num)
	    {
		head = tail = dgbuf;
		*head = '\0';
		return 0;
	    }
	if (num > sizeof(dgbuf) - 1)
		num = sizeof(dgbuf) - 1;
dgetsagain:
	if (head > dgbuf)
	    {
		for (nr = tail - head, s = head, t = dgbuf; nr > 0; nr--)
			*t++ = *s++;
		tail = t;
		head = dgbuf;
	    }
	/*
	** check input buffer for EOL and if present return string.
	*/
	if (head < tail &&
	    ((s = index(head, '\n')) || (s = index(head, '\r'))) && s < tail)
	    {
		n = MIN(s - head + 1, num);	/* at least 1 byte */
dgetsreturnbuf:
		bcopy(head, buf, n);
		head += n;
		if (head == tail)
			head = tail = dgbuf;
		return n;
	    }

	if (tail - head >= num)		/* dgets buf is big enough */
	    {
		n = num;
		goto dgetsreturnbuf;
	    }

	n = sizeof(dgbuf) - (tail - dgbuf) - 1;
	nr = read(fd, tail, n);
	if (nr == -1)
	    {
		head = tail = dgbuf;
		return -1;
	    }
	if (!nr)
	    {
		if (tail > head)
		    {
			n = MIN(tail - head, num);
			goto dgetsreturnbuf;
		    }
		head = tail = dgbuf;
		return 0;
	    }
	tail += nr;
	*tail = '\0';
	for (t = head; (s = index(t, '\n')); )
	    {
		if ((s > head) && (s > dgbuf))
		    {
			t = s-1;
			for (nr = 0; *t == '\\'; nr++)
				t--;
			if (nr & 1)
			    {
				t = s+1;
				s--;
				nr = tail - t;
				while (nr--)
					*s++ = *t++;
				tail -= 2;
				*tail = '\0';
			    }
			else
				s++;
		    }
		else
			s++;
		t = s;
	    }
	*tail = '\0';
	goto dgetsagain;
}

/*
 * By Mika
 */
int	irc_sprintf(outp, formp,
		    i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11)
char	*outp;
char	*formp;
char	*i0, *i1, *i2, *i3, *i4, *i5, *i6, *i7, *i8, *i9, *i10, *i11;
{
	/* rp for Reading, wp for Writing, fp for the Format string */
	/* we could hack this if we know the format of the stack */
	char	*inp[12];
	Reg	char	*rp, *fp, *wp, **pp = inp;
	Reg	char	f;
	Reg	long	myi;
	int	i;

	inp[0] = i0;
	inp[1] = i1;
	inp[2] = i2;
	inp[3] = i3;
	inp[4] = i4;
	inp[5] = i5;
	inp[6] = i6;
	inp[7] = i7;
	inp[8] = i8;
	inp[9] = i9;
	inp[10] = i10;
	inp[11] = i11;

	/*
	 * just scan the format string and puke out whatever is necessary
	 * along the way...
	 */

	for (i = 0, wp = outp, fp = formp; (f = *fp++); )
		if (f != '%')
			*wp++ = f;
		else
			switch (*fp++)
			{
 			/* put the most common case at the top */
			/* copy a string */
			case 's':
				for (rp = *pp++; (*wp++ = *rp++); )
					;
				--wp;
				/* get the next parameter */
				break;
			/*
			 * reject range for params to this mean that the
			 * param must be within 100-999 and this +ve int
			 */
			case 'd':
			case 'u':
				myi = (long)*pp++;
				if ((myi < 100) || (myi > 999))
				    {
					(void)sprintf(outp, formp, i0, i1, i2,
						      i3, i4, i5, i6, i7, i8,
						      i9, i10, i11);
					return -1;
				    }

				*wp++ = (char)(myi / 100 + (int) '0');
				myi %= 100;
				*wp++ = (char)(myi / 10 + (int) '0');
				myi %= 10;
				*wp++ = (char)(myi + (int) '0');
				break;
			case 'c':
				*wp++ = (char)(long)*pp++;
				break;
			case '%':
				*wp++ = '%';
				break;
			default :
				(void)sprintf(outp, formp, i0, i1, i2, i3, i4,
					      i5, i6, i7, i8, i9, i10, i11);
				return -1;
			}
	*wp = '\0';
	return wp - outp;
}

/*
 * Make 'readable' version string.
 */
char *make_version()
{
	int ve, re, pl, be, al;
	char ver[15];

	sscanf(PATCHLEVEL, "%2d%2d%2d%2d%2d", &ve, &re, &pl, &be, &al);
	sprintf(ver, "%d.%d", ve, re);	/* version & revision */
	if (pl)	/* patchlevel */
		sprintf(ver + strlen(ver), ".%d", be ? pl+1 : pl);
	if (be)	/* beta, note how visual patchlevel is raised above */
		sprintf(ver + strlen(ver), "b%d", be);
	if (al)	/* patch */
		sprintf(ver + strlen(ver), "p%d", al);
	return mystrdup(ver);
}
