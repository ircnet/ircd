
#ifndef	__service_include__
#define	__service_include__

#ifdef USE_SERVICES
extern	aService *make_service __P((aClient *));
#ifndef USE_STDARG
extern	void	check_services_butone();
#else
extern	void	check_services_butone(long, char *, aClient *, char *, ...);
#endif
extern	void	check_services_num __P((aClient *, char *));
#endif

/* The different things a service can `sniff' */

#define	SERVICE_WANT_SERVICE	0x00001	/* other services signing on/off */
#define	SERVICE_WANT_OPER	0x00002	/* operators, included in umodes too */
#define	SERVICE_WANT_UMODE	0x00004	/* user modes, iow + local modes */
#define	SERVICE_WANT_AWAY	0x00008	/* away isn't propaged anymore.. */
#define	SERVICE_WANT_KILL	0x00010	/* KILLs */
#define	SERVICE_WANT_NICK	0x00020	/* all NICKs (new user, change) */
#define	SERVICE_WANT_USER	0x00040	/* USER signing on */
#define	SERVICE_WANT_QUIT	0x00080	/* all QUITs (users signing off) */
#define	SERVICE_WANT_SERVER	0x00100	/* servers signing on */
#define	SERVICE_WANT_WALLOP	0x00200	/* wallops */
#define	SERVICE_WANT_SQUIT	0x00400	/* servers signing off */
#define	SERVICE_WANT_RQUIT	0x00800	/* regular user QUITs (these which
					   are also sent between servers) */
#define	SERVICE_WANT_MODE	0x01000	/* channel modes (not +ov) */
#define	SERVICE_WANT_CHANNEL	0x02000	/* channel creations/destructions */

/* masks */
#define	SERVICE_MASK_GLOBAL	0x03000	/* for these, service must be global */
#define	SERVICE_MASK_PREFIX	0x00FFF	/* these actions have a prefix */
#define	SERVICE_MASK_ALL	0x03FFF	/* all possible actions */
#define	SERVICE_MASK_NUM	(SERVICE_WANT_NICK|SERVICE_WANT_USER|\
				 SERVICE_WANT_UMODE)

/* options */
#define	SERVICE_WANT_PREFIX	0x10000	/* to receive n!u@h instead of n */
#define	SERVICE_WANT_TOKEN	0x20000	/* use server token instead of name */
#define	SERVICE_WANT_EXTNICK	0x40000	/* user extended NICK syntax */

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

#endif	/* __service_include__ */
