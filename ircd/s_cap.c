/*
 *   IRC - Internet Relay Chat, ircd/s_cap.c
 *
 *   Copyright (C) 2021 IRCnet.com team
 *   Thanks to Lee Hardy and the IRCv3 Working Group!
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

struct Cap
{
	const char *name;
	const int flag;
} cap_tab[] = {
		{"extended-join", CAP_EXTENDED_JOIN},
		{"multi-prefix",  CAP_MULTI_PREFIX},
		{"sasl",          CAP_SASL},
		{NULL,            0}
};

void send_cap_list(aClient *target, char *sub_cmd, int flags)
{
	char buf[BUFSIZE];
	int prfx_len, cnt = 0;
	struct Cap *cap;

	prfx_len = snprintf(buf, BUFSIZE, ":%s CAP %s %s :", ME, BadTo(target->name), sub_cmd);

	for (cap = cap_tab; cap->name; cap++)
	{
		if (flags != -1 && !(cap->flag & flags))
		{
			continue;
		}

		if (strlen(buf) + strlen(cap->name) + 1 >= BUFSIZE - 3)
		{
			sendto_one(target, buf);
			prfx_len = snprintf(buf, sizeof(buf), ":%s CAP %s %s :", ME, BadTo(target->name), sub_cmd);
		}

		strcat(buf, cap->name);
		strcat(buf, " ");
		cnt++;
	}

	if (strlen(buf) > prfx_len || cnt == 0)
	{
		sendto_one(target, buf);
	}
}

/*
 * Finds a supported cap by its name.
 */
struct Cap *find_cap(char *name)
{
	struct Cap *cap;

	if (!name)
	{
		return NULL;
	}

	for (cap = cap_tab; cap->name; cap++)
	{
		if (!strcasecmp((char *) cap->name, name))
		{
			return cap;
		}
	}

	return NULL;
}

/*
 * Lists the capabilities supported by this server.
 * The registration will be suspended until "CAP END" is received from the client.
 */
void cap_ls(aClient *target, char *arg)
{
	if (!IsRegistered(target))
	{
		target->cap_negotation = 1;
	}

	send_cap_list(target, "LS", -1);
}

/*
 * A client requests a list of the capabilities currently active for his connection.
 */
void cap_list(aClient *target, char *arg)
{
	struct Cap *cap;
	send_cap_list(target, "LIST", target->caps);
}

/*
 * A client is changing his capabilities.
 */
void cap_req(aClient *target, char *arg)
{
	char buf[2][BUFSIZE], *p = NULL, *s, *arg_copy;
	int buf_idx = 0, prfx_len, current_caps;
	struct Cap *cap;

	if (!arg || !*arg)
	{
		return;
	}

	if (!IsRegistered(target))
	{
		// The registration will be suspended until "CAP END" is received from the client.
		target->cap_negotation = 1;
	}

	arg_copy = mystrdup(arg);
	prfx_len = snprintf(buf[0], BUFSIZE, ":%s CAP %s ACK :", ME, BadTo(target->name));
	memset(buf[1], 0, BUFSIZE);
	current_caps = target->caps;

	for (s = strtoken(&p, arg_copy, " "); s; s = strtoken(&p, NULL, " "))
	{
		if ((cap = find_cap(*s == '-' ? s + 1 : s)))
		{
			if (strlen(buf[buf_idx]) + strlen(cap->name) + 1 >= BUFSIZE - 3)
			{
				prfx_len = snprintf(buf[1], BUFSIZE, ":%s CAP %s ACK :", ME, BadTo(target->name));
				buf_idx = 1;
			}

			if (*s != '-')
			{
				current_caps |= cap->flag;
			}
			else
			{
				current_caps &= ~cap->flag;
			}

			strcat(buf[buf_idx], s);
			strcat(buf[buf_idx], " ");
		}
		else
		{
			// The server MUST NOT make any change to any capabilities if it replies with a NAK subcommand.
			sendto_one(target, ":%s CAP %s NAK :%s", ME, BadTo(target->name), arg);
			MyFree(arg_copy);
			return;
		}
	}

	target->caps = current_caps;

	// Send ACK
	if (strlen(buf[0]) > prfx_len)
	{
		sendto_one(target, buf[0]);
	}
	if (strlen(buf[1]) > prfx_len)
	{
		sendto_one(target, buf[1]);
	}

	MyFree(arg_copy);
}

/*
 * The capability negotiation is complete.
 */
int cap_end(aClient *cptr, aClient *sptr, char *arg)
{
	if (IsRegistered(cptr))
	{
		return 0;
	}

	if ((sptr->sasl_service != NULL || sptr->sasl_auth_attempts > 0) && !IsSASLAuthed(sptr))
	{
		// SASL authentication exchange has been aborted
		return process_implicit_sasl_abort(sptr);
	}

	cptr->cap_negotation = 0;

	// complete registration if we received NICK and USER already
	if (sptr->name[0] && sptr->user)
	{
		return register_user(cptr, sptr, sptr->name, sptr->user->username);
	}

	return 0;
}

int m_cap(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int ret = 0;

	if (!strcasecmp(parv[1], "LS"))
	{
		cap_ls(sptr, parv[2]);
	}
	else if (!strcasecmp(parv[1], "LIST"))
	{
		cap_list(sptr, parv[2]);
	}
	else if (!strcasecmp(parv[1], "REQ"))
	{
		cap_req(sptr, parv[2]);
	}
	else if (!strcasecmp(parv[1], "END"))
	{
		ret = cap_end(cptr, sptr, NULL);
	}

	return ret;
}