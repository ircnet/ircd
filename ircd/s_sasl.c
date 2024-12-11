/*
 *   IRC - Internet Relay Chat, ircd/s_sasl.c
 *
 *   Copyright (C) 2006 Michael Tharp <gxti@partiallystapled.com>
 *   Copyright (C) 2006 charybdis development team
 *   Copyright (C) 2021 IRCnet.com team
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

static void sendto_service(aClient *service, char *fmt, ...);
static void m_sasl_service(aClient *cptr, aClient *sptr, int parc, char *parv[]);
static void m_sasl_server(aClient *cptr, aClient *sptr, int parc, char *parv[]);

int m_authenticate(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!HasCap(sptr, CAP_SASL))
	{
		return 0;
	}

	if (*parv[1] == ':' || strchr(parv[1], ' '))
	{
		sptr->exitc = EXITC_SASL_REQUIRED;
		exit_client(cptr, sptr, sptr, "Malformed AUTHENTICATE");
		return cptr == sptr ? FLUSH_BUFFER : 0;
	}

	if (IsSASLAuthed(sptr))
	{
		sendto_one(sptr, replies[ERR_SASLALREADY], me.name, BadTo(sptr->name));
		return 0;
	}

	if (strlen(parv[1]) > 400)
	{
		sendto_one(sptr, replies[ERR_SASLTOOLONG], me.name, BadTo(sptr->name));
		return 0;
	}

	if (!*sptr->uid)
	{
		// A unique identifier is required at this point. Allocate UID nick here and re-use it on registration.
		strcpy(sptr->uid, next_uid());
		add_to_uid_hash_table(sptr->uid, sptr);
	}

	if (!sptr->sasl_service)
	{
		// Assign a service
		sptr->sasl_service = best_service_with_flags(SERVICE_WANT_SASL);

		if (!sptr->sasl_service)
		{
			// No SASL service found.
			sptr->exitc = EXITC_SASL_REQUIRED;
			exit_client(cptr, sptr, sptr, "SASL service is temporary not available");
			return cptr == sptr ? FLUSH_BUFFER : 0;
		}

		sendto_service(sptr->sasl_service, "SASL %s %s H %s", sptr->uid, sptr->sasl_service->name,
					   get_client_name(sptr, FALSE));
		sendto_service(sptr->sasl_service, "SASL %s %s S %s", sptr->uid, sptr->sasl_service->name, parv[1]);
	}
	else
	{
		sendto_service(sptr->sasl_service, "SASL %s %s C %s", sptr->uid, sptr->sasl_service->name, parv[1]);
	}

	return 0;
}

int m_sasl(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (IsServer(sptr))
	{
		m_sasl_server(cptr, sptr, parc, parv);
	}
	else if (IsService(sptr))
	{
		m_sasl_service(cptr, sptr, parc, parv);
	}

	return 0;
}

/*
 * SASL message from service.
 *
 * parv[0] = Server name
 * parv[1] = UID nick of the authenticating user
 * parv[2] = Name of the SASL service
 * parv[3] = SASL message type
 * parv[4] = SASL message
 * parv[5] = SASL message (optional)
 */
void m_sasl_service(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient * acptr;

	if (parc < 5)
	{
		return;
	}

	if (!IsService(sptr))
	{
		return;
	}

	// Find user by UID nick
	acptr = find_uid(parv[1], NULL);

	if (acptr == NULL)
	{
		return;
	}

	// Check if the assigned service is responding
	if (acptr->sasl_service == NULL
		|| acptr->sasl_service->service == NULL
		|| acptr->sasl_service->service != sptr->service)
	{
		return;
	}

	if (*parv[3] == 'C')
	{
		sendto_one(acptr, "AUTHENTICATE %s", parv[4]);
	}
	else if (*parv[3] == 'L')
	{
		// Login
		if (parc == 6) {
			if (bad_hostname(parv[5], strlen(parv[5])))
			{
				char comment[BUFSIZE];
				sendto_flag(SCH_ERROR, "Received bad hostname %s from %s", parv[5], acptr->sasl_service->name);
				acptr->exitc = EXITC_SASL_REQUIRED;
				snprintf(comment, BUFSIZE, "Bad hostname (%s)", parv[5]);
				exit_client(acptr, acptr, &me, comment);
				return;
			}
			else
			{
				// Store spoofed hostname. It will finally be set by attach_Iline().
				acptr->spoof_tmp = mystrdup(parv[5]);
			}
		}
		acptr->sasl_user = mystrdup(parv[4]);
		sendto_one(acptr, replies[RPL_LOGGEDIN], me.name, BadTo(acptr->name), BadTo(acptr->name),
				   acptr->user ? acptr->user->username : "unknown",
				   acptr->spoof_tmp ? acptr->spoof_tmp : acptr->sockhost,
				   parv[4], parv[4]);
	}
	else if (*parv[3] == 'D')
	{
		// Authentication done
		if (*parv[4] == 'S')
		{
			// Authentication successful
			acptr->flags |= FLAGS_SASL;
			sendto_one(acptr, replies[RPL_SASLSUCCESS], me.name, BadTo(acptr->name));
			acptr->sasl_service = NULL;
		}
		else if (*parv[4] == 'F')
		{
			// Authentication failed
			sendto_one(acptr, replies[ERR_SASLFAIL], me.name, BadTo(acptr->name));
		}
		else if (*parv[4] == 'A')
		{
			// Authentication aborted
			sendto_one(acptr, replies[ERR_SASLABORTED], me.name, BadTo(acptr->name));
		}
	}
	else if (*parv[3] == 'M')
	{
		// Supported mechanisms
		sendto_one(acptr, replies[RPL_SASLMECHS], me.name, BadTo(acptr->name), parv[4]);
	}
	else if (*parv[3] == 'N')
	{
		// NOTICE from SASL service to user
		sendto_one(acptr, ":%s NOTICE %s :%s: %s", ME, BadTo(acptr->name), sptr->name, parv[4]);
	}
	else if (*parv[3] == 'K' && !IsRegisteredUser(acptr))
	{
		// KILL from SASL service to user
		acptr->exitc = EXITC_SASL_REQUIRED;
		exit_client(acptr, acptr, &me, parv[4]);
	}
}

/*
 * SASL message from server.
 *
 * parv[0] = Server name that sent the message
 * parv[1] = UID nick of the authenticating user
 * parv[2] = Name of the SASL service
 * parv[3] = SASL message type
 * parv[4] = SASL message
 */
void m_sasl_server(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient * acptr;

	if (parc < 5)
	{
		return;
	}

	if ((acptr = find_service(parv[2], NULL)) && MyConnect(acptr))
	{
		sendto_one(acptr, ":%s SASL %s %s %s %s", parv[0], parv[1], parv[2], parv[3], parv[4]);
	}
}

/*
 * Executed when the client aborted the SASL authentication exchange implicitly.
 * Example: the client sent "CAP END" before completing the authentication.
 */
int process_implicit_sasl_abort(aClient *sptr)
{
	if (sptr->sasl_service != NULL)
	{
		// Inform the service that the authentication has been aborted
		sendto_service(sptr->sasl_service, "SASL %s %s D A", sptr->uid, sptr->sasl_service->name);
	}

	/*
	 * If SASL authentication fails, some clients are automatically trying to register without authentication
	 * which would expose the IP address of the user. We disconnect him until he explicitly decides to connect
	 * without authentication.
	 */
	sptr->exitc = EXITC_SASL_REQUIRED;
	exit_client(sptr, sptr, sptr, "SASL authentication failed");
	return FLUSH_BUFFER;
}

/*
 * Sends a message to a service.
 * If the service is not connected to this server, ENCAP will be sent. This way the message is also routed if not all
 * servers support the "SASL" command.
 */
void sendto_service(aClient *service, char *fmt, ...)
{
	char buf[BUFSIZE];
	va_list va;

	if (!IsService(service))
	{
		return;
	}

	va_start(va, fmt);
	vsnprintf(buf, BUFSIZE, fmt, va);
	va_end(va);

	if (MyConnect(service))
	{
		sendto_one(service, ":%s %s", me.serv->sid, buf);
	}
	else
	{
		sendto_one(service, ":%s ENCAP %s PARSE %s %s", me.serv->sid, service->service->servp->sid, me.serv->sid, buf);
	}
}

/*
 * Disconnects all local clients that are currently negotiating with a specific SASL service.
 * This function must be called when a SASL service exits.
 */
void unlink_sasl_service(aClient *cptr)
{
	aClient *acptr;
	int i;
	char comment[BUFSIZE];

	snprintf(comment, BUFSIZE, "%s died", cptr->name);

	for (i = 0; i <= highest_fd; i++)
	{
		if (!(acptr = local[i]))
			continue;

		if (acptr->sasl_service == cptr && !IsRegisteredUser(acptr))
		{
			exit_client(acptr, acptr, &me, comment);
		}
	}
}