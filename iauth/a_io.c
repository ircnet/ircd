/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_io.c
 *   Copyright (C) 1998 Christophe Kalt
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
static  char rcsid[] = "@(#)$Id: a_io.c,v 1.22 1999/07/11 22:09:59 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define A_IO_C
#include "a_externs.h"
#undef A_IO_C

anAuthData 	cldata[MAXCONNECTIONS]; /* index == ircd fd */
static int	cl_highest = -1;
#if defined(USE_POLL)
static int	fd2cl[MAXCONNECTIONS]; /* fd -> cl mapping */
#endif

#define IOBUFSIZE 4096
static char		iobuf[IOBUFSIZE+1];
static char		rbuf[IOBUFSIZE+1];	/* incoming ircd stream */
static int		iob_len = 0, rb_len = 0;

void
init_io()
{
    bzero((char *) cldata, sizeof(cldata));
}

/* sendto_ircd() functions */
#if ! USE_STDARG
void
sendto_ircd(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
#else
void
vsendto_ircd(char *pattern, va_list va)
#endif
{
	char	ibuf[4096];

#if ! USE_STDARG
	sprintf(ibuf, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
	vsprintf(ibuf, pattern, va);
#endif
	DebugLog((ALOG_DSPY, 0, "To ircd: [%s]", ibuf));
	strcat(ibuf, "\n");
	if (write(0, ibuf, strlen(ibuf)) != strlen(ibuf))
	    {
		sendto_log(ALOG_DMISC, LOG_NOTICE, "Daemon exiting. [w %s]",
			   strerror(errno));
		exit(0);
	    }
}

#if USE_STDARG
void
sendto_ircd(char *pattern, ...)
{
        va_list va;
        va_start(va, pattern);
        vsendto_ircd(pattern, va);
        va_end(va);
}
#endif

/*
 * next_io
 *
 *	given an entry, look for the next module instance to start
 */
static void
next_io(cl, last)
int cl;
AnInstance *last;
{
    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): last=%s state=0x%X", cl, last,
	      (last) ? last->mod->name : "", cldata[cl].state));

    /* first, bail out immediately if the entry is flagged A_DONE */
    if (cldata[cl].state & A_DONE)
	    return;

    /* second, make sure the last instance which ran cleaned up */
    if (cldata[cl].rfd > 0 || cldata[cl].wfd > 0)
	{
	    /* last is defined here */
	    sendto_log(ALOG_IRCD|ALOG_DMISC, LOG_ERR,
		       "module \"%s\" didn't clean up fd's! (%d %d)",
		       last->mod->name, cldata[cl].rfd, cldata[cl].wfd);
	    if (cldata[cl].rfd > 0)
		    close(cldata[cl].rfd);
	    if (cldata[cl].wfd > 0 && cldata[cl].rfd != cldata[cl].wfd)
		    close(cldata[cl].wfd);
	    cldata[cl].rfd = cldata[cl].wfd = 0;
	}
		       
    cldata[cl].buflen = 0;
    cldata[cl].mod_status = 0;
    cldata[cl].instance = NULL;
    cldata[cl].timeout = 0;

    /* third, if A_START is set, a new pass has to be started */
    if (cldata[cl].state & A_START)
	{
	    cldata[cl].state ^= A_START;
	    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): Starting again",
		      cl, last));
	    last = NULL; /* start from beginning */
	}

    /* fourth, find next instance to be ran */
    if (last == NULL)
	{
	    cldata[cl].instance = instances;
	    cldata[cl].ileft = 0;
	}
    else
	    cldata[cl].instance = last->nexti;

    while (cldata[cl].instance)
	{
	    int cm;

	    if (CheckBit(cldata[cl].idone, cldata[cl].instance->in))
		{
		    DebugLog((ALOG_DIO, 0,
	      "conf_match(#%d, %x, goth=%d, noh=%d) skipped %x (%s)",
			      cl, last, (cldata[cl].state & A_GOTH) == A_GOTH,
			      (cldata[cl].state & A_NOH) == A_NOH,
			      cldata[cl].instance,
			      cldata[cl].instance->mod->name));
		    cldata[cl].instance = cldata[cl].instance->nexti;
		    continue;
		}
	    cm = conf_match(cl, cldata[cl].instance);
	    DebugLog((ALOG_DIO, 0,
	      "conf_match(#%d, %x, goth=%d, noh=%d) said \"%s\" for %x (%s)",
		      cl, last, (cldata[cl].state & A_GOTH) == A_GOTH,
		      (cldata[cl].state & A_NOH) == A_NOH,
		      (cm==-1) ? "no match" : (cm==0) ? "match" : "try again",
		      cldata[cl].instance, cldata[cl].instance->mod->name));
	    if (cm == 0)
		    break;
	    if (cm == -1)
		    SetBit(cldata[cl].idone, cldata[cl].instance->in);
	    else /* cm == 1 */
		    cldata[cl].ileft += 1;
	    cldata[cl].instance = cldata[cl].instance->nexti;
	}

    if (cldata[cl].instance == NULL)
	    /* fifth, when there's no instance to try.. */
	{
	    DebugLog((ALOG_DIO, 0,
		      "next_io(#%d, %x): no more instances to try (%d)",
		      cl, last, cldata[cl].ileft));
	    if (cldata[cl].ileft == 0)
		{
		    /* we are done */
		    sendto_ircd("D %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
                    cldata[cl].state |= A_DONE;
		    free(cldata[cl].inbuffer);
		    cldata[cl].inbuffer = NULL;
		}
	    return;
	}
    else
	    /* sixth, we've got an instance to try */
	{
	    int r;

	    cldata[cl].timeout = time(NULL) + cldata[cl].instance->timeout;
	    r = cldata[cl].instance->mod->start(cl);
	    DebugLog((ALOG_DIO, 0,
		      "next_io(#%d, %x): %s->start() returned %d",
		      cl, last, cldata[cl].instance->mod->name, r));
	    if (r != 1)
		    /* started, or nothing to do or failed: don't try again */
		    SetBit(cldata[cl].idone, cldata[cl].instance->in);
	    if (r == 1)
		    cldata[cl].ileft += 1;
	    if (r != 0)
		    /* start() didn't start something */
		    next_io(cl, cldata[cl].instance);
	}
}

/*
 * parse_ircd
 *
 *	parses data coming from ircd (doh ;-)
 */
static void
parse_ircd()
{
	char *ch, *chp, *buf = iobuf;
	int cl = -1, ncl;

	iobuf[iob_len] = '\0';
	while (ch = index(buf, '\n'))
	    {
		*ch = '\0';
		DebugLog((ALOG_DSPY, 0, "parse_ircd(): got [%s]", buf));

		cl = atoi(chp = buf);
		while (*chp++ != ' ');
		switch (chp[0])
		    {
		case 'C': /* new connection */
		case 'O': /* old connection: do nothing, just update data */
			if (cldata[cl].state & A_ACTIVE)
			    {
				/* this is not supposed to happen!!! */
                                sendto_log(ALOG_IRCD, LOG_CRIT,
			   "Entry %d [%c] is already active (fatal)!",
					   cl, chp[0]);
				exit(1);
			    }
			if (cldata[cl].instance || cldata[cl].rfd > 0 ||
			    cldata[cl].wfd > 0)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d [%c] is already active! (fatal)",
					   cl, chp[0]);
				exit(1);
			    }
			if (cldata[cl].authuser)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased data [%c %d]!", chp[0],
					   cl);
				free(cldata[cl].authuser);
				cldata[cl].authuser = NULL;
			    }
			if (cldata[cl].inbuffer)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased buffer [%c %d]!",
					   chp[0], cl);
				free(cldata[cl].inbuffer);
				cldata[cl].inbuffer = NULL;
			    }
			cldata[cl].user[0] = '\0';
			cldata[cl].passwd[0] = '\0';
			cldata[cl].host[0] = '\0';
			bzero(cldata[cl].idone, BDSIZE);
			cldata[cl].buflen = 0;
			if (chp[0] == 'C')
				cldata[cl].state = A_ACTIVE;
			else
			    {
				cldata[cl].state = A_ACTIVE|A_IGNORE;
				break;
			    }
			if (sscanf(chp+2, "%[^ ] %hu %[^ ] %hu",
				   cldata[cl].itsip, &cldata[cl].itsport,
				   cldata[cl].ourip, &cldata[cl].ourport) != 4)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
					   "Bad data from ircd [%s] (fatal)",
					   chp);
				exit(1);
			    }
			/* we should really be using a pool of buffer here */
			cldata[cl].inbuffer = malloc(INBUFSIZE+1);
			if (cl > cl_highest)
				cl_highest = cl;
			next_io(cl, NULL); /* get started */
			break;
		case 'D': /* client disconnect */
			if (!(cldata[cl].state & A_ACTIVE))
				/*
				** this is not fatal, it happens with servers
				** we connected to (and more?).
				** It's better/safer to ignore here rather
				** than try to filter in ircd. -kalt
				*/
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [D] is not active.", cl);
			cldata[cl].state = 0;
			if (cldata[cl].rfd > 0 || cldata[cl].wfd > 0)
				cldata[cl].instance->mod->clean(cl);
			cldata[cl].instance = NULL;
			/* log something here? hmmpf */
			if (cldata[cl].authuser)
				free(cldata[cl].authuser);
			cldata[cl].authuser = NULL;
			if (cldata[cl].inbuffer)
				free(cldata[cl].inbuffer);
			cldata[cl].inbuffer = NULL;
			break;
		case 'R': /* fd remap */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* this should really not happen */
                                sendto_log(ALOG_IRCD, LOG_CRIT,
					   "Entry %d [R] is not active!", cl);
				break;
			    }
			ncl = atoi(chp+2);
			if (cldata[ncl].state & A_ACTIVE)
			    {
				/* this is not supposed to happen!!! */
                                sendto_log(ALOG_IRCD, LOG_CRIT,
			   "Entry %d [R] is already active (fatal)!", ncl);
				exit(1);
			    }
			if (cldata[ncl].instance || cldata[ncl].rfd > 0 ||
			    cldata[ncl].wfd > 0)
			    {
				sendto_log(ALOG_IRCD, LOG_CRIT,
				   "Entry %d is already active! (fatal)",
					   ncl);
				exit(1);
			    }
			if (cldata[ncl].authuser)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased data [%d]!", ncl);
				free(cldata[ncl].authuser);
				cldata[ncl].authuser = NULL;
			    }
			if (cldata[ncl].inbuffer)
			    {
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
					   "Unreleased buffer [%c %d]!",
					   chp[0], ncl);
				free(cldata[ncl].inbuffer);
				cldata[ncl].inbuffer = NULL;
			    }
			bcopy(cldata+cl, cldata+ncl, sizeof(anAuthData));

			cldata[cl].state = 0;
			cldata[cl].rfd = cldata[cl].wfd = 0;
			cldata[cl].instance = NULL;
			cldata[cl].authuser = NULL;
			cldata[cl].inbuffer = NULL;
			/*
			** this is the ugly part of having a slave (considering
			** that ircd remaps fd's: there is lag between the
			** server and the slave.
			** I can't think of any better way to handle this at
			** the moment -kalt
			*/
			if (cldata[ncl].state & A_IGNORE)
				break;
			if (cldata[ncl].state & A_LATE)
				/* pointless 99.9% of the time */
				break;
			if (cldata[ncl].authuser)
				sendto_ircd("%c %d %s %u %s", 
					    (cldata[ncl].state&A_UNIX)?'U':'u',
					    ncl, cldata[ncl].itsip,
                                            cldata[ncl].itsport,
					    cldata[ncl].authuser);
			if (cldata[ncl].state & A_DENY)
				sendto_ircd("K %d %s %u ", ncl,
					    cldata[ncl].itsip,
					    cldata[ncl].itsport,
                                            cldata[ncl].authuser);
			if (cldata[ncl].state & A_DONE)
				sendto_ircd("D %d %s %u ", ncl,
					    cldata[ncl].itsip,
					    cldata[ncl].itsport,
                                            cldata[ncl].authuser);
			break;
		case 'N': /* hostname */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* let's be conservative and just ignore */
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [N] is not active.", cl);
				break;
			    }
			if (cldata[cl].state & A_IGNORE)
				break;
			strcpy(cldata[cl].host, chp+2);
			cldata[cl].state |= A_GOTH|A_START;
			if (cldata[cl].instance == NULL)
				next_io(cl, NULL);
			break;
		case 'A': /* host alias */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* let's be conservative and just ignore */
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [A] is not active.", cl);
				break;
			    }
			if (cldata[cl].state & A_IGNORE)
				break;
			/* hmmpf */
			break;
		case 'U': /* user provided username */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* let's be conservative and just ignore */
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [U] is not active.", cl);
				break;
			    }
			if (cldata[cl].state & A_IGNORE)
				break;
			strcpy(cldata[cl].user, chp+2);
			cldata[cl].state |= A_GOTU|A_START;
			if (cldata[cl].instance == NULL)
				next_io(cl, NULL);
			break;
		case 'P': /* user provided password */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* let's be conservative and just ignore */
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [P] is not active.", cl);
				break;
			    }
			if (cldata[cl].state & A_IGNORE)
				break;
			strcpy(cldata[cl].passwd, chp+2);
			cldata[cl].state |= A_GOTP;
			/*
			** U message will follow immediately, 
			** no need to do any thing else here
			*/
			break;
		case 'T': /* ircd is registering the client */
			/* what to do with this? abort/continue? */
			cldata[cl].state |= A_LATE;
			break;
		case 'd': /* DNS timeout */
		case 'n': /* No hostname information, but no timeout either */
			if (!(cldata[cl].state & A_ACTIVE))
			    {
				/* let's be conservative and just ignore */
                                sendto_log(ALOG_IRCD, LOG_WARNING,
				   "Warning: Entry %d [%c] is not active.", 
					   cl, chp[0]);
				break;
			    }
			cldata[cl].state |= A_NOH|A_START;
			if (cldata[cl].instance == NULL)
				next_io(cl, NULL);
			break;
		case 'E': /* error message from ircd */
			sendto_log(ALOG_DIRCD, LOG_DEBUG,
				   "Error from ircd: %s", chp);
			break;
		default:
			sendto_log(ALOG_IRCD, LOG_ERR, "Unexpected data [%s]",
				   chp);
			break;
		    }

		buf = ch+1;
	    }
	rb_len = 0; iob_len = 0;
	if (strlen(buf))
		bcopy(buf, rbuf, rb_len = strlen(buf));
}

/*
 * loop_io
 *
 *	select()/poll() loop
 */
void
loop_io()
{
    /* the following is from ircd/s_bsd.c */
#if ! USE_POLL
# define SET_READ_EVENT( thisfd )       FD_SET( thisfd, &read_set)
# define SET_WRITE_EVENT( thisfd )      FD_SET( thisfd, &write_set)
# define CLR_READ_EVENT( thisfd )       FD_CLR( thisfd, &read_set)
# define CLR_WRITE_EVENT( thisfd )      FD_CLR( thisfd, &write_set)
# define TST_READ_EVENT( thisfd )       FD_ISSET( thisfd, &read_set)
# define TST_WRITE_EVENT( thisfd )      FD_ISSET( thisfd, &write_set)

        fd_set  read_set, write_set;
        int     highfd = -1;
#else
/* most of the following use pfd */
# define POLLSETREADFLAGS       (POLLIN|POLLRDNORM)
# define POLLREADFLAGS          (POLLSETREADFLAGS|POLLHUP|POLLERR)
# define POLLSETWRITEFLAGS      (POLLOUT|POLLWRNORM)
# define POLLWRITEFLAGS         (POLLOUT|POLLWRNORM|POLLHUP|POLLERR)

# define SET_READ_EVENT( thisfd ){  CHECK_PFD( thisfd );\
                                   pfd->events |= POLLSETREADFLAGS;}
# define SET_WRITE_EVENT( thisfd ){ CHECK_PFD( thisfd );\
                                   pfd->events |= POLLSETWRITEFLAGS;}

# define CLR_READ_EVENT( thisfd )       pfd->revents &= ~POLLSETREADFLAGS
# define CLR_WRITE_EVENT( thisfd )      pfd->revents &= ~POLLSETWRITEFLAGS
# define TST_READ_EVENT( thisfd )       pfd->revents & POLLREADFLAGS
# define TST_WRITE_EVENT( thisfd )      pfd->revents & POLLWRITEFLAGS

# define CHECK_PFD( thisfd )                    \
        if ( pfd->fd != thisfd ) {              \
                pfd = &poll_fdarray[nbr_pfds++];\
                pfd->fd     = thisfd;           \
                pfd->events = 0;                \
					    }

        struct pollfd   poll_fdarray[MAXCONNECTIONS];
        struct pollfd * pfd     = poll_fdarray;
        int        nbr_pfds = 0;
#endif

	int i, nfds = 0;
	struct timeval wait;
	time_t now = time(NULL);

#if !defined(USE_POLL)
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	highfd = 0;
#else
	/* set up such that CHECK_FD works */
	nbr_pfds = 0;
	pfd      = poll_fdarray;
	pfd->fd  = -1;
#endif  /* USE_POLL */

	SET_READ_EVENT(0); nfds = 1;		/* ircd stream */
#if defined(USE_POLL) && defined(IAUTH_DEBUG)
	for (i = 0; i < MAXCONNECTIONS; i++)
		fd2cl[i] = -1; /* sanity */
#endif
	for (i = 0; i <= cl_highest; i++)
	    {
		if (cldata[i].timeout && cldata[i].timeout < now &&
		    cldata[i].instance /* shouldn't be needed.. but it is */)
		    {
			DebugLog((ALOG_DIO, 0,
				  "io_loop(): module %s timeout [%d]",
				  cldata[i].instance->mod->name, i));
			if (cldata[i].instance->mod->timeout(i) != 0)
				next_io(i, cldata[i].instance);
		    }
		if (cldata[i].rfd > 0)
		    {
			SET_READ_EVENT(cldata[i].rfd);
#if !defined(USE_POLL)
			if (cldata[i].rfd > highfd)
				highfd = cldata[i].rfd;
#else
			fd2cl[cldata[i].rfd] = i;
#endif
			nfds++;
		    }
		else if (cldata[i].wfd > 0)
		    {
			SET_WRITE_EVENT(cldata[i].wfd);
#if ! USE_POLL
			if (cldata[i].wfd > highfd)
				highfd = cldata[i].wfd;
#else
			fd2cl[cldata[i].wfd] = i;
#endif
			nfds++;
		    }
	    }

	DebugLog((ALOG_DIO, 0, "io_loop(): checking for %d fd's", nfds));
	wait.tv_sec = 5; wait.tv_usec = 0;
#if ! USE_POLL
	nfds = select(highfd + 1, (SELECT_FDSET_TYPE *)&read_set,
		      (SELECT_FDSET_TYPE *)&write_set, 0, &wait);
	DebugLog((ALOG_DIO, 0, "io_loop(): select() returned %d, errno = %d",
		  nfds, errno));
#else
	nfds = poll(poll_fdarray, nbr_pfds,
		    wait.tv_sec * 1000 + wait.tv_usec/1000 );
	DebugLog((ALOG_DIO, 0, "io_loop(): poll() returned %d, errno = %d",
		  nfds, errno));
	pfd = poll_fdarray;
#endif
	if (nfds == -1)
		if (errno == EINTR)
			return;
		else
		    {
			sendto_log(ALOG_IRCD, LOG_CRIT,
				   "fatal select/poll error: %s",
				   strerror(errno));
			exit(1);
		    }
	if (nfds == 0)	/* end of timeout */
		return;

	/* no matter select() or poll() this is also fd # 0 */
	if (TST_READ_EVENT(0))
		nfds--;

#if !defined(USE_POLL)
	for (i = 0; i <= cl_highest && nfds; i++)
#else
	for (pfd = poll_fdarray+1; pfd != poll_fdarray+nbr_pfds && nfds; pfd++)
#endif
	    {
#if defined(USE_POLL)
		i = fd2cl[pfd->fd];
# if defined(IAUTH_DEBUG)
		if (i == -1)
		    {
			sendto_log(ALOG_DALL, LOG_CRIT,"io_loop(): fatal bug");
			exit(1);
		    }
# endif
#endif
		if (cldata[i].rfd <= 0 && cldata[i].wfd <= 0)
		    {
#if defined(USE_POLL)
			sendto_log(ALOG_IRCD, LOG_CRIT,
			   "io_loop(): fatal data inconsistency #%d (%d, %d)",
				   i, cldata[i].rfd, cldata[i].wfd);
			exit(1);
#else
			continue;
#endif
		    }
		if (cldata[i].rfd > 0 && TST_READ_EVENT(cldata[i].rfd))
		    {
			int len;
 
			len = recv(cldata[i].rfd,
				   cldata[i].inbuffer + cldata[i].buflen,
				   INBUFSIZE - cldata[i].buflen, 0);
			DebugLog((ALOG_DIO, 0, "io_loop(): i = #%d: recv(%d) returned %d, errno = %d", i, cldata[i].rfd, len, errno));
			if (len < 0)
			    {
				cldata[i].instance->mod->clean(i);
				next_io(i, cldata[i].instance);
			    }
			else
			    {
				cldata[i].buflen += len;
				if (cldata[i].instance->mod->work(i) != 0)
					next_io(i, cldata[i].instance);
				else if (len == 0)
				    {
					cldata[i].instance->mod->clean(i);
					next_io(i, cldata[i].instance);
				    }
			    }
			nfds--;
		    }
		else if (cldata[i].wfd > 0 && TST_WRITE_EVENT(cldata[i].wfd))
		    {
			if (cldata[i].instance->mod->work(i) != 0)
				next_io(i, cldata[i].instance);
			
			nfds--;
		    }
	    }

	/*
        ** no matter select() or poll() this is also fd # 0
	** this has to be done last (for the USE_POLL version) because
	** of R messages we may get from the server :/
	*/
#if defined(USE_POLL)
	pfd = poll_fdarray;
#endif
	if (TST_READ_EVENT(0))
	    {
		/* data from the ircd.. */
		while (1)
		    {
			if (rb_len)
				bcopy(rbuf, iobuf, iob_len = rb_len);
			if ((i=recv(0,iobuf+iob_len,IOBUFSIZE-iob_len,0)) <= 0)
			    {
				DebugLog((ALOG_DIO, 0, "io_loop(): recv(0) returned %d, errno = %d", i, errno));
				break;
			    }
			iob_len += i;
			DebugLog((ALOG_DIO, 0,
				  "io_loop(): got %d bytes from ircd [%d]", i,
				  iob_len));
			parse_ircd();
		    }
		if (i == 0)
		    {
			sendto_log(ALOG_DMISC, LOG_NOTICE,
				   "Daemon exiting. [r]");
			exit(0);
		    }
	    }

#if defined(IAUTH_DEBUG)
	if (nfds > 0)
		sendto_log(ALOG_DIO, 0, "io_loop(): nfds = %d !!!", nfds);
# if !defined(USE_POLL)
	/* the equivalent should be written for poll() */
	if (nfds == 0)
		while (i <= cl_highest)
		    {
			if (cldata[i].rfd > 0 && TST_READ_EVENT(cldata[i].rfd))
			    {
				/* this should not happen! */
				/* hmmpf */
			    }
			i++;
		    }
# endif
#endif
}

/*
 * set_non_blocking (ripped from ircd/s_bsd.c)
 */
static void
set_non_blocking(fd, ip, port)
int fd;
char *ip;
u_short port;
{
	int     res, nonb = 0;

#if NBLOCK_POSIX
        nonb |= O_NONBLOCK;
#endif
#if NBLOCK_BSD
        nonb |= O_NDELAY;
#endif
#if NBLOCK_SYSV
        /* This portion of code might also apply to NeXT.  -LynX */
        res = 1;
 
        if (ioctl (fd, FIONBIO, &res) < 0)
                sendto_log(ALOG_IRCD, 0, "ioctl(fd,FIONBIO) failed for %s:%u",
			   ip, port);
#else   
        if ((res = fcntl(fd, F_GETFL, 0)) == -1)
                sendto_log(ALOG_IRCD, 0, "fcntl(fd, F_GETFL) failed for %s:%u",
			   ip, port);
        else if (fcntl(fd, F_SETFL, res | nonb) == -1)
                sendto_log(ALOG_IRCD, 0,
			   "fcntl(fd, F_SETL, nonb) failed for %s:%u",
			   ip, port);
#endif  
}

/*
 * tcp_connect
 *
 *	utility function for use in modules, creates a socket and connects
 *	it to an IP/port
 *
 *	Returns the fd
 */
int
tcp_connect(ourIP, theirIP, port, error)
char *ourIP, *theirIP, **error;
u_short port;
{
	int fd;
	static char errbuf[BUFSIZ];
	struct SOCKADDR_IN sk;

	fd = socket(AFINET, SOCK_STREAM, 0);
	if (fd < 0)
	    {
		sprintf(errbuf, "socket() failed: %s", strerror(errno));
		*error = errbuf;
		return -1;
	    }
	/*
	 * this bzero() shouldn't be needed.. should it?
	 * AIX 4.1.5 doesn't like not having it tho.. I have no clue why -kalt
	 */
	bzero((char *)&sk, sizeof(sk));
	sk.SIN_FAMILY = AFINET;
#if defined(INET6)
	if(!inet_pton(AF_INET6, ourIP, sk.sin6_addr.s6_addr))
		bcopy(minus_one, sk.sin6_addr.s6_addr, IN6ADDRSZ);
#else
	sk.sin_addr.s_addr = inetaddr(ourIP);
#endif
	sk.SIN_PORT = htons(0);
	if (bind(fd, (SAP)&sk, sizeof(sk)) < 0)
	    {
		sprintf(errbuf, "bind() failed: %s", strerror(errno));
		*error = errbuf;
		close(fd);
		return -1;
	    }
	set_non_blocking(fd, theirIP, port);
#if defined(INET6)
	if(!inet_pton(AF_INET6, theirIP, sk.sin6_addr.s6_addr))
		bcopy(minus_one, sk.sin6_addr.s6_addr, IN6ADDRSZ);
#else
	sk.sin_addr.s_addr = inetaddr(theirIP);
#endif
	sk.SIN_PORT = htons(port);
	if (connect(fd, (SAP)&sk, sizeof(sk)) < 0 && errno != EINPROGRESS)
	    {
		sprintf(errbuf, "connect() to %s %u failed: %s", theirIP, port,
			strerror(errno));
		*error = errbuf;
		close(fd);
		return -1;
	    }
	*error = NULL;
	return fd;
}
