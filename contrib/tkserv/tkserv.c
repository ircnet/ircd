/* 
** Powered by Linux. :-)
**
** Copyright (c) 1998 Kaspar 'Kasi' Landsberg, <kl@berlin.Snafu.DE> 
**
** File     : tkserv.c v1.2
** Author   : Kaspar 'Kasi' Landsberg, <kl@snafu.de>
** Desc.    : Temporary K-line Service.
**            For further info see the README file.
** Location : http://www.snafu.de/~kl/tkserv
** Usage    : tkserv <server> <port>
** E.g.     : tkserv localhost 6667
**
** This program is distributed under the GNU General Public License in the 
** hope that it will be useful, but without any warranty. Without even the 
** implied warranty of merchantability or fitness for a particular purpose. 
** See the GNU General Public License for details.
**
** Note: The C version of this service is based on parts of the 
**       ircII-EPIC IRC client by the EPIC Software Labs and on
**       the NoteServ service by Kai 'Oswald' Seidler - see
**       http://oswald.pages.de for more about NoteServ. 
**       Thanks to both. =)
**
** PS: Casting rules the world! (doh)
*/

#include "os.h"
#undef strcasecmp
#include "config.h"
#include "tkconf.h"
#include "proto.h"

/* Max. kill reason length */
#define TKS_MAXKILLREASON 128

/* Used max. length for an absolute Unix path */
#define TKS_MAXPATH 128

/* Max. buffer length (don't change this?) */
#define TKS_MAXBUFFER 8192

/* don't change this either(?) */
#define TKS_MAXARGS 250

/* The version information */
#define TKS_VERSION "Hello, i'm TkServ v1.2."

static char *nuh;
FILE *tks_logf;
int fd = -1, tklined = 0;

/*
** Returns the current time in a formated way.
** Yes, "ts" stands for "time stamp". I know,
** this hurts, but i couldn't find any better
** description. ;->
*/
char *tks_ts(void)
{
    static char tempus[256];
    time_t now;
    struct tm *loctime;

    /* Get the current time */
    now = time(NULL);

    /* Convert it to local time representation */
    loctime = localtime(&now);
    
    strftime(tempus, 256, "@%H:%M %m/%d", loctime);

    return(tempus);
}

/* logging routine, with timestamps */
void tks_log(char *text, ...)
{
    char txt[TKS_MAXBUFFER];
    va_list va;

    tks_logf = fopen(TKSERV_LOGFILE, "a");
    va_start(va, text);
    vsprintf(txt, text, va);

    if (tks_logf != NULL)
        fprintf(tks_logf, "%s %s\n", txt, tks_ts());
    else
    {
        perror(TKSERV_LOGFILE);
        va_end(va);
        return;
    }

    va_end(va);
    fclose(tks_logf);
}

/* an optimized system() function */
void exec_cmd(char *cmd, ...)
{
    char command[TKS_MAXBUFFER];
    va_list va;

    va_start(va, cmd);
    vsprintf(command, cmd, va);
    system(command);
    va_end(va);
}

/* sends a string (<= TKS_MAXBUFFER) to the server */
void sendto_server(char *buf, ...)
{
    char buffer[TKS_MAXBUFFER];
    va_list va;

    va_start(va, buf);
    vsprintf(buffer, buf, va);
    write(fd, buffer, strlen(buffer));
    va_end(va);
    
#ifdef TKSERV_DEBUG
    printf("%s", buffer);
#endif
}

/* sends a NOTICE to the SQUERY origin */
void sendto_user(char *text, ...)
{
    char *nick, *ch;
    char txt[TKS_MAXBUFFER];
    va_list va;

    nick = (char *) strdup(nuh);
    ch   = (char *) strchr(nick, '!');
    *ch  = '\0';

    va_start(va, text);
    vsprintf(txt, text, va);
    sendto_server("NOTICE %s :%s\n", nick, txt);
    va_end(va);
}

void process_server_output(char *line)
{
    char *ptr;
    static char *args[TKS_MAXARGS];
    int i, argc = 0;

    while ((ptr = (char *) strchr(line, ' ')) && argc < TKS_MAXARGS)
    {
        args[argc] = line;
        argc++;
        *ptr = '\0';
        line = ptr + 1;
    }

    args[argc] = line;

    for (i = argc + 1; i < TKS_MAXARGS; i++)
        args[i] = "";

    /* 
    ** After successfull registering, backup the ircd.conf file
    ** and set the perms of the log file -- the easy way :)
    */
    if ((*args[0] == ':') && (!strcmp(args[1], "SERVSET")))
    {
        chmod(TKSERV_ACCESSFILE, S_IRUSR | S_IWRITE);
        chmod(TKSERV_LOGFILE, S_IRUSR | S_IWRITE);
        exec_cmd("cp "CPATH" "TKSERV_IRCD_CONFIG_BAK);
        tks_log("Registration successful.");
    }
        
    /* We do only react upon PINGs, SQUERYs and &NOTICES */
    if (!strcmp(args[0], "PING"))
        service_pong();

    if ((*args[0] == ':') && (!strcmp(args[1], "SQUERY")))
        service_squery(args);
    
    if (!strcmp(args[0], "&NOTICES"))
        service_notice(args);
} 

/* reformats the server output */
void parse_server_output(char *buffer)
{
    char *ch, buf[TKS_MAXBUFFER];
    static char tmp[TKS_MAXBUFFER];

    /* server sent an empty line, so just return */
    if (!buffer && !*buffer)
        return;

    while ((ch = (char *) strchr(buffer, '\n')))
    {
        *ch = '\0';

        if (*(ch - 1) == '\r')
            *(ch - 1) == '\0';

        sprintf(buf, "%s%s", tmp, buffer);
        *tmp = '\0';
        process_server_output(buf);
        buffer = ch + 1;
    }

    if (*buffer)
        strcpy(tmp, buffer);
}

/* reads and returns output from the server */
int server_output(int fd, char *buffer)
{
    int n     = read(fd, buffer, TKS_MAXBUFFER);
    buffer[n] = '\0';
    
#ifdef TKSERV_DEBUG
    printf("%s", buffer);
#endif

    return(n);
}

/* is the origin of the /squery opered? */
int is_opered(void)
{
    char *nick, *ch, *token, *u_num, *userh;
    char buffer[TKS_MAXBUFFER];

    nick = (char *) strdup(nuh);
    ch   = (char *) strchr(nick, '!');
    *ch  = '\0';
    sendto_server("USERHOST %s\n", nick);

    /* get the USERHOST reply (hopefully) */
    server_output(fd, buffer);

    token = (char *) strtok(buffer, " ");
    token = (char *) strtok(NULL,   " ");
    u_num = (char *) strdup(token);
    token = (char *) strtok(NULL,   " ");
    token = (char *) strtok(NULL,   " ");
    userh = (char *) strdup(token);

    /* if we got the USERHOST reply, perform the check */ 
    if (!strcmp(u_num, "302"))
    {
        char *ch;
        ch = (char *) strchr(userh, '=') - 1;

        /* is the origin opered? */
        if (*ch == '*')
        {
            char *old_uh, *new_uh, *ch;

            old_uh = (char *) (strchr(nuh, '!') + 1);
            new_uh = (char *) (strchr(userh, '=') + 2);

            if (ch = (char *) strchr(new_uh, '\r'))
                *ch = '\0';

            /* Does the u@h of the USERHOST reply correspond to the u@h of our origin? */
            if (!strcmp(old_uh, new_uh))
                return(1);
            else 
                /* 
                ** race condition == we sent a USERHOST request and got the USERHHOST reply,
                ** but this reply doesn't correspond to our origin of the SQUERY --
                ** this should never happen (but never say never ;)
                */
                sendto_user("A race condition has occured -- please try again.");
        }
    }
    else
        /*
        ** race condition == we sent a USERHOST request but the next message from
        ** the server was not a USERHOST reply (usually due to lag)
        */
        sendto_user("A race condition has occured -- please try again (and ignore the following error message).");

    return(0);
}

/* 
** Look for an entry in the access file and
** see if the origin needs to be opered
*/
int must_be_opered()
{
    FILE *fp;

    /* if the access file exists, check for auth */
    if ((fp = fopen(TKSERV_ACCESSFILE, "r")) != NULL)
    {
        char buffer[TKS_MAXBUFFER];
        char *access_uh, *token, *uh;

        while (fgets(buffer, TKS_MAXBUFFER, fp))
        {
            uh         = (char *) (strchr(nuh, '!') + 1);
            token      = (char *) strtok(buffer, " ");

            if (token)
                access_uh  = (char *) strdup(token);
                
            /* check for access file corruption */
            if (!access_uh)
            {
                tks_log("Corrupt access file. RTFM. :-)");

                return(0);
            }

			/* do we need an oper? */
            if (*access_uh == '!')
            {
                if (!fnmatch((char *) (strchr(access_uh, '!') + 1), uh, 0))
                    return(0);
            }
        }
    }
    else
        tks_log("%s not found.", TKSERV_ACCESSFILE);

    return(1);
}

/* check whether origin is authorized to use the service */
int is_authorized(char *pwd, char *host)
{
    FILE *fp;

    /* if the access file exists, check for authorization */
    if ((fp = fopen(TKSERV_ACCESSFILE, "r")) != NULL)
    {
        char buffer[TKS_MAXBUFFER];
        char *access_uh, *access_pwd;
        char *token, *uh, *ch, *tlds = NULL;

        while (fgets(buffer, TKS_MAXBUFFER, fp))
        {
            uh         = (char *) (strchr(nuh, '!') + 1);
            token      = (char *) strtok(buffer, " ");

            if (token)
                access_uh  = (char *) strdup(token);
                
            if (*access_uh == '!')
                access_uh = (char *) (strchr(access_uh, '!') + 1);

            token = (char *) strtok(NULL, " ");

            if (token)
                access_pwd = (char *) strdup(token);

            token = (char *) strtok(NULL, " ");

            if (token)
                tlds   = (char *) strdup(token);
            else
                if (ch = (char *) strchr(access_pwd, '\n'))
                    *ch = '\0';

            /* check for access file corruption */
            if (!access_uh || !access_pwd)
            {
                tks_log("Corrupt access file. RTFM. :-)");

                return(0);
            }

            /* check uh, pass and TLD */
            if (!fnmatch(access_uh, uh, 0))
                if (!strcmp(pwd, access_pwd))
                    if (!tlds)
                        return(1);
                    else
                    {
                        char *token, *ch;

                        /* blah */
                        if (ch = (char *) strchr(tlds, '\n'))
                            *ch = '\0';

                        token = (char *) strtok(tlds, ",");

                        /* '!' negates the given host/domain -> not allowed to tkline */
                        if (*token == '!')
                        {
                            if (!fnmatch(((char *) strchr(token, '!') + 1), host, 0))
                            {
                                sendto_user("You are not allowed to tkline \"%s\",", host);
                                return(0);
                            }
                        }
                        else if (!fnmatch(token, host, 0))
                            return(1);

                        /* walk thru the list */
                        while (token = (char *) strtok(NULL, ","))
                        {
                            if (*token == '!')
                            {
                                if (!fnmatch((char *) (strchr(token, '!') + 1), host, 0))
                                {
                                    sendto_user("You are not allowed to tkline \"%s\",", host);
                                    return(0);
                                }
                            }
                            else if (!fnmatch(token, host, 0))
                                return(1);
                        }
                        
                        sendto_user("You are not allowed to tkline \"%s\".", host);
                    }
        }

    }
    else
        tks_log("%s not found.", TKSERV_ACCESSFILE);

    return(0);
}

/*************** ircd.conf section ****************/

/* Appends new tklines to the ircd.conf file */
int add_tkline(char *host, char *user, char *reason, int lifetime)
{
    FILE *iconf;

    if (iconf = fopen(CPATH, "a"))
    {
        time_t now;

        now = time(NULL);
        fprintf(iconf, "K:%s:%s:%s:0 # %d %u tkserv\n", 
                host, reason, user, lifetime, now);
        fclose(iconf);
        rehash(1);
        tks_log("K:%s:%s:%s:0 added for %d hour(s) by %s.",
            host, reason, user, lifetime, nuh);

        return(1);
    }
    else
        tks_log("Couldn't write to "CPATH);

    return(0);
}

/* Check for expired tklines in the ircd.conf file */
int check_tklines(char *host, char *user, int lifetime)
{
    FILE *iconf, *iconf_tmp;
    
    if ((iconf = fopen(CPATH, "r")) && (iconf_tmp = fopen(TKSERV_IRCD_CONFIG_TMP, "w")))
    {
        int count = 0, found = 0;
        time_t now;
        char buffer[TKS_MAXBUFFER];
        char buf_tmp[TKS_MAXBUFFER];
        
        /* just in case... */
        chmod(TKSERV_IRCD_CONFIG_TMP, S_IRUSR | S_IWRITE);

        now = time(NULL);

        while (fgets(buffer, TKS_MAXBUFFER, iconf))
        {
            if ((*buffer != 'K') || (!strstr(buffer, "tkserv")))
                fprintf(iconf_tmp, buffer);
            else
            {
                /*
                ** If lifetime has a value of -1, look for matching
                ** tklines and remove them. Otherwise, perform
                ** the expiration check.
                */
                if (lifetime == -1)
                {
                    char *token;
                    char buf[TKS_MAXBUFFER];

                    strcpy(buf, buffer);
                    token = (char *) strtok(buf, ":");
                    token = (char *) strtok(NULL, ":");
                    
                    if (!strcasecmp(token, host))
                    {
                        token = (char *) strtok(NULL, ":");
                        token = (char *) strtok(NULL, ":");
                        
                        if (!strcasecmp(token, user))
                        {
                            count++;
                            found = 1;
                        }
                        else
                            fprintf(iconf_tmp, buffer);
                    }
                    else
                        fprintf(iconf_tmp, buffer);
                }
                else
                {
                    char *ch, *token;
                    char buf[TKS_MAXBUFFER];
                    unsigned long int lifetime, then;
                
                    strcpy(buf, buffer);
                    ch       = (char *) strrchr(buf, '#');
                    token    = (char *) strtok(ch, " ");
                    token    = (char *) strtok(NULL, " ");
                    lifetime = strtoul(token, NULL, 0);
                    token    = (char *) strtok(NULL, " ");
                    then     = strtoul(token, NULL, 0);
            
                    if (!(((now - then) / (60 * 60)) >= lifetime))
                        fprintf(iconf_tmp, buffer);
                    else
                        found = 1;
                }
            }
        }
        
        fclose(iconf);
        fclose(iconf_tmp);
        exec_cmd("cp %s %s", TKSERV_IRCD_CONFIG_TMP,CPATH);
        unlink(TKSERV_IRCD_CONFIG_TMP);
        
        if (found)
            rehash(-1);

        return(count);
    }
    else
        tks_log("Error while checking for expired tklines...");
}

/* reloads the ircd.conf file  -- the easy way */
void rehash(int what)
{
    exec_cmd("kill -HUP `cat "PPATH"`");
 
    if (what != -1)
        tklined = what;
}

/*************** end of ircd.conf section **************/
    
/*************** The service command section *************/

/* On PING, send PONG and check for expired tklines */
void service_pong(void)
{
    sendto_server("PONG %s\n", TKSERV_NAME);
    check_tklines(NULL, NULL, 0);
}

/* 
** If we get a rehash, tell the origin that the tklines are active/removed 
** and check for expired tklines... 
*/
void service_notice(char **args)
{
    if ((!strcmp(args[4], "reloading") && (!strcmp(args[5], TKSERV_IRCD_CONF))) ||
        (!strcmp(args[3], "rehashing") && (!strcmp(args[4], "Server"))))
    {
        if (tklined)
        {
            sendto_user("TK-line%s.", (tklined > 1) ? "(s) removed" : " active");
            tklined = 0;
        }
    }
}

/* parse the received SQUERY */
void service_squery(char **args)
{
    char *cmd, *ch;

    nuh  = (char *) strdup(args[0] + 1);
    cmd  = (char *) strdup(args[3] + 1);
 
    if (ch = (char *) strchr(cmd, '\r'))
        *ch = '\0';

    if (!strcasecmp(cmd, "admin"))
    {
        sendto_user(TKSERV_ADMIN_NAME);
        sendto_user(TKSERV_ADMIN_CONTACT);
        sendto_user(TKSERV_ADMIN_OTHER);
    }

    else if (!strcasecmp(cmd, "help"))
        squery_help(args);

    else if (!strcasecmp(cmd, "info"))
    {
        sendto_user("This service is featuring temporary k-lines.");
        sendto_user("It's available at http://www.snafu.de/~kl/tkserv.");
    }

    else if (!strcasecmp(cmd, "quit"))
        squery_quit(args);

    else if (!strcasecmp(cmd, "tkline"))
        squery_tkline(args);

    else if (!strcasecmp(cmd, "version"))
        sendto_user(TKS_VERSION);
        
    else
    	sendto_user("Unknown command. Try HELP.");
}

/* SQUERY HELP */
void squery_help(char **args)
{
    char *ch, *help_about;

    help_about = args[4];

    if (help_about && *help_about)
    {
        if (ch = (char *) strchr(help_about, '\r'))
            *ch = '\0';

        if (!strcasecmp(help_about, "admin"))
            sendto_user("ADMIN shows you the administrative info for this service.");

        if (!strcasecmp(help_about, "help"))
            sendto_user("HELP <command> shows you the help text for <command>.");

        if (!strcasecmp(help_about, "info"))
            sendto_user("INFO shows you a short description about this service.");

        if (!strcasecmp(help_about, "tkline"))
        {
            sendto_user("TKLINE adds a temporary entry to the server's k-line list.");
            sendto_user("TKLINE is a privileged command.");
        }
        
        if (!strcasecmp(help_about, "version"))
            sendto_user("VERSION shows you the version information of this service.");
    }
    else
    {
        sendto_user("Available commands:");
        sendto_user("HELP, INFO, VERSION, ADMIN, TKLINE.");
        sendto_user("Send HELP <command> for further help.");
    }
}

/* SQUERY TKLINE */
void squery_tkline(char **args)
{
    int lifetime, i;
    char *passwd, *pattern, *host, *ch, *user = "*";
    char reason[TKS_MAXKILLREASON];

    /* Before we go thru all this, make sure we don't waste our time... */
    if (must_be_opered())
    {
        if (!is_opered())
        {
            sendto_user("Only IRC-Operators may use this command.");
            return;
        }
    }
    
    i = 5;

    while (args[i] && *args[i])
    {
        if (strchr(args[i], ':'))
        {
            sendto_user("Colons are only allowed in the password.");
            return;
        }

        i++;
    }

    if (args[5] && *args[5])
    {
        if (isdigit(*args[5]) || (*args[5] == '-'))
            lifetime = atoi(args[5]);
        else
        {
            sendto_user("The lifetime may only contain digits.");
            return;
        }
    }
    else
    {
        sendto_user("Usage: TKLINE <password> [<lifetime>] <u@h> <reason>");
        return;
    }

    /* TKLINE <pass> <lifetime> <u@h> <reason> */
    if ((lifetime > 0) && !(args[7] && *args[7]))
    {
        sendto_user("Usage: TKLINE <password> <lifetime> <u@h> <reason>");
        return;
    }

    /* TKLINE <pass> <u@h> <reason> (default expiration) */
    if ((lifetime == 0) && !(args[6] && *args[6]))
    {
        sendto_user("Usage: TKLINE <password> <u@h> <reason>");
        return;
    }

    /* TKLINE <pass> -1 <u@h> (removal of tklines) */
    if ((lifetime == -1) && !(args[6] && *args[6]))
    {
        sendto_user("Usage: TKLINE <password> -1 <u@h>");
        return;
    }
        
    if ((lifetime >= 768) || (lifetime < -1))
    {
        sendto_user("<lifetime> must be greater than 0 and less than 768.");
        return;
    }

    /* I don't want to get confused, so all this may be a bit redundant */

    if (lifetime > 0)
    {
        passwd   = args[4];
        pattern  = args[6];
        strcpy(reason, args[7]);
        i = 8;

        /* I know... */
        while(args[i] && *args[i])
        {
            strncat(reason, " ", TKS_MAXKILLREASON - strlen(reason) - 1);
            strncat(reason, args[i], TKS_MAXKILLREASON - strlen(reason) - 1);
            i++;
        }
        
        if (ch = (char *) strchr(reason, '\r'))
            *ch = '\0';        
    }
    
    if (lifetime == 0)
    {
        if (!(strchr(args[5], '@') || strchr(args[5], '*') ||
              strchr(args[5], '.')))
        {
            sendto_user("<lifetime> must be greater than 0.");
            return;
        }

        passwd   = args[4];
        lifetime = 2; /* Default lifetime */
        pattern  = args[5];
        strcpy(reason, args[6]);
        i = 7;

        while(args[i] && *args[i])
        {
            strncat(reason, " ", TKS_MAXKILLREASON - strlen(reason) - 1);
            strncat(reason, args[i], TKS_MAXKILLREASON - strlen(reason) - 1);
            i++;
        }
        
        if (ch = (char *) strchr(reason, '\r'))
            *ch = '\0';        
    }

    if (lifetime == -1)
    {
        passwd  = args[4];
        pattern = args[6];

        if (ch = (char *) strchr(pattern, '\r'))
            *ch = '\0';
    }


    /* Don't allow "*@*" and "*" in the pattern */
    if (!strcmp(pattern, "*@*") || !strcmp(pattern, "*"))
    {
        sendto_user("The pattern \"%s\" is not allowed.", pattern);
        tks_log("%s tried to tkline/untkline \"%s\".", nuh, pattern);
        return;
    }

    /* Split the pattern up into username and hostname */
    if (ch = (char *) strchr(pattern, '@'))
    {
        host = (char *) (strchr(pattern, '@') + 1);
        user = pattern;
        *ch  = '\0';
    }
    else /* user defaults to "*" */
        host = pattern;

    /*
    ** Make sure there's a dot in the hostname.
    ** The reason for this being that i "need" a dot
    ** later on and that i don't want to perform
    ** extra checks whether it's there or not...
    ** Call this lazyness, but it also makes the service faster. ;-)
    */
    if (!strchr(host, '.'))
    {
        sendto_user("The hostname must contain at least one dot.");
        return;
    }
    
    if (!is_authorized(passwd, host))
    {
        sendto_user("Authorization failed.");
        return;
    }

    if (lifetime == -1)
    {
        int i;

        i = check_tklines(host, user, lifetime);
        sendto_user("%d tkline%sfor \"%s@%s\" found.", i, 
                    (i > 1) ? "s " : " ", user, host);

        if (i > 0)
            rehash(2);
    }
    else
        if (!add_tkline(host, user, reason, lifetime))
            sendto_user("Error while trying to edit the "CPATH" file.");
}

/* SQUERY QUIT 
** Each time we receive a QUIT via SQUERY we check whether
** the supplied password matches the one in the conf file or not.
** If not, an error is sent out. If yes, we close the connection to
** the server.
*/
void squery_quit(char **args)
{
    char *ch;

    if (ch = (char *) strchr(args[4], '\r'))
        *ch = '\0';

    if (!strcmp(args[4], TKSERV_PASSWORD))
    {
        tks_log("Got QUIT from %s. Terminating.", nuh);
        sendto_server("QUIT :Linux makes the world go round. :)\n");
    }
    else
    {
        sendto_user("I refuse to QUIT. Have a look at the log to see why.");
        tks_log("Got QUIT from %s with wrong password. Continuing.", nuh);
    }
}

/**************** End of service command section ***************/

int main(int argc, char *argv[])
{

    char *host, *port, buffer[TKS_MAXBUFFER], last_buf[TKS_MAXBUFFER]; 
    char tmp[TKS_MAXPATH];

    int is_unix    = (argv[1] && *argv[1] == '/');
    int sock_type  = (is_unix) ? AF_UNIX : AF_INET;
    int proto_type = SOCK_STREAM;
    int eof        = 0;

    struct in_addr     LocalHostAddr;
    struct sockaddr_in server;
    struct sockaddr_in localaddr;
    struct hostent *hp;
    struct timeval timeout;

    fd_set read_set;
    fd_set write_set;

    if ((is_unix) && (argc != 2))
    {
        fprintf(stderr, "Usage: %s <server> <port>\n", argv[0]);
        fprintf(stderr, "       %s <Unix domain socket>\n", argv[0]);
        exit(1);
    }
    else if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server> <port>\n", argv[0]);
        fprintf(stderr, "       %s <Unix domain socket>\n", argv[0]);
        exit(1);
    }

    if (!strcmp(TKSERV_DIST, "*"))
    {
        printf("Your service has a global distribution. Please make sure that\n");
        printf("you read the part about the service distribution in the README.\n");
    }

    tks_log("Welcome to TkServ. Lean back and enjoy the show...");

    if ((fd = socket(sock_type, proto_type, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    /* copy the args into something more documentable */
    host = argv[1];

    if (!is_unix)
        port = argv[2];

    /* Unix domain socket */
    if (is_unix)
    {
        struct sockaddr_un name;
        memset(&name, 0, sizeof(struct sockaddr_un));
        name.sun_family = AF_UNIX;
        strcpy(name.sun_path, host);

        if (connect(fd, (struct sockaddr *) &name, strlen(name.sun_path) + 2) == -1)
        {
            perror("connect");
            close(fd);
            exit(1);
        }
    }

    memset(&localaddr, 0, sizeof(struct sockaddr_in));
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr   = LocalHostAddr;
    localaddr.sin_port   = 0;
    
    if (bind(fd, (struct sockaddr *) &localaddr, sizeof(localaddr)))
    {
        perror("bind");
        close(fd);
        exit(1);
    }

    memset(&server, 0, sizeof(struct sockaddr_in));
    memset(&LocalHostAddr, 0, sizeof(LocalHostAddr));

    if (!(hp = gethostbyname(host)))
    {
        perror("resolv");
        close(fd);
        exit(1);
    }

    memmove(&(server.sin_addr), hp->h_addr, hp->h_length);
    memmove((void *) &LocalHostAddr, hp->h_addr, sizeof(LocalHostAddr));
    server.sin_family = AF_INET;
    server.sin_port   = htons(atoi(port));

    if (connect(fd, (struct sockaddr *) &server, sizeof(server)) == -1)
    {
        perror("connect");
        exit(1);
    }

    /* register the service with SERVICE_WANT_NOTICE */
    sendto_server("PASS %s\n", TKSERV_PASSWORD);
    sendto_server("SERVICE %s localhost %s 33554432 0 :%s\n", TKSERV_NAME, TKSERV_DIST, TKSERV_DESC);
    sendto_server("SERVSET 33619968\n");

    timeout.tv_usec = 1000;
    timeout.tv_sec  = 10;

	/* daemonization... i'm sure it's not complete */	
	switch (fork())
	{
		case -1:
			perror("fork()");
			exit(3);
		case 0:
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			if (setsid() == -1)
				exit(4);
			break;
		default:
			return 0;
	}

    /* listen for server output and parse it */
    while (!eof)
    {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(fd, &read_set);

        if (select(FD_SETSIZE, &read_set, &write_set, NULL, &timeout) == -1)
        {
            perror("select");
        }
        
        if (!server_output(fd, buffer))
        {
            printf("Connection closed.\n");
            printf("Last server output was: %s\n", last_buf);
            eof = 1;
        }

        strcpy(last_buf, buffer);
        parse_server_output(buffer);
    }

    close(fd);

    exit(0);
}
/* eof */
