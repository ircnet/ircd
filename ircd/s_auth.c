/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_auth.c
 *   Copyright (C) 1992 Darren Reed
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
static  char rcsid[] = "@(#)$Id: s_auth.c,v 1.43 1999/07/02 16:38:21 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_AUTH_C
#include "s_externs.h"
#undef S_AUTH_C

/*
 * set_clean_username
 *
 *	As non OTHER type of usernames retrieved via ident lookup are forced as
 *	usernames for the user, one has to be careful not to allow potentially
 *	damaging characters in the username.
 *	This procedure fills up cptr->username based on cptr->auth
 *	Characters disallowed:
 *		leading :	for obvious reasons
 *		spaces		for obvious reasons
 *		@		because of how get_sockhost() is implemented,
 *				and because it's used from attached_Iline()
 *		[		/trace parsing is impossible
 */
static void
set_clean_username(cptr)
aClient *cptr;
{
	int i = 0, dirty = 0;
	char *s;

	if (cptr->auth == NULL)
		return;
	s = cptr->auth;
	if (index(cptr->auth, '[') || index(cptr->auth, '@') ||
	    strlen(cptr->auth) > USERLEN)
		dirty = 1;
	else if (cptr->auth[0] == ':')
	    {
		dirty = 1;
		s += 1;
	    }
	else
	    {
		char *r = cptr->auth;

		while (*r)
			if (isspace(*(r++)))
				break;
		if (*r)
			dirty = 1;
	    }
	if (dirty)
		cptr->username[i++] = '-';
	while (i < USERLEN && *s)
	    {
		if (*s != '@' && *s != '[' && !isspace(*s))
			cptr->username[i++] = *s;
		s += 1;
	    }
	cptr->username[i] = '\0';
	if (!strcmp(cptr->username, cptr->auth))
	    {
		MyFree(cptr->auth);
		cptr->auth = cptr->username;
	    }
	else
	    {
		istat.is_authmem += sizeof(cptr->auth);
		istat.is_auth += 1;
	    }
}

#if defined(USE_IAUTH)

u_char		iauth_options = 0;
u_int		iauth_spawn = 0;
char		*iauth_version = NULL;

static aExtCf	*iauth_conf = NULL;
static aExtData	*iauth_stats = NULL;

/*
 * sendto_iauth
 *
 *	Send the buffer to the authentication slave process.
 *	Return 0 if everything went well, -1 otherwise.
 */
#if ! USE_STDARG
int
sendto_iauth(pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
#else
int
vsendto_iauth(char *pattern, va_list va)
#endif
{
    static char abuf[BUFSIZ];

#if ! USE_STDARG
    sprintf(abuf, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else   
    vsprintf(abuf, pattern, va);
#endif  
    strcat(abuf, "\n");

    if (adfd < 0)
	    return -1;
    if (write(adfd, abuf, strlen(abuf)) != strlen(abuf))
	{
	    sendto_flag(SCH_AUTH, "Aiiie! lost slave authentication process");
	    close(adfd);
	    adfd = -1;
	    start_iauth(0);
	    return -1;
	}
    return 0;
}

# if USE_STDARG
int
sendto_iauth(char *pattern, ...)
{
	int i;

        va_list va;
        va_start(va, pattern);
        i = vsendto_iauth(pattern, va);
        va_end(va);
	return i;
}
# endif

/*
 * read_iauth
 *
 *	read and process data from the authentication slave process.
 */
void
read_iauth()
{
    static char obuf[READBUF_SIZE+1], last = '?';
    static int olen = 0, ia_dbg = 0;
    char buf[READBUF_SIZE+1], *start, *end, tbuf[BUFSIZ];
    aClient *cptr;
    int i;

    if (adfd == -1)
	{
	    olen = 0;
	    return;
	}
    while (1)
	{
	    if (olen)
		    bcopy(obuf, buf, olen);
	    if ((i = recv(adfd, buf+olen, READBUF_SIZE-olen, 0)) <= 0)
		{
		    if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
			    sendto_flag(SCH_AUTH, "Aiiie! lost slave authentication process (errno = %d)", errno);
			    close(adfd);
			    adfd = -1;
			    start_iauth(0);
			}
		    break;
		}
	    olen += i;
	    buf[olen] = '\0';
	    start = buf;
	    while (end = index(start, '\n'))
		{
		    *end++ = '\0';
		    last = *start;
		    if (*start == '>')
			{
			    sendto_flag(SCH_AUTH, "%s", start+1);
			    start = end;
			    continue;
			}
		    if (*start == 'G')
			{
			    ia_dbg = atoi(start+2);
			    if (ia_dbg)
				    sendto_flag(SCH_AUTH,"ia_dbg = %d",ia_dbg);
			    start = end;
			    continue;
			}
		    if (*start == 'O') /* options */
			{
			    iauth_options = 0;
			    if (strchr(start+2, 'A'))
				    iauth_options |= XOPT_EARLYPARSE;
			    if (strchr(start+2, 'R'))
				    iauth_options |= XOPT_REQUIRED;
			    if (strchr(start+2, 'T'))
				    iauth_options |= XOPT_NOTIMEOUT;
			    if (strchr(start+2, 'W'))
				    iauth_options |= XOPT_EXTWAIT;
			    if (iauth_options)
				    sendto_flag(SCH_AUTH, "iauth options: %x",
						iauth_options);
			    start = end;
			    continue;
			}
		    if (*start == 'V') /* version */
			{
			    if (iauth_version)
				    MyFree(iauth_version);
			    iauth_version = mystrdup(start+2);
			    sendto_flag(SCH_AUTH, "iauth version %s running.",
					iauth_version);
			    start = end;
			    continue;
			}
		    if (*start == 'a')
			{
			    aExtCf *ectmp;

			    while (ectmp = iauth_conf)
				{
				    iauth_conf = iauth_conf->next;
				    MyFree(ectmp->line);
				    MyFree(ectmp);
				}
			    /* little lie.. ;) */
			    sendto_flag(SCH_AUTH, "New iauth configuration.");
			    start = end;
			    continue;
			}
		    if (*start == 'A')
			{
			    aExtCf **ectmp = &iauth_conf;

			    while (*ectmp)
				    ectmp = &((*ectmp)->next);
			    *ectmp = (aExtCf *) MyMalloc(sizeof(aExtCf));
			    (*ectmp)->line = mystrdup(start+2);
			    (*ectmp)->next = NULL;
			    start = end;
			    continue;
			}
		    if (*start == 's')
			{
			    aExtData *ectmp;

			    while (ectmp = iauth_stats)
				{
				    iauth_stats = iauth_stats->next;
				    MyFree(ectmp->line);
				    MyFree(ectmp);
				}
			    iauth_stats = (aExtData *)
				    MyMalloc(sizeof(aExtData));
			    iauth_stats->line = MyMalloc(60);
			    sprintf(iauth_stats->line,
				    "iauth modules statistics (%s)",
				    myctime(timeofday));
			    iauth_stats->next = (aExtData *)
				    MyMalloc(sizeof(aExtData));
			    iauth_stats->next->line = MyMalloc(60);
			    sprintf(iauth_stats->next->line,
				    "spawned: %d, current options: %X (%.11s)",
				    iauth_spawn, iauth_options,
				    (iauth_version) ? iauth_version : "???");
			    iauth_stats->next->next = NULL;
			    start = end;
			    continue;
			}
		    if (*start == 'S')
			{
			    aExtData **ectmp = &iauth_stats;

			    while (*ectmp)
				    ectmp = &((*ectmp)->next);
			    *ectmp = (aExtData *) MyMalloc(sizeof(aExtData));
			    (*ectmp)->line = mystrdup(start+2);
			    (*ectmp)->next = NULL;
			    start = end;
			    continue;
			}
		    if (*start != 'U' && *start != 'u' && *start != 'o' &&
			*start != 'K' && *start != 'k' &&
			*start != 'D')
			{
			    sendto_flag(SCH_AUTH, "Garbage from iauth [%s]",
					start);
			    sendto_iauth("-1 E Garbage [%s]", start);
			    /*
			    ** The above should never happen, but i've seen it
			    ** occasionnally, so let's try to get more info
			    ** about it! -kalt
			    */
			    sendto_flag(SCH_AUTH,
				"last=%u start=%x end=%x buf=%x olen=%d i=%d",
					last, start, end, buf, olen, i);
			    sendto_iauth(
			 "-1 E last=%u start=%x end=%x buf=%x olen=%d i=%d",
			 		last, start, end, buf, olen, i);
			    start = end;
			    continue;
			}
		    if ((cptr = local[i = atoi(start+2)]) == NULL)
			{
			    /* this is fairly common and can be ignored */
			    if (ia_dbg)
				{
				    sendto_flag(SCH_AUTH, "Client %d is gone.",
						i);
				    sendto_iauth("%d E Gone [%s]", i, start);
				}
			    start = end;
			    continue;
			}
		    sprintf(tbuf, "%c %d %s %u ", start[0], i,
			    inetntoa((char *)&cptr->ip), cptr->port);
		    if (strncmp(tbuf, start, strlen(tbuf)))
			{
			    /* this is fairly common and can be ignored */
			    if (ia_dbg)
				{
				    sendto_flag(SCH_AUTH,
					"Client mismatch: %d [%s] != [%s]",
						i, start, tbuf);
				    sendto_iauth("%d E Mismatch [%s] != [%s]",
						 i, start, tbuf);
				}
			    start = end;
			    continue;
			}
		    if (start[0] == 'U')
			{
			    if (*(start+strlen(tbuf)) == '\0')
				{
				    sendto_flag(SCH_AUTH,
						"Null U message! %d [%s]",
						i, start);
				    sendto_iauth("%d E Null U [%s]", i, start);
				    start = end;
				    continue;
				}
			    if (cptr->auth != cptr->username)
				{   
				    istat.is_authmem -= sizeof(cptr->auth);
				    istat.is_auth -= 1;
				    MyFree(cptr->auth);
				}
			    cptr->auth = mystrdup(start+strlen(tbuf));
			    set_clean_username(cptr);
			    cptr->flags |= FLAGS_GOTID;
			}
		    else if (start[0] == 'u')
			{
			    if (*(start+strlen(tbuf)) == '\0')
				{
				    sendto_flag(SCH_AUTH,
						"Null u message! %d [%s]",
						i, start);
				    sendto_iauth("%d E Null u [%s]", i, start);
				    start = end;
				    continue;
				}
			    if (cptr->auth != cptr->username)
				{
				    istat.is_authmem -= sizeof(cptr->auth);
				    istat.is_auth -= 1;
				    MyFree(cptr->auth);
				}
			    cptr->auth = MyMalloc(strlen(start+strlen(tbuf))
						  + 2);
			    *cptr->auth = '-';
			    strcpy(cptr->auth+1, start+strlen(tbuf));
			    set_clean_username(cptr);
			    cptr->flags |= FLAGS_GOTID;
			}
		    else if (start[0] == 'o')
			{
			    if (!WaitingXAuth(cptr))
				{
				    sendto_flag(SCH_AUTH,
						"Early o message discarded!");
				    sendto_iauth("%d E Early o [%s]", i,start);
				    start = end;
				    continue;
				}
			    if (cptr->user == NULL)
				{
				    /* just to be safe */
				    sendto_flag(SCH_AUTH,
						"Ack! cptr->user is NULL");
				    start = end;
				    continue;
				}
			    strncpyzt(cptr->user->username, tbuf, USERLEN+1);
			}
		    else if (start[0] == 'D')
		      {
			    /*authentication finished*/
			    ClearXAuth(cptr);
			    SetDoneXAuth(cptr);
			    if (WaitingXAuth(cptr))
				{
				    ClearWXAuth(cptr);
				    register_user(cptr, cptr, cptr->name,
						  cptr->user->username);
				}
			    else
				    ClearWXAuth(cptr);
		      }
		    else
			{
			    /*
			    ** mark for kill, because it cannot be killed
			    ** yet: we don't even know if this is a server
			    ** or a user connection!
			    */
			    if (start[0] == 'K')
				    cptr->exitc = EXITC_AREF;
			    else
				    cptr->exitc = EXITC_AREFQ;
			    /* should also check to make sure it's still
			       an unregistered client.. */
			    /* should be extended to work after registration */
			}
		    start = end;
		}
	    if (start != buf+olen)
		    bcopy(start, obuf, olen = (buf+olen)-start+1);
	    else
		    olen = 0;
	}
}

/*
 * report_iauth_conf
 *
 * called from m_stats(), this is the reply to /stats a
 */
void
report_iauth_conf(sptr, to)
aClient *sptr;
char *to;
{
	aExtCf *ectmp = iauth_conf;

	if (adfd < 0)
		return;
	while (ectmp)
	    {
		sendto_one(sptr, ":%s %d %s :%s",
			   ME, RPL_STATSIAUTH, to, ectmp->line);
		ectmp = ectmp->next;
	    }
}

/*
 * report_iauth_stats
 *
 * called from m_stats(), this is part of the reply to /stats t
 */
void
report_iauth_stats(sptr, to)
aClient *sptr;
char *to;
{
	aExtData *ectmp = iauth_stats;

	while (ectmp)
	    {
		sendto_one(sptr, ":%s %d %s :%s",
			   ME, RPL_STATSDEBUG, to, ectmp->line);
		ectmp = ectmp->next;
	    }
}
#endif

/*
 * start_auth
 *
 * Flag the client to show that an attempt to contact the ident server on
 * the client's host.  The connect and subsequently the socket are all put
 * into 'non-blocking' mode.  Should the connect or any later phase of the
 * identifing process fail, it is aborted and the user is given a username
 * of "unknown".
 */
void	start_auth(cptr)
Reg	aClient	*cptr;
{
#ifndef	NO_IDENT
	struct	SOCKADDR_IN	us, them;

	SOCK_LEN_TYPE ulen, tlen;

# if defined(USE_IAUTH)
	if ((iauth_options & XOPT_REQUIRED) && adfd < 0)
		return;
# endif
	Debug((DEBUG_NOTICE,"start_auth(%x) fd %d status %d",
		cptr, cptr->fd, cptr->status));
	if ((cptr->authfd = socket(AFINET, SOCK_STREAM, 0)) == -1)
	    {
# ifdef	USE_SYSLOG
		syslog(LOG_ERR, "Unable to create auth socket for %s:%m",
			get_client_name(cptr,TRUE));
# endif
		Debug((DEBUG_ERROR, "Unable to create auth socket for %s:%s",
			get_client_name(cptr, TRUE),
			strerror(get_sockerr(cptr))));
		ircstp->is_abad++;
		return;
	    }
	if (cptr->authfd >= (MAXCONNECTIONS - 2))
	    {
		sendto_flag(SCH_ERROR, "Can't allocate fd for auth on %s",
			   get_client_name(cptr, TRUE));
		(void)close(cptr->authfd);
		return;
	    }

	set_non_blocking(cptr->authfd, cptr);

	/* get remote host peer - so that we get right interface -- jrg */
	tlen = ulen = sizeof(us);
	if (getpeername(cptr->fd, (struct sockaddr *)&them, &tlen) < 0)
	    {
		/* we probably don't need this error message -kalt */
		report_error("getpeername for auth request %s:%s", cptr);
		close(cptr->authfd);
		cptr->authfd = -1;
		return;
	    }
	them.SIN_FAMILY = AFINET;

	/* We must bind the local end to the interface that they connected
	   to: The local system might have more than one network address,
	   and RFC931 check only sends port numbers: server takes IP addresses
	   from query socket -- jrg */
	(void)getsockname(cptr->fd, (struct sockaddr *)&us, &ulen);
	us.SIN_FAMILY = AFINET;
# if defined(USE_IAUTH)
	if (adfd >= 0)
	    {
		char abuf[BUFSIZ];
#  ifdef INET6
		sprintf(abuf, "%d C %s %u ", cptr->fd,
			inetntop(AF_INET6, (char *)&them.sin6_addr, mydummy,
				 MYDUMMY_SIZE), ntohs(them.SIN_PORT));
		sprintf(abuf+strlen(abuf), "%s %u",
			inetntop(AF_INET6, (char *)&us.sin6_addr, mydummy,
				 MYDUMMY_SIZE), ntohs(us.SIN_PORT));
#  else
		sprintf(abuf, "%d C %s %u ", cptr->fd,
			inetntoa((char *)&them.sin_addr),ntohs(them.SIN_PORT));
		sprintf(abuf+strlen(abuf), "%s %u",
			inetntoa((char *)&us.sin_addr), ntohs(us.SIN_PORT));
#  endif
		if (sendto_iauth(abuf) == 0)
		    {
			close(cptr->authfd);
			cptr->authfd = -1;
			cptr->flags |= FLAGS_XAUTH;
			return;
		    }
	    }
# endif
# ifdef INET6
	Debug((DEBUG_NOTICE,"auth(%x) from %s %x %x",
	       cptr, inet_ntop(AF_INET6, (char *)&us.sin6_addr, mydummy,
			       MYDUMMY_SIZE), us.sin6_addr.s6_addr[14],
	       us.sin6_addr.s6_addr[15]));
# else
	Debug((DEBUG_NOTICE,"auth(%x) from %s",
	       cptr, inetntoa((char *)&us.sin_addr)));
# endif
	them.SIN_PORT = htons(113);
	us.SIN_PORT = htons(0);  /* bind assigns us a port */
	if (bind(cptr->authfd, (struct SOCKADDR *)&us, ulen) >= 0)
	    {
		(void)getsockname(cptr->fd, (struct SOCKADDR *)&us, &ulen);
# ifdef INET6
		Debug((DEBUG_NOTICE,"auth(%x) to %s",
			cptr, inet_ntop(AF_INET6, (char *)&them.sin6_addr,
					mydummy, MYDUMMY_SIZE)));
# else
		Debug((DEBUG_NOTICE,"auth(%x) to %s",
			cptr, inetntoa((char *)&them.sin_addr)));
# endif
		(void)alarm((unsigned)4);
		if (connect(cptr->authfd, (struct SOCKADDR *)&them,
			    tlen) == -1 && errno != EINPROGRESS)
		    {
# ifdef INET6
			Debug((DEBUG_ERROR,
				"auth(%x) connect failed to %s - %d", cptr,
				inet_ntop(AF_INET6, (char *)&them.sin6_addr,
					  mydummy, MYDUMMY_SIZE), errno));
# else
			Debug((DEBUG_ERROR,
				"auth(%x) connect failed to %s - %d", cptr,
				inetntoa((char *)&them.sin_addr), errno));
# endif
			ircstp->is_abad++;
			/*
			 * No error report from this...
			 */
			(void)alarm((unsigned)0);
			(void)close(cptr->authfd);
			cptr->authfd = -1;
			return;
		    }
		(void)alarm((unsigned)0);
	    }
	else
	    {
		report_error("binding stream socket for auth request %s:%s",
			     cptr);
# ifdef INET6
		Debug((DEBUG_ERROR,"auth(%x) bind failed on %s port %d - %d",
		      cptr, inet_ntop(AF_INET6, (char *)&us.sin6_addr,
		      mydummy, MYDUMMY_SIZE),
		      ntohs(us.SIN_PORT), errno));
# else
		Debug((DEBUG_ERROR,"auth(%x) bind failed on %s port %d - %d",
		      cptr, inetntoa((char *)&us.sin_addr),
		      ntohs(us.SIN_PORT), errno));
# endif
	    }

	cptr->flags |= (FLAGS_WRAUTH|FLAGS_AUTH);
	if (cptr->authfd > highest_fd)
		highest_fd = cptr->authfd;
#endif
	return;
}

/*
 * send_authports
 *
 * Send the ident server a query giving "theirport , ourport".
 * The write is only attempted *once* so it is deemed to be a fail if the
 * entire write doesn't write all the data given.  This shouldnt be a
 * problem since the socket should have a write buffer far greater than
 * this message to store it in should problems arise. -avalon
 */
void	send_authports(cptr)
aClient	*cptr;
{
	struct	SOCKADDR_IN	us, them;

	char	authbuf[32];
	SOCK_LEN_TYPE ulen, tlen;

	Debug((DEBUG_NOTICE,"write_authports(%x) fd %d authfd %d stat %d",
		cptr, cptr->fd, cptr->authfd, cptr->status));
	tlen = ulen = sizeof(us);
	if (getsockname(cptr->fd, (struct SOCKADDR *)&us, &ulen) ||
	    getpeername(cptr->fd, (struct SOCKADDR *)&them, &tlen))
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "auth get{sock,peer}name error for %s:%m",
			get_client_name(cptr, TRUE));
#endif
		goto authsenderr;
	    }

	SPRINTF(authbuf, "%u , %u\r\n",
		(unsigned int)ntohs(them.SIN_PORT),
		(unsigned int)ntohs(us.SIN_PORT));

#ifdef INET6
	Debug((DEBUG_SEND, "sending [%s] to auth port %s.113",
		authbuf, inet_ntop,(AF_INET6, (char *)&them.sin6_addr,
				    mydummy, MYDUMMY_SIZE)));
#else
	Debug((DEBUG_SEND, "sending [%s] to auth port %s.113",
		authbuf, inetntoa((char *)&them.sin_addr)));
#endif
	if (write(cptr->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
	    {
authsenderr:
		ircstp->is_abad++;
		(void)close(cptr->authfd);
		if (cptr->authfd == highest_fd)
			while (!local[highest_fd])
				highest_fd--;
		cptr->authfd = -1;
		cptr->flags &= ~(FLAGS_AUTH|FLAGS_WRAUTH);
		return;
	    }
	cptr->flags &= ~FLAGS_WRAUTH;
	return;
}

/*
 * read_authports
 *
 * read the reply (if any) from the ident server we connected to.
 * The actual read processijng here is pretty weak - no handling of the reply
 * if it is fragmented by IP.
 */
void	read_authports(cptr)
Reg	aClient	*cptr;
{
	Reg	char	*s, *t;
	Reg	int	len;
	char	ruser[513], system[8];
	u_short	remp = 0, locp = 0;

	*system = *ruser = '\0';
	Debug((DEBUG_NOTICE,"read_authports(%x) fd %d authfd %d stat %d",
		cptr, cptr->fd, cptr->authfd, cptr->status));
	/*
	 * Nasty.  Can't allow any other reads from client fd while we're
	 * waiting on the authfd to return a full valid string.  Use the
	 * client's input buffer to buffer the authd reply.
	 * Oh. this is needed because an authd reply may come back in more
	 * than 1 read! -avalon
	 */
	if ((len = read(cptr->authfd, cptr->buffer + cptr->count,
			sizeof(cptr->buffer) - 1 - cptr->count)) >= 0)
	    {
		cptr->count += len;
		cptr->buffer[cptr->count] = '\0';
	    }

	if ((len > 0) && (cptr->count != (sizeof(cptr->buffer) - 1)) &&
	    (sscanf(cptr->buffer, "%hd , %hd : USERID : %*[^:]: %512s",
		    &remp, &locp, ruser) == 3))
	    {
		s = rindex(cptr->buffer, ':');
		*s++ = '\0';
		for (t = (rindex(cptr->buffer, ':') + 1); *t; t++)
			if (!isspace(*t))
				break;
		strncpyzt(system, t, sizeof(system));
		for (t = ruser; *s && (t < ruser + sizeof(ruser)); s++)
			if (!isspace(*s) && *s != ':')
				*t++ = *s;
		*t = '\0';
		Debug((DEBUG_INFO,"auth reply ok [%s] [%s]", system, ruser));
	    }
	else if (len != 0)
	    {
		if (!index(cptr->buffer, '\n') && !index(cptr->buffer, '\r'))
			return;
		Debug((DEBUG_ERROR,"local %d remote %d s %x",
				locp, remp, ruser));
		Debug((DEBUG_ERROR,"bad auth reply in [%s]", cptr->buffer));
		*ruser = '\0';
	    }
	(void)close(cptr->authfd);
	if (cptr->authfd == highest_fd)
		while (!local[highest_fd])
			highest_fd--;
	cptr->count = 0;
	cptr->authfd = -1;
	ClearAuth(cptr);
	if (len > 0)
		Debug((DEBUG_INFO,"ident reply: [%s]", cptr->buffer));

	if (!locp || !remp || !*ruser)
	    {
		ircstp->is_abad++;
		return;
	    }
	ircstp->is_asuc++;
	if (cptr->auth != cptr->username)/*impossible, but...*/
	    {
		istat.is_authmem -= sizeof(cptr->auth);
		istat.is_auth -= 1;
		MyFree(cptr->auth);
	    }
  	if (!strncmp(system, "OTHER", 5))
	    { /* OTHER type of identifier */
		cptr->auth = MyMalloc(strlen(ruser) + 2);
		*cptr->auth = '-';
		strcpy(cptr->auth+1, ruser);
	    }
	else
		cptr->auth = mystrdup(ruser);
	set_clean_username(cptr);
 	cptr->flags |= FLAGS_GOTID;
	Debug((DEBUG_INFO, "got username [%s]", ruser));
	return;
}
