/* $Id: irc_sprintf_body.c,v 1.2 2003/07/18 16:24:25 chopin Exp $ */
{
#ifndef IRC_SPRINTF_V
	va_list ap;
#endif
#ifdef IRC_SPRINTF_SN
	char	*bufstart = buf;
#endif
#ifdef IRC_SPRINTF_DEBUG
	char	*sformat = format;
# ifdef IRC_SPRINTF_SN
	char	*bufend = buf + size;
# else
	char	*bufend = buf + BUFSIZE;
# endif
#endif
	int	n;		/* (calculated) size of parameter */
	int	count = 0;	/* counter of bytes written to buffer */
	int	mult=0;		/* multiplicator or shift for numbers */
	int	radix;		/* parameter base of numbers (8,10,16) */
	static int	width = 0;		/* width of pad */
	static int	precision = 0;	/* precision width */
	static char	plusminus = 0;	/* keeps sign (+/- or space) */
	static int	zeropad = 0;	/* padding with zeroes */
	static int	minus = 0;		/* padding to left or right */
	static int	unsig = 0;		/* unsigned mark */
	static int	dotseen = 0;	/* dot mark, precision mode */
	static int	hash = 0;		/* #-mark for 0 to octals and 0x to hex */
	unsigned long val;	/* va_args for ints and longs */
	unsigned long nqval;	/* temp for ints and longs */
#ifndef _NOLONGLONG
	unsigned long long ll_val=0;	/* va_args for long longs */
	unsigned long long ll_nqval;	/* temp for long longs */
#endif
	const char	*tab;	/* pointer to proper ato(o,dd,x) table */
	register char	cc;		/* current fmt char */
	register char	nomodifiers = 1;
	register char	ilong = 0;		/* long */

#ifdef IRC_SPRINTF_SN
#define CONT    goto farend
#else
#define CONT    continue
#endif

#ifdef IRC_SPRINTF_DEBUG
	assert( buf != NULL );
	assert( format != NULL );
# ifdef IRC_SPRINTF_SN
	assert( size <= BUFSIZE );
# endif
#endif

#ifndef IRC_SPRINTF_V
	va_start(ap, format);
#endif
	while ( ( cc = *format++ ) )
	{
		/* if we had no modifiers, go shortcut */

/* Interesting, we gain speed in single "%s" on FreeBSD/x86
   (vs. Solaris/Sparc) using this define.
   (We lose speed on Solaris/Sparc, though)
   Anyone care to explain? --Beeth */
#define PERCENT_LAST 1
#ifndef PERCENT_LAST
		if ( cc != '%' )
		{
			*buf++ = cc;
			count++;
			continue;	/* perhaps CONT? */
		}
#else
		if ( cc == '%' )
		{
#endif
		/* % has been found */
		if ( !nomodifiers )
		{
			width = zeropad = minus = plusminus = ilong =
				hash = unsig = dotseen = precision = 0;
			nomodifiers = 1;
		}

chswitch:
		cc = *format++;
		if ( cc == 's' )
		{
			register const char *sval = va_arg(ap, const char *);

			/* shortcut */
			if ( nomodifiers && (*buf = *sval) )
			{
/* here's something wrong; either count is not updated or else */
				count++;
				while ( (*++buf = *++sval) )
					count++;
				CONT;
			}

			n = count;
			if ( !dotseen )
			{
				while ( *sval )
				{
					*buf++ = *sval++;
					count++;
				}
			}
			else
			{
				while ( *sval && precision-- )
				{
					*buf++ = *sval++;
					count++;
				}
			}
			if ( width )
			{
				register int	fil;

				n = count - n;
				fil = width - n;
				if ( fil > 0 )
				{
					if ( !minus )
					{
						sval = buf - n;
						memmove(sval + fil,
							sval, n);
						buf = sval;
						MEMSET(zeropad ? '0'
							: ' ', fil);
						buf += n;
					}
					else
						MEMSET(' ', fil);
					count += fil;
				}
			}
			CONT;
		}
		/* I fear if (cc == 'd' || cc == 'i') would be sloooow */
		if ( cc == 'd' )
decimal:
		{
			register char	*pdtmpbuf = &dtmpbuf[MAXDIGS];	/* pointer inside scratch buffer */
			register int	fil = 0;

			hash = 0;
			radix = 10;
			tab = atod_tab + 2;
numbers:
#ifndef _NOLONGLONG
			if ( ilong == 2 )
			{
				ll_val = va_arg(ap, long long);
				if ( !unsig && (long long)ll_val < 0 )
				{
					fil = -1;
					ll_val = - (long long)ll_val;
				}
				val = ll_val;
			}
			else 
#endif
			if ( ilong )
			{
				val = va_arg(ap, long);
				if ( !unsig && (long) val < 0 )
				{
					fil = -1;
					val = - (long)val;
				}
			}
			else /* if ( !ilong ) */
			{
				val = va_arg(ap, int);
				if ( !unsig && (int)val < 0 )
				{
					fil = -1;
					val = - (int)val;
				}
			}
			if ( fil == -1 )
				plusminus = '-';

			/* pdtmpbuf = dtmpbuf + MAXDIGS; */
#ifdef IRC_SPRINTF_DEBUG
			memset(dtmpbuf, 0, MAXDIGS);
#endif
			if ( !val )	/* is it me or val==0 is slower? */
			{
				if ( nomodifiers )
				{
					*buf++ = '0';
					count++;
					CONT;
				}
				else
				{
					*--pdtmpbuf = '0';
				}
			}
			else
			{
#ifndef _NOLONGLONG
				/* I hate duplicating code, but using
				   long long vars for long operations
				   slows things down twice! */
				if ( ilong == 2 )
				{
					register const char *pp;
					
					if ( radix == 10 )
					{
						do
						{
							ll_nqval = ll_val / 1000;
							pp = tab + ((ll_val -
								(ll_nqval * 1000)) << 2);
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp;
						} while ( ( ll_val = ll_nqval ) > 0 );
					}
					else
					{
						/* with octal and hex we take
						   advantage of shifting */
						do
						{
							ll_nqval = ll_val >> mult;
							pp = tab + ((ll_val -
								(ll_nqval << mult)) << 1);
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp;
						} while ( ( ll_val = ll_nqval ) > 0 );
					}
				}
				else
#endif /* _NOLONGLONG */
				{
					register const char *pp;

					if ( radix == 10 )
					{
						do
						{
							nqval = val / 1000;
							pp = tab + ((val -
								(nqval * 1000)) << 2);
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp;
						} while ( ( val = nqval ) > 0 );
					}
					else
					{
						/* with base 2 (octal and hex)
						   we take advantage of shifting */
						do
						{
							nqval = val >> mult;
							pp = tab + ((val -
								(nqval << mult)) << 1);
							*--pdtmpbuf = *pp--;
							*--pdtmpbuf = *pp;
						} while ( ( val = nqval ) > 0 );
					}
				}	/* if ( ilong == 2 ) */
				/* eat (probably) not needed zeroes */
				while ( '0' == *pdtmpbuf )
				{
					pdtmpbuf++;
				}
			}	/* val != 0 */
			if ( nomodifiers )
			{
				/* shortcut */
				if ( plusminus )
				{
					/* due to nomodifiers, this is case
					   of negative number */
					*buf++ = plusminus;
					count++;
				}
				n = dtmpbuf + MAXDIGS - pdtmpbuf;
				MEMCPY(pdtmpbuf, n);
				count += n;
				CONT;
			}
			if ( dotseen )
			{
				zeropad = 1;
				if ( width < precision )
				{
					width = precision;
					minus = 0;
				}
			}
			/* for octals add '0', for hex 0x or 0X */
			if ( hash )
			{
				/* if we're padding with zeroes, make sure
				   hash string goes before them */
				if ( zeropad )
				{
					*buf++ = '0';
					if ( !dotseen )
					{
						width--;
					}
					if ( radix == 16 )
					{
						*buf++ = hash;
						if ( !dotseen )
						{
							width--;
						}
					}
				}
				else
				{
					if ( radix == 16 )
					{
						*--pdtmpbuf = hash;
					}
					*--pdtmpbuf = '0';
				}
			}
			n = dtmpbuf + MAXDIGS - pdtmpbuf;
			if ( dotseen && n >= precision )
				zeropad = 0;
			if ( width > 0 && ( fil = width - n ) > 0
				&& !minus )
			{
				count += fil;
				if ( !zeropad )
					MEMSET(' ', fil);
				if ( plusminus )
				{
					*buf++ = plusminus;
					count++;
				}
				if ( zeropad )
				{
					if ( dotseen )
					{
						MEMSET(' ', width - precision );
						MEMSET('0', precision - n );
					}
					else
					{
						MEMSET('0', fil);
					}
				}
			}
			else if ( plusminus )
			{
					*buf++ = plusminus;
					count++;
			}
			MEMCPY(pdtmpbuf, n);
			count += n;
			if ( minus && !dotseen && width > 0 && fil > 0 )
			{
				MEMSET(' ', fil)
				count += fil;
			}
			CONT;
		}
		if ( cc == 'u' )
		{
			unsig = 1;
			goto decimal;
		}
		if ( cc == 'c' )
		{
			*buf++ = (char) va_arg(ap, int);
			count++;
			CONT;
		}
		if ( cc == 'l' )
		{
			ilong = 1;
#ifndef _NOLONGLONG
			if ( *format == 'l' )
			{
				format++;
				ilong = 2;
			}
#endif
			goto chswitch;
		}
		if ( cc == '-' )
		{
			minus = 1;
			zeropad = 0;
			nomodifiers = 0;
			goto chswitch;
		}
		if ( cc >= '0' && cc <= '9' )
		{
			register int num = cc - '0';

			if ( cc == '0' )
				if ( !dotseen && !minus )
					zeropad = 1;
			while ( ( cc = *format ) >= '0' && cc <= '9' )
			{
				num = num * 10 + cc - '0';
				format++;
			}
			if ( !dotseen )
				width = num;
			else
				precision = num;
			nomodifiers = 0;
			goto chswitch;
		}
		if ( cc == 'x' )
		{
			unsig = 1;
			radix = 16;
			mult = 8;
			tab = atox_tab + 1;
			if ( hash )
				hash = 'x';
			goto numbers;
		}
		if ( cc == 'X' )
		{
			unsig = 1;
			radix = 16;
			mult = 8;
			tab = atoxx_tab + 1;
			if ( hash )
				hash = 'X';
			goto numbers;
		}
		if ( cc == 'o' )
		{
			unsig = 1;
			radix = 8;
			mult = 6;
			tab = atoo_tab + 1;
			if ( hash )
				hash = 8;
			goto numbers;
		}
		if ( cc == '#' )
		{
			hash = 1;
			nomodifiers = 0;
			goto chswitch;
		}
		if ( cc == '%' )
		{
			*buf++ = '%';
			count++;
			CONT;
		}
		if ( cc == 'i' )
		{
			goto decimal;
		}
		if ( cc == '+' )
		{
			plusminus = '+';
			nomodifiers = 0;
			goto chswitch;
		}
		if ( cc == ' ' )
		{
			plusminus = ' ';
			nomodifiers = 0;
			goto chswitch;
		}
		if ( cc == '*' )
		{
			if ( !dotseen )
			{
				width = va_arg(ap, int);
				if ( width < 0 )
				{
					minus = 1;
					zeropad = 0;
					width = -width;
				}
			}
			else
			{
				precision = va_arg(ap, int);
				if ( precision < 0 )
				{
					precision = 0;
					dotseen = 0;
				}
			}
			nomodifiers = 0;
			goto chswitch;
		}
		if (cc == '.' )
		{
			dotseen = 1;
			nomodifiers = 0;
			goto chswitch;
		}
		/* so far just leave literal %y in the string */
		if ( cc == 'y' || cc == 'Y' )
		{
			*buf++ = '%';
			count++;
			format--;
			CONT;
		}
#if 0
		/* I have no idea, when I would bother doing these */
		else if (
			cc == 'f' ||
			cc == 'e' ||
			cc == 'E' ||
			cc == 'g' ||
			cc == 'G' ||
			cc == '$' ||
			cc == '\'' )
		{
				format--;	/* back out!? */
				continue;
		}
#endif
		else
		{
			/* any better ideas?
			   excluding bailing out to real sprintf? */
#ifdef IRC_SPRINTF_DEBUG
			fprintf(stderr, "Unknown char \"%c\" ", cc);
			fprintf(stderr, "in format \"%s\"\n", sformat);
#endif
			/* anyway, we shouldn't use in ircd formats our
			   own sprintf does not know, should we? ;> */
			abort();
		}
#ifdef PERCENT_LAST
		}
		else
		{
			*buf++ = cc;
			count++;
			continue;	/* perhaps CONT? */
		}
#endif
#ifdef IRC_SPRINTF_SN
farend:
		if ( count >= size )
		{
			bufstart[size-1] = '\0';
			count = size;
			break;
		}
#endif
	}	/* end of while ( ( cc = *format++ ) ) */
	if ( !nomodifiers )
	{
		/* clear them for next sprintf call */
		width = zeropad = minus = plusminus = ilong =
			hash = unsig = dotseen = precision = 0;
		nomodifiers = 1;
	}
#ifndef IRC_SPRINTF_V
	va_end(ap);
#endif
#ifdef IRC_SPRINTF_DEBUG
	*(bufstart+count)='\0';
	fprintf(stderr, "output:>%s<\n", bufstart);
	fprintf(stderr, "fcount:>%d<\n", count);
#endif
	return count;
#undef CONT
}
