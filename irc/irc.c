/************************************************************************
 *   IRC - Internet Relay Chat, irc/irc.c
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
static  char rcsid[] = "@(#)$Id: irc.c,v 1.9 2003/10/18 17:26:34 q Exp $";
#endif
 
#include "os.h"
#include "c_defines.h"
#define IRC_C
#include "c_externs.h"
#undef IRC_C

#define DEPTH 10
#define KILLMAX 2  /* Number of kills to accept to really die */
                    /* this is to prevent looping with /unkill */

#ifdef AUTOMATON
#ifdef DOCURSES
#undef DOCURSES
#endif
#ifdef DOTERMCAP
#undef DOTERMCAP
#endif
#endif /* AUTOMATON */

struct Command commands[] = {
  { NULL,	"SIGNOFF", SERVER_CMD, "\0\0", MSG_QUIT },
  { do_bye,	"QUIT",    LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_bye,	"EXIT",    LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_bye,	"BYE",     LOCAL_FUNC, "\0\0", MSG_QUIT },
  { do_kill,	"KILL",    LOCAL_FUNC, "\0\0", MSG_KILL },
  { NULL,	"SUMMON",  SERVER_CMD, "\0\0", MSG_SUMMON },
  { NULL,	"STATS",   SERVER_CMD, "\0\0", MSG_STATS },
  { NULL,	"USERS",   SERVER_CMD, "\0\0", MSG_USERS },
  { NULL,	"TIME",    SERVER_CMD, "\0\0", MSG_TIME },
  { NULL,	"DATE",    SERVER_CMD, "\0\0", MSG_TIME },
  { NULL,	"NAMES",   SERVER_CMD, "\0\0", MSG_NAMES },
  { NULL,	"NICK",    SERVER_CMD, "\0\0", MSG_NICK },
  { NULL,	"WHO",     SERVER_CMD, "\0\0", MSG_WHO },
  { NULL,	"WHOIS",   SERVER_CMD, "\0\0", MSG_WHOIS },
  { NULL,	"WHOWAS",  SERVER_CMD, "\0\0", MSG_WHOWAS },
  { do_kill,    "LEAVE",   LOCAL_FUNC, "\0\0", MSG_PART },
  { do_kill,    "PART",    LOCAL_FUNC, "\0\0", MSG_PART },
  { NULL,	"WOPS",    SERVER_CMD, "\0\0", MSG_WALLOPS },
  { do_channel,	"JOIN",    LOCAL_FUNC, "\0\0", MSG_JOIN },
  { do_channel,	"CHANNEL", LOCAL_FUNC, "\0\0", MSG_JOIN },
#ifdef VMSP
  { do_exec,	"EXEC",    LOCAL_FUNC, "\0\0", "EXEC" },
  { do_oper,	"OPER",    LOCAL_FUNC, "\0\0", "OPER" },
#endif
#ifdef GETPASS
  { do_oper,	"OPER",    LOCAL_FUNC, "\0\0", "OPER" },
#else
  { NULL,	"OPER",    SERVER_CMD, "\0\0", MSG_OPER },
#endif
  { do_away,	"AWAY",    LOCAL_FUNC, "\0\0", MSG_AWAY },
  { do_mypriv,  "MSG",     LOCAL_FUNC, "\0\0", MSG_PRIVATE },
  { do_kill,    "TOPIC",   LOCAL_FUNC, "\0\0", MSG_TOPIC },
  { do_cmdch,   "CMDCH",   LOCAL_FUNC, "\0\0", "CMDCH" },
  { NULL,	"INVITE",  SERVER_CMD, "\0\0", MSG_INVITE },
  { NULL,	"INFO",    SERVER_CMD, "\0\0", MSG_INFO },
  { NULL,	"LIST",    SERVER_CMD, "\0\0", MSG_LIST },
  { NULL,	"KILL",    SERVER_CMD, "\0\0", MSG_KILL },
  { do_quote,	"QUOTE",   LOCAL_FUNC, "\0\0", "QUOTE" },
  { NULL,	"LINKS",   SERVER_CMD, "\0\0", MSG_LINKS },
  { NULL,	"ADMIN",   SERVER_CMD, "\0\0", MSG_ADMIN },
  { do_ignore,	"IGNORE",  LOCAL_FUNC, "\0\0", "IGNORE" },
  { NULL,	"TRACE",   SERVER_CMD, "\0\0", MSG_TRACE },
  { do_help,	"HELP",    LOCAL_FUNC, "\0\0", "HELP" },
  { do_log,	"LOG",     LOCAL_FUNC, "\0\0", "LOG" },
  { NULL,	"VERSION", SERVER_CMD, "\0\0", MSG_VERSION },
  { do_clear,	"CLEAR",   LOCAL_FUNC, "\0\0", "CLEAR" },
  { NULL,	"REHASH",  SERVER_CMD, "\0\0", MSG_REHASH },
  { do_query,	"QUERY",   LOCAL_FUNC, "\0\0", "QUERY" },
  { NULL,	"LUSERS",  SERVER_CMD, "\0\0", MSG_LUSERS },
  { NULL,	"MOTD",    SERVER_CMD, "\0\0", MSG_MOTD },
  { do_unkill,	"UNKILL",  LOCAL_FUNC, "\0\0", "UNKILL" },
  { do_server,	"SERVER",  LOCAL_FUNC, "\0\0", "SERVER" },
  { NULL,	"MODE",    SERVER_CMD, "\0\0", MSG_MODE },
#ifdef MSG_MAIL
  { NULL,	"MAIL",    SERVER_CMD, "\0\0", MSG_MAIL },
#endif
  { do_kick,	"KICK",    LOCAL_FUNC, "\0\0", MSG_KICK },
  { NULL,	"USERHOST",SERVER_CMD, "\0\0", MSG_USERHOST },
  { NULL,	"ISON",    SERVER_CMD, "\0\0", MSG_ISON },
  { NULL,	"CONNECT", SERVER_CMD, "\0\0", MSG_CONNECT },
  { do_kill,	"SQUIT",   LOCAL_FUNC, "\0\0", MSG_SQUIT },
  { NULL,	"SERVLIST",SERVER_CMD, "\0\0", MSG_SERVLIST },
  { do_kill, 	"SQUERY",  LOCAL_FUNC, "\0\0", MSG_SQUERY },
  { do_kill,	"NOTICE",  LOCAL_FUNC, "\0\0", MSG_NOTICE },
  { NULL, 	NULL,      0,          "\0\0", NULL }
};

aChannel *channel = NULL;
aClient	me, *client = &me;
anUser	meUser;	/* User block for 'me' --msa */
FILE	*logfile = NULL;
char	buf[BUFSIZE];
char	*querychannel;
int	portnum, termtype = CURSES_TERM;
int	debuglevel = DEBUG_ERROR;
int	unkill_flag = 0, cchannel = 0;
int	QuitFlag = 0;

static	int	KillCount = 0;
static	int	apu = 0;  /* Line number we're currently on screen */
static	int	sock;     /* Server socket fd */
static	char	currserver[HOSTLEN + 1];

#if defined(HPUX) || defined(SVR3) || defined(SVR4)
char	logbuf[BUFSIZ]; 
#endif

#ifdef	MAIL50
int	ucontext = 0,	perslength;
char	persname[81];

struct itmlst
  null_list[] = {{0,0,0,0}},
  gplist[] = {{80, MAIL$_USER_PERSONAL_NAME,
	       &persname, &perslength}};
#endif

int	main(int argc, char *argv[])
{
  static char usage[] =
    "Usage: %s [-c channel] [-k passwd] [-p port] [-i] [-w] [-s] [nickname [server]]\n";
	char	channel[BUFSIZE+1];
	int	length, mode = 0;
	struct	passwd	*userdata;
	char	*cp, *argv0=argv[0], *nickptr, *servptr, ch;
extern	char	*getenv(const char *name);

	if ((cp = rindex(argv0, '/')) != NULL)
		argv0 = ++cp;
	portnum = PORTNUM;
	*buf = *currserver = '\0';
	channel[0] = '\0';
	me.user = &meUser;
	me.from = &me;
	me.info = (char *) malloc(REALLEN + 1);
	setuid(getuid());
	version = make_version();

	while (argc > 1 && argv[1][0] == '-') {
		switch(ch = argv[1][1])
		{
		case 'h':
			printf(usage, argv0);
			exit(1);
			break;
		case 'p':
			length = 0;
			if (argv[1][2] != '\0')
				length = atoi(&argv[1][2]);
			else if (argc > 2) {
				length = atoi(argv[2]);
				argv++;
				argc--;
			}
			if (length <= 0) {
				printf(usage, argv0);
				exit(1);
			}
			cchannel = length;
			break;
		case 'c':
			if (argv[1][2] != '\0')
				strncpy(channel, &argv[1][2], BUFSIZE);
			else if (argc > 2) {
				strncpy(channel, argv[2], BUFSIZE);
				argv++;
				argc--;
			}
			if (!channel[0]) {
				printf(usage, argv0);
				exit(1);
			}
			break;
		case 'i':
			mode |= FLAGS_INVISIBLE;
			break;
		case 'w':
			mode |= FLAGS_WALLOP;
			break;
		case 'k':
                        if (argv[1][2] != '\0')
                                strncpy(me.passwd, &argv[1][2], PASSWDLEN);
                        else if (argc > 2) {
                                strncpy(me.passwd, argv[2], PASSWDLEN);
                                argv++;
                                argc--;
                        }
                        if (!me.passwd[0]) {
                                printf(usage, argv0);
                                exit(1);
                        }
			break;
#ifdef DOTERMCAP
		case 's':
			termtype = TERMCAP_TERM;
			break;
#endif
		case 'v':
			(void)printf("irc %s\n", version);
			exit(0);
		}
		argv++;
		argc--;
	}

	me.name[0] = me.buffer[0] = '\0';
	me.next = NULL;
	me.status = STAT_ME;
	if ((servptr = getenv("IRCSERVER")))
		strncpyzt(currserver, servptr, HOSTLEN);
	if (argc > 2)
		strncpyzt(currserver, argv[2], HOSTLEN);

	do {
		QuitFlag = 0;
		if (unkill_flag < 0)
			unkill_flag += 2;
#ifdef UPHOST
		if (!*currserver)
			strncpyzt(currserver, UPHOST, HOSTLEN);
#else
		if (!*currserver)
			strncpyzt(currserver, me.sockhost, HOSTLEN);
#endif
		if (cchannel > 0)
			portnum = cchannel;

		sock = client_init(currserver, portnum, &me);
		if (sock < 0) {
			printf("sock < 0\n");
			exit(1);
		}
		userdata = getpwuid(getuid());
		if (strlen(userdata->pw_name) >= (size_t) USERLEN) {
			userdata->pw_name[USERLEN-1] = '\0';
		}
		if (strlen(userdata->pw_gecos) >= (size_t) REALLEN) {
			userdata->pw_gecos[REALLEN-1] = '\0';
		}
		/* FIX:    jtrim@orion.cair.du.edu -- 3/14/88 
		   & jto@tolsun.oulu.fi */
		if (!*me.name) {
			if (argc >= 2) {
				strncpy(me.name, argv[1], NICKLEN);
			} else if ((nickptr = getenv("IRCNICK"))) {
				strncpy(me.name, nickptr, NICKLEN);
			} else
#ifdef AUTOMATON
				strncpy(me.name, a_myname(), NICKLEN);
#else
				strncpy(me.name, userdata->pw_name ,NICKLEN);
#endif
		}
		me.name[NICKLEN] = '\0';
		/* END FIX */

		if (argv0[0] == ':') {
			strcpy(me.sockhost, "OuluBox");
			strncpy(me.info, &argv0[1], REALLEN);
			strncpy(meUser.username, argv[1], USERLEN);
		} else {
			sprintf(me.sockhost, "%d", mode);
			if ((cp = getenv("IRCNAME")))
				strncpy(me.info, cp, REALLEN);
			else if ((cp = getenv("NAME")))
				strncpy(me.info, cp, REALLEN);
			else {
#ifdef AUTOMATON
				strncpy(me.info, a_myreal(), REALLEN);
#else
				strncpy(me.info,real_name(userdata),REALLEN);
#endif
				if (me.info[0] == '\0')
					strcpy(me.info, "*real name unknown*");
			}
#ifdef AUTOMATON
			strncpy(meUser.username, a_myuser(), USERLEN);
#else
			strncpy(meUser.username,userdata->pw_name,USERLEN);
#endif
		}
		meUser.server = mystrdup(me.sockhost);
		meUser.username[USERLEN] = '\0';
		me.info[REALLEN] = '\0';
		me.fd = sock;
#ifdef AUTOMATON
		a_init();
#endif
#ifdef DOCURSES
		if (termtype == CURSES_TERM) {
			initscr();
			signal(SIGINT, quit_intr);
			signal(SIGTSTP, suspend_irc);
			noecho();
			crmode();
			clear();
			refresh();
		}
#endif
#ifdef DOTERMCAP
		if (termtype == TERMCAP_TERM) {
			printf("Io on !\n");
			io_on(1);
			clearscreen();
		}
#endif
		if (me.passwd[0])
			sendto_one(&me, "PASS %s", me.passwd);
		sendto_one(&me, "NICK %s", me.name);
		sendto_one(&me, "USER %s %s %s :%s", meUser.username,
			   me.sockhost, meUser.server, me.info);
	        querychannel = (char *)malloc(strlen(me.name) + 1);
		strcpy(querychannel, me.name);	/* Kludge? */
		if (channel[0])
			do_channel(channel, "JOIN");
		myloop(sock);
		if (logfile)
			do_log(NULL, NULL);
		printf("Press any key.");
#ifdef	DOCURSES
		refresh();
#endif
        	getchar();
		printf("\n");
#ifdef DOCURSES
		if (termtype == CURSES_TERM) {
			echo();
			nocrmode();
			endwin();
		} 
#endif
#ifdef DOTERMCAP
		if (termtype == TERMCAP_TERM)
			io_off();
#endif
		apu = 0;
	} while (unkill_flag && KillCount++ < KILLMAX);
	exit(0);
}

void	intr(void)
{
	if (logfile)
		do_log(NULL, NULL);

#ifdef DOCURSES
	if (termtype == CURSES_TERM) {
		echo();
		nocrmode();
		endwin();
	}
#endif
#ifdef DOTERMCAP
	if (termtype == TERMCAP_TERM)
		io_off();
#endif
	exit(0);
}

void	myloop(int sock)
{
	write_statusline();
#ifdef DOTERMCAP
	if (termtype == TERMCAP_TERM)
		put_statusline();
#endif
	client_loop(sock);
}

#define QUERYLEN 50

static	char	cmdch = '/';
static	char	queryuser[QUERYLEN+2] = "";

void	do_cmdch(char *ptr, char *temp)
{
	if (BadPtr(ptr)) {
		putline("Error: Command character not changed");
		return;
	}
	cmdch = *ptr;
}

void	do_quote(char *ptr, char *temp)
{
	if (BadPtr(ptr)) {
		putline("*** Error: Empty command");
		return;
	}
	sendto_one(&me,"%s", ptr);
}

void	do_query(char *ptr, char *temp)
{
	if (BadPtr(ptr)) {
		sprintf(buf, "*** Ending a private chat with %s", queryuser);
		putline(buf);
		queryuser[0] = '\0';
	} else {
		strncpyzt(queryuser, ptr, QUERYLEN);
		sprintf(buf, "*** Beginning a private chat with %s",
			queryuser);
		putline(buf);
	}
}

void	do_mypriv(char *buf1, char *buf2)
{
	char	*tmp = index(buf1, ' ');

	if (tmp == NULL) {
		putline("*** Error: Empty message not sent");
		return;
	}
	if (buf1[0] == ',' && buf1[1] == ' ') {
		sendto_one(&me, "PRIVMSG %s :%s", last_to_me(NULL), &buf1[2]);
		last_from_me(last_to_me(NULL));
		*(tmp++) = '\0';
		sprintf(buf,"-> !!*%s* %s", last_to_me(NULL), tmp);
		putline(buf);
	} else if (buf1[0] == '.' && buf1[1] == ' ') {
		sendto_one(&me, "PRIVMSG %s :%s", last_from_me(NULL), &buf1[2]);
		*(tmp++) = '\0';
		sprintf(buf,"-> ##*%s* %s", last_from_me(NULL), tmp);
		putline(buf);
	} else {
		*(tmp++) = '\0';
		sendto_one(&me, "PRIVMSG %s :%s", buf1, tmp);
		last_from_me(buf1);
		if (*buf1 == '#' || *buf1 == '&' || *buf1 == '+' || atoi(buf1))
			sprintf(buf, "%s> %s", buf1, tmp);
		else
			sprintf(buf,"->%s> %s", buf1, tmp);
		putline(buf);
	}
}

void	do_myqpriv(char *buf1, char *buf2)
{
	if (BadPtr(buf1)) {
		putline("*** Error: Empty message not sent");
		return;
	}
	sendto_one(&me, "PRIVMSG %s :%s", queryuser, buf1);

	sprintf(buf,"-> *%s* %s", queryuser, buf1);
	putline(buf);
}

void	do_mytext(char *buf1, char *temp)
{
	sendto_one(&me, "PRIVMSG %s :%s", querychannel, buf1);
	sprintf(buf,"%s> %s", querychannel, buf1);
	putline(buf);
}

void	do_unkill(char *buf, char *temp)
{
	if (unkill_flag)
		unkill_flag = 0;
	else
		unkill_flag = 1;

	sprintf(buf, "*** Unkill feature turned %s",
		(unkill_flag) ? "on" : "off");
	putline(buf);
}

void	do_bye(char *buf, char *tmp)
{
	unkill_flag = 0;
	sendto_one(&me, "%s :%s", tmp, buf);
}

/* KILL, PART, SQUIT, TOPIC	"CMD PARA1 [:PARA2]" */
void	do_kill(char *buf1, char *tmp)
{
	char *b2;

	b2 = index(buf1, SPACE);		/* find end of servername */
	if (b2)					/* comment */
	    {
		*b2 = 0;
        	sendto_one(&me, "%s %s :%s", tmp, buf1, b2 + 1);
	    }
	else
       		sendto_one(&me, "%s %s", tmp, buf1);
	if (*tmp == 'P') {			/* PART */
		free(querychannel);
	        querychannel = (char *)malloc(strlen(me.name) + 1);
		strcpy(querychannel, me.name);	/* Kludge? */
	}
}

/* "CMD PARA1 PARA2 [:PARA3]" */
void	do_kick(char *buf1, char *tmp)
{
	char *b2, *b3 = NULL;

	b2 = index(buf1, SPACE);		/* find end of channel name */
	if (b2)
	    b3 = index(b2 + 1, SPACE);		/* find end of victim name */
	if (b3)
	    {
		*b3 = 0;
       		sendto_one(&me, "%s %s :%s", tmp, buf1, b3 + 1);
	    }
	else
       		sendto_one(&me, "%s %s :No comment", tmp, buf1);
}

/* "CMD :PARA1" */
void	do_away(char *buf1, char *tmp)
{
	sendto_one(&me, "%s :%s", tmp, buf1);
}

void	do_server(char *buf, char *tmp)
{
	strncpyzt(currserver, buf, HOSTLEN);
	unkill_flag -= 2;
	sendto_one(&me,"QUIT");
	close(sock);
	QuitFlag = 1;
}

void	sendit(char *line)
{
	char	*ptr = NULL;
	struct	Command	*cmd = commands;

	KillCount = 0;
	if (line[0] != cmdch) {
		if (*queryuser)
			do_myqpriv(line, NULL);
		else
			do_mytext(line, NULL);
		return /* 0 */ ;
	}
	if (line[1] == ' ')
		do_mytext(&line[2], NULL);
	else if (line[1]) {
		for ( ; cmd->name; cmd++)
			if ((ptr = mycncmp(&line[1], cmd->name)))
				break;
		if (!cmd->name)
			putline("*** Error: Unknown command");
		else {
			switch (cmd->type)
			{
			case SERVER_CMD:
				sendto_one(&me, "%s %s", cmd->extra, ptr);
				break;
			case LOCAL_FUNC:
				(*cmd->func)(ptr, cmd->extra);
				return;
				break;
			default:
				putline("*** Error: Data error in irc.h");
				break;
			}
		}
	}
}

char	*mycncmp(char *str1, char *str2)
{
	int	flag = 0;
	char	*s1;

	for (s1 = str1; *s1 != ' ' && *s1 && *str2; s1++, str2++) {
		/* if (!isascii(*s1)) */
		if (*s1 & 0x80)
			return 0;
		*s1 = toupper(*s1);
		if (*s1 != *str2)
			flag = 1;
	}
	if (*s1 && *s1 != ' ' && *str2 == '\0')
		return 0;
	if (flag)
		return 0;
	if (*s1)
		return s1 + 1;
	else
		return s1;
}

void	do_clear(char *buf, char *temp)
{
#ifdef DOCURSES
	char	header[HEADERLEN];

	if (termtype == CURSES_TERM) {
		apu = 0;
		sprintf(header, IRCHEADER ,version, me.name, currserver);
		clear();
		standout();
		mvaddstr(LINES - 2, 0, header);
		standend();
		refresh();
	}
#endif
#ifdef DOTERMCAP
	if (termtype == TERMCAP_TERM)
		clearscreen();
#endif
}

void	putline(char *line)
{
	char *ptr, *ptr2;

#ifdef DOCURSES
	char	ch='\0';
	/* int pagelen = LINES - 3; not used -Armin */
	int	rmargin = COLS - 1;
#endif

	/*
	 ** This is a *safe* client--filter out all possibly dangerous
	 ** codes from the messages (this sets them as "_").
	 */
	if (line)
		for (ptr = line; *ptr; ptr++)
			if ((*ptr < 32 && *ptr != 7 && *ptr != 9) ||
			    (*ptr > 126))
				*ptr = '_'; 

	ptr = line;

#ifdef DOCURSES
	if (termtype == CURSES_TERM) {
		while (ptr) {

/* first, see if we have to chop the string into pieces */

			if (strlen(ptr) > (size_t) rmargin) {
				ch = ptr[rmargin];
				ptr[rmargin] = '\0';
				ptr2 = &ptr[rmargin - 1];
			} 
			else
				ptr2 = NULL;

/* move cursor to correct position and place line */

			move(apu,0);
			addstr(ptr);
			if (logfile)
				fprintf(logfile, "%s\n", ptr);

/* now see if we are at the end of the page, and take action */

#ifndef SCROLLINGCLIENT
			/* clear one line. */
			addstr("\n\n");
			if (++apu > LINES - 4) {
				apu = 0;
				move(0,0);
				clrtoeol();
			}

#else /* doesn't work, dumps core :-( */

			if(++apu > LINES - 4) {
				char	header[HEADERLEN];
				/* erase status line */
				move(LINES - 2, 0 );
				clrtobot();
				refresh();
				/* scroll screen */
				scrollok(stdscr,TRUE);
				move(LINES - 1, 0 );
				addstr("\n\n\n\n");
				refresh();
				apu -= 4;
				/* redraw status line */
				sprintf(header, IRCHEADER, version,
					me.name, currserver);
				standout();
				mvaddstr(LINES - 2, 0, header);
				standend();
				tulosta_viimeinen_rivi();
			} else
				addstr( "\n\n" );
#endif

/* finally, if this is a multiple-line line, munge things up so that
	 we print the next line. overwrites the end of the previous line. */

			ptr = ptr2;
			if (ptr2) {
				*ptr2++ = '+';
				*ptr2 = ch;
			}
		}
		refresh();
	}
#endif
#ifdef DOTERMCAP
	if (termtype == TERMCAP_TERM)
		tcap_putline(line);
#endif
#ifdef AUTOMATON
	puts(line);
#endif
}

int	unixuser(void)
{
	return(!StrEq(me.sockhost,"OuluBox"));
}

void	do_log(char *ptr, char *temp)
{
	time_t	tloc;
	char	buf[150];
	char	*ptr2;

	if (!unixuser())
		return;
	if (!logfile) {		          /* logging currently off */
		if (BadPtr(ptr))
			putline("*** You must specify a filename to log to.");
		else {
			if (!(logfile = fopen(ptr, "a"))) {
				sprintf(buf,
					"*** Error: Can't open log file %s.\n",
					ptr);
				putline(buf);
			} else {
#if defined(HPUX) || defined(SVR3) || defined(SVR4)
				setvbuf(logfile,logbuf,_IOLBF,sizeof(logbuf));
#else
# if !defined(_SEQUENT_) && !defined(SVR4)
				setlinebuf(logfile);
# endif
#endif
				time(&tloc);
				sprintf(buf,
					"*** IRC session log started at %s",
					ctime(&tloc));
				ptr2 = rindex(buf, '\n');
				*ptr2 = '.';
				putline(buf);
			}
		}
	} else {                            /* logging currently on */
		if (BadPtr(ptr)) {

			time(&tloc);
			sprintf(buf, "*** IRC session log ended at %s",
				ctime(&tloc));
			ptr2 = rindex(buf, '\n');
			*ptr2 = '.';
			putline(buf);
			fclose(logfile);
			logfile = NULL;
		} else
			putline("*** Your session is already being logged.");
	}
}

/* remember the commas */
#define LASTKEEPLEN (MAXRECIPIENTS * (NICKLEN+1)) 

char	*last_to_me(char *sender)
{
	static	char	name[LASTKEEPLEN+1] = ",";

	if (sender)
		strncpyzt(name, sender, sizeof(name));

	return (name);
}

char	*last_from_me(char *recipient)
{
	static	char	name[LASTKEEPLEN+1] = ".";

	if (recipient)
		strncpyzt(name, recipient, sizeof(name));

	return (name);
}

/*
 * Left out until it works with VMS as well..
 */

#ifdef GETPASS
void	do_oper(char *ptr, xchar *tra)
{
	extern char *getmypass();

	if (BadPtr(ptr))
		ptr = getmypass("Enter nick & password: ");

	sendto_one(&me, "%s %s", xtra, ptr);
}
#endif

/* Fake routine (it's only in server...) */

void	do_channel(char *ptr, char *xtra)
{
	char *p1;

	if (BadPtr(ptr)) {
		putline("*** Which channel do you want to join?");
		return;
	}

	free((char *)querychannel);

	if ((querychannel = (char *)malloc(strlen(ptr) + 1)))
	    {
		/* Copy only channel name from *ptr	-Vesa */
		strcpy(buf, ptr);
		if ((p1 = index(buf, ' ')))
			*p1 = '\0';
		if ((p1 = rindex(buf, ',')))	/* The last channel */
			strcpy(querychannel, p1 + 1);
		else				/* The only channel */
			strcpy(querychannel, buf);
	    }
	else
		printf("Blah! Out of memory?\n");

	sendto_one(&me, "%s %s", xtra, ptr);
}

void	write_statusline(void)
{
#ifdef DOCURSES
	char	header[HEADERLEN];

	if (termtype == CURSES_TERM) {
		sprintf(header, IRCHEADER, version, me.name, currserver);
		standout();
		mvaddstr(LINES - 2, 0, header);
		standend();
	}
#endif
}

RETSIGTYPE	quit_intr(int s)
{
	signal(SIGINT, SIG_IGN);
#ifdef DOCURSES
	if (termtype == CURSES_TERM) {
		clear();
		refresh();
		echo();
		nocrmode();
		endwin();
	}
#endif
	exit(0);
}

