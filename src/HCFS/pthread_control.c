/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pthread_control.c
* Abstract: The c source file for handling self-defined pthread structure
*
* Revision History
* 2016/10/18 Jiahong created this file.
*
**************************************************************************/

#include "pthread_control.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "logger.h"
#include "macro.h"

static pthread_key_t PTHREAD_sighandler_key;
static pthread_once_t PTHREAD_sighandler_key_once = PTHREAD_ONCE_INIT;

void PTHREAD_sighandler_wrapper(int signum)
{
	void (*actual_routine)(int);

	actual_routine = (void *) pthread_getspecific(PTHREAD_sighandler_key);
	if (actual_routine == NULL) {
		write_log(10, "No routine specified for signal %d\n", signum);
		return;
	}
	actual_routine(signum);
}

void PTHREAD_sighandler_initonce(void)
{
	struct sigaction actions;

	(void) pthread_key_create(&PTHREAD_sighandler_key, NULL);
	(void) pthread_key_create(&PTHREAD_status_key, NULL);

	memset(&actions, 0, sizeof(actions));
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;
	actions.sa_handler = PTHREAD_sighandler_wrapper;
	sigaction(SIGUSR2,&actions,NULL);
}
void PTHREAD_sighandler_init(void (*handler_ftn)(int))
{
	(void) pthread_once(&PTHREAD_sighandler_key_once,
	                    PTHREAD_sighandler_initonce);
	pthread_setspecific(PTHREAD_sighandler_key, (void *) handler_ftn);
}

void *PTHREAD_wrapper(void *thread_ptr)
{
	PTHREAD_T *this_thread;
	sigset_t sigset;

	this_thread = (PTHREAD_T *) thread_ptr;

	PTHREAD_sighandler_init(this_thread->SIGUSR2_handler);
	pthread_setspecific(PTHREAD_status_key, thread_ptr);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
	return this_thread->thread_routine(this_thread->arg);
}

int PTHREAD_create(PTHREAD_T *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
	sigset_t sigset, oldset;
	int retval;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigset, &oldset);
	(void) pthread_once(&PTHREAD_sighandler_key_once,
	                    PTHREAD_sighandler_initonce);

	thread->SIGUSR2_handler = pthread_getspecific(PTHREAD_sighandler_key);

	thread->thread_routine = start_routine;
	thread->cancelable = TRUE;
	thread->terminating = FALSE;
	thread->arg = arg;

	retval = pthread_create(&(thread->self), attr, PTHREAD_wrapper,
	                        (void *)thread);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);

	return retval;
}

void PTHREAD_exit_handler(_UNUSED int sig)
{
	PTHREAD_T *calling_ptr
		= (PTHREAD_T *) pthread_getspecific(PTHREAD_status_key);

	if (calling_ptr == NULL)
		return;

	if (calling_ptr->cancelable != 0)
		pthread_exit(0);
	else
		calling_ptr->terminating = 1;
}

void PTHREAD_set_exithandler()
{
	PTHREAD_sighandler_init(&PTHREAD_exit_handler);
}

int PTHREAD_kill(PTHREAD_T thread, int sig)
{
	return pthread_kill(thread.self, sig);
}
int PTHREAD_join(PTHREAD_T thread, void **retval)
{
	return pthread_join(thread.self, retval);
}

