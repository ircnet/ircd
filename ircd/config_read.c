
/* used in config_error() */
#define CF_NONE 0
#define CF_WARN 1
#define CF_ERR  2

/* max file length */
#define FILEMAX 255

/* max nesting depth. ircd.conf itself is depth = 0 */
#define MAXDEPTH 13

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
static aConfig	*config_read(int, int, aFile *);
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
aConfig *config_read(int fd, int depth, aFile *curfile)
{
	int len, linenum;
	struct stat fst;
	char *i, *address;
	aConfig *ConfigTop = NULL;
	aConfig *ConfigCur = NULL;

	if (curfile == NULL)
	{
		curfile = new_config_file(configfile, NULL, 0);
	}
	fstat(fd, &fst);
	len = fst.st_size;
	if ((address = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0))
		== MAP_FAILED)
	{
		config_error(CF_ERR, curfile, 0, 
			"mmap failed reading config");
		return NULL;
	}

	i = address;
	linenum = 0;
	while (i < address + len)
	{
		char *p;
		aConfig	*new;
		int linelen;

		/* eat empty lines first */
		while (*i == '\n' || *i == '\r')
		{
			if (*i == '\n')
				linenum++;
			i++;
		}
		p = strchr(i, '\n');
		if (p == NULL)
		{
			/* EOF without \n, I presume */
			p = address + len;
		}
		linenum++;

		if (*i == '#' && strncasecmp(i+1, "include ", 8) == 0)
		{
			char	*start = i + 9, *end = p;
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
				filep += sprintf(file, IRCDCONF_DIR);
			}
			if (end - start + filep - file >= FILEMAX)
			{
				config_error(CF_ERR, curfile, linenum,
					"too long filename (max %d with "
					"path)", FILEMAX);
				goto eatline;
			}
			savefilep = filep;
			memcpy(filep, start, end - start + 1);
			filep += end - start + 1;
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
			if ((fd = open(file, O_RDONLY)) < 0)
			{
				config_error(CF_ERR, curfile, linenum,
					"cannot open \"%s\"", savefilep);
				goto eatline;
			}
			ret = config_read(fd, depth + 1,
				new_config_file(file, curfile, linenum));
			close(fd);
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
			i = p + 1;
			continue;
		}
eatline:
		linelen = p - i;
		if (*(p - 1) == '\r')
			linelen--;
		new = (aConfig *)malloc(sizeof(aConfig));
		new->line = (char *) malloc((linelen+1) * sizeof(char));
		memcpy(new->line, i, linelen);
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
		i = p + 1;
	}
	munmap(address, len);
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
	aFile *tmp = (aFile *) malloc(sizeof(aFile));

	tmp->filename = strdup(filename);
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
		fprintf(stdout, "%s", vbuf);
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
