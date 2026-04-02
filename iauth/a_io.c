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
static const volatile char rcsid[] = "@(#)$Id: a_io.c,v 1.31 2005/01/03 17:33:55 q Exp $";
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

typedef struct {
	int fd;
	int want_write;
	AnInstance *inst;
} GFD;

#ifndef MAXGFD
#define MAXGFD MAXI
#endif
static GFD gfdv[MAXGFD]; /* fd == 0 indicates a free (unused) entry */

static void gfd_clear_slot(int i)
{
	gfdv[i].fd = 0;
	gfdv[i].want_write = 0;
	gfdv[i].inst = NULL;
}

static int gfd_used(int i)
{
	return gfdv[i].fd > 0 && gfdv[i].inst != NULL;
}

/* Registers a global file descriptor for the given instance */
int io_register_gfd(AnInstance *inst, int fd, int want_write)
{
	int i;
	if (!inst || fd <= 0)
	{
		return -1;
	}
	for (i = 0; i < MAXGFD; i++)
	{
		if (!gfd_used(i))
		{
			gfdv[i].fd = fd;
			gfdv[i].want_write = want_write ? 1 : 0;
			gfdv[i].inst = inst;
			return 0;
		}
	}
	return -1;
}

/* Updates write-interest flag for a previously registered global
 * file descriptor.
 * Returns 0 on success, -1 if the (inst, fd) pair was not found.
 */
int io_update_gfd(AnInstance *inst, int fd, int want_write)
{
	int i;

	if (!inst || fd <= 0)
	{
		return -1;
	}

	for (i = 0; i < MAXGFD; i++)
	{
		if (gfd_used(i) && gfdv[i].inst == inst && gfdv[i].fd == fd)
		{
			gfdv[i].want_write = want_write ? 1 : 0;
			return 0;
		}
	}
	return -1;
}

/* Unregisters all global file descriptors belonging to an instance */
void io_unregister_gfd(AnInstance *inst)
{
	int i;
	if (!inst)
	{
		return;
	}
	for (i = 0; i < MAXGFD; i++)
	{
		if (gfdv[i].inst == inst)
		{
			gfd_clear_slot(i);
		}
	}
}

/* Dispatches I/O event for a ready global file descriptor */
int io_handle_global_fd(int ready_fd)
{
	int gi, handled = 0;

	for (gi = 0; gi < MAXGFD; gi++)
	{
		if (gfd_used(gi) && gfdv[gi].fd == ready_fd)
		{
			if (gfdv[gi].inst && gfdv[gi].inst->mod &&
				gfdv[gi].inst->mod->gwork)
			{
				gfdv[gi].inst->mod->gwork(gfdv[gi].inst);
			}
			handled = 1;
			break;
		}
	}
	return handled ? 0 : -1;
}

void	init_io(void)
{
    bzero((char *) cldata, sizeof(cldata));
}

/*
 * Count iauth instances that should run after the client has sent NICK/USER
 * (and possibly CAP/AUTHENTICATE).
 *
 * Criteria:
 *  - instance has wait_for_reg set (directly or implicitly)
 *  - and is not currently skipped:
 *      - skip_if_sasl:  skip when A_SASL is set
 *      - skip_if_ident: skip when A_GOTIDENT is set
 */
static int count_wait_for_reg_instances(int cl)
{
	int n = 0;
	AnInstance *it = instances;

	while (it)
	{
		int cm = conf_match((u_int) cl, it);
		if (cm == 0)
		{
			if (it->wait_for_reg)
			{
				if (it->skip_if_sasl && (cldata[cl].state & A_SASL))
				{
					/* module would be skipped later due to completed SASL */
				}
				else if (it->skip_if_ident && (cldata[cl].state & A_GOTIDENT))
				{
					/* module would be skipped later due to ident reply */
				}
				else
				{
					n++;
				}
			}
		}
		it = it->nexti;
	}
	return n;
}

/* sendto_ircd() functions */
void	vsendto_ircd(char *pattern, va_list va)
{
	char	ibuf[4096];

	vsprintf(ibuf, pattern, va);
	DebugLog((ALOG_DSPY, 0, "To ircd: [%s]", ibuf));
	strcat(ibuf, "\n");
	if (write(0, ibuf, strlen(ibuf)) != strlen(ibuf))
	    {
		sendto_log(ALOG_DMISC, LOG_NOTICE, "Daemon exiting. [w %s]",
			   strerror(errno));
		exit(0);
	    }
}

void	sendto_ircd(char *pattern, ...)
{
        va_list va;
        va_start(va, pattern);
        vsendto_ircd(pattern, va);
        va_end(va);
}

static char *iauth_skip_ws(char *p)
{
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

/* Reads a token up to the next whitespace and stores it in dst (bounded).
 * Returns a pointer to the character right after the token.
 */
static char *iauth_read_token(char *p, char *dst, size_t dstlen)
{
	size_t i = 0;

	p = iauth_skip_ws(p);
	if (!*p)
	{
		if (dstlen > 0)
			dst[0] = '\0';
		return p;
	}

	while (*p && *p != ' ' && *p != '\t')
	{
		if (i + 1 < dstlen)
			dst[i++] = *p;
		p++;
	}
	if (dstlen > 0)
		dst[i] = '\0';

	return p;
}

/*
 * next_io
 *
 *	given an entry, look for the next module instance to start
 */
static	void	next_io(int cl, AnInstance *last)
{
	/* If any module set A_DENY, stop scheduling immediately */
	if (cldata[cl].state & A_DENY)
	{
		DebugLog((ALOG_DSPY, 0,
				  "skipping further modules for client %d (A_DENY set)", cl));
		sendto_ircd("D %d %s %u ", cl, cldata[cl].itsip, cldata[cl].itsport,
					cldata[cl].authuser);
		return;
	}

	DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): last=%s state=0x%X", cl, last,
			  (last) ? last->mod->name : "", cldata[cl].state));

	/*
	 * iauth distinguishes between:
	 *   (1) modules which run immediately after the TCP
	 *       connection is established;
	 *   (2) wait_for_reg modules which run after the client has sent
	 *       NICK/USER (and possibly CAP/AUTHENTICATE), but
	 *       before registration completes (before numeric 001).
	 *
	 * This block handles (2). On the first detection that any
	 * wait_for_reg modules are required for this client, we notify
	 * ircd with:   "P <clid> <n>"
	 * where <n> is the number of wait_for_reg modules. The 'P' tells
	 * ircd to hold registration until we later signal completion with 'H'.
	 */
	if (!(cldata[cl].state & A_WAIT_FOR_REG))
	{
		int n = count_wait_for_reg_instances(cl);
		if (n > 0)
		{
			sendto_log(ALOG_DSPY, LOG_DEBUG,
					   "sending wait_for_reg request to ircd for %d", cl);
			sendto_ircd("P %d %d", cl, n);
			cldata[cl].state |= A_WAIT_FOR_REG;
			/*
 			 * If called from inside a module (last != NULL), clear the current
			 * instance pointer. Otherwise, iauth would still think a module is
			 * running and could block the later "H" completion signal.
			 */
			if (last)
			{
				cldata[cl].instance = NULL;
			}
			return;
		}
	}
	/* Stop here if this entry is already completed (A_DONE) */
	if (cldata[cl].state & A_DONE)
	{
		return;
	}

	/* Next, ensure that the previously running instance has cleaned up its FDs.
	 * This applies only when advancing from 'last' (i.e., switching modules).
	 * If last == NULL (external trigger such as 'H'), do not discard
	 * the currently running module.
	 */
	if (last && (cldata[cl].rfd > 0 || cldata[cl].wfd > 0))
	{
		/* last is defined here */
		sendto_log(ALOG_IRCD | ALOG_DMISC, LOG_ERR,
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

	if (last)
	{
		cldata[cl].instance = NULL;
	}

	cldata[cl].timeout = 0;

    /* If A_START is set, a new pass has to be started */
    if (cldata[cl].state & A_START)
	{
	    cldata[cl].state ^= A_START;
	    DebugLog((ALOG_DIO, 0, "next_io(#%d, %x): Starting again",
		      cl, last));
      	/* start from beginning */
	    last = NULL;
	}

	/* Find next instance to be run */
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
		/* Implicit skip or defer based on option flags:
		 * skip_if_sasl / skip_if_ident */
		if (cldata[cl].instance)
		{
			AnInstance *inst = cldata[cl].instance;
			const char *modname = (inst->mod && inst->mod->name)
										  ? inst->mod->name
										  : "<unknown>";

			/* Skip if SASL already succeeded */
			if (inst->skip_if_sasl && (cldata[cl].state & A_SASL))
			{
				SetBit(cldata[cl].idone, inst->in);
				cldata[cl].instance = inst->nexti;
				DebugLog((ALOG_DIO, 0,
						  "skipping module \"%s\" (in=%d) due to SASL", modname,
						  inst->in));
				continue;
			}

			/* Skip if an ident reply was already received */
			if (inst->skip_if_ident && (cldata[cl].state & A_GOTIDENT))
			{
				SetBit(cldata[cl].idone, inst->in);
				cldata[cl].instance = inst->nexti;
				DebugLog((ALOG_DIO, 0,
						  "skipping module \"%s\" (in=%d) due to ident",
						  modname, inst->in));
				continue;
			}

			/* Defer wait_for_reg modules until client registration */
			if (inst->wait_for_reg && !(cldata[cl].state & A_REG_PENDING))
			{
				cldata[cl].instance = inst->nexti;
				DebugLog((ALOG_DIO, 0,
						  "deferring module \"%s\" (in=%d) "
						  "waiting for client registration",
						  modname, inst->in));
				continue;
			}
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
			if (cldata[cl].state & A_DENY)
			{
				sendto_log(ALOG_DSPY, LOG_DEBUG,
						   "suppressing D for %d (A_DENY set)", cl);
				return;
			}
			if ((cldata[cl].state & A_WAIT_FOR_REG) &&
				!(cldata[cl].state & A_REG_PENDING))
			{
				sendto_log(ALOG_DSPY, LOG_DEBUG,
						   "deferring D for %d (P sent; waiting for "
						   "client registration)",
						   cl);
				return;
			}
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

	    if (cldata[cl].instance->delayed &&
			!(cldata[cl].state & A_DELAYEDSENT))
		{
		    /* fake to ircd that we're done */
		    sendto_ircd("D %d %s %u ", cl, cldata[cl].itsip,
				cldata[cl].itsport);
		    cldata[cl].state |= A_DELAYEDSENT;
		}

		DebugLog((ALOG_DIO, 0,
				  "next_io(#%d): calling %s->start() with instance=%p", cl,
				  cldata[cl].instance && cldata[cl].instance->mod
						  ? cldata[cl].instance->mod->name
						  : "<null>",
				  cldata[cl].instance));

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
static	void	parse_ircd(void)
{
	char *ch, *chp, *buf = iobuf, *p;
	int cl = -1, ncl;

	iobuf[iob_len] = '\0';
	while ((ch = index(buf, '\n')))
	    {
		*ch = '\0';
		DebugLog((ALOG_DSPY, 0, "parse_ircd(): got [%s]", buf));

		cl = atoi(chp = buf);
		if (cl >= MAXCONNECTIONS)
		    {
			sendto_log(ALOG_IRCD, LOG_CRIT,
			   "Recompile iauth, (fatal %d>=%d)", cl, MAXCONNECTIONS);
			exit(1);
		    }
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
			if (cldata[cl].sasl_user)
			{
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
						   "Unreleased data [%c %d]!", chp[0],
						   cl);
				free(cldata[cl].sasl_user);
				cldata[cl].sasl_user = NULL;
			}
			cldata[cl].nick[0] = '\0';
			cldata[cl].user1[0] = '\0';
			cldata[cl].user2[0] = '\0';
			cldata[cl].user3[0] = '\0';
			cldata[cl].realname[0] = '\0';
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
			if (cldata[cl].sasl_user)
				free(cldata[cl].sasl_user);
			cldata[cl].sasl_user = NULL;
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
				   "Entry %d is already active! (fatal)", ncl);
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
			if (cldata[ncl].sasl_user)
			{
				/* shouldn't be here - hmmpf */
				sendto_log(ALOG_IRCD|ALOG_DIO, LOG_WARNING,
						   "Unreleased buffer [%c %d]!",
						   chp[0], ncl);
				free(cldata[ncl].sasl_user);
				cldata[ncl].sasl_user = NULL;
			}
			bcopy(cldata+cl, cldata+ncl, sizeof(anAuthData));

			cldata[cl].state = 0;
			cldata[cl].rfd = cldata[cl].wfd = 0;
			cldata[cl].instance = NULL;
			cldata[cl].authuser = NULL;
			cldata[cl].inbuffer = NULL;
			cldata[cl].sasl_user = NULL;
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
			strcpy(cldata[cl].user1, chp + 2);
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
		case 'M':
			/* RPL_HELLO to be exact, but who cares. */
			strConnLen = sprintf(strConn, ":%s 020 * :", chp+2);
			break;
		case 'S': /* SASL authentication */
			cldata[cl].state |= A_SASL;
			cldata[cl].sasl_user = mystrdup(chp+2);
			break;
		case 'H': /* ircd received NICK/USER and possibly CAP/AUTHENTICATE
 					 and is waiting for iauth before registering the user */
			/* If already denied, ignore 'H' to avoid deferring teardown */
			sendto_log(ALOG_DSPY, LOG_DEBUG,
					   "received registration pending trigger (H) for client "
					   "%d: [%s]",
					   cl, chp);

			/* Entry must be active */
			if (!(cldata[cl].state & A_ACTIVE))
			{
				sendto_log(ALOG_IRCD, LOG_WARNING,
						   "Warning: Entry %d [H] is not active.", cl);
				break;
			}

			/* Parse and store values of:
			 * H <nick> <user1> <user2> <user3> :<realname> */
			p = chp + 1;
			p = iauth_skip_ws(p);

			p = iauth_read_token(p, cldata[cl].nick, sizeof(cldata[cl].nick));
			p = iauth_read_token(p, cldata[cl].user1, sizeof(cldata[cl].user1));
			p = iauth_read_token(p, cldata[cl].user2, sizeof(cldata[cl].user2));
			p = iauth_read_token(p, cldata[cl].user3, sizeof(cldata[cl].user3));

			p = iauth_skip_ws(p);
			if (*p == ':')
				p++;
			p = iauth_skip_ws(p);

			if (*p)
			{
				strncpy(cldata[cl].realname, p,
						sizeof(cldata[cl].realname) - 1);
				cldata[cl].realname[sizeof(cldata[cl].realname) - 1] = '\0';
			}
			else
			{
				cldata[cl].realname[0] = '\0';
			}

			/* Mark registration pending */
			cldata[cl].state |= A_REG_PENDING;
			sendto_log(ALOG_DSPY, LOG_DEBUG,
					   "marking registration pending for %d (state=0x%lx)", cl,
					   (unsigned long) cldata[cl].state);

			/* If a module is still running, wait until it completes */
			if (cldata[cl].rfd > 0 || cldata[cl].wfd > 0)
			{
				const char *modname = "<none>";
				/* reschedule once current module completes */
				cldata[cl].state |= A_START;
				if (cldata[cl].instance && cldata[cl].instance->mod &&
					cldata[cl].instance->mod->name)
				{
					modname = cldata[cl].instance->mod->name;
				}
				sendto_log(ALOG_DSPY, LOG_DEBUG,
						   "deferring until current module "
						   "completes (cl=%d, running=%s)",
						   cl, modname);
				break;
			}

			sendto_log(ALOG_DSPY, LOG_DEBUG,
					   "no active module; starting wait_for_reg modules now "
					   "(cl=%d)",
					   cl);
			next_io(cl, NULL);

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
void	loop_io(void)
{
    /* the following is from ircd/s_bsd.c */
#if !defined(USE_POLL)
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

	int gi, i, nfds = 0;
	struct timeval wait;
	time_t now = time(NULL);
	struct pollfd *pf;

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

	/* always register fd 0 (ircd stream) first */
	SET_READ_EVENT(0);
	nfds = 1;

	/* now register global file descriptors */
#if !defined(USE_POLL)
	for (gi = 0; gi < MAXGFD; gi++)
	{
		if (gfd_used(gi))
		{
			FD_SET(gfdv[gi].fd, &read_set);
			if (gfdv[gi].want_write)
			{
				FD_SET(gfdv[gi].fd, &write_set);
			}
			if (gfdv[gi].fd > highfd)
			{
				highfd = gfdv[gi].fd;
			}
		}
	}
#else
	for (gi = 0; gi < MAXGFD; gi++)
	{
		if (gfd_used(gi))
		{
			CHECK_PFD(gfdv[gi].fd);
			pfd->events |= POLLSETREADFLAGS;
			if (gfdv[gi].want_write)
			{
				pfd->events |= POLLSETWRITEFLAGS;
			}
		}
	}
	/* reset fd->client mapping: global FDs remain -1 */
	for (i = 0; i < MAXCONNECTIONS; i++)
		fd2cl[i] = -1;
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
#if !defined(USE_POLL)
			if (cldata[i].wfd > highfd)
				highfd = cldata[i].wfd;
#else
			fd2cl[cldata[i].wfd] = i;
#endif
			nfds++;
		    }
	    }

	wait.tv_sec = 5; wait.tv_usec = 0;
#if !defined(USE_POLL)
	nfds = select(highfd + 1, (SELECT_FDSET_TYPE *)&read_set,
		      (SELECT_FDSET_TYPE *)&write_set, 0, &wait);
	if (nfds < 0)
		DebugLog((ALOG_DIO, 0, "io_loop(): select() returned %d, errno = %d",
			  nfds, errno));
#else
	nfds = poll(poll_fdarray, nbr_pfds,
		    wait.tv_sec * 1000 + wait.tv_usec/1000 );
	if (nfds < 0)
		DebugLog((ALOG_DIO, 0, "io_loop(): poll() returned %d, errno = %d",
			  nfds, errno));
	pfd = poll_fdarray;
#endif
	if (nfds == -1)
	{
		if (errno == EINTR)
		{
			return;
		}
		else
		{
			sendto_log(ALOG_IRCD, LOG_CRIT,
				   "fatal select/poll error: %s",
				   strerror(errno));
			exit(1);
		}
	}
	if (nfds == 0)	/* end of timeout */
		return;

#if !defined(USE_POLL)
	for (gi = 0; gi < MAXGFD; gi++)
	{
		if (gfd_used(gi))
		{
			int ready = 0;
			if (FD_ISSET(gfdv[gi].fd, &read_set))
			{
				ready = 1;
			}
			if (!ready && gfdv[gi].want_write &&
				FD_ISSET(gfdv[gi].fd, &write_set))
			{
				ready = 1;
			}
			if (ready)
			{
				if (gfdv[gi].inst && gfdv[gi].inst->mod &&
					gfdv[gi].inst->mod->gwork)
				{
					gfdv[gi].inst->mod->gwork(gfdv[gi].inst);
				}
				nfds--;
			}
		}
	}
#else
	for (pf = poll_fdarray; pf != poll_fdarray + nbr_pfds; ++pf)
	{
		int fdj;
		if (pf->revents == 0)
		{
			continue;
		}
		fdj = pf->fd;
		for (gi = 0; gi < MAXGFD; gi++)
		{
			if (gfd_used(gi) && gfdv[gi].fd == fdj)
			{
				if (gfdv[gi].inst && gfdv[gi].inst->mod &&
					gfdv[gi].inst->mod->gwork)
				{
					gfdv[gi].inst->mod->gwork(gfdv[gi].inst);
				}
				pf->revents = 0;
				nfds--;
				break;
			}
		}
	}
#endif

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
			/* bounds check against fd2cl array and skip global FDs */
			int fdj = pfd->fd;
			/* fd outside mapping range -> not a client FD */
			if (fdj < 0 || fdj >= MAXCONNECTIONS)
			{
				continue;
			}
			i = fd2cl[fdj];
			/* global or unmapped FD -> already handled in global block */
			if (i == -1)
			{
				continue;
			}
#if defined(IAUTH_DEBUG)
			/* sanity check: ensure client index is within valid range */
			if (i < 0 || i > cl_highest)
			{
				sendto_log(ALOG_DALL, LOG_CRIT,
						   "io_loop(): fatal bug (invalid cl=%d fd=%d)", i,
						   fdj);
				exit(1);
			}
#endif
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

#if 0
/* stupid code that tries to find a bug, but does nothing --Q */
#if defined(IAUTH_DEBUG)
	if (nfds > 0)
		sendto_log(ALOG_DIO, 0, "io_loop(): nfds = %d !!!", nfds);
# if !defined(USE_POLL)
	/* the equivalent should be written for poll() */
	if (nfds == 0)
		while (i <= cl_highest)
		    {
			/* Q got core here, he had i=-1 */
			if (cldata[i].rfd > 0 && TST_READ_EVENT(cldata[i].rfd))
			    {
				/* this should not happen! */
				/* hmmpf */
			    }
			i++;
		    }
# endif
#endif
#endif
}

/*
 * set_non_blocking (ripped from ircd/s_bsd.c)
 */
static	void	set_non_blocking(int fd, char *ip, u_short port)
{
	int res;

	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		sendto_log(ALOG_IRCD, 0, "fcntl(fd, F_GETFL) failed for %s:%u", ip, port);
	else if (fcntl(fd, F_SETFL, res | O_NONBLOCK) == -1)
		sendto_log(ALOG_IRCD, 0, "fcntl(fd, F_SETFL, O_NONBLOCK) failed for %s:%u", ip, port);
}

/*
 * tcp_connect
 *
 *	utility function for use in modules, creates a socket and connects
 *	it to an IP/port
 *
 *	Returns the fd
 */
int	tcp_connect(char *ourIP, char *theirIP, u_short port, char **error)
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

	if (ourIP)
	{
		if (!inetpton(AF_INET6, ourIP, sk.sin6_addr.s6_addr))
		{
			bcopy(minus_one, sk.sin6_addr.s6_addr, IN6ADDRSZ);
		}
		sk.SIN_PORT = htons(0);

		if (bind(fd, (SAP) &sk, sizeof(sk)) < 0)
		{
			sprintf(errbuf, "bind() failed: %s", strerror(errno));
			*error = errbuf;
			close(fd);
			return -1;
		}
	}
	set_non_blocking(fd, theirIP, port);
	if(!inetpton(AF_INET6, theirIP, sk.sin6_addr.s6_addr))
		bcopy(minus_one, sk.sin6_addr.s6_addr, IN6ADDRSZ);
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

/*
 * Ident state helpers (called by modules, e.g. rfc931)
 * - OK:    set A_GOTIDENT
 * - FAIL:  set A_NOIDENT   (timeout/refused/unavailable)
 * Both trigger the scheduler via next_io() so wait_for_ident
 * modules can proceed.
 */
void iauth_mark_ident_ok(u_int cl)
{
	if (cl >= MAXCONNECTIONS)
		return;
	if (!(cldata[cl].state & A_ACTIVE))
		return;

	if (!(cldata[cl].state & A_GOTIDENT))
	{
		cldata[cl].state |= A_GOTIDENT;
		sendto_log(ALOG_DSPY, LOG_DEBUG, "ident: ok for #%u (state=0x%lX)", cl,
				   (unsigned long) cldata[cl].state);
	}
	/* if nothing is running, try to advance */
	if (cldata[cl].instance == NULL)
		next_io((int) cl, NULL);
}

void iauth_mark_noident(u_int cl)
{
	if (cl >= MAXCONNECTIONS)
		return;

	cldata[cl].state |= A_NOIDENT;
	sendto_log(ALOG_DSPY, LOG_DEBUG, "ident: unavailable for #%u", cl);
	if (cldata[cl].instance == NULL)
		next_io((int) cl, NULL);
}
