/*
 *   IRC - Internet Relay Chat, ircd/s_pp2_ext.h
 *
 *   Copyright (C) 2025 Patrick Okraku
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
#define EXTERN
/*
 * Initializes the list of trusted proxy IP addresses.
 */
void init_trusted_proxy_ips();

/*
 * Verifies that the given address is listed as a trusted proxy.
 *
 * Only connections from trusted proxy addresses are allowed to send
 * PROXY protocol v2 headers that override the client IP/port. This
 * check prevents untrusted clients from spoofing their source address.
 */
int is_trusted_proxy(const struct in6_addr *addr);

/*
 * Initializes PROXY protocol v2 parser for a newly accepted client.
 */
EXTERN void pp2_init(aClient *cptr);

/*
 * Processes incoming bytes of PROXY protocol v2.
 *
 * Returns:
 *   -1  → fatal error, drop connection
 *    0  → parsing still incomplete, need more data
 *    1  → parsing complete, client address applied, continue as normal
 */
EXTERN int pp2_consume(aClient *cptr, const unsigned char *data, size_t len, size_t *consumed_out);

/*
 * Frees the PROXY protocol v2 state of a client.
 */
void pp2_free(aClient *cptr);
#undef EXTERN