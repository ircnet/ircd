
/* used in config_error() */
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

static aConfig	*config_read(int, int, aFile *);
static void	config_free(aConfig *);
#ifdef CONFIG_DIRECTIVE_INCLUDE
#define STACKTYPE aFile
#else
#define STACKTYPE char
#endif
void	config_error(int, STACKTYPE *, int, char *, ...);
aFile	*new_config_file(char *, aFile *, int);


#ifdef CONFIG_DIRECTIVE_INCLUDE
/* 
** Syntax of include is simple (but very strict):
** #include "filename"
** # must be first char on the line, word include, one space, then ",
** filename and another " must be the last char on the line.
** If filename does not start with slash, it's loaded from dir 
** where IRCDCONF_PATH is.
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
#ifdef CHKCONF_COMPILE
		(void)fprintf(stderr, "mmap failed reading config");
#else
		sendto_flag(SCH_ERROR, "mmap failed reading config");
#endif
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

		if (*i == '#')
		{
			char	*start = i + 9, *end = p;

			end--;			/* eat last \n */
			if (*end == '\r')
				end--;		/* ... and \r, if is */

			if (*start == '"' && *end == '"'
				&& strncasecmp(i, "#include ", 9) == 0)
			{
				char	file[FILEMAX + 1];
				char	*filep = file;
				aConfig	*ret;

				*filep = '\0';
				if (depth >= MAXDEPTH)
				{
#ifdef CHKCONF_COMPILE
					(void)fprintf(stderr,
						"config: too nested (%d)",
						depth);
#else
					sendto_flag(SCH_ERROR,
						"config: too nested (%d)",
						depth);
#endif
					goto eatline;
				}
				if (*(start+1) != '/')
				{
					strcat(file, IRCDCONF_PATH);
					filep = strrchr(file, '/') + 1;
					*filep = '\0';
				}
				if (end - start + filep - file >= FILEMAX)
				{
#ifdef CHKCONF_COMPILE
					(void)fprintf(stderr, "config: too "
						"long filename to process");
#else
					sendto_flag(SCH_ERROR, "config: too "
						"long filename to process");
#endif
					goto eatline;
				}
				start++;
				memcpy(filep, start, end - start);
				filep += end - start;
				*filep = '\0';
				if ((fd = open(file, O_RDONLY)) < 0)
				{
#ifdef CHKCONF_COMPILE
					(void)fprintf(stderr,
						"config: error opening %s "
						"(depth=%d)", file, depth);
#else
					sendto_flag(SCH_ERROR,
						"config: error opening %s "
						"(depth=%d)", file, depth);
#endif
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

void config_error(int level, STACKTYPE *stack, int line, char *pattern, ...)
{
	int len;
	static int etclen = 0;
	va_list va;
	char vbuf[BUFSIZE];
	char *filep;
#ifdef CONFIG_DIRECTIVE_INCLUDE
	aFile *curF = stack;
	char *filename = stack->filename;
#else
	char *filename = stack;
#endif

	if (!etclen)
	{
		/* begs to rewrite IRCDCONF_PATH to not have ircd.conf */
		char *etc = IRCDCONF_PATH;

		filep = strrchr(IRCDCONF_PATH, '/') + 1;
		etclen = filep - etc;
	}

	va_start(va, pattern);
	len = vsprintf(vbuf, pattern, va);
	va_end(va);

	/* no need to show full path, if the same dir */
	filep = filename;
	if (0 == strncmp(filename, IRCDCONF_PATH, etclen))
		filep += etclen;
#ifdef CHKCONF_COMPILE
	fprintf(stderr, "%s:%d %s%s\n", filep, line,
		((level == CF_ERR) ? "ERROR: " : "WARNING: "), vbuf);
# ifdef CONFIG_DIRECTIVE_INCLUDE
	while ((curF && curF->parent))
	{
		filep = curF->parent->filename;
		if (0 == strncmp(filep, IRCDCONF_PATH, etclen))
			filep += etclen;
		fprintf(stderr, "\tincluded in %s:%d\n",
			filep, curF->includeline);
		curF = curF->parent;
	}
# endif
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
