/************************************************************************
 *   IRC - Internet Relay Chat, irc/help.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
static  char rcsid[] = "@(#)$Id: help.c,v 1.2 1997/09/03 17:45:38 kalt Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define HELP_C
#include "c_externs.h"
#undef HELP_C

struct Help helplist[] = {
  { "ADMIN", "/ADMIN <server>",
    { "Prints administrative information about an IRC server.",
      "<server> defaults to your own IRC server.", "", "", "" } },
  { "AWAY", "/AWAY <message>",
    { "<Mark yourself as being away. <message> is a message that will be",
      "automatically sent to anyone who tries sending you a private message.",
      "If you are already marked as being away, /AWAY will change your status",
      "back to \"here.\"", "" } },
  { "BYE", "/BYE",
    { "Exit from IRC. /BYE, /EXIT, /QUIT and /SIGNOFF are identical.",
      "", "", "", "" } },
  { "CHANNEL", "/CHANNEL <channel>",
    { "Leave the current channel and join a new one. Channel is any number",
      "or a string beginning with a plus (+) sign. Numbered channels above 999",
      "are private channels, you cannot see them by /LIST. Negative channels",
      "are secret; they do not appear in /WHO at all. String channels are open",
      "first, but the channel operators can change the mode with /MODE" } },
  { "CLEAR", "/CLEAR",
    { "Clear your screen.", "", "", "", "" } },
  { "CMDCH", "/CMDCH <x>",
    { "Changes your command prefix character to <x>. This is useful if you",
      "often start lines with slashes. For example, after typing \"/cmdch #\"",
      "your commands would look like #who or #links.", "", "" } },
  { "DATE", "/DATE <server>",
    { "Prints the date and time local to a specific server. <server> defaults",
      "to your own IRC server. /DATE and /TIME are identical.", "", "", "" } },
  { "EXIT", "/EXIT",
    { "Exit from IRC. /BYE, /EXIT, /QUIT and /SIGNOFF are identical.",
      "", "", "", "" } },
#ifdef VMSP
  { "EXEC", "/EXEC <CP/CMS command>",
    { "Executes a CP/CMS command. If the command spends some time, you may",
      "be signed off by the server. See UNKILL.",
      "Warning: Screen is cleared after execcuting a command.",
      "", "" } },
#endif
  { "HELP", "/HELP <command>",
    { "/HELP without parameters lists all IRC commands.",
      "/HELP followed by a command name prints a description of that command.",
      "", "", "" } },
  { "IGNORE", "/IGNORE <+|-><nicknames>",
    { "Allows you to automatically ignore messages from certain users. If",
      "+ is specified before <nicknames>, only public messages are ignored.",
      "Similarly, - ignores only private messages. If neither symbol is given",
      "all messages are ignored. /IGNORE without parameters prints the current",
      "list of ignored users." } },
  { "INFO", "/INFO",
    { "Prints some information about IRC.", "", "", "", "" } },
  { "INVITE", "/INVITE <channel> <nickname>",
    { "Invites a user to join your channel. The user must be currently using",
      "IRC.", "", "", "" } },
  { "JOIN", "/JOIN <channel>{,<channel>} [<key>{,<key>}]",
    { "Leave the current channel and join a new one. Channels above 999",
      "are private channels; their numbers are not listed by /WHO. Negative",
      "numbered channels are secret; they do not appear in /WHO at all.",
      "/JOIN and /CHANNEL are identical.", "" } },
  { "KICK", "/KICK <channel> <user> [<comment>]",
    { "Kicks specified user off given channel",
      "Only channel operators are privileged to use this command",
      "Channel operator privileges can be given to other users of channel",
      "by command '/MODE <channel> +o <user>' and taken away by command",
      "'/MODE <channel> -o <user>'" } },
  { "LINKS", "/LINKS [<pattern> [<server>]]",
    { "Lists all active IRC servers.",
      "If <pattern> is given, list all active irc links matching <pattern>",
      "For example, /links *.fi lists all links in Finland", "", "" } },
  { "LIST", "/LIST",
    { "Lists all active channels and, if set, their topics.", "", "", "", "" } },
  { "LUSERS", "/LUSERS",
    { "Show the number of people and servers connected to the IRC network.",
      "", "", "", "" } },
  { "LOG", "/LOG <filename>",
    { "Sends a copy of your IRC session to a file.",
      "/LOG followed by a filename begins logging in the given file.",
      "/LOG with no parameters turns logging off.", "", "" } },
  { "MSG", "/MSG <nicknames> <message>",
    { "Send a private message. <nicknames> should be one or more nicknames or",
      "channel numbers separated by commas (no spaces). If <nicknames> is \",\"",
      "your message is sent to the last person who sent you a private message.",
      "If <nicknames> is \".\" it's sent to the last personyou sent one to.",
      "Messages sent to , or . can (currently) contain no other recipients." } },
  { "MODE", "/MODE <channel> [+|-]<modechars> <parameters>",
    { "Mode command is quite complicated and it allows channel operators to",
      "change channel mode. <modechars> is one of m (moderated), s (secret),",
      "p (private), l (limited), t (topiclimited), a (anonymous), o (oper)",
      "i (inviteonly). + or - sign whether the specifies mode should be added",
      "or deleted. Parameter for l is the maximum users allowed" } },
  { "MOTD", "/MOTD <server>",
    { "Query for message-of-today in given server. If <server> parameter is",
      "left out, query local server", "", "", "" } },
  { "NAMES", "/NAMES <channel>{,<channel>}",
    { "/NAMES without a parameter lists the nicknames of users on all channels.",
      "/NAMES followed by a channel number lists the names on that channel.",
      "", "", "" } },
  { "NICK", "/NICK <nickname>",
    { "Change your nickname. You cannot choose a nickname that is already in",
      "use. Additionally, some characters cannot be used in nicknames.",
      "", "", "" } },
  { "QUERY", "/QUERY <nicknames>",
    { "Begin private chat with <nicknames>. All subsequent messages you type",
      "will be automatically sent only to <nicknames>. /QUERY without",
      "parameters ends any current chat. You can send a normal message to your",
      "channel by prefixing it with a slash and a space, like \"/ hi\".", "" } },
  { "QUIT", "/QUIT [<comment>]",
    { "Exit from IRC. /BYE, /EXIT, /QUIT and /SIGNOFF are identical.",
      "", "", "", "" } },
  { "SERVER", "/SERVER <server>",
    { "Disconnects from currect server and connects your client into a new",
      "server specified in command line", "", "", "" } },
  { "SIGNOFF", "/SIGNOFF",
    { "Exit from IRC. /BYE, /EXIT, /QUIT and /SIGNOFF are identical.",
      "", "", "", "" } },
  { "STATS", "/STATS [<c|h|i|k|l|m|n|o|q|y>]",
    { "Shows various IRC server statistics. This command is rather boring",
      "", "", "", "" } },
  { "SUMMON", "/SUMMON <user> [<server>]",
    { "Ask a user to enter IRC. <user> is of the form guest@tolsun.oulu.fi.",
      "You can only summon users on machines where an IRC server is running.",
      "Some servers may have disabled the /SUMMON command.", "", "" } },
  { "TIME", "/TIME <server>",
    { "Prints the date and time local to a specific server. <server> defaults",
      "to your own IRC server. /DATE and /TIME are identical.", "", "", "" } },
  { "TOPIC", "/TOPIC <channel> [<topic>]",
    { "Sets the topic for the channel you're on.", "", "", "", "" } },
  { "UNKILL", "/UNKILL",
    { "Orders to irc reconnect to server if you happen to become killed",
      "accidentally or in purpose", "", "", "" } },
  { "USERS", "/USERS <host>",
    { "List all users logged in to a host. The host must be running an IRC",
      "server. Finger(1) usually works better.", "", "", "" } },
  { "VERSION", "/VERSION <server>",
    { "Prints the version number of an IRC server. <server> defaults to your",
      "own IRC server.", "", "", "" } },
  { "WHO", "/WHO <channel> [<o>]",
    { "/WHO without parameters lists users on all channels.",
      "/WHO followed by a channel number lists users on that channel.",
      "/WHO * lists users that are on the same channel as you.",
      "You cannot see users that are on negative-numbered channels.", "" } },
  { "WHOIS", "/WHOIS <nicknames>{,<nickname>}",
    { "/WHOIS prints information about a particular user, including his or",
      "her name, host name and IRC server. <nicknames> should be one of more",
      "nicknames separated by commas.", "", "" } },
  { "WHOWAS", "/WHOWAS <nickname>{,<nickname>} [<count> [<server>]]",
    { "/WHOWAS returns nickname history information for each of the given",
      "nicknames.", "", "", "" } },
  { NULL, NULL,
    { NULL, NULL, NULL, NULL, NULL } }
};

char helpbuf[80];

void do_help(ptr, temp)
char *ptr, *temp;
{
  struct Help *hptr;
  int count;

  if (BadPtr(ptr)) {
    sprintf(helpbuf, "*** Help: Internet Relay Chat v%s Commands:", version);
    putline(helpbuf);
    count = 0;
    for (hptr = helplist; hptr->command; hptr++) {
      sprintf(&helpbuf[count*10], "%10s", hptr->command);
      if (++count >= 6) {
	count = 0;
	putline(helpbuf);
      }
    }
    if (count)
      putline(helpbuf);
    putline("Type /HELP <command> to get help about a particular command.");
    putline("For example \"/HELP signoff\" gives you help about the");
    putline("/SIGNOFF command. To use a command you must prefix it with a");
    putline("slash or whatever your current command character is (see");
    putline("\"/HELP cmdch\"");
    putline("*** End Help");
  } else {
    for (hptr = helplist; hptr->command; hptr++) 
      if (mycncmp(ptr, hptr->command)) 
	break;

    if (hptr->command == (char *) 0) {
      putline("*** There is no help information for that command.");
      putline("*** Type \"/HELP\" to get a list of commands.");
      return;
    }
    sprintf(helpbuf, "*** Help: %s", hptr->syntax);
    putline(helpbuf);
    for (count = 0; count < 5; count++)
      if (hptr->explanation[count] && *(hptr->explanation[count])) {
	sprintf(helpbuf, "    %s", hptr->explanation[count]);
	putline(helpbuf);
      }
    putline("*** End Help");
  }
}

