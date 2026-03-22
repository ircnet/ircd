/*
*   IRC - Internet Relay Chat, ircd/s_pp2.c
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

#include "os.h"
#include "s_defines.h"
#include "s_externs.h"

static const char *trusted_proxy_ip_strings[] = { TRUSTED_PROXY_ADDRESSES };
#define TRUSTED_PROXY_IP_COUNT elementsof(trusted_proxy_ip_strings)
static struct in6_addr trusted_proxy_ips[TRUSTED_PROXY_IP_COUNT];

void init_trusted_proxy_ips()
{
	size_t i;
	for (i = 0; i < TRUSTED_PROXY_IP_COUNT; i++)
	{
		const char *ip_str = trusted_proxy_ip_strings[i];
		struct in6_addr addr6;

		// Try to parse the string as a native IPv6 address
		if (inet_pton(AF_INET6, ip_str, &addr6) == 1)
		{
			trusted_proxy_ips[i] = addr6;
		}
		else
		{
			struct in_addr addr4;

			// Try to parse the string as an IPv4 address
			if (inet_pton(AF_INET, ip_str, &addr4) == 1)
			{
				// Build an IPv4-mapped IPv6 address (::ffff:w.x.y.z)
				memset(&addr6, 0, sizeof(addr6));
				addr6.s6_addr[10] = 0xff;
				addr6.s6_addr[11] = 0xff;
				memcpy(&addr6.s6_addr[12], &addr4, 4);
				trusted_proxy_ips[i] = addr6;
			}
			else
			{
				// Neither IPv6 nor IPv4
				fprintf(stderr, "Invalid IP in TRUSTED_PROXY_ADDRESSES: %s\n", ip_str);
			}
		}
	}
}

int is_trusted_proxy(const struct in6_addr *addr)
{
	size_t i;
	for (i = 0; i < TRUSTED_PROXY_IP_COUNT; i++)
	{
		if (memcmp(addr, &trusted_proxy_ips[i], sizeof(struct in6_addr)) == 0)
		{
			return 1;
		}
	}
	return 0;
}

void pp2_init(aClient *cptr)
{
	cptr->pp2_state = (PP2State *) MyMalloc(sizeof(*cptr->pp2_state));
	memset(cptr->pp2_state, 0, sizeof(*cptr->pp2_state));
	cptr->pp2_state->phase = PROXY_NEED_HDR;
	cptr->pp2_state->buflen = 0;
	cptr->pp2_state->need = 16;
	Debug((DEBUG_INFO, "pp2(%d): init", cptr->fd));
}

int pp2_consume(aClient *cptr, const unsigned char *data, size_t len, size_t *consumed_out)
{
	static const unsigned char pp2_sig[12] = { 0x0D, 0x0A, 0x0D, 0x0A, 0x00, 0x0D,
											   0x0A, 0x51, 0x55, 0x49, 0x54, 0x0A };
	char sig[3 * 12 + 1], src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
	size_t off = 0;
	int i;
	*consumed_out = 0;

	while (off < len && cptr->pp2_state->phase != PROXY_DONE)
	{
		size_t take = cptr->pp2_state->need;
		if (take > (len - off))
		{
			take = len - off;
		}
		if (take)
		{
			if (cptr->pp2_state->buflen + take > sizeof(cptr->pp2_state->buf))
			{
				Debug((DEBUG_ERROR, "pp2(%d): buffer overflow, buflen=%lu, take=%lu, bufsize=%lu",
					   cptr->fd, (unsigned long) cptr->pp2_state->buflen, (unsigned long) take,
					   (unsigned long) sizeof(cptr->pp2_state->buf)));
				*consumed_out = off;
				return -1;
			}

			memcpy(cptr->pp2_state->buf + cptr->pp2_state->buflen, data + off, take);
			cptr->pp2_state->buflen += take;
			cptr->pp2_state->need -= take;
			off += take;
		}

		if (cptr->pp2_state->phase == PROXY_NEED_HDR && cptr->pp2_state->need == 0)
		{
			unsigned char ver_cmd, fam;
			unsigned short paylen;

			if (cptr->pp2_state->buflen < 16)
			{
				Debug((DEBUG_ERROR, "pp2(%d): header too short: %lu", cptr->fd,
					   (unsigned long) cptr->pp2_state->buflen));
				*consumed_out = off;
				return -1;
			}
			if (memcmp(cptr->pp2_state->buf, pp2_sig, 12) != 0)
			{
				for (i = 0; i < 12; i++)
				{
					sprintf(sig + i * 3, "%02x ", cptr->pp2_state->buf[i]);
				}
				sig[3 * 12] = '\0';
				Debug((DEBUG_ERROR, "pp2(%d): bad signature: %s", cptr->fd, sig));
				*consumed_out = off;
				return -1;
			}

			ver_cmd = cptr->pp2_state->buf[12];
			fam = cptr->pp2_state->buf[13];
			paylen = (unsigned short) ((((unsigned int) cptr->pp2_state->buf[14]) << 8) |
									   ((unsigned int) cptr->pp2_state->buf[15]));
			Debug((DEBUG_INFO, "pp2(%d): header ver=%u cmd=%u fam=0x%02x len=%u", cptr->fd,
				   (ver_cmd >> 4) & 0x0F, ver_cmd & 0x0F, fam, (unsigned) paylen));

			if ((ver_cmd & 0xF0) != 0x20)
			{
				Debug((DEBUG_ERROR, "pp2(%d): unsupported version, ver_cmd=0x%02x", cptr->fd,
					   ver_cmd));
				*consumed_out = off;
				return -1;
			}
			if ((ver_cmd & 0x0F) != 0x01)
			{
				Debug((DEBUG_ERROR,
					   "pp2(%d): unsupported command, ver_cmd=0x%02x (only PROXY=0x01 allowed)",
					   cptr->fd, ver_cmd));
				*consumed_out = off;
				return -1;
			}
			/* transport protocol: 0x01=STREAM(TCP) */
			if ((fam & 0x0F) != 0x01)
			{
				Debug((DEBUG_ERROR, "pp2(%d): unsupported transport protocol 0x%02x", cptr->fd,
					   (unsigned) (fam & 0x0F)));
				*consumed_out = off;
				return -1;
			}
			if ((size_t) paylen > sizeof(cptr->pp2_state->buf))
			{
				Debug((DEBUG_ERROR, "pp2(%d): payload length too large: %u > %lu", cptr->fd,
					   (unsigned) paylen, (unsigned long) sizeof(cptr->pp2_state->buf)));
				*consumed_out = off;
				return -1;
			}

			cptr->pp2_state->fam = fam;
			cptr->pp2_state->fam_len = paylen;
			cptr->pp2_state->phase = PROXY_NEED_PAYLOAD;
			cptr->pp2_state->need = paylen;
			/* reset buffer before reading payload */
			cptr->pp2_state->buflen = 0;
		}
		else if (cptr->pp2_state->phase == PROXY_NEED_PAYLOAD && cptr->pp2_state->need == 0)
		{
			const unsigned char *p = cptr->pp2_state->buf;

			if ((cptr->pp2_state->fam & 0xF0) == 0x10)
			{
				/* AF_INET */
				unsigned sport, dport;
				struct in_addr s, d;
				struct in6_addr v4m_src, v4m_dst;

				if (cptr->pp2_state->fam_len < 12)
				{
					Debug((DEBUG_ERROR,
						   "pp2(%d): payload too short for AF_INET, need at least 12 bytes, got "
						   "%lu",
						   cptr->fd, (unsigned long) cptr->pp2_state->fam_len));
					*consumed_out = off;
					return -1;
				}

				memcpy(&s.s_addr, p, 4);
				memcpy(&d.s_addr, p + 4, 4);

				sport = ((unsigned) p[8] << 8) | p[9];
				dport = ((unsigned) p[10] << 8) | p[11];

				/* IPv4 -> v4-mapped IPv6 (source + dest) */
				memset(&v4m_src, 0, sizeof(v4m_src));
				v4m_src.s6_addr[10] = 0xff;
				v4m_src.s6_addr[11] = 0xff;
				memcpy(&v4m_src.s6_addr[12], &s.s_addr, 4);

				memset(&v4m_dst, 0, sizeof(v4m_dst));
				v4m_dst.s6_addr[10] = 0xff;
				v4m_dst.s6_addr[11] = 0xff;
				memcpy(&v4m_dst.s6_addr[12], &d.s_addr, 4);

				inetntop(AF_INET6, &v4m_src, src, sizeof(src));
				inetntop(AF_INET6, &v4m_dst, dst, sizeof(dst));
				Debug((DEBUG_INFO, "pp2(%d): IPv4 src=%s:%u dst=%s:%u", cptr->fd, src, sport, dst,
					   dport));

				/* Source (client) */
				memcpy(&cptr->ip, &v4m_src, sizeof(v4m_src));
				cptr->port = sport;
				get_sockhost(cptr, src);

				/* Destination (server) */
				memcpy(&cptr->pp2_dip, &v4m_dst, sizeof(v4m_dst));
				cptr->pp2_dport = dport;
			}
			else if ((cptr->pp2_state->fam & 0xF0) == 0x20)
			{
				struct in6_addr s6, d6;
				unsigned sport, dport;

				if (cptr->pp2_state->fam_len < 36)
				{
					Debug((DEBUG_ERROR,
						   "pp2(%d): payload too short for AF_INET6, need at least 36 bytes, got "
						   "%lu",
						   cptr->fd, (unsigned long) cptr->pp2_state->fam_len));
					*consumed_out = off;
					return -1;
				}

				memcpy(&s6, p, 16);
				memcpy(&d6, p + 16, 16);

				sport = ((unsigned) p[32] << 8) | p[33];
				dport = ((unsigned) p[34] << 8) | p[35];

				inetntop(AF_INET6, &s6, src, sizeof(src));
				inetntop(AF_INET6, &d6, dst, sizeof(dst));
				Debug((DEBUG_INFO, "pp2(%d): IPv6 src=[%s]:%u dst=[%s]:%u", cptr->fd, src, sport,
					   dst, dport));

				/* Source (client) */
				memcpy(&cptr->ip, &s6, sizeof(s6));
				cptr->port = sport;
				get_sockhost(cptr, src);

				/* Destination (server) */
				memcpy(&cptr->pp2_dip, &d6, sizeof(d6));
				cptr->pp2_dport = dport;
			}
			else
			{
				Debug((DEBUG_ERROR, "pp2(%d): unknown address family 0x%02x", cptr->fd,
					   cptr->pp2_state->fam));
				*consumed_out = off;
				return -1;
			}

			cptr->pp2_state->phase = PROXY_DONE;
			cptr->pp2_state->buflen = 0;
			cptr->pp2_state->need = 0;

			Debug((DEBUG_INFO, "pp2(%d): complete", cptr->fd));

			if (finalize_connection(cptr, src) < 0)
			{
				*consumed_out = off;
				return -1;
			}
		}
	}
	*consumed_out = off;
	Debug((DEBUG_READ, "pp2(%d): consumed=%lu state=%d len=%lu", cptr->fd, (unsigned long) off,
		   (int) cptr->pp2_state->phase, (unsigned long) len));

	return (cptr->pp2_state->phase == PROXY_DONE) ? 1 : 0;
}

void pp2_free(aClient *cptr)
{
	if (cptr->pp2_state)
	{
		MyFree(cptr->pp2_state);
		cptr->pp2_state = NULL;
	}
}