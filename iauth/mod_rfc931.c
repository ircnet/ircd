/************************************************************************
 *   IRC - Internet Relay Chat, iauth/mod_rfc931.c
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
static  char rcsid[] = "@(#)$Id: mod_rfc931.c,v 1.16 1999/07/11 20:56:25 chopin Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define MOD_RFC931_C
#include "a_externs.h"
#undef MOD_RFC931_C

#define OPT_PROTOCOL	0x1
#define OPT_LAZY	0x2

struct _data
{
	u_char	options;
	u_int	tried;
	u_int	connected;
	u_int	unx;
	u_int	other;
	u_int	bad;
	u_int	skipped;
	u_int	clean, timeout;
};

/*
 * rfc931_init
 *
 *	This procedure is called when a particular module is loaded.
 *	Returns NULL if everything went fine,
 *	an error message otherwise.
 */
char *
rfc931_init(self)
AnInstance *self;
{
	struct _data *dt;

	dt = (struct _data *) malloc(sizeof(struct _data));
	bzero((char *) dt, sizeof(struct _data));
	self->data = (void *) dt;

	/* undocumented option */
	if (self->opt && strstr(self->opt, "protocol"))
		dt->options |= OPT_PROTOCOL;
	if (self->opt && strstr(self->opt, "lazy"))
		dt->options |= OPT_LAZY;

	if (dt->options & (OPT_LAZY|OPT_PROTOCOL))
		self->popt = "protocol,lazy";
	else if (dt->options & OPT_LAZY)
		self->popt = "lazy";
	else if (dt->options & OPT_PROTOCOL)
		self->popt = "protocol";
	else
		return NULL;
	return self->popt;
}

/*
 * rfc931_release
 *
 *	This procedure is called when a particular module is unloaded.
 */
void
rfc931_release(self)
AnInstance *self;
{
	struct _data *st = self->data;
	free(st);
}

/*
 * rfc931_stats
 *
 *	This procedure is called regularly to update statistics sent to ircd.
 */
void
rfc931_stats(self)
AnInstance *self;
{
	struct _data *st = self->data;

	sendto_ircd("S rfc931 connected %u unix %u other %u bad %u out of %u",
		    st->connected, st->unx, st->other, st->bad, st->tried);
	sendto_ircd("S rfc931 skipped %u aborted %u / %u",
		    st->skipped, st->clean, st->timeout);
}

/*
 * rfc931_start
 *
 *	This procedure is called to start an authentication.
 *	Returns 0 if everything went fine,
 *	-1 else otherwise (nothing to be done, or failure)
 *
 *	It is responsible for sending error messages where appropriate.
 *	In case of failure, it's responsible for cleaning up (e.g. rfc931_clean
 *	will NOT be called)
 */
int
rfc931_start(cl)
u_int cl;
{
	char *error;
	int fd;
	struct _data *st = cldata[cl].instance->data;

	if (st->options & OPT_LAZY && cldata[cl].state & A_DENY)
	    {
		DebugLog((ALOG_D931, 0, "rfc931_start(%d): Lazy.", cl));
		return -1;
	    }
	if (cldata[cl].authuser &&
	    cldata[cl].authfrom < cldata[cl].instance->in)
	    {
		DebugLog((ALOG_D931, 0,
			  "rfc931_start(%d): Instance %d already got the info",
			  cl, cldata[cl].authfrom));
		return -1;
	    }
	DebugLog((ALOG_D931, 0, "rfc931_start(%d): Connecting to %s %u", cl,
		  cldata[cl].itsip, 113));
	st->tried += 1;
	fd = tcp_connect(cldata[cl].ourip, cldata[cl].itsip, 113, &error);
	if (fd < 0)
	    {
		DebugLog((ALOG_D931, 0,
			  "rfc931_start(%d): tcp_connect() reported %s",
			  cl, error));
		return -1;
	    }

	cldata[cl].wfd = fd; /*so that rfc931_work() is called when connected*/
	return 0;
}

/*
 * rfc931_work
 *
 *	This procedure is called whenever there's new data in the buffer.
 *	Returns 0 if everything went fine, and there is more work to be done,
 *	Returns -1 if the module has finished its work (and cleaned up).
 *
 *	It is responsible for sending error messages where appropriate.
 */
int
rfc931_work(cl)
u_int cl;
{
	struct _data *st = cldata[cl].instance->data;

    	DebugLog((ALOG_D931, 0, "rfc931_work(%d): %d %d buflen=%d", cl,
		  cldata[cl].rfd, cldata[cl].wfd, cldata[cl].buflen));
	if (cldata[cl].wfd > 0)
	    {
		/*
		** We haven't sent the query yet, the connection was just
		** established.
		*/
		char query[32];

		sprintf(query, "%u , %u\r\n", cldata[cl].itsport,
			cldata[cl].ourport);
		if (write(cldata[cl].wfd, query, strlen(query)) < 0)
		    {
			/* most likely the connection failed */
			DebugLog((ALOG_D931, 0,
				  "rfc931_work(%d): write() failed: %s", cl,
				  strerror(errno)));
			close(cldata[cl].wfd);
			cldata[cl].rfd = cldata[cl].wfd = 0;
			return 1;
		    }
		else
			st->connected += 1;
		cldata[cl].rfd = cldata[cl].wfd;
		cldata[cl].wfd = 0;
	    }
	else
	    {
		/* data's in from the ident server */
		char *ch;
		u_char bad = 0;

		cldata[cl].inbuffer[cldata[cl].buflen] = '\0';
		ch = index(cldata[cl].inbuffer, '\r');
		if (ch)
		    {
			/* got all of it! */
			*ch = '\0';
			DebugLog((ALOG_D931, 0, "rfc931_work(%d): Got [%s]",
				  cl, cldata[cl].inbuffer));
			if (cldata[cl].buflen > 1024)
			    cldata[cl].inbuffer[1024] = '\0';
			if (ch = index(cldata[cl].inbuffer, '\n'))
				/* delimiter for ircd<->iauth messages. */
				*ch = '\0';
			ch = cldata[cl].inbuffer;
			while (*ch && !isdigit(*ch)) ch++;
			if (!*ch || atoi(ch) != cldata[cl].itsport)
			    {
				DebugLog((ALOG_D931, 0,
					  "remote port mismatch."));
				ch = NULL;
			    }
			while (ch && *ch && *ch != ',') ch++;
			while (ch && *ch && !isdigit(*ch)) ch++;
			if (ch && (!*ch || atoi(ch) != cldata[cl].ourport))
			    {
				DebugLog((ALOG_D931, 0,
					  "local port mismatch."));
				ch = NULL;
			    }
			if (ch) ch = index(ch, ':');
			if (ch) ch += 1;
			while (ch && *ch && *ch == ' ') ch++;
			if (ch && strncmp(ch, "USERID", 6))
			    {
				DebugLog((ALOG_D931, 0, "No USERID."));
				ch = NULL;
			    }
			if (ch) ch = index(ch, ':');
			if (ch) ch += 1;
			while (ch && *ch && *ch == ' ') ch++;
			if (ch)
			    {
				int other = 0;

				if (!strncmp(ch, "OTHER", 5))
					other = 1;
				ch = rindex(ch, ':');
				if (ch) ch += 1;
				while (ch && *ch && *ch == ' ') ch++;
				if (ch && *ch)
				    {
					if (cldata[cl].authuser)
						free(cldata[cl].authuser);
					cldata[cl].authuser = mystrdup(ch);
					cldata[cl].authfrom =
						cldata[cl].instance->in;
					if (other)
						st->other += 1;
					else
					    {
						st->unx += 1;
						cldata[cl].state |= A_UNIX;
					    }
					sendto_ircd("%c %d %s %u %s",
						    (other) ? 'u' : 'U', cl,
						    cldata[cl].itsip,
						    cldata[cl].itsport,
						    cldata[cl].authuser);
				    }
				else
					bad = 1;
			    }
			else
				bad = 1;
			if (bad)
			    {
				st->bad += 1;

				if (st->options & OPT_PROTOCOL)
				    {
					ch = cldata[cl].inbuffer;
					while (*ch)
					    {
						if (!(isalnum(*ch) || 
						      ispunct(*ch) ||
						      isspace(*ch)))
							break;
						ch += 1;
					    }
					*ch = '\0';
					sendto_log(ALOG_IRCD|ALOG_FLOG,
						   LOG_WARNING,
		   "rfc931: bad reply from %s[%s] to \"%u, %u\": %u, \"%s\"",
						   cldata[cl].host,
						   cldata[cl].itsip,
						   cldata[cl].itsport,
						   cldata[cl].ourport,
						   cldata[cl].buflen,
						   cldata[cl].inbuffer);
				    }
			    }
			/*
			** In any case, our job is done, let's cleanup.
			*/
			close(cldata[cl].rfd);
			cldata[cl].rfd = 0;
			return -1;
		    }
		else
			return 0;
	    }
	return 0;
}

/*
 * rfc931_clean
 *
 *	This procedure is called whenever the module should interrupt its work.
 *	It is responsible for cleaning up any allocated data, and in particular
 *	closing file descriptors.
 */
void
rfc931_clean(cl)
u_int cl;
{
	struct _data *st = cldata[cl].instance->data;

	st->clean += 1;
	DebugLog((ALOG_D931, 0, "rfc931_clean(%d): cleaning up", cl));
	/*
	** only one of rfd and wfd may be set at the same time,
	** in any case, they would be the same fd, so only close() once
	*/
	if (cldata[cl].rfd)
		close(cldata[cl].rfd);
	else if (cldata[cl].wfd)
		close(cldata[cl].wfd);
	cldata[cl].rfd = cldata[cl].wfd = 0;
}

/*
 * rfc931_timeout
 *
 *	This procedure is called whenever the timeout set by the module is
 *	reached.
 *
 *	Returns 0 if things are okay, -1 if authentication was aborted.
 */
int
rfc931_timeout(cl)
u_int cl;
{
	struct _data *st = cldata[cl].instance->data;

	st->timeout += 1;
	DebugLog((ALOG_D931, 0, "rfc931_timeout(%d): calling rfc931_clean ",
		  cl));
	rfc931_clean(cl);
	return -1;
}

aModule Module_rfc931 =
	{ "rfc931", rfc931_init, rfc931_release, rfc931_stats,
	  rfc931_start, rfc931_work, rfc931_timeout, rfc931_clean };
