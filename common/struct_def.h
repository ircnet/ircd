/************************************************************************
 *   IRC - Internet Relay Chat, common/struct_def.h
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

typedef	struct	ConfItem aConfItem;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	Service	aService;
typedef	struct	SLink	Link;
typedef	struct	SMode	Mode;
typedef	struct	fdarray	FdAry;
typedef	struct	CPing	aCPing;
typedef	struct	Zdata	aZdata;
#ifdef CACHED_MOTD
typedef struct        MotdItem aMotd;
typedef struct        MotdItem aExtCf;
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define	NICKLEN		9	/* Necessary to put 9 here instead of 10
				** if s_msg.c/m_nick has been corrected.
				** This preserves compatibility with old
				** servers --msa
				*/
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	80
#define	CHANNELLEN	50
#define	PASSWDLEN 	20
#define	KEYLEN		23
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXBANS		30
#define	MAXBANLENGTH	1024
#define	BANLEN		(USERLEN + NICKLEN + HOSTLEN + 3)
#define MAXPENALTY	10
#define	CHIDLEN		5		/* WARNING: *DONT* CHANGE THIS!!!! */

#define	READBUF_SIZE	16384	/* used in s_bsd.c *AND* s_zip.c ! */
 
/*
 * Make up some numbers which should reflect average leaf server connect
 * queue max size.
 * queue=(<# of channels> * <channel size> + <user size> * <# of users>) * 2
 * pool=<queue per client> * <avg. # of clients having data in queue>
 */
#define	QUEUELEN	(((MAXCONNECTIONS / 10) * (CHANNELLEN + BANLEN + 16) +\
			  (HOSTLEN * 4 + REALLEN + NICKLEN + USERLEN + 24) *\
			  (MAXCONNECTIONS / 2)) * 2)

#define	BUFFERPOOL	(DBUFSIZ * MAXCONNECTIONS * 2) + \
			(QUEUELEN * MAXSERVERS)

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)

/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_CONSOLE	0x001
#define	BOOT_QUICK	0x002
#define	BOOT_DEBUG	0x004
#define	BOOT_INETD	0x008
#define	BOOT_TTY	0x010
#define	BOOT_OPER	0x020
#define	BOOT_AUTODIE	0x040
#define	BOOT_BADTUNE	0x080
#define	BOOT_PROT	0x100
#define	BOOT_STRICTPROT	0x200
#define	BOOT_NOIAUTH	0x400

#define	STAT_RECONNECT	-7	/* Reconnect attempt for server connections */
#define	STAT_LOG	-6	/* logfile for -x */
#define	STAT_MASTER	-5	/* Local ircd master before identification */
#define	STAT_CONNECTING	-4
#define	STAT_HANDSHAKE	-3
#define	STAT_UNKNOWN	-2
#define	STAT_ME		-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1
#define	STAT_SERVICE	2

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT && (x)->user)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER || \
				 (x)->status == STAT_ME)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN || \
				 (x)->status == STAT_MASTER)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsLog(x)		((x)->status == STAT_LOG)
#define	IsService(x)		((x)->status == STAT_SERVICE && (x)->service)
#define	IsReconnect(x)		((x)->status == STAT_RECONNECT)

#define	SetMaster(x)		((x)->status = STAT_MASTER)
#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetLog(x)		((x)->status = STAT_LOG)
#define	SetService(x)		((x)->status = STAT_SERVICE)

#define	FLAGS_PINGSENT   0x0001	/* Unreplied ping sent */
#define	FLAGS_DEADSOCKET 0x0002	/* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED     0x0004	/* Prevents "QUIT" from being sent for this */
#define	FLAGS_BLOCKED    0x0008	/* socket is in a blocked condition [unused] */
#define	FLAGS_UNIX	 0x0010	/* socket is in the unix domain, not inet */
#define	FLAGS_CLOSING    0x0020	/* set when closing to suppress errors */
#define	FLAGS_LISTEN     0x0040 /* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS  0x0080 /* ok to check clients access if set [unused]*/
#define	FLAGS_DOINGDNS	 0x0100 /* client is waiting for a DNS response */
#define	FLAGS_AUTH	 0x0200 /* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	 0x0400	/* set if we havent writen to ident server */
#define	FLAGS_LOCAL	 0x0800 /* set for local clients */
#define	FLAGS_GOTID	 0x1000	/* successful ident lookup achieved */
#define	FLAGS_XAUTH	 0x2000	/* waiting on external authentication */
#define	FLAGS_NONL	 0x4000 /* No \n in buffer */
#define	FLAGS_HELD	 0x8000	/* connection held and reconnect try */
#define	FLAGS_CBURST	0x10000	/* set to mark connection burst being sent */
#define FLAGS_RILINE    0x20000 /* Restricted i-line [unused?] */
#define FLAGS_QUIT      0x40000 /* QUIT :comment shows it's not a split */
#define FLAGS_SPLIT     0x80000 /* client QUITting because of a netsplit */
#define FLAGS_HIDDEN   0x100000 /* netsplit is behind a hostmask */
#define	FLAGS_UNKCMD   0x200000	/* has sent an unknown command */
#define	FLAGS_ZIP      0x400000 /* link is zipped */
#define	FLAGS_ZIPRQ    0x800000 /* zip requested */
#define	FLAGS_ZIPSTART	0x1000000 /* start of zip (ignore any CRLF) */

#define	FLAGS_OPER       0x0001	/* Operator */
#define	FLAGS_LOCOP      0x0002 /* Local operator -- SRB */
#define	FLAGS_WALLOP     0x0004 /* send wallops to them */
#define	FLAGS_INVISIBLE  0x0008 /* makes user invisible */
#define FLAGS_RESTRICTED 0x0010 /* Restricted user */
#define FLAGS_AWAY       0x0020 /* user is away */

#define	SEND_UMODES	(FLAGS_INVISIBLE|FLAGS_OPER|FLAGS_WALLOP|FLAGS_AWAY)
#define	ALL_UMODES	(SEND_UMODES|FLAGS_LOCOP|FLAGS_RESTRICTED)

/*
 * flags macros.
 */
#define	IsOper(x)		((x)->user && (x)->user->flags & FLAGS_OPER)
#define	IsLocOp(x)		((x)->user && (x)->user->flags & FLAGS_LOCOP)
#define	IsInvisible(x)		((x)->user->flags & FLAGS_INVISIBLE)
#define IsRestricted(x)         ((x)->user && \
				 (x)->user->flags & FLAGS_RESTRICTED)
#define	IsAnOper(x)		((x)->user && \
				 (x)->user->flags & (FLAGS_OPER|FLAGS_LOCOP))
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsServer(x) || IsAnOper(x))
#define	SendWallops(x)		((x)->user->flags & FLAGS_WALLOP)
#define	IsUnixSocket(x)		((x)->flags & FLAGS_UNIX)
#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
#define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)
#define	IsLocal(x)		(MyConnect(x) && (x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)
#define	IsHeld(x)		((x)->flags & FLAGS_HELD)
#define	CBurst(x)		((x)->flags & FLAGS_CBURST)

#define	SetOper(x)		((x)->user->flags |= FLAGS_OPER)
#define	SetLocOp(x)    		((x)->user->flags |= FLAGS_LOCOP)
#define	SetInvisible(x)		((x)->user->flags |= FLAGS_INVISIBLE)
#define SetRestricted(x)        ((x)->user->flags |= FLAGS_RESTRICTED)
#define	SetWallops(x)  		((x)->user->flags |= FLAGS_WALLOP)
#define	SetUnixSock(x)		((x)->flags |= FLAGS_UNIX)
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	DoingXAuth(x)		((x)->flags & FLAGS_XAUTH)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)

#define	ClearOper(x)		((x)->user->flags &= ~FLAGS_OPER)
#define	ClearInvisible(x)	((x)->user->flags &= ~FLAGS_INVISIBLE)
#define ClearRestricted(x)      ((x)->user->flags &= ~FLAGS_RESTRICTED)
#define	ClearWallops(x)		((x)->user->flags &= ~FLAGS_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearXAuth(x)		((x)->flags &= ~FLAGS_XAUTH)
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS)

/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1	/* report_error() and other errors that are found */
#define	DEBUG_READ   2
#define	DEBUG_WRITE  2
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4	/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5	/* general usful info */
#define	DEBUG_NUM    6	/* numerics */
#define	DEBUG_SEND   7	/* everything that is sent out */
#define	DEBUG_DEBUG  8	/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9	/* malloc/free calls */
#define	DEBUG_LIST  10	/* debug list use */
#define	DEBUG_L10   10
#define	DEBUG_L11   11

/*
 * defines for curses in client
 */
#define	DUMMY_TERM	0
#define	CURSES_TERM	1
#define	TERMCAP_TERM	2

struct	CPing	{
	u_short	port;		/* port to send pings to */
	u_long	rtt;		/* average RTT */
	u_long	ping;
	u_long	seq;		/* # sent still in the "window" */
	u_long	lseq;		/* sequence # of last sent */
	u_long	recvd;		/* # received still in the "window" */
	u_long	lrecvd;		/* # received */
};

struct	ConfItem	{
	u_int	status;		/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	struct	IN_ADDR ipnum;	/* ip number of host field */
	char	*host;
	char	*passwd;
	char	*name;
	int	port;
	u_int	pref;		/* preference value */
	struct	CPing	*ping;
	time_t	hold;	/* Hold action until this time (calendar time) */
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x000001
#define	CONF_CLIENT		0x000002
#define CONF_RCLIENT            0x000004
#define	CONF_CONNECT_SERVER	0x000008
#define	CONF_NOCONNECT_SERVER	0x000010
#define	CONF_ZCONNECT_SERVER	0x000020
#define	CONF_LOCOP		0x000040
#define	CONF_OPERATOR		0x000080
#define	CONF_ME			0x000100
#define	CONF_KILL		0x000200
#define	CONF_ADMIN		0x000400
#ifdef 	R_LINES
#define	CONF_RESTRICT		0x000800
#endif
#define	CONF_CLASS		0x001000
#define	CONF_SERVICE		0x002000
#define	CONF_LEAF		0x004000
#define	CONF_LISTEN_PORT	0x008000
#define	CONF_HUB		0x010000
#define	CONF_VER		0x020000
#define	CONF_BOUNCE		0x040000
#define	CONF_OTHERKILL		0x080000
#define	CONF_DENY		0x100000

#define	CONF_OPS		(CONF_OPERATOR | CONF_LOCOP)
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER |\
				 CONF_ZCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_RCLIENT | CONF_SERVICE | CONF_OPS | \
				 CONF_SERVER_MASK)

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)

typedef	struct	{
	u_long	pi_id;
	u_long	pi_seq;
	struct	timeval	pi_tv;
	aConfItem *pi_cp;
} Ping;


#define	PING_REPLY	0x01
#define	PING_CPING	0x02

#ifdef	ZIP_LINKS
/* the minimum amount of data needed to trigger compression */
# define	ZIP_MINIMUM	4096

/* the maximum amount of data to be compressed (can actually be a bit more) */
# define	ZIP_MAXIMUM	8192	/* WARNING: *DON'T* CHANGE THIS!!!! */

struct Zdata {
	z_stream	*in;		/* input zip stream data */
	z_stream	*out;		/* output zip stream data */
	char		outbuf[ZIP_MAXIMUM]; /* outgoing (unzipped) buffer */
	int		outcount;	/* size of outbuf content */
};
#endif

#ifdef CACHED_MOTD
struct  MotdItem
{ 
    char    *line;
    struct  MotdItem *next;
};
#endif

/*
 * Client structures
 */
struct	User	{
	Link	*channel;	/* chain of channel pointer blocks */
	Link	*invited;	/* chain of invite pointer blocks */
	Link	*uwas;		/* chain of whowas pointer blocks */
	char	*away;		/* pointer to away message */
	time_t	last;
	int	refcnt;		/* Number of times this block is referenced
				** from aClient (field user), aServer (field
				** by) and whowas array (field ww_user).
				*/
	int	joined;		/* number of channels joined */
	int	flags;		/* user modes */
        struct	Server	*servp;
				/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
	struct	User	*nextu, *prevu;
	aClient	*bcptr;
	char	username[USERLEN+1];
	char	host[HOSTLEN+1];
	char	*server;
};

struct	Server	{
	anUser	*user;		/* who activated this connection */
	anUser  *userlist;      /* first user on this server in the user list*/
	char	*up;	/* uplink for this server */
	aConfItem *nline;	/* N-line pointer for this server */
	int	version;        /* version id for local client */
	int	snum;
	int	stok,
		ltok;
	int	refcnt;		/* Number of times this block is referenced
				** from anUser (field servp), aService (field
				** servp) and aClient (field serv)
				*/
	struct	Server	*nexts, *prevs, *shnext;
	aClient	*bcptr;
	char	by[NICKLEN+1];
	char	tok[5];
	time_t	lastload;	/* penalty like counters, see s_serv.c
				** should be in the local part, but..
				*/
};

struct	Service	{
	int	wants;
	int	type;
	char	*server;
	aServer	*servp;
	struct	Service	*nexts, *prevs;
	aClient	*bcptr;
	char	dist[HOSTLEN+1];
};

struct Client	{
	struct	Client *next,*prev, *hnext;
	anUser	*user;		/* ...defined, if this is a User */
	aServer	*serv;		/* ...defined, if this is a server */
	aService *service;
	u_int	hashv;		/* raw hash value */
	long	flags;		/* client flags */
	aClient	*from;		/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	short	status;		/* Client type */
	char	name[HOSTLEN+1]; /* Unique name of the client, nick or host */
	char	username[USERLEN+1]; /* username here now for auth stuff */
	char	*info;		/* Free form additional client information */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	char	buffer[BUFSIZE]; /* Incoming message buffer */
#ifdef	ZIP_LINKS
	aZdata	*zip;		/* zip data */
#endif
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
	dbuf	recvQ;		/* Hold for data incoming yet to be parsed */
	long	sendM;		/* Statistics: protocol messages send */
	long	sendK;		/* Statistics: total k-bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
	long	receiveK;	/* Statistics: total k-bytes received */
	u_short	sendB;		/* counters to count upto 1-k lots of bytes */
	u_short	receiveB;	/* sent and received. */
	time_t	lasttime;
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	u_int	sact;		/* could conceivably grow large...*/
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	int	authfd;		/* fd for rfc931 authentication */
	char	*auth;
	int	priority;	/* priority for selection as active */
	u_short	ract;		/* no fear about this. */
	u_short	port;		/* and the remote port# too :-) */
	struct	IN_ADDR	ip;	/* keep real ip# too */
	struct	hostent	*hostp;
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char	passwd[PASSWDLEN+1];
	char	exitc;
};

#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

/*
 * statistics structures
 */
struct	stats {
	u_int	is_cl;	/* number of client connections */
	u_int	is_sv;	/* number of server connections */
	u_int	is_ni;	/* connection but no idea who it was
			 * (can be a P: line that has been removed -krys) */
	u_short	is_cbs;	/* bytes sent to clients */
	u_short	is_cbr;	/* bytes received to clients */
	u_short	is_sbs;	/* bytes sent to servers */
	u_short	is_sbr;	/* bytes received to servers */
	u_long	is_cks;	/* k-bytes sent to clients */
	u_long	is_ckr;	/* k-bytes received to clients */
	u_long	is_sks;	/* k-bytes sent to servers */
	u_long	is_skr;	/* k-bytes received to servers */
	time_t	is_cti;	/* time spent connected by clients */
	time_t	is_sti;	/* time spent connected by servers */
	u_int	is_ac;	/* connections accepted */
	u_int	is_ref;	/* accepts refused */
	u_int	is_unco; /* unknown commands */
	u_int	is_wrdi; /* command going in wrong direction */
	u_int	is_unpf; /* unknown prefix */
	u_int	is_empt; /* empty message */
	u_int	is_num;	/* numeric message */
	u_int	is_kill; /* number of kills generated on collisions */
	u_int	is_fake; /* MODE 'fakes' */
	u_int	is_asuc; /* successful auth requests */
	u_int	is_abad; /* bad auth requests */
	u_int	is_udpok;	/* packets recv'd on udp port */
	u_int	is_udperr;	/* packets recvfrom errors on udp port */
	u_int	is_udpdrop;	/* packets recv'd but dropped on udp port */
	u_int	is_loc;	/* local connections made */
	u_int	is_nosrv; /* user without server */
	u_long	is_wwcnt; /* number of nicks overwritten in whowas[] */
	u_long	is_wwt;	  /* sum of elapsed time on when overwriting whowas[]*/
	u_long	is_wwMt;  /* max elapsed time on when overwriting whowas[] */
	u_long	is_wwmt;  /* min elapsed time on when overwriting whowas[] */
	u_long	is_lkcnt; /* number of nicks overwritten in locked[] */
	u_long	is_lkt;   /* sum of elapsed time on when overwriting locked[]*/
	u_long	is_lkMt;  /* max elapsed time on when overwriting locked[] */
	u_long	is_lkmt;  /* min elapsed time on when overwriting locked[] */
	u_int	is_ckl;   /* calls to check_link() */
	u_int	is_cklQ;  /* rejected: SendQ too high */
	u_int	is_ckly;  /* rejected: link too young */
	u_int	is_cklno; /* rejected: "flood" */
	u_int	is_cklok; /* accepted */
	u_int	is_cklq;  /* accepted early */
};

/* mode structure for channels */

struct	SMode	{
	u_int	mode;
	int	limit;
	char	key[KEYLEN+1];
};

/* Message table structure */

struct	Message	{
	char	*cmd;
	int	(* func)();
	int	parameters;
	u_int	flags;
		/* bit 0 set means that this command is allowed to be used
		 * only on the average of once per 2 seconds -SRB */
	u_int	count;	/* total count */
	u_int	rcount;	/* remote count */
	u_long	bytes;
};

#define	MSG_LAG		0x0001
#define	MSG_NOU		0x0002	/* Not available to users */
#define	MSG_SVC		0x0004	/* Services only */
#define	MSG_NOUK	0x0008	/* Not available to unknowns */
#define	MSG_REG		0x0010	/* Must be registered */
#define	MSG_REGU	0x0020	/* Must be a registered user */
/*#define	MSG_PP		0x0040*/
/*#define	MSG_FRZ		0x0080*/
#define	MSG_OP		0x0100	/* opers only */
#define	MSG_LOP		0x0200	/* locops only */

/* fd array structure */

struct	fdarray	{
	int	fd[MAXCONNECTIONS];
	int	highest;
};

/* general link structure used for chains */

struct	SLink	{
	struct	SLink	*next;
	union {
		aClient	*cptr;
		aChannel *chptr;
		aConfItem *aconf;
		char	*cp;
		int	i;
	} value;
	int	flags;
};

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	u_int	hashv;		/* raw hash value */
	Mode	mode;
	char	topic[TOPICLEN+1];
	int	users;		/* current membership total */
	Link	*members;	/* channel members */
	Link	*invites;	/* outstanding invitations */
	Link	*mlist;		/* list of extended modes: +b/+e/+I */
	Link	*clist;		/* list of connections which are members */
	time_t	history;	/* channel history (aka channel delay) */
	time_t	reop;		/* server reop stamp for !channels */
	char	chname[1];
};

/*
** Channel Related macros follow
*/

/* Channel related flags */

#define	CHFL_UNIQOP     0x0001 /* Channel creator */
#define	CHFL_CHANOP     0x0002 /* Channel operator */
#define	CHFL_VOICE      0x0004 /* the power to speak */
#define	CHFL_BAN	0x0008 /* ban channel flag */
#define	CHFL_EXCEPTION	0x0010 /* exception channel flag */
#define	CHFL_INVITE	0x0020 /* invite channel flag */

/* Channel Visibility macros */

#define	MODE_UNIQOP	CHFL_UNIQOP
#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_PRIVATE	0x0008
#define	MODE_SECRET	0x0010
#define	MODE_MODERATED  0x0020
#define	MODE_TOPICLIMIT 0x0040
#define	MODE_INVITEONLY 0x0080
#define	MODE_NOPRIVMSGS 0x0100
#define	MODE_KEY	0x0200
#define	MODE_BAN	0x0400
#define	MODE_LIMIT	0x0800
#define	MODE_ANONYMOUS	0x1000
#define	MODE_QUIET	0x2000
#define	MODE_EXCEPTION	0x4000
#define	MODE_INVITE	0x8000
#define	MODE_REOP	0x10000
#define	MODE_FLAGS	0x1ffff
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS	(MODE_UNIQOP|MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY\
			 |MODE_LIMIT|MODE_INVITE|MODE_EXCEPTION)
/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define	MODE_DEL       0x40000000
#define	MODE_ADD       0x80000000
 */

#define	HoldChannel(x)		(!(x))
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	IsAnonymous(c)		((c) && ((c)->mode.mode & MODE_ANONYMOUS))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | MODE_SECRET)) == 0)

/*
#define	IsMember(u, c)		(assert(*(c)->chname != '\0'), find_user_link((c)->members, u) ? 1 : 0)
#define	IsMember(u, c)		(find_user_link((c)->members, u) ? 1 : 0)
*/
#define       IsMember(u, c)          (u && (u)->user && \
		       find_channel_link((u)->user->channel, c) ? 1 : 0)
#define	IsChannelName(n)	((n) && (*(n) == '#' || *(n) == '&' || \
					*(n) == '+' || *(n) == '!'))
#define	IsQuiet(x)		((x)->mode.mode & MODE_QUIET)
#define	UseModes(n)		((n) && (*(n) == '#' || *(n) == '&' || \
					 *(n) == '!'))

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

#define	isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyPerson(x)			(MyConnect(x) && IsPerson(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))
#define	MyService(x)			(MyConnect(x) && IsService(x))
#define	ME	me.name

typedef	struct	{
	u_long	is_user[2];	/* users, non[0] invis and invis[1] */
	u_long	is_serv;	/* servers */
	u_long	is_service;	/* services */
	u_long	is_chan;	/* channels */
	u_long	is_chanmem;
	u_long	is_chanusers;	/* channels users */
	u_long	is_hchan;	/* channels in history */
	u_long	is_hchanmem;
	u_long	is_cchan;	/* channels in cache */
	u_long	is_cchanmem;
	u_long	is_away;	/* away sets */
	u_long	is_awaymem;
	u_long	is_oper;	/* opers */
	u_long	is_bans;	/* bans */
	u_long	is_banmem;
	u_long	is_invite;	/* invites */
	u_long	is_class;	/* classes */
	u_long	is_conf;	/* conf lines */
	u_long	is_confmem;
	u_long	is_conflink;	/* attached conf lines */
	u_long	is_myclnt;	/* local clients */
	u_long	is_myserv;	/* local servers */
	u_long	is_myservice;	/* local services */
	u_long	is_unknown;	/* unknown (local) connections */
	u_long	is_wwusers;	/* users kept for whowas[] */
	u_long	is_wwaways;	/* aways in users in whowas[] */
	u_long	is_wwawaysmem;
	u_long	is_wwuwas;	/* uwas links */
	u_long	is_localc;	/* local items (serv+service+client+..) */
	u_long	is_remc;	/* remote clients */
	u_long	is_users;	/* user structs */
	u_long	is_useri;	/* user invites */
	u_long	is_userc;	/* user links to channels */
	u_long	is_auth;	/* OTHER ident reply block */
	u_long	is_authmem;
	u_int	is_dbuf;	/* number of dbuf allocated (originally) */
	u_int	is_dbufnow;	/* number of dbuf allocated */
	u_int	is_dbufuse;	/* number of dbuf in use */
	u_int	is_dbufmin;	/* min number of dbuf in use */
	u_int	is_dbufmax;	/* max number of dbuf in use */
	u_int	is_dbufmore;	/* how many times we increased the bufferpool*/
} istat_t;

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define	MODE_NULL      0
#define	MODE_ADD       0x40000000
#define	MODE_DEL       0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* used for sendto_serv */

#define	SV_OLD		0x0000
#define	SV_29		0x0001	/* useless, but preserved for coherence */
#define	SV_NJOIN	0x0002	/* server understands the NJOIN command */
#define	SV_NMODE	0x0004	/* server knows new MODEs (+e/+I) */
#define	SV_NCHAN	0x0008	/* server knows new channels -????name */
				/* ! SV_NJOIN implies ! SV_NCHAN */
#define	SV_2_10		(SV_29|SV_NJOIN|SV_NMODE|SV_NCHAN)

/* used for sendto_flag */

typedef	struct	{
	int	svc_chan;
	char	*svc_chname;
	struct	Channel	*svc_ptr;
}	SChan;

#define	SCH_ERROR	1
#define	SCH_NOTICE	2
#define	SCH_KILL	3
#define	SCH_CHAN	4
#define	SCH_NUM		5
#define	SCH_SERVER	6
#define	SCH_HASH	7
#define	SCH_LOCAL	8
#define	SCH_SERVICE	9
#define	SCH_DEBUG	10
#define	SCH_AUTH	11
#define	SCH_MAX		11

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* Client exit codes for log file */
#define EXITC_UNDEF	'-'	/* unregistered client */
#define EXITC_REG	'0'	/* normal exit */
#define EXITC_DIE	'd'	/* server died */
#define EXITC_DEAD	'D'	/* socket died */
#define EXITC_ERROR	'E'	/* socket error */
#define EXITC_FLOOD	'F'	/* client flooding */
#define EXITC_KLINE	'k'	/* K-lined */
#define EXITC_KILL	'K'	/* KILLed */
#define EXITC_MBUF	'M'	/* mem alloc error */
#define EXITC_PING	'P'	/* ping timeout */
#define EXITC_SENDQ	'Q'	/* send queue exceeded */
#define EXITC_RLINE	'r'	/* R-lined */
#define EXITC_REF	'R'	/* Refused */
#define EXITC_AREF	'U'	/* Unauthorized by iauth */

/* misc defines */

#define	FLUSH_BUFFER	-2
#define	UTMP		"/etc/utmp"
#define	COMMA		","

#define	SAP	struct SOCKADDR *

/* IRC client structures */

#ifdef	CLIENT_COMPILE
typedef	struct	Ignore {
	char	user[NICKLEN+1];
	char	from[USERLEN+HOSTLEN+2];
	int	flags;
	struct	Ignore	*next;
} anIgnore;

#define	IGNORE_PRIVATE	1
#define	IGNORE_PUBLIC	2
#define	IGNORE_TOTAL	3

#define	HEADERLEN	200

#endif /* CLIENT_COMPILE */
