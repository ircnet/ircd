/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_msg.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

#ifndef lint
static  char rcsid[] = "@(#)$Id: c_msg.c,v 1.2 1997/09/03 17:45:34 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define C_MSG_C
#include "c_externs.h"
#undef C_MSG_C

char	mybuf[513];

void m_die() {
  exit(-1);
}

int m_mode(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  strcpy(mybuf, "*** Mode change ");
  while (parc--) {
    strcat(mybuf, parv[0]);
    strcat(mybuf, " ");
    parv++;
  }
  putline(mybuf);
  return 0;
}

int m_wall(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** #%s# %s", parv[0], parv[1]);
  putline(mybuf);
  return 0;
}

int m_wallops(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** =%s= %s %s", parv[0], parv[1],
                                BadPtr(parv[2]) ? "" : parv[2]);
  putline(mybuf);
  return 0;
}

int m_ping(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  if (parv[2] && *parv[2])
    sendto_one(client, "PONG %s@%s %s", client->user->username,
	       client->sockhost, parv[2]);
  else
    sendto_one(client, "PONG %s@%s", client->user->username, client->sockhost);
  return 0;
}

int m_pong(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** Received PONG message: %s %s from %s",
	  parv[1], (parv[2]) ? parv[2] : "", parv[0]);
  putline(mybuf);
  return 0;
}

int	m_nick(sptr, cptr, parc, parv)
aClient	*sptr, *cptr;
int	parc;
char	*parv[];
{
	sprintf(mybuf,"*** Change: %s is now known as %s", parv[0], parv[1]);
	if (!mycmp(me.name, parv[0])) {
		strcpy(me.name, parv[1]);
		write_statusline();
	}
	putline(mybuf);
#ifdef AUTOMATON
	a_nick(parv[0], parv[1]);
#endif
  return 0;
}

void m_away(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** %s is away: \"%s\"",parv[0], parv[1]);
  putline(mybuf);
}

void m_server(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** New server: %s", parv[1]);
  putline(mybuf);
}

int m_topic(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf, "*** %s changed the topic on %s to: %s",
	  parv[0], parv[1], parv[2]);
  putline(mybuf);
  return 0;
}

int	m_join(sptr, cptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	char *tmp = index(parv[1], '\007');	/* Find o/v mode in 2.9 */

	if (tmp)
		*tmp = ' ';
	sprintf(mybuf,"*** %s <%s> joined channel %s", 
		parv[0], userhost, parv[1]);
	putline(mybuf);
  return 0;
}

int m_part(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Change: %s has left channel %s (%s)", 
	  parv[0], parv[1], BadPtr(parv[2]) ? parv[1] : parv[2]);
  putline(mybuf);
  return 0;
}

void m_version(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Version: %s", parv[1]);
  putline(mybuf);
}

void m_bye()
{
#if defined(DOCURSES) && !defined(AUTOMATON) && !defined(VMSP)
  echo();
  nocrmode();
  clear();
  refresh();
#endif
  exit(-1);    
}

int m_quit(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Signoff: %s (%s)", parv[0], parv[1]);
  putline(mybuf);
#ifdef AUTOMATON
  a_quit(sender);
#endif
  return 0;
}

int m_kill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Kill: %s (%s)", parv[0], parv[2]);
  putline(mybuf);
  return 0;
}

void m_info(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Info: %s", parv[1]);
  putline(mybuf);
}

void m_squit(sptr, cptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Server %s has died. Snif.", parv[1]);
  putline(mybuf);
}

void m_newwhoreply(channel, username, host, nickname, away, realname)
char *channel, *username, *host, *nickname, *away, *realname;
{
  /* ...dirty hack, just assume all parameters present.. --msa */

  if (*away == 'S')
    sprintf(mybuf, "  %-13s    %s %-42s %s",
	"Nickname", "Chan", "Name", "<User@Host>");
  else {
    int i;
    char uh[USERLEN + HOSTLEN + 1];
    if (!realname)
      realname = "";
    if (!username)
      username = "";
    i = 50-strlen(realname)-strlen(username);

    if (channel[0] == '*')
	channel = "";

    if (strlen(host) > (size_t) i)       /* kludge --argv */
	host += strlen(host) - (size_t) i;

    sprintf(uh, "%s@%s", username, host);

    sprintf(mybuf, "%c %s%s %*s %s %*s",
	away[0], nickname, away+1,
	(int)(21-strlen(nickname)-strlen(away)), channel,
	realname,
	(int)(53-strlen(realname)), uh);
  }
}

void m_newnamreply(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  if (parv[2]) {
    switch (parv[2][0]) {
      case '*':
	sprintf(mybuf,"Prv: %-3s %s", parv[3], parv[4]);
	break;
      case '=':
	sprintf(mybuf,"Pub: %-3s %s", parv[3], parv[4]);
	break;
      case '@':
	sprintf(mybuf,"Sec: %-3s %s", parv[3], parv[4]);
	break;
      default:
	sprintf(mybuf,"???: %s %s %s", parv[3], parv[4], parv[5]);
	break;
    }
  } else
    sprintf(mybuf, "*** Internal Error: namreply");
}

void m_linreply(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Server: %s (%s) %s", parv[2], parv[3], parv[4]);
}

int	m_private(sptr, cptr, parc, parv)
aClient	*sptr, *cptr;
int	parc;
char	*parv[];
{
	anIgnore *iptr;

	iptr = find_ignore(parv[0], (anIgnore *)NULL, userhost);
	if ((iptr != (anIgnore *)NULL) &&
	    ((iptr->flags == IGNORE_TOTAL) ||
	     (IsChannelName(parv[1]) && (iptr->flags & IGNORE_PUBLIC)) ||
	     (!IsChannelName(parv[1]) && (iptr->flags & IGNORE_PRIVATE))))
		return 0;
	check_ctcp(sptr, cptr, parc, parv);

	if (parv[0] && *parv[0]) {
#ifdef AUTOMATON
		a_privmsg(sender, parv[2]);
#else
		if (((*parv[1] >= '0') && (*parv[1] <= '9')) ||
		    (*parv[1] == '-') || (*parv[1] == '+') ||
		    IsChannelName(parv[1]) || (*parv[1] == '$')) {
			sprintf(mybuf,"<%s:%s> %s", parv[0], parv[1], parv[2]);
		} else {
			sprintf(mybuf,"*%s* %s", parv[0], parv[2]);
			last_to_me(parv[0]);
		}
		putline(mybuf);
#endif
	} else
		putline(parv[2]); /* parv[2] and no parv[0] ?! - avalon */
  return 0;
}


int m_kick(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
	sprintf(mybuf,"*** %s kicked %s off channel %s (%s)",
		(parv[0]) ? parv[0] : "*Unknown*",
		(parv[2]) ? parv[2] : "*Unknown*",
		(parv[1]) ? parv[1] : "*Unknown*",
		parv[3]);
	if (!mycmp(me.name, parv[2])) {
		free(querychannel);
		querychannel = (char *)malloc(strlen(me.name) + 1);
		strcpy(querychannel, me.name);  /* Kludge? */
	}
	putline(mybuf);
  return 0;
}

int	m_notice(sptr, cptr, parc, parv)
aClient	*sptr, *cptr;
int	parc;
char	*parv[];
{
        anIgnore *iptr;

        iptr = find_ignore(parv[0], (anIgnore *)NULL, userhost);
        if ((iptr != (anIgnore *)NULL) &&
            ((iptr->flags == IGNORE_TOTAL) ||
             (IsChannelName(parv[1]) && (iptr->flags & IGNORE_PUBLIC)) ||
             (!IsChannelName(parv[1]) && (iptr->flags & IGNORE_PRIVATE))))
                return 0;

	if (parv[0] && parv[0][0] && parv[1]) {
		if ((*parv[1] >= '0' && *parv[1] <= '9') ||
		    *parv[1] == '-' || IsChannelName(parv[1]) ||
		    *parv[1] == '$' || *parv[1] == '+')
			sprintf(mybuf,"(%s:%s) %s",parv[0],parv[1],parv[2]);
		else if (index(userhost, '@'))  /* user */
			sprintf(mybuf, "-%s- %s", parv[0], parv[2]);
                    else                        /* service */
			sprintf(mybuf, "-%s@%s- %s",
                                parv[0], userhost, parv[2]);
		putline(mybuf);
	} else
		putline(parv[2]);
  return 0;
}

int m_invite(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  anIgnore *iptr;
  if ((iptr = find_ignore(parv[0], (anIgnore *)NULL, userhost)) &&
      (iptr->flags & IGNORE_PRIVATE))
	return 0;
#ifdef AUTOMATON
  a_invite(parv[0], parv[2]);
#else
  sprintf(mybuf,"*** %s Invites you to channel %s", parv[0], parv[2]);
  putline(mybuf);
#endif
  return 0;
}

int m_error(sptr, cptr, parc, parv)
aClient *sptr, *cptr;
int parc;
char *parv[];
{
  sprintf(mybuf,"*** Error: %s %s", parv[1], (parv[2]) ? parv[2] : "");
  putline(mybuf);
  return 0;
}

