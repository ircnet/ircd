/************************************************************************
 *   IRC - Internet Relay Chat, iauth/a_defines.h
 *   Copyright (C) 1998 Christophe Kalt
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

/*  This file includes all files defining constants, macros and types
    definitions used by the authentication process.
 */

#define IAUTH_DEBUG	1

#include "config.h"
#include "patchlevel.h"

#include "dbuf_def.h"	/* needed for struct_def.h, sigh */
#include "class_def.h"	/* needed for struct_def.h, sigh */
#include "struct_def.h"
#if INET6
# include "../ircd/nameser_def.h"
#endif
#include "support_def.h"

#include "a_conf_def.h"
#include "a_struct_def.h"
#include "a_log_def.h"
