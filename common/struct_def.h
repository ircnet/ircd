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
 *
 *   $Id: struct_def.h,v 1.151 2009/11/13 20:08:10 chopin Exp $
 */

typedef	struct	ConfItem aConfItem;
typedef	struct	ListItem aListItem;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	Service	aService;
typedef	struct	SLink	Link;
typedef	struct	invSLink	invLink;
typedef	struct	SMode	Mode;
typedef	struct	fdarray	FdAry;
typedef	struct	CPing	aCPing;
typedef	struct	Zdata	aZdata;
typedef struct        LineItem aMotd;
#if defined(USE_IAUTH)
typedef struct        LineItem aExtCf;
typedef struct        LineItem aExtData;
#endif

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#if defined(INET6) && (INET6_ADDRSTRLEN > HOSTLEN)
#error HOSTLEN must not be smaller than INET6_ADDRSTRLEN
#endif

#define	NICKLEN		15	/* Must be the same network-wide. */
#define UIDLEN		9	/* must not be bigger than NICKLEN --Beeth */
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	255
#define	CHANNELLEN	50
#define	PASSWDLEN 	20
#define	KEYLEN		23
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXBANS		64
#define	MAXBANLENGTH	2048
#define	BANLEN		(USERLEN + NICKLEN + HOSTLEN + 3)
#define MAXPENALTY	10
#define	CHIDLEN		5		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	SIDLEN		4		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXMODEPARAMS	3		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	DELAYCHASETIMELIMIT	1800	/* WARNING: *DONT* CHANGE THIS!!!! */
#define	LDELAYCHASETIMELIMIT	5400	/* WARNING: *DONT* CHANGE THIS!!!! */

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

#define	BOOT_AUTODIE	0x040
#define	BOOT_BADTUNE	0x080
#define	BOOT_PROT	0x100
#define	BOOT_STRICTPROT	0x200
#define	BOOT_NOIAUTH	0x400
#define	BOOT_STANDALONE	0x800

typedef enum Status {
	STAT_CONNECTING = -4,
	STAT_HANDSHAKE, /* -3 */
	STAT_UNKNOWN,	/* -2 */
	STAT_ME,	/* -1 */
	STAT_SERVER,	/* 0 */
	STAT_CLIENT,	/* 1 */
	STAT_OPER,	/* 2 */
	STAT_SERVICE,	/* 3 */
	STAT_UNREG,	/* 4 */
	STAT_MAX	/* 5 -- size of handler[] table we need */
} Status;

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	(((x)->status == STAT_CLIENT || \
				 (x)->status == STAT_OPER) && (x)->user)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER || \
				 (x)->status == STAT_ME)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT || \
				 (x)->status == STAT_OPER)
#define	IsService(x)		((x)->status == STAT_SERVICE && (x)->service)

#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = IsAnOper((x)) ? STAT_OPER : STAT_CLIENT)
#define	SetService(x)		((x)->status = STAT_SERVICE)

#define	FLAGS_PINGSENT	0x0000001 /* Unreplied ping sent */
#define	FLAGS_DEADSOCK	0x0000002 /* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED	0x0000004 /* Prevents "QUIT" from being sent for this */
#define	FLAGS_BLOCKED	0x0000008 /* socket is in blocked condition [unused] */
#ifdef UNIXPORT
#define	FLAGS_UNIX	0x0000010 /* socket is in the unix domain, not inet */
#endif
#define	FLAGS_CLOSING	0x0000020 /* set when closing to suppress errors */
#define	FLAGS_LISTEN	0x0000040 /* used to mark clients which we listen() on */
#define	FLAGS_XAUTHDONE	0x0000080 /* iauth is finished with this client */
#define	FLAGS_DOINGDNS	0x0000100 /* client is waiting for a DNS response */
#define	FLAGS_AUTH	0x0000200 /* client is waiting on rfc931 response */
#define	FLAGS_WRAUTH	0x0000400 /* set if we havent writen to ident server */
#define	FLAGS_LOCAL	0x0000800 /* set for local clients */
#define	FLAGS_GOTID	0x0001000 /* successful ident lookup achieved */
#define	FLAGS_XAUTH	0x0002000 /* waiting on external authentication */
#define	FLAGS_WXAUTH	0x0004000 /* same as above, but also prevent parsing */
#define	FLAGS_NONL	0x0008000 /* No \n in buffer */
#define	FLAGS_CBURST	0x0010000 /* set to mark connection burst being sent */
#define	FLAGS_QUIT	0x0040000 /* QUIT :comment shows it's not a split */
#define	FLAGS_SPLIT	0x0080000 /* client QUITting because of a netsplit */
#define	FLAGS_HIDDEN	0x0100000 /* netsplit is behind a hostmask,
				     also used for marking clients in who_find
				   */
#define	FLAGS_UNKCMD	0x0200000 /* has sent an unknown command */
#define	FLAGS_ZIP	0x0400000 /* link is zipped */
#define	FLAGS_ZIPRQ	0x0800000 /* zip requested */
#define	FLAGS_ZIPSTART	0x1000000 /* start of zip (ignore any CRLF) */
#define	FLAGS_SQUIT	0x2000000 /* This is set when we send the last
				  ** server, so we know we have to send
				  ** a SQUIT. */
#define	FLAGS_EOB	0x4000000 /* EOB received */
#define FLAGS_LISTENINACTIVE 0x8000000 /* Listener does not listen() */
#ifdef JAPANESE
#define	FLAGS_JP	0x10000000 /* jp version, used both for chans and servs */
#endif
	
#define	FLAGS_OPER	0x0001 /* operator */
#define	FLAGS_LOCOP	0x0002 /* local operator -- SRB */
#define	FLAGS_WALLOP	0x0004 /* send wallops to them */
#define	FLAGS_INVISIBLE	0x0008 /* makes user invisible */
#define FLAGS_RESTRICT	0x0010 /* restricted user */
#define FLAGS_AWAY	0x0020 /* user is away */
#define FLAGS_EXEMPT    0x0040 /* user is exempted from k-lines */
#ifdef XLINE
#define FLAGS_XLINED	0x0100	/* X-lined client */
#endif
#define	SEND_UMODES	(FLAGS_INVISIBLE|FLAGS_OPER|FLAGS_WALLOP|FLAGS_AWAY|FLAGS_RESTRICT)
#define	ALL_UMODES	(SEND_UMODES|FLAGS_LOCOP)

/*
 * user flags macros.
 */
#define	IsOper(x)		((x)->user && (x)->user->flags & FLAGS_OPER)
#define	IsLocOp(x)		((x)->user && (x)->user->flags & FLAGS_LOCOP)
#define	IsInvisible(x)		((x)->user->flags & FLAGS_INVISIBLE)
#define IsRestricted(x)         ((x)->user && \
				 (x)->user->flags & FLAGS_RESTRICT)
#define	IsAnOper(x)		((x)->user && \
				 (x)->user->flags & (FLAGS_OPER|FLAGS_LOCOP))
#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsPrivileged(x)		(IsServer(x) || IsAnOper(x))
#define	SendWallops(x)		((x)->user->flags & FLAGS_WALLOP)
#ifdef UNIXPORT
#define	IsUnixSocket(x)		((x)->flags & FLAGS_UNIX)
#endif
#define	IsListener(x)		((x)->flags & FLAGS_LISTEN)
#define IsListenerInactive(x)	((x)->flags & FLAGS_LISTENINACTIVE)
#define	IsLocal(x)		(MyConnect(x) && (x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCK)
#define	IsBursting(x)		(!((x)->flags & FLAGS_EOB))
#define IsKlineExempt(x)        ((x)->user && (x)->user->flags & FLAGS_EXEMPT)

#define	SetDead(x)		((x)->flags |= FLAGS_DEADSOCK)
#define	CBurst(x)		((x)->flags & FLAGS_CBURST)
#define	SetOper(x)		((x)->user->flags |= FLAGS_OPER, \
				 (x)->status = STAT_OPER)
#define	SetLocOp(x)		((x)->user->flags |= FLAGS_LOCOP, \
				 (x)->status = STAT_OPER)
#define	SetInvisible(x)		((x)->user->flags |= FLAGS_INVISIBLE)
#define SetRestricted(x)	((x)->user->flags |= FLAGS_RESTRICT)
#define	SetWallops(x)		((x)->user->flags |= FLAGS_WALLOP)
#ifdef UNIXPORT
#define	SetUnixSock(x)		((x)->flags |= FLAGS_UNIX)
#endif
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)
#define	SetDoneXAuth(x)		((x)->flags |= FLAGS_XAUTHDONE)
#define	SetEOB(x)		((x)->flags |= FLAGS_EOB)
#define SetListenerInactive(x)	((x)->flags |= FLAGS_LISTENINACTIVE)
#define SetKlineExempt(x)	((x)->user->flags |= FLAGS_EXEMPT)

#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)
#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define	DoingXAuth(x)		((x)->flags & FLAGS_XAUTH)
#define	WaitingXAuth(x)		((x)->flags & FLAGS_WXAUTH)
#define	DoneXAuth(x)		((x)->flags & FLAGS_XAUTHDONE)
#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)

#define	ClearOper(x)		((x)->user->flags &= ~FLAGS_OPER, \
				 (x)->status = STAT_CLIENT)
#define	ClearLocOp(x)		((x)->user->flags &= ~FLAGS_LOCOP, \
				 (x)->status = STAT_CLIENT)
#define	ClearInvisible(x)	((x)->user->flags &= ~FLAGS_INVISIBLE)
#define ClearRestricted(x)      ((x)->user->flags &= ~FLAGS_RESTRICT)
#define	ClearWallops(x)		((x)->user->flags &= ~FLAGS_WALLOP)
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)
#define	ClearXAuth(x)		((x)->flags &= ~FLAGS_XAUTH)
#define	ClearWXAuth(x)		((x)->flags &= ~FLAGS_WXAUTH)
#define ClearListenerInactive(x) ((x)->flags &= ~FLAGS_LISTENINACTIVE)
#ifdef XLINE
#define IsXlined(x)		((x)->user && (x)->user->flags & FLAGS_XLINED)
#define SetXlined(x)		((x)->user->flags |= FLAGS_XLINED)
#define ClearXlined(x)		((x)->user->flags &= ~FLAGS_XLINED)
#endif


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
	char	*name2;
#ifdef XLINE
	char	*name3;
#endif
	int	port;
	long	flags;		/* I-line flags */
	int	pref;		/* preference value */
	struct	CPing	*ping;
	time_t	hold;	/* Hold action until this time (calendar time) */
	char	*source_ip;
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

struct	ListItem	{
	char	*nick;
	char	*user;
	char	*host;
};

/* these define configuration lines (A:, M:, I:, K:, etc.) */
#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x000001
#define	CONF_CLIENT		0x000002

#define	CONF_CONNECT_SERVER	0x000008
#define	CONF_NOCONNECT_SERVER	0x000010
#define	CONF_ZCONNECT_SERVER	0x000020

#define	CONF_OPERATOR		0x000080
#define	CONF_ME			0x000100
#define	CONF_KILL		0x000200
#define	CONF_ADMIN		0x000400
#define	CONF_CLASS		0x001000
#define	CONF_SERVICE		0x002000
#define	CONF_LEAF		0x004000
#define	CONF_LISTEN_PORT	0x008000
#define	CONF_HUB		0x010000
#define	CONF_VER		0x020000
#define	CONF_BOUNCE		0x040000
#define	CONF_OTHERKILL		0x080000
#define	CONF_DENY		0x100000
#ifdef TKLINE
#define	CONF_TKILL		0x200000
#define	CONF_TOTHERKILL		0x400000
#endif
#ifdef XLINE
#define CONF_XLINE		0x800000
#endif
#define	CONF_OPS		CONF_OPERATOR
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER |\
				 CONF_ZCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_SERVICE | CONF_OPS | \
				 CONF_SERVER_MASK)

#define CFLAG_RESTRICTED	0x00001
#define CFLAG_RNODNS		0x00002
#define CFLAG_RNOIDENT		0x00004
#define CFLAG_KEXEMPT		0x00008
#define CFLAG_NORESOLVE		0x00010
#define CFLAG_FALL		0x00020
#define CFLAG_NORESOLVEMATCH	0x00040
#ifdef XLINE
#define CFLAG_XEXEMPT		0x00080
#endif

#define IsConfRestricted(x)	((x)->flags & CFLAG_RESTRICTED)
#define IsConfRNoDNS(x)		((x)->flags & CFLAG_RNODNS)
#define IsConfRNoIdent(x)	((x)->flags & CFLAG_RNOIDENT)
#define IsConfKlineExempt(x)	((x)->flags & CFLAG_KEXEMPT)
#define IsConfNoResolve(x)	((x)->flags & CFLAG_NORESOLVE)
#define IsConfNoResolveMatch(x)	((x)->flags & CFLAG_NORESOLVEMATCH)
#define IsConfFallThrough(x)	((x)->flags & CFLAG_FALL)
#ifdef XLINE
#define IsConfXlineExempt(x)	((x)->flags & CFLAG_XEXEMPT)
#endif

#define PFLAG_DELAYED		0x00001
#define PFLAG_SERVERONLY	0x00002

#define IsConfDelayed(x)	((x)->flags & PFLAG_DELAYED)
#define IsConfServeronly(x)	((x)->flags & PFLAG_SERVERONLY)

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
	Bytef		outbuf[ZIP_MAXIMUM]; /* outgoing (unzipped) buffer */
	int		outcount;	/* size of outbuf content */
};
#endif

struct LineItem
{ 
    char    *line;
    struct  LineItem *next;
};

/*
 * Client structures
 */
struct	User	{
	Link	*channel;	/* chain of channel pointer blocks */
	invLink	*invited;	/* chain of invite pointer blocks */
	Link	*uwas;		/* chain of whowas pointer blocks */
	char	*away;		/* pointer to away message */
	time_t	last;		/* "idle" time */
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
				** I think it's not true anymore --Beeth
				*/
	u_int	hashv;
	aClient	*uhnext;
	aClient	*bcptr;
	char	username[USERLEN+1];
	char	uid[UIDLEN+1];
	char	host[HOSTLEN+1];
	char	*server;
	u_int	hhashv;		/* hostname hash value */
	u_int	iphashv;	/* IP hash value */
	struct User *hhnext;	/* next entry in hostname hash */
	struct User *iphnext;	/* next entry in IP hash */
				/* sip MUST be the last in this struct!!! */
	char	sip[1];		/* ip as a string, big enough for ipv6
				 * allocated to real size in make_user */

};

struct	Server	{
	char	namebuf[HOSTLEN+1];
	anUser	*user;		/* who activated this connection */
	aClient	*up;		/* uplink for this server */
	aConfItem *nline;	/* N-line pointer for this server */
	int	version;        /* version id for local client */
	int	snum;
	int	refcnt;		/* Number of times this block is referenced
				** from anUser (field servp), aService (field
				** servp) and aClient (field serv) */
	int	usercnt[3];	/* # of clients - visible, invisible, opers */
	struct	Server	*nexts, *prevs, *shnext;
	aClient	*bcptr;
	aClient	*maskedby;	/* Pointer to server masking this server.
				** Self if not masked, *NEVER* NULL. */
	char	by[NICKLEN+1];
	char	byuid[UIDLEN + 1];
	char	sid[SIDLEN + 1];/* The Server ID. */
	char	verstr[11];	/* server version, PATCHLEVEL format */
	u_int	sidhashv;	/* Raw hash value. */
	aServer	*sidhnext;	/* Next server in the sid hash. */
	time_t	lastload;	/* penalty like counters, see s_serv.c
				** should be in the local part, but.. */
	int	servers;	/* Number of downlinks of this server. */
	aClient	*left, *right;	/* Left and right nodes in server tree. */
	aClient	*down;		/* Ptr to first downlink of this server. */
};

struct	Service	{
	char	namebuf[HOSTLEN+1];
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
	char	*name;		/* Pointer to unique name of the client */
	char	namebuf[NICKLEN+1]; /* nick of the client */
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
	long	receiveM;	/* Statistics: protocol messages received */
	unsigned long long	sendB;		/* Statistics: total bytes send */
	unsigned long long	receiveB;	/* Statistics: total bytes received */
	time_t	lasttime;	/* last time we received data */
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	int	ping;
	int	authfd;		/* fd for rfc931 authentication */
	char	*auth;
	u_short	port;		/* and the remote port# too :-) */
	struct	IN_ADDR	ip;	/* keep real ip# too */
	struct	hostent	*hostp;
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				  ** and after which the connection was
				  ** accepted.
				  */
	char	passwd[PASSWDLEN+1];
	char	exitc;
	char	*reason;	/* additional exit message */
#ifdef XLINE
	/* Those logically should be in anUser struct, but would be null for
	** all remote users... so better waste two pointers for all local
	** non-users than two pointers for all remote users. --B. */
	char	*user2;	/* 2nd param of USER */
	char	*user3;	/* 3rd param of USER */
#endif

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
	unsigned long long	is_cbs;	/* bytes sent to clients */
	unsigned long long	is_cbr;	/* bytes received to clients */
	unsigned long long	is_sbs;	/* bytes sent to servers */
	unsigned long long	is_sbr;	/* bytes received to servers */
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
	u_int	is_save; /* number of saved clients */
	u_int	is_fake; /* MODE 'fakes' */
	u_int	is_reop; /* number of local reops */
	u_int	is_rreop; /* number of remote reops */
	u_int	is_asuc; /* successful auth requests */
	u_int	is_abad; /* bad auth requests */
	u_int	is_udpok;	/* packets recv'd on udp port */
	u_int	is_udperr;	/* packets recvfrom errors on udp port */
	u_int	is_udpdrop;	/* packets recv'd but dropped on udp port */
	u_int	is_loc;	/* local connections made */
	u_int	is_nosrv; /* user without server */
	u_long	is_wwcnt; /* number of nicks overwritten in whowas[] */
	unsigned long long	is_wwt;	/* sum of elapsed time on when 
					** overwriting whowas[] */
	u_long	is_wwMt;  /* max elapsed time on when overwriting whowas[] */
	u_long	is_wwmt;  /* min elapsed time on when overwriting whowas[] */
	u_long	is_lkcnt; /* number of nicks overwritten in locked[] */
	unsigned long long	is_lkt;	/* sum of elapsed time on when
					** overwriting locked[]*/
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

typedef	int	(*CmdHandler)(aClient *, aClient *, int, char **);

struct	Cmd {
	CmdHandler	handler;	/* command */
	u_int		count;		/* total count */
	u_int		rcount;		/* remote count */
	u_long		bytes;
	u_long		rbytes;
};

struct	Message	{
	char	*cmd;
	int	minparams;
	int	maxparams;
	struct Cmd	handlers[STAT_MAX];
};

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
		aListItem *alist;
		char	*cp;
		int	i;
	} value;
	int	flags;
};

/* link structure used for invites */

struct	invSLink	{
	struct	invSLink	*next;
	aChannel		*chptr;
	char			*who;
	int			flags;
};

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	u_int	hashv;		/* raw hash value */
	Mode	mode;
	char	topic[TOPICLEN+1];
#ifdef TOPIC_WHO_TIME
	char	topic_nuh[BANLEN+1];
	time_t	topic_t;
#endif
	int	users;		/* current membership total */
	Link	*members;	/* channel members */
	Link	*invites;	/* outstanding invitations */
	Link	*mlist;		/* list of extended modes: +b/+e/+I */
	Link	*clist;		/* list of local! connections which are members */
	time_t	history;	/* channel history (aka channel delay) */
	time_t	reop;		/* server reop stamp for !channels */
#ifdef JAPANESE
	int flags;
#endif
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
#define	CHFL_REOPLIST	0x0040 /* reoplist channel flag */

/* Channel Visibility macros */

#define	MODE_UNIQOP	CHFL_UNIQOP
#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_PRIVATE	0x00008
#define	MODE_SECRET	0x00010
#define	MODE_MODERATED  0x00020
#define	MODE_TOPICLIMIT 0x00040
#define	MODE_INVITEONLY 0x00080
#define	MODE_NOPRIVMSGS 0x00100
#define	MODE_KEY	0x00200
#define	MODE_BAN	0x00400
#define	MODE_LIMIT	0x00800
#define	MODE_ANONYMOUS	0x01000
#define	MODE_QUIET	0x02000
#define	MODE_EXCEPTION	0x04000
#define	MODE_INVITE	0x08000
#define	MODE_REOP	0x10000
#define	MODE_REOPLIST	0x20000
#define	MODE_FLAGS	0x3ffff
/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS	(MODE_UNIQOP|MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY\
			 |MODE_LIMIT|MODE_INVITE|MODE_EXCEPTION|MODE_REOPLIST)
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
# define	IsChannelName(n)	((n) && (*(n) == '#' || *(n) == '&' ||\
					*(n) == '+' || \
					(*(n) == '!' && cid_ok(n, CHIDLEN))))
#define	IsQuiet(x)		((x)->mode.mode & MODE_QUIET)
#define	UseModes(n)		((n) && (*(n) == '#' || *(n) == '&' || \
					 *(n) == '!'))

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))
#define	BadTo(x) (BadPtr((x)) ? "*" : (x))

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyPerson(x)			(MyConnect(x) && IsPerson(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))
#define	MyService(x)			(MyConnect(x) && IsService(x))
#define	ME	me.name
#define	MES	me.serv->sid

#define	GotDependantClient(x)	(x->prev &&				\
		 		 ((IsRegisteredUser(x->prev) &&		\
				  x->prev->user->servp == x->serv) ||	\
				  (IsService(x->prev) &&		\
				  x->prev->service->servp == x->serv)))

#define	IsMasked(x)		(x && x->serv && x->serv->maskedby != x)

#define IsSplit()		(iconf.split > 0)

typedef	struct	{
	u_long	is_user[2];	/* users, non[0] invis and invis[1] */
	u_long	is_serv;	/* servers */
	u_long	is_eobservers;  /* number of servers which sent EOB */
	u_long	is_masked;	/* masked servers. */
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
	u_long	is_m_users;	/* maximum users connected */
	time_t	is_m_users_t;	/* timestamp of last maximum users */
	u_long	is_m_serv;	/* maximum servers connected */
	u_long	is_m_service;	/* maximum services connected */
	u_long	is_m_myclnt;	/* maximum local clients */
	time_t	is_m_myclnt_t;	/* timestamp of last maximum local clients */
	u_long	is_m_myserv;	/* maximum local servers */
	u_long	is_m_myservice;	/* maximum local services */
	u_long	is_l_myclnt;	/* last local user count */
	time_t	is_l_myclnt_t;	/* timestamp for last count */
#ifdef DELAY_CLOSE
	u_long	is_delayclose;	/* number of fds that got delayed close() */
	u_int	is_delayclosewait;	/* number of fds that wait for delayed close() */
#endif
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

/* used when sending to $#mask or $$mask */

#define	MATCH_SERVER	1
#define	MATCH_HOST	2

/* used for sendto_serv */

#define	SV_OLD		0x0000
#define SV_UID		0x0001
#define	SV_2_11		SV_UID

/* used for sendto_flag */

typedef	struct	{
	int	svc_chan;
	char	*svc_chname;
	struct	Channel	*svc_ptr;
	int	fd;
}	SChan;

typedef enum ServerChannels {
	SCH_ERROR,
	SCH_NOTICE,
	SCH_KILL,
	SCH_CHAN,
	SCH_NUM,
	SCH_SERVER,
	SCH_HASH,
	SCH_LOCAL,
	SCH_SERVICE,
	SCH_DEBUG,
	SCH_AUTH,
	SCH_SAVE,
	SCH_WALLOP,
#ifdef CLIENTS_CHANNEL
	SCH_CLIENT,
#endif
	SCH_OPER,
	SCH_MAX	
} ServerChannels;

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* Client exit codes for log file */
#define EXITC_UNDEF	'-'	/* unregistered client */
#define EXITC_REG	'0'	/* normal exit */
#define EXITC_AUTHFAIL	'A'	/* Authentication failure (iauth problem) */
#define EXITC_AUTHTOUT	'a'	/* Authentication time out */
#define EXITC_CLONE	'C'	/* CLONE_CHECK */
#define EXITC_DIE	'd'	/* server died */
#define EXITC_DEAD	'D'	/* socket died */
#define EXITC_ERROR	'E'	/* socket error */
#define EXITC_FLOOD	'F'	/* client flooding */
#define EXITC_FAILURE	'f'	/* connect failure */
#define EXITC_GHMAX	'G'	/* global clients per host max limit */
#define EXITC_GUHMAX	'g'	/* global clients per user@host max limit */
#define EXITC_NOILINE	'I'	/* No matching I:line */
#define EXITC_KLINE	'k'	/* K-lined */
#define EXITC_KILL	'K'	/* KILLed */
#define EXITC_LHMAX	'L'	/* local clients per host max limit */
#define EXITC_LUHMAX	'l'	/* local clients per user@host max limit */
#define EXITC_MBUF	'M'	/* mem alloc error */
#define EXITC_PING	'P'	/* ping timeout */
#define EXITC_BADPASS	'p'	/* bad password */
#define EXITC_SENDQ	'Q'	/* send queue exceeded */
#define EXITC_REF	'R'	/* Refused */
#ifdef TKLINE
#define EXITC_TKLINE	't'	/* tkline */
#endif
#define EXITC_AREF	'U'	/* Unauthorized by iauth */
#define EXITC_AREFQ	'u'	/* Unauthorized by iauth, be quiet */
#define EXITC_VIRUS	'v'	/* joined a channel used by PrettyPark virus */
#ifdef XLINE
#define EXITC_XLINE	'X'	/* Forbidden GECOS */
#endif
#define EXITC_YLINEMAX	'Y'	/* Y:line max clients limit */

/* eXternal authentication slave OPTions */
#define	XOPT_REQUIRED	0x01	/* require authentication be done by iauth */
#define	XOPT_NOTIMEOUT	0x02	/* disallow iauth time outs */
#define XOPT_EXTWAIT	0x10	/* extend registration ping timeout */
#define XOPT_EARLYPARSE	0x20	/* allow early parsing and send USER/PASS
				   information to iauth */

/* misc defines */

#define	FLUSH_BUFFER	-2
#define	UTMP		"/etc/utmp"
#define	COMMA		","

#define	SAP	struct SOCKADDR *

/* safety checks */
#if ! (UIDLEN <= NICKLEN)
#   error UIDLEN must not be bigger than NICKLEN
#endif
#if ! (UIDLEN > SIDLEN)
#   error UIDLEN must be bigger than SIDLEN
#endif
#if (NICKLEN < LOCALNICKLEN)
#   error LOCALNICKLEN must not be bigger than NICKLEN
#endif

/*
 * base for channel IDs and UIDs
 */
#define CHIDNB 36


/* Defines used for SET command */
#define TSET_ACONNECT 0x001
#define TSET_POOLSIZE 0x002
#define TSET_CACCEPT 0x004
#define TSET_SPLIT 0x008
#define TSET_SHOWALL (int) ~0

/* Runtime configuration structure */
typedef struct
{
	int aconnect;	/* 0 - OFF 1 - ON */
	int split;	/* 0 - NO 1 - YES */
	int split_minservers;
	int split_minusers;
	int caccept;	/* 0 - OFF 1 - ON 2 - SPLIT */
} iconf_t;

/* O:line flags, used also in is_allowed() */
#define ACL_LOCOP		0x00001
#define ACL_KILLLOCAL		0x00002
#define ACL_KILLREMOTE		0x00004
#define ACL_KILL		(ACL_KILLLOCAL|ACL_KILLREMOTE)
#define ACL_SQUITLOCAL		0x00008
#define ACL_SQUITREMOTE		0x00010
#define ACL_SQUIT		(ACL_SQUITLOCAL|ACL_SQUITREMOTE)
#define ACL_CONNECTLOCAL	0x00020
#define ACL_CONNECTREMOTE	0x00040
#define ACL_CONNECT		(ACL_CONNECTLOCAL|ACL_CONNECTREMOTE)
#define ACL_CLOSE		0x00080
#define ACL_HAZH		0x00100
#define ACL_DNS			0x00200
#define ACL_REHASH		0x00400
#define ACL_RESTART		0x00800
#define ACL_DIE			0x01000
#define ACL_SET			0x02000
#define ACL_TKLINE		0x04000
#define ACL_UNTKLINE		ACL_TKLINE
#define ACL_CLIENTS		0x08000
#define ACL_CANFLOOD		0x10000
#define ACL_NOPENALTY		0x20000
#define ACL_TRACE		0x40000
#define ACL_KLINE		0x80000
#define ACL_SIDTRACE		0x100000

#define ACL_ALL_REMOTE		(ACL_KILLREMOTE|ACL_SQUITREMOTE|ACL_CONNECTREMOTE)
#define ACL_ALL			0x1FFFFF

#ifdef CLIENTS_CHANNEL
/* information scope of &CLIENTS channel. */
#define CCL_CONN     0x01	/* connections */
#define CCL_CONNINFO 0x02	/* if connections, then with realname */
#define CCL_QUIT     0x04	/* quits */
#define CCL_QUITINFO 0x08	/* if quits, then with quit message */
#define CCL_NICK     0x10	/* nick changes */
#endif

