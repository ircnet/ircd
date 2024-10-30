/************************************************************************
 *   IRC - Internet Relay Chat, common/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static const volatile char rcsid[] = "@(#)$Id: parse.c,v 1.97 2010/08/12 16:29:30 bif Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define PARSE_C
#include "s_externs.h"
#undef PARSE_C

/* max parameters accepted */
#define MPAR 15

#define _m(f) {f, 0, 0, 0L, 0L}
/* commands should be sorted by their average usage count */
/* handlers are for: server, client, oper, service, unregistered */
struct Message msgtab[] = {
{ "PRIVMSG",  2, MPAR, { _m(m_nop), _m(m_private), _m(m_private), _m(m_nop), _m(m_unreg) } },
{ "NJOIN",    2, MPAR, { _m(m_njoin), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "JOIN",     1, MPAR, { _m(m_nop), _m(m_join), _m(m_join), _m(m_nop), _m(m_unreg) } },
{ "MODE",     1, MPAR, { _m(m_mode), _m(m_mode), _m(m_mode), _m(m_nop), _m(m_unreg) } },
{ "UNICK",    7, MPAR, { _m(m_unick), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "NICK",     1, MPAR, { _m(m_nick), _m(m_nick), _m(m_nick), _m(m_nop), _m(m_nick) } },
{ "PART",     1, MPAR, { _m(m_part), _m(m_part), _m(m_part), _m(m_nop), _m(m_unreg) } },
{ "QUIT",     0, MPAR, { _m(m_quit), _m(m_quit), _m(m_quit), _m(m_quit), _m(m_quit) } },
{ "NOTICE",   2, MPAR, { _m(m_notice), _m(m_notice), _m(m_notice), _m(m_notice), _m(m_unreg) } },
{ "KICK",     2, MPAR, { _m(m_kick), _m(m_kick), _m(m_kick), _m(m_nop), _m(m_unreg) } },
{ "SERVER",   2, MPAR, { _m(m_server), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_server) } },
{ "SMASK",    2, MPAR, { _m(m_smask), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "TRACE",    0, MPAR, { _m(m_trace), _m(m_trace), _m(m_trace), _m(m_trace), _m(m_unreg) } },
{ "TOPIC",    1, MPAR, { _m(m_nop), _m(m_topic), _m(m_topic), _m(m_nop), _m(m_unreg) } },
{ "INVITE",   2, MPAR, { _m(m_nop), _m(m_invite), _m(m_invite), _m(m_nop), _m(m_unreg) } },
{ "WALLOPS",  1, MPAR, { _m(m_wallops), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "PING",     1, MPAR, { _m(m_ping), _m(m_ping), _m(m_ping), _m(m_ping), _m(m_unreg) } },
{ "PONG",     1, MPAR, { _m(m_pong), _m(m_pong), _m(m_pong), _m(m_pong), _m(m_unreg) } },
{ "ERROR",    1, MPAR, { _m(m_error), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "KILL",     2, MPAR, { _m(m_kill), _m(m_nopriv), _m(m_kill), _m(m_nop), _m(m_unreg) } },
{ "SAVE",     1, MPAR, { _m(m_save), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "USER",     4, MPAR, { _m(m_nop), _m(m_reg), _m(m_reg), _m(m_nop), _m(m_user) } },
{ "CAP",      1, MPAR, { _m(m_nop), _m(m_cap), _m(m_cap), _m(m_nop), _m(m_cap) } },
{ "AWAY",     0, MPAR, { _m(m_nop), _m(m_away), _m(m_away), _m(m_nop), _m(m_unreg) } },
{ "UMODE",    1, MPAR, { _m(m_nop), _m(m_umode), _m(m_umode), _m(m_nop), _m(m_unreg) } },
{ "ISON",     1,    1, { _m(m_ison), _m(m_ison), _m(m_ison), _m(m_ison), _m(m_unreg) } },
{ "SQUIT",    2, MPAR, { _m(m_squit), _m(m_nopriv), _m(m_squit), _m(m_nop), _m(m_unreg) } },
{ "WHOIS",    1, MPAR, { _m(m_whois), _m(m_whois), _m(m_whois), _m(m_whois), _m(m_unreg) } },
{ "WHO",      1, MPAR, { _m(m_who), _m(m_who), _m(m_who), _m(m_who), _m(m_unreg) } },
{ "WHOWAS",   1, MPAR, { _m(m_whowas), _m(m_whowas), _m(m_whowas), _m(m_whowas), _m(m_unreg) } },
{ "LIST",     0, MPAR, { _m(m_list), _m(m_list), _m(m_list), _m(m_list), _m(m_unreg) } },
{ "NAMES",    0, MPAR, { _m(m_nop), _m(m_names), _m(m_names), _m(m_nop), _m(m_unreg) } },
{ "USERHOST", 1, MPAR, { _m(m_userhost), _m(m_userhost), _m(m_userhost), _m(m_userhost), _m(m_unreg) } },
{ "PASS",     1, MPAR, { _m(m_nop), _m(m_reg), _m(m_reg), _m(m_nop), _m(m_pass) } },
{ "LUSERS",   0, MPAR, { _m(m_lusers), _m(m_lusers), _m(m_lusers), _m(m_lusers), _m(m_unreg) } },
{ "TIME",     0, MPAR, { _m(m_nop), _m(m_time), _m(m_time), _m(m_nop), _m(m_unreg) } },
{ "OPER",     2, MPAR, { _m(m_nop), _m(m_oper), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "CONNECT",  1, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_connect), _m(m_nop), _m(m_unreg) } },
{ "VERSION",  0, MPAR, { _m(m_nop), _m(m_version), _m(m_version), _m(m_nop), _m(m_unreg) } },
{ "STATS",    1, MPAR, { _m(m_nop), _m(m_stats), _m(m_stats), _m(m_nop), _m(m_unreg) } },
{ "LINKS",    0, MPAR, { _m(m_links), _m(m_links), _m(m_links), _m(m_links), _m(m_unreg) } },
{ "ADMIN",    0, MPAR, { _m(m_admin), _m(m_admin), _m(m_admin), _m(m_admin), _m(m_admin) } },
{ "USERS",    0, MPAR, { _m(m_nop), _m(m_users), _m(m_users), _m(m_users), _m(m_unreg) } },
{ "SUMMON",   0, MPAR, { _m(m_nop), _m(m_summon), _m(m_summon), _m(m_nop), _m(m_unreg) } },
{ "HELP",     0, MPAR, { _m(m_nop), _m(m_help), _m(m_help), _m(m_nop), _m(m_unreg) } },
{ "INFO",     0, MPAR, { _m(m_nop), _m(m_info), _m(m_info), _m(m_nop), _m(m_unreg) } },
{ "MOTD",     0, MPAR, { _m(m_nop), _m(m_motd), _m(m_motd), _m(m_nop),
#ifdef MOTD_UNREG
									_m(m_motd)
#else
									_m(m_unreg)
#endif
									} },
{ "CLOSE",    0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_close), _m(m_nop), _m(m_unreg) } },
{ "SERVICE",  4, MPAR, { _m(m_service), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_service) } },
{ "EOB",      0, MPAR, { _m(m_eob), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "EOBACK",   0, MPAR, { _m(m_eoback), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
{ "ENCAP",    2, MPAR, { _m(m_encap), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_nop) } },
{ "SDIE",     0, MPAR, { _m(m_sdie), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_unreg) } },
#ifdef	USE_SERVICES
{ "SERVSET",  1, MPAR, { _m(m_nop), _m(m_nop), _m(m_nop), _m(m_servset), _m(m_nop) } },
#endif
{ "SQUERY",   2, MPAR, { _m(m_nop), _m(m_squery), _m(m_squery), _m(m_nop), _m(m_unreg) } },
{ "SERVLIST", 0, MPAR, { _m(m_servlist), _m(m_servlist), _m(m_servlist), _m(m_nop), _m(m_unreg) } },
{ "HAZH",     0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_hash), _m(m_nop), _m(m_nop) } },
{ "DNS",      0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_dns), _m(m_nop), _m(m_nop) } },
{ "REHASH",   0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_rehash), _m(m_nop), _m(m_unreg) } },
{ "RESTART",  0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_restart), _m(m_nop), _m(m_unreg) } },
{ "DIE",      0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_die), _m(m_nop), _m(m_unreg) } },
{ "SET",      0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_set), _m(m_nop), _m(m_unreg) } },
{ "MAP",      0, MPAR, { _m(m_map), _m(m_map), _m(m_map), _m(m_nop), _m(m_unreg) } },
{ "POST",     0, MPAR, { _m(m_nop), _m(m_nop), _m(m_nop), _m(m_nop), _m(m_post) } },
#ifdef TKLINE
{ "TKLINE",   3, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_tkline), _m(m_tkline), _m(m_unreg) } },
{ "UNTKLINE", 1, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_untkline), _m(m_untkline), _m(m_unreg) } },
#endif
#ifdef KLINE
{ "KLINE",    2, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_kline), _m(m_kline), _m(m_unreg) } },
#endif
{ "ETRACE",   0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_etrace), _m(m_nop), _m(m_unreg) } },
#ifdef ENABLE_SIDTRACE
{ "SIDTRACE", 0, MPAR, { _m(m_nop), _m(m_nopriv), _m(m_sidtrace), _m(m_nop), _m(m_unreg) } },
#endif
{ NULL,       0,    0, { _m(NULL), _m(NULL), _m(NULL), _m(NULL), _m(NULL) } }
};
#undef _m

/*
 * NOTE: parse() should not be called recursively by other functions!
 */
static	char	*para[MPAR+1];

static	char	sender[HOSTLEN+1];
static	int	cancel_clients (aClient *, aClient *, char *);
static	void	remove_unknown (aClient *, char *);

static	int	find_sender (aClient *cptr, aClient **sptr, char *sender,
			char *buffer);

/*
**  Find a client (server or user) by name.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient	*find_client(char *name, aClient *cptr)
{
	aClient *acptr = cptr;

	if (name && *name)
		acptr = hash_find_client(name, cptr);

	return acptr;
}

aClient	*find_uid(char *uid, aClient *cptr)
{
	aClient *acptr = cptr;

	if (uid && isdigit(*uid))
		acptr = hash_find_uid(uid, cptr);

	return acptr;
}

aClient	*find_sid(char *sid, aClient *cptr)
{
	if (sid && isdigit(*sid))
	{
		cptr = hash_find_sid(sid, cptr);
	}

	return cptr;
}

aClient	*find_service(char *name, aClient *cptr)
{
	aClient *acptr = cptr;

	if (index(name, '@'))
		acptr = hash_find_client(name, cptr);
	return acptr;
}

aClient	*find_matching_client(char *mask)
{
	aClient *acptr;
	aServer *asptr;
	aService *sp;
	char *ch;
	int wild = 0, dot = 0;
	
	/* try to find exact match */	
	acptr = find_client(mask, NULL);
	
	if (acptr)
	{
		return acptr;
	}
	/* check if we should check against wilds */
	for (ch = mask; *ch; ch++)
	{
		if (*ch == '*' || *ch == '?')
		{
			wild = 1;
			break;
		}
		if (*ch == '.')
		{
			dot = 1;
			break;
		}
	}
	
	if (!wild && !dot)
	{
		return NULL;
	}
	
	(void) collapse(mask);
	
	/* try to match some servername against mask */
	/* start from bottom, from ME, to return ourselves first */
	for (asptr = me.serv; asptr; asptr = asptr->prevs)
	{
		if (!match(asptr->bcptr->name, mask) ||
		    !match(mask, asptr->bcptr->name))
		{
			acptr = asptr->bcptr;
			return acptr->serv->maskedby;
		}
	}
	/* no match, try services */
	for (sp = svctop; sp; sp = sp->nexts)
	{
		if (!match(sp->bcptr->name, mask) ||
		    !match(mask, sp->bcptr->name))
		{
			acptr = sp->bcptr;
			return acptr;	
		}
	}
	return NULL;
}

/*
**  Find a user@host (server or user).
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient	*find_userhost(char *user, char *host, aClient *cptr, int *count)
{
	Reg	aClient	*c2ptr;
	Reg	aClient	*res = cptr;

	*count = 0;
	if (user)
	{
		if (host)
		{
			anUser *auptr;
#ifdef USE_HOSTHASH
			for (auptr = hash_find_hostname(host, NULL); auptr;
					auptr = auptr->hhnext)
			{
				if (MyConnect(auptr->bcptr)
				    && !mycmp(user, auptr->username))
				{
					if (++(*count) > 1)
					{
						/* We already failed
						 * - just return */
						return res;
					}
					res = auptr->bcptr;
				}
			}
#endif
#ifdef USE_IPHASH
#ifdef USE_HOSTHASH
			if (!res)
#endif
			for (auptr = hash_find_ip(host, NULL); auptr;
					auptr = auptr->iphnext)
			{
				if (MyConnect(auptr->bcptr)
				    && !mycmp(user, auptr->username))
				{
					if (++(*count) > 1)
					{
						/* We already failed
						 * - just return */
						return res;
					}
					res = auptr->bcptr;
				}
			}
#endif
		}
		else
		{
			int i;
			for (i = 0; i <= highest_fd; i++)
			{
				if (!(c2ptr = local[i])
				      || !IsRegisteredUser(c2ptr))
				{
					continue;
				}
				if (!mycmp(user, c2ptr->user->username))
				{
					if (++(*count) > 1)
					{
					/* Already failed, just return */
						return res;
					}
					res = c2ptr;
				}
			}
	    }
	}
	return res;
}


/*
**  Find server by name.
**
**	This implementation assumes that server and user names
**	are unique, no user can have a server name and vice versa.
**	One should maintain separate lists for users and servers,
**	if this restriction is removed.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string.
*/

/*
** Find a server from hash table, given its name
*/
aClient	*find_server(char *name, aClient *cptr)
{
	aClient *acptr = cptr;

	if (name && *name)
		acptr = hash_find_server(name, cptr);
	return acptr;
}

/*
** Given a server name, find the server mask behind which the server
** is hidden.
*/
aClient	*find_mask(char *name, aClient *cptr)
{
	static	char	servermask[HOSTLEN+1];
	Reg	aClient	*c2ptr = cptr;
	Reg	char	*mask = servermask;

	if (!name || !*name)
		return c2ptr;
	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (index(name, '*'))
		return c2ptr;
	strcpy (servermask, name);
	while (*mask)
	{
		if (*(mask+1) == '.')
		{
			*mask = '*';
			if ((c2ptr = hash_find_server(mask, cptr)))
				return (c2ptr);
		}
		mask++;
	}
	return (c2ptr ? c2ptr : cptr);
}

/*
** Find a server, given its name (which might contain *'s, in which case
** the first match will be return [not the best one])
*/
aClient	*find_name(char *name, aClient *cptr)
{
	Reg	aClient	*c2ptr = cptr;
	Reg	aServer	*sp = NULL;

	if (!name || !*name)
		return c2ptr;

	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (!index(name, '*'))
		return c2ptr;
	for (sp = svrtop; sp; sp = sp->nexts)
	    {
		/*
		** A server present in the list necessarily has a non NULL
		** bcptr pointer.
		*/
		if (match(name, sp->bcptr->name) == 0)
			break;
		if (index(sp->bcptr->name, '*'))
			if (match(sp->bcptr->name, name) == 0)
					break;
	    }
	return (sp ? sp->bcptr : cptr);
}

/*
**  Find person by (nick)name.
*/
aClient	*find_person(char *name, aClient *cptr)
{
	Reg	aClient	*c2ptr = cptr;

	c2ptr = find_client(name, c2ptr);

	if (c2ptr && IsClient(c2ptr) && c2ptr->user)
		return c2ptr;
	else
		return cptr;
}

/*
** find_sender(): 
** Find the client structure for the sender of the message we got from cptr
** and checks it to be valid.
** Stores the result in *sptr.
** Returns:
**	 1 on success.
**	 0 when we removed a remote client.
**	-1 when coming from a wrong server (wrong direction).
**	-2 (FLUSH_BUFFER) when we removed a local client (server).
**	-3 when client not found.
*/
static	int	find_sender(aClient *cptr, aClient **sptr, char *sender,
			char *buffer)
{
	aClient *from = NULL;

	if (IsServer(cptr))
	{
		if (isdigit(*sender))
		{
			if (strlen(sender) == SIDLEN)
			{
				/* SID */
				from = find_sid(sender, NULL);
			}
			else
			{
				/* UID */
				from = find_uid(sender, NULL);
			}
		}
	}
	if (!from)
	{
		from = find_client(sender, (aClient *) NULL);
		if (!from ||
			/*
			** I really believe that the followin line is 
			** useless.  What a waste, especially with 2.9
			** hostmasks.. at least the test on from->name
			** will make it a bit better. -krys
			*/
			(*from->name == '*' && match(from->name, sender)))
		{
			from = find_server(sender, (aClient *)NULL);
		}
	}
	/* Is there svc@server prefix ever? -Vesa */
	/* every time a service talks -krys */
	if (!from && index(sender, '@'))
	{
		from = find_service(sender, (aClient *)NULL);
	}
	if (!from)
	{
		from = find_mask(sender, (aClient *) NULL);
	}
	if (from && isdigit(sender[0]))
	{
		para[0] = from->name;
	}
	else
	{
		para[0] = sender;
	}

	/* Hmm! If the client corresponding to the
	** prefix is not found--what is the correct
	** action??? Now, I will ignore the message
	** (old IRC just let it through as if the
	** prefix just wasn't there...) --msa
	** Since 2.9 we pick them up and .. --Vesa
	*/
	if (!from)
	{
		Debug((DEBUG_ERROR,
			"Unknown prefix (%s)(%s) from (%s)",
			sender, buffer, cptr->name));
		ircstp->is_unpf++;
		remove_unknown(cptr, sender);
		return -3;	/* Grab it in read_message() */
	}
	if (from->from != cptr)
	{
		ircstp->is_wrdi++;
		Debug((DEBUG_ERROR,
			"Message (%s) coming from (%s)",
			buffer, cptr->name));
		return cancel_clients(cptr, from, buffer);
	}
	*sptr = from;
	return 1;
}

/* find target.
**  name - name of the client to be searched
**  cptr - originating socket
*/
aClient	*find_target(char *name, aClient *cptr)
{
	aClient *acptr = NULL;
	
	if (IsServer(cptr))
	{
		if (isdigit(name[0]))
		{
			if (name[SIDLEN] == '\0')
			{
				acptr = find_sid(name, NULL);
			}
			else
			{
				acptr = find_uid(name, NULL);
			}
		}
	}
	if (!acptr)
	{
		acptr = find_client(name, NULL);
		if (!acptr)
		{
			acptr = find_server(name, NULL);
		}
		if (!acptr && !match(name, ME))
		{
			/* Matches when the target is "*.ourmask" which
			 * is not handled by above functions.
			 */
			acptr = &me;
		}
	}
	return acptr;
}

/*
 * parse a buffer.
 * Return values:
 *  errors: -3 for unknown origin/sender, -2 for FLUSH_BUFFER, -1 for bad cptr
 *
 * NOTE: parse() should not be called recusively by any other fucntions!
 */
int	parse(aClient *cptr, char *buffer, char *bufend)
{
	aClient *from = cptr;
	Reg	char	*ch, *s;
	Reg	int	len, i, numeric = 0, paramcount;
	Reg	struct	Message *mptr = NULL;
	int	ret;
	int	status = STAT_UNREG;
	struct Cmd	*handler;
	CmdHandler	fhandler = m_nop;

	Debug((DEBUG_DEBUG, "Parsing %s: %s",
		get_client_name(cptr, FALSE), buffer));
	if (IsDead(cptr))
		return -1;

	s = sender;
	*s = '\0';
	for (ch = buffer; *ch == ' '; ch++)
		;
	para[0] = from->name;
	if (*ch == ':')
	    {
		/*
		** Copy the prefix to 'sender' assuming it terminates
		** with SPACE (or NULL, which is an error, though).
		*/
		for (++ch; *ch && *ch != ' '; ++ch )
			if (s < (sender + sizeof(sender)-1))
				*s++ = *ch; /* leave room for NULL */
		*s = '\0';
		/*
		** Actually, only messages coming from servers can have
		** the prefix--prefix silently ignored, if coming from
		** a user client...
		**
		** ...sigh, the current release "v2.2PL1" generates also
		** null prefixes, at least to NOTIFY messages (e.g. it
		** puts "sptr->nickname" as prefix from server structures
		** where it's null--the following will handle this case
		** as "no prefix" at all --msa  (": NOTICE nick ...")
		*/
		if (*sender && IsServer(cptr))
		{
			i = find_sender(cptr, &from, sender, buffer);
			if (i <= 0)
			{
				return i;
			}
		}
		while (*ch == ' ')
			ch++;
	    }
	if (*ch == '\0')
	    {
		ircstp->is_empt++;
		Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
		      cptr->name, from->name));
		return -1;
	    }
	/*
	** Extract the command code from the packet.  Point s to the end
	** of the command code and calculate the length using pointer
	** arithmetic.  Note: only need length for numerics and *all*
	** numerics must have paramters and thus a space after the command
	** code. -avalon
	*/
	s = (char *)index(ch, ' '); /* s -> End of the command code */
	len = (s) ? (s - ch) : 0;
	if (len == 3 &&
	    isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2)))
	    {
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10
			+ (*(ch + 2) - '0');
		paramcount = MPAR;
		ircstp->is_num++;
	    }
	else
	    {
		if (s)
			*s++ = '\0';
		for (mptr = msgtab; mptr->cmd; mptr++) 
			if (mycmp(mptr->cmd, ch) == 0)
				break;

		if (!mptr->cmd)
		    {
			/*
			** Note: Give error message *only* to recognized
			** persons. It's a nightmare situation to have
			** two programs sending "Unknown command"'s or
			** equivalent to each other at full blast....
			** If it has got to person state, it at least
			** seems to be well behaving. Perhaps this message
			** should never be generated, though...  --msa
			** Hm, when is the buffer empty -- if a command
			** code has been found ?? -Armin
			*/
			if (buffer[0] != '\0')
			    {
				cptr->flags |= FLAGS_UNKCMD;
				if (IsPerson(from) || IsService(from))
					sendto_one(from,
					    ":%s %d %s %s :Unknown command",
					    me.name, ERR_UNKNOWNCOMMAND,
					    from->name, ch);
				else if (IsServer(cptr))
					sendto_flag(SCH_ERROR,
					    "Unknown command from %s:%s",
					    get_client_name(cptr, TRUE), ch);
				Debug((DEBUG_ERROR,"Unknown (%s) from %s",
					ch, get_client_name(cptr, TRUE)));
			    }
			ircstp->is_unco++;
			return -1;
		    }
		paramcount = mptr->maxparams;
		i = bufend - ((s) ? s : ch);
		status = from->status < STAT_SERVER ? STAT_UNREG : from->status;
		handler = &(mptr->handlers[status]);
		fhandler = handler->handler;
		handler->count++;
		handler->bytes += i;
		if (!MyConnect(from))
		{
			handler->rcount++;
			handler->rbytes += i;
		}
		if (!(IsServer(cptr) || IsService(cptr)))
		    {	/* Flood control partly migrated into penalty */
			if ((bootopt & BOOT_PROT) &&
				!is_allowed(cptr, ACL_NOPENALTY))
				cptr->since += (1 + i / 100);
			else
				cptr->since = timeofday;
			/* Allow only 1 msg per 2 seconds
			 * (on average) to prevent dumping.
			 * to keep the response rate up,
			 * bursts of up to 5 msgs are allowed
			 * -SRB
			 */
		    }
	    }
	/*
	** Must the following loop really be so devious? On
	** surface it splits the message to parameters from
	** blank spaces. But, if paramcount has been reached,
	** the rest of the message goes into this last parameter
	** (about same effect as ":" has...) --msa
	*/

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

	i = 0;
	if (s)
	    {
		if (paramcount > MPAR)
			paramcount = MPAR;
		for (;;)
		    {
			/*
			** Never "FRANCE " again!! ;-) Clean
			** out *all* blanks.. --msa
			*/
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			    {
				/*
				** The rest is single parameter--can
				** include blanks also.
				*/
				para[++i] = s + 1;
				break;
			    }
			para[++i] = s;
			if (i >= paramcount-1)
				break;
			for (; *s != ' ' && *s; s++)
				;
		    }
	    }
	para[++i] = NULL; /* at worst, ++i is paramcount (MPAR) */
	if (mptr == NULL)
		return (do_numeric(numeric, cptr, from, i, para));
	if (IsRegisteredUser(cptr) &&
#ifdef	IDLE_FROM_MSG
	    fhandler == m_private)
#else
	    fhandler != m_ping && fhandler != m_pong)
#endif
		from->user->last = timeofday;
	Debug((DEBUG_DEBUG, "Function(%d): %#x = %s parc %d parv %#x",
		status, fhandler, mptr->cmd, i, para));
	if (fhandler != m_nop && fhandler != m_nopriv
		&& fhandler != m_unreg &&
		mptr->minparams > 0 && 
		(i <= mptr->minparams || para[mptr->minparams][0] == '\0'))
	{
		if (status == STAT_SERVER)
		{
			char	rbuf[BUFSIZE];

			sprintf(rbuf, "%s: Not enough parameters", mptr->cmd);
			ret = exit_client(cptr, cptr, &me, rbuf);
		}
		else
		{
			sendto_one(from, replies[ERR_NEEDMOREPARAMS], 
				ME, BadTo(para[0]), mptr->cmd);
			ret = 1;
		}
	}
	else
	{
		/*
		** ALL m_functions return now UNIFORMLY:
		**   -2  old FLUSH_BUFFER return value (unchanged).
		**   -1  if parsing of a protocol message leads in a syntactic/semantic
		**       error and NO penalty scoring should be applied.
		**   >=0 if protocol message processing was successful. The return
		**       value indicates the penalty score.
		*/
		ret = (*fhandler)(cptr, from, i, para);
	}
	/*
        ** Add penalty score for sucessfully parsed command if issued by
	** a LOCAL user client.
	*/
	if ((ret > 0) && IsRegisteredUser(cptr) && (bootopt & BOOT_PROT)
		&& !is_allowed(cptr, ACL_NOPENALTY))
	    {
		cptr->since += ret;
/* only to lurk
		sendto_one(cptr,
			   ":%s NOTICE %s :*** Penalty INCR [%s] +%d",
			   me.name, cptr->name, ch, ret);
*/
	    }
	return (ret != FLUSH_BUFFER) ? 2 : FLUSH_BUFFER;
}

/*
 * field breakup for ircd.conf file.
 */
char	*getfield(char *irc_newline)
{
	static	char *line = NULL;
	char	*end, *field;
	
	if (irc_newline)
		line = irc_newline;
	if (line == NULL)
		return(NULL);

	field = line;

	end = index(line, IRCDCONF_DELIMITER);
	if (end == line)
	{ /* empty */
		line++;
	}
	else
	{
		for (;;)
		{
			if (!end)
			{
				/* we can't find delimiter at the end of
				 * this field. (probably last one)
				 */
				break;
			}
			if (*(end - 1) != '\\')
			{ /* not escaped delimiter */
				break;
			}
			else
			{ /* escaped one, dequote */
				char *s;
				if (*(end+1) == '\0')
					break;
				for (s = (end - 1); (*s = *(s+1)) ;s++);
				end++;
				end = index(end, IRCDCONF_DELIMITER);
			}
		}
		if (!end)
		{
			line = NULL;
			if ((end = (char *)index(field, '\n')) == NULL)
				end = field + strlen(field);
		}
		else
		{
			line = end + 1;
		}
	}
	*end = '\0';
	return(field);
}

static	int	cancel_clients(aClient *cptr, aClient *sptr, char *cmd)
{
	/*
	 * kill all possible points that are causing confusion here,
	 * I'm not sure I've got this all right...
	 * - avalon
	 */
	sendto_flag(SCH_NOTICE, "Message (%s) for %s[%s!%s@%s] from %s",
		    cmd, sptr->name, sptr->from->name, sptr->from->username,
		    sptr->from->sockhost, get_client_name(cptr, TRUE));
	/*
	 * Incorrect prefix for a server from some connection.  If it is a
	 * client trying to be annoying, just QUIT them, if it is a server
	 * then the same deal.
	 */
	if (IsServer(sptr) || IsMe(sptr))
	    {
		sendto_flag(SCH_NOTICE, "Dropping server %s",cptr->name);
		return exit_client(cptr, cptr, &me, "Fake Direction");
	    }
	/*
	 * Ok, someone is trying to impose as a client and things are
	 * confused.  If we got the wrong prefix from a server, send out a
	 * kill, else just exit the lame client.
	 */
	if (IsServer(cptr))
	    {
		sendto_serv_butone(NULL, ":%s KILL %s :%s (%s[%s] != %s)",
				   me.name,
				   sptr->user ? sptr->user->uid : sptr->name,
				   me.name, sptr->name, sptr->from->name,
				   get_client_name(cptr, TRUE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Fake Prefix");
	    }
	return exit_client(cptr, cptr, &me, "Fake prefix");
}

static	void	remove_unknown(aClient *cptr, char *sender)
{
	if (!IsRegistered(cptr) || IsClient(cptr))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(cptr))
		return;
	/*
	 * squit if it is a server because it means something is really
	 * wrong.
	 */
	/* Trying to find out if it's server prefix (contains '.' but no '@'
	 * (services) or is valid SID. --B. */
	if ((index(sender, '.') && !index(sender, '@'))
		|| sid_valid(sender))
	    {
		sendto_flag(SCH_NOTICE, "Squitting unknown %s brought by %s.",
			    sender, get_client_name(cptr, FALSE));
		sendto_one(cptr, ":%s SQUIT %s :(Unknown from %s)",
			   me.name, sender, get_client_name(cptr, FALSE));
	    }
	else
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 * it can simply be caused by lag (among other things), so just
	 * drop it if it is not a server. -krys
	 * services aren't prone to collisions, so lag shouldn't be responsible
	 * if we get here and sender is a service, we should probably issue
	 * a kill in this case! -krys
	 */
		sendto_flag(SCH_NOTICE, "Dropping unknown %s brought by %s.",
			    sender, get_client_name(cptr, FALSE));
}

int	m_nop(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	return 1;
}

int	m_nopriv(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	sendto_one(sptr, replies[ERR_NOPRIVILEGES], ME, parv[0]);
	return 1;
}

int	m_unreg(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	sendto_one(sptr, replies[ERR_NOTREGISTERED], ME, "*");
	return -1;
}

int	m_reg(aClient *cptr, aClient *sptr, int parc, char **parv)
{
	sendto_one(sptr, replies[ERR_ALREADYREGISTRED], ME, parv[0]);
	return 1;
}
