# Makefile for the mkpasswd

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
