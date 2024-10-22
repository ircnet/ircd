/************************************************************************
 *   IRC - Internet Relay Chat, common/support_def.h
 *   Copyright (C) 1991, 1993, 1995 Free Software Foundation, Inc.
 *   Contributed by Torbjorn Granlund (tege@sics.se).
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

#ifdef __ptr_t
#undef __ptr_t
#endif
#if defined(__STDC__) && __STDC__
#define __ptr_t void *
#else
#define __ptr_t char *
#endif
