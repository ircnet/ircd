
#ifndef	__service_include__
#define	__service_include__

#ifdef USE_SERVICES
extern	aService *make_service __P((aClient *));
extern	void	check_services_butone();
#endif

/* The different things a service can `sniff' */

#define	SERVICE_WANT_SERVICE	0x0001
#define	SERVICE_WANT_OPER	0x0002	/* operators, included in umodes too */
#define	SERVICE_WANT_UMODE	0x0004	/* user modes, iow + local modes */
#define	SERVICE_WANT_AWAY	0x0008	/* away isn't propaged anymore.. */
#define	SERVICE_WANT_KILL	0x0010
#define	SERVICE_WANT_NICK	0x0020
#define	SERVICE_WANT_USER	0x0040
#define	SERVICE_WANT_QUIT	0x0080
#define	SERVICE_WANT_SERVER	0x0100
#define	SERVICE_WANT_WALLOP	0x0200
#define	SERVICE_WANT_SQUIT	0x0400
#define	SERVICE_WANT_MODE	0x0800	/* channel modes */

#define SERVICE_WANT_ALL	0x0FFF

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
