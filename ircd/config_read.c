/* "@(#)$Id: config_read.c,v 1.33 2005/05/13 17:39:12 chopin Exp $"; */

/* used in config_error() */
#define CF_NONE 0
#define CF_WARN 1
#define CF_ERR  2

/* max file length */
#define FILEMAX 255

/* max nesting depth. ircd.conf itself is depth = 0 */
#define MAXDEPTH 13

#if defined(__sun) || defined(__sun__) || defined(sun)
/* Sun has a buggy implementation of FILE functions 
** (they do not work when fds 0-255 are already used).
** ircd-ratbox 1.5-3 had a nice reimplementation, so I took it. --B. */
#define FILE FBFILE
#define fclose fbclose
#define fdopen fdbopen
#define fgets fbgets
#define fopen fbopen
#if !defined(HAVE_STRLCPY)
#define strlcpy(x, y, N) strncpyzt((x), (y), (N))
#endif
#include "fileio.c"
#endif

typedef struct File aFile;
struct File
{
	char *filename;
	int includeline;
	aFile *parent;
	aFile *next;
};

typedef struct Config aConfig;
struct Config
{
	char *line;
	int linenum;
	aFile *file;
	aConfig *next;
};

#ifdef CONFIG_DIRECTIVE_INCLUDE
static aConfig	*config_read(FILE *, int, aFile *);
static void	config_free(aConfig *);
aFile	*new_config_file(char *, aFile *, int);
void	config_error(int, aFile *, int, char *, ...);
#else
void	config_error(int, char *, int, char *, ...);
#endif


#ifdef CONFIG_DIRECTIVE_INCLUDE
/* 
** Syntax of include is simple (but very strict):
** #include "filename"
** # must be first char on the line, word include, one space, then ",
** filename and another " must be the last char on the line.
** If filename does not start with slash, it's loaded from IRCDCONF_DIR.
*/

/* read from supplied fd, putting line by line onto aConfig struct.
** calls itself recursively for each #include directive */
aConfig *config_read(FILE *fd, int depth, aFile *curfile)
{
	int linenum = 0;
	aConfig *ConfigTop = NULL;
	aConfig *ConfigCur = NULL;
	char	line[BUFSIZE+1];
	FILE	*fdn;

	if (curfile == NULL)
	{
		curfile = new_config_file(configfile, NULL, 0);
	}
	while (NULL != (fgets(line, sizeof(line), fd)))
	{
		char *p;
		aConfig	*new;
		int linelen;

		linenum++;
		if ((p = strchr(line, '\n')) != NULL)
			*p = '\0';
		if ((p = strchr(line, '\r')) != NULL)
			*p = '\0';
		if (*line == '\0')
			continue;
		linelen = strlen(line);

		if (*line == '#' && strncasecmp(line + 1, "include ", 8) == 0)
		{
			char	*start = line + 9;
			char	*end = line + linelen - 1;
			char	file[FILEMAX + 1];
			char	*filep = file;
			char	*savefilep;
			aConfig	*ret;
			aFile	*tcf;

			/* eat all white chars around filename */
			while (isspace(*end))
			{
				end--;
			}
			while (isspace(*start))
			{
				start++;
			}

			/* remove quotes, when they're both there */
			if (*start == '"' && *end == '"')
			{
				start++;
				end--;
			}
			if (start >= end)
			{
				config_error(CF_ERR, curfile, linenum,
					"config: empty include");
				goto eatline;
			}
			*filep = '\0';
			if (depth >= MAXDEPTH)
			{
				config_error(CF_ERR, curfile, linenum,
					"config: too nested (max %d)",
					depth);
				goto eatline;
			}
			if (*start != '/')
			{
				filep += snprintf(file, FILEMAX,
					"%s", IRCDCONF_DIR);
			}
			if ((end - start) + (filep - file) >= FILEMAX)
			{
				config_error(CF_ERR, curfile, linenum,
					"too long filename (max %d with "
					"path)", FILEMAX);
				goto eatline;
			}
			savefilep = filep;
			memcpy(filep, start, (end - start) + 1);
			filep += (end - start) + 1;
			*filep = '\0';
			for (tcf = curfile; tcf; tcf = tcf->parent)
			{
				if (0 == strcmp(tcf->filename, file))
				{
					config_error(CF_ERR, curfile,
						linenum,
						"would loop include");
					goto eatline;
				}
			}
			if ((fdn = fopen(file, "r")) == NULL)
			{
				config_error(CF_ERR, curfile, linenum,
					"cannot open \"%s\"", savefilep);
				goto eatline;
			}
			ret = config_read(fdn, depth + 1,
				new_config_file(file, curfile, linenum));
			fclose(fdn);
			if (ConfigCur)
			{
				ConfigCur->next = ret;
			}
			else
			{
				ConfigTop = ret;
				ConfigCur = ret;
			}
			while ((ConfigCur && ConfigCur->next))
			{
				ConfigCur = ConfigCur->next;
			}
			/* good #include is replaced by its content */
			continue;
		}
eatline:
		new = (aConfig *)MyMalloc(sizeof(aConfig));
		new->line = (char *) MyMalloc((linelen+1) * sizeof(char));
		memcpy(new->line, line, linelen);
		new->line[linelen] = '\0';
		new->linenum = linenum;
		new->file = curfile;
		new->next = NULL;
		if (ConfigCur)
		{
			ConfigCur->next = new;
		}
		else
		{
			ConfigTop = new;
		}
		ConfigCur = new;
	}
	return ConfigTop;
}

/* should be called with topmost config struct */
void config_free(aConfig *cnf)
{
	aConfig *p;
	aFile *pf, *pt;

	if (cnf == NULL)
	{
		return;
	}

	pf = cnf->file;
	while(pf)
	{
		pt = pf;
		pf = pf->next;
		MyFree(pt->filename);
		MyFree(pt);
	}
	while (cnf)
	{
		p = cnf;
		cnf = cnf->next;
		MyFree(p->line);
		MyFree(p);
	}
	return;
}

aFile *new_config_file(char *filename, aFile *parent, int fnr)
{
	aFile *tmp = (aFile *) MyMalloc(sizeof(aFile));

	tmp->filename = mystrdup(filename);
	tmp->includeline = fnr;
	tmp->parent = parent;
	tmp->next = NULL;

	/* First get to the root of the file tree */
	while (parent && parent->parent)
	{
		parent = parent->parent;
	}
	/* Then go to the end to add a new one */
	while (parent && parent->next)
	{
		parent = parent->next;
	}
        if (parent)
        {
		parent->next = tmp;
        }
        return tmp;
}
#endif /* CONFIG_DIRECTIVE_INCLUDE */

#ifdef CONFIG_DIRECTIVE_INCLUDE
void config_error(int level, aFile *curF, int line, char *pattern, ...)
#else
void config_error(int level, char *filename, int line, char *pattern, ...)
#endif
{
	static int etclen = 0;
	va_list va;
	char vbuf[8192];
	char *filep;
#ifdef CONFIG_DIRECTIVE_INCLUDE
	char *filename = curF->filename;
#endif

	if (!etclen)
	{
		etclen = strlen(IRCDCONF_DIR);
	}

	va_start(va, pattern);
	vsprintf(vbuf, pattern, va);
	va_end(va);

	/* no need to show full path, if the same dir */
	filep = filename;
	if (0 == strncmp(filename, IRCDCONF_DIR, etclen))
		filep += etclen;
#ifdef CHKCONF_COMPILE
	if (level == CF_NONE)
	{
		fprintf(stdout, "%s\n", vbuf);
	}
	else
	{
		fprintf(stderr, "%s:%d %s%s\n", filep, line,
			((level == CF_ERR) ? "ERROR: " : "WARNING: "), vbuf);
# ifdef CONFIG_DIRECTIVE_INCLUDE
		while ((curF && curF->parent))
		{
			filep = curF->parent->filename;
			if (0 == strncmp(filep, IRCDCONF_DIR, etclen))
				filep += etclen;
			fprintf(stderr, "\tincluded in %s:%d\n",
				filep, curF->includeline);
			curF = curF->parent;
		}
# endif
	}
#else
	if (level != CF_ERR)
		return;

	/* for ircd &ERRORS reporting show only config file name,
	** without full path. --B. */
	if (filep[0] == '/')
		filep = strrchr(filename, '/') + 1;
	sendto_flag(SCH_ERROR, "config %s:%d %s", filep, line, vbuf);
#endif
	return;
}
