/************************************************************************
 *   IRC - Internet Relay Chat, common/dbuf.c
 *   Copyright (C) 1990 Markku Savela
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
static const volatile char rcsid[] = "@(#)$Id: dbuf.c,v 1.12 2004/10/01 20:22:12 chopin Exp $";
#endif

/*
** For documentation of the *global* functions implemented here,
** see the header file (dbuf.h).
**
*/

#include "os.h"
#include "s_defines.h"
#define DBUF_C
#include "s_externs.h"
#undef DBUF_C

u_int poolsize	  = (BUFFERPOOL > 1500000) ? BUFFERPOOL : 1500000;
dbufbuf *freelist = NULL;

/* dbuf_init--initialize a stretch of memory as dbufs.
   Doing this early on should save virtual memory if not real memory..
   at the very least, we get more control over what the server is doing

   mika@cs.caltech.edu 6/24/95
*/

void dbuf_init(void)
{
	dbufbuf *dbp;
	int i = 0, nb;

	nb		 = poolsize / sizeof(dbufbuf);
	freelist = (dbufbuf *) malloc(nb * sizeof(dbufbuf));
	if (!freelist)
		return; /* screw this if it doesn't work */
	dbp = freelist;
	for (; i < (nb - 1); i++, dbp++, istat.is_dbufnow++)
		dbp->next = (dbp + 1);
	dbp->next = NULL;
	istat.is_dbufnow++;
	istat.is_dbuf = istat.is_dbufnow;
}

/*
** dbuf_alloc - allocates a dbufbuf structure either from freelist or
** creates a new one.
** Return: 0 on success, -1 on fatal alloc error, -2 on pool exceeding
*/
static int dbuf_alloc(dbufbuf **dbptr)
{
	if (istat.is_dbufuse++ == istat.is_dbufmax)
		istat.is_dbufmax = istat.is_dbufuse;
	if ((*dbptr = freelist))
	{
		freelist = freelist->next;
		return 0;
	}
	if (istat.is_dbufuse * DBUFSIZ > poolsize)
	{
		istat.is_dbufuse--;
		return -2; /* Not fatal, go back and increase poolsize */
	}

	istat.is_dbufnow++;
	if (!(*dbptr = (dbufbuf *) MyMalloc(sizeof(dbufbuf))))
		return -1;
	return 0;
}
/*
** dbuf_free - return a dbufbuf structure to the freelist
*/
static void dbuf_free(dbufbuf *ptr)
{
	istat.is_dbufuse--;
	ptr->next = freelist;
	freelist  = ptr;
}
/*
** This is called when malloc fails. Scrap the whole content
** of dynamic buffer and return -1. (malloc errors are FATAL,
** there is no reason to continue this buffer...). After this
** the "dbuf" has consistent EMPTY status... ;)
*/
int dbuf_malloc_error(dbuf *dyn)
{
	dbufbuf *p;

	dyn->length = 0;
	dyn->offset = 0;
	while ((p = dyn->head) != NULL)
	{
		dyn->head = p->next;
		dbuf_free(p);
	}
#ifdef DBUF_TAIL
	dyn->tail = dyn->head;
#endif
	return -1;
}


/*
** dbuf_put
**	Append the number of bytes to the buffer, allocating more
**	memory as needed. Bytes are copied into internal buffers
**	from users buffer.
**
**	returns	> 0, if operation successfull
**		< 0, if failed (due memory allocation problem)
**
** Parameters:
**	dbuf	*dyn		Dynamic buffer header
**	char	*buf		Pointer to data to be stored
**	int	length		Number of bytes to store
**
*/
int dbuf_put(dbuf *dyn, char *buf, int length)
{
	Reg dbufbuf **h;
	dbufbuf *d;
#ifdef DBUF_TAIL
	dbufbuf *dtail;
	Reg int off;
#else
	Reg int nbr, off;
#endif
	Reg int chunk, i, dlength;

	dlength = dyn->length;

	off = (dyn->offset + dyn->length) % DBUFSIZ;
#ifdef DBUF_TAIL
	dtail = dyn->tail;
	if (!dyn->length)
		h = &(dyn->head);
	else
	{
		if (off)
			h = &(dyn->tail);
		else
			h = &(dyn->tail->next);
	}
#else
	/*
	** Locate the last non-empty buffer. If the last buffer is
	** full, the loop will terminate with 'd==NULL'. This loop
	** assumes that the 'dyn->length' field is correctly
	** maintained, as it should--no other check really needed.
	*/
	nbr = (dyn->offset + dyn->length) / DBUFSIZ;
	for (h = &(dyn->head); (d = *h) && --nbr >= 0; h = &(d->next));
#endif
	/*
	** Append users data to buffer, allocating buffers as needed
	*/
	chunk = DBUFSIZ - off;
	dyn->length += length;
	for (; length > 0; h = &(d->next))
	{
		if ((d = *h) == NULL)
		{
			if ((i = dbuf_alloc(&d)))
			{
				if (i == -1) /* out of memory, cleanup */
					/* modifies dyn->tail */
					dbuf_malloc_error(dyn);
				else
				/* If we run out of bufferpool, visit upper
				 * level to increase it and retry. -Vesa
				 */
				{
					/*
					** Cancel this dbuf_put as well,
					** since it is incomplete. -krys
					*/
					dyn->length = dlength;
#ifdef DBUF_TAIL
					dyn->tail = dtail;
#endif
				}
				return i;
			}
#ifdef DBUF_TAIL
			dyn->tail = d;
#endif
			*h		= d;
			d->next = NULL;
		}
		if (chunk > length)
			chunk = length;
		bcopy(buf, d->data + off, chunk);
		length -= chunk;
		buf += chunk;
		off	  = 0;
		chunk = DBUFSIZ;
	}
	return 1;
}


/*
** dbuf_map, dbuf_delete
**	These functions are meant to be used in pairs and offer
**	a more efficient way of emptying the buffer than the
**	normal 'dbuf_get' would allow--less copying needed.
**
**	map	returns a pointer to a largest contiguous section
**		of bytes in front of the buffer, the length of the
**		section is placed into the indicated "long int"
**		variable. Returns NULL *and* zero length, if the
**		buffer is empty.
**
**	delete	removes the specified number of bytes from the
**		front of the buffer releasing any memory used for them.
**
**	Example use (ignoring empty condition here ;)
**
**		buf = dbuf_map(&dyn, &count);
**		<process N bytes (N <= count) of data pointed by 'buf'>
**		dbuf_delete(&dyn, N);
**
**	Note: 	delete can be used alone, there is no real binding
**		between map and delete functions...
**
** Parameters:
**
**	dbuf	*dyn		Dynamic buffer header
**	int	*length		Return number of bytes accessible
**
*/
char *dbuf_map(dbuf *dyn, int *length)
{
	if (dyn->head == NULL)
	{
#ifdef DBUF_TAIL
		dyn->tail = NULL;
#endif
		*length = 0;
		return NULL;
	}
	*length = DBUFSIZ - dyn->offset;
	if (*length > dyn->length)
		*length = dyn->length;
	return (dyn->head->data + dyn->offset);
}

/*
** Parameters:
**
**	dbuf	*dyn		Dynamic buffer header
**	int	length		Number of bytes to delete
*/
int dbuf_delete(dbuf *dyn, int length)
{
	dbufbuf *d;
	int chunk;

	if (length > dyn->length)
		length = dyn->length;
	chunk = DBUFSIZ - dyn->offset;
	while (length > 0)
	{
		if (chunk > length)
			chunk = length;
		length -= chunk;
		dyn->offset += chunk;
		dyn->length -= chunk;
		if (dyn->offset == DBUFSIZ || dyn->length == 0)
		{
			if ((d = dyn->head))
			{ /* What did I do? A memory leak.. ? */
				dyn->head = d->next;
				dbuf_free(d);
			}
			dyn->offset = 0;
		}
		chunk = DBUFSIZ;
	}
	if (dyn->head == (dbufbuf *) NULL)
#ifdef DBUF_TAIL
	{
		dyn->tail = NULL;
#endif
		dyn->length = 0;
#ifdef DBUF_TAIL
	}
#endif
	return 0;
}

/*
** dbuf_get
**	Remove number of bytes from the buffer, releasing dynamic
**	memory, if applicaple. Bytes are copied from internal buffers
**	to users buffer.
**
**	returns	the number of bytes actually copied to users buffer,
**		if >= 0, any value less than the size of the users
**		buffer indicates the dbuf became empty by this operation.
**
**		Return 0 indicates that buffer was already empty.
**
**		Negative return values indicate some unspecified
**		error condition, rather fatal...
**
**  Parameters:
**
**	dbuf	*dyn		Dynamic buffer header
**	char	*buf		Pointer to buffer to receive the data
**	int	length		Max amount of bytes that can be received
*/
int dbuf_get(dbuf *dyn, char *buf, int length)
{
	int moved = 0;
	int chunk;
	char *b;

	while (length > 0 && (b = dbuf_map(dyn, &chunk)) != NULL)
	{
		if (chunk > length)
			chunk = length;
		bcopy(b, buf, (int) chunk);
		(void) dbuf_delete(dyn, chunk);
		buf += chunk;
		length -= chunk;
		moved += chunk;
	}
	return moved;
}

/*
int	dbuf_copy(dbuf *dyn, char *buf, int length)
{
	register dbufbuf	*d = dyn->head;
	register char	*s;
	register int	chunk, len = length, dlen = dyn->length;

	s = d->data + dyn->offset;
	chunk = MIN(DBUFSIZ - dyn->offset, dlen);

	while (len > 0)
	    {
		if (chunk > dlen)
			chunk = dlen;
		if (chunk > len)
			chunk = len;

		bcopy(s, buf, chunk);
		buf += chunk;
		len -= chunk;
		dlen -= chunk;

		if (dlen > 0 && (d = d->next))
		    {
			chunk = DBUFSIZ;
			s = d->data;
		    }
		else
			break;
	    }
	return length - len;
}
*/

/*
** dbuf_getmsg
**
** Check the buffers to see if there is a string which is terminted with
** either a \r or \n prsent.  If so, copy as much as possible (determined by
** length) into buf and return the amount copied - else return 0.
*/
int dbuf_getmsg(dbuf *dyn, char *buf, int length)
{
	dbufbuf *d;
	register char *s;
	register int dlen;
	register int i;
	int copy;

getmsg_init:
	d	 = dyn->head;
	dlen = dyn->length;
	i	 = DBUFSIZ - dyn->offset;
	if (i <= 0)
		return -1;
	copy = 0;
	if (d && dlen)
		s = dyn->offset + d->data;
	else
		return 0;

	if (i > dlen)
		i = dlen;
	while (length > 0 && dlen > 0)
	{
		dlen--;
		if (*s == '\n' || *s == '\r')
		{
			copy = dyn->length - dlen;
			/*
			** Shortcut this case here to save time elsewhere.
			** -avalon
			*/
			if (copy == 1)
			{
				(void) dbuf_delete(dyn, 1);
				goto getmsg_init;
			}
			break;
		}
		length--;
		if (!--i)
		{
			if ((d = d->next))
			{
				s = d->data;
				i = MIN(DBUFSIZ, dlen);
			}
		}
		else
			s++;
	}

	if (copy <= 0)
		return 0;

	/*
	** copy as much of the message as wanted into parse buffer
	*/
	i = dbuf_get(dyn, buf, MIN(copy, length));
	/*
	** and delete the rest of it!
	*/
	if (copy - i > 0)
		(void) dbuf_delete(dyn, copy - i);
	if (i >= 0)
		*(buf + i) = '\0'; /* mark end of messsage */

	return i;
}
