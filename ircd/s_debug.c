/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_debug.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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
static  char rcsid[] = "@(#)$Id: s_debug.c,v 1.28 1999/07/11 22:11:17 kalt Exp $";
#endif

#include "os.h"
#include "s_defines.h"
#define S_DEBUG_C
#include "s_externs.h"
#undef S_DEBUG_C

/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 * spaces are not allowed.
 */
char	serveropts[] = {
#ifndef	NO_IDENT
'a',
#endif
#ifdef	CHROOTDIR
'c',
#endif
#ifdef	CMDLINE_CONFIG
'C',
#endif
#ifdef	DEBUGMODE
'D',
#endif
#ifdef	RANDOM_NDELAY
'd',
#endif
#if defined(LOCOP_REHASH) && defined(OPER_REHASH)
'e',
#endif
#ifdef	OPER_REHASH
'E',
#endif
#ifdef	SLOW_ACCEPT
'f',
#endif
#ifdef	CLONE_CHECK
'F',
#endif
#ifdef	SUN_GSO_BUG
'g',
#endif
#ifdef	HUB
'H',
#endif
#ifdef	BETTER_CDELAY
'h',
#endif
#ifdef	SHOW_INVISIBLE_LUSERS
'i',
#endif
#ifndef	NO_DEFAULT_INVISIBLE
'I',
#endif
#if defined(LOCOP_DIE) && defined(OPER_DIE)
'j',
#endif
#ifdef	OPER_DIE
'J',
#endif
#ifdef	OPER_KILL
# ifdef  LOCAL_KILL_ONLY
'k',
# else
'K',
# endif
#endif
#ifdef	LEAST_IDLE
'L',
#endif
#ifdef	M4_PREPROC
'm',
#endif
#ifdef	IDLE_FROM_MSG
'M',
#endif
#ifdef	NPATH /* gone */
'N',
#endif
#ifdef	BETTER_NDELAY
'n',
#endif
#ifdef	CRYPT_OPER_PASSWORD
'p',
#endif
#ifdef	CRYPT_LINK_PASSWORD
'P',
#endif
#if defined(LOCOP_RESTART) && defined(OPER_RESTART)
'r',
#endif
#ifdef	OPER_RESTART
'R',
#endif
#ifdef	USE_SERVICES
's',
#endif
#ifdef	ENABLE_SUMMON
'S',
#endif
#ifdef	OPER_REMOTE
't',
#endif
#ifndef	NO_PREFIX
'u',
#endif
#ifdef	ENABLE_USERS
'U',
#endif
#ifdef	VALLOC
'V',
#endif
#ifdef	NOWRITEALARM
'w',
#endif
#ifdef	UNIXPORT
'X',
#endif
#ifdef	USE_SYSLOG
'Y',
#endif
#ifdef	ZIP_LINKS
'Z',
#endif
#ifdef INET6
'6',
#endif
'\0'};

#ifdef DEBUGMODE
static	char	debugbuf[2*READBUF_SIZE]; /* needs to be big.. */

#if ! USE_STDARG
void	debug(level, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
int	level;
char	*form, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
#else
void	debug(int level, char *form, ...)
#endif
{
	int	err = errno;

#ifdef	USE_SYSLOG
	if (level == DEBUG_ERROR)
	    {
#if ! USE_STDARG
		syslog(LOG_ERR, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
# if HAVE_VSYSLOG
		va_list va;
		va_start(va, form);
		vsyslog(LOG_ERR, form, va);
		va_end(va);
# else
		va_list va;
		va_start(va, form);
		vsprintf(debugbuf, form, va);
		va_end(va);
		syslog(LOG_ERR, debugbuf);
# endif
#endif
	    }
#endif
	if ((debuglevel >= 0) && (level <= debuglevel))
	    {
#if ! USE_STDARG
		(void)sprintf(debugbuf, form,
				p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
		va_list va;
		va_start(va, form);
		(void)vsprintf(debugbuf, form, va);
		va_end(va);
#endif
		if (local[2])
		    {
			local[2]->sendM++;
			local[2]->sendB += strlen(debugbuf);
		    }
		(void)fprintf(stderr, "%s", debugbuf);
		(void)fputc('\n', stderr);
	    }
	errno = err;
}
#endif /* DEBUGMODE */

/*
 * This is part of the STATS replies. There is no offical numeric for this
 * since this isnt an official command, in much the same way as HASH isnt.
 * It is also possible that some systems wont support this call or have
 * different field names for "struct rusage".
 * -avalon
 */
void	send_usage(cptr, nick)
aClient *cptr;
char	*nick;
{
#if HAVE_GETRUSAGE
	struct	rusage	rus;
	time_t	secs, rup;
#ifdef	hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int	hzz = 1;
#  ifdef HPUX
	hzz = (int)sysconf(_SC_CLK_TCK);
#  endif
# endif
#endif

	if (getrusage(RUSAGE_SELF, &rus) == -1)
	    {
		sendto_one(cptr,":%s NOTICE %s :Getruseage error: %s.",
			   me.name, nick, sys_errlist[errno]);
		return;
	    }
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	rup = timeofday - me.since;
	if (secs == 0)
		secs = 1;

	sendto_one(cptr,
		   ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
		   me.name, RPL_STATSDEBUG, nick, secs/60, secs%60,
		   rus.ru_utime.tv_sec/60, rus.ru_utime.tv_sec%60,
		   rus.ru_stime.tv_sec/60, rus.ru_stime.tv_sec%60);
	if (rup && hzz)
		sendto_one(cptr, ":%s %d %s :RSS %d ShMem %d Data %d Stack %d",
			   me.name, RPL_STATSDEBUG, nick, rus.ru_maxrss,
			   rus.ru_ixrss / (rup * hzz),
			   rus.ru_idrss / (rup * hzz),
			   rus.ru_isrss / (rup * hzz));
	sendto_one(cptr, ":%s %d %s :Swaps %d Reclaims %d Faults %d",
		   me.name, RPL_STATSDEBUG, nick, rus.ru_nswap,
		   rus.ru_minflt, rus.ru_majflt);
	sendto_one(cptr, ":%s %d %s :Block in %d out %d",
		   me.name, RPL_STATSDEBUG, nick, rus.ru_inblock,
		   rus.ru_oublock);
	sendto_one(cptr, ":%s %d %s :Msg Rcv %d Send %d",
		   me.name, RPL_STATSDEBUG, nick, rus.ru_msgrcv, rus.ru_msgsnd);
	sendto_one(cptr, ":%s %d %s :Signals %d Context Vol. %d Invol %d",
		   me.name, RPL_STATSDEBUG, nick, rus.ru_nsignals,
		   rus.ru_nvcsw, rus.ru_nivcsw);
#else /* HAVE_GETRUSAGE */
# if HAVE_TIMES
	struct	tms	tmsbuf;
	time_t	secs, mins;
	int	hzz = 1, ticpermin;
	int	umin, smin, usec, ssec;

#  ifdef HPUX
	hzz = sysconf(_SC_CLK_TCK);
#  endif
	ticpermin = hzz * 60;

	umin = tmsbuf.tms_utime / ticpermin;
	usec = (tmsbuf.tms_utime%ticpermin)/(float)hzz;
	smin = tmsbuf.tms_stime / ticpermin;
	ssec = (tmsbuf.tms_stime%ticpermin)/(float)hzz;
	secs = usec + ssec;
	mins = (secs/60) + umin + smin;
	secs %= hzz;

	if (times(&tmsbuf) == -1)
	    {
		sendto_one(cptr, ":%s %d %s :times(2) error: %s.",
			   me.name, RPL_STATSDEBUG, nick, strerror(errno));
		return;
	    }
	secs = tmsbuf.tms_utime + tmsbuf.tms_stime;

	sendto_one(cptr,
		   ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
		   me.name, RPL_STATSDEBUG, nick, mins, secs, umin, usec,
		   smin, ssec);
# endif /* HAVE_TIMES */
#endif /* HAVE_GETRUSAGE */
	sendto_one(cptr, ":%s %d %s :DBUF alloc %d blocks %d",
		   me.name, RPL_STATSDEBUG, nick, istat.is_dbufuse,
		   istat.is_dbufnow);
#ifdef DEBUGMODE
	sendto_one(cptr, ":%s %d %s :Reads %d Writes %d",
		   me.name, RPL_STATSDEBUG, nick, readcalls, writecalls);
	sendto_one(cptr,
		   ":%s %d %s :Writes:  <0 %d 0 %d <16 %d <32 %d <64 %d",
		   me.name, RPL_STATSDEBUG, nick,
		   writeb[0], writeb[1], writeb[2], writeb[3], writeb[4]);
	sendto_one(cptr,
		   ":%s %d %s :<128 %d <256 %d <512 %d <1024 %d >1024 %d",
		   me.name, RPL_STATSDEBUG, nick,
		   writeb[5], writeb[6], writeb[7], writeb[8], writeb[9]);
#endif
	return;
}

void	send_defines(cptr, nick)
aClient *cptr;
char	*nick;
{
    	sendto_one(cptr, ":%s %d %s :HUB:%s MS:%d", 
		   ME, RPL_STATSDEFINE, nick,
#ifdef HUB
		   "yes",
#else
		   "no",
#endif
		   MAXSERVERS);
    	sendto_one(cptr,
		   ":%s %d %s :LQ:%d MXC:%d TS:%d HRD:%d HGL:%d WWD:%d ATO:%d",
		   ME, RPL_STATSDEFINE, nick, LISTENQUEUE, MAXCONNECTIONS,
		   TIMESEC, HANGONRETRYDELAY, HANGONGOODLINK, WRITEWAITDELAY,
		   ACCEPTTIMEOUT);
    	sendto_one(cptr, ":%s %d %s :KCTL:%d DCTL:%d LDCTL: %d CF:%d MCPU:%d",
		   ME, RPL_STATSDEFINE, nick, KILLCHASETIMELIMIT,
		   DELAYCHASETIMELIMIT, LDELAYCHASETIMELIMIT,
		   CLIENT_FLOOD, MAXCHANNELSPERUSER);
	sendto_one(cptr, ":%s %d %s :H:%d N:%d U:%d R:%d T:%d C:%d P:%d K:%d",
		   ME, RPL_STATSDEFINE, nick, HOSTLEN, NICKLEN, USERLEN,
		   REALLEN, TOPICLEN, CHANNELLEN, PASSWDLEN, KEYLEN);
	sendto_one(cptr, ":%s %d %s :BS:%d MXR:%d MXB:%d MXBL:%d PY:%d",
		   ME, RPL_STATSDEFINE, nick, BUFSIZE, MAXRECIPIENTS, MAXBANS,
		   MAXBANLENGTH, MAXPENALTY);
	sendto_one(cptr, ":%s %d %s :ZL:%d CM:%d CP:%d",
		   ME, RPL_STATSDEFINE, nick,
#ifdef	ZIP_LINKS
		   ZIP_LEVEL,
#else
		   -1,
#endif
#ifdef	CLONE_CHECK
		   CLONE_MAX, CLONE_PERIOD
#else
		   -1, -1
#endif
		   );
}

void	count_memory(cptr, nick, debug)
aClient	*cptr;
char	*nick;
int	debug;
{
	extern	aChannel	*channel;
	extern	aClass	*classes;
	extern	aConfItem	*conf;
	extern	int	_HASHSIZE, _CHANNELHASHSIZE;

	Reg	aClient	*acptr;
	Reg	Link	*link;
	Reg	aChannel *chptr;
	Reg	aConfItem *aconf;
	Reg	aClass	*cltmp;

	int	lc = 0, d_lc = 0,	/* local clients */
		ch = 0, d_ch = 0,	/* channels */
		lcc = 0, d_lcc = 0,	/* local client conf links */
		rc = 0, d_rc = 0,	/* remote clients */
		us = 0, d_us = 0,	/* user structs */
		chu = 0, d_chu = 0,	/* channel users */
		chi = 0, d_chi = 0,	/* channel invites */
		chb = 0, d_chb = 0,	/* channel bans */
		chh = 0, d_chh = 0,	/* channel in history */
		wwu = 0, d_wwu = 0,	/* whowas users */
		cl = 0, d_cl = 0,	/* classes */
		co = 0, d_co = 0;	/* conf lines */

	int	usi = 0, d_usi = 0,	/* users invited */
		usc = 0, d_usc = 0,	/* users in channels */
		aw = 0, d_aw = 0,	/* aways set */
		wwa = 0, d_wwa = 0,	/* whowas aways */
		wwuw = 0, d_wwuw = 0;   /* whowas uwas */

	u_long	chm = 0, d_chm = 0,	/* memory used by channels */
		chhm = 0, d_chhm = 0,	/* memory used by channel in history */
		chbm = 0, d_chbm = 0,	/* memory used by channel bans */
		lcm = 0, d_lcm = 0,	/* memory used by local clients */
		rcm = 0, d_rcm = 0,	/* memory used by remote clients */
		awm = 0, d_awm = 0,	/* memory used by aways */
		wwam = 0, d_wwam = 0,	/* whowas away memory used */
		wwm = 0, d_wwm = 0,	/* whowas array memory used */
		dm = 0, d_dm = 0,	/* delay array memory used */
		com = 0, d_com = 0,	/* memory used by conf lines */
		db = 0, d_db = 0,	/* memory used by dbufs */
		rm = 0, d_rm = 0,	/* res memory used */
		totcl = 0, d_totcl = 0,
		totch = 0, d_totch = 0,
		totww = 0, d_totww = 0,
		tot = 0, d_tot = 0;
	time_t	start = 0;

	if (debug)
	    {
		start = time(NULL);
		count_whowas_memory(&d_wwu, &d_wwa, &d_wwam, &d_wwuw);
		d_wwm = sizeof(aName) * ww_size;
		d_dm = sizeof(aLock) * lk_size;
	    }
	wwu = istat.is_wwusers;
	wwa = istat.is_wwaways;
	wwam = istat.is_wwawaysmem;
	wwuw = istat.is_wwuwas;
	wwm = sizeof(aName) * ww_size;
	dm = sizeof(aLock) * lk_size;

	/*lc = istat.is_unknown + istat.is_myclnt + istat.is_serv;*/
	lc = istat.is_localc;
	lcc = istat.is_conflink;
	rc = istat.is_remc;
	us = istat.is_users;
	usi = istat.is_useri;
	usc = istat.is_userc;
	aw = istat.is_away;
	awm = istat.is_awaymem;

	if (debug)
		for (acptr = client; acptr; acptr = acptr->next)
		    {
			if (MyConnect(acptr))
			    {
				d_lc++;
				for (link =acptr->confs; link; link=link->next)
					d_lcc++;
			}
			    else
				d_rc++;
			if (acptr->user)
			    {
				d_us++;
				for (link = acptr->user->invited; link;
				     link = link->next)
					d_usi++;
				d_usc += acptr->user->joined;
				if (acptr->user->away)
				    {
					d_aw++;
					d_awm += (strlen(acptr->user->away)+1);
				    }
			    }
		    }

	lcm = lc * CLIENT_LOCAL_SIZE;
	rcm = rc * CLIENT_REMOTE_SIZE;

	d_lcm = d_lc * CLIENT_LOCAL_SIZE;
	d_rcm = d_rc * CLIENT_REMOTE_SIZE;

	ch = istat.is_chan;
	chm = istat.is_chanmem;
	chh = istat.is_hchan;
	chhm = istat.is_hchanmem;
	chi = istat.is_invite;
	chb = istat.is_bans;
	chbm = istat.is_banmem + chb * sizeof(Link);
	chu = istat.is_chanusers;

	if (debug)
	    {
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (chptr->users == 0)
			    {
				d_chh++;
				d_chhm+=strlen(chptr->chname)+sizeof(aChannel);
			    }
			else
			    {
				d_ch++;
				d_chm += (strlen(chptr->chname) +
					  sizeof(aChannel));
			    }
			for (link = chptr->members; link; link = link->next)
				d_chu++;
			for (link = chptr->invites; link; link = link->next)
				d_chi++;
			for (link = chptr->mlist; link; link = link->next)
			    {
				d_chb++;
				d_chbm += strlen(link->value.cp) + 1;
			    }
		    }
		d_chbm += d_chb * sizeof(Link);
	    }

	co = istat.is_conf;
	com = istat.is_confmem;
	cl = istat.is_class;

	if (debug)
	    {
		for (aconf = conf; aconf; aconf = aconf->next)
		    {
			d_co++;
			d_com += aconf->host ? strlen(aconf->host)+1 : 0;
			d_com += aconf->passwd ? strlen(aconf->passwd)+1 : 0;
			d_com += aconf->name ? strlen(aconf->name)+1 : 0;
			d_com += aconf->ping ? sizeof(*aconf->ping) : 0;
			d_com += sizeof(aConfItem);
		    }
		for (cltmp = classes; cltmp; cltmp = cltmp->next)
			d_cl++;
	    }

	if (debug)
		sendto_one(cptr, ":%s %d %s :Request processed in %u seconds",
			   me.name, RPL_STATSDEBUG, nick, time(NULL) - start);

	sendto_one(cptr,
		   ":%s %d %s :Client Local %d(%d) Remote %d(%d) Auth %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, lc, lcm, rc, rcm,
		   istat.is_auth, istat.is_authmem);
	if (debug
	    && (lc != d_lc || lcm != d_lcm || rc != d_rc || rcm != d_rcm))
		sendto_one(cptr,
			":%s %d %s :Client Local %d(%d) Remote %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_lc, d_lcm, d_rc,
			   d_rcm);
	sendto_one(cptr,
		   ":%s %d %s :Users %d in/visible %d/%d(%d) Invites %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, us, istat.is_user[1],
		   istat.is_user[0], us*sizeof(anUser), usi,
		   usi*sizeof(Link));
	if (debug && (us != d_us || usi != d_usi))
		sendto_one(cptr,
			   ":%s %d %s :Users %d(%d) Invites %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_us,
			   d_us*sizeof(anUser), d_usi, d_usi * sizeof(Link));
	sendto_one(cptr, ":%s %d %s :User channels %d(%d) Aways %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, usc, usc*sizeof(Link),
		   aw, awm);
	if (debug && (usc != d_usc || aw != d_aw || awm != d_awm))
		sendto_one(cptr,
			":%s %d %s :User channels %d(%d) Aways %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_usc,
			   d_usc*sizeof(Link), d_aw, d_awm);
	sendto_one(cptr, ":%s %d %s :Attached confs %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, lcc, lcc*sizeof(Link));
	if (debug && lcc != d_lcc)
		sendto_one(cptr, ":%s %d %s :Attached confs %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_lcc,
			   d_lcc*sizeof(Link));

	totcl = lcm + rcm + us*sizeof(anUser) + usc*sizeof(Link) + awm;
	totcl += lcc*sizeof(Link) + usi*sizeof(Link);
	d_totcl = d_lcm + d_rcm + d_us*sizeof(anUser) + d_usc*sizeof(Link);
	d_totcl += d_awm + d_lcc*sizeof(Link) + d_usi*sizeof(Link);

	sendto_one(cptr, ":%s %d %s :Conflines %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, co, com);
	if (debug && (co != d_co || com != d_com))
		sendto_one(cptr, ":%s %d %s :Conflines %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_co, d_com);

	sendto_one(cptr, ":%s %d %s :Classes %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, cl, cl*sizeof(aClass));
	if (debug && cl != d_cl)
		sendto_one(cptr, ":%s %d %s :Classes %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_cl,
			   d_cl*sizeof(aClass));

	sendto_one(cptr,
   ":%s %d %s :Channels %d(%d) Modes %d(%d) History %d(%d) Cache %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, ch, chm, chb, chbm, chh,
		   chhm, istat.is_cchan, istat.is_cchanmem);
	if (debug && (ch != d_ch || chm != d_chm || chb != d_chb
		      || chbm != d_chbm || chh != d_chh || chhm != d_chhm))
		sendto_one(cptr,
	       ":%s %d %s :Channels %d(%d) Modes %d(%d) History %d(%d) [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_ch, d_chm, d_chb,
			   d_chbm, d_chh, d_chhm);
	sendto_one(cptr, ":%s %d %s :Channel members %d(%d) invite %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, chu, chu*sizeof(Link),
		   chi, chi*sizeof(Link));
	if (debug && (chu != d_chu || chi != d_chi))
		sendto_one(cptr,
		   ":%s %d %s :Channel members %d(%d) invite %d(%d) [REAL]",
		   me.name, RPL_STATSDEBUG, nick, d_chu, d_chu*sizeof(Link),
		   d_chi, d_chi*sizeof(Link));

	totch = chm + chhm + chbm + chu*sizeof(Link) + chi*sizeof(Link);
	d_totch = d_chm + d_chhm + d_chbm + d_chu*sizeof(Link)
		  + d_chi*sizeof(Link);

	sendto_one(cptr,
		   ":%s %d %s :Whowas users %d(%d) away %d(%d) links %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, wwu, wwu*sizeof(anUser),
		   wwa, wwam, wwuw, wwuw*sizeof(Link));
	if (debug && (wwu != d_wwu || wwa != d_wwa || wwam != d_wwam
		      || wwuw != d_wwuw))
		sendto_one(cptr,
	     ":%s %d %s :Whowas users %d(%d) away %d(%d) links %d(%d) [REAL]",
		   me.name, RPL_STATSDEBUG, nick, d_wwu, d_wwu*sizeof(anUser),
		   d_wwa, d_wwam, d_wwuw, d_wwuw*sizeof(Link));
	sendto_one(cptr, ":%s %d %s :Whowas array %d(%d) Delay array %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, ww_size, wwm, lk_size, dm);
	if (debug && (wwm != d_wwm || dm != d_dm))
		sendto_one(cptr,
		   ":%s %d %s :Whowas array %d(%d) Delay array %d(%d) [REAL]",
		   me.name, RPL_STATSDEBUG, nick, ww_size, d_wwm, lk_size,
		   d_dm);

	totww = wwu*sizeof(anUser) + wwam + wwm;
	d_totww = d_wwu*sizeof(anUser) + d_wwam + d_wwm;

	sendto_one(cptr, ":%s %d %s :Hash: client %d(%d) chan %d(%d)",
		   me.name, RPL_STATSDEBUG, nick, _HASHSIZE,
		   sizeof(aHashEntry) * _HASHSIZE,
		   _CHANNELHASHSIZE, sizeof(aHashEntry) * _CHANNELHASHSIZE);
	d_db = db = istat.is_dbufnow * sizeof(dbufbuf);
	db = istat.is_dbufnow * sizeof(dbufbuf);
	sendto_one(cptr,
		   ":%s %d %s :Dbuf blocks %u(%d) (> %u [%u]) (%u < %u) [%u]",
		   me.name, RPL_STATSDEBUG, nick, istat.is_dbufnow, db,
		   istat.is_dbuf,
		   (u_int) (((u_int)BUFFERPOOL) / ((u_int)sizeof(dbufbuf))),
		   istat.is_dbufuse, istat.is_dbufmax, istat.is_dbufmore);

	d_rm = rm = cres_mem(cptr, nick);

	tot = totww + totch + totcl + com + cl*sizeof(aClass) + db + rm;
	tot += sizeof(aHashEntry) * _HASHSIZE;
	tot += sizeof(aHashEntry) * _CHANNELHASHSIZE;
	d_tot = d_totww + d_totch + d_totcl + d_com + d_cl*sizeof(aClass);
	d_tot += d_db + d_rm;
	d_tot += sizeof(aHashEntry) * _HASHSIZE;
	d_tot += sizeof(aHashEntry) * _CHANNELHASHSIZE;

	sendto_one(cptr, ":%s %d %s :Total: ww %d ch %d cl %d co %d db %d",
		   me.name, RPL_STATSDEBUG, nick, totww, totch, totcl, com,db);
	if (debug && tot != d_tot)
	    {
		sendto_one(cptr,
		   ":%s %d %s :Total: ww %d ch %d cl %d co %d db %d [REAL]",
		   me.name, RPL_STATSDEBUG, nick, d_totww, d_totch, d_totcl,
		   d_com, d_db);
		sendto_one(cptr, ":%s %d %s :TOTAL: %d [REAL]",
			   me.name, RPL_STATSDEBUG, nick, d_tot);
	    }
	sendto_one(cptr, ":%s %d %s :TOTAL: %d sbrk(0)-etext: %u",
		   me.name, RPL_STATSDEBUG, nick, tot,
		   (u_long)sbrk((size_t)0)-(u_long)sbrk0);
	return;
}
