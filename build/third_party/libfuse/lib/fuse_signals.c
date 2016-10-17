/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse/fuse_lowlevel.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>

static struct fuse_session *fuse_instance;

static void exit_handler(int sig)
{
	(void) sig;
	if (fuse_instance)
		fuse_session_exit(fuse_instance);
}

static int set_one_signal_handler(int sig, void (*handler)(int), int remove)
{
	struct sigaction sa;
	struct sigaction old_sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = remove ? SIG_DFL : handler;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;

	if (sigaction(sig, NULL, &old_sa) == -1) {
		perror("fuse: cannot get old signal handler");
		return -1;
	}

	if (old_sa.sa_handler == (remove ? handler : SIG_DFL) &&
	    sigaction(sig, &sa, NULL) == -1) {
		perror("fuse: cannot set signal handler");
		return -1;
	}
	return 0;
}

int fuse_set_signal_handlers(struct fuse_session *se)
{
	if (set_one_signal_handler(SIGHUP, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGINT, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGTERM, exit_handler, 0) == -1 ||
	    set_one_signal_handler(SIGPIPE, SIG_IGN, 0) == -1)
		return -1;

	fuse_instance = se;
	return 0;
}

void fuse_remove_signal_handlers(struct fuse_session *se)
{
	if (fuse_instance != se)
		fprintf(stderr,
			"fuse: fuse_remove_signal_handlers: unknown session\n");
	else
		fuse_instance = NULL;

	set_one_signal_handler(SIGHUP, exit_handler, 1);
	set_one_signal_handler(SIGINT, exit_handler, 1);
	set_one_signal_handler(SIGTERM, exit_handler, 1);
	set_one_signal_handler(SIGPIPE, SIG_IGN, 1);
}

void sighandler_wrapper(int signum)
{
	void (*actual_routine)(int);

	actual_routine = (void *) pthread_getspecific(sighandler_key);
	if (actual_routine == NULL) {
		printf("No routine specified for signal %d\n", signum);
		return;
	}
	actual_routine(signum);
}
/* Initiate sighandler_key for signal handling, and hook
the signal handler wrapper to SIGUSR2 */
void sighandler_initonce(void)
{
	struct sigaction actions;

	(void) pthread_key_create(&sighandler_key, NULL);

	memset(&actions, 0, sizeof(actions));
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;
	actions.sa_handler = sighandler_wrapper;
	sigaction(SIGUSR2,&actions,NULL);
}
void sighandler_init(void (*handler_ftn)(int))
{
	(void) pthread_once(&sighandler_key_once, sighandler_initonce);
	pthread_setspecific(sighandler_key, (void *) handler_ftn);
}
