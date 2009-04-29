/************************************************************************
 *   IRC - Internet Relay Chat, ircd/chkconf.c
 *   Copyright (C) 1993 Darren Reed
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
static const volatile char rcsid[] = "@(#)$Id: chkconf.c,v 1.52 2009/04/29 22:05:08 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define CHKCONF_C
#include "match_ext.h"
#undef CHKCONF_C

#define mystrdup(x) strdup(x)
#ifdef MyMalloc
#undef MyMalloc
#endif
#define MyMalloc(x)     malloc(x)
/*#define MyFree(x)       free(x)*/

static	char	*configfile = IRCDCONF_PATH;
#include "config_read.c"

#ifdef CONFIG_DIRECTIVE_INCLUDE
#define CK_FILE filelist->file
#define CK_LINE filelist->linenum
static aConfig *findConfLineNumber(int);
static aConfig *files;
#else
#define CK_FILE filelist->filename
#define CK_LINE nr - filelist->min
static struct wordcount *findConfLineNumber(int);
static struct wordcount *files;
struct	wordcount {
	char *filename;
	int min;
	int max;
	struct wordcount *next;
};
static	void	mywc(void);
#ifdef M4_PREPROC
static	char *	mystrinclude(char *, int);
static	int	simulateM4Include(struct wordcount *, int, char *, int);
#endif
static	int	dgets(int, char *, int);
#endif

static	void	new_class(int);
static	char	*getfield(char *), confchar(u_int);
static	int	openconf(void);
static	void	validate(aConfItem *);
static	aClass	*get_class(int, int);
static	aConfItem	*initconf(void);
static	void	showconf(void);
static	int	checkSID(char *, int);

static	int	numclasses = 0, *classarr = (int *)NULL, debugflag = 0;
static	char	nullfield[] = "";
static	char	maxsendq[12];

#define	SHOWSTR(x)	((x) ? (x) : "*")

int	main(int argc, char *argv[])
{
	aConfItem *result;
	int	showflag = 0;

	if (argc > 1 && !strncmp(argv[1], "-h", 2))
	{
		(void)printf("Usage: %s [-h | -s | -d[#]] [ircd.conf]\n",
			      argv[0]);
		(void)printf("\t-h\tthis help\n");
		(void)printf("\t-s\tshows preprocessed config file (after "
			"includes and/or M4)\n");
		(void)printf("\t-d[#]\tthe bigger number, the more verbose "
			"chkconf is in its checks\n");
		(void)printf("\tDefault ircd.conf = %s\n", IRCDCONF_PATH);
		exit(0);
	}
	new_class(0);

	if (argc > 1 && !strncmp(argv[1], "-d", 2))
   	{
		debugflag = 1;
		if (argv[1][2])
			debugflag = atoi(argv[1]+2);
		argc--;
		argv++;
	}
	if (argc > 1 && !strncmp(argv[1], "-s", 2))
   	{
		showflag = 1;
		argc--;
		argv++;
	}
	if (argc > 1)
		configfile = argv[1];
	if (showflag)
	{
		showconf();
		return 0;
	}
#ifndef CONFIG_DIRECTIVE_INCLUDE
	/* Initialize counters to be able to print line number even with m4 */
	mywc();
# ifdef DEBUGMODE
	{
	struct wordcount *filelist;

	for(filelist = files; filelist->next; filelist = filelist->next)
	  fprintf(stderr, "%s: Min %d - Max %d\n",
		  filelist->filename, filelist->min, filelist->max);
	}
# endif
#endif
	/* If I do not use result as temporary return value
	 * I get loops when M4_PREPROC is defined - Babar */
	result = initconf();
	validate(result);
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	config_free(files);
#endif
	return 0;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
static	int	openconf(void)
{
#ifdef	M4_PREPROC
	int	pi[2];
#ifdef HAVE_GNU_M4
	char	*includedir, *includedirptr;

	includedir = strdup(IRCDM4_PATH);
	includedirptr = strrchr(includedir, '/');
	if (includedirptr)
		*includedirptr = '\0';
#endif

	/* ircd.m4 with full path now! Kratz */
	if (access(IRCDM4_PATH, R_OK) == -1)
	{
		(void)fprintf(stderr, "%s missing.\n", IRCDM4_PATH);
		return -1;
	}
	if (pipe(pi) == -1)
		return -1;
	switch(fork())
	{
	case -1:
		return -1;
	case 0:
		(void)close(pi[0]);
		(void)close(2);
		if (pi[1] != 1)
		    {
			(void)close(1);
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		(void)dup2(1,2);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp(M4_PATH, "m4",
#ifdef HAVE_GNU_M4
#ifdef USE_M4_PREFIXES
			"-P",
#endif
			"-I", includedir,
#endif
#ifdef INET6
			"-DINET6",
#endif
			IRCDM4_PATH, configfile, (char *) NULL);
		perror("m4");
		exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	return open(configfile, O_RDONLY);
#endif
}

/* show config as ircd would see it before starting to parse it */
static	void	showconf(void)
{
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	aConfig *p, *p2;
	int etclen = 0;
	FILE *fdn;
#else
	int dh;
	char	line[512], c[80], *tmp;
#endif
	int fd;

	fprintf(stderr, "showconf(): %s\n", configfile);
	if ((fd = openconf()) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return;
	    }
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	if (debugflag)
	{
		etclen = strlen(IRCDCONF_DIR);
	}
	fdn = fdopen(fd, "r");
	p2 = config_read(fdn, 0, new_config_file(configfile, NULL, 0));
	for(p = p2; p; p = p->next)
	{
		if (debugflag)
			printf("%s:%d:", p->file->filename +
				(strncmp(p->file->filename, IRCDCONF_DIR,
				etclen) == 0 ? etclen : 0), p->linenum);
		printf("%s\n", p->line);
	}
	config_free(p2);
	(void)fclose(fdn);
#else
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((dh = dgets(fd, line, sizeof(line) - 1)) > 0)
	{
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1))
			if ((tmp = (char *)index(c, '\n')))
			{   
				*tmp = 0;
				break;
			}
		printf("%s\n", line);
	}
	(void)close(fd);
#endif
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	return;
}


/*
** initconf()
**    Read configuration file.
**
**    returns a pointer to the config items chain if successful
**            NULL if config is invalid (some mandatory fields missing)
*/

static	aConfItem 	*initconf(void)
{
	int	fd;
	char	*tmp, *tmp3 = NULL, *s;
	int	ccount = 0, ncount = 0, flags = 0, nr = 0;
	aConfItem *aconf = NULL, *ctop = NULL;
	int	mandatory_found = 0, valid = 1;
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	char    *line;
	aConfig *ConfigTop, *filelist;
	aFile	*ftop;
	FILE	*fdn;
#else
	int	dh;
	struct wordcount *filelist = files;
	char	line[512], c[80], *ftop;
#endif

	(void)fprintf(stderr, "initconf(): ircd.conf = %s\n", configfile);
	if ((fd = openconf()) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return NULL;
	    }

#if defined(CONFIG_DIRECTIVE_INCLUDE)
	ftop = new_config_file(configfile, NULL, 0);
	fdn = fdopen(fd, "r");
	files = ConfigTop = config_read(fdn, 0, ftop);
	for(filelist = ConfigTop; filelist; filelist = filelist->next)
#else
	ftop = configfile;
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((dh = dgets(fd, line, sizeof(line) - 1)) > 0)
#endif
	    {
		if (aconf)
		    {
			if (aconf->host && (aconf->host != nullfield))
				(void)free(aconf->host);
			if (aconf->passwd && (aconf->passwd != nullfield))
				(void)free(aconf->passwd);
			if (aconf->name && (aconf->name != nullfield))
				(void)free(aconf->name);
		    }
		else
			aconf = (aConfItem *)malloc(sizeof(*aconf));
		aconf->host = (char *)NULL;
		aconf->passwd = (char *)NULL;
		aconf->name = (char *)NULL;
		aconf->class = (aClass *)NULL;
		/* abusing clients to store ircd.conf line number */
		aconf->clients = ++nr;

#if defined(CONFIG_DIRECTIVE_INCLUDE)
		line = filelist->line;
#else
	    	while (filelist->next && (filelist->max < nr))
	    		filelist = filelist->next;
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1))
			if ((tmp = (char *)index(c, '\n')))
			    {
				*tmp = 0;
				break;
			    }
#endif
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		    {
			if (*tmp == '\\')
			    {
				switch (*(tmp+1))
				{
				case 'n' :
					*tmp = '\n';
					break;
				case 'r' :
					*tmp = '\r';
					break;
				case 't' :
					*tmp = '\t';
					break;
				case '0' :
					*tmp = '\0';
					break;
				default :
					*tmp = *(tmp+1);
					break;
				}
				if (!*(tmp+1))
					break;
				else
					for (s = tmp; (*s = *(s+1)); s++)
						;
				tmp++;
			    }
			else if (*tmp == '#')
				*tmp = '\0';
		    }
		if (!*line || *line == '#' || *line == '\n' ||
		    *line == ' ' || *line == '\t')
			continue;

		if (line[1] != IRCDCONF_DELIMITER)
		    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"wrong delimiter in line (%s)", line);
                        continue;
                    }

		if (debugflag)
			config_error(CF_NONE, CK_FILE, CK_LINE, "%s", line);

		tmp = getfield(line);
		if (!tmp)
		    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"no field found");
			continue;
		    }

		aconf->status = CONF_ILLEGAL;
		aconf->flags = 0L;

		switch (*tmp)
		{
			case 'A': /* Name, e-mail address of administrator */
			case 'a': /* of this server. */
				aconf->status = CONF_ADMIN;
				mandatory_found |= CONF_ADMIN;
				break;
			case 'B': /* bounce line */
			case 'b':
				aconf->status = CONF_BOUNCE;
				break;
			case 'C': /* Server where I should try to connect */
			  	  /* in case of lp failures             */
				ccount++;
				aconf->status = CONF_CONNECT_SERVER;
				break;
			case 'c':
				ccount++;
				aconf->status = CONF_ZCONNECT_SERVER;
				break;
			case 'D': /* auto connect restrictions */
			case 'd':
				aconf->status = CONF_DENY;
				break;
			case 'H': /* Hub server line */
			case 'h':
				aconf->status = CONF_HUB;
				break;
			case 'i' : /* Restricted client */
				aconf->flags |= CFLAG_RESTRICTED;
			case 'I':
				aconf->status = CONF_CLIENT;
				mandatory_found |= CONF_CLIENT;
				break;
			case 'K': /* Kill user line on irc.conf           */
				aconf->status = CONF_KILL;
				break;
			case 'k':
				aconf->status = CONF_OTHERKILL;
				break;
			/* Operator. Line should contain at least */
			/* password and host where connection is  */
			case 'L': /* guaranteed leaf server */
			case 'l':
				aconf->status = CONF_LEAF;
				break;
			/* Me. Host field is name used for this host */
			/* and port number is the number of the port */
			case 'M':
			case 'm':
				aconf->status = CONF_ME;
				mandatory_found |= CONF_ME;
				break;
			case 'N': /* Server where I should NOT try to     */
			case 'n': /* connect in case of lp failures     */
				  /* but which tries to connect ME        */
				++ncount;
				aconf->status = CONF_NOCONNECT_SERVER;
				break;
			case 'o':
			case 'O':
				aconf->status = CONF_OPERATOR;
				break;
			case 'P': /* listen port line */
			case 'p':
				aconf->status = CONF_LISTEN_PORT;
				mandatory_found |= CONF_LISTEN_PORT;
				break;
			case 'Q': /* a server that you don't want in your */
			case 'q': /* network. USE WITH CAUTION! */
				aconf->status = CONF_QUARANTINED_SERVER;
				break;
			case 'S': /* Service. Same semantics as   */
			case 's': /* CONF_OPERATOR                */
				aconf->status = CONF_SERVICE;
				break;
			case 'V':
				aconf->status = CONF_VER;
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
#ifdef XLINE
			case 'X':
				aconf->status = CONF_XLINE;
				break;
#endif
		    default:
			config_error(CF_WARN, CK_FILE, CK_LINE,
				"unknown conf line letter (%c)\n", *tmp);
			break;
		    }

		if (IsIllegal(aconf))
			continue;

		do
		{
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_CLIENT_MASK)
			    {
				if (!*tmp)
					config_error(CF_WARN, CK_FILE, CK_LINE,
						"no class, default 0");
				aconf->class = get_class(atoi(tmp), nr);
			    }
			tmp3 = getfield(NULL);
		} while (0); /* to use break without compiler warnings */

		if ((aconf->status & CONF_CLIENT) && tmp3)
		{
			/* Parse I-line flags */
			for(s = tmp3; *s; ++s)
			{
				switch (*s)
				{
#ifdef XLINE
				case 'e':
#endif
				case 'R':
				case 'D':
				case 'I':
				case 'E':
				case 'N':
				case 'M':
				case 'F':
					break;
				case ' ':
				case '\t':
					/* so there's no weird warnings */
					break;
				default:
					config_error(CF_WARN, CK_FILE, CK_LINE,
						"unknown I-line flag: %c", *s);
				}
			}
		}
		if ((aconf->status & CONF_OPERATOR) && tmp3)
		{
			/* Parse O-line flags */
			for(s = tmp3; *s; ++s)
			{
				switch (*s)
				{
				case 'A':
				case 'L':
				case 'K':
				case 'k':
				case 'S':
				case 's':
				case 'C':
				case 'c':
				case 'l':
				case 'h':
				case 'd':
				case 'r':
				case 'R':
				case 'D':
				case 'e':
				case 'T':
#ifdef CLIENTS_CHANNEL
				case '&':
#endif
				case 'p':
				case 'P':
				case 't':
					break;
				case ' ':
				case '\t':
					/* so there's no weird warnings */
					break;
				default:
					config_error(CF_WARN, CK_FILE, CK_LINE,
						"unknown O-line flag: %c", *s);
				}
			}
		}

		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS)
		    {
			if (!aconf->host)
			    {
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"no class #");
				continue;
			    }
			if (!tmp)
			    {
				config_error(CF_WARN, CK_FILE, CK_LINE,
					"missing sendq field, "
					"assuming default %d", QUEUELEN);
				(void)sprintf(maxsendq, "%d", QUEUELEN);
			    }
			else
				(void)sprintf(maxsendq, "%d", atoi(tmp));
			new_class(atoi(aconf->host));
			aconf->class = get_class(atoi(aconf->host), nr);
			goto print_confline;
		    }

		if (aconf->status & CONF_LISTEN_PORT)
		{
			if (!aconf->host)
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"null host field in P-line");
#ifdef	UNIXPORT
			else if (index(aconf->host, '/'))
			{
				struct	stat	sb;

				if (stat(aconf->host, &sb) == -1)
				{
					config_error(CF_ERR, CK_FILE, CK_LINE,
						"error in stat(%s)",
						aconf->host);
				}
				else if ((sb.st_mode & S_IFMT) != S_IFDIR)
					config_error(CF_ERR, CK_FILE, CK_LINE,
						"%s not directory",
						aconf->host);
			}
#else
			else if (index(aconf->host, '/'))
			{
				config_error(CF_WARN, CK_FILE, CK_LINE,
					"/ present in P-line but UNIXPORT "
					"undefined");
			}
#endif
			aconf->class = get_class(0, nr);
			goto print_confline;
		}

		if (aconf->status & CONF_SERVER_MASK &&
		    (!aconf->host || index(aconf->host, '*') ||
		     index(aconf->host, '?')))
		{
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"bad host field");
			continue;
		}

		if (aconf->status & CONF_SERVER_MASK && BadPtr(aconf->passwd))
		    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"empty/no password field");
			continue;
		    }

		if (aconf->status & CONF_SERVER_MASK && !aconf->name)
		    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"bad name field");
			continue;
		    }

		if (aconf->status & (CONF_SERVER_MASK|CONF_OPS))
			if (!index(aconf->host, '@'))
			    {
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)sprintf(newhost, "*@%s", aconf->host);
				(void)free(aconf->host);
				aconf->host = newhost;
			    }

		if (!aconf->class)
			aconf->class = get_class(0, nr);
		(void)sprintf(maxsendq, "%d", aconf->class->class);

		if (!aconf->name)
			aconf->name = nullfield;
		if (!aconf->passwd)
			aconf->passwd = nullfield;
		if (!aconf->host)
			aconf->host = nullfield;
		if (aconf->status & CONF_ME)
		{
			if (flags & aconf->status)
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"multiple M-lines");
			else
				flags |= aconf->status;
			if (tmp && *tmp)
			{
			    	/* Check for SID at the end of M: line */
			    	if (debugflag > 2)
			    		(void)printf("SID is set to: %s\n", tmp);
			    	if (checkSID(tmp, nr))
					config_error(CF_ERR, CK_FILE, CK_LINE,
						"SID invalid");
			}
			else
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"no SID in M-line");
		}
		if (aconf->status & CONF_ADMIN)
		{
			if (flags & aconf->status)
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"multiple A-lines");
			else
				flags |= aconf->status;
			if (tmp && *tmp)
			{
			    	/* Check for Network at the end of A: line */
			    	if (debugflag > 2)
			    		(void)printf("Network is set to: %s\n", tmp);
			}
			else
				config_error(CF_ERR, CK_FILE, CK_LINE,
					"no network in A-line");
		}

		if (aconf->status & CONF_VER)
		{
			if (*aconf->host && !index(aconf->host, '/'))
				config_error(CF_WARN, CK_FILE, CK_LINE,
					"no / in V line");
			else if (*aconf->passwd && !index(aconf->passwd, '/'))
				config_error(CF_WARN, CK_FILE, CK_LINE,
					"no / in V line");
		}
print_confline:
		if (debugflag > 8)
		{
			(void)printf("(%d) (%s) (%s) (%s) (%d) (%s)\n",
			      aconf->status, aconf->host, aconf->passwd,
			      aconf->name, aconf->port, maxsendq);
		}
		(void)fflush(stdout);
		if (aconf->status & (CONF_SERVER_MASK|CONF_HUB|CONF_LEAF))
		    {
			aconf->next = ctop;
			ctop = aconf;
			aconf = NULL;
		    }
	    }
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	(void)fclose(fdn);
#else
	(void)close(fd);
#endif
#ifdef	M4_PREPROC
	(void)wait(0);
#endif

	if (!(mandatory_found & CONF_ME))
	{
		config_error(CF_ERR, ftop, 0,
			"no M-line found (mandatory)");
		valid = 0;
	}
	if (!(mandatory_found & CONF_ADMIN))
	{
		config_error(CF_ERR, ftop, 0,
			"no A-line found (mandatory)");
		valid = 0;
	}
	if (!(mandatory_found & CONF_LISTEN_PORT))
	{
		config_error(CF_ERR, ftop, 0,
			"no P-line found (mandatory)");
		valid = 0;
	}
	if (!(mandatory_found & CONF_CLIENT))
	{
		config_error(CF_ERR, ftop, 0,
			"no I-line found (mandatory)");
		valid = 0;
	}

	if (valid)
	{
		return ctop;
	}
	return NULL;
}

static	aClass	*get_class(int cn, int nr)
{
	static	aClass	cls;
	int	i = numclasses - 1;
#ifdef CONFIG_DIRECTIVE_INCLUDE
	aConfig *filelist;
#else
	struct wordcount *filelist;
#endif

	cls.class = -1;
	for (; i >= 0; i--)
		if (classarr[i] == cn)
		    {
			cls.class = cn;
			break;
		    }
	if (i == -1)
	    {
	    	filelist = findConfLineNumber(nr);
		config_error(CF_WARN, CK_FILE, CK_LINE,
			"class %d not found", cn);
	    }
	return &cls;
}

static	void	new_class(int cn)
{
	numclasses++;
	if (classarr)
		classarr = (int *)realloc(classarr, sizeof(int) * numclasses);
	else
		classarr = (int *)malloc(sizeof(int));
	classarr[numclasses-1] = cn;
}

/*
 * field breakup for ircd.conf file.
 */
static	char	*getfield(char *irc_newline)
{
	static	char *line = NULL;
	char	*end, *field;

	if (irc_newline)
		line = irc_newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if ((end = (char *)index(line, IRCDCONF_DELIMITER)) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}

#ifndef CONFIG_DIRECTIVE_INCLUDE
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
static	int	dgets(int fd, char *buf, int num)
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
		if (head < tail)
		    {
			n = MIN(tail - head, num);
			/* File hasn't got a final new line */
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
#endif

static	void	validate(aConfItem *top)
{
	Reg	aConfItem *aconf, *bconf;
	u_int	otype = 0, valid = 0;
	int	nr;
#ifdef CONFIG_DIRECTIVE_INCLUDE
	aConfig *filelist;
#else
	struct wordcount *filelist;
#endif

	if (!top)
		return;

	for (aconf = top; aconf; aconf = aconf->next)
	{
		if (aconf->status & CONF_MATCH)
			continue;

		if (aconf->status & CONF_SERVER_MASK)
		    {
			if (aconf->status & (CONF_CONNECT_SERVER|CONF_ZCONNECT_SERVER))
				otype = CONF_NOCONNECT_SERVER;
			else if (aconf->status & CONF_NOCONNECT_SERVER)
				otype = CONF_CONNECT_SERVER;

			for (bconf = top; bconf; bconf = bconf->next)
			    {
				if (bconf == aconf || !(bconf->status & otype))
					continue;
				if (bconf->class == aconf->class &&
				    !mycmp(bconf->name, aconf->name) &&
				    !mycmp(bconf->host, aconf->host))
				    {
					aconf->status |= CONF_MATCH;
					bconf->status |= CONF_MATCH;
						break;
				    }
			    }
		    }
		else
			for (bconf = top; bconf; bconf = bconf->next)
			    {
				if ((bconf == aconf) ||
				    !(bconf->status & CONF_SERVER_MASK))
					continue;
				if (!mycmp(bconf->name, aconf->name))
				    {
					aconf->status |= CONF_MATCH;
					break;
				    }
			    }
	}

	for (aconf = top; aconf; aconf = aconf->next)
		if (aconf->status & CONF_MATCH)
			valid++;
		else
		    {
			nr = aconf->clients;
			filelist = findConfLineNumber(nr);
			config_error(CF_WARN, CK_FILE, CK_LINE,
				"unmatched %c%c%s%c%s%c%s",
				confchar(aconf->status), IRCDCONF_DELIMITER,
				aconf->host, IRCDCONF_DELIMITER,
				SHOWSTR(aconf->passwd), IRCDCONF_DELIMITER,
				aconf->name);
		    }
	return;
}

#ifdef M4_PREPROC
static int simulateM4Include(struct wordcount *filelist, int nr, char *filename, int fnrmin)
{
	int	fd, dh, fnr = 0, finalnewline = 0;
	char	line[512];
	struct wordcount *listnew;
#ifdef M4_PREPROC
	char *inc;
#endif

	listnew = (struct wordcount *)malloc(sizeof(struct wordcount));
	filelist->next = listnew;
	filelist = filelist->next;
	filelist->min = nr - fnrmin;
	filelist->filename = strdup(filename);
	filelist->max = 0;
	filelist->next = NULL;
	if ((fd = open(filename, O_RDONLY)) == -1)
	{
		perror(filename);
		return nr;
	}
	(void)dgets(-1, NULL, 0);
	while ((dh = dgets(fd, line, sizeof(line) - 1)) > 0)
	{
		if (dh != sizeof(line) - 1)
		{
			fnr++;
			if (fnr > fnrmin)
			{
				nr++;
				if (line[dh - 1] != '\n')
				{
					config_error(CF_WARN, CK_FILE, CK_LINE,
						"no final new line");
					finalnewline = 0;
				} else
					finalnewline = 1;
				line[dh - 1] = 0;
#ifdef	M4_PREPROC
				inc = mystrinclude(line, nr);
				if (inc)
				{
					filelist->max = --nr;
					nr = simulateM4Include(filelist, nr, inc, 0);
					while (filelist->next) filelist = filelist->next;
					nr = simulateM4Include(filelist, nr, filename, fnr);
					break;
				}
#endif
			}
		}
		else
			if (fnr > fnrmin)
			{
				line[dh - 1] = 0;

				config_error(CF_WARN, CK_FILE, CK_LINE,
					"line (%s) too long, maxlen %d",
					line, sizeof(line));
			}
	}
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	nr += finalnewline;
	if (!filelist->max) filelist->max = nr;
	return nr;
}
#endif

#ifndef CONFIG_DIRECTIVE_INCLUDE
static void	mywc(void)
{
	int	fd, dh, nr = 0;
	char	line[512];
	struct wordcount *listtmp;
#ifdef M4_PREPROC
	char *inc;
#endif

	/* Dealing with ircd.m4 */
	files = listtmp = (struct wordcount *)malloc(sizeof(struct wordcount));
	listtmp->min = 0;
#ifdef	M4_PREPROC
	listtmp->filename = strdup(IRCDM4_PATH);
	inc = configfile;
	configfile = 0; /* used to have openconf launch only m4 ircd.m4 */
#else
	listtmp->filename = strdup("ircd.conf");
#endif
	if ((fd = openconf()) == -1)
	{
		(void)wait(0);
		return;
	}
	(void)dgets(-1, NULL, 0);
	/* We only count lines. No include in ircd.m4 if M4 defined */
	while ((dh = dgets(fd, line, sizeof(line) - 1)) > 0)
		if (dh != sizeof(line) - 1)
			nr++;
	listtmp->max = nr;
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
	configfile = inc;
	nr = simulateM4Include(listtmp, nr, configfile, 0);
#endif
}
#endif	/* CONFIG_DIRECTIVE_INCLUDE */

static	char	confchar(u_int status)
{
	static	char	letrs[] = "QIicNCoOMKARYSLPHV";
	char	*s = letrs;

	status &= ~(CONF_MATCH|CONF_ILLEGAL);

	for (; *s; s++, status >>= 1)
		if (status & 1)
			return *s;
	return '-';
}

static	int	checkSID(char *sid, int nr)
{
	char *s = sid;
	int len = 0, rc = 0;
#ifdef CONFIG_DIRECTIVE_INCLUDE
	aConfig *filelist;
#else
	struct wordcount *filelist;
#endif

	/* First SID char shall be numeric, rest must be alpha */
    	filelist = findConfLineNumber(nr);
	if (!isdigit(*s))
	    {
		config_error(CF_ERR, CK_FILE, CK_LINE,
			"SID %s invalid: 1st character must be number", sid);
		++rc;
	    }
	++s; ++len;
    	for (; *s; ++s, ++len)
		if (!isalnum(*s))
		    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
                        	"SID %s invalid: wrong character (%c)",
				sid, *s);
			++rc;
		}
	if (!(len == SIDLEN))
	    {
			config_error(CF_ERR, CK_FILE, CK_LINE,
				"SID %s invalid: wrong size (%d should be %d)",
				sid, len, SIDLEN);
		++rc;
	    }
	return rc;
}

#ifdef M4_PREPROC
/* Do: s/include\((.*)\)/\1/
 * and checks that it's not "commented out" */
static	char *	mystrinclude(char *s, int nr)
{
	int off;
	struct wordcount *filelist;
	char *match = strstr(s, "include(");

	if (!match)
		return 0;
	off = match - s;
	for(; s < match; ++s)
		if (!isspace(*s))
			return 0;
	s += 8; /* length("include(") */
	for(; match && *match != ')'; ++match);
	*match = '\0';

	if (off > 0)
	{
	    	filelist = findConfLineNumber(nr);
		config_error(CF_WARN, CK_FILE, CK_LINE,
			"spaces before include(%s): first line of included "
			"file will be shifted", s);
	}
	return s;
}
#endif

#ifdef CONFIG_DIRECTIVE_INCLUDE
static aConfig *findConfLineNumber(int nr)
{
	int     mynr = 1;
	aConfig *p = files;

	for( ; p->next && mynr < nr; p = p->next)
		mynr++;
	return p;
}
#else
/* Locates file and line number of a config line */
static  struct	wordcount *	findConfLineNumber(int nr)
{
	struct wordcount *filelist = files;
	for(; filelist->next && (filelist->max < nr); filelist = filelist->next);
	return filelist;
}
#endif
