
#ifndef	__service_include__
#define	__service_include__

#ifdef USE_SERVICES
extern	aService *make_service __P((aClient *));
extern	void	check_services_butone();
#endif

/* The different things a service can `sniff' */

#define	SERVICE_WANT_SERVICE	0x00001	/* other services signing on/off */
#define	SERVICE_WANT_OPER	0x00002	/* operators, included in umodes too */
#define	SERVICE_WANT_UMODE	0x00004	/* user modes, iow + local modes */
#define	SERVICE_WANT_AWAY	0x00008	/* away isn't propaged anymore.. */
#define	SERVICE_WANT_KILL	0x00010	/* KILLs */
#define	SERVICE_WANT_NICK	0x00020	/* all NICKs (new user, change) */
#define	SERVICE_WANT_USER	0x00040	/* USER signing on */
#define	SERVICE_WANT_QUIT	0x00080	/* QUITs (users signing off) */
#define	SERVICE_WANT_SERVER	0x00100	/* servers signing on */
#define	SERVICE_WANT_WALLOP	0x00200	/* wallops */
#define	SERVICE_WANT_SQUIT	0x00400	/* servers signing off */

#define	SERVICE_WANT_MODE	0x00800	/* channel modes (not +ov) */
#define	SERVICE_WANT_CHANNEL	0x01000 /* channel creations/destructions */

#define	SERVICE_WANT_PREFIX	0x10000	/* to receive n!u@h instead of n */

#define	SERVICE_WANT_GLOBAL	0x01800 /* for these, service must be global */

#define	SERVICE_WANT_ALL	0x01FFF

/* A couple example types of services */
#define	SERVICE_ALL	SERVICE_WANT_ALL	/* 4095 */
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
