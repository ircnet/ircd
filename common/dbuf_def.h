/************************************************************************
 *   IRC - Internet Relay Chat, common/dbuf_def.h
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

#define	DBUF_TAIL

/*
** dbuf is a collection of functions which can be used to
** maintain a dynamic buffering of a byte stream.
** Functions allocate and release memory dynamically as
** required [Actually, there is nothing that prevents
** this package maintaining the buffer on disk, either]
*/

/*
** These structure definitions are only here to be used
** as a whole, *DO NOT EVER REFER TO THESE FIELDS INSIDE
** THE STRUCTURES*! It must be possible to change the internal
** implementation of this package without changing the
** interface.
*/
#if !defined(_SEQUENT_)
typedef struct dbuf
    {
	u_int	length;	/* Current number of bytes stored */
	u_int	offset;	/* Offset to the first byte */
	struct	dbufbuf *head;	/* First data buffer, if length > 0 */
#ifdef DBUF_TAIL
	/* added by mnystrom@mit.edu: */
	struct  dbufbuf *tail; /* last data buffer, if length > 0 */
#endif
    } dbuf;
#else
typedef struct dbuf
    {
        uint   length; /* Current number of bytes stored */
        uint   offset; /* Offset to the first byte */
        struct  dbufbuf *head;  /* First data buffer, if length > 0 */
#ifdef DBUF_TAIL
	/* added by mnystrom@mit.edu: */
	struct  dbufbuf *tail; /* last data buffer, if length > 0 */
#endif
    } dbuf;
#endif
/*
** And this 'dbufbuf' should never be referenced outside the
** implementation of 'dbuf'--would be "hidden" if C had such
** keyword...
** If it was possible, this would compile to be exactly 1 memory
** page in size. 2048 bytes seems to be the most common size, so
** as long as a pointer is 4 bytes, we get 2032 bytes for buffer
** data after we take away a bit for malloc to play with. -avalon
*/
typedef struct dbufbuf
    {
	struct	dbufbuf	*next;	/* Next data buffer, NULL if this is last */
	char	data[2032];	/* Actual data stored here */
    } dbufbuf;

/*
** DBufLength
**	Return the current number of bytes stored into the buffer.
**	(One should use this instead of referencing the internal
**	length field explicitly...)
*/
#define DBufLength(dyn) ((dyn)->length)

/*
** DBufClear
**	Scratch the current content of the buffer. Release all
**	allocated buffers and make it empty.
*/
#define DBufClear(dyn)	dbuf_delete((dyn),DBufLength(dyn))

/* This is a dangerous define because a broken compiler will set DBUFSIZ
** to 4, which will work but will be very inefficient. However, there
** are other places where the code breaks badly if this is screwed
** up, so... -- Wumpus
*/

#define DBUFSIZ sizeof(((dbufbuf *)0)->data)
