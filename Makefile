#************************************************************************
#*   IRC - Internet Relay Chat, Makefile
#*   Copyright (C) 1990, Jarkko Oikarinen
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
#*/

SHELL=/bin/sh

rev=`support/rev.sh`

all install config configure:
	@if [ -d ${rev} -a -f ${rev}/Makefile ]; then \
		echo "Configuration for ${rev} already exists"; \
		echo "Please \"cd ${rev}\" first"; \
	else \
		echo "Configuring ${rev}"; \
		mkdir -p ${rev}; \
		cd ${rev}; \
		${SHELL} ../support/configure ${CONFIGARGS}; \
		if [ ! -f config.h ]; then \
			/bin/cp ../include/config.h.dist config.h; \
		fi; \
		/bin/cp ../support/Makefile.irc ../support/Makefile.ircd .; \
		cd ..; \
		echo "Next cd ${rev}, edit config.h and run make to build"; \
	fi

clean:
	@echo 'To make clean move to the arch/OS specific directory'

distclean realclean: clean
	@echo "To make $@ remove all the arch/OS specific directories"

rcs:
	cii -H -R Makefile common doc include irc ircd support

