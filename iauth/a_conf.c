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
static  char rcsid[] = "@(#)$Id: a_conf.c,v 1.7 1999/01/13 02:32:41 kalt Exp $";
#endif

#include "os.h"
#include "a_defines.h"
#define A_CONF_C
#include "a_externs.h"
#undef A_CONF_C

static aModule *Mlist[] =
	{ &Module_rfc931, &Module_socks, &Module_pipe, (aModule *)NULL };

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
		sendto_log(ALOG_DCONF, LOG_ERR,
			   "Configuration error line %d: %s", nb, msg);
}

/* conf_read: read the configuration file, instanciate modules */
void
conf_read(cfile)
char *cfile;
{
	u_char ident = 0; /* make sure this module is used */
	u_int lnnb = 0, i;
	char buffer[160], *ch;
	AnInstance **last = &instances, *itmp;
	FILE *cfh;

	cfh = fopen((cfile) ? cfile : QPATH, "r");
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
			if (Mlist[i] == &Module_rfc931)
			    {
				if (ident)
				    {
					conf_err(lnnb,"Module already loaded.",
						 cfile);
					continue;
				    }
				ident = 1;
			    }
			*last = (AnInstance *) malloc(sizeof(AnInstance));
			(*last)->nexti = NULL;
			(*last)->mod = Mlist[i];
			(*last)->opt = NULL;
			(*last)->data = NULL;
			(*last)->hostname = NULL;
			(*last)->address = NULL;

			while (fgets(buffer, 160, cfh))
			    {
				aTarget **ttmp;

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
					ttmp = &((*last)->hostname);
					ch = buffer + 8;
				    }
				else if (!strncasecmp(buffer+1, "ip = ", 5))
				    {
					ttmp = &((*last)->address);
					ch = buffer + 6;
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
				(*ttmp)->value = mystrdup(ch);
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
	if (ident == 0)
	    {
		*last = (AnInstance *) malloc(sizeof(AnInstance));
		(*last)->nexti = NULL;
		(*last)->opt = NULL;
		(*last)->mod = &Module_rfc931;
		(*last)->hostname = NULL;
		(*last)->address = NULL;
	    }

	itmp = instances;
	if (cfile)
	    {
		aTarget *ttmp;
		char *err;

		printf("Module(s) loaded:\n");
		while (itmp)
		    {
			printf("\t%s\t%s\n", itmp->mod->name,
			       (itmp->opt) ? itmp->opt : "");
			if (ttmp = itmp->hostname)
			    {
				printf("\t\tHost = %s", ttmp->value);
				while (ttmp = ttmp->nextt)
					printf(",%s", ttmp->value);
				printf("\n");
			    }
			if (ttmp = itmp->address)
			    {
				printf("\t\tIP   = %s", ttmp->value);
				while (ttmp = ttmp->nextt)
					printf(",%s", ttmp->value);
				printf("\n");
			    }
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
}

/* conf_match: check if an instance is to be applied to a connection */
int
conf_match(cl, inst, noipchk)
u_int cl;
AnInstance *inst;
{
	aTarget *ttmp;

	if (noipchk == 0 && inst->address == NULL && inst->hostname == NULL)
		return 0;
	if ((ttmp = inst->hostname) && (cldata[cl].state & A_GOTH))
		while (ttmp)
		    {
			if (match(ttmp->value, cldata[cl].host) == 0)
				return 0;
			ttmp = ttmp->nextt;
		    }
	if (noipchk == 0 && (ttmp = inst->address))
		while (ttmp)
		    {
			if (match(ttmp->value, cldata[cl].itsip) == 0)
				return 0;
			ttmp = ttmp->nextt;
		    }
	return 1;
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
