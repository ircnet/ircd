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
static  char rcsid[] = "@(#)$Id: chkconf.c,v 1.31 2004/07/15 01:03:45 chopin Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define CHKCONF_C
#include "match_ext.h"
#undef CHKCONF_C

#define MyMalloc(x)     malloc(x)
/*#define MyFree(x)       free(x)*/

static	char	*configfile = IRCDCONF_PATH;
#ifdef CONFIG_DIRECTIVE_INCLUDE
#include "config_read.c"
#endif

static	void	new_class(int);
static	char	*getfield(char *), confchar(u_int);
static	int	openconf(void);
static	void	validate(aConfItem *);
static	int	dgets(int, char *, int);
static	aClass	*get_class(int, int);
static	aConfItem	*initconf(void);
static	void	showconf(void);

struct	wordcount {
	char *filename;
	int min;
	int max;
	struct wordcount *next;
};
static	void	mywc(void);
static	int	checkSID(char *, int);
static  struct	wordcount *	findConfLineNumber(int);
#ifdef M4_PREPROC
static	char *	mystrinclude(char *, int);
static	int	simulateM4Include(struct wordcount *, int, char *, int);
#endif

static	int	numclasses = 0, *classarr = (int *)NULL, debugflag = 0;
static	char	nullfield[] = "";
static	char	maxsendq[12];
static	struct	wordcount *files;

#define	SHOWSTR(x)	((x) ? (x) : "*")

int	main(int argc, char *argv[])
{
#ifdef DEBUGMODE
	struct wordcount *filelist = files;
#endif
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
	else if (argc > 1 && !strncmp(argv[1], "-s", 2))
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
	/* Initialize counters to be able to print line number even with m4 */
	mywc();
#ifdef DEBUGMODE
	for(filelist = files; filelist->next; filelist = filelist->next)
	  fprintf(stderr, "%s: Min %d - Max %d\n",
		  filelist->filename, filelist->min, filelist->max);
#endif
	/* If I do not use result as temporary return value
	 * I get loops when M4_PREPROC is defined - Babar */
	result = initconf();
	validate(result);
	return 0;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
static	int	openconf()
{
#ifdef	M4_PREPROC
	int	pi[2];

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
		(void)execlp(M4_PATH, "m4", IRCDM4_PATH, configfile, 0);
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
static	void	showconf()
{
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	aConfig *p, *p2;
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
	p2 = config_read(fd, 0, new_config_file(configfile, NULL, 0));
	for(p = p2; p; p = p->next)
		printf("%s\n", p->line);
	config_free(p2);
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
#endif
	(void)close(fd);
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

static	aConfItem 	*initconf()
{
	int	fd;
	char	*tmp, *tmp3 = NULL, *s;
	int	ccount = 0, ncount = 0, dh, flags = 0, nr = 0;
	aConfItem *aconf = NULL, *ctop = NULL;
	int	mandatory_found = 0, valid = 1;
	struct wordcount *filelist = files;
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	char    *line;
	aConfig *ConfigTop, *p, *p2;
#else
	char	line[512], c[80];
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
	ConfigTop = config_read(fd, 0, new_config_file(configfile, NULL, 0));
	for(p = ConfigTop; p; p = p->next)
#else
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
		aconf->clients = ++nr;
	    	while (filelist->next && (filelist->max < nr))
	    		filelist = filelist->next;

#if defined(CONFIG_DIRECTIVE_INCLUDE)
		line = p->line;
#else
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
                        (void)fprintf(stderr, "%s:%d ERROR: Bad config line (%s)\n",
				filelist->filename, nr - filelist->min, line);
			(void)fprintf(stderr,
			      "\tWrong delimiter? (should be %c)\n",
				      IRCDCONF_DELIMITER);
                        continue;
                    }

		if (debugflag)
			(void)printf("\n%s:%d\n%s\n",
				filelist->filename, nr - filelist->min, line);
		(void)fflush(stdout);

		tmp = getfield(line);
		if (!tmp)
		    {
                        (void)fprintf(stderr, "%s:%d ERROR: no field found\n",
				filelist->filename, nr - filelist->min);
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
                        (void)fprintf(stderr, "%s:%d\tWARNING: unknown conf line letter (%c)\n",
				filelist->filename, nr - filelist->min, *tmp);
			break;
		    }

		if (IsIllegal(aconf))
			continue;

		for (;;) /* Fake loop, that I can use break here --msa */
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
		                        (void)fprintf(stderr, "%s:%d\tWARNING: No class. Default 0\n",
						filelist->filename, nr - filelist->min);
				aconf->class = get_class(atoi(tmp), nr);
			    }
			tmp3 = getfield(NULL);
			break;
		    }

		if ((aconf->status & CONF_CLIENT) && tmp3)
		{
			/* Parse I-line flags */
			for(s = tmp3; *s; ++s)
			{
				switch (*s)
				{
				case 'R':
					aconf->flags |= CFLAG_RESTRICTED;
					break;
				case 'D':
					aconf->flags |= CFLAG_RNODNS;
					break;
				case 'I':
					aconf->flags |= CFLAG_RNOIDENT;
					break;
				case 'E':
					aconf->flags |= CFLAG_KEXEMPT;
					break;
				case 'N':
					aconf->flags |= CFLAG_NORESOLVE;
					break;
				case 'M':
					aconf->flags |= CFLAG_NORESOLVEMATCH;
					break;
				case 'F':
					aconf->flags |= CFLAG_FALL;
					break;
				default:
					(void)fprintf(stderr, "%s:%d\tWARNING: "
						"unknown I-line flag: %c\n",
						filelist->filename,
						nr - filelist->min, *s);
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
				(void)fprintf(stderr,"%s:%d ERROR: no class #\n",
					filelist->filename, nr - filelist->min);
				continue;
			    }
			if (!tmp)
			    {
				(void)fprintf(stderr,
					"%s:%d\tWARNING: missing sendq field\n",
					filelist->filename, nr - filelist->min);
				(void)fprintf(stderr, "\t\t default: %d\n", QUEUELEN);
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
#ifdef	UNIXPORT
			struct	stat	sb;

			if (!aconf->host)
				(void)fprintf(stderr, "%s:%d ERROR: null host field in P-line\n",
					filelist->filename, nr - filelist->min);
			else if (index(aconf->host, '/'))
			    {
				if (stat(aconf->host, &sb) == -1)
				    {
					(void)fprintf(stderr, "%s:%d ERROR: (%s)\n",
						filelist->filename, nr - filelist->min, aconf->host);
					perror("stat");
				    }
				else if ((sb.st_mode & S_IFMT) != S_IFDIR)
					(void)fprintf(stderr, "%s:%d ERROR: %s not directory\n",
						filelist->filename, nr - filelist->min, aconf->host);
			    }
#else
			if (!aconf->host)
				(void)fprintf(stderr, "%s:%d ERROR: null host field in P-line\n",
					filelist->filename, nr - filelist->min);
			else if (index(aconf->host, '/'))
				(void)fprintf(stderr, "%s:%d\tWARNING: %s\n",
					filelist->filename, nr - filelist->min,
					"/ present in P-line for non-UNIXPORT configuration");
#endif
			aconf->class = get_class(0, nr);
			goto print_confline;
		    }

		if (aconf->status & CONF_SERVER_MASK &&
		    (!aconf->host || index(aconf->host, '*') ||
		     index(aconf->host, '?')))
		    {
			(void)fprintf(stderr, "%s:%d ERROR: bad host field\n",
				filelist->filename, nr - filelist->min);
			continue;
		    }

		if (aconf->status & CONF_SERVER_MASK && BadPtr(aconf->passwd))
		    {
			(void)fprintf(stderr, "%s:%d ERROR: empty/no password field\n",
				filelist->filename, nr - filelist->min);
			continue;
		    }

		if (aconf->status & CONF_SERVER_MASK && !aconf->name)
		    {
			(void)fprintf(stderr, "%s:%d ERROR: bad name field\n",
				filelist->filename, nr - filelist->min);
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
				(void)fprintf(stderr, "%s:%d ERROR: multiple M-lines\n",
					filelist->filename, nr - filelist->min);

			else
				flags |= aconf->status;
			if (tmp && *tmp)
			    {
			    	/* Check for SID at the end of M: line */
			    	if (debugflag > 2)
			    		(void)printf("SID is set to: %s\n", tmp);
			    	if (checkSID(tmp, nr))
					(void)fprintf(stderr, "%s:%d ERROR: SID invalid\n",
						filelist->filename, nr - filelist->min);
			    }
			else
				(void)fprintf(stderr, "%s:%d ERROR: no SID in M-line\n",
					filelist->filename, nr - filelist->min);
		    }
		if (aconf->status & CONF_ADMIN)
		    {
			if (flags & aconf->status)
				(void)fprintf(stderr, "%s:%d ERROR: multiple A-lines\n",
					filelist->filename, nr - filelist->min);
			else
				flags |= aconf->status;
			if (tmp && *tmp)
			    {
			    	/* Check for Network at the end of A: line */
			    	if (debugflag > 2)
			    		(void)printf("Network is set to: %s\n", tmp);
			    }
			else
				(void)fprintf(stderr, "%s:%d ERROR: no network in A-line\n",
					filelist->filename, nr - filelist->min);
		    }

		if (aconf->status & CONF_VER)
		    {
			if (*aconf->host && !index(aconf->host, '/'))
				(void)fprintf(stderr, "%s:%d\tWARNING: No / in V line.\n",
					filelist->filename, nr - filelist->min);
			else if (*aconf->passwd && !index(aconf->passwd, '/'))
				(void)fprintf(stderr, "%s:%d\tWARNING: No / in V line.\n",
					filelist->filename, nr - filelist->min);
		    }
print_confline:
		if (debugflag > 8)
			(void)printf("(%d) (%s) (%s) (%s) (%d) (%s)\n",
			      aconf->status, aconf->host, aconf->passwd,
			      aconf->name, aconf->port, maxsendq);
		(void)fflush(stdout);
		if (aconf->status & (CONF_SERVER_MASK|CONF_HUB|CONF_LEAF))
		    {
			aconf->next = ctop;
			ctop = aconf;
			aconf = NULL;
		    }
	    }
#if defined(CONFIG_DIRECTIVE_INCLUDE)
	config_free(ConfigTop);
#endif
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif

	if (!(mandatory_found & CONF_ME))
	    {
	        fprintf(stderr, "No M-line found (mandatory)\n");
		valid = 0;
	    }

	if (!(mandatory_found & CONF_ADMIN))
	    {
		fprintf(stderr, "No A-line found (mandatory)\n");
		valid = 0;
	    }

	if (!(mandatory_found & CONF_LISTEN_PORT))
	    {
		fprintf(stderr, "No P-line found (mandatory)\n");
		valid = 0;
	    }

	if (!(mandatory_found & CONF_CLIENT))
	    {
		fprintf(stderr, "No I-line found (mandatory)\n");
		valid = 0;
	    }

	if (valid)
	{
		return ctop;
	}
	return NULL;
}

static	aClass	*get_class(int cn, int ln)
{
	static	aClass	cls;
	int	i = numclasses - 1;
	struct wordcount *filelist;

	cls.class = -1;
	for (; i >= 0; i--)
		if (classarr[i] == cn)
		    {
			cls.class = cn;
			break;
		    }
	if (i == -1)
	    {
	    	filelist = findConfLineNumber(ln);
		(void)fprintf(stderr, "%s:%d\tWARNING: class %d not found\n",
			filelist->filename, ln - filelist->min, cn);
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


static	void	validate(aConfItem *top)
{
	Reg	aConfItem *aconf, *bconf;
	u_int	otype = 0, valid = 0;
	struct wordcount *filelist;

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

	(void) fprintf(stderr, "\n");
	for (aconf = top; aconf; aconf = aconf->next)
		if (aconf->status & CONF_MATCH)
			valid++;
		else
		    {
			filelist = findConfLineNumber(aconf->clients);
			for(filelist = files; filelist->next && (filelist->max < aconf->clients); filelist = filelist->next);
			(void)fprintf(stderr, "%s:%d\tWARNING: Unmatched %c:%s:%s:%s\n",
				filelist->filename, aconf->clients - filelist->min,
				confchar(aconf->status), aconf->host,
				SHOWSTR(aconf->passwd), aconf->name);
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
					(void)fprintf(stderr, "%s\tWARNING: No final new line.\n",
						filelist->filename);
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

				(void)fprintf(stderr, "%s:%d\tWARNING: Line too long. Only %d characters allowed\n\t(%s)\n",
					filelist->filename, nr - filelist->min, sizeof(line), line);
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

static void	mywc()
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
	struct wordcount *filelist;

	/* First SID char shall be numeric, rest must be alpha */
    	filelist = findConfLineNumber(nr);
	if (!isdigit(*s))
	    {
		(void)fprintf(stderr,
			"%s:%d ERROR: SID invalid: 1st character must be numeric (%s)\n",
			filelist->filename, nr - filelist->min, sid);
		++rc;
	    }
	++s; ++len;
    	for (; *s; ++s, ++len)
		if (!isalnum(*s))
		    {
                        (void)fprintf(stderr,
                        	"%s:%d ERROR: SID invalid: (%s) wrong character (%c)\n",
				filelist->filename, nr - filelist->min, sid, *s);
			++rc;
		}
	if (!(len == SIDLEN))
	    {
		(void)fprintf(stderr,
			"%s:%d ERROR: SID invalid: wrong size (%d should be %d)\n",
			filelist->filename, nr - filelist->min, len, SIDLEN);
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
		(void)fprintf(stderr, "%s:%d\tWARNING: while including %s\n\t\t%s\n",
			filelist->filename, nr - filelist->min, s,
			"spaces before include. (First line of include file will be shift)");
	}
	return s;
}
#endif

/* Locates file and line number of a config line */
static  struct	wordcount *	findConfLineNumber(int nr)
{
	struct wordcount *filelist = files;
	for(; filelist->next && (filelist->max < nr); filelist = filelist->next);
	return filelist;
}
