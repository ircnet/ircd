
/* The different things a service can `sniff' */

#define	SERVICE_WANT_SERVICE	0x00000001 /* other services signing on/off */
#define	SERVICE_WANT_OPER	0x00000002 /* operators, included in _UMODE */
#define	SERVICE_WANT_UMODE	0x00000004 /* user modes, iow + local modes */
#define	SERVICE_WANT_AWAY	0x00000008 /* away isn't propaged anymore.. */
#define	SERVICE_WANT_KILL	0x00000010 /* KILLs */
#define	SERVICE_WANT_NICK	0x00000020 /* all NICKs (new user, change) */
#define	SERVICE_WANT_USER	0x00000040 /* USER signing on */
#define	SERVICE_WANT_QUIT	0x00000080 /* all QUITs (users signing off) */
#define	SERVICE_WANT_SERVER	0x00000100 /* servers signing on */
#define	SERVICE_WANT_WALLOP	0x00000200 /* wallops */
#define	SERVICE_WANT_SQUIT	0x00000400 /* servers signing off */
#define	SERVICE_WANT_RQUIT	0x00000800 /* regular user QUITs (these which
					   are also sent between servers) */
#define	SERVICE_WANT_MODE	0x00001000 /* channel modes (not +ov) */
#define	SERVICE_WANT_CHANNEL	0x00002000 /* channel creations/destructions */
#define	SERVICE_WANT_VCHANNEL	0x00004000 /* channel joins/parts */

#define	SERVICE_WANT_ERRORS	0x01000000 /* &ERRORS */
#define	SERVICE_WANT_NOTICES	0x02000000 /* &NOTICES */
#define	SERVICE_WANT_LOCAL	0x04000000 /* &LOCAL */
#define	SERVICE_WANT_NUMERICS	0x08000000 /* &NUMERICS */

#define	SERVICE_WANT_USERLOG	0x10000000 /* FNAME_USERLOG */
#define	SERVICE_WANT_CONNLOG	0x20000000 /* FNAME_CONNLOG */

/* masks */
#define	SERVICE_MASK_GLOBAL	0x00007000 /*for these,service must be global*/
#define	SERVICE_MASK_PREFIX	0x00000FFF /* these actions have a prefix */
#define	SERVICE_MASK_ALL	0x3F007FFF /* all possible actions */
#define	SERVICE_MASK_NUM	(SERVICE_WANT_NICK|SERVICE_WANT_USER|\
				 SERVICE_WANT_UMODE)

/* options */
#define	SERVICE_WANT_PREFIX	0x00010000 /* to receive n!u@h instead of n */
#define	SERVICE_WANT_TOKEN	0x00020000 /* use serv token instead of name */
#define	SERVICE_WANT_EXTNICK	0x00040000 /* user extended NICK syntax */

/* A couple example types of services */
#define	SERVICE_ALL	SERVICE_MASK_ALL	/* 4095 */
#define	SERVICE_NICK	SERVICE_WANT_NICK | \
			SERVICE_WANT_QUIT | \
			SERVICE_WANT_AWAY	/* 168 */
#define	SERVICE_USERS	SERVICE_WANT_NICK | \
			SERVICE_WANT_USER | \
			SERVICE_WANT_QUIT | \
			SERVICE_WANT_AWAY | \
			SERVICE_WANT_UMODE	/* 236 */
#define	SERVICE_LINKS	SERVICE_WANT_SERVER | \
			SERVICE_WANT_SQUIT | \
			SERVICE_WANT_WALLOP	/* 1792 */
