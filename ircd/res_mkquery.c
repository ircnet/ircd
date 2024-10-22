/*
 * ++Copyright++ 1985, 1993
 * -
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const volatile char sccsid[] = "@(#)res_mkquery.c	8.1 (Berkeley) 6/4/93";
static const volatile char rcsid[]	= "$Id: res_mkquery.c,v 1.8 2004/10/01 20:22:14 chopin Exp $";
#endif /* LIBC_SCCS and not lint */

#include "os.h"
#include "s_defines.h"
#define RES_MKQUERY_C
#include "s_externs.h"
#undef RES_MKQUERY_C

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 *
 * Parameters:
 *	int op;			opcode of query
 *	const char *dname;	domain name
 *	int class, type;	class and type of query
 *	const u_char *data;	resource record data
 *	int datalen;		length of data
 *	const u_char *newrr_in;	new rr for modify or append
 *	u_char *buf;		buffer to put query
 *	int buflen;		size of buffer
 */
int ircd_res_mkquery(int op, const char *dname, int class, int type,
					 const u_char *data, int datalen, const u_char *newrr_in,
					 u_char *buf, int buflen)
{
	register HEADER *hp;
	register u_char *cp;
	register int	 n;
	u_char			*dnptrs[20], **dpp, **lastdnptr;

	if ((ircd_res.options & RES_INIT) == 0 && ircd_res_init() == -1)
	{
		h_errno = NETDB_INTERNAL;
		return (-1);
	}
#ifdef DEBUG
	if (ircd_res.options & RES_DEBUG)
		printf(";; res_mkquery(%d, %s, %d, %d)\n",
			   op, dname, class, type);
#endif
	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < HFIXEDSZ))
		return (-1);
	bzero(buf, HFIXEDSZ);
	hp		   = (HEADER *) buf;
	hp->id	   = htons(++ircd_res.id);
	hp->opcode = op;
	hp->rd	   = (ircd_res.options & RES_RECURSE) != 0;
	hp->rcode  = NOERROR;
	cp		   = buf + HFIXEDSZ;
	buflen -= HFIXEDSZ;
	dpp		  = dnptrs;
	*dpp++	  = buf;
	*dpp++	  = NULL;
	lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];
	/*
	 * perform opcode specific processing
	 */
	switch (op)
	{
		case QUERY: /*FALLTHROUGH*/
		case NS_NOTIFY_OP:
			if ((buflen -= QFIXEDSZ) < 0)
				return (-1);
			if ((n = ircd_dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
				return (-1);
			cp += n;
			buflen -= n;
			ircd__putshort(type, cp);
			cp += INT16SZ;
			ircd__putshort(class, cp);
			cp += INT16SZ;
			hp->qdcount = htons(1);
			if (op == QUERY || data == NULL)
				break;
			/*
		 * Make an additional record for completion domain.
		 */
			buflen -= RRFIXEDSZ;
			n = ircd_dn_comp((const char *) data, cp, buflen, dnptrs, lastdnptr);
			if (n < 0)
				return (-1);
			cp += n;
			buflen -= n;
			ircd__putshort(T_NULL, cp);
			cp += INT16SZ;
			ircd__putshort(class, cp);
			cp += INT16SZ;
			ircd__putlong(0, cp);
			cp += INT32SZ;
			ircd__putshort(0, cp);
			cp += INT16SZ;
			hp->arcount = htons(1);
			break;

		case IQUERY:
			/*
		 * Initialize answer section
		 */
			if (buflen < 1 + RRFIXEDSZ + datalen)
				return (-1);
			*cp++ = '\0'; /* no domain name */
			ircd__putshort(type, cp);
			cp += INT16SZ;
			ircd__putshort(class, cp);
			cp += INT16SZ;
			ircd__putlong(0, cp);
			cp += INT32SZ;
			ircd__putshort(datalen, cp);
			cp += INT16SZ;
			if (datalen)
			{
				bcopy(data, cp, datalen);
				cp += datalen;
			}
			hp->ancount = htons(1);
			break;

		default:
			return (-1);
	}
	return (cp - buf);
}
