/*
 * ++Copyright++ 1985, 1989, 1993
 * -
 * Copyright (c) 1985, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const volatile char sccsid[] = "@(#)res_init.c	8.1 (Berkeley) 6/7/93";
static const volatile char rcsid[]	= "$Id: res_init.c,v 1.16 2005/01/03 22:17:00 q Exp $";
#endif /* LIBC_SCCS and not lint */

#include "os.h"
#include "s_defines.h"
#define RES_INIT_C
#include "s_externs.h"
#undef RES_INIT_C

/*-------------------------------------- info about "sortlist" --------------
 * Marc Majka		1994/04/16
 * Allan Nathanson	1994/10/29 (BIND 4.9.3.x)
 *
 * NetInfo resolver configuration directory support.
 *
 * Allow a NetInfo directory to be created in the hierarchy which
 * contains the same information as the resolver configuration file.
 *
 * - The local domain name is stored as the value of the "domain" property.
 * - The Internet address(es) of the name server(s) are stored as values
 *   of the "nameserver" property.
 * - The name server addresses are stored as values of the "nameserver"
 *   property.
 * - The search list for host-name lookup is stored as values of the
 *   "search" property.
 * - The sortlist comprised of IP address netmask pairs are stored as
 *   values of the "sortlist" property. The IP address and optional netmask
 *   should be seperated by a slash (/) or ampersand (&) character.
 * - Internal resolver variables can be set from the value of the "options"
 *   property.
 */
#if defined(NEXT)
#define NI_PATH_RESCONF "/locations/resolver"
#define NI_TIMEOUT 10
static int ircd_netinfo_res_init(int *haveenv, int *havesearch);
#endif

static void ircd_res_setoptions(char *, char *);

#ifdef RESOLVSORT
static const char sort_mask[] = "/&";
#define ISSORTMASK(ch) (strchr(sort_mask, ch) != NULL)
#ifdef INET6
static u_int32_t ircd_net_mask(struct in_addr);
#else
static u_int32_t ircd_net_mask(struct in_addr);
#endif
#endif

#if !defined(isascii) /* XXX - could be a function */
#define isascii(c) (!(c & 0200))
#endif

/*
 * Resolver state default settings.
 */

struct __res_state ircd_res
#if defined(__BIND_RES_TEXT)
		= {
				RES_TIMEOUT,
} /* Motorola, et al. */
#endif
;

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * An interrim version of this code (BIND 4.9, pre-4.4BSD) used 127.0.0.1
 * rather than INADDR_ANY ("0.0.0.0") as the default name server address
 * since it was noted that INADDR_ANY actually meant ``the first interface
 * you "ifconfig"'d at boot time'' and if this was a SLIP or PPP interface,
 * it had to be "up" in order for you to reach your own name server.  It
 * was later decided that since the recommended practice is to always 
 * install local static routes through 127.0.0.1 for all your network
 * interfaces, that we could solve this problem without a code change.
 *
 * The configuration file should always be used, since it is the only way
 * to specify a default domain.  If you are running a server on your local
 * machine, you should say "nameserver 0.0.0.0" or "nameserver 127.0.0.1"
 * in the configuration file.
 *
 * Return 0 if completes successfully, -1 on error
 */
int ircd_res_init(void)
{
	register FILE *fp;
	register char *cp, **pp;
	register int n;
	char buf[MAXDNAME];
	int nserv	   = 0; /* number of nameserver records read from file */
	int haveenv	   = 0;
	int havesearch = 0;
#ifdef RESOLVSORT
	int nsort = 0;
	char *net;
#endif
#ifndef RFC1535
	int dots;
#endif

	/*
	 * These three fields used to be statically initialized.  This made
	 * it hard to use this code in a shared library.  It is necessary,
	 * now that we're doing dynamic initialization here, that we preserve
	 * the old semantics: if an application modifies one of these three
	 * fields of _res before res_init() is called, res_init() will not
	 * alter them.  Of course, if an application is setting them to
	 * _zero_ before calling res_init(), hoping to override what used
	 * to be the static default, we can't detect it and unexpected results
	 * will follow.  Zero for any of these fields would make no sense,
	 * so one can safely assume that the applications were already getting
	 * unexpected results.
	 *
	 * _res.options is tricky since some apps were known to diddle the bits
	 * before res_init() was first called. We can't replicate that semantic
	 * with dynamic initialization (they may have turned bits off that are
	 * set in RES_DEFAULT).  Our solution is to declare such applications
	 * "broken".  They could fool us by setting RES_INIT but none do (yet).
	 */
	if (!ircd_res.retrans)
		ircd_res.retrans = RES_TIMEOUT;
	if (!ircd_res.retry)
		ircd_res.retry = 4;
	if (!(ircd_res.options & RES_INIT))
		ircd_res.options = RES_DEFAULT;

	/*
	 * This one used to initialize implicitly to zero, so unless the app
	 * has set it to something in particular, we can randomize it now.
	 */
	if (!ircd_res.id)
		ircd_res.id = ircd_res_randomid();

#ifdef INET6
#ifdef USELOOPBACK
	ircd_res.nsaddr.sin6_addr = in6addr_loopback;
#else
	ircd_res.nsaddr.sin6_addr = in6addr_any;
#endif
#else
#ifdef USELOOPBACK
	ircd_res.nsaddr.sin_addr = inet_makeaddr(IN_LOOPBACKNET, 1);
#else
	ircd_res.nsaddr.sin_addr.s_addr = INADDR_ANY;
#endif
#endif /* INET6 */
	ircd_res.nsaddr.SIN_FAMILY = AFINET;
	ircd_res.nsaddr.SIN_PORT   = htons(NAMESERVER_PORT);
	ircd_res.nscount		   = 1;
	ircd_res.ndots			   = 1;
	ircd_res.pfcode			   = 0;

	/* Allow user to override the local domain definition */
	if ((cp = getenv("LOCALDOMAIN")) != NULL)
	{
		strncpyzt(ircd_res.defdname, cp, sizeof(ircd_res.defdname));
		haveenv++;

		/*
		 * Set search list to be blank-separated strings
		 * from rest of env value.  Permits users of LOCALDOMAIN
		 * to still have a search list, and anyone to set the
		 * one that they want to use as an individual (even more
		 * important now that the rfc1535 stuff restricts searches)
		 */
		cp	  = ircd_res.defdname;
		pp	  = ircd_res.dnsrch;
		*pp++ = cp;
		for (n = 0; *cp && pp < ircd_res.dnsrch + MAXDNSRCH; cp++)
		{
			if (*cp == '\n') /* silly backwards compat */
				break;
			else if (*cp == ' ' || *cp == '\t')
			{
				*cp = 0;
				n	= 1;
			}
			else if (n)
			{
				*pp++	   = cp;
				n		   = 0;
				havesearch = 1;
			}
		}
		/* null terminate last domain if there are excess */
		while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '\n')
			cp++;
		*cp	  = '\0';
		*pp++ = 0;
	}

#define MATCH(line, name)                      \
	(!strncmp(line, name, sizeof(name) - 1) && \
	 (line[sizeof(name) - 1] == ' ' ||         \
	  line[sizeof(name) - 1] == '\t'))

#ifdef NEXT
	if (ircd_netinfo_res_init(&haveenv, &havesearch) == 0)
#endif
		if ((fp = fopen(IRC_RESCONF, "r")) != NULL)
		{
			/* read the config file */
			while (fgets(buf, sizeof(buf), fp) != NULL)
			{
				/* skip comments */
				if (*buf == ';' || *buf == '#')
					continue;
				/* read default domain name */
				if (MATCH(buf, "domain"))
				{
					if (haveenv) /* skip if have from environ */
						continue;
					cp = buf + sizeof("domain") - 1;
					while (*cp == ' ' || *cp == '\t')
						cp++;
					if ((*cp == '\0') || (*cp == '\n'))
						continue;
					strncpyzt(ircd_res.defdname, cp, sizeof(ircd_res.defdname));
					if ((cp = strpbrk(ircd_res.defdname, " \t\n")) != NULL)
						*cp = '\0';
					havesearch = 0;
					continue;
				}
				/* set search list */
				if (MATCH(buf, "search"))
				{
					if (haveenv) /* skip if have from environ */
						continue;
					cp = buf + sizeof("search") - 1;
					while (*cp == ' ' || *cp == '\t')
						cp++;
					if ((*cp == '\0') || (*cp == '\n'))
						continue;
					strncpyzt(ircd_res.defdname, cp, sizeof(ircd_res.defdname));
					if ((cp = strchr(ircd_res.defdname, '\n')) != NULL)
						*cp = '\0';
					/*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
					cp	  = ircd_res.defdname;
					pp	  = ircd_res.dnsrch;
					*pp++ = cp;
					for (n = 0; *cp && pp < ircd_res.dnsrch + MAXDNSRCH; cp++)
					{
						if (*cp == ' ' || *cp == '\t')
						{
							*cp = 0;
							n	= 1;
						}
						else if (n)
						{
							*pp++ = cp;
							n	  = 0;
						}
					}
					/* null terminate last domain if there are excess */
					while (*cp != '\0' && *cp != ' ' && *cp != '\t')
						cp++;
					*cp		   = '\0';
					*pp++	   = 0;
					havesearch = 1;
					continue;
				}
				/* read nameservers to query */
				if (MATCH(buf, "nameserver") && nserv < MAXNS)
				{
					struct IN_ADDR a;
					char *tmp;

					cp = buf + sizeof("nameserver") - 1;
					while (*cp == ' ' || *cp == '\t')
						cp++;
					if ((*cp == '\0') || (*cp == '\n'))
						continue;
					/* strip ending \n or inetpton fails */
					if ((tmp = index(cp, '\n')) || (tmp = index(cp, '\r')))
						*tmp = '\0';
					if (
#ifdef INET6
							inetpton(AF_INET6, cp, a.s6_addr)
#else
						inetaton(cp, &a)
#endif
					)
					{
						ircd_res.nsaddr_list[nserv].SIN_ADDR   = a;
						ircd_res.nsaddr_list[nserv].SIN_FAMILY = AFINET;
						ircd_res.nsaddr_list[nserv].SIN_PORT =
								htons(NAMESERVER_PORT);
						nserv++;
					}
					continue;
				}
#ifdef RESOLVSORT
#ifdef INET6
#error No support for RESOLVSORT and INET6
#endif
				if (MATCH(buf, "sortlist"))
				{
					struct in_addr a;

					cp = buf + sizeof("sortlist") - 1;
					while (nsort < MAXRESOLVSORT)
					{
						while (*cp == ' ' || *cp == '\t')
							cp++;
						if (*cp == '\0' || *cp == '\n' || *cp == ';')
							break;
						net = cp;
						while (*cp && !ISSORTMASK(*cp) && *cp != ';' &&
							   isascii(*cp) && !isspace(*cp))
							cp++;
						n	= *cp;
						*cp = 0;
						if (inetaton(net, &a))
						{
							ircd_res.sort_list[nsort].addr = a;
							if (ISSORTMASK(n))
							{
								*cp++ = n;
								net	  = cp;
								while (*cp && *cp != ';' &&
									   isascii(*cp) && !isspace(*cp))
									cp++;
								n	= *cp;
								*cp = 0;
								if (inetaton(net, &a))
								{
									ircd_res.sort_list[nsort].mask = a.s_addr;
								}
								else
								{
									ircd_res.sort_list[nsort].mask =
											ircd_net_mask(ircd_res.sort_list[nsort].addr);
								}
							}
							else
							{
								ircd_res.sort_list[nsort].mask =
										ircd_net_mask(ircd_res.sort_list[nsort].addr);
							}
							nsort++;
						}
						*cp = n;
					}
					continue;
				}
#endif
				if (MATCH(buf, "options"))
				{
					ircd_res_setoptions(buf + sizeof("options") - 1, "conf");
					continue;
				}
			}
			if (nserv > 1)
				ircd_res.nscount = nserv;
#ifdef RESOLVSORT
			ircd_res.nsort = nsort;
#endif
			(void) fclose(fp);
		}
	if (ircd_res.defdname[0] == 0 &&
		gethostname(buf, sizeof(ircd_res.defdname) - 1) == 0 &&
		(cp = strchr(buf, '.')) != NULL)
		strcpy(ircd_res.defdname, cp + 1);

	/* find components of local domain that might be searched */
	if (havesearch == 0)
	{
		pp	  = ircd_res.dnsrch;
		*pp++ = ircd_res.defdname;
		*pp	  = NULL;

#ifndef RFC1535
		dots = 0;
		for (cp = ircd_res.defdname; *cp; cp++)
			dots += (*cp == '.');

		cp = ircd_res.defdname;
		while (pp < ircd_res.dnsrch + MAXDFLSRCH)
		{
			if (dots < LOCALDOMAINPARTS)
				break;
			cp	  = strchr(cp, '.') + 1; /* we know there is one */
			*pp++ = cp;
			dots--;
		}
		*pp = NULL;
#ifdef DEBUG
		if (ircd_res.options & RES_DEBUG)
		{
			printf(";; res_init()... default dnsrch list:\n");
			for (pp = ircd_res.dnsrch; *pp; pp++)
				printf(";;\t%s\n", *pp);
			printf(";;\t..END..\n");
		}
#endif /* DEBUG */
#endif /* !RFC1535 */
	}

	if ((cp = getenv("RES_OPTIONS")) != NULL)
		ircd_res_setoptions(cp, "env");
	ircd_res.options |= RES_INIT;
	return (0);
}

static void ircd_res_setoptions(char *options, char *source)
{
	char *cp = options;
	int i;

#ifdef DEBUG
	if (ircd_res.options & RES_DEBUG)
		printf(";; ircd_res_setoptions(\"%s\", \"%s\")...\n",
			   options, source);
#endif
	while (*cp)
	{
		/* skip leading and inner runs of spaces */
		while (*cp == ' ' || *cp == '\t')
			cp++;
		/* search for and process individual options */
		if (!strncmp(cp, "ndots:", sizeof("ndots:") - 1))
		{
			i = atoi(cp + sizeof("ndots:") - 1);
			if (i <= RES_MAXNDOTS)
				ircd_res.ndots = i;
			else
				ircd_res.ndots = RES_MAXNDOTS;
#ifdef DEBUG
			if (ircd_res.options & RES_DEBUG)
				printf(";;\tndots=%d\n", ircd_res.ndots);
#endif
		}
		else if (!strncmp(cp, "debug", sizeof("debug") - 1))
		{
#ifdef DEBUG
			if (!(ircd_res.options & RES_DEBUG))
			{
				printf(";; ircd_res_setoptions(\"%s\", \"%s\")..\n",
					   options, source);
				ircd_res.options |= RES_DEBUG;
			}
			printf(";;\tdebug\n");
#endif
		}
		else if (!strncmp(cp, "inet6", sizeof("inet6") - 1))
		{
#ifndef HAVE_GETIPNODEBYNAME
			ircd_res.options |= RES_USE_INET6;
#endif
		}
		else
		{
			/* XXX - print a warning here? */
		}
		/* skip to next run of spaces */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
	}
}

#ifdef RESOLVSORT
/* XXX - should really support CIDR which means explicit masks always. */
/* XXX - should really use system's version of this */
static u_int32_t ircd_net_mask(struct in_addr in)
{
	register u_int32_t i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	else if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	return (htonl(IN_CLASSC_NET));
}
#endif

#ifdef NEXT
static int ircd_netinfo_res_init(int *haveenv, int *havesearch)
{
	register int n;
	void *domain, *parent;
	ni_id dir;
	ni_status status;
	ni_namelist nl;
	int nserv = 0;
#ifdef RESOLVSORT
	int nsort = 0;
#endif

	status = ni_open(NULL, ".", &domain);
	if (status == NI_OK)
	{
		ni_setreadtimeout(domain, NI_TIMEOUT);
		ni_setabort(domain, 1);

		/* climb the NetInfo hierarchy to find a resolver directory */
		while (status == NI_OK)
		{
			status = ni_pathsearch(domain, &dir, NI_PATH_RESCONF);
			if (status == NI_OK)
			{
				/* found a resolver directory */

				if (*haveenv == 0)
				{
					/* get the default domain name */
					status = ni_lookupprop(domain, &dir, "domain", &nl);
					if (status == NI_OK && nl.ni_namelist_len > 0)
					{
						(void) strncpy(ircd_res.defdname,
									   nl.ni_namelist_val[0],
									   sizeof(ircd_res.defdname) - 1);
						ircd_res.defdname[sizeof(ircd_res.defdname) - 1] = '\0';
						ni_namelist_free(&nl);
						*havesearch = 0;
					}

					/* get search list */
					status = ni_lookupprop(domain, &dir, "search", &nl);
					if (status == NI_OK && nl.ni_namelist_len > 0)
					{
						(void) strncpy(ircd_res.defdname,
									   nl.ni_namelist_val[0],
									   sizeof(ircd_res.defdname) - 1);
						ircd_res.defdname[sizeof(ircd_res.defdname) - 1] = '\0';
						/* copy  */
						for (n = 0;
							 n < nl.ni_namelist_len && n < MAXDNSRCH;
							 n++)
						{
							/* duplicate up to MAXDNSRCH servers */
							char *cp = nl.ni_namelist_val[n];
							ircd_res.dnsrch[n] =
									strcpy((char *) malloc(strlen(cp) + 1), cp);
						}
						ni_namelist_free(&nl);
						*havesearch = 1;
					}
				}

				/* get list of nameservers */
				status = ni_lookupprop(domain, &dir, "nameserver", &nl);
				if (status == NI_OK && nl.ni_namelist_len > 0)
				{
					/* copy up to MAXNS servers */
					for (n = 0;
						 n < nl.ni_namelist_len && nserv < MAXNS;
						 n++)
					{
						struct in_addr a;

						if (inetaton(nl.ni_namelist_val[n], &a))
						{
							ircd_res.nsaddr_list[nserv].sin_addr   = a;
							ircd_res.nsaddr_list[nserv].sin_family = AF_INET;
							ircd_res.nsaddr_list[nserv].sin_port =
									htons(NAMESERVER_PORT);
							nserv++;
						}
					}
					ni_namelist_free(&nl);
				}

				if (nserv > 1)
					ircd_res.nscount = nserv;

#ifdef RESOLVSORT
				/* get sort order */
				status = ni_lookupprop(domain, &dir, "sortlist", &nl);
				if (status == NI_OK && nl.ni_namelist_len > 0)
				{

					/* copy up to MAXRESOLVSORT address/netmask pairs */
					for (n = 0;
						 n < nl.ni_namelist_len && nsort < MAXRESOLVSORT;
						 n++)
					{
						char ch;
						char *cp;
						const char *sp;
						struct in_addr a;

						cp = NULL;
						for (sp = sort_mask; *sp; sp++)
						{
							char *cp1;
							cp1 = strchr(nl.ni_namelist_val[n], *sp);
							if (cp && cp1)
								cp = (cp < cp1) ? cp : cp1;
							else if (cp1)
								cp = cp1;
						}
						if (cp != NULL)
						{
							ch	= *cp;
							*cp = '\0';
							break;
						}
						if (inetaton(nl.ni_namelist_val[n], &a))
						{
							ircd_res.sort_list[nsort].addr = a;
							if (*cp && ISSORTMASK(ch))
							{
								*cp++ = ch;
								if (inetaton(cp, &a))
								{
									ircd_res.sort_list[nsort].mask = a.s_addr;
								}
								else
								{
									ircd_res.sort_list[nsort].mask =
											ircd_net_mask(ircd_res.sort_list[nsort].addr);
								}
							}
							else
							{
								ircd_res.sort_list[nsort].mask =
										ircd_net_mask(ircd_res.sort_list[nsort].addr);
							}
							nsort++;
						}
					}
					ni_namelist_free(&nl);
				}

				ircd_res.nsort = nsort;
#endif

				/* get resolver options */
				status = ni_lookupprop(domain, &dir, "options", &nl);
				if (status == NI_OK && nl.ni_namelist_len > 0)
				{
					ircd_res_setoptions(nl.ni_namelist_val[0], "conf");
					ni_namelist_free(&nl);
				}

				ni_free(domain);
				return (1); /* using DNS configuration from NetInfo */
			}

			status = ni_open(domain, "..", &parent);
			ni_free(domain);
			if (status == NI_OK)
				domain = parent;
		}
	}
	return (0); /* if not using DNS configuration from NetInfo */
}
#endif /* NEXT */

u_int ircd_res_randomid(void)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	return (0xffff & (now.tv_sec ^ now.tv_usec ^ getpid()));
}
