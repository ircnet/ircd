/*
 *   IRC - Internet Relay Chat, common/irc_sprintf.c
 *   Copyright (C) 2002 Piotr Kucharski
 *
 *   BSD licence applies. Yeah!
 *
 */

#ifndef lint
static const volatile char rcsid[] = "$Id: irc_sprintf.c,v 1.5 2004/10/01 20:22:12 chopin Exp $";
#endif

#define IRC_SPRINTF_C
#include "irc_sprintf_ext.h"
#undef IRC_SPRINTF_C

/*
 * Our sprintf. Currently supports:
 * formats: %s, %d, %i, %u, %c, %%, %x, %X, %o
 * flags: l, ll, '-', '+', space, 0 (zeropadding), 0-9 (width),
 * '#' (for o,x,X only), '*' (width), '.' (precision)
 * 
 * Soon will handle: %y/%Y returning ->name or ->uid
 * from an aClient pointer, depending on target.
 */

#define	MAXDIGS	32
#undef _NOLONGLONG

#define	MEMSET(c, n){register int k = n; while (k--) *buf++ = c;}
#define	MEMCPY(s, n){register int k = n; while (k--) *buf++ = *s++;}

static char	dtmpbuf[MAXDIGS];	/* scratch buffer for numbers */

int irc_sprintf(aClient *target, char *buf, char *format, ...)
#include "irc_sprintf_body.c"

#define IRC_SPRINTF_V 1

int irc_vsprintf(aClient *target, char *buf, char *format, va_list ap)
#include "irc_sprintf_body.c"

#if 0
error SN version is unsafe so far, sorry

#define IRC_SPRINTF_SN 1

int irc_vsnprintf(aClient *target, char *buf, size_t size, char *format, va_list ap)
#include "irc_sprintf_body.c"

#undef IRC_SPRINTF_V

int irc_snprintf(aClient *target, char *buf, size_t size, char *format, ...)
#include "irc_sprintf_body.c"

#undef IRC_SPRINTF_SN

#endif /* 0 */

