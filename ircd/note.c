/************************************************************************
 *   IRC - Internet Relay Conferencing, ircd/note.c
 *   Copyright (C) 1990, 1994 Jarle Lyngaas
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

/*
 *        Author: Jarle Lyngaas
 *        E-mail: jarlel@ii.uib.no
 */

#include "struct.h"
#ifdef NPATH
#include "numeric.h"
#include <stdio.h>
#include <time.h>
#include "common.h"
#include "sys.h"
#include "channel.h"
#include "h.h"

#define VERSION "v2.7+"

#define NOTE_SAVE_FREQUENCY 30 /* Frequency of save time in minutes */
#define NOTE_MAXSERVER_TIME 120 /* Max days for a request in the server */
#define NOTE_MAXSERVER_MESSAGES 5000 /* Max number of requests in the server */
#define NOTE_MAXUSER_MESSAGES 200 /* Max number of requests for each user */
#define NOTE_MAXSERVER_WILDCARDS 200 /* Max number of server toname w.cards */
#define NOTE_MAXUSER_WILDCARDS 5 /* Max number of user toname wildcards */

#define MAX_DAYS_NO_USE_SPY 31 /* No matches or no user on */
#define ONLY_LOCAL_SPY_ON_CHANNEL  /* To save 50% CPU */
#define MIN_DELAY 10
#define BUF_LEN 256
#define MSG_LEN 512
#define REALLOC_SIZE 1024

#define FLAGS_WASOPER (1<<0)
#define FLAGS_SIGNOFF_REMOVE (1<<1)
#define FLAGS_SORT_BY_TONAME (1<<2)
#define FLAGS_FROM_REG (1<<3)
#define FLAGS_NEWS (1<<4)
#define FLAGS_ON_THIS_SERVER (1<<5)
#define FLAGS_REPEAT_UNTIL_TIMEOUT (1<<6)
#define FLAGS_ALL_NICK_VALID (1<<7)
#define FLAGS_SERVER_GENERATED (1<<8)
#define FLAGS_NICK_AND_WILDCARD_VALID (1<<9)
#define FLAGS_SERVER_GENERATED_DESTINATION (1<<10)
#define FLAGS_DISTRIBUTE (1<<11)
#define FLAGS_CHANNEL_PASSWORD (1<<12)
#define FLAGS_DISPLAY_IF_DEST_REGISTER (1<<13)
#define FLAGS_SEND_ONLY_IF_SENDER_ON_ICN (1<<14)
#define FLAGS_CHANNEL (1<<15)
#define FLAGS_NO_NICKCHANGE_SPY (1<<16)
#define FLAGS_SEND_ONLY_IF_THIS_SERVER (1<<17)
#define FLAGS_SEND_ONLY_IF_DESTINATION_OPER (1<<18)
#define FLAGS_SEND_ONLY_IF_NICK_NOT_NAME (1<<19)
#define FLAGS_SEND_ONLY_IF_NOT_EXCEPTION (1<<20)
#define FLAGS_KEY_TO_OPEN_OPER_LOCKS (1<<21)
#define FLAGS_FIND_CORRECT_DEST_SEND_ONCE (1<<22)
#define FLAGS_DISPLAY_CHANNEL_DEST_REGISTER (1<<23)
#define FLAGS_DISPLAY_SERVER_DEST_REGISTER (1<<24)
#define FLAGS_PRIVATE_DISPLAYED (1<<25)
#define FLAGS_REGISTER_NEWNICK (1<<26)
#define FLAGS_NOTICE_RECEIVED_MESSAGE (1<<27)
#define FLAGS_RETURN_CORRECT_DESTINATION (1<<28)
#define FLAGS_DENY (1<<29)
#define FLAGS_NEWNICK_DISPLAYED (1<<30)

#define DupNewString(x,y) if (!StrEq(x,y)) { MyFree(x); DupString(x,y); }  
#define MyEq(x,y) (!myncmp(x,y,strlen(x)))
#undef	mycmp
#define mycmp mystrcasecmp   /* mycmp sux, making note use double cpu */
#define Usermycmp(x,y) mycmp(x,y)
#define Key(sptr) KeyFlags(sptr,-1)
#define Message(msgclient) get_msg(msgclient, 'm')
#define IsOperHere(sptr) (IsOper(sptr) && MyConnect(sptr))
#define HasPrefix(x) ((x=='^')||(x=='~')||(x=='+')||(x=='=')||(x=='-'))
#define NULLCHAR ((char *)0)
#define SPY_CTRLCHAR 13
#define SECONDS_DAY 3600*24
#define SECONDS_WEEK SECONDS_DAY*7
#define SECONDS_MONTH SECONDS_WEEK*31
#define SECONDS_FOR_ALL_DISTRIBUTE SECONDS_DAY/3
#define SECONDS_SERVER_DISTRIBUTE SECONDS_DAY

/* Using Message(msgclient) to get message part of *any* message
   cause the spy function is an ugly hack saving log in this field - Jarle */

typedef int long32;

typedef struct MsgClient {
            char *fromnick, *fromname, *fromhost, *tonick,
                 *toname, *tohost, *message, *passwd;
            time_t timeout, time;
	    long32 flags, id;
          } aMsgClient;
  
static int note_mst = NOTE_MAXSERVER_TIME,
           note_mum = NOTE_MAXUSER_MESSAGES,
           note_msm = NOTE_MAXSERVER_MESSAGES,
           note_msw = NOTE_MAXSERVER_WILDCARDS,
           note_muw = NOTE_MAXUSER_WILDCARDS,
           note_msf = NOTE_SAVE_FREQUENCY*60,
           wildcard_index = 0,
           toname_index = 0,
           fromname_index = 0,
           max_toname,
           max_wildcards,
           max_fromname,
           m_id = 0,
           changes_to_save = 0,
           fast_distribute_timeout = 0;

static time_t saved_clock = 0;
static aMsgClient **ToNameList, **FromNameList, **WildCardList;
extern aClient *client, *find_person(), *local[];
extern int highest_fd;
extern aChannel *channel;
extern char *myctime();
#ifndef REAL_MALLOC
extern char *MyMalloc(), *MyRealloc(), *myctime();
#endif
static char *ptr = "IRCnote", *note_save_filename_tmp, file_inited = 0;

static unsigned char charmap[] = {
        '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
        '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
        '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
        '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
        '\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
        '\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
        '\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
        '\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
        '\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
        '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
        '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
        '\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
        '\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
        '\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
        '\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
        '\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
        '\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
        '\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
        '\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
        '\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
        '\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
        '\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
        '\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
        '\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
        '\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
        '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
        '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
        '\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
        '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
        '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
        '\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
        '\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

static int mystrcasecmp(s1, s2)
char *s1, *s2;
{
    register unsigned char u1, u2;

    for (;;) {
        u1 = (unsigned char) *s1++;
        u2 = (unsigned char) *s2++;
        if (charmap[u1] != charmap[u2]) {
            return charmap[u1] - charmap[u2];
        }
        if (u1 == '\0') {
            return 0;
        }
    }
}

static char *UserName(sptr)
aClient *sptr;
{
 if (HasPrefix(*(sptr->user->username))) return sptr->user->username+1;
  else return sptr->user->username; 
}

static int numeric(string)
char *string;
{
 register char *c = string;

 if (!*c) return 0;
 while (*c) if (!isdigit(*c)) return 0; else c++;
 return 1;
}

static char *clean_spychar(string)
char *string;
{
 static char buf[MSG_LEN]; 
 char *c, *bp = buf;
 
 for (c = string; *c; c++) if (*c != SPY_CTRLCHAR) *bp++ = *c;
 *bp = 0;
 return buf;
}

static char *myitoa(value)
long32 value;
{
 static char buf[BUF_LEN]; 
  
 sprintf(buf, "%d", value);
 return buf;
}

static char *relative_time(seconds)
time_t seconds;
{
 static char buf[20];
 char *c;
 long32 d, h, m;

 if (seconds < 0) seconds = 0; 
 d = seconds / (3600*24);
 seconds -= d*3600*24;
 h = seconds / 3600;
 seconds -= h*3600;
 m = seconds / 60;
 seconds -= m*60;
 sprintf(buf, "%3dd:%2dh:%2dm", d, h, m);
 c = buf; 
 while (*c) {
              if (*c == ' ') *c = '0';
              c++;
        } 
 return buf;
}

static char *mytime(value)
long32 value;
{
 return (relative_time(timeofday-value));
}

static char *get_msg(msgclient, field)
aMsgClient *msgclient;
char field;
{
 char *c = msgclient->message;
 static char buf[MSG_LEN], *empty_char = "";
 int t, p;
 buf[0] = 0;

 switch(field) {
     case 'n' : t = 1; break;
     case 'u' : t = 2; break;
     case 'h' : t = 3; break;
     case 'r' : t = 4; break;
     case '1' : t = 5; break;
     case '2' : t = 6; break;
     default: t = 0;
   }
 if (!t) {
           while (*c) c++;
           while (c != msgclient->message && *c != SPY_CTRLCHAR) c--;
           if (*c == SPY_CTRLCHAR) c++;
           return c;
       }
 while(t--) {
      p = 0;
      while (*c && *c != SPY_CTRLCHAR) buf[p++] = *c++;
      if (!*c) return empty_char;
      buf[p] = 0; c++;
  };
 return buf;
}       

static void update_spymsg(msgclient)
aMsgClient *msgclient;
{
 long32 t;
 char *buf, *empty = "-", ctrlbuf[2], mbuf[MSG_LEN]; 

 mbuf[0] = 0; ctrlbuf[0] = SPY_CTRLCHAR; ctrlbuf[1] = 0;

 buf = get_msg(msgclient, 'n'); if (!*buf) buf = empty; 
 strcat(mbuf, buf); strcat(mbuf, ctrlbuf); 
 buf = get_msg(msgclient, 'u'); if (!*buf) buf = empty; 
 strcat(mbuf, buf); strcat(mbuf, ctrlbuf); 
 buf = get_msg(msgclient, 'h'); if (!*buf) buf = empty; 
 strcat(mbuf, buf); strcat(mbuf, ctrlbuf); 
 buf = get_msg(msgclient, 'r'); if (!*buf) buf = empty; 
 strcat(mbuf, buf); strcat(mbuf, ctrlbuf); 
 buf = get_msg(msgclient, '1'); if (!*buf) buf = empty; 
 strcat(mbuf, buf); strcat(mbuf, ctrlbuf); 
 strcat(mbuf, myitoa(timeofday-msgclient->time)); 
 strcat(mbuf, ctrlbuf); t = MSG_LEN - strlen(mbuf) - 10;
 strncat(mbuf, clean_spychar(Message(msgclient)), t);
 strcat(mbuf, "\0");
 DupNewString(msgclient->message, mbuf);
 changes_to_save = 1;
}

static char *wildcards(string)
char *string;
{
 register char *c;

 for (c = string; *c; c++) if (*c == '*' || *c == '?') return(c);
 return 0;
}

static int only_wildcards(string)
char *string;
{
 register char *c;

 for (c = string;*c;c++) 
      if (*c != '*' && *c != '?') return 0;
 return 1;
}

static char *split_string(string, field, n)
char *string;
int field, n;
{
 static char buf[MSG_LEN];
 char *c = string;
 int p, t;
 buf[0] = 0;

 while(field--) {
      p = 0; t = n;
      while (*c) {
            if (p > 0 && buf[p-1] != ' ' && 
                *c == ' ' && (field || !--t)) break;
            buf[p] = *c++; if (*buf != ' ') p++; 
        }
      if (!*c) if (!field) {
                  buf[p] = 0; return buf;
                } else {
                         *buf = 0; return buf;
                   }
      buf[p] = 0; c++;
  };
 return buf;
}

static char *wild_fromnick(nick, msgclient)
char *nick;
aMsgClient *msgclient;
{
 static char buf[BUF_LEN];
 char *msg, *c;
 
 if (msgclient->flags & FLAGS_ALL_NICK_VALID) {
     strcpy(buf, "*"); return buf;
   }
 if (msgclient->flags & FLAGS_NICK_AND_WILDCARD_VALID
     && MyEq(msgclient->fromnick, nick)) {
     strcpy(buf, nick);
     strcat(buf, "*"); return buf;
  }
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER) {
    msg = Message(msgclient);
    while (*msg == '%') { 
           msg++; c = split_string(msg, 1, 1);
           if (!mycmp(c, nick)) { 
               strcpy(buf, "*");
               return buf;
	    }
           while (*msg && *msg != ' ') msg++; if (*msg) msg++;
      }     
  }
 return NULLCHAR;
}

static int number_fromname()
{
 register int t, nr = 0;

 timeofday = time(NULL);
 for (t = 1;t <= fromname_index; t++) 
      if (FromNameList[t]->timeout > timeofday
          && !(FromNameList[t]->flags & FLAGS_SERVER_GENERATED_DESTINATION)) 
       nr++;
 return nr;         
}


static int first_tnl_indexnode(name)
char *name;
{
 register int s, t = toname_index+1, b = 0, tname;
 aMsgClient *msgclient;

 if (!t) return 0;
 while ((s = (b+t) >> 1) != b) {
       msgclient = ToNameList[s];
       tname = (msgclient->flags & FLAGS_SORT_BY_TONAME) ? 1 : 0;
       if (mycmp(tname ? msgclient->toname : msgclient->tonick, name) < 0)
        b = s; else t = s;
  }
 return t;
}

static int last_tnl_indexnode(name)
char *name;
{
 register int s, t = toname_index+1, b = 0, tname;
 aMsgClient *msgclient;

 if (!t) return 0;
 while ((s = (b+t) >> 1) != b) {
       msgclient = ToNameList[s];
       tname = (msgclient->flags & FLAGS_SORT_BY_TONAME) ? 1 : 0;
       if (mycmp(tname ? msgclient->toname : msgclient->tonick, name) > 0)
        t = s; else b = s;
   }
 return b;
}

static int first_fnl_indexnode(fromname)
char *fromname;
{
 register int s, t = fromname_index+1, b = 0;

 if (!t) return 0;
 while ((s = (b+t) >> 1) != b)
       if (mycmp(FromNameList[s]->fromname,fromname)<0) b = s; else t = s;
 return t;
}

static int last_fnl_indexnode(fromname)
char *fromname;
{
 register int s, t = fromname_index+1, b = 0;

 if (!t) return 0;
 while ((s = (b+t) >> 1) != b)
       if (mycmp(FromNameList[s]->fromname,fromname)>0) t = s; else b = s;
 return b;
}

static int fnl_msgclient(msgclient)
aMsgClient *msgclient;
{
 register int t, f, l;
 aMsgClient **index_p;

 f = first_fnl_indexnode(msgclient->fromname);
 l = last_fnl_indexnode(msgclient->fromname);
 index_p = FromNameList + f;

 for (t = f; t <= l; t++) 
      if (*(index_p++) == msgclient) return(t);
 return 0;
} 

static int tnl_msgclient(msgclient)
aMsgClient *msgclient;
{
 register int t, f, l, tname;
 aMsgClient **index_p;

 if (msgclient->flags & FLAGS_SORT_BY_TONAME 
     || !wildcards(msgclient->tonick)) {
     tname = (msgclient->flags & FLAGS_SORT_BY_TONAME) ? 1 : 0;
     f = first_tnl_indexnode(tname ? msgclient->toname : msgclient->tonick);
     l = last_tnl_indexnode(tname ? msgclient->toname : msgclient->tonick);
     index_p = ToNameList + f;
  } else {
          index_p = WildCardList + 1;
          f = 1; l = wildcard_index;
      }
 for (t = f; t <= l; t++) 
      if (*(index_p++) == msgclient) return(t);
 return 0;
} 
 
static aMsgClient *new(passwd,fromnick,fromname,fromhost,tonick,
                       toname,tohost,flags,timeout,time,message)
char *passwd,*fromnick,*fromname,*fromhost,
     *tonick,*toname,*tohost,*message;
time_t timeout,time;
long32 flags;
{
 register aMsgClient **index_p;
 register int t;
 int allocate,first,last,n;
 aMsgClient *msgclient;
            
 if (number_fromname() > note_msm) return NULL; 
 if (fromname_index == max_fromname-1) {
    max_fromname += REALLOC_SIZE;
    allocate = max_fromname*sizeof(FromNameList)+1;
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (wildcard_index == max_wildcards-1) {
    max_wildcards += REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList)+1;
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (toname_index == max_toname-1) {
    max_toname += REALLOC_SIZE;
    allocate = max_toname*sizeof(ToNameList)+1;
    ToNameList = (aMsgClient **) MyRealloc((char *)ToNameList,allocate);
  }

 /* Correction if corrupt format */
 if (!(flags & FLAGS_SERVER_GENERATED)
     && !(flags & FLAGS_SERVER_GENERATED_DESTINATION)) 
     flags &= ~FLAGS_SORT_BY_TONAME; 
 if (!wildcards(toname) && wildcards(tonick)) flags |= FLAGS_SORT_BY_TONAME; 

 msgclient = (aMsgClient *)MyMalloc(sizeof(struct MsgClient));
 DupString(msgclient->passwd,passwd);
 DupString(msgclient->fromnick,fromnick);
 DupString(msgclient->fromname,fromname);
 DupString(msgclient->fromhost,fromhost);
 DupString(msgclient->tonick,tonick);
 DupString(msgclient->toname,toname);
 DupString(msgclient->tohost,tohost);
 DupString(msgclient->message,message);
 msgclient->flags = flags;
 msgclient->timeout = timeout;
 msgclient->time = time;
 if (flags & FLAGS_SORT_BY_TONAME || !wildcards(tonick)) {
    n = last_tnl_indexnode(flags & FLAGS_SORT_BY_TONAME ? toname : tonick) + 1;
    index_p = ToNameList+toname_index;
    toname_index++;t = toname_index-n;
    while (t--) {
                  index_p[1] = *index_p; index_p--;
      }
    ToNameList[n] = msgclient;
  } else { 
          wildcard_index++;
          WildCardList[wildcard_index] = msgclient;
     }
 first = first_fnl_indexnode(fromname);
 last = last_fnl_indexnode(fromname);
 if (!(n = first)) n = 1;
 index_p = FromNameList+n;
 while (n <= last && mycmp(msgclient->fromhost,(*index_p)->fromhost)>0) {
        index_p++;n++;
   }
 while (n <= last && mycmp(msgclient->fromnick,(*index_p)->fromnick)>=0){ 
        index_p++;n++;
   }
 index_p = FromNameList+fromname_index;
 fromname_index++;t = fromname_index-n; 
 while (t--) {
               index_p[1] = *index_p; index_p--;
    }
 FromNameList[n] = msgclient;
 changes_to_save = 1;
 msgclient->id = ++m_id;
 return msgclient;
}

static void r_code(string,fp)
register char *string;
register FILE *fp;
{
 register int v;
 register char c, *cp = ptr;

 do {
      if ((v = getc(fp)) == EOF) {
         exit(-1);       
       }
      c = v;
      *string = c-*cp;
      if (!*cp || !*++cp) cp = ptr;
  } while (*string++);
}

static void w_code(string,fp)
register char *string;
register FILE *fp;
{
 register char *cp = ptr;

 do {
      putc((char)(*string+*cp),fp);
      if (!*cp || !*++cp) cp = ptr;
  } while (*string++);
}

static char *ltoa(i)
register long32 i;
{
 static unsigned char c[20];
 register unsigned char *p = c;

  do {
      *p++ = (i&127) + 1;
       i >>= 7;
    } while(i > 0);
    *p = 0;
    return (char *) c;
}

static long32 atol32(c)
register unsigned char *c;
{
 register long32 a = 0;
 register unsigned char *s = c;

 while (*s != 0) ++s;
 while (--s >= c) {
        a <<= 7;
        a += *s - 1;
   }
  return a;
}

static void init_messages()
{
 static FILE *fp = 0;
 FILE *fopen();
 time_t atime, timeout;
 long32 flags;
 int allocate;
 char passwd[20], fromnick[BUF_LEN], fromname[BUF_LEN],
      fromhost[BUF_LEN], tonick[BUF_LEN], toname[BUF_LEN],
      tohost[BUF_LEN], message[MSG_LEN], buf[20];
 static int rnd, first_call = 1;

 if (file_inited) return;
 if (first_call) {
    first_call = 0;
    saved_clock = timeofday;
    srand48(timeofday%(int)passwd);
    max_fromname = max_toname = max_wildcards = REALLOC_SIZE;
    allocate = REALLOC_SIZE*sizeof(FromNameList)+1;
    ToNameList = (aMsgClient **) MyMalloc(allocate);
    FromNameList = (aMsgClient **) MyMalloc(allocate);
    WildCardList = (aMsgClient **) MyMalloc(allocate); 
    note_save_filename_tmp = MyMalloc(strlen(NPATH)+6);
    sprintf(note_save_filename_tmp,"%s.tmp",NPATH);
    fp = fopen(NPATH,"r");
    if (!fp) {
       file_inited = 1; return;
    }
    r_code(buf,fp);note_msm = atol32(buf);
    r_code(buf,fp);note_mum = atol32(buf);
    r_code(buf,fp);note_msw = atol32(buf);
    r_code(buf,fp);note_muw = atol32(buf);
    r_code(buf,fp);note_mst = atol32(buf);
    r_code(buf,fp);note_msf = atol32(buf);
  }
 r_code(passwd,fp);
 if (*passwd) {
    r_code(fromnick,fp);r_code(fromname,fp);
    r_code(fromhost,fp);r_code(tonick,fp);r_code(toname,fp);
    r_code(tohost,fp);r_code(buf,fp),flags = atol32(buf);
    r_code(buf,fp);timeout = atol32(buf);r_code(buf,fp);
    atime = atol32(buf);r_code(message,fp);
    flags &= ~FLAGS_FROM_REG; rnd+=atime;
    if (timeout > 0 && !HasPrefix(*toname) && !HasPrefix(*fromname))
      new(passwd,fromnick,fromname,fromhost,tonick,toname,
	  tohost,flags,timeout,atime,message);
 } else {
          srand48((timeofday+rnd)%(int)passwd);
	  file_inited = 1;
	  fclose(fp);
    }
}

static int numeric_host(host)
char *host;
{
  while (*host) {
        if (!isdigit(*host) && *host != '.') return 0;
        host++;
   } 
 return 1;
}

static int elements_inhost(host)
char *host;
{
 int t = 0;

 while (*host) {
       if (*host == '.') t++;
       host++;
    }
 return t;
}

static int valid_elements(host)
char *host;
{
 register char *c = host;
 register int t = 0, numeric = 0;

 while (*c) {
        if (!isdigit(*c) && *c != '.' && *c != '*' && *c != '?' ) break;
        c++;
   } 
 if (!*c) numeric = 1;
 c = host;
 if (numeric)
     while (*c && *c != '*' && *c != '?') {
            if (*c == '.') t++;
            c++;
	  }
  else {
        while (*c++);
        while (c != host && *c != '*' && *c != '?') { 
               if (*c == '.') t++;
               c--;
          }
    } 
 if (!t && *c != '*' && *c != '?') t = 1;
 return t;
}

static char *local_host(host)
char *host;
{
 static char buf[BUF_LEN];
 char *buf_p = buf, *host_p;
 int t, e;

 e = elements_inhost(host);
 if (e < 2) return host;
 if (!numeric_host(host)) {
     if (!e) return host;
     host_p = host;
     if (e > 2) t = 2; else t = 1;
    while (t--) { 
           while (*host_p && *host_p != '.') {
                  host_p++;buf_p++;
             }
           if (t && *host_p) { 
              host_p++; buf_p++;
	    }
      }
     buf[0] = '*';
     strcpy(buf+1, host_p);
  } else {
          host_p = buf;
          strcpy(buf,host);
          while (*host_p) host_p++;    
          t = 2;
          while(t--) {
               while (host_p != buf && *host_p-- != '.'); 
	     }
           host_p+=2;
           *host_p++ = '*';
           *host_p = 0;
       }
  return buf;
}

static int host_check(host1,host2)
char *host1,*host2;
{
 char buf[BUF_LEN];
 
 if (numeric_host(host1) != numeric_host(host2)) return 0;
 strcpy(buf,local_host(host1));
 if (!mycmp(buf,local_host(host2))) return 1;
 return 0;
}

static time_t Mytimegm(tm)
struct tm *tm;
{
 static int days[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
 register long32 mday = tm->tm_mday-1, mon = tm->tm_mon, year = tm->tm_year-70;

 mday+=((year+2) >> 2)+days[mon];
 if (mon < 2 && !((year+2) & 3)) mday--;
 return tm->tm_sec+tm->tm_min*60+tm->tm_hour*3600+mday*86400+year*31536000;
}

static time_t set_date(sptr,time_s)
aClient *sptr;
char *time_s;
{
 struct tm *tm;
 time_t tm_gmtoff;
 int t,t1,month,date,year;
 char *c = time_s,arg[3][BUF_LEN];
 static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
    }; 

 tm = localtime(&timeofday);
 tm_gmtoff = Mytimegm(tm)-timeofday;
 tm->tm_sec = 0;
 tm->tm_min = 0;
 tm->tm_hour = 0;
 if (*time_s == '-') {
    if (!time_s[1]) return 0;
    return Mytimegm(tm)-tm_gmtoff-SECONDS_DAY*atoi(time_s+1);
  }
 for (t = 0;t<3;t++) {
      t1 = 0;
      while (*c && *c != '/' && *c != '.' && *c != '-')
             arg[t][t1++] = *c++;
      arg[t][t1] = 0;
      if (*c) c++;
  } 

 date = atoi(arg[0]);
 if (*arg[0] && (date<1 || date>31)) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown date",sptr->name);
     return -1;
  }
 month = atoi(arg[1]);
 if (month) month--;
  else for (t = 0;t<12;t++) {
            if (MyEq(arg[1],months[t])) { 
                month = t;break;
             } 
            month = -1;
        }
 if (*arg[1] && (month<0 || month>11)) {
      sendto_one(sptr,"NOTICE %s :#?# Unknown month",sptr->name);
      return -1; 
  }       
 year = atoi(arg[2]);
 if (*arg[2] && (year<71 || year>99)) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown year",sptr->name);
     return -1;
   }
 tm->tm_mday = date;
 if (*arg[1]) tm->tm_mon = month;
 if (*arg[2]) tm->tm_year = year;
 return Mytimegm(tm)-tm_gmtoff;
}

static int local_check(sptr, msgclient, passwd, flags, tonick,
                   toname, tohost, time_l, id)
aClient *sptr;
aMsgClient *msgclient;
char *passwd, *tonick, *toname, *tohost;
long32 flags;
time_t time_l;
int id;
{
 int chn = 0;
 if (IsOper(sptr) 
     && (msgclient->flags & FLAGS_CHANNEL
	 || msgclient->flags & FLAGS_DENY)) chn = 1;

 if (msgclient->flags == flags 
     && (!id || id == msgclient->id)
     && (chn && (msgclient->flags & FLAGS_SERVER_GENERATED
		 || msgclient->flags & FLAGS_DENY
		 || !matches(msgclient->toname, UserName(sptr))
		    && !matches(msgclient->tohost, sptr->user->host))
	 || !Usermycmp(UserName(sptr),msgclient->fromname))
     && (chn || !mycmp(sptr->name, msgclient->fromnick)
         || wild_fromnick(sptr->name, msgclient))
     && (!time_l || msgclient->time >= time_l 
                    && msgclient->time < time_l+SECONDS_DAY)
     && !matches(tonick,msgclient->tonick)
     && !matches(toname,msgclient->toname)
     && !matches(tohost,msgclient->tohost)
     && (*msgclient->passwd == '*' && !msgclient->passwd[1]
         || chn || StrEq(passwd,msgclient->passwd))
     &&  (chn || host_check(sptr->user->host,msgclient->fromhost)))
     return 1;
 return 0;
}

static int send_flag(flags)
long32 flags;
{
  if (flags & FLAGS_RETURN_CORRECT_DESTINATION
      || flags & FLAGS_CHANNEL_PASSWORD
      || flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS
      || flags & FLAGS_DISPLAY_IF_DEST_REGISTER
      || flags & FLAGS_SERVER_GENERATED_DESTINATION
      || flags & FLAGS_DENY
      || flags & FLAGS_NEWS
      || flags & FLAGS_CHANNEL
      || flags & FLAGS_REPEAT_UNTIL_TIMEOUT
         && !(flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE)) return 0;
  return 1;
}

static void display_flags(flags, c, mode)
long32 flags;
char *c, mode;
{
 char t = 0;
 int send = 0;
 
 if (send_flag(flags)) send = 1;
 if (mode != 'q') {
     if (send) c[t++] = '['; else c[t++] = '<';
  } else c[t++] = '+';
 if (mode != 'q' && (flags & FLAGS_WASOPER)) c[t++] = 'O';
 if (flags & FLAGS_SERVER_GENERATED_DESTINATION) c[t++] = 'H';
 if (flags & FLAGS_ALL_NICK_VALID) c[t++] = 'C';
 if (flags & FLAGS_SERVER_GENERATED) c[t++] = 'G';
 if (flags & FLAGS_DISTRIBUTE) c[t++] = 'D';
 if (flags & FLAGS_CHANNEL_PASSWORD) c[t++] = 'P';
 if (flags & FLAGS_NEWS) c[t++] = 'S';
 if (flags & FLAGS_CHANNEL) c[t++] = 'L';
 if (flags & FLAGS_DISPLAY_IF_DEST_REGISTER) c[t++] = 'X';
 if (flags & FLAGS_DISPLAY_CHANNEL_DEST_REGISTER) c[t++] = 'J';
 if (flags & FLAGS_DISPLAY_SERVER_DEST_REGISTER) c[t++] = 'A';
 if (flags & FLAGS_REPEAT_UNTIL_TIMEOUT) c[t++] = 'R';
 if (flags & FLAGS_SIGNOFF_REMOVE) c[t++] = 'M';
 if (flags & FLAGS_NO_NICKCHANGE_SPY) c[t++] = 'U';
 if (flags & FLAGS_NICK_AND_WILDCARD_VALID) c[t++] = 'V';
 if (flags & FLAGS_SEND_ONLY_IF_THIS_SERVER) c[t++] = 'I';
 if (flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME) c[t++] = 'Q';
 if (flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) c[t++] = 'E';
 if (flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS) c[t++] = 'K';
 if (flags & FLAGS_SEND_ONLY_IF_SENDER_ON_ICN) c[t++] = 'Y';
 if (flags & FLAGS_NOTICE_RECEIVED_MESSAGE) c[t++] = 'N';
 if (flags & FLAGS_RETURN_CORRECT_DESTINATION) c[t++] = 'F';
 if (flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) c[t++] = 'B';
 if (flags & FLAGS_REGISTER_NEWNICK) c[t++] = 'T';
 if (flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER) c[t++] = 'W';
 if (flags & FLAGS_DENY) c[t++] = 'Z';
 if (t == 1) c[t++] = '-';
 if (mode != 'q') {
     if (send) c[t++] = ']'; else c[t++] = '>';
  }
 c[t] = 0;
}

static void remove_msg(msgclient)
aMsgClient *msgclient;
{
 register aMsgClient **index_p;
 register int t;
 int n,allocate;

 n = tnl_msgclient(msgclient);
 if (msgclient->flags & FLAGS_SORT_BY_TONAME 
     || !wildcards(msgclient->tonick)) {
     index_p = ToNameList+n; 
     t = toname_index-n;   
     while (t--) {
                   *index_p = index_p[1]; index_p++;
       }
     ToNameList[toname_index] = 0;
     toname_index--;
  } else { 
          index_p = WildCardList+n; 
          t = wildcard_index-n;
          while (t--) {
                        *index_p = index_p[1]; index_p++;
            }
          WildCardList[wildcard_index] = 0;
          wildcard_index--;
     }
 n = fnl_msgclient(msgclient);
 index_p = FromNameList+n;t = fromname_index-n;
 while (t--) {
                *index_p = index_p[1]; index_p++;
   }
 FromNameList[fromname_index] = 0;
 fromname_index--;
 MyFree(msgclient->passwd);
 MyFree(msgclient->fromnick);
 MyFree(msgclient->fromname);
 MyFree(msgclient->fromhost);
 MyFree(msgclient->tonick);
 MyFree(msgclient->toname);
 MyFree(msgclient->tohost);
 MyFree(msgclient->message);
 MyFree((char *)msgclient);
 changes_to_save = 1;
 if (max_fromname - fromname_index > REALLOC_SIZE *2) {
    max_fromname -= REALLOC_SIZE; 
    allocate = max_fromname*sizeof(FromNameList)+1;
    FromNameList = (aMsgClient **) MyRealloc((char *)FromNameList,allocate);
  }
 if (max_wildcards - wildcard_index > REALLOC_SIZE * 2) {
    max_wildcards -= REALLOC_SIZE;
    allocate = max_wildcards*sizeof(WildCardList)+1;
    WildCardList = (aMsgClient **) MyRealloc((char *)WildCardList,allocate);
  }
 if (max_toname - toname_index > REALLOC_SIZE * 2) {
    max_toname -= REALLOC_SIZE;
    allocate = max_toname*sizeof(ToNameList)+1;
    ToNameList = (aMsgClient **) MyRealloc((char *)ToNameList,allocate);
  }
}

static int KeyFlags(sptr, flags) 
aClient *sptr;
long32 flags;
{
 int t,last, first_tnl, last_tnl, nick_list = 1;
 aMsgClient **index_p,*msgclient; 
 
 t = first_tnl_indexnode(sptr->name);
 last = last_tnl_indexnode(sptr->name);
 first_tnl = first_tnl_indexnode(UserName(sptr));
 last_tnl = last_tnl_indexnode(UserName(sptr));
 index_p = ToNameList;
 timeofday = time(NULL);
 while (1) {
     while (last && t <= last) {
	    msgclient = index_p[t];
	    if (msgclient->flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS
		&& msgclient->flags & flags
		&& !matches(msgclient->tonick,sptr->name)
		&& !matches(msgclient->toname,UserName(sptr))
		&& !matches(msgclient->tohost,sptr->user->host)) return 1;
            t++;
	}
     if (index_p == ToNameList) {
         if (nick_list) {
             nick_list = 0; t = first_tnl; last = last_tnl;
	  } else {
                   index_p = WildCardList;
                   t = 1; last = wildcard_index;
	      }
      } else return 0;
 }
}

static int set_flags(sptr, string, flags, mode, type)
aClient *sptr;
char *string, mode, *type;
long32 *flags;
{
 char *c,on,buf[40],cu;
 int op,uf = 0;

 op = IsOperHere(sptr) ? 1:0; 
 for (c = string; *c; c++) {
      if (*c == '+') { 
          on = 1;continue;
       } 
      if (*c == '-') { 
          on = 0;continue;
       } 
      cu = islower(*c)?toupper(*c):*c;
      switch (cu) {
              case 'S': if (on) *flags |= FLAGS_NEWS;
                         else *flags &= ~FLAGS_NEWS;
                        break;             
              case 'R': if (on) *flags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
                         else *flags &= ~FLAGS_REPEAT_UNTIL_TIMEOUT;
                        break;        
              case 'U': if (on) *flags |= FLAGS_NO_NICKCHANGE_SPY;
                         else *flags &= ~FLAGS_NO_NICKCHANGE_SPY;
                        break;             
              case 'V': if (on) *flags |= FLAGS_NICK_AND_WILDCARD_VALID;
                         else *flags &= ~FLAGS_NICK_AND_WILDCARD_VALID;
                        break;             
              case 'I': if (on) *flags |= FLAGS_SEND_ONLY_IF_THIS_SERVER; 
                         else *flags &= ~FLAGS_SEND_ONLY_IF_THIS_SERVER;
                        break;             
              case 'W': if (on) *flags |= FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_DESTINATION_OPER;
                        break;             
              case 'Q': if (on) *flags |= FLAGS_SEND_ONLY_IF_NICK_NOT_NAME;
                         else *flags &= ~FLAGS_SEND_ONLY_IF_NICK_NOT_NAME;
                        break;             
              case 'E': if (on) *flags |= FLAGS_SEND_ONLY_IF_NOT_EXCEPTION; 
                         else *flags &= ~FLAGS_SEND_ONLY_IF_NOT_EXCEPTION;
                        break;             
              case 'Y': if (on) *flags |= FLAGS_SEND_ONLY_IF_SENDER_ON_ICN;
                         else *flags &= FLAGS_SEND_ONLY_IF_SENDER_ON_ICN;
                        break;             
              case 'N': if (on) *flags |= FLAGS_NOTICE_RECEIVED_MESSAGE;
                         else *flags &= ~FLAGS_NOTICE_RECEIVED_MESSAGE;
                        break;             
              case 'F': if (on) *flags |= FLAGS_RETURN_CORRECT_DESTINATION;
                         else *flags &= ~FLAGS_RETURN_CORRECT_DESTINATION;
                        break;
              case 'X':  if (on) *flags |= FLAGS_DISPLAY_IF_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_IF_DEST_REGISTER;
                        break;
              case 'J':  if (on) *flags |= FLAGS_DISPLAY_CHANNEL_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_CHANNEL_DEST_REGISTER;
                        break;
              case 'A':  if (on) *flags |= FLAGS_DISPLAY_SERVER_DEST_REGISTER;
                          else *flags &= ~FLAGS_DISPLAY_SERVER_DEST_REGISTER;
                        break;
              case 'C': if (on) *flags |= FLAGS_ALL_NICK_VALID;
                         else *flags &= ~FLAGS_ALL_NICK_VALID;
                        break;
              case 'M': if (on) *flags |= FLAGS_SIGNOFF_REMOVE;
                         else *flags &= ~FLAGS_SIGNOFF_REMOVE;
                        break;
              case 'T': if (KeyFlags(sptr,FLAGS_REGISTER_NEWNICK) 
                            || op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_REGISTER_NEWNICK;
                             else *flags &= ~FLAGS_REGISTER_NEWNICK;
                         } else buf[uf++] = cu;
                        break;
              case 'L': if (KeyFlags(sptr,FLAGS_CHANNEL)
                            || IsOper(sptr) || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_CHANNEL;
                             else *flags &= ~FLAGS_CHANNEL;
                         } else buf[uf++] = cu;
                        break;
              case 'P': if (KeyFlags(sptr,FLAGS_CHANNEL_PASSWORD)
                            || IsOper(sptr) || mode == 'd' || !on) {
			    if (on) *flags |= FLAGS_CHANNEL_PASSWORD;
			     else *flags &= ~FLAGS_CHANNEL_PASSWORD;
                         } else buf[uf++] = cu;
                        break;
              case 'D': if (KeyFlags(sptr, FLAGS_DISTRIBUTE)
                            || op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_DISTRIBUTE;
                             else *flags &= ~FLAGS_DISTRIBUTE;
                         } else buf[uf++] = cu;
                        break;
              case 'B': if (KeyFlags(sptr,FLAGS_FIND_CORRECT_DEST_SEND_ONCE)
                            || op || mode == 'd' || !on) {
                            if (on) 
                               *flags |= FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                             else *flags &= ~FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
                         } else buf[uf++] = cu;
                        break;
              case 'K': if (op || mode == 'd' || !on) {
                            if (on) *flags |= FLAGS_KEY_TO_OPEN_OPER_LOCKS;
                             else *flags &= ~FLAGS_KEY_TO_OPEN_OPER_LOCKS;
                         } else buf[uf++] = cu;
                        break;
              case 'O': if (mode == 'd') {
                           if (on) *flags |= FLAGS_WASOPER;
                            else *flags &= ~FLAGS_WASOPER;
       		         } else buf[uf++] = cu;
                         break;
              case 'G': if (mode == 'd') {
                           if (on) *flags |= FLAGS_SERVER_GENERATED;
                             else *flags &= ~FLAGS_SERVER_GENERATED;
		         } else buf[uf++] = cu;
                        break;
              case 'H': if (mode == 'd') {
                           if (on) 
                               *flags |= FLAGS_SERVER_GENERATED_DESTINATION;
                            else *flags &= ~FLAGS_SERVER_GENERATED_DESTINATION;
                          } else buf[uf++] = cu;
                        break;
              case 'Z': if (KeyFlags(sptr,FLAGS_DENY)
                            || IsOper(sptr) || mode == 'd' || !on) {
                             if (on) *flags |= FLAGS_DENY;
			      else *flags &= ~FLAGS_DENY;
                         } else buf[uf++] = cu;
                         break;  
              default:  buf[uf++] = cu;
        } 
  }
 buf[uf] = 0;
 if (uf) {
     sendto_one(sptr,"NOTICE %s :#?# Unknown flag%s: %s %s",sptr->name,
                uf> 1  ? "s" : "",buf,type);
     return 0;
  }
 if (mode == 's') {
    if (*flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS)
        sendto_one(sptr,"NOTICE %s :### %s", sptr->name,
                   "WARNING: Recipient got keys to unlock the secret portal;");
     else if (*flags & FLAGS_DISTRIBUTE && *flags & FLAGS_CHANNEL)
              sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                         "WARNING: Channel is distributed to other servers;");
     else if (*flags & FLAGS_DISTRIBUTE && *flags & FLAGS_DENY)
              sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                         "WARNING: Deny is distributed to other servers;");
     else if (*flags & FLAGS_DENY)
              sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                         "WARNING: Channel and note deny set;");
     else if (*flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE
                && *flags & FLAGS_REPEAT_UNTIL_TIMEOUT
                && send_flag(*flags)) 
                sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                          "WARNING: Broadcast message in action;");
 }
 return 1;
}

static void split(string, nick, name, host)
char *string, *nick, *name, *host;
{
 char *np = string, *fill;

 *nick = 0; *name = 0; *host = 0;
 fill = nick;
 while (*np) { 
        *fill = *np;
        if (*np == '!') { 
           *fill = 0; fill = name;
	 } else if (*np == '@') { 
                    *fill = 0; fill = host;
 	         } else fill++;
        np++;
   } 
 *fill = 0;       
 if (!*nick) { *nick = '*'; nick[1] = 0; } 
 if (!*name) { *name = '*'; name[1] = 0; } 
 if (!*host) { *host = '*'; host[1] = 0; } 
}

static void garbage_collector()
{
 long32 gflags = 0;
 aMsgClient *msgclient;
 char *c, mbuf[MSG_LEN], dibuf[40];
 static int t = 1, count;

 if (!file_inited) return;
 gflags |= FLAGS_WASOPER; gflags |= FLAGS_SORT_BY_TONAME;
 gflags |= FLAGS_SERVER_GENERATED;
 
 for (count = 0; count < MIN_DELAY; count++) {

     if (!fromname_index) return;
     if (t > fromname_index) t = 1;
     
     msgclient = FromNameList[t];
     if (timeofday > msgclient->timeout) {
	if (msgclient->timeout
	    && !(msgclient->flags & FLAGS_CHANNEL)
	    && !(msgclient->flags & FLAGS_SERVER_GENERATED)
	    && !(msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)) {
	    display_flags(msgclient->flags, dibuf, 'q');
	    sprintf(mbuf,"Expired: /Note User -%d %s %s!%s@%s %s", 
		    (int)((msgclient->timeout - msgclient->time)/3600),
		    dibuf, msgclient->tonick, msgclient->toname, 
		    msgclient->tohost, Message(msgclient));
	    c = wild_fromnick(msgclient->fromnick, msgclient);
	    new(msgclient->passwd,"SERVER_EXPIRED","-","-",
	        c ? c : msgclient->fromnick, msgclient->fromname,
	        local_host(msgclient->fromhost), gflags,
	        SECONDS_WEEK+timeofday, timeofday, mbuf);
	  }
	remove_msg(msgclient); continue;
     }  
     t++;
   }
}

void save_messages()
{
 aMsgClient *msgclient;
 FILE *fp,*fopen();
 static time_t t2 = MAX_DAYS_NO_USE_SPY*SECONDS_DAY; 
 int t = 1;
 char mbuf[MSG_LEN];

 if (!file_inited || !changes_to_save) return;
 saved_clock = timeofday;
 while (fromname_index && t <= fromname_index) {
        msgclient = FromNameList[t];
        if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER) {
            *mbuf = '\0';
            while (!*mbuf) {
                   strcpy(mbuf, get_msg(msgclient, '2'));
                   if (!*mbuf) update_spymsg(msgclient);
              }
            if (*get_msg(msgclient, 'n') == '-') {
              if (timeofday > msgclient->time + t2)
                  msgclient->timeout = 0; 
            } else {
                     if (timeofday > msgclient->time + t2 + atoi(mbuf))
                         msgclient->timeout = 0;
                } 
        }       
       t++;
   }
 fp = fopen(note_save_filename_tmp,"w");
 if (!fp) {
    sendto_flag(SCH_ERROR, "Can't open for write: %s", NPATH);
    return;
 }
 t = 1;
 w_code(ltoa((long32)note_msm),fp);
 w_code(ltoa((long32)note_mum),fp);
 w_code(ltoa((long32)note_msw),fp);
 w_code(ltoa((long32)note_muw),fp);
 w_code(ltoa((long32)note_mst),fp);
 w_code(ltoa((long32)note_msf),fp);

 while (fromname_index && t <= fromname_index) {
     msgclient = FromNameList[t];
     w_code(msgclient->passwd,fp),w_code(msgclient->fromnick,fp);
     w_code(msgclient->fromname,fp);w_code(msgclient->fromhost,fp);
     w_code(msgclient->tonick,fp),w_code(msgclient->toname,fp);
     w_code(msgclient->tohost,fp),
     w_code(ltoa(msgclient->flags),fp);
     w_code(ltoa(msgclient->timeout),fp);
     w_code(ltoa(msgclient->time),fp);
     w_code(msgclient->message,fp);
     t++;
  }
 w_code("",fp);
 fclose(fp);
 fp = fopen(note_save_filename_tmp,"r");
 if (!fp || getc(fp) == EOF) {
    sendto_flag(SCH_ERROR, "Error writing: %s", note_save_filename_tmp);
    if (fp) fclose(fp); return; 
  }
 fclose (fp); changes_to_save = 0;
 unlink(NPATH);link(note_save_filename_tmp,NPATH);
 unlink(note_save_filename_tmp);
 chmod(NPATH, 432);
}

static char *flag_send(aptr, sptr, qptr, nick, msgclient, mode, chn)
aClient *aptr, *sptr, *qptr;
char *nick, mode, *chn;
aMsgClient *msgclient;
{
 int t, t1, exception;
 static char ebuf[BUF_LEN];
 char *c, *message;

 message = Message(msgclient);

 if (MyConnect(sptr)) msgclient->flags |= FLAGS_ON_THIS_SERVER;
     else if (mode != 'e' && mode != 'q') 
              msgclient->flags &= ~FLAGS_ON_THIS_SERVER;
 if (!(msgclient->flags & FLAGS_ON_THIS_SERVER) 
       && msgclient->flags & FLAGS_SEND_ONLY_IF_THIS_SERVER
     || (msgclient->flags & FLAGS_SEND_ONLY_IF_NICK_NOT_NAME 
         && mode == 'v' && StrEq(nick,UserName(sptr)))
     || (!IsOper(sptr) &&
         msgclient->flags & FLAGS_SEND_ONLY_IF_DESTINATION_OPER)
     || (mode == 'v' || mode == 'j' || mode == 'l') && qptr == aptr) 
        return NULLCHAR;

 if (msgclient->flags & FLAGS_SEND_ONLY_IF_NOT_EXCEPTION) {
     c = message;
     for(;;) {
         exception = 0;t = t1 = 0 ;ebuf[0] = 0;
         while (*c != '!') {
                if (!*c || *c == ' ' || t > BUF_LEN) return message;
                ebuf[t++] = *c++;
           }
         if (!*c++) return message;
         t1 += t;ebuf[t] = 0;
         if (!matches(ebuf,nick)) exception = 1;
         t=0;ebuf[0] = 0;            
         while (*c != '@') {
                if (!*c || *c == ' ' || t > BUF_LEN) return message;
                ebuf[t++] = *c++;
           }
         if (!*c++) return message;
         t1 += t;ebuf[t] = 0;
         if (matches(ebuf,UserName(sptr))) exception = 0;
         t=0;ebuf[0] = 0;
         if (*c == ' ') return message;
         while (*c && *c != ' ') {
              if (t > BUF_LEN) return message;
              ebuf[t++] = *c++;
           } 
         if (*c) c++; t1 += t;ebuf[t] = 0; message += t1+2;
         if (*c) message++;
         if (exception && !matches(ebuf,sptr->user->host)) return NULLCHAR;
     }
   }
 return message;
}

static char *check_flags(aptr, sptr, qptr, nick, newnick, qptr_nick,
                         msgclient, repeat, gnew, mode, sptr_chn)
aClient *aptr, *sptr, *qptr;
aMsgClient *msgclient;
char *nick, *newnick, *qptr_nick, mode;
int *repeat, *gnew;
aChannel *sptr_chn;
{
 char *c, mbuf[MSG_LEN], buf[BUF_LEN], ebuf[BUF_LEN], *message, 
      wmode[2], *spy_channel = NULLCHAR, *spy_server = NULLCHAR;
 long32 gflags;
 int t, t1, t2, last, secret = 1, send = 1, right_tonick = 0, 
     show_channel = 0, sptr_chn_exits = 0;
 aMsgClient *fmsgclient;
 aChannel *chptr;
 Link *link;

 if (mode != 'g') {
    if (!(msgclient->flags & FLAGS_FROM_REG)) qptr = 0;
     else for (qptr = client; qptr; qptr = qptr->next)
            if (qptr->user && !strcmp(UserName(qptr),msgclient->fromname)
                && (!mycmp(qptr->name,msgclient->fromnick)
                || wild_fromnick(qptr->name, msgclient))
                && host_check(qptr->user->host,msgclient->fromhost)) 
                break; 
    if (!qptr) msgclient->flags &= ~FLAGS_FROM_REG;
  }
 if (!mycmp(nick, msgclient->tonick)
     || !mycmp(nick, get_msg(msgclient, 'n'))) right_tonick = 1;
 if (!sptr->user->channel && !IsInvisible(sptr)) secret = 0;
 if (secret && !IsInvisible(sptr)) 
  for (link = sptr->user->channel; link; link = link->next)
       if (!SecretChannel(link->value.chptr)) secret = 0;
 if ((mode == 'a' || mode == 'v')
     && (*sptr->name == '_' || *sptr->info == '_')) secret = 1;
 wmode[1] = 0; *repeat = 0; *gnew = 0; 

 if (!send_flag(msgclient->flags)) send = 0; 
 if (mode == 'a' || mode == 'v'
     || mode == 'c' && !wildcards(msgclient->tonick)
     || mode == 'g' && (!wild_fromnick(qptr->name, msgclient)
                        || StrEq(qptr_nick, qptr->name))) {
     msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED; 
     msgclient->flags &= ~FLAGS_PRIVATE_DISPLAYED;
  }
 message = flag_send(aptr, sptr, qptr, nick, msgclient, mode, sptr_chn);
 if (!message
     || (msgclient->flags & FLAGS_SERVER_GENERATED) && 
        (mode != 'a' && mode != 'v')
     || msgclient->flags & FLAGS_SEND_ONLY_IF_SENDER_ON_ICN && !qptr) { 
    *repeat = 1; return NULLCHAR; 
  }
 if ((!secret || right_tonick) 
     && msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER) {
     mbuf[0] = 0; buf[0] = SPY_CTRLCHAR; buf[1] = 0;
     strcat(mbuf, clean_spychar(nick)); strcat(mbuf, buf);
     strcat(mbuf, clean_spychar(UserName(sptr))); strcat(mbuf, buf);
     strcat(mbuf, clean_spychar(sptr->user->host)); strcat(mbuf, buf);
     strcat(mbuf, clean_spychar(sptr->info)); strcat(mbuf, buf);
     strcat(mbuf, myitoa(timeofday-msgclient->time)); strcat(mbuf, buf);
     if (*get_msg(msgclient, '2'))
        strcat(mbuf, get_msg(msgclient, '2')); 
      else strcat(mbuf, myitoa(timeofday-msgclient->time));
     strcat(mbuf, buf);
     t = MSG_LEN - strlen(mbuf) - 10;
     strncat(mbuf, clean_spychar(Message(msgclient)), t);
     strcat(mbuf, "\0");
     DupNewString(msgclient->message, mbuf);
     message = flag_send(aptr, sptr, qptr, nick, msgclient, mode, sptr_chn);
     changes_to_save = 1;
   }
 if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER
     && qptr && qptr != sptr) {
     t = first_fnl_indexnode(UserName(qptr));
     last = last_fnl_indexnode(UserName(qptr));
     while (last && t <= last) {
            fmsgclient = FromNameList[t];
            if (fmsgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER
                && timeofday < fmsgclient->timeout
                && fmsgclient != msgclient
                && (!mycmp(mode == 'g' ? qptr_nick : qptr->name, 
                    fmsgclient->fromnick)
                   || wild_fromnick(mode == 'g' ? qptr_nick :qptr->name,
                                    fmsgclient))
                && !Usermycmp(UserName(qptr), fmsgclient->fromname)
                && host_check(qptr->user->host, fmsgclient->fromhost)
                && !matches(fmsgclient->tonick, nick)
                && !matches(fmsgclient->toname, UserName(sptr))
                && !matches(fmsgclient->tohost, sptr->user->host)
                && flag_send(aptr, sptr, qptr, nick, fmsgclient, 
                             mode, sptr_chn)) {
                t1 = wildcards(fmsgclient->tonick) ? 1 : 0;
                t2 = wildcards(msgclient->tonick) ? 1 : 0;
                if (!t1 && t2) goto end_this_flag;
                if (t1 && !t2) { t++; continue; }
                t1 = (fmsgclient->flags & FLAGS_SORT_BY_TONAME) ? 1 : 0;
                t2 = (msgclient->flags & FLAGS_SORT_BY_TONAME) ? 1 : 0;
                if (t1 && !t2) goto end_this_flag;
                if (!t1 && t2) { t++; continue; }
                if (fnl_msgclient(fmsgclient) < fnl_msgclient(msgclient))
                    goto end_this_flag;
	      }
            t++;
       }
         mbuf[0] = 0;
         for (link = sptr->user->channel; link; link = link->next) {
              chptr = link->value.chptr;
              if (sptr_chn == chptr) sptr_chn_exits = 1;
              if (IsMember(qptr, chptr)) {
                  msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                  if (mode != 'j' && mode != 'l') goto end_this_flag;
	       } else if (ShowChannel(qptr, chptr)) show_channel = 1;
              if (ShowChannel(qptr, chptr)) {
                  if (strlen(mbuf)+strlen(chptr->chname) >= MSG_LEN) continue;
                  if (mbuf[0]) strcat(mbuf, " ");
	          strcat(mbuf, chptr->chname);
	       }       
	    }
       for (link = qptr->user->channel; link; link = link->next)
            if (link->value.chptr == sptr_chn) break;
       if (link || mbuf[0] && sptr_chn_exits && !PubChannel(sptr_chn)) 
           mbuf[0] = 0;
        else if (!show_channel) {
               if (msgclient->flags & FLAGS_PRIVATE_DISPLAYED) mbuf[0] = 0;
                else {
                       strcpy(mbuf, "*Private*");
                       msgclient->flags |= FLAGS_PRIVATE_DISPLAYED;
	          }
             } else if (mbuf[0]) msgclient->flags &= ~FLAGS_PRIVATE_DISPLAYED; 

      if (msgclient->flags & FLAGS_DISPLAY_CHANNEL_DEST_REGISTER
          && mode != 'v' && mbuf[0]) spy_channel = mbuf;
      if (msgclient->flags & FLAGS_DISPLAY_SERVER_DEST_REGISTER)
          spy_server = sptr->user->server;
      if (spy_channel || spy_server)
          sprintf(ebuf,"%s%s%s%s%s", spy_channel ? " " : "",
                  spy_channel ? spy_channel : "",
                  spy_server ? " (" : "",
                  spy_server ? spy_server : "",
                  spy_server ? ")" : "");
        else ebuf[0] = 0;
      while (*message == '%') {
            while (*message && *message != ' ') message++;
            if (*message) message++;
	}
      sprintf(buf,"%s@%s",UserName(sptr),sptr->user->host);
      switch (mode) {
        case 'm' :
        case 'l' :
 	case 'j' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                       && !(only_wildcards(msgclient->tonick)
                       && only_wildcards(msgclient->toname))
                       && !secret && !right_tonick && !spy_channel) {
                       sendto_one(qptr, "NOTICE %s :### %s (%s) %s%s %s",
                                  qptr->name, nick, buf, "signs on",
                                  ebuf, message);
           	       msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
		    } else if (spy_channel && 
                               (!secret || right_tonick)) {
                               sendto_one(qptr,
                                          "NOTICE %s :### %s (%s) is on %s",
                                          qptr->name, nick, buf, spy_channel);
                               msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
			}
                    break;
        case 'v' :
  	case 'a' : if (!secret || right_tonick) {
                       sendto_one(qptr,
                                  "NOTICE %s :### %s (%s) %s%s %s",
                                  qptr->name, nick, buf, 
                                  "signs on", ebuf, message);
                       msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
		    }
                   break;
	case 'c' : if (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                       && !(only_wildcards(msgclient->tonick)
                            && only_wildcards(msgclient->toname))
                       && (!secret || right_tonick)) {
                       sendto_one(qptr,
                                  "NOTICE %s :### %s (%s) is %s%s %s",
	     			   qptr->name, nick, buf,
		       		   spy_channel ? "on" : "here",
                                   ebuf,message);
                        msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                    }
                   break;
        case 's' :
	case 'g' : if ((!secret || right_tonick) &&
                      (!(msgclient->flags & FLAGS_NEWNICK_DISPLAYED)
                        && !(only_wildcards(msgclient->tonick)
                             && only_wildcards(msgclient->toname)))) {
                       sendto_one(qptr,
                                  "NOTICE %s :### %s (%s) is %s%s %s",
	     	                  qptr->name, nick, buf,
                                  spy_channel ? "on" : "on IRC now",
                                  ebuf,message);
                       msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
                    }
                   break;
        case 'q' :
        case 'e' : if (!secret || right_tonick)
                       sendto_one(qptr,
		  	          "NOTICE %s :### %s (%s) %s %s",
				  qptr->name, nick,
			          buf,"signs off", message);
                       break;
	case 'n' : msgclient->flags &= ~FLAGS_NEWNICK_DISPLAYED;
                   if (secret) { 
                       if (right_tonick) 
                           sendto_one(qptr,
                                      "NOTICE %s :### %s (%s) %s %s",
                                      qptr->name, nick, buf,
                                      "signs off", message);
	            } else {
                         if (mycmp(nick, newnick)
                             && !(msgclient->flags & FLAGS_NO_NICKCHANGE_SPY))
                            sendto_one(qptr,
                                       "NOTICE %s :### %s (%s) %s <%s> %s",
                                       qptr->name, nick, buf, 
                                       "changed name to", newnick, message);
                            if (!matches(msgclient->tonick,newnick))
                                msgclient->flags |= FLAGS_NEWNICK_DISPLAYED;
	                 }

      }
        end_this_flag:;
 }

 if (send && !right_tonick && secret
     && !(msgclient->flags & FLAGS_SERVER_GENERATED)
     && !(msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE)) {
     if (mode == 'v') nick = msgclient->tonick;
      else { 
             *repeat = 1; return NULLCHAR;
	   }
 }
 if (mode == 'q' || mode == 'e' || mode == 'n') { 
     *repeat = 1; return NULLCHAR;
  }
 while (mode != 'g' && (msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION)){
        if (*message && matches(message, sptr->info) || 
            secret && !right_tonick) {
           *repeat = 1; break;
	 }
        sprintf(mbuf,"Search for %s!%s@%s (%s): %s!%s@%s (%s)",
                msgclient->tonick,msgclient->toname,msgclient->tohost,
                *message ? message : "*", nick,
                UserName(sptr),sptr->user->host,sptr->info);
        t1 = 0; 
        t = first_tnl_indexnode(msgclient->fromname);
        last = last_tnl_indexnode(msgclient->fromname);
        while (last && t <= last) {
              if (ToNameList[t]->timeout > timeofday &&
                  ToNameList[t]->flags & FLAGS_SERVER_GENERATED &&
                  !Usermycmp(ToNameList[t]->toname, msgclient->fromname) &&
                  !mycmp(ToNameList[t]->tohost,
                         local_host(msgclient->fromhost))) {
                  t1++;
                  if (!mycmp(Message(ToNameList[t]),mbuf)) {
                     t1 = -1; break;
		   }
	       }	  
              t++;
          }  
        if (t1 < 0) break;
        if (t1 > 10) {
            msgclient->timeout = timeofday-1; 
            break;
	 }
        gflags = 0;
        gflags |= FLAGS_WASOPER;
        gflags |= FLAGS_SORT_BY_TONAME;
        gflags |= FLAGS_SERVER_GENERATED;
        *gnew = 1;
        c = wild_fromnick(msgclient->fromnick, msgclient);
        new(msgclient->passwd,"SERVER_FIND","-","-",
            c ? c : msgclient->fromnick, msgclient->fromname,
            local_host(msgclient->fromhost), gflags, 
            SECONDS_WEEK+timeofday, timeofday, mbuf);
        break;
  }
if (msgclient->flags & FLAGS_REPEAT_UNTIL_TIMEOUT) *repeat = 1;
 
while (send && qptr != sptr &&
        (msgclient->flags & FLAGS_NOTICE_RECEIVED_MESSAGE)) {
        if (!right_tonick && secret && timeofday-msgclient->time < SECONDS_WEEK)
	   { *repeat = 1; send = 0; break; }
        sprintf(buf,"%s (%s@%s) has received note queued %s before delivery.",
                nick, UserName(sptr), sptr->user->host,
                mytime(msgclient->time));
        if (qptr && (right_tonick || !secret)) {
           sendto_one(qptr,"NOTICE %s :### %s", qptr->name, buf);
           break;
         }
       gflags = 0;
       gflags |= FLAGS_WASOPER;
       gflags |= FLAGS_SERVER_GENERATED;
       *gnew = 1;
       c = wild_fromnick(msgclient->fromnick, msgclient);
       new(msgclient->passwd,"SERVER_RECEIVED","-","-",
           c ? c : msgclient->fromnick, msgclient->fromname,
           local_host(msgclient->fromhost), gflags,
	   note_mst*SECONDS_DAY+timeofday, timeofday,buf);
       break;
    }
 while (send && msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE) {
        if (mode == 'g') {
            *repeat = 1; send = 0; break;
	  }
        t = first_tnl_indexnode(UserName(sptr));
        last = last_tnl_indexnode(UserName(sptr));
        while (last && t <= last) {
             if (ToNameList[t]->flags & FLAGS_SERVER_GENERATED_DESTINATION
                 && (!mycmp(ToNameList[t]->fromnick,msgclient->fromnick)
                     || wild_fromnick(ToNameList[t]->fromnick, msgclient))
                 && !Usermycmp(ToNameList[t]->fromname,msgclient->fromname)
                 && host_check(ToNameList[t]->fromhost,msgclient->fromhost)
                 && (!(msgclient->flags & FLAGS_REGISTER_NEWNICK) 
                     || !mycmp(ToNameList[t]->tonick,nick))
                 && !Usermycmp(ToNameList[t]->toname,UserName(sptr))
                 && host_check(ToNameList[t]->tohost,sptr->user->host)) {
                 send = 0; break;
             }
             t++;
	  }
        if (!send) break;
        gflags = 0;
        gflags |= FLAGS_SORT_BY_TONAME;
        gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
        gflags |= FLAGS_SERVER_GENERATED_DESTINATION;
        if (msgclient->flags & FLAGS_ALL_NICK_VALID) 
         gflags |= FLAGS_ALL_NICK_VALID;
        if (msgclient->flags & FLAGS_NICK_AND_WILDCARD_VALID) 
         gflags |= FLAGS_NICK_AND_WILDCARD_VALID;
        if (msgclient->flags & FLAGS_WASOPER) gflags |= FLAGS_WASOPER;
        *gnew = 1;
        new(msgclient->passwd,msgclient->fromnick, msgclient->fromname,
            msgclient->fromhost,nick,UserName(sptr),
            sptr->user->host,gflags,msgclient->timeout,timeofday,"");
        break;
   }
  if (send) return message; 
   else return NULLCHAR;
}

static void check_messages(aptr, sptr, info, mode)
aClient *sptr, *aptr; /* aptr = who activated */
void *info;
char mode;
{
 aMsgClient *msgclient, **index_p;
 char *qptr_nick, dibuf[40], *newnick, *message, *tonick;
 int last, first_tnl = 0, last_tnl = 0, first_tnil, last_tnil, nick_list = 1,
     number_matched = 0, t, repeat, gnew;
 long32 flags;
 aClient *qptr = sptr; /* qptr points to active person queued a request */
 aChannel *sptr_chn = 0;

 if (!file_inited) return;
 if (timeofday > saved_clock+note_msf) save_messages();

 if (mode == 'm' || mode == 'j' || mode == 'l') {
     sptr_chn = (aChannel *)info; qptr_nick = sptr->name;
  } else qptr_nick = (char *)info;
 tonick = qptr_nick;
 if (!sptr->user || !*tonick) return;
 if (mode == 'a' && StrEq(sptr->info,"ICNnote")
     && StrEq(sptr->name, sptr->info)) {
         sendto_one(sptr,":%s NOTICE %s : <%s> %s (%d) %s",
                    me.name, tonick, me.name, VERSION, number_fromname(),
                    StrEq(ptr,"ICNnote") ? "" : (!*ptr ? "-" : ptr));
      }
 if (!fromname_index) return;
 if (mode != 'j' && mode != 'l') {
     t = first_fnl_indexnode(UserName(sptr));
     last = last_fnl_indexnode(UserName(sptr));
     while (last && t <= last) {
           msgclient = FromNameList[t];
           if (!host_check(msgclient->fromhost, sptr->user->host)) {
               t++; continue;
            }
           if (!mycmp(qptr_nick, msgclient->fromnick)
               || wild_fromnick(qptr_nick, msgclient)) {
               if (msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER)
                   update_spymsg(msgclient);
               if ((mode == 'e' || mode == 'q')
                   && msgclient->flags & FLAGS_SIGNOFF_REMOVE)
                   msgclient->timeout = 0;
	    }
           msgclient->flags |= FLAGS_FROM_REG;
           t++; 
        }
   }
 if (mode == 'v') {
     nick_list = 0; /* Rest done with A flag */
  }
 if (mode == 'n') {
     newnick = tonick;tonick = sptr->name;
   }
 if (mode != 'g') {
     first_tnl = first_tnl_indexnode(UserName(sptr));
     last_tnl = last_tnl_indexnode(UserName(sptr));
     first_tnil = first_tnl_indexnode(tonick);
     last_tnil = last_tnl_indexnode(tonick);
  }
 if (mode == 's' || mode == 'g') {
     t = first_fnl_indexnode(UserName(sptr));
     last = last_fnl_indexnode(UserName(sptr));
     index_p = FromNameList;
     sptr = client; /* Notice new sptr */
     while (sptr && (!sptr->user || !*sptr->name)) sptr = sptr->next;
     if (!sptr) return;
     tonick = sptr->name;
  } else {
           if (nick_list) {
             t = first_tnil; last = last_tnil;
	    } else {
                     t = first_tnl; last = last_tnl;
		}
           index_p = ToNameList;
      }
 while(1) {
    while (last && t <= last) {
           msgclient = index_p[t];
           if (msgclient->timeout < timeofday) {
               t++; continue;
	    }
           gnew = 0; repeat=1;
           if (!(msgclient->flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS)
               && !(msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
               && (index_p != ToNameList || 
                  (!nick_list && msgclient->flags & FLAGS_SORT_BY_TONAME) ||
                  (nick_list && !(msgclient->flags & FLAGS_SORT_BY_TONAME)))
               && !matches(msgclient->tonick, tonick)
               && !matches(msgclient->toname, UserName(sptr))
               && !matches(msgclient->tohost, sptr->user->host)
               && (mode != 's' && mode != 'g'
                   || (wild_fromnick(qptr_nick, msgclient)
                       || !mycmp(qptr_nick, msgclient->fromnick))
                       && host_check(msgclient->fromhost, 
                                     qptr->user->host))) {
               message = check_flags(aptr, sptr, qptr, tonick, newnick,
                                     qptr_nick, msgclient, &repeat, 
                                     &gnew, mode, sptr_chn);
               if (message) {
                   flags = msgclient->flags;
                   display_flags(flags, dibuf, '-');
                   if (flags & FLAGS_SERVER_GENERATED)
                       sendto_one(sptr,"NOTICE %s :/%s/ %s",
                                  tonick, mytime(msgclient->time), message);
                   else sendto_one(sptr,
                                   "NOTICE %s :Note from %s!%s@%s /%s/ %s %s",
                                    tonick, msgclient->fromnick, 
                                    msgclient->fromname, msgclient->fromhost, 
                                    mytime(msgclient->time), dibuf, message);
       	        }
               if (!(msgclient->flags & FLAGS_SORT_BY_TONAME)) 
		  number_matched++;
	     }
           if (!repeat) msgclient->timeout = 0;
           if (gnew) {
               if (mode != 'g') {
                  first_tnl = first_tnl_indexnode(UserName(sptr));
                  last_tnl = last_tnl_indexnode(UserName(sptr));
                  first_tnil = first_tnl_indexnode(tonick);
                  last_tnil = last_tnl_indexnode(tonick);
		}
               if (mode == 's' || mode == 'g') {
                  t = fnl_msgclient(msgclient);
                  last = last_fnl_indexnode(UserName(qptr));
		} else {
                         if (index_p == ToNameList) {
                             t = tnl_msgclient(msgclient);
                             if (nick_list) last = last_tnil; 
                              else last = last_tnl;
      		          }
		    }
            } 
           if (repeat && (mode == 's' || mode =='g')) {
               sptr = sptr->next;
               while (sptr && (!sptr->user || !*sptr->name)) sptr = sptr->next;
                if (!sptr || number_matched) {
                    number_matched = 0; sptr = client; 
                    while (sptr && (!sptr->user || !*sptr->name))
                           sptr = sptr->next;
                    if (!sptr) return;
                    tonick = sptr->name; t++; continue;
		 }
               tonick = sptr->name;
            } else t++;
	 }
     if (mode == 's' || mode == 'g') return;
     if (index_p == ToNameList) {
         if (nick_list) {
             if (mode == 'a' || mode == 'q') break;
             nick_list = 0; t = first_tnl; last = last_tnl;
	  } else {
                  index_p = WildCardList;
                  t = 1; last = wildcard_index;
	     }
      } else {
               if (mode == 'n') {
                   mode = 'c'; tonick = newnick;
                   first_tnil = first_tnl_indexnode(tonick);
                   last_tnil = last_tnl_indexnode(tonick);
                   t = first_tnil; last = last_tnil;
                   nick_list = 1; index_p = ToNameList;
	        } else break;
	 }
  }
  if (mode == 'c' || mode == 'a') 
     check_messages(sptr, sptr, tonick, 'g');
}

static int isletter(c)
char c;
{
  if (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') return 1;
  return 0;
}

static char *create_pwd()
{
  static char pwd[BUF_LEN];
  char p;
  int t;

  for (t = 0; t < 8; t++) {
       do p = lrand48(); while (!isletter(p) && !isdigit(p));
       pwd[t] = p;
     }
  pwd[t] = 0;
  return pwd;
}

static int note_headchannel(chn)
char *chn;
{
  if (*chn != '#' || wildcards(chn) || index(chn, '-')) return 0;
  if (index(chn,'.')) return 1;
  return 0;
}

static int xrm_distribute_privs(fromname, fromhost)
char *fromname, *fromhost;
{
  return 1;
}

/* Format for param:
   matches.server.name CHANNEL 
   &pwd +flags -hours #chn.name!user@host fromnick!fromname@host <topic>
   matches.server.name DENY 
   &pwd +flags -hours nick!user@host fromnick!fromname@host <message>
*/

static void distribute(cptr, sptr, param)
aClient *cptr, *sptr;
char *param;
{
  aClient *acptr;
  int t, last, first_tnl = 0, last_tnl= 0, create_new = 1, root_access = 0,
      seconds, deny = 0, channel = 0, root_call = 1, nick_list = 1;
  char *name, toserver[BUF_LEN], message[MSG_LEN], tonick[BUF_LEN], 
       password_s[BUF_LEN], toname[BUF_LEN], flags[BUF_LEN], *password,
       tohost[BUF_LEN], timeout_s[BUF_LEN], fromnick[BUF_LEN], 
       fromname[BUF_LEN], fromhost[BUF_LEN], buf[BUF_LEN];
  aMsgClient *msgclient, **index_p;
  long32 gflags = 0;

  name = split_string(param, 2, 1);
  if (!mycmp(name, "CHANNEL")) channel = 1;
   else if (!mycmp(name, "DENY")) deny = 1;
    else return;
  strncpyzt(toserver, split_string(param, 1, 1), BUF_LEN-1);
  strncpyzt(password_s, split_string(param, 3, 1), BUF_LEN-1);
  if (*password_s != '&') return; 
  password = password_s + 1;
  strncpyzt(flags, split_string(param, 4, 1), 10);
  strncpyzt(timeout_s, split_string(param, 5, 1), 10);
  name = split_string(param, 6, 1); split(name, tonick, toname, tohost);
  strncpyzt(buf, split_string(param, 7, 1), BUF_LEN-1);
  split(buf, fromnick, fromname, fromhost);
  strncpyzt(message, split_string(param, 8, 0), BUF_LEN-1);

  if (channel && (!index(toserver, '.') || strlen(message) < 10 || 
      !note_headchannel(tonick))) return;

  if (!mycmp(me.name, sptr->name)
      || !xrm_distribute_privs(fromname, fromhost)) goto just_distribute;

  if (*timeout_s == '+') seconds = atoi(timeout_s + 1) * SECONDS_DAY;
     else if (*timeout_s == '-') seconds = atoi(timeout_s + 1) * 3600;

  if (index(flags, 'G')) root_call = 0;
  if (index(flags, 'D')) gflags |= FLAGS_DISTRIBUTE;
  if (index(flags, 'P')) gflags |= FLAGS_CHANNEL_PASSWORD;
  gflags |= FLAGS_WASOPER;
  if (channel) gflags |= FLAGS_CHANNEL;
  if (deny) gflags |= FLAGS_DENY;
  gflags |= FLAGS_SERVER_GENERATED;

  t = first_tnl_indexnode(tonick);
  last = last_tnl_indexnode(tonick);
  if (!channel) {
      first_tnl = first_tnl_indexnode(toname);
      last_tnl = last_tnl_indexnode(toname);
  }
  index_p = ToNameList;
  while (1) {   
     while (last && t <= last) {
           msgclient = index_p[t];
	   if (timeofday < msgclient->timeout
	       && (!channel || msgclient->flags & FLAGS_CHANNEL)
	       && (!deny || msgclient->flags & FLAGS_DENY)
	       && !mycmp(msgclient->tonick, tonick)
	       && (channel || !mycmp(msgclient->toname, toname))
	       && (channel || !mycmp(msgclient->tohost, tohost))) {
	       index_p = WildCardList; break;
	    }
	   t++;
	 }
     if (!channel && index_p == ToNameList) {
         if (nick_list) {
             nick_list = 0; t = first_tnl; last = last_tnl;
          } else {
                   index_p = WildCardList;
                   t = 1; last = wildcard_index;
              }
      } else break;
   }
  /* Don't change or queue anything if this server called distribute */
  if (t <= last && timeofday < msgclient->timeout) {
      /* If new request isn't equal, but matches old, remove old */
     if (!matches(toname, msgclient->toname) 
         && !matches(tohost, msgclient->tohost)
         && (mycmp(toname, msgclient->toname)
	     || mycmp(tohost, msgclient->tohost))) msgclient->timeout = 0;
      /* If old request matches new... */
     else if (!matches(msgclient->toname, toname) 
	      && !matches(msgclient->tohost, tohost)) {
              if (root_call &&
		  (!strcmp(password, msgclient->passwd)
		   || !mycmp(fromname, msgclient->fromname)
		      && host_check(fromhost, msgclient->fromhost)
		   || !*msgclient->passwd
		   || !msgclient->passwd[1])) root_access = 1;
              /* Remove requests we don't need. Notice that a root_call
	         removes other roots if password known or not valid */
	      if (!(msgclient->flags & FLAGS_SERVER_GENERATED)
		  && (root_access || !(msgclient->flags & FLAGS_DISTRIBUTE))
		  || (channel && strlen(Message(msgclient)) < 10))
		  msgclient->timeout = 0; 
	      else {
		 /* If it's distributing, and there is no root access:
		    If updated or a root (not server generated) - return */ 
	         if (!root_access && msgclient->flags & FLAGS_DISTRIBUTE
		    && (!(msgclient->flags & FLAGS_SERVER_GENERATED) || 
		       (timeofday - msgclient->time < SECONDS_SERVER_DISTRIBUTE)))
		   return;
		 /* If fromname has changed queue new, else just update */
		 if (strcmp(msgclient->fromname, fromname))
		     msgclient->timeout = 0;
		 else {
			create_new = 0;
		        msgclient->time = timeofday;
			if (msgclient->timeout < timeofday + seconds) 
			  msgclient->timeout = timeofday + seconds;
			if (gflags & FLAGS_CHANNEL_PASSWORD)
			  msgclient->flags |= FLAGS_CHANNEL_PASSWORD;
			else msgclient->flags &= ~FLAGS_CHANNEL_PASSWORD;
			if (strcmp(Message(msgclient), message))
			   DupNewString(msgclient->message, message);
			   DupNewString(msgclient->fromhost, fromhost);
			   DupNewString(msgclient->fromnick, fromnick);
		      }
	       } 
	    }
   }
  if (create_new) {
      new(password, fromnick, fromname, fromhost, tonick, toname, 
	  tohost, gflags, timeofday + seconds, timeofday, message);
    }
just_distribute:
  for (t = 0; t <= highest_fd; t++) {
      if (!(acptr = local[t])) continue;
      if (IsServer(acptr) && acptr != cptr
	  && (deny || !matches(toserver, acptr->name)))
          sendto_one(acptr, ":%s NOTE %s", sptr->name, param);
  }
}

static int number_distribute()
{
 register int t, nr = 0;

 timeofday = time(NULL);
 for (t = 1; t <= fromname_index; t++) 
      if (FromNameList[t]->timeout > timeofday
          && FromNameList[t]->flags & FLAGS_DISTRIBUTE) nr++;
 return nr;      
}   

static void msg_distribute(sptr, seconds)
aClient *sptr;
int seconds;
{

 if (!seconds) {
     sendto_one(sptr,"NOTICE %s :### Tell me for how many seconds",
		sptr->name);
     return;
  }  
 if (seconds > SECONDS_DAY) {
     sendto_one(sptr,"NOTICE %s :### Don't try fucin' up the net",
		sptr->name);
     return;
  }  
 if (IsOperHere(sptr)) {
    fast_distribute_timeout = timeofday + seconds;
    sendto_one(sptr,"NOTICE %s :### Distributing every second for %d sec.",
	       sptr->name, seconds);
  } else
      sendto_one(sptr,"NOTICE %s :### Don't even think about it...",
		 sptr->name);
}

static void request_distributor()
{
  aMsgClient *msgclient;
  static int t = 0, seconds_between_distr = 0;
  static time_t last_count_distribute = 0, last_call = 0;
  int delay, f, count, hours = 24*7;
  char buf[MSG_LEN+BUF_LEN], flags[BUF_LEN], *d_wildcards = "*.*", *toserver;

  if (timeofday - last_count_distribute > 1800) {
      last_count_distribute = timeofday; f = number_distribute();
      if (!f) return;
      seconds_between_distr = SECONDS_FOR_ALL_DISTRIBUTE/f;
      if (!seconds_between_distr) seconds_between_distr = 1;
   }
  delay = seconds_between_distr;
  if (timeofday < fast_distribute_timeout) delay = 1;
  if (!fromname_index || (timeofday - last_call < delay)) return;
  last_call = timeofday;
  for (count = 0; count < fromname_index/10 || count < 10; count++) {
       t++; if (t > fromname_index) t = 1;
       msgclient = FromNameList[t];
       if (timeofday < msgclient->timeout
          && (msgclient->flags & FLAGS_CHANNEL      
              || msgclient->flags & FLAGS_DENY)
	  && msgclient->flags & FLAGS_DISTRIBUTE) {
	  if (msgclient->tohost[0] == '*' && !msgclient->tohost[1])
	     toserver = d_wildcards;
	  else toserver = msgclient->tohost;
	  f = 0;
	  if (msgclient->flags & FLAGS_SERVER_GENERATED) {
	      if (timeofday - msgclient->time < SECONDS_SERVER_DISTRIBUTE) 
		  continue;
	       /* Sniff :'( Nobody has distributed me for a day. I'll 
		  go distribute myself if I live more than 1 hour */
	      hours = (msgclient->timeout - msgclient->time)/3600;
	      if (hours < 2) continue;
	      hours--; flags[f++] = 'G';
	   } 
	  flags[f++] = 'D';
	  if (msgclient->flags & FLAGS_CHANNEL_PASSWORD) flags[f++] = 'P';
	  flags[f] = 0;
	  sprintf(buf, "%s %s &%s +%s -%d %s!%s@%s %s!%s@%s %s", toserver, 
		  msgclient->flags & FLAGS_DENY ? "DENY" : "CHANNEL", 
		  msgclient->passwd, flags, hours, msgclient->tonick, 
		  msgclient->toname, msgclient->tohost, msgclient->fromnick,
		  msgclient->fromname, msgclient->fromhost, 
		  Message(msgclient));
	  if (!seconds_between_distr) last_count_distribute = 0;
	  distribute(me, me, buf); return;
	}
    }
}

void note_delay(delay)
time_t *delay;
{
 static time_t last_call = 0;

 timeofday = time(NULL);
 if (!file_inited) { 
     *delay = 0; init_messages(); 
     return; /* Don't do anything more before save file is read */
   }
 if (timeofday > saved_clock+note_msf) save_messages();
 if (*delay > MIN_DELAY) *delay = MIN_DELAY;
 request_distributor();
 if (timeofday - last_call < MIN_DELAY) return;
 last_call = timeofday; 
 garbage_collector();
}

void note_oper(sptr)
aClient *sptr;
{
  check_messages(sptr, sptr, sptr->name, 'm');
}

void note_signon(sptr)
aClient *sptr;
{
  check_messages(sptr, sptr, sptr->name, 'a'); /* nick on */
  check_messages(sptr, sptr, sptr->name, 'v'); /* !user and @host */
}

void note_signoff(sptr)
aClient *sptr;
{
  check_messages(sptr, sptr, sptr->name, 'e'); /* nick, !user and @host */
}

void note_leave(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
#ifdef ONLY_LOCAL_SPY_ON_CHANNEL
 if (!MyConnect(sptr)) return;
#endif
  check_messages(sptr, sptr, chptr, 'l');
}

void note_join(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
#ifdef ONLY_LOCAL_SPY_ON_CHANNEL
 if (*sptr->name != '_' && *sptr->info != '_' && !MyConnect(sptr)) return;
#endif
  check_messages(sptr, sptr, chptr, 'j');
}

void note_nickchange(sptr, newnick)
aClient *sptr;
char *newnick;
{
  check_messages(sptr, sptr, newnick, 'n');
}

static void remote_remove_notice(sptr, msgclient, message)
aClient *sptr;
aMsgClient *msgclient;
char *message;
{
 long32 gflags = 0;
 char buf[MSG_LEN], *c;

 if ((!mycmp(sptr->name, msgclient->fromnick)
      || wild_fromnick(sptr->name, msgclient))
      && !Usermycmp(UserName(sptr),msgclient->fromname)
      && host_check(sptr->user->host, msgclient->fromhost)) return;

 timeofday = time(NULL);
 sprintf(buf,"%s (%s@%s) %s for %s@%s", sptr->name, UserName(sptr),
	 sptr->user->host, message, msgclient->toname, msgclient->tohost);
 c = wild_fromnick(msgclient->fromnick, msgclient);
 gflags |= FLAGS_WASOPER;
 gflags |= FLAGS_SERVER_GENERATED;
 new(msgclient->passwd,"SERVER_REMOTE_RM","-","-",
     c ? c : msgclient->fromnick, msgclient->fromname,
     local_host(msgclient->fromhost), gflags, 
     note_mst*SECONDS_DAY+timeofday, timeofday, buf);
}

static void msg_remove(sptr, arg, passwd, flag_s, id_s, name, time_s)
aClient *sptr;
char *arg, *passwd, *flag_s, *id_s, *name, *time_s;
{
 aMsgClient *msgclient;
 int removed = 0, t, last, id, xrm = 0;
 time_t flags = 0, time_l;
 char dibuf[40], tonick[BUF_LEN], toname[BUF_LEN], tohost[BUF_LEN];

 if (!mycmp(arg,"XRM")) xrm = 1;
 if (xrm && (!IsOper(sptr) || 
	     !xrm_distribute_privs(sptr->user->username,
				   sptr->user->host))) {
     sendto_one(sptr,"NOTICE %s :#?# Privileged command", sptr->name); 
     return;
  }
 if (!set_flags(sptr,flag_s, &flags,'d',"")) return;
 if (!*time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(UserName(sptr));
 last = last_fnl_indexnode(UserName(sptr));
 if (xrm) { 
    t = 1; last = fromname_index;
 }
 timeofday = time(NULL);
 while (last && t <= last) {
       msgclient = FromNameList[t]; flags = msgclient->flags;
        if (timeofday > msgclient->timeout 
	    || xrm && !(msgclient->flags & FLAGS_CHANNEL) 
	               && !(msgclient->flags & FLAGS_DENY)
	    || xrm && msgclient->flags & FLAGS_CHANNEL_PASSWORD
	           && msgclient->flags & FLAGS_SERVER_GENERATED
	               && !IsOperHere(sptr)) {
	   t++; continue; 
	}
        set_flags(sptr, flag_s, &flags, 'd',"");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,time_l,id)) {
            display_flags(msgclient->flags, dibuf, '-'),
            sendto_one(sptr,"NOTICE %s :### Removed -> %s %s (%s@%s)",
                       sptr->name,dibuf,msgclient->tonick,
                       msgclient->toname,msgclient->tohost);
	    if (!MyConnect(sptr) && xrm &&
	        !(msgclient->flags & FLAGS_SERVER_GENERATED)) {
	        if (msgclient->flags & FLAGS_DISTRIBUTE &&
		    msgclient->flags & FLAGS_DENY) {
		    remote_remove_notice(sptr, msgclient, 
					 "has deactivated your Note Deny");
		    last = last_fnl_indexnode(UserName(sptr));
		  }
	 	 msgclient->flags &= ~FLAGS_DISTRIBUTE; 
		 removed++; t++; continue;
	     }
            msgclient->timeout = 0; removed++;
        } else t++;
   }
 if (!removed) 
  sendto_one(sptr,"NOTICE %s :#?# No such request(s) found", sptr->name);
}

static void msg_save(sptr)
aClient *sptr;
{
 if (!changes_to_save) {
     sendto_one(sptr,"NOTICE %s :### No changes to save",sptr->name);
     return;
 }
 if (!file_inited) {
     sendto_one(sptr,"NOTICE %s :### Busy reading save file. Check stats",
		sptr->name);
     return;
 }
 if (IsOperHere(sptr)) {
    save_messages();
    sendto_one(sptr,"NOTICE %s :### Requests are now saved",sptr->name);
  } else
      sendto_one(sptr,"NOTICE %s :### Save the fhiles...",sptr->name);
}

static void setvar(sptr,msg,l,value)
aClient *sptr;
int *msg,l;
char *value;
{
 int max;
 static char *message[] = {
             "Max server messages:",
             "I don't think that this is a good idea...",
             "Max server messages are set to:",
             "Max server messages with wildcards:",
             "Too many wildcards makes life too hard...",
             "Max server messages with wildcards are set to:",
             "Max user messages:",
             "Too cheeky fingers on keyboard error...",
             "Max user messages are set to:",
             "Max user messages with wildcards:",
             "Give me $$$, and I may fix your problem...",
             "Max user messages with wildcards are set to:",
             "Max server days:",
             "Can't remember that long time...",
             "Max server days are set to:",
             "Note save frequency:",
             "Save frequency may not be like that...",
             "Note save frequency is set to:" 
          };

 if (*value) {
    max = atoi(value);
    if (!IsOperHere(sptr))
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,message[l+1]);
     else { 
           if (!max && (msg == &note_mst || msg == &note_msf)) max = 1;
           *msg = max;if (msg == &note_msf) *msg *= 60;
           sendto_one(sptr,"NOTICE %s :### %s %d",sptr->name,message[l+2],max);
           changes_to_save = 1;
       }
 } else {
         max = *msg; if (msg == &note_msf) max /= 60;
         sendto_one(sptr,"NOTICE %s :### %s %d",sptr->name,message[l],max);
     }
}

static void msg_stats(sptr, arg, value)
aClient *sptr;
char *arg, *value;
{
 char buf[BUF_LEN], *fromhost = NULLCHAR, *fromnick, *fromname;
 int tonick_wildcards = 0,toname_wildcards = 0,tohost_wildcards = 0,any = 0,
     nicks = 0,names = 0,hosts = 0,t = 1,last = fromname_index,flag_notice = 0,
     flag_destination = 0, all_wildcards = 0;
 aMsgClient *msgclient;

 if (*arg) {
     if (!mycmp(arg,"MSM")) setvar(sptr,&note_msm,0,value); else 
     if (!mycmp(arg,"MSW")) setvar(sptr,&note_msw,3,value); else
     if (!mycmp(arg,"MUM")) setvar(sptr,&note_mum,6,value); else
     if (!mycmp(arg,"MUW")) setvar(sptr,&note_muw,9,value); else
     if (!mycmp(arg,"MST")) setvar(sptr,&note_mst,12,value); else
     if (!mycmp(arg,"MSF")) setvar(sptr,&note_msf,15,value); else
     if (MyEq(arg,"USED")) {
         while (last && t <= last) {
                msgclient = FromNameList[t];
                if (timeofday > msgclient->timeout) { t++; continue; }
                any++;
                if (msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)
                    flag_destination++; else
                if (msgclient->flags & FLAGS_SERVER_GENERATED)
                    flag_notice++; else
                if (IsOperHere(sptr) || Key(sptr))
                    if (!fromhost || 
                        !host_check(msgclient->fromhost,fromhost)) {
                        nicks++;names++;hosts++;
                        fromhost = msgclient->fromhost;
                        fromname = msgclient->fromname;
                        fromnick = msgclient->fromnick;
                     } else if (Usermycmp(msgclient->fromname,fromname)) {
                                nicks++;names++;
                                fromname = msgclient->fromname;
                                fromnick = msgclient->fromnick;
            	             } else if (mycmp(msgclient->fromnick,fromnick)) {
                                        nicks++;
                                        fromnick = msgclient->fromnick;
	  	                     } 
                if (wildcards(msgclient->tonick) && 
                    wildcards(msgclient->toname)) all_wildcards++;
                if (wildcards(msgclient->tonick))
                    tonick_wildcards++;
                if (wildcards(msgclient->toname))
                    toname_wildcards++;
                if (wildcards(msgclient->tohost))
                    tohost_wildcards++;
                t++;
	    }
	    if (!any) 
             sendto_one(sptr,"NOTICE %s :#?# No request(s) found",sptr->name);
                else {
                     if (IsOperHere(sptr) || Key(sptr)) {
                         sprintf(buf,"%s%d %s%d %s%d %s (%s%d %s%d %s%d %s%d)",
                                 "Nicks:",nicks,"Names:",names,
                                 "Hosts:",hosts,"W.cards",
                                 "Nicks:",tonick_wildcards,
                                 "Names:",toname_wildcards,
                                 "Hosts:",tohost_wildcards,
                                 "All:",all_wildcards);
                         sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
		      }
                      sprintf(buf,"%s %s%d / %s%d",
                              "Server generated",
                              "G-notice: ",flag_notice,
                              "H-header: ",flag_destination);
                      sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
		 }
     } else if (!mycmp(arg,"RESET")) {
                if (!IsOperHere(sptr)) 
                    sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                               "Wrong button - try another next time...");
                 else {
                       note_mst = NOTE_MAXSERVER_TIME,
                       note_mum = NOTE_MAXUSER_MESSAGES,
                       note_msm = NOTE_MAXSERVER_MESSAGES,
                       note_msw = NOTE_MAXSERVER_WILDCARDS,
                       note_muw = NOTE_MAXUSER_WILDCARDS;
                       note_msf = NOTE_SAVE_FREQUENCY*60;
                       sendto_one(sptr,"NOTICE %s :### %s",
                                  sptr->name,"Stats have been reset");
                       changes_to_save = 1;
		   }
	    }
  } else {
      t = number_fromname();
      sprintf(buf,"%s%d /%s%d /%s%d /%s%d /%s%d /%s%d /%s%d",
              "QUEUE:",t,
              "MSM:",note_msm,
              "MSW:",note_msw,
              "MUM:",note_mum,
              "MUW:",note_muw,
              "MST:",note_mst,
              "MSF:",note_msf/60);
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,buf);
    }
}

static int denied(sptr)
aClient *sptr;
{
 int last, t, first_tnl, last_tnl, nick_list = 1;
 aMsgClient *msgclient, **index_p;
 char *msg, buf[BUF_LEN];

 timeofday = time(NULL);
 t = first_tnl_indexnode(sptr->name);
 last = last_tnl_indexnode(sptr->name);
 first_tnl = first_tnl_indexnode(UserName(sptr));
 last_tnl = last_tnl_indexnode(UserName(sptr));
 index_p = ToNameList;
 while (1) {
  while (last && t <= last) {
         msgclient = index_p[t];
	 if (timeofday > msgclient->timeout) { t++; continue; }
	 msg = flag_send((aClient *)0, sptr, (aClient *)0 , sptr->name, 
			 msgclient, '-', NULLCHAR);
         if (msg && msgclient->flags & FLAGS_DENY
	     && (!only_wildcards(msgclient->tonick)
		 || !only_wildcards(msgclient->toname)
		 || !only_wildcards(msgclient->tohost))
	     && !(msgclient->flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS)
	     && !matches(msgclient->tonick,sptr->name) 
	     && !matches(msgclient->toname,UserName(sptr))
	     && !matches(msgclient->tohost,sptr->user->host)) {
	     sprintf(buf,"doesn't allow %s!%s@%s:", msgclient->tonick, 
		     msgclient->toname, msgclient->tohost);
	     sendto_one(sptr,"NOTICE %s :### %s (%s@%s) %s %s", sptr->name,
		        msgclient->fromnick, msgclient->fromname,
		        msgclient->fromhost, buf, msg);
	   return 1;
	 }
	 t++;
     }
     if (index_p == ToNameList) {
         if (nick_list) {
             nick_list = 0; t = first_tnl; last = last_tnl;
          } else {
                   index_p = WildCardList;
                   t = 1; last = wildcard_index;
              }

      } else break;
 }
 return 0;
}

static int check_channel(sptr, mode, name, toname, tohost)
aClient *sptr;
char mode, *name, *toname, *tohost;
{
  int t, last, found = 0;
  aMsgClient *msgclient;

  timeofday = time(NULL);

  if (wildcards(name) || mode == 'l') { 
      t = 1; last = toname_index;
   }
  else {
         t = first_tnl_indexnode(name);
         last = last_tnl_indexnode(name);
   }
  while (last && t <= last) {
         msgclient = ToNameList[t];
         if (msgclient->timeout < timeofday) {
             t++; continue;
          }
         if (msgclient->flags & FLAGS_CHANNEL
	    && note_headchannel(msgclient->tonick)
            && !(msgclient->flags & FLAGS_SORT_BY_TONAME)
            && (!matches(name, msgclient->tonick)
	       || MyEq(name, msgclient->tonick))
	    && (!*toname || !matches(msgclient->toname, toname))
            && (!*tohost || !matches(msgclient->tohost, tohost))
	    && !matches(msgclient->toname, UserName(sptr))
            && !matches(msgclient->tohost, sptr->user->host)) {
	    found++;
	    if (mode == 'c') return t;
             sendto_one(sptr,"NOTICE %s :*** %-30.29s : %.40s", sptr->name, 
                        msgclient->tonick, Message(msgclient));
          }
         t++;
    }
  return found;
}

static int note_channel(cptr, sptr, mode, name)
aClient *cptr, *sptr;
char mode, *name;
{
  char *c;
  aChannel *chptr;
  char buf[BUF_LEN], *wildcard = "*";
  int ret = 0, only_users = 0, only_head = 0;

  if (mode == 'l' && (MyEq("tail", name) || !*name)) only_users = 1;
  if (mode == 'l' && (MyEq("head", name) || !mycmp(name, "."))) {
      only_head = 1; name = wildcard;
    }
  if (!only_users) ret = check_channel(sptr, mode, name, "", "");
  if (!only_head && (only_users || mode == 'l'))
      for (chptr = channel; chptr; chptr = chptr->nextch) 
	if (index(chptr->chname,'.')) {
	  strncpyzt(buf, chptr->chname, BUF_LEN-5);
	  if (c = index(buf, '-')) *c = 0;
	  if ((!matches(name, chptr->chname)
	       || MyEq(name, chptr->chname)
	       || only_users)
	      && check_channel(sptr, 'c', buf, "", ""))
	    sendto_one(sptr,"NOTICE %s :*** %-30.29s > %.40s", 
		       sptr->name, chptr->chname, chptr->topic);
	}
  return ret;
}

static int msg_send(sptr, silent, passwd, flag_s, timeout_s, name, message)
aClient *sptr;
int silent;
char *passwd, *flag_s, *timeout_s, *name, *message;
{
 aMsgClient *msgclient;
 int sent_wild = 0, sent = 0, t, first, last, join = 0;
 time_t timeout;
 long32 flags = 0;
 char buf[BUF_LEN], dibuf[40], *empty_char = "", tonick[BUF_LEN], 
      toname[BUF_LEN], tohost[BUF_LEN];

 if (!file_inited) {
     sendto_one(sptr,"NOTICE %s :### Busy reading save file", sptr->name);
     return -1;
  }
 timeofday = time(NULL);
 if (denied(sptr)) return -1;
 if (number_fromname() >= note_msm 
     && !IsOperHere(sptr) && !Key(sptr)) {
     if (!note_msm || !note_mum)
         sendto_one(sptr,
                    "NOTICE %s :#?# The notesystem is closed for no-operators",
                    sptr->name);
      else sendto_one(sptr,"NOTICE %s :#?# No more than %d request%s %s",
                      sptr->name, note_msm, note_msm < 2 ? "" : "s",
                      "allowed in the server");
     return -1;
  }
 if (!set_flags(sptr,flag_s,&flags,'s',"")) return -1;
 split(name, tonick, toname, tohost);
 if (IsOper(sptr)) flags |= FLAGS_WASOPER;
 if (*timeout_s == '+') timeout = atoi(timeout_s + 1) * 24;
  else if (*timeout_s == '-') timeout = atoi(timeout_s + 1);
 if (timeout > note_mst*24 && !(flags & FLAGS_WASOPER) && !Key(sptr)) {
    sendto_one(sptr,"NOTICE %s :#?# Max time allowed is %d day%s",
               sptr->name,note_mst,note_mst > 1 ? "s" : "");
    return -1;
  }
 if (!message) {
    if (!send_flag(flags)) message = empty_char; 
     else {
           sendto_one(sptr,"NOTICE %s :#?# No message specified",sptr->name);
           return -1;
       }
  }
 if (HasPrefix(*toname)) {
     sendto_one(sptr,
                "NOTICE %s :#?# Please skip that first character in username",
                sptr->name);
     return -1;
  }
 first = first_fnl_indexnode(UserName(sptr));
 last = last_fnl_indexnode(UserName(sptr));
 t = first;
 while (last && t <= last) {
        msgclient = FromNameList[t];
        if (timeofday > msgclient->timeout) { t++; continue; }
        if (!mycmp(sptr->name, msgclient->fromnick)
            && !Usermycmp(UserName(sptr), msgclient->fromname)
            && host_check(sptr->user->host, msgclient->fromhost)
            && StrEq(msgclient->tonick, tonick)
            && StrEq(msgclient->toname, toname)
            && StrEq(msgclient->tohost, tohost)
            && StrEq(msgclient->passwd, passwd)
            && StrEq(Message(msgclient), clean_spychar(message))
            && msgclient->flags == (msgclient->flags | flags)) {
            msgclient->timeout = timeout*3600+timeofday;
            join = 1;
	  }
        t++;
    }
  if (!join && !(flags & FLAGS_WASOPER) && !Key(sptr)) {
     t = first;
     while (last && t <= last) {
         if (!Usermycmp(UserName(sptr),FromNameList[t]->fromname)) {
             if (host_check(sptr->user->host,FromNameList[t]->fromhost)) {
                 sent++;
                 if (wildcards(FromNameList[t]->tonick)
                     && wildcards(FromNameList[t]->toname))
                    sent_wild++;  
              }
                 
          }
         t++;
       }
     if (sent >= note_mum) {
        sendto_one(sptr,"NOTICE %s :#?# No more than %d request%s %s",
                   sptr->name,note_mum,note_mum < 2?"":"s",
                   "for each user allowed in the server");
        return -1;
      }
     while (wildcards(tonick) && wildcards(toname)) {
            if (!note_msw || !note_muw)
                sendto_one(sptr,
                          "NOTICE %s :#?# No-operators are not allowed %s",
                          sptr->name,
                          "to specify nick and username with wildcards");
            else if (wildcard_index >= note_msw) 
                     sendto_one(sptr,"NOTICE %s :#?# No more than %d req. %s",
                                sptr->name, note_msw,
                                "with nick and username w.cards allowed.");
            else if (sent_wild >= note_muw) 
                     sendto_one(sptr,"NOTICE %s :#?# No more than %d %s %s",
                                sptr->name, note_muw, 
                                note_muw < 2 ? "request ":" requests",
                                "with nick and username w.cards allowed.");
          else break;
          return -1;
     }
   }
 while ((send_flag(flags) || flags & FLAGS_DISPLAY_IF_DEST_REGISTER) &&
       wildcards(tonick) && wildcards(toname)) { 
       if ((flags & FLAGS_WASOPER || Key(sptr)) && !valid_elements(tohost))
          sendto_one(sptr, 
                     "NOTICE %s :#?# This matches more than one country.",
                     sptr->name);
        else if (!(flags & FLAGS_WASOPER) && !Key(sptr) &&
                 matches(local_host(sptr->user->host), tohost))
                 sendto_one(sptr, "NOTICE %s :#?# %s must be a local host.",
                            sptr->name, local_host(sptr->user->host));
          else break; 
       return -1;
  }
 if (flags & FLAGS_CHANNEL && *tonick != '#') {
     sendto_one(sptr, "NOTICE %s :#?# Channel names begin with #",
                sptr->name);
     return -1;
  }
 if (flags & FLAGS_CHANNEL && wildcards(tonick)) {
     sendto_one(sptr, "NOTICE %s :#?# No wildcards allowed in channel name",
                sptr->name);
     return -1;
  }
 if (note_headchannel(tonick)) {
     if (!(flags & FLAGS_CHANNEL)) {
         sendto_one(sptr, "NOTICE %s :#?# Channel flag missing",
		    sptr->name);
	 return -1;
       }
     if (check_channel(sptr, 'c', tonick, toname, tohost)) {
         if (MyConnect(sptr) && !silent)
	     sendto_one(sptr, "NOTICE %s :#?# Already registered channel: %s",
			sptr->name, tonick);
	     return -1;
      }
  }
 if (!join) {
     flags |= FLAGS_FROM_REG;
     msgclient = new(passwd,sptr->name, UserName(sptr),  
                     sptr->user->host, tonick, toname, tohost, flags, 
                     timeout*3600+timeofday, timeofday, clean_spychar(message));
     if (flags & FLAGS_DISPLAY_IF_DEST_REGISTER) update_spymsg(msgclient);
  }
 display_flags(flags, dibuf, '-');
 sprintf(buf, "%s %s %s!%s@%s for %s",
         join ? "Joined..." : "Queued...",dibuf, 
         tonick, toname, tohost, relative_time(timeout*3600));
 if (!silent) {
    sendto_one(sptr,"NOTICE %s :### %s",sptr->name, buf);
    if (send_flag(flags)) check_messages(sptr, sptr, sptr->name, 's');
  }
 if (join) return 0; else return 1;
}

static void msg_news(sptr, silent, passwd, flag_s, timeout_s, name, message)
aClient *sptr;
int silent;
char *passwd, *flag_s, *timeout_s, *name, *message;
{
 aMsgClient *msgclient;
 int joined = 0, queued = 0, ret, t = 1, msg_len;
 char *c, tonick[BUF_LEN], toname[BUF_LEN], tohost[BUF_LEN], 
      anyname[BUF_LEN], buf[MSG_LEN];

 split(name, tonick, toname, tohost);
 if (MyEq("ADMIN.", tonick) && !IsOper(sptr) && !KeyFlags(sptr, FLAGS_NEWS)) {
     sendto_one(sptr,
                "NOTICE %s :#?# No privileges for admin group.", 
                sptr->name);
     return;
  }
 timeofday = time(NULL);
 sprintf(buf, "[News:%s] ", tonick); msg_len = MSG_LEN-strlen(buf)-1;
 strncat(buf, message, msg_len); strcat(flag_s, "-RS");
 while (fromname_index && t <= fromname_index) {
        msgclient = FromNameList[t];
        if (!Usermycmp(UserName(sptr), msgclient->fromname)
            && (!mycmp(sptr->name, msgclient->fromnick)
                || wild_fromnick(sptr->name, msgclient)
            &&  host_check(sptr->user->host,msgclient->fromhost))) {
	    t++; continue;
	 }
        if (timeofday < msgclient->timeout
            && !(msgclient->flags & FLAGS_SERVER_GENERATED)
            && !(msgclient->flags & FLAGS_SERVER_GENERATED_DESTINATION)  
            && (msgclient->flags & FLAGS_NEWS
                && !matches(msgclient->tonick, tonick)
                && !matches(msgclient->toname, UserName(sptr))
                && !matches(msgclient->tohost, sptr->user->host)
                && (!matches(toname, msgclient->tonick)
                    || matches(toname, tonick)
                       && !mycmp(msgclient->tonick, tonick))      
                && (!*Message(msgclient)
                    || !matches(Message(msgclient), message))
                || !mycmp(tonick, "admin.users"))
            && !matches(tohost, msgclient->fromhost)
            && !(msgclient->flags & FLAGS_KEY_TO_OPEN_OPER_LOCKS)) {
            c = wild_fromnick(msgclient->fromnick, msgclient);            
            sprintf(anyname, "%s!%s@%s", 
                    c ? c : msgclient->fromnick, msgclient->fromname, 
                    local_host(msgclient->fromhost));
            ret = msg_send(sptr, 1, passwd, flag_s, 
                           timeout_s, anyname, buf);
            if (!ret) joined++;
             else if (ret < 0) return;
               else { 
                     queued++;
                     t = fnl_msgclient(msgclient);
		 }
	 }
       t++;
   }
 strcpy(buf, "user");
 if (queued > 1) strcat(buf, "s"); 
 if (joined) { 
    strcat(buf, ", ");
    strcat(buf, myitoa(joined)); strcat(buf, " joined");
  } 
 sendto_one(sptr,"NOTICE %s :### News to %d %s", sptr->name, queued, buf);
}

static void msg_list(sptr, arg, passwd, flag_s, id_s, name, time_s)
aClient *sptr;
char *arg, *passwd, *flag_s, *id_s, *name, *time_s;
{
 aMsgClient *msgclient;
 int number = 0, t, last, ls = 0, count = 0, 
     found = 0, log = 0, id, id_count = 0, channel = 0;
 time_t time_l, time_queued;
 long32 flags = 0;
 char tonick[BUF_LEN], toname[BUF_LEN], tohost[BUF_LEN],
      *message, buf[BUF_LEN], dibuf[40], mbuf[MSG_LEN], 
      *dots = "...", *wildcard = "*";

 if (MyEq(arg,"ls")) ls = 1; else
 if (MyEq(arg,"count")) count = 1; else
 if (MyEq(arg,"log")) log = 1; else
 if (MyEq(arg,"xls")) channel = 1; else
 if (MyEq(arg,"llog")) log = 3;
  else {
         sendto_one(sptr,"NOTICE %s :#?# No such option: %s",sptr->name,arg); 
         return;
    }
 if (channel && !IsOper(sptr)) {
         sendto_one(sptr,"NOTICE %s :#?# Priviliegied command",
		    sptr->name); 
         return;
  }
 if (!*name && !id_s && !*flag_s) {
              if (log) log++;
              if (ls) ls++;
              name = wildcard;
  }
 if (!set_flags(sptr,flag_s, &flags,'d',"")) return;
 if (!*time_s) time_l = 0; 
  else { 
        time_l = set_date(sptr,time_s);
        if (time_l < 0) return;
    }
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(UserName(sptr));
 last = last_fnl_indexnode(UserName(sptr));
 if (channel) { 
    t = 1; last = fromname_index;
 }
 while (last && t <= last) {
        msgclient = FromNameList[t];
	msgclient->id = ++id_count;
        flags = msgclient->flags;
        if (timeofday > msgclient->timeout || channel 
           && !(msgclient->flags & FLAGS_CHANNEL) 
           && !(msgclient->flags & FLAGS_DENY)) { 
           t++; continue; 
        }
        set_flags(sptr,flag_s,&flags,'d',"");
        if (local_check(sptr,msgclient,passwd,flags,
                           tonick,toname,tohost,time_l, id)) {
            message = Message(msgclient); number++;
            if (ls == 2 && *message) message = dots;
            display_flags(msgclient->flags, dibuf, '-');
            if (log) { 
                if (!(msgclient->flags & FLAGS_DISPLAY_IF_DEST_REGISTER))
                    { t++ ; continue; }
                strcpy(mbuf, get_msg(msgclient, 'n'));
                if (*mbuf == '-') *mbuf = '\0';
                if (*mbuf) { 
                   strcat(mbuf, "!");
                   strcat(mbuf, get_msg(msgclient, 'u')); strcat(mbuf, "@");
                   strcat(mbuf, get_msg(msgclient, 'h')); 
                   time_queued = atoi(get_msg(msgclient, '1'))+msgclient->time;
                 }
                if (log == 1 || log == 3) {            
                   found++;
                   if (*mbuf) {
                      strcat(mbuf, " (");
                      strcat(mbuf, get_msg(msgclient, 'r')); strcat(mbuf, ")");
                    } else {
                             time_queued = timeofday;
                             strcpy(mbuf, "<No matches yet>");
		        }
                   sendto_one(sptr,"NOTICE %s :### %s: %s!%s@%s => %s", 
                              sptr->name, log == 1 ? mytime(time_queued) :
                              myctime(time_queued),
                              msgclient->tonick, msgclient->toname, 
                              msgclient->tohost, mbuf); 
	         } else if (*mbuf) {
                            found++;
                            sendto_one(sptr,"NOTICE %s :### %s: %s", 
                                       sptr->name, 
                                       log == 2 ? mytime(time_queued) :
                                       myctime(time_queued), mbuf); 
		        }
	      } else 
                 if (!count) {
                    found++;
		    if (msgclient->flags & FLAGS_CHANNEL
			|| msgclient->flags & FLAGS_DENY)
		    sprintf(mbuf,"[%s!%s@%s] %s", msgclient->fromnick,
			    msgclient->fromname, msgclient->fromhost, dibuf);
		    else strcpy(mbuf,dibuf);
                    sprintf(buf,"for %s", 
                           relative_time(msgclient->timeout-timeofday));
                    sendto_one(sptr,"NOTICE %s :%d: %s %s (%s@%s) %s: %s",
                              sptr->name, msgclient->id, mbuf,
                              msgclient->tonick, msgclient->toname,
                              msgclient->tohost, buf, message);
		 }
	 }
       t++;
      }
 if (count) sendto_one(sptr,"NOTICE %s :### %s %s (%s@%s): %d",
                       sptr->name,"Number of requests to",
                       tonick ? tonick : "*", toname ? toname:"*",
                       tohost ? tohost : "*", number);
  else if (!found) sendto_one(sptr,"NOTICE %s :#?# No such %s", sptr->name,
                              log ? "log(s)" : "request(s) found");
}

static void msg_flag(sptr, passwd, flag_s, id_s, name, newflag_s)
aClient *sptr;
char *passwd, *flag_s, *id_s, *name, *newflag_s;
{
 aMsgClient *msgclient;
 int flagged = 0, t, last, id;
 long32 flags = 0;
 char tonick[BUF_LEN], toname[BUF_LEN], tohost[BUF_LEN],
      dibuf1[40], dibuf2[40];

 if (!*newflag_s) {
     sendto_one(sptr,"NOTICE %s :#?# No flag changes specified",sptr->name);
     return;
  }
 if (!set_flags(sptr, flag_s, &flags,'d',"in matches flag")) return;
 if (!set_flags(sptr, newflag_s, &flags,'c',"in flag changes")) return;
 split(name, tonick, toname, tohost);
 if (id_s) id = atoi(id_s); else id = 0;
 t = first_fnl_indexnode(UserName(sptr));
 last = last_fnl_indexnode(UserName(sptr));
 while (last && t <= last) {
       msgclient = FromNameList[t];flags = msgclient->flags;
        if (timeofday > msgclient->timeout) { t++; continue; }
        set_flags(sptr,flag_s,&flags,'d',"");
        if (local_check(sptr,msgclient,passwd,flags,
                        tonick,toname,tohost,0,id)) {
            flags = msgclient->flags; display_flags(flags, dibuf1, '-');
            set_flags(sptr,newflag_s,&msgclient->flags,'s',"");
            display_flags(msgclient->flags, dibuf2, '-');
            if (flags == msgclient->flags) 
                sendto_one(sptr,"NOTICE %s :### %s -> %s %s (%s@%s)",
                           sptr->name, "No flag change for",
                           dibuf1, msgclient->tonick,
                           msgclient->toname,msgclient->tohost);
             else                                        
                sendto_one(sptr,"NOTICE %s :### %s -> %s %s (%s@%s) to %s",
                          sptr->name,"Flag change",dibuf1,msgclient->tonick,
                          msgclient->toname, msgclient->tohost, dibuf2);
           flagged++;
        } 
       t++;
   }
 if (!flagged) 
  sendto_one(sptr,"NOTICE %s :#?# No such request(s) found",sptr->name);
}

static void msg_sent(sptr, arg, name, time_s, delete)
aClient *sptr;
char *arg, *name, *time_s, *delete;
{
 aMsgClient *msgclient,*next_msgclient;
 char fromnick[BUF_LEN], fromname[BUF_LEN], fromhost[BUF_LEN]; 
 int number = 0, t, t1, last, nick = 0, count = 0, users = 0;
 time_t time_l;

 if (!*arg) nick = 1; else
  if (MyEq(arg,"COUNT")) count = 1; else
   if (MyEq(arg,"USERS")) users = 1; else
   if (!MyEq(arg,"NAME")) {
       sendto_one(sptr,"NOTICE %s :#?# No such option: %s",sptr->name, arg); 
       return;
  }
 if (users) {
    if (!IsOperHere(sptr)) {
        sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                   "A dragon is guarding the names...");
        return;
     }
    if (!*time_s) time_l = 0; 
     else { 
            time_l = set_date(sptr,time_s);
            if (time_l < 0) return;
       }
    split(name, fromnick, fromname, fromhost); 
    for (t = 1; t <= fromname_index; t++) {
         msgclient = FromNameList[t]; t1 = t;
         do next_msgclient = t1 < fromname_index ? FromNameList[++t1] : 0;
          while (next_msgclient && next_msgclient->timeout <= timeofday);
         if (msgclient->timeout > timeofday
             && (!time_l || msgclient->time >= time_l 
                 && msgclient->time < time_l+SECONDS_DAY)
             && (!matches(fromnick,msgclient->fromnick))
             && (!matches(fromname,msgclient->fromname))
             && (!matches(fromhost,msgclient->fromhost))
             && (!*delete || !mycmp(delete,"RM")
                 || !mycmp(delete,"RMBF") &&
                    (msgclient->flags & FLAGS_RETURN_CORRECT_DESTINATION ||
                     msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE))) {
             if (*delete || !next_msgclient
                 || mycmp(next_msgclient->fromnick,msgclient->fromnick)
                 || mycmp(next_msgclient->fromname,msgclient->fromname)     
                 || !(host_check(next_msgclient->fromhost,
                                 msgclient->fromhost))) {
                  sendto_one(sptr,"NOTICE %s :### %s[%d] %s (%s@%s) @%s",
                             sptr->name,
                             *delete ? "Removing -> " : "", 
                             *delete ? 1 : count+1,
                             msgclient->fromnick,msgclient->fromname,
                             local_host(msgclient->fromhost),
                             msgclient->fromhost);
                    if (*delete) msgclient->timeout = timeofday-1;
                 count = 0; number = 1;
	     } else count++;
	 }
     }
    if (!number) 
     sendto_one(sptr, "NOTICE %s :#?# No request(s) from such user(s) found",
                sptr->name);
  return;
 }
 t = first_fnl_indexnode(UserName(sptr));
 last = last_fnl_indexnode(UserName(sptr));
 timeofday = time(NULL);
 while (last && t <= last) {
        msgclient = FromNameList[t];
        if (timeofday > msgclient->timeout) { t++; continue; }
        if (!Usermycmp(UserName(sptr),msgclient->fromname)
            && (!nick || !mycmp(sptr->name,msgclient->fromnick))) {
            if (host_check(sptr->user->host,msgclient->fromhost)) { 
                if (!count) 
                    sendto_one(sptr,"NOTICE %s :### Queued %s from host %s",
                               sptr->name, mytime(msgclient->time),
                               msgclient->fromhost);
                number++;
             }
         }
       t++;
    }
 if (!number) 
  sendto_one(sptr,"NOTICE %s :#?# No such request(s) found",sptr->name);
  else if (count) sendto_one(sptr,"NOTICE %s :### %s %d",sptr->name,
                             "Number of requests queued:",number);
}

static int name_len_error(sptr, name)
aClient *sptr;
char *name;
{
 if (strlen(name) >= BUF_LEN) {
    sendto_one(sptr,
               "NOTICE %s :#?# Nick!name@host can't be longer than %d chars",
               sptr->name, BUF_LEN-1);
    return 1;
  }
 return 0;
}

static int flag_len_error(sptr, flag_s)
aClient *sptr;
char *flag_s;
{
 if (strlen(flag_s) >= BUF_LEN) {
    sendto_one(sptr,"NOTICE %s :#?# Flag string can't be longer than %d chars",
               sptr->name,BUF_LEN-1);
    return 1;
  }
 return 0;
}

static int alias_send(sptr, option, flags, msg, timeout)
aClient *sptr;
char *option, **flags, **msg, **timeout;
{
 static char flag_s[BUF_LEN+10], *month = "+31", *year = "+365",
        *waitfor_message = "[Waiting]";
 
 if (MyEq(option,"SEND")) {
     sprintf(flag_s,"+N%s", *flags);
     if (!*timeout) *timeout = month; 
  } else
 if (MyEq(option,"NEWS")) {
     sprintf(flag_s,"+RS%s", *flags);
     if (!*timeout) *timeout = month; 
  } else
 if (MyEq(option,"CHANNEL")) {
     sprintf(flag_s,"+LR%s", *flags);
     if (!*timeout) *timeout = year; 
  } else
 if (MyEq(option,"WAITFOR")) { 
     sprintf(flag_s,"+YN%s", *flags); 
     if (!*msg) *msg = waitfor_message; 
  } else 
 if (MyEq(option,"SPY")) sprintf(flag_s,"+RX%s", *flags); else
 if (MyEq(option,"FIND")) sprintf(flag_s,"+FR%s", *flags); else
 if (MyEq(option,"KEY")) sprintf(flag_s,"+KR%s", *flags); else
 if (MyEq(option,"WALL")) sprintf(flag_s,"+BR%s", *flags); else
 if (MyEq(option,"WALLOPS")) sprintf(flag_s,"+BRW%s", *flags); else
 if (MyEq(option,"DENY")) sprintf(flag_s,"+RZ%s", *flags); else
  return 0;
 *flags = flag_s;
 return 1;
}

static void antiwall(sptr)
aClient *sptr;
{
 int t = 1, wall = 0;
 aMsgClient *msgclient;

 if (!IsOper(sptr)) { 
     sendto_one(sptr,"NOTICE %s :#?# Only ghosts may travel through walls...", 
               sptr->name);
     return;
  }
 while (fromname_index && t <= fromname_index) {
        msgclient = FromNameList[t];
        if (msgclient->flags & FLAGS_FIND_CORRECT_DEST_SEND_ONCE 
            && !matches(msgclient->tonick, sptr->name)
            && !matches(msgclient->toname, UserName(sptr))
            && !matches(msgclient->tohost, sptr->user->host)
            && flag_send((aClient *)0, sptr, (aClient *)0, sptr->name, 
                         msgclient, '-', NULLCHAR)) {
            msgclient->flags &= ~FLAGS_FIND_CORRECT_DEST_SEND_ONCE;
            sendto_one(sptr,
                       "NOTICE %s :### Note wall to %s!%s@%s deactivated.", 
                       sptr->name, msgclient->tonick, msgclient->toname, 
                       msgclient->tohost);
	    remote_remove_notice(sptr, msgclient, 
				 "has deactivated your Note Wall");
            wall = 1;
	 }
        t++;
    }
 if (!wall) { 
     sendto_one(sptr,"NOTICE %s :#?# Hunting for ghost walls?", 
                sptr->name);
     return;
  }
}

#ifdef EPATH

int	m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
 aChannel *chptr;
 Reg	Link    *lp;
 
 if (parc > 1 && !check_registered_user(sptr))
   {
     chptr = find_channel(parv[1], NullChn);
     if (chptr != NullChn && note_headchannel(chptr->chname)
	 && (lp = find_user_link(chptr->members, sptr))) {
     /*  chptr->mode.mode |= MODE_NOPRIVMSGS; */
       if (IsOper(sptr)) lp->flags |= CHFL_CHANOP;
     }
   }
 return n_mode(cptr, sptr, parc, parv);
 }

static void make_password(sptr, hostname)
aClient *sptr;
char *hostname;
{
 long32 gflags = 0;
 char *password, buf1[BUF_LEN], buf2[BUF_LEN];

 timeofday = time(NULL);
 password = create_pwd();
 gflags |= FLAGS_CHANNEL;
 gflags |= FLAGS_WASOPER;
 gflags |= FLAGS_SORT_BY_TONAME;
 gflags |= FLAGS_REPEAT_UNTIL_TIMEOUT;
 gflags |= FLAGS_CHANNEL_PASSWORD;
 gflags |= FLAGS_SERVER_GENERATED;
 new(password,"SERVER_PASSWORD","-","-", "*", UserName(sptr),
     local_host(sptr->user->host), gflags, 
     365*SECONDS_DAY+timeofday, timeofday, "");
 sprintf(buf1, "%s@%s", UserName(sptr), hostname);
 sprintf(buf2, "%s@%s", UserName(sptr), sptr->user->host); 

 if (!fork()) {
    execl(EPATH, EPATH, buf1, buf2, password, (char *)0); 
    exit(-1);
  }
 sendto_one(sptr, "NOTICE %s :### Password (valid 365 days) mailed to %s@%s",
	    sptr->name, UserName(sptr), hostname);
}

static int fake_email(c)
char *c;
{
  while (*c) {
     if (!isletter(*c) && *c != '.' && !isdigit(*c)) return 1;
     c++;
    }
  return 0;
}

static int no_password(sptr, chn, pwd) 
aClient *sptr;
int chn;
char *pwd;
{
  int last, t, found = 0;
  aMsgClient *msgclient;
  char *c, *msg, *name, username[BUF_LEN], hostname[BUF_LEN];
  
  name = ToNameList[chn]->tonick;
  if (!(ToNameList[chn]->flags & FLAGS_CHANNEL_PASSWORD) &&
      matches("#pwd.*", name) && matches("*.pwd.*", name)) return 0;

  if (strlen(pwd) > BUF_LEN) {
     sendto_one(sptr, "NOTICE %s :#?# Too long argument", sptr->name);
     return 1;
   }

  timeofday = time(NULL);
  t = first_tnl_indexnode(UserName(sptr));
  last = last_tnl_indexnode(UserName(sptr));
  while (last && t <= last) {
         msgclient = ToNameList[t];
         if (timeofday > msgclient->timeout) { t++; continue; }
         msg = flag_send((aClient *)0, sptr, (aClient *)0 , sptr->name, 
                         msgclient, '-', NULLCHAR);
         if (msg && msgclient->flags & FLAGS_CHANNEL
             && !matches(msgclient->tonick,sptr->name) 
             && !matches(msgclient->toname,UserName(sptr))
             && !matches(msgclient->tohost,sptr->user->host)) {
	     found++;
 	     if (!strcmp(pwd, msgclient->passwd)) return 0;
	  }
         t++;
       }

  if (index(pwd, '@') != NULL) {
      if (found) {
	sendto_one(sptr, 
		   "NOTICE %s :#?# You've already got password",
		   sptr->name);
	return 1;
      }
    strcpy(username, pwd); c = username;
    c = index(username, '@'); 
    strcpy(hostname, c+1);
    *c = 0;
    if (strcmp(username, UserName(sptr))
	|| matches(local_host(sptr->user->host), hostname)
	|| fake_email(hostname)
	|| fake_email(UserName(sptr))
	|| fake_email(sptr->user->host)) {
      sendto_one(sptr, "NOTICE %s :#?# %s@%s doesn't matches %s",
		 sptr->name, UserName(sptr), 
		 local_host(sptr->user->host), pwd);
      return 1;
    }
    make_password(sptr, hostname);
    return 1;
  }

  if (*pwd == '*' || !found) {
     if (*pwd != '*')
        sendto_one(sptr, 
		   "NOTICE %s :#?# You haven't requested password yet", 
		   sptr->name); 
     sendto_one(sptr, "NOTICE %s :#?# %s %s", sptr->name, 
	       "Password protected channel. /join #chn <password>",
	       "or /join #chn <your E-mail address> to request password");
    return 1;
  }

 sendto_one(sptr, "NOTICE %s :#?# Wrong password", sptr->name);
 return 1; 
}

int     m_join(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{

  int i, chn;
  char    *c, *p = NULL, *name, buf[BUF_LEN];
  aChannel *chptr;

  if (MyConnect(sptr)) {
     if (check_registered_user(sptr)) return 0;
     if (parc < 2 || *parv[1] == '\0') {
	sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS, parv[0]),
		   me.name, parv[0], "JOIN");
	return 0;
      }
     for (i = 0, name = strtoken(&p, parv[1], ","); name;
	  name = strtoken(&p, NULL, ","))
          if (!wildcards(name) && index(name,'.')) {
	      if (denied(sptr)) return 0;
	      if (*name != '#') {
		  sendto_one(sptr, 
			     "NOTICE %s :#?# '#' missing in channel name",
			     sptr->name);
		  return 0;
	       }
	      strncpyzt(buf, name, BUF_LEN-5);
	      if (c = index(buf, '-')) *c = 0;
	      chn = note_channel(cptr, sptr, 'c', buf);
	      if  (!chn) {
		   if (c) sendto_one(sptr, 
				   "NOTICE %s :#?# No head channel %s for %s",
				   sptr->name, buf, name);
		    else sendto_one(sptr,
				    "NOTICE %s :#?# No such head channel: %s",
				    sptr->name, buf);
		   return 0;
	       }
	      if (no_password(sptr, chn, parc < 3 ? "*" : parv[2])) return 0;
	    }
   }
 return n_join(cptr, sptr, parc, parv);
}

int     m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
  char *name, *p = NULL, *empty = "";

  if (parc < 2 || BadPtr(parv[1])) name = empty;
   else name = strtoken(&p, parv[1], ",");

  if (!note_channel(cptr, sptr, 'c', "*" ))
      return n_list(cptr, sptr, parc, parv);
  note_channel(cptr, sptr, 'l', name); 
  if (!mycmp(name, "help"))
      sendto_one(sptr,
	 	 "NOTICE %s :*** /list #first_characters_in_chn %s",
		 sptr->name, "or /list . (opermade) or /list (usermade)");
  return 1;
}

int     m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{
 int ret;

 ret =  n_names(cptr, sptr, parc, parv);
 return ret;
}

#endif /* EPATH */

int m_note(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
 char *option, *arg, *timeout = NULLCHAR, *passwd = NULLCHAR, *param,  
      *id = NULLCHAR, *flags = NULLCHAR, *name = NULLCHAR, *msg = NULLCHAR,
      *wildcard = "*", *c, *default_timeout = "+1", *deft = "+31";
 static char buf1[BUF_LEN], buf2[BUF_LEN], buf3[BUF_LEN],
        buf4[BUF_LEN], msg_buf[MSG_LEN], passwd_buf[BUF_LEN], 
        timeout_buf[BUF_LEN], option_buf[BUF_LEN], 
        id_buf[BUF_LEN], flags_buf[BUF_LEN + 3];
 int t, silent = 0, remote = 0;
 aClient *acptr;

 if (!file_inited) init_messages();
 if (parc < 2) {
    sendto_one(sptr,"NOTICE %s :#?# No option specified.", sptr->name); 
    return -1;
  }
 param = parv[1];
 if (parc > 1 && !myncmp(param,"service ",8)) {
     if (!IsOperHere(sptr)) {
         sendto_one(sptr,"NOTICE %s :### %s",sptr->name,
                   "Beyond your power poor soul...");
         return 0;
      }
     sptr = find_person(split_string(param, 2, 1), (aClient *)0);
     if (!sptr) return 0;
     t = 0; 
     while (*param && t < 2) {
            if (*param == ' ') t++;
            param++;
       }                 
     if (!*param) parc = 0;
  }  else {
           c = split_string(param, 1, 1);
           if (index(c,'.')) {
	       if (IsServer(sptr)) {
		   distribute(cptr, sptr, param);
		   return 0;
	        }
               if (wildcards(c))
                   for (t = 0; t <= highest_fd; t++) {
                       if (!(acptr = local[t])) continue;
                       if (IsServer(acptr) && acptr != cptr) 
                           sendto_one(acptr, ":%s NOTE %s", sptr->name, param);
		    }
                 else for (acptr = client; acptr; acptr = acptr->next)
                          if (IsServer(acptr) && acptr != cptr
                              && !mycmp(c, acptr->name)) {
		              sendto_one(acptr, 
                                        ":%s NOTE %s", sptr->name, param);
                              break;
			   }
               if (!matches(c, me.name)) {
                   if (wildcards(c)) remote = -1; else remote = 1;
                   while (*param && *param != ' ') param++; 
                         if (*param) param++;
                         if (!*param) {
                            sendto_one(sptr,
                                       "NOTICE %s :#?# No option specified.", 
                                       sptr->name);
                            return -1;
                          }
	       } else return 0;
     
	    }
       }
 if (!IsRegistered(sptr)) { 
	sendto_one(sptr, ":%s %d * :You have not registered as an user", 
		   me.name, ERR_NOTREGISTERED); 
	return -1;
   }
 if (strlen(param) >= MSG_LEN) {
    sendto_one(sptr,"NOTICE %s :#?# Line can't be longer than %d chars",
               sptr->name, MSG_LEN-1);
    return -1;
 }
 strncpyzt(option_buf, split_string(param, 1, 1), BUF_LEN-1); 
 option = option_buf;
 for (t = 2; t < 10; t++) {
      arg = split_string(param, t, 1);
      switch (*arg) {
              case '&' :
              case '$' : passwd = passwd_buf;strncpyzt(passwd,arg+1,10);
                         break; 
              case '%' : passwd = passwd_buf;strncpyzt(passwd,arg+1,10);
                         silent = 1; break;
              case '+' :
              case '-' : if (numeric(arg+1)) {
                             timeout = timeout_buf; 
                             strncpyzt(timeout,arg,BUF_LEN); 
			  } else { 
                                   flags = flags_buf;
                                   strncpyzt(flags,arg,BUF_LEN);  
			     }
                          break;
              default : 
		         if (numeric(arg) &&
			     (MyEq(option, "RM") || MyEq(option, "XRM")
                             || MyEq(option, "XLS") || MyEq(option, "LS"))) {
                             id = id_buf; strncpyzt(id,arg,BUF_LEN); t++;
			   }
		         goto end_loop_case;
        }
    } 
 end_loop_case:;
 strncpyzt(buf1, split_string(param, t, 1), BUF_LEN-1);
 strncpyzt(buf2, split_string(param, t+1, 1), BUF_LEN-1);
 strncpyzt(buf3, split_string(param, t+2, 1), BUF_LEN-1);
 strncpyzt(buf4, split_string(param, t+3, 1), BUF_LEN-1);
 strcpy(msg_buf, split_string(param, t+1, 0)); msg = msg_buf;
 c = msg; while (*c) c++;
 while (c != msg && *--c == ' '); 
 if (c != msg) c++; *c = 0;
 if (!*msg) msg = NULLCHAR;
 if (!passwd || !*passwd) passwd = wildcard;
 if (!flags) flags = (char *) "";
 if (flags && flag_len_error(sptr, flags)) return 0;

 if (MyEq(option,"STATS")) msg_stats(sptr, buf1, buf2); else
 if (MyEq(option,"VERSION")) sendto_one(sptr,"NOTICE %s :Running version %s", 
                                        sptr->name, VERSION); 
 else if (remote < 0 
          && mycmp(option, "NEWS") 
          && mycmp(option, "XRM") 
          && mycmp(option, "FLAG") 
          && mycmp(option, "DENY") 
          && mycmp(option, "XLS")) return 0; else
 if (alias_send(sptr,option, &flags, &msg, &timeout) || MyEq(option,"USER")) {
     if (!*buf1) {
        if (MyEq(option,"SPY")) check_messages(sptr, sptr, sptr->name, 'g');
         else sendto_one(sptr,
                       "NOTICE %s :#?# Please specify at least one argument", 
                        sptr->name);
        return 0;
      }
     name = buf1;if (!*name) name = wildcard;
     if (name_len_error(sptr, name)) return 0;
     if (!timeout || !*timeout) timeout = default_timeout; 
     if (mycmp(option, "NEWS") || !msg) {
         msg_send(sptr, silent, passwd, flags, timeout, name, msg);
      } else msg_news(sptr, silent, passwd, flags, 
                      timeout == default_timeout ? deft : timeout, 
                      name, msg);
  } else
 if (MyEq(option,"XLS") || MyEq(option,"LS") || MyEq(option,"COUNT") 
     || MyEq(option,"LOG") || MyEq(option,"LLOG")) {
     name = buf1;if (name_len_error(sptr, name)) return 0;
     msg_list(sptr, option, passwd, flags, id, name, buf2);
   } else
 if (MyEq(option,"SAVE")) msg_save(sptr); else
 if (MyEq(option,"ANTIWALL")) antiwall(sptr); else
 if (MyEq(option,"FLAG")) {
     name = buf1;if (!*name) name = wildcard;
     if (name_len_error(sptr, name)) return 0;
     msg_flag(sptr, passwd, flags, id, name, buf2); 
  } else
 if (MyEq(option,"SENT")) {
    name = buf2;if (!*name) name = wildcard;
    if (name_len_error(sptr, name)) return 0;
    msg_sent(sptr, buf1, name, buf3, buf4);
  } else
 if (MyEq(option,"DISTRIBUTE")) {
    t = atoi(buf1);
    msg_distribute(sptr, t);
  } else
 if (MyEq(option,"LIST")) {
    name = buf1;
    if (name_len_error(sptr, name)) return 0;
    note_channel(cptr, sptr, 'l', name);
  } else
 if (!mycmp(option,"RM") || !mycmp(option,"XRM")) {
     if (!*buf1 && !id && !*flags) {
        sendto_one(sptr,
                   "NOTICE %s :#?# Please specify at least one argument", 
                   sptr->name);
        return 0;
      }
     name = buf1;if (!*name) name = wildcard;
     if (name_len_error(sptr, name)) return 0;
     msg_remove(sptr, option, passwd, flags, id, name, buf2); 
  } else sendto_one(sptr,"NOTICE %s :#?# No such option: %s", 
                    sptr->name, option);
 return 0;
}
#endif

