
/* max file length */
#define FILEMAX 255

/* max nesting depth. ircd.conf itself is depth = 0 */
#define MAXDEPTH 3

typedef struct Config aConfig;
struct Config
{
	char *line;
	aConfig *next;
};

static aConfig	*config_read(int, int);
static void	config_free(aConfig *);

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
aConfig *config_read(int fd, int depth)
{
	int len;
	struct stat fst;
	char *i, *address;
	aConfig *ConfigTop = NULL;
	aConfig *ConfigCur = NULL;

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
	while (i < address + len)
	{
		char *p;
		aConfig	*new;
		int linelen;

		/* eat empty lines first */
		while (*i == '\n' || *i == '\r')
		{
			i++;
		}
		p = strchr(i, '\n');
		if (p == NULL)
		{
			/* EOF without \n, I presume */
			p = address + len;
		}

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
					i = p + 1;
					continue;
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
					i = p + 1;
					continue;
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
					i = p + 1;
					continue;
				}
				ret = config_read(fd, depth + 1);
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
				i = p + 1;
				continue;
			}
			/* comments and unknown #directives are not discarded,
			** bad #include directives are --B. */
		}
		linelen = p - i;
		if (*(p - 1) == '\r')
			linelen--;
		new = (aConfig *)malloc(sizeof(aConfig));
		new->line = (char *) malloc((linelen+1) * sizeof(char));
		memcpy(new->line, i, linelen);
		new->line[linelen] = '\0';
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

void config_free(aConfig *cnf)
{
	aConfig *p;

	while (cnf)
	{
		p = cnf;
		cnf = cnf->next;
		MyFree(p->line);
		MyFree(p);
	}
}
