# Makefile for the mkpasswd
# CVS $Id: Makefile,v 1.2 2001/10/20 17:57:23 q Exp $

CC=gcc
CFLAGS	= -O2 -Wall
LDFLAGS = -lcrypt

OBJS =	mkpasswd.o
SRCS =	mkpasswd.c

.c.o:
	$(CC) $(CFLAGS) -c $<

all: mkpasswd

clean:
	/bin/rm -rf *.o core *.core

mkpasswd: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# Catch any changes in compilation options at the top of this file
$(OBJS): Makefile
