/************************************************************************
 *   IRC - Internet Relay Chat, irc/c_numeric.c
 *   Copyright (C) 1990 Jarkko Oikarinen
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

/* -- Jto -- 16 Jun 1990
 * Added a couple of other numerics...
 */

/* -- Jto -- 03 Jun 1990
 * Added ERR_YOUWILLBEBANNED
 */

/* -- Jto -- 12 May 1990
 * Made RPL_LISTEND, RPL_ENDOFWHO, RPL_ENDOFNAMES and RPL_ENDOFLINKS
 * to simply ignore the message and print out nothing
 */

char c_numeric_id[] = "numeric.c (c) 1989 Jarkko Oikarinen";

#include "struct.h"
#include "common.h"
#include "numeric.h"
#include "msg.h"
#include "sys.h"
#include "h.h"
#include "irc.h"

extern char mybuf[];
/*
** DoNumeric (replacement for the old do_numeric)
**
**	parc	number of arguments ('sender' counted as one!)
**	parv[0]	pointer to 'sender' (may point to empty string) (not used)
**	parv[1]..parv[parc-1]
**		pointers to additional parameters, this is a NULL
**		terminated list (parv[parc] == NULL).
*/

int	do_numeric(numeric, cptr, sptr, parc, parv)
int	numeric;
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *tmp;
	int i;
	time_t l;		/* ctime(&l) on STATS L */
	
	/* ...make sure undefined parameters point to empty string */
	for (i = parc; i < MAXPARA; parv[i++] = "");
	
	switch (numeric)
	    {
	    case ERR_NOSUCHNICK:
		sprintf(mybuf, "*** Error: %s: %s (%s)",
			parv[0], parv[3], parv[2]);
		sendto_one(&me, "WHOWAS %s 1", parv[2]);
		break;
	    case ERR_WASNOSUCHNICK:
		mybuf[0] = '\0';
		break;
	    case ERR_NOSUCHSERVER:
		sprintf(mybuf, "*** Error: %s: No such server (%s)",
			parv[0], parv[2]);
		break;
	    case ERR_NOSUCHCHANNEL:
		sprintf(mybuf, "*** Error: %s: No such channel (%s)",
			parv[0], parv[2]);
		break;
	    case ERR_NOSUCHSERVICE:
		sprintf(mybuf, "*** Error: %s: No such service (%s)",
			parv[0], parv[2]);
		break;
	    case ERR_TOOMANYCHANNELS:
		sprintf(mybuf, "*** Error: %s: You have join max. channels",
			parv[0]);
		break;
	    case ERR_TOOMANYTARGETS:
		sprintf(mybuf, "*** Error: %s: Too many targets given",
			parv[0]);
		break;
	    case ERR_NORECIPIENT:
		sprintf(mybuf, "*** Error: %s: Message had no recipient",
			parv[0]);
		break;
	    case ERR_NOTEXTTOSEND:
		sprintf(mybuf, "*** Error: %s: Empty messages cannot be sent",
			parv[0]);
		break;
	    case ERR_NOTOPLEVEL:
		sprintf(mybuf, "*** Error: %s: No toplevel domainname given",
			parv[0]);
		break;
	    case ERR_WILDTOPLEVEL:
		sprintf(mybuf, "*** Error: %s: Wildcard in toplevel name",
			parv[0]);
		break;
	    case ERR_UNKNOWNCOMMAND:
		sprintf(mybuf, "*** Error: %s: Unknown command (%s)",
			parv[0],parv[2]);
		break;
	    case ERR_NONICKNAMEGIVEN:
		sprintf(mybuf, "*** Error: %s: No nickname given", parv[0]);
		break;
	    case ERR_ERRONEUSNICKNAME:
		sprintf(mybuf,
			"*** Error: %s: Some special characters cannot %s",
			parv[0], "be used in nicknames");
		break;
	    case ERR_NICKNAMEINUSE:
		sprintf(mybuf,
			"*** Error: %s: Nickname %s is already in use. %s",
			parv[0], parv[2], "Please choose another.");
		break;
	    case ERR_SERVICENAMEINUSE:
		sprintf(mybuf, "*** Error: %s: Service %s is already in use.",
			parv[0], parv[2]);
		break;
	    case ERR_SERVICECONFUSED:
		sprintf(mybuf, "Error: %s: Your service name is confused",
			parv[0]);
		break;
	    case ERR_USERNOTINCHANNEL:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] :
			"You have not joined any channel");
		break;
	    case ERR_NOTONCHANNEL:
		sprintf(mybuf, "*** Error: %s: %s %s",
			parv[0], parv[3], parv[2]);
		break;
	    case ERR_INVITEONLYCHAN:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			"Magic locks open only with an invitation key");
		break;
	    case ERR_BANNEDFROMCHAN:
		sprintf(mybuf,"*** Error: %s: %s %s",
			parv[0], "You are banned from the channel", parv[2]);
		break;
	    case ERR_NOTREGISTERED:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] :
			"You have not registered yourself yet");
		break;
	    case ERR_NEEDMOREPARAMS:
		sprintf(mybuf, "*** Error: %s: %s: %s", parv[0], parv[2],
			(parv[3][0]) ? parv[3] : "Not enough parameters");
		break;
	    case ERR_ALREADYREGISTRED:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] : "Identity problems, eh ?");
		break;
	    case ERR_NOPERMFORHOST:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] :
			"Your host isn't among the privileged");
		break;
	    case ERR_PASSWDMISMATCH:
		sprintf(mybuf, "*** Error: %s: %s", parv[0], 
			(parv[2][0]) ? parv[2] : "Incorrect password");
		break;
	    case ERR_YOUREBANNEDCREEP:
		sprintf(mybuf, "*** %s: %s", parv[0], 
			(parv[2][0]) ? parv[2] :
			"You're banned from irc...");
		break;
	    case ERR_YOUWILLBEBANNED:
		sprintf(mybuf, "*** Warning: You will be banned in %d minutes",
			atoi(parv[2]));
		break;
	    case ERR_CHANNELISFULL:
		sprintf(mybuf, "*** Error: %s: Channel %s is full",
			parv[0], parv[2]);
		break;
	    case ERR_CANNOTSENDTOCHAN:
		sprintf(mybuf, "*** Error: Sending to channel is %s",
			"forbidden from heathens");
		break;
	    case ERR_NOPRIVILEGES:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] :
		"Only few and chosen are granted privileges. You're not one.");
		break;
	    case ERR_NOOPERHOST:
		sprintf(mybuf, "*** Error: %s: %s", parv[0],
			(parv[2][0]) ? parv[2] :
	      "Only few of mere mortals may try to enter the twilight zone..");
		break;
	    case ERR_UMODEUNKNOWNFLAG:
		sprintf(mybuf, "*** Error: %s: Unknown User Mode Flag",
			parv[0]);
		break;
	    case ERR_USERSDONTMATCH:
		sprintf(mybuf, "*** Error: %s: Can only change your own mode",
			parv[0]);
		break;
	    case RPL_AWAY:
		sprintf(mybuf, "*** %s: %s is away: %s", parv[0],
			(parv[2][0]) ? parv[2] : "*Unknown*",
			(parv[3][0]) ? parv[3] : "*No message (strange)*");
		break;
	    case RPL_USERHOST:
		sprintf(mybuf, "*** USERHOST reply: %s", parv[2]);
		break;
	    case RPL_ISON:
		sprintf(mybuf, "*** ISON reply: %s", parv[2]);
		break;
	    case RPL_WHOISUSER:
		sprintf(mybuf, "*** %s is %s@%s (%s)",
			parv[2], parv[3], parv[4], parv[6]);
		break;
	    case RPL_WHOWASUSER:
		sprintf(mybuf, "*** %s was %s@%s (%s)", 
			parv[2], parv[3], parv[4], parv[6]);
		break;
	    case RPL_WHOISSERVER:
		if (parc == 4)
			sprintf(mybuf, "*** On irc via server %s (%s)",
				parv[2], parv[3]);
		else
			sprintf(mybuf, "*** On irc via server %s (%s)",
				parv[3], parv[4]);
		break;
	    case RPL_WHOISOPERATOR:
		sprintf(mybuf, "*** %s has a connection to the twilight zone",
			parv[2]);
		break;
	    case RPL_WHOISCHANOP:
		sprintf(mybuf, "*** %s has been touched by magic forces",
			parv[2]);
		break;
	    case RPL_WHOISIDLE:
		sprintf(mybuf, "*** %s %s %s",
			parv[2], parv[3], parv[4]);
		break;
	    case RPL_WHOISCHANNELS:
		sprintf(mybuf, "*** On Channels: %s", parv[3]);
		break;
	    case RPL_LISTSTART:
		sprintf(mybuf, "*** Chn Users  Name");
		break;
	    case RPL_LIST:
		sprintf(mybuf, "*** %3s %5s  %s",
			(parv[2][0] == '*') ? "Prv" : parv[2],
			parv[3], parv[4]);
		break;
	    case RPL_LISTEND:
		mybuf[0] = '\0';
		break;
	    case RPL_NOTOPIC:
		sprintf(mybuf, "*** %s: No Topic is set", parv[0]);
		break;
	    case RPL_TOPIC:
		sprintf(mybuf, "*** %s: Topic on %s is: %s", parv[0], parv[2],
			parv[3]);
		break;
	    case RPL_INVITING:
		sprintf(mybuf, "*** %s: Inviting user %s into channel %s",
			parv[0], parv[2], parv[3]);
		break;
	    case RPL_VERSION:
	/* sprintf(mybuf, "*** %s: Server %s runs ircd %s (%s)", parv[0], */
		sprintf(mybuf, "*** Server %s runs ircd %s (%s)",
			parv[3], parv[2], parv[4]);
		break;
	    case RPL_KILLDONE:
		sprintf(mybuf, "*** %s: May %s rest in peace",
			parv[0], parv[2]);
		break;
	    case RPL_INFO:
		sprintf(mybuf, "*** %s: Info: %s", parv[0], parv[2]);
		break;
#if 1
	    case RPL_MOTD:
		sprintf(mybuf, "*** %s: Motd: %s", parv[0], parv[2]);
		break;
#endif /* 0	Looks better this way!	-Vesa */
	    case RPL_YOUREOPER:
		sprintf(mybuf, "*** %s: %s", parv[0], (parv[2][0] == '\0') ?
	   "You have operator privileges now. Be nice to mere mortal souls" :
			parv[2]);
		break;
	    case RPL_NOTOPERANYMORE:
		sprintf(mybuf, "*** %s: You are No Longer Have Operator %s",
			parv[0], "Privileges");
		break;
	    case RPL_REHASHING:
		sprintf(mybuf, "*** %s: %s", parv[0], (parv[2][0] == '\0') ?
			"Rereading configuration file.." : parv[2]);
		break;
	    case RPL_MYPORTIS:
		sprintf(mybuf, "*** %s: %s %s", parv[0], parv[2], parv[1]);
		break;
	    case RPL_TIME:
		sprintf(mybuf, "*** Time on host %s is %s",
			parv[2], parv[3]);
		break;
	    case RPL_CHANNELMODEIS:
		sprintf(mybuf, "*** Mode is %s %s %s",
			parv[2], parv[3], parv[4]);
		break;
	    case RPL_LINKS:
		m_linreply(cptr, sptr, parc, parv);
		break;
	    case RPL_WHOREPLY:
		m_newwhoreply(parv[2],parv[3],parv[4],parv[6],parv[7],parv[8]);
		break;
	    case RPL_NAMREPLY:
		m_newnamreply(cptr, sptr, parc, parv);
		break;
	    case RPL_BANLIST:
		sprintf(mybuf, "*** %s is banned on %s",
			parv[3], parv[2]);
		break;
	    case RPL_TRACELINK:
                sprintf(mybuf,"%s<%s> Link %s> %s (%s up %s) bQ:%s fQ:%s",
                        parv[0], parv[3], parv[6], parv[4], parv[5], parv[7],
                        parv[8], parv[9]);
  		break;
	    case RPL_TRACESERVER:
		if (parc <= 5)
			sprintf(mybuf,"*** %s Class: %s %s: %s",
				parv[0], parv[3], parv[2], parv[4]);
		else
			sprintf(mybuf,"*** %s %s Cl:%s %s/%s %s %s %s",
				parv[0], parv[2], parv[3], parv[4],
				parv[5], parv[6], parv[7], parv[8]);
		break;
	    case RPL_TRACECONNECTING:
	    case RPL_TRACEHANDSHAKE:
	    case RPL_TRACEUNKNOWN:
	    case RPL_TRACEOPERATOR:
	    case RPL_TRACEUSER:
	    case RPL_TRACESERVICE:
	    case RPL_TRACENEWTYPE:
		sprintf(mybuf,"*** %s %s Class: %s %s",
			parv[0], parv[2], parv[3], parv[4]);
		break;
	    case RPL_TRACELOG:
		sprintf(mybuf,"*** %s File: %s level:%s ",
			parv[0], parv[3], parv[4]);
		break;
	    case RPL_TRACECLASS:
		sprintf(mybuf,"*** %s Class: %s Links: %s",
			parv[0], parv[3], parv[4]);
		break;
	    case RPL_STATSLINKINFO:
		l = time(NULL) - atol(parv[8]);	/* count startup time */
		tmp = (char *) ctime(&l);
		*((char *) index(tmp, '\n')) = (char) '\0'; /* remove '\n' */
		sprintf(mybuf,"*** %s: %s Q:%s sM:%s sB:%s rM:%s rB:%s D:%s",
			parv[0], parv[2], parv[3], parv[4], parv[5],
			parv[6], parv[7], tmp);
		break;
	    case RPL_STATSCOMMANDS:
		sprintf(mybuf, "*** %s: %s has been used %s times (%s bytes)",
			parv[0], parv[2], parv[3], parv[4]);
		break;
	    case RPL_STATSYLINE:
		sprintf(mybuf, "*** %s: Cl:%s Pf:%s Cf:%s Max:%s Sq:%s",
			parv[0], parv[3], parv[4],
			parv[5], parv[6], parv[7]);
		break;
	    case RPL_UMODEIS:
		sprintf(mybuf, "*** %s: Mode for user %s is %s",
			parv[0], parv[1], parv[2]);
		break;
#ifdef HIDE_NUMERICS
	    case RPL_STATSCLINE:
	    case RPL_STATSNLINE:
	    case RPL_STATSILINE:
		sprintf(mybuf, "*** %s: %s:%s:*:%s:%s:%s",
			parv[0], parv[2], parv[3], parv[5], parv[6], parv[7]);
		break;
	    case RPL_STATSKLINE:
	    case RPL_STATSQLINE:
		sprintf(mybuf, "*** %s: %s:%s:%s:%s:%s:%s",
			parv[0], parv[2], parv[3], parv[4],
			parv[5], parv[6], parv[7]);
		break;
	    case RPL_SERVICEINFO:
		sprintf(mybuf, "*** %s: Info For Service %s: %s",
			parv[0], parv[3], parv[4]);
		break;
	    case RPL_ENDOFWHO:
	    case RPL_ENDOFWHOIS:
	    case RPL_ENDOFWHOWAS:
	    case RPL_ENDOFINFO:
	    case RPL_ENDOFMOTD:
	    case RPL_ENDOFUSERS:
	    case RPL_ENDOFLINKS:
	    case RPL_ENDOFNAMES:
	    case RPL_ENDOFSTATS:
	    case RPL_ENDOFBANLIST:
	    case RPL_ENDOFSERVICES:
		mybuf[0] = '\0';
		break;
#endif /* !HIDE_NUMERICS */
	    case RPL_WELCOME:
		strcpy(me.name, parv[1]);
		write_statusline();
	    default:
		sprintf(mybuf, "%03d %s %s %s %s %s %s %s %s %s",
			numeric, parv[0], parv[2],
			parv[3], parv[4], parv[5], parv[6], parv[7],
			parv[8], parv[9]);
		break;
	    }
	if (mybuf[0])
	  putline(mybuf);
	return 0;
}
