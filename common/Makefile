#************************************************************************
#*   IRC - Internet Relay Chat, common/Makefile
#*   Copyright (C) 1990 Jarkko Oikarinen
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 1, or (at your option)
#*   any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*
#*   $Id: Makefile,v 1.2 1997/04/14 15:04:04 kalt Exp $
#*
#*/

CC=gcc
RM=/bin/rm
INCLUDE=../include

# For MIPS, use:
# CFLAGS=-O -systype bsd43 -I${INCLUDE}
# and otherwise:
#CFLAGS=-O2 -I${INCLUDE}
CFLAGS=-g -I${INCLUDE}


OBJS=bsd.o dbuf.o packet.o send.o match.o parse.o support.o inet_addr.o

SRC=bsd.c dbuf.c packet.c parse.c send.c match.c support.c inet_addr.c

tm: tm.o manew.o tab.o
	$(CC) -g tab.o tm.o manew.o -o tm
tm1: tm.o ma2.o tab.o
	$(CC) -g tab.o tm.o ma2.o -o tm1
tmo: tm.o match.o tab.o
	$(CC) -g tm.o match.o -o tmo

all: build

build: $(OBJS)

install:

clean:
	${RM} -f *.o *~ core #* *.bak

depend:
	makedepend -I${INCLUDE} ${SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.

bsd.o: bsd.c ../include/common.h ../include/sys.h ../include/config.h
dbuf.o: ../include/config.h ../include/common.h ../include/dbuf.h
dbuf.o: ../include/sys.h dbuf.c
packet.o: ../include/struct.h ../include/config.h ../include/class.h
packet.o: ../include/dbuf.h ../include/common.h ../include/msg.h packet.c
parse.o: ../include/struct.h ../include/config.h
parse.o: ../include/class.h ../include/dbuf.h ../include/common.h
parse.o: ../include/msg.h ../include/sys.h
parse.o: ../include/numeric.h parse.c
send.o: ../include/struct.h ../include/config.h ../include/class.h
send.o: ../include/dbuf.h ../include/common.h send.c
match.o: ../include/config.h ../include/common.h ../include/sys.h match.c
manew.o: ../include/config.h ../include/common.h ../include/sys.h manew.c
support.o: ../include/config.h ../include/common.h ../include/sys.h support.c
inet_addr.o: ../include/sys.h inet_addr.c ../include/common.h
