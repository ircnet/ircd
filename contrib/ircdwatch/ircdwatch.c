/*
 *   Copyright (C) 1998 Bjorn Borud <borud@guardian.no>
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
 */

#ifndef lint
static  char rcsid[] = "@(#)$Id: ircdwatch.c,v 1.2 1998/08/05 01:47:07 kalt Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>     /* atol() */
#include <unistd.h>     /* fork() exec() */
#include <sys/types.h>
#include <sys/stat.h>   /* stat() */
#include <signal.h>
#include <syslog.h>
#include <string.h>     /* strncmp() */
#include <time.h>

#include "os.h"
#include "config.h"

/* 
 * Try and find the correct name to use with getrlimit() for setting
 * the max.  number of files allowed to be open by this process. 
 * (borrowed from ircd/s_bsd.c)
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif

#define PID_LEN 7 /* overkill, but hey */
#define MAX_OPEN_FILEDESCRIPTORS_WILD_GUESS 256

static int want_to_quit = 0;

void finalize(int i)
{
#ifdef IRCDWATCH_USE_SYSLOG
  syslog(LOG_NOTICE, "ircdwatch daemon exiting");
  closelog();
#endif
  exit(i);
}

int daemonize(void)
{
  pid_t pid;
  int   i;
  int   open_max;

#ifdef RLIMIT_FD_MAX
  struct rlimit rlim;

  if (getrlimit(RLIMIT_FD_MAX, &rlim) != 0) {
    perror("getrlimit");
    finalize(1);
  }

  /* if we get a lame answer we just take a wild guess */
  if (rlim.rlim_max == 0) {
    open_max = MAX_OPEN_FILEDESCRIPTORS_WILD_GUESS;
  }

  open_max = rlim.rlim_max;

#else

  /* no RLIMIT_FD_MAX?  take a wild guess */
  open_max = MAX_OPEN_FILEDESCRIPTORS_WILD_GUESS;

#endif

  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(1);
  }

  /* parent process dies */
  if (pid != 0) {
    exit(0);
  }

  setsid();
  chdir(DPATH);
  umask(0);

  /* close file descriptors */
  for (i=0; i < open_max; i++) {
    close(i);
  }

  return(0);
}

static void sig_handler (int signo) 
{
  if (signo == SIGHUP) {
    want_to_quit = 1;
    return;
  }

  if (signo == SIGALRM) {

#ifndef POSIX_SIGNALS
    (void)signal(SIGALRM, &sig_handler);
#endif;

    return;
  }
}


void set_up_signals(void) 
{
#ifdef POSIX_SIGNALS
  struct sigaction act;

  act.sa_handler = sig_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction(SIGHUP, &act, NULL) < 0) {
    perror("sigaction");
  }

  if (sigaction(SIGALRM, &act, NULL) < 0) {
    perror("sigaction");
  }
#else
  (void)signal(SIGHUP, &sig_handler);
  (void)signal(SIGALRM, &sig_handler);
#endif

  /* ignore it if child processes die */
  signal(SIGCHLD, SIG_IGN);
}

int write_my_pid(void)
{
  FILE *f;

  f = fopen(IRCDWATCH_PID_FILENAME, "w");
  if (f == NULL) {
    return(-1);
  }

  fprintf(f, "%d\n", getpid());
  fclose(f);

  return(0);
}


int file_modified(char *s)
{
  struct stat st;
  
  if (stat(s, &st) < 0) {
    return(-1);
  }
  return(st.st_ctime);
}


/*
 * the only thing that needs to be in our path is the directory
 * containing the ircd binary -- if MYNAME is not fully qualified
 */
void set_path ()
{
  char newpath[FILENAME_MAX+6];

  sprintf(newpath, "PATH=%s", SERVER_BIN_DIR);
  putenv(newpath);
}


int spawn (char *cmd) 
{
  pid_t pid;

  pid = fork();

  if (pid == -1) {
#ifdef IRCDWATCH_USE_SYSLOG
    syslog(LOG_ERR, "spawn() unable to fork, errno=%d", errno);
#endif
    return(-1);
  }

  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", cmd, (char *) NULL);
    _exit(127);
  }
  return(0);
}

int read_pid(char *pid_filename) 
{
  FILE *f;
  char pidbuf[PID_LEN];
  pid_t pid;

  f = fopen(pid_filename, "r");
  if (f == NULL) {
#ifdef IRCDWATCH_USE_SYSLOG
    syslog(LOG_ERR, "unable to fopen() %s: %s", pid_filename, strerror(errno));
#endif
    return(-1);
  }

  if (fgets(pidbuf, PID_LEN, f) != NULL) {
    pid = atol(pidbuf);
  } else {
#ifdef IRCDWATCH_USE_SYSLOG
    syslog(LOG_ERR, "fgets() %s: %s", pid_filename, strerror(errno));
#endif
    fclose(f);
    return(-1);
  }

  fclose(f);
  return(pid);
}

int file_exists (char *s)
{
  struct stat st;
  if ((stat(s, &st) < 0) && (errno == ENOENT)) {
    return(0);
  }
  return(1);
}

/* yeah, I'll get around to these in some later version */

int file_readable (char *s)
{
  return(access(s, R_OK) == 0);
}

int file_writable (char *s)
{
  return(access(s, W_OK) == 0);
}

int file_executable (char *s)
{
  int rc;

  if (*s != '/')
    chdir(SERVER_BIN_DIR);
  rc = (access(s, X_OK) == 0);
  if (*s != '/')
    chdir(DPATH);
  return rc;
}

int verify_pid(int pid) 
{
  int res;

  res = kill(pid, 0);
  if (res < 0 && errno == EPERM) {
    fprintf(stderr, "Not process owner\n");
    exit(1);
  }

  return(res == 0);
}

int ircdwatch_running () {
  int pid;

  if (file_exists(IRCDWATCH_PID_FILENAME)) {
    pid = read_pid(IRCDWATCH_PID_FILENAME);
    if (pid > 0) {
      return(verify_pid(pid) == 1);
    } else {
      return(-1);
    }
  }
  return(0);
}

int ircd_running () {
  int pid;

  if (file_exists(PPATH)) {
    pid = read_pid(PPATH);
    if (pid > 0) {
      return(verify_pid(pid) == 1);
    } else {
      return(-1);
    }
  }
  return(0);
}

void hup_ircd ()
{
  int pid;
  int res;
  
  if (file_exists(PPATH)) {
    pid = read_pid(PPATH);
    if (pid > 0) {
      res = kill(pid, SIGHUP);
      if (res < 0 && errno == EPERM) {
#ifdef IRCDWATCH_USE_SYSLOG
	syslog(LOG_ERR, "not allowed to send SIGHUP to ircd");
#endif
	finalize(1);
      }
    }
  }
}


void daemon_run () 
{
  int i;
  int last_config_time = 0;

  /* is ircdwatch already running? */
  i = ircdwatch_running();
  if (i == -1) {
    /* unable to open pid file.  wrong user? */
    fprintf(stderr, "ircdwatch pid file exists but is unreadable\n");
    exit(1);
  } else if (i) {
    fprintf(stderr, "ircdwatch is already running\n");    
    exit(0);
  }

  /* is ircd running? */
  i = ircd_running();
  if (i == -1) {
    /* unable to open pid file.  wrong user? */
    fprintf(stderr, "ircdwatch pid file exists but is unreadable\n");
    exit(1);
  } else  if (!i) {
    fprintf(stderr, "ircd not running. attempting to start ircd...\n");
    if (file_exists(MYNAME)) {
      if (file_executable(MYNAME)) {
	spawn(MYNAME);
      } else {
	fprintf(stderr, "%s not executable\n", MYNAME);
	exit(1);
      }
    } else {
      fprintf(stderr, "%s does not exist in %s\n", MYNAME, DPATH);
      exit(1);      
    }
  }

  set_up_signals();
  closelog();
  /*  daemonize(); */

#ifdef IRCDWATCH_USE_SYSLOG
  openlog(IRCDWATCH_SYSLOG_IDENT, 
	  IRCDWATCH_SYSLOG_OPTIONS, 
	  IRCDWATCH_SYSLOG_FACILITY);
  
  syslog(LOG_NOTICE, "starting ircdwatch daemon");
#endif

  alarm(IRCDWATCH_POLLING_INTERVAL);
  pause();

  while (!want_to_quit) {
    if (! ircd_running() ) {

#ifdef IRCDWATCH_USE_SYSLOG
      syslog(LOG_ERR, "spawning %s", MYNAME);
#endif

      spawn(MYNAME);
    }

#ifdef IRCDWATCH_HUP_ON_CONFIG_CHANGE
    i = file_modified(CONFIGFILE);
    if (i != -1) {

      if (last_config_time == 0) {
	last_config_time = i;
      } 

      else if (i > last_config_time) {
	last_config_time = i;	
	hup_ircd();

#ifdef IRCDWATCH_USE_SYSLOG
	syslog(LOG_NOTICE, "config change, HUPing ircd");
#endif

      }
    }
#endif

    alarm(IRCDWATCH_POLLING_INTERVAL);
    pause();
  }
  return;
}

void kill_ircd ()
{
  int pid;
  int res;
  
  if (file_exists(PPATH)) {
    pid = read_pid(PPATH);
    if (pid > 0) {
      res = kill(pid, SIGTERM);
      if (res < 0) {
	perror("ircd kill");
	finalize(1);
      } else {
	fprintf(stderr, "killed ircd\n");
      }
    }
  } else {
    fprintf(stderr, "File %s does not exist\n", PPATH);
  }
}

void kill_ircdwatch ()
{
  int pid;
  int res;
  
  if (file_exists(IRCDWATCH_PID_FILENAME)) {
    pid = read_pid(IRCDWATCH_PID_FILENAME);
    if (pid > 0) {
      res = kill(pid, SIGHUP);
      if (res < 0) {
	perror("ircdwatch kill");
	finalize(1);
      } else {
	fprintf(stderr, "Sent HUP to ircdwatch.  it will die soon...\n");
      }
    }
  } else {
    fprintf(stderr, "File %s does not exist\n", IRCDWATCH_PID_FILENAME);
  }
}


void usage (void)
{
  fprintf(stderr,"\n\
Usage:\n\
  ircdwatch [--kill | --rest | --help]\n\
\n\
     --kill, stop both ircdwatch and ircd\n\
     --rest, stop ircdwatch but let ircd alone\n\
     --help, display this text\n\
\n\
%s\n", rcsid);
}

int
main (int argc, char **argv) {
  int i;

  chdir(DPATH);
  set_path();

#ifdef IRCDWATCH_USE_SYSLOG
  openlog(IRCDWATCH_SYSLOG_IDENT, 
	  IRCDWATCH_SYSLOG_OPTIONS, 
	  IRCDWATCH_SYSLOG_FACILITY);
#endif

  if (argc > 1) {
    if (strncmp(argv[1], "--rest", 6) == 0) {
      kill_ircdwatch();
      exit(0);
    }

    if (strncmp(argv[1], "--kill", 6) == 0) {
      kill_ircdwatch();
      kill_ircd();
      exit(0);
    }

    usage();
    exit(0);
  }

  daemon_run();
  finalize(0);
}
