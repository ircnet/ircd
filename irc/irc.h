/************************************************************************
 *   IRC - Internet Relay Chat, irc/irc.h
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

/* -- Jto -- 07 Jul 1990
 * Added mail command
 */

#ifndef __irc_h_include__
#define __irc_h_include__
 
#include "msg.h"

struct Command {
  void (*func)();
  char *name;
  int type;
  char keybinding[3];
  char *extra;   /* Normally contains the command to send to irc daemon */
};

#define SERVER_CMD   0
#define LOCAL_FUNC  1

extern void do_mypriv(), do_cmdch(), do_quote(), do_query();
extern void do_ignore(), do_help(), do_log(), do_clear();
extern void do_unkill(), do_bye(), do_kill(), do_kick();
extern void do_server(), do_channel(), do_away();
#ifdef VMSP
extern void do_bye(), do_exec();
#endif
#ifdef GETPASS
extern void do_oper();
#endif

#ifdef IRCCMDS
struct Command commands[] = {
  { (void (*)()) 0, "SIGNOFF", SERVER_CMD, "\0\0", MSG_QUIT },
  { do_bye,         "QUIT",    LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_bye,         "EXIT",    LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_bye,         "BYE",     LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_kill,        "KILL",    LOCAL_FUNC, "\0\0", MSG_KILL },
  { (void (*)()) 0, "SUMMON",  SERVER_CMD, "\0\0", MSG_SUMMON },
  { (void (*)()) 0, "STATS",   SERVER_CMD, "\0\0", MSG_STATS },
  { (void (*)()) 0, "USERS",   SERVER_CMD, "\0\0", MSG_USERS },
  { (void (*)()) 0, "TIME",    SERVER_CMD, "\0\0", MSG_TIME },
  { (void (*)()) 0, "DATE",    SERVER_CMD, "\0\0", MSG_TIME },
  { (void (*)()) 0, "NAMES",   SERVER_CMD, "\0\0", MSG_NAMES },
  { (void (*)()) 0, "NICK",    SERVER_CMD, "\0\0", MSG_NICK },
  { (void (*)()) 0, "WHO",     SERVER_CMD, "\0\0", MSG_WHO },
  { (void (*)()) 0, "WHOIS",   SERVER_CMD, "\0\0", MSG_WHOIS },
  { (void (*)()) 0, "WHOWAS",  SERVER_CMD, "\0\0", MSG_WHOWAS },
  { do_kill,	    "LEAVE",   LOCAL_FUNC, "\0\0", MSG_PART },
  { do_kill,	    "PART",    LOCAL_FUNC, "\0\0", MSG_PART },
  { (void (*)()) 0, "WOPS",    SERVER_CMD, "\0\0", MSG_WALLOPS },
  { do_channel,     "JOIN",    LOCAL_FUNC, "\0\0", MSG_JOIN },
  { do_channel,     "CHANNEL", LOCAL_FUNC, "\0\0", MSG_JOIN },
#ifdef VMSP
  { do_exec,        "EXEC",    LOCAL_FUNC, "\0\0", "EXEC" },
  { do_oper,        "OPER",    LOCAL_FUNC, "\0\0", "OPER" },
#endif
#ifdef GETPASS
  { do_oper,        "OPER",    LOCAL_FUNC, "\0\0", "OPER" },
#else
  { (void (*)()) 0, "OPER",    SERVER_CMD, "\0\0", MSG_OPER },
#endif
  { do_away,	    "AWAY",    LOCAL_FUNC, "\0\0", MSG_AWAY },
  { do_mypriv,      "MSG",     LOCAL_FUNC, "\0\0", MSG_PRIVATE },
  { do_kill,        "TOPIC",   LOCAL_FUNC, "\0\0", MSG_TOPIC },
  { do_cmdch,       "CMDCH",   LOCAL_FUNC, "\0\0", "CMDCH" },
  { (void (*)()) 0, "INVITE",  SERVER_CMD, "\0\0", MSG_INVITE },
  { (void (*)()) 0, "INFO",    SERVER_CMD, "\0\0", MSG_INFO },
  { (void (*)()) 0, "LIST",    SERVER_CMD, "\0\0", MSG_LIST },
  { (void (*)()) 0, "KILL",    SERVER_CMD, "\0\0", MSG_KILL },
  { do_quote,       "QUOTE",   LOCAL_FUNC, "\0\0", "QUOTE" },
  { (void (*)()) 0, "LINKS",   SERVER_CMD, "\0\0", MSG_LINKS },
  { (void (*)()) 0, "ADMIN",   SERVER_CMD, "\0\0", MSG_ADMIN },
  { do_ignore,      "IGNORE",  LOCAL_FUNC, "\0\0", "IGNORE" },
  { (void (*)()) 0, "TRACE",   SERVER_CMD, "\0\0", MSG_TRACE },
  { do_help,        "HELP",    LOCAL_FUNC, "\0\0", "HELP" },
  { do_log,         "LOG",     LOCAL_FUNC, "\0\0", "LOG" },
  { (void (*)()) 0, "VERSION", SERVER_CMD, "\0\0", MSG_VERSION },
  { do_clear,       "CLEAR",   LOCAL_FUNC, "\0\0", "CLEAR" },
  { (void (*)()) 0, "REHASH",  SERVER_CMD, "\0\0", MSG_REHASH },
  { do_query,       "QUERY",   LOCAL_FUNC, "\0\0", "QUERY" },
  { (void (*)()) 0, "LUSERS",  SERVER_CMD, "\0\0", MSG_LUSERS },
  { (void (*)()) 0, "MOTD",    SERVER_CMD, "\0\0", MSG_MOTD },
  { do_unkill,      "UNKILL",  LOCAL_FUNC, "\0\0", "UNKILL" },
  { do_server,      "SERVER",  LOCAL_FUNC, "\0\0", "SERVER" },
  { (void (*)()) 0, "MODE",    SERVER_CMD, "\0\0", MSG_MODE },
#ifdef MSG_MAIL
  { (void (*)()) 0, "MAIL",    SERVER_CMD, "\0\0", MSG_MAIL },
#endif
  { do_kick,        "KICK",    LOCAL_FUNC, "\0\0", MSG_KICK },
  { (void (*)()) 0, "USERHOST",SERVER_CMD, "\0\0", MSG_USERHOST },
  { (void (*)()) 0, "ISON",    SERVER_CMD, "\0\0", MSG_ISON },
  { (void (*)()) 0, "CONNECT", SERVER_CMD, "\0\0", MSG_CONNECT },
  { do_kill,        "SQUIT",   LOCAL_FUNC, "\0\0", MSG_SQUIT },
  { (void (*)()) 0, "SERVLIST",SERVER_CMD, "\0\0", MSG_SERVLIST },
  { do_kill,        "SQUERY",  LOCAL_FUNC, "\0\0", MSG_SQUERY },
  { do_kill,        "NOTICE",  LOCAL_FUNC, "\0\0", MSG_NOTICE },
  { (void (*)()) 0, (char *) 0, 0,         "\0\0", (char *) 0 }
};
#else
extern struct Command commands[];
#endif /* IRCCMDS */

extern int client_init();
extern void suspend_irc();
extern void client_loop();
extern void putline();
extern int do_char();
extern int in_insert_mode();
extern int tulosta_viimeinen_rivi();
extern void write_statusline();
extern int get_char();
extern void set_char();
extern int get_position();
extern void sendit();
extern void send_this_line();
extern void bol();
extern void check_ctcp();
extern void m_newnamreply();
extern void m_newwhoreply();
extern void m_linreply();
extern char *mycncmp();
extern void yank();
extern anIgnore *find_ignore();

#endif /* __irc_h_include__ */
