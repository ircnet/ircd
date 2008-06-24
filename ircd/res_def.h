/*
 * $Id: res_def.h,v 1.7 2008/06/24 00:12:56 chopin Exp $
 *
 * ircd/res_def.h (C)opyright 1992 Darren Reed.
 */

#define	RES_INITLIST	1
#define	RES_CALLINIT	2
#define RES_INITSOCK	4
#define RES_INITDEBG	8
#define RES_INITCACH    16

#define MAXPACKET	1024
#define MAXALIASES	35
#define MAXADDRS	35

#define	AR_TTL		600	/* TTL in seconds for dns cache entries */

#define FLG_A_VALID		1
#ifdef INET6
#define FLG_AAAA_VALID		2
#endif
#define FLG_PTR_PEND_FWD	4
#define FLG_PTR_PEND_REV	8
#define FLG_PTR_PEND		(FLG_PTR_PEND_FWD|FLG_PTR_PEND_REV)
#define FLG_PTR_VALID		16

struct	hent {
	char	*h_name;	/* official name of host */
	char	*h_aliases[MAXALIASES];	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	/* list of addresses from name server */
	struct	IN_ADDR	h_addr_list[MAXADDRS];
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

typedef	struct	reslist {
	int	id;
	int	sent;	/* number of requests sent */
	int	srch;
	time_t	ttl;
	char	type;
	char	retries; /* retry counter */
	char	sends;	/* number of sends (>1 means resent) */
	char	resend;	/* send flag. 0 == dont resend */
	time_t	sentat;
	time_t	timeout;
	struct	IN_ADDR	addr;
	char	*name;
	struct	reslist	*next;
	Link	cinfo;
	struct	hent he;
	} ResRQ;

typedef	struct	cache {
	time_t	expireat;
	time_t	ttl;
	int	flags;
	struct	hostent	he;
	struct	cache	*hname_next, *hnum_next, *list_next;
	} aCache;

typedef struct	cachetable {
	aCache	*num_list;
	aCache	*name_list;
	} CacheTable;

/* must be a prime */
#define ARES_CACSIZE	1009
/* should be around twice smaller */
#define	MAXCACHED	512
