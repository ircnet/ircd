/************************************************************************
 *   IRC - Internet Relay Chat, include/h.h
 *   Copyright (C) 1992 Darren Reed
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
 * "h.h". - Headers file.
 *
 * Most of the externs and prototypes thrown in here to 'cleanup' things.
 * -avalon
 */

extern	time_t	nextconnect, nextdnscheck, nextping, timeofday;
extern	aClient	*client, me, *local[];
extern	aChannel *channel;
extern	struct	stats	*ircstp;
extern	int	bootopt;
#ifndef	CLIENT_COMPILE
extern	aServer	*svrtop;
extern	aService *svctop;
extern	anUser	*usrtop;
extern	istat_t istat;
extern	FdAry	fdas, fdall, fdaa;
#endif

extern	void	setup_server_channels __P((aClient *));
extern	aChannel *find_channel __P((char *, aChannel *));
extern	void	remove_user_from_channel __P((aClient *, aChannel *));
extern	void	del_invite __P((aClient *, aChannel *));
extern	void	send_user_joins __P((aClient *, aClient *));
extern	void	clean_channelname __P((char *));
extern	int	can_send __P((aClient *, aChannel *));
extern	int	is_chan_op __P((aClient *, aChannel *));
extern	int	has_voice __P((aClient *, aChannel *));
extern	int	count_channels __P((aClient *));
extern	time_t	collect_channel_garbage __P((time_t));

extern	aClient	*find_client __P((char *, aClient *));
extern	aClient	*find_name __P((char *, aClient *));
extern	aClient	*find_mask __P((char *, aClient *));
extern	aClient	*find_person __P((char *, aClient *));
extern	aClient	*find_server __P((char *, aClient *));
extern	aServer	*find_tokserver __P((int, aClient *, aClient *));
extern	aClient	*find_nickserv __P((char *, aClient *));
extern	aClient	*find_service __P((char *, aClient *));
extern	aClient	*find_userhost __P((char *, char *, aClient *, int *));

extern	int	attach_conf __P((aClient *, aConfItem *));
extern	aConfItem *attach_confs __P((aClient*, char *, int));
extern	aConfItem *attach_confs_host __P((aClient*, char *, int));
extern	int	attach_Iline __P((aClient *, struct hostent *, char *));
extern	aConfItem *conf, *find_me __P(()), *find_admin __P(());
extern	aConfItem *count_cnlines __P((Link *));
extern	void	det_confs_butmask __P((aClient *, int));
extern	int	detach_conf __P((aClient *, aConfItem *));
extern	aConfItem *det_confs_butone __P((aClient *, aConfItem *));
extern	aConfItem *find_conf __P((Link *, char*, int));
extern	aConfItem *find_conf_exact __P((char *, char *, char *, int));
extern	aConfItem *find_conf_host __P((Link *, char *, int));
extern	aConfItem *find_conf_ip __P((Link *, char *, char *, int));
extern	aConfItem *find_conf_name __P((char *, int));
extern	int	find_kill __P((aClient *, int));
extern	int	find_restrict __P((aClient *));
extern	int	rehash __P((aClient *, aClient *, int));
extern	int	initconf __P((int));
extern	int	rehashed;

extern	char	*MyMalloc __P((size_t)), *MyRealloc __P((char *, size_t));
extern	char	*debugmode, *configfile, *sbrk0;
extern	char	*getfield __P((char *));
extern	void	get_sockhost __P((aClient *, char *));
extern	char	*rpl_str __P((int, char *)), *err_str __P((int, char *));
extern	char	*strerror __P((int));
extern	int	dgets __P((int, char *, int));
extern	void	ircsprintf __P(());
extern	char	*inetntoa __P((char *)), *mystrdup __P((char *));

extern	int	dbufalloc, dbufblocks, debuglevel, errno, h_errno, poolsize;
extern	int	highest_fd, debuglevel, portnum, debugtty, maxusersperchannel;
extern	int	readcalls, udpfd, resfd;
extern	aClient	*add_connection __P((aClient *, int));
extern	int	add_listener __P((aConfItem *));
extern	void	add_local_domain __P((char *, int));
extern	int	check_client __P((aClient *));
extern	int	check_server __P((aClient *, struct hostent *, \
				    aConfItem *, aConfItem *, int));
extern	int	check_server_init __P((aClient *));
extern	int	hold_server __P((aClient *));
extern	void	close_connection __P((aClient *));
extern	void	close_listeners __P(());
extern	int	connect_server __P((aConfItem *, aClient *, struct hostent *));
extern	void	get_my_name __P((aClient *, char *, int));
extern	int	get_sockerr __P((aClient *));
extern	int	inetport __P((aClient *, char *, int));
extern	void	init_sys __P(());
extern	int	read_message __P((time_t, FdAry *));
extern	void	report_error __P((char *, aClient *));
extern	void	send_ping __P((aConfItem *));
extern	void	set_non_blocking __P((int, aClient *));
extern	void	summon __P((aClient *, char *, char *, char *));
extern	int	unixport __P((aClient *, char *, int));
extern	int	utmp_open __P(());
extern	int	utmp_read __P((int, char *, char *, char *, int));
extern	int	utmp_close __P((int));
extern	void	ircd_readtune(), ircd_writetune();
extern	char	*tunefile;

extern	void	start_auth __P((aClient *));
extern	void	read_authports __P((aClient *));
extern	void	send_authports __P((aClient *));

extern	void	restart __P((char *));
extern	void	send_channel_modes __P((aClient *, aChannel *));
extern	void	server_reboot __P(());
extern	void	terminate __P(()), write_pidfile __P(());

extern	int	send_queued __P((aClient *));
/*VARARGS2*/
extern	int	sendto_one();
/*VARARGS4*/
extern	void	sendto_channel_butone();
/*VARARGS2*/
extern	void	sendto_serv_butone();
/*VARARGS2*/
extern	void	sendto_serv_v();
/*VARARGS2*/
extern	void	sendto_common_channels();
/*VARARGS3*/
extern	void	sendto_channel_butserv();
/*VARARGS3*/
extern	void	sendto_match_servs();
/*VARARGS5*/
extern	void	sendto_match_butone();
/*VARARGS3*/
extern	void	sendto_all_butone();
/*VARARGS1*/
extern	void	sendto_ops();
/*VARARGS3*/
extern	void	sendto_ops_butone();
/*VARARGS3*/
extern	void	sendto_prefix_one();
/*VARARGS2*/
extern	void	sendto_flag();

extern	void	setup_svchans __P(());

extern	void	sendto_flog __P((char *, char *, time_t, char *, char *,
				 char *, char *));
extern	int	writecalls, writeb[];
extern	int	deliver_it __P((aClient *, char *, int));

extern	int	check_registered __P((aClient *));
extern	int	check_registered_user __P((aClient *));
extern	int	check_registered_service __P((aClient *));
extern	char	*get_client_name __P((aClient *, int));
extern	char	*get_client_host __P((aClient *));
extern	char	*my_name_for_link __P((char *, int));
extern	char	*myctime __P((time_t)), *date __P((time_t));
extern	int	exit_client __P((aClient *, aClient *, aClient *, char *));
extern	void	initstats __P(()), tstats __P((aClient *, char *));

extern	int	parse __P((aClient *, char *, char *));
extern	int	do_numeric __P((int, aClient *, aClient *, int, char **));
extern	int hunt_server __P((aClient *,aClient *,char *,int,int,char **));
extern	aClient	*next_client __P((aClient *, char *));
#ifndef	CLIENT_COMPILE
extern	int	do_nick_name __P((char *));
extern	char	*canonize __P((char *));
extern	int	m_umode __P((aClient *, aClient *, int, char **));
extern	int	m_names __P((aClient *, aClient *, int, char **));
extern	int	m_server_estab __P((aClient *));
extern	void	send_umode __P((aClient *, aClient *, int, int, char *));
extern	void	send_umode_out __P((aClient*, aClient *, int));
#endif

extern	int	numclients;
extern	void	free_client __P((aClient *));
extern	void	free_link __P((Link *));
extern	void	delist_conf __P((aConfItem *));
extern	void	free_conf __P((aConfItem *));
extern	void	free_class __P((aClass *));
extern	void	free_user __P((anUser *, aClient *));
extern	void	free_server __P((aServer *, aClient *));
extern	void	free_service __P((aClient *));
extern	Link	*make_link __P(());
extern	anUser	*make_user __P((aClient *));
extern	aConfItem *make_conf __P(());
extern	aClass	*make_class __P(());
extern	aServer	*make_server __P(());
extern	aClient	*make_client __P((aClient *));
extern	Link	*find_user_link __P((Link *, aClient *));
extern	void	add_client_to_list __P((aClient *));
extern	void	checklist __P(());
extern	void	remove_client_from_list __P((aClient *));
extern	void	initlists __P(());
extern	void	add_fd __P((int, FdAry *));
extern	int	del_fd __P((int, FdAry *));
#ifdef HUB
extern	void	add_active __P((int, FdAry *));
extern	void	decay_activity __P(());
extern	int	sort_active __P((const void *, const void *));
extern	void	build_active __P(());
#endif


extern	void	add_class __P((int, int, int, int, long));
extern	void	fix_class __P((aConfItem *, aConfItem *));
extern	long	get_sendq __P((aClient *));
extern	int	get_con_freq __P((aClass *));
extern	int	get_client_ping __P((aClient *));
extern	int	get_client_class __P((aClient *));
extern	int	get_conf_class __P((aConfItem *));
extern	void	report_classes __P((aClient *, char *));

extern	struct	hostent	*get_res __P((char *));
extern	struct	hostent	*gethost_byaddr __P((char *, Link *));
extern	struct	hostent	*gethost_byname __P((char *, Link *));
extern	void	flush_cache __P(());
extern	u_long	cres_mem __P((aClient *, char *));
extern	int	init_resolver __P((int));
extern	time_t	timeout_query_list __P((time_t));
extern	time_t	expire_cache __P((time_t));
extern	void    del_queries __P((char *));

extern	void	inithashtables __P(());
extern	int	add_to_client_hash_table __P((char *, aClient *));
extern	int	del_from_client_hash_table __P((char *, aClient *));
extern	int	add_to_server_hash_table __P((aServer *, aClient *));
extern	int	del_from_server_hash_table __P((aServer *, aClient *));
extern	int	add_to_channel_hash_table __P((char *, aChannel *));
extern	int	del_from_channel_hash_table __P((char *, aChannel *));
extern	aChannel *hash_find_channel __P((char *, aChannel *));
extern	aServer	*hash_find_stoken __P((int, aClient *, void *));
extern	aClient	*hash_find_client __P((char *, aClient *));
extern	aClient	*hash_find_nickserv __P((char *, aClient *));
extern	aClient	*hash_find_server __P((char *, aClient *));

extern	int	ww_size, lk_size;
extern	void	add_history __P((aClient *, aClient *));
extern	aClient	*get_history __P((char *, time_t));
extern	int	find_history __P((char *, time_t));
extern	void	initwhowas __P(());
extern	void	off_history __P((aClient *));

extern	int	dopacket __P((aClient *, char *, int));

#ifdef	CLIENT_COMPILE
extern	char	*mycncmp __P((char *, char *));
#endif

/*VARARGS2*/
extern	void	debug();
#if defined(DEBUGMODE) && !defined(CLIENT_COMPILE)
extern	void	send_usage __P((aClient *, char *));
extern	void	send_listinfo __P((aClient *, char *));
extern	void	checklists __P(());
extern	void	dumpcore();
/*VARARGS?*/
#endif

#ifndef CLIENT_COMPILE
extern	void	count_memory __P((aClient *, char *, int));
extern	void	send_defines __P((aClient *, char *));
#endif

#ifdef KRYS
extern	char	*find_server_string __P((int));
extern	int	find_server_num __P((char *));
#endif

extern	char	*make_version();
