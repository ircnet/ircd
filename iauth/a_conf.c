/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_conf.c
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
static  char rcsid[] = "@(#)$Id: a_conf.c,v 1.21 1999/07/11 22:11:33 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define A_CONF_C
#include "a_externs.h"
#undef A_CONF_C

static aModule *Mlist[16];

#define DEFAULT_TIMEOUT 30

u_int	debuglevel = 0;

AnInstance *instances = NULL;

static void
conf_err(nb, msg, chk)
u_int nb;
char *msg, *chk;
{
	if (chk)
		printf("line %d: %s\n", nb, msg);
	else
		sendto_log(ALOG_IRCD|ALOG_DCONF, LOG_ERR,
			   "Configuration error line %d: %s", nb, msg);
}

/*
 * Match address by #IP bitmask (10.11.12.128/27)
 */
static int
match_ipmask(mask, ipaddr)
aTarget	*mask;
char	*ipaddr;
{
#ifdef INET6
	return 1;
#else
        int i1, i2, i3, i4;
	u_long iptested;

        if (sscanf(ipaddr, "%d.%d.%d.%d", &i1, &i2, &i3, &i4) != 4) 
		return -1;
	iptested = htonl(i1 * 0x1000000 + i2 * 0x10000 + i3 * 0x100 + i4);
        return ((iptested & mask->lmask) == mask->baseip) ? 0 : 1;
#endif
}

/* conf_read: read the configuration file, instanciate modules */
char *
conf_read(cfile)
char *cfile;
{
	AnInstance *ident = NULL; /* make sure this module is used */
	u_char needh = 0; /* do we need hostname information for any host? */
	u_char o_req = 0, o_dto = 0, o_wup = 0;
	static char o_all[5];
	u_int timeout = DEFAULT_TIMEOUT, totto = 0;
	u_int lnnb = 0, i;
	u_char icount = 0, Mcnt = 0;
	char buffer[160], *ch;
	AnInstance **last = &instances, *itmp;
	FILE *cfh;

	Mlist[Mcnt++] = &Module_rfc931;
	Mlist[Mcnt++] = &Module_socks;
	Mlist[Mcnt++] = &Module_pipe;
	Mlist[Mcnt++] = &Module_lhex;
	Mlist[Mcnt] = NULL;

	cfh = fopen((cfile) ? cfile : IAUTHCONF_PATH, "r");
	if (cfh)
	    {
		while (fgets(buffer, 160, cfh))
		    {
			if (ch = index(buffer, '\n'))
				lnnb += 1;
			else
			    {
				conf_err(lnnb, "line too long, ignoring.",
					 cfile);
				/* now skip what's left */
				while (fgets(buffer, 160, cfh))
					if (index(buffer, '\n'))
						break;
				continue;
			    }
			if (buffer[0] == '#' || buffer[0] == '\n')
				continue;
			*ch = '\0';
			if (ch = index(buffer, '#'))
				*ch = '\0';
			if (!strncmp("required", buffer, 8))
			  {
				o_req = 1;
				continue;
			  }
			if (!strncmp("notimeout", buffer, 9))
			  {
				o_dto = 1;
				continue;
			  }
			if (!strncmp("extinfo", buffer, 7))
			  {
				o_wup = 1;
				continue;
			  }
			if (!strncmp("timeout = ", buffer, 10))
			    {
				if (sscanf(buffer, "timeout = %u",
					   &timeout) != 1)
					conf_err(lnnb, "Invalid setting.",
						 cfile);
				continue;
			    }
			/* debugmode setting */
			if (!strncmp("debuglvl = 0x", buffer, 13))
			    {
				if (sscanf(buffer, "debuglvl = %x",
					   &debuglevel) != 1)
					conf_err(lnnb, "Invalid setting.",
						 cfile);
				else if (!cfile)
					sendto_log(ALOG_DCONF, LOG_DEBUG,
						   "debuglevel = %X",
						   debuglevel);
				continue;
			    }
#if defined(USE_DSM)
			if (!strncmp("shared ", buffer, 7))
			    {
				char lfname[80];
				void *mod_handle;
				aModule *(*load_func)();

				ch = index(buffer+7, ' ');
				if (ch == NULL)
				    {
					conf_err(lnnb, "Syntax error.", cfile);
					continue;
				    }
				*ch++ = '\0';
				mod_handle = dlopen(ch, RTLD_NOW);
				if (mod_handle == NULL)
				    {
					conf_err(lnnb, dlerror(), cfile);
					continue;
				    }
# if defined(DLSYM_NEEDS_UNDERSCORE)
				sprintf(lfname, "_%s_load", buffer+7);
# else
				sprintf(lfname, "%s_load", buffer+7);
# endif
				load_func = (aModule *(*)())dlsym(mod_handle,
								  lfname);
				if (load_func == NULL)
				    {
					conf_err(lnnb,"Invalid shared object.",
						 cfile);
					dlclose(mod_handle);
					continue;
				    }
				Mlist[Mcnt] = load_func();
				if (Mlist[Mcnt])
				    {
					Mcnt += 1;
					Mlist[Mcnt] = NULL;
				    }
				else
				    {
					conf_err(lnnb, "Failed.", cfile);
					dlclose(mod_handle);
				    }
				continue;
			    }
#endif
			if (buffer[0] == '\t')
			    {
				conf_err(lnnb, "Ignoring unexpected property.",
					 cfile);
				continue;
			    }
			/* at this point, it has to be the following */
			if (strncasecmp("module ", buffer, 7))
			    {
				conf_err(lnnb,
					 "Unexpected line: not a module.",
					 cfile);
				continue;
			    }
			for (i = 0; Mlist[i] != NULL; i++)
				if (!strcasecmp(buffer+7, Mlist[i]->name))
					break;
			if (Mlist[i] == NULL)
			    {
				conf_err(lnnb, "Unknown module name.", cfile);
				continue;
			    }
			if (Mlist[i] == &Module_rfc931 && ident)
			    {
				conf_err(lnnb, 
				 "This module can only be loaded once.",
					 cfile);
				continue;
			    }
			*last = (AnInstance *) malloc(sizeof(AnInstance));
			(*last)->nexti = NULL;
			(*last)->in = icount++;
			(*last)->mod = Mlist[i];
			(*last)->opt = NULL;
			(*last)->popt = NULL;
			(*last)->data = NULL;
			(*last)->hostname = NULL;
			(*last)->address = NULL;
			(*last)->timeout = timeout;
			if (Mlist[i] == &Module_rfc931)
				ident = *last;

			while (fgets(buffer, 160, cfh))
			    {
				aTarget **ttmp;
				u_long baseip = 0, lmask = 0;

				if (ch = index(buffer, '\n'))
					lnnb += 1;
				else
				    {
					conf_err(lnnb,
						 "line too long, ignoring.",
						 cfile);
					/* now skip what's left */
					while (fgets(buffer, 160, cfh))
						if (index(buffer,'\n'))
							break;
					continue;
				    }
				if (buffer[0] == '#')
					continue;
				if (buffer[0] == '\n')
					break;
				if (buffer[0] != '\t')
				    {
					conf_err(lnnb, "Invalid syntax.",
						 cfile);
					continue;
				    }
				*ch = '\0';
				if (!strncasecmp(buffer+1, "option = ", 9))
				    {
					if ((*last)->opt)
						conf_err(lnnb,
					 "Duplicate option keyword: ignored.",
							 cfile);
					else
						(*last)->opt =
							mystrdup(buffer + 10);
					continue;
				    }
				if (!strncasecmp(buffer+1, "host = ", 7))
				    {
					needh = 1;
					ttmp = &((*last)->hostname);
					ch = buffer + 8;
				    }
				else if (!strncasecmp(buffer+1, "ip = ", 5))
				    {
					ttmp = &((*last)->address);
					ch = buffer + 6;
					if (strchr(ch, '/'))
					    {
						int i1, i2, i3, i4, m;
						
						if (sscanf(ch,"%d.%d.%d.%d/%d",
							   &i1, &i2, &i3, &i4,
							   &m) != 5 ||
						    m < 1 || m > 31)
						    {
							conf_err(lnnb,
								 "Bad mask.",
								 cfile);
							continue;
						    }
						lmask = htonl((u_long)0xffffffffL << (32 - m));
						baseip = htonl(i1 * 0x1000000 +
							       i2 * 0x10000 +
							       i3 * 0x100 +
							       i4);
					    }
					else
					    {
						lmask = 0;
						baseip = 0;
					    }
				    }
				else if (!strncmp(buffer+1, "timeout = ", 10))
				    {
					u_int local_timeout;
					if (sscanf(buffer+1, "timeout = %u",
						   &local_timeout) != 1)
						conf_err(lnnb,
							 "Invalid setting.",
							 cfile);
					(*last)->timeout = local_timeout;
					continue;
				    }
				else
				    {
					conf_err(lnnb, "Invalid keyword.",
						 cfile);
					continue;
				    }
				if (Mlist[i] == &Module_rfc931)
					continue;
				while (*ttmp)
					ttmp = &((*ttmp)->nextt);
				*ttmp = (aTarget *) malloc(sizeof(aTarget));
				if (*ch == '!')
				    {
					(*ttmp)->yes = -1;
					ch++;
				    }
				else
					(*ttmp)->yes = 0;
				(*ttmp)->value = mystrdup(ch);
				if ((*ttmp)->baseip)
				    {
					(*ttmp)->lmask = lmask;
					(*ttmp)->baseip = baseip;
				    }
				(*ttmp)->nextt = NULL;
			    }

			last = &((*last)->nexti);
		    }
	    }
	else if (cfile)
	    {
		perror("fopen");
		exit(0);
	    }
	if (ident == NULL)
	    {
		ident = *last = (AnInstance *) malloc(sizeof(AnInstance));
		(*last)->nexti = NULL;
		(*last)->opt = NULL;
		(*last)->mod = &Module_rfc931;
		(*last)->hostname = NULL;
		(*last)->address = NULL;
		(*last)->timeout = DEFAULT_TIMEOUT;
	    }
	ident->timeout = MAX(DEFAULT_TIMEOUT, ident->timeout);

	itmp = instances;
	while (itmp)
	    {
		totto += itmp->timeout;
		itmp = itmp->nexti;
	    }
	if (totto > ACCEPTTIMEOUT)
	    {
		if (cfile)
			printf("Warning: sum of timeouts exceeds ACCEPTTIMEOUT!\n");
		else
			sendto_log(ALOG_IRCD|ALOG_DCONF, LOG_ERR,
			   "Warning: sum of timeouts exceeds ACCEPTTIMEOUT!");
		if (o_dto)
			if (cfile)
				printf("Error: \"notimeout\" is set!\n");
			else
				sendto_log(ALOG_IRCD|ALOG_DCONF, LOG_ERR,
					   "Error: \"notimeout\" is set!");
	    }

	itmp = instances;
	if (cfile)
	    {
		aTarget *ttmp;
		char *err;

		printf("\nModule(s) loaded:\n");
		while (itmp)
		    {
			printf("\t%s\t%s\n", itmp->mod->name,
			       (itmp->opt) ? itmp->opt : "");
			if (ttmp = itmp->hostname)
			    {
				printf("\t\tHost = %s%s",
				       (ttmp->yes == 0) ? "" : "!",
				       ttmp->value);
				while (ttmp = ttmp->nextt)
					printf(",%s%s",
					       (ttmp->yes == 0) ? "" : "!",
					       ttmp->value);
				printf("\n");
			    }
			if (ttmp = itmp->address)
			    {
				printf("\t\tIP   = %s",
				       (ttmp->yes == 0) ? "" : "!",
				       ttmp->value);
				while (ttmp = ttmp->nextt)
					printf(",%s%s",
					       (ttmp->yes == 0) ? "" : "!",
					       ttmp->value);
				printf("\n");
			    }
			if (itmp->timeout != DEFAULT_TIMEOUT)
				printf("\t\ttimeout: %u seconds\n",
				       itmp->timeout);
			if (itmp->mod->init)
			    {
				err = itmp->mod->init(itmp);
				printf("\t\tInitialization: %s\n",
				       (err) ? err : "Successful");
			    }
			itmp = itmp->nexti;
		    }
	    }
	else
		while (itmp)
		    {
			if (itmp->mod->init)
				itmp->mod->init(itmp);
			itmp = itmp->nexti;
		    }

	ch = o_all;
	if (o_req) *ch++ = 'R';
	if (o_dto) *ch++ = 'T';
	if (o_wup) *ch++ = 'A';
	if (needh) *ch++ = 'W';
	*ch++ = '\0';
	return o_all;
}

/* conf_match: check if an instance is to be applied to a connection
   Returns -1: no match, and never will
            0: got a match, doIt[tm]
	    1: no match, but might be later so ask again */
int
conf_match(cl, inst)
u_int cl;
AnInstance *inst;
{
	aTarget *ttmp;

	/* general case, always matches */
	if (inst->address == NULL && inst->hostname == NULL)
		return 0;
	/* feature case, "host = *" to force to wait for DNS info */
	if ((cldata[cl].state & A_NOH) && inst->hostname &&
	    !strcmp(inst->hostname->value, "*"))
		return 0;
	/* check matches on IP addresses */
	if (ttmp = inst->address)
		while (ttmp)
		    {
			if (ttmp->baseip)
			    {
				if (match_ipmask(ttmp, cldata[cl].itsip) == 0)
					return ttmp->yes;
			    }
			else
				if (match(ttmp->value, cldata[cl].itsip) == 0)
					return ttmp->yes;
			ttmp = ttmp->nextt;
		    }
	/* check matches on hostnames */
	if (ttmp = inst->hostname)
	    {
		if (cldata[cl].state & A_GOTH)
		    {
			while (ttmp)
			    {
				if (match(ttmp->value, cldata[cl].host) == 0)
					return ttmp->yes;
				ttmp = ttmp->nextt;
			    }
			/* no match, will never match */
			return -1;
		    }
		else if (cldata[cl].state & A_NOH)
			return -1;
		else
			/* may be later, once we have DNS information */
			return 1;
	    }
	/* fall through, no match, will never match */
	return -1;
}

/* conf_ircd: send the configuration to the ircd daemon */
void
conf_ircd()
{
	AnInstance *itmp = instances;
	aTarget *ttmp;

	sendto_ircd("a");
	while (itmp)
	    {
		if (itmp->address == NULL && itmp->hostname == NULL)
			sendto_ircd("A * %s %s", itmp->mod->name,
				    (itmp->popt) ? itmp->popt : "");
		else
		    {
			ttmp = itmp->address;
			while (ttmp)
			    {
				sendto_ircd("A %s %s %s", ttmp->value,
					    itmp->mod->name,
					    (itmp->popt) ? itmp->popt : "");
				ttmp = ttmp->nextt;
			    }
			ttmp = itmp->hostname;
			while (ttmp)
			    {
				sendto_ircd("A %s %s %s", ttmp->value,
					    itmp->mod->name,
					    (itmp->popt) ? itmp->popt : "");
				ttmp = ttmp->nextt;
			    }
		    }
		itmp = itmp->nexti;
	    }
}
