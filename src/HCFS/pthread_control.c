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

/**
 * Wrapper for signal handling in PTHREAD routines. Specific signal
 * handlers can be set via pthread_setspecific(PTHREAD_sighandler_key, routine).
 *
 * @param signum Indicating which signal is interrupting.
 * @return None
 */ 
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

/**
 * One-shot init routine for PTHREAD signal handling. Will be called from
 * PTHREAD_sighandler_init. Targeted signal is SIGUSR2.
 *
 * @return None
 */ 
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

/**
 * Routine for setting the signal handler of SIGUSR2 for the current thread.
 *
 * @param handler_ftn Customized signal handling routine for SIGUSR2.
 * @return None
 */ 
void PTHREAD_sighandler_init(void (*handler_ftn)(int))
{
	(void) pthread_once(&PTHREAD_sighandler_key_once,
	                    PTHREAD_sighandler_initonce);
	pthread_setspecific(PTHREAD_sighandler_key, (void *) handler_ftn);
}

/**
 * PTHREAD wrapper for running the actual routine started by PTHREAD_create.
 *
 * @param thread_ptr Pointer to PTHREAD_T structure of this thread.
 * @return None
 */ 
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

/**
 * Start a new thread wrapped by PTHREAD_T structure. The thread
 * can be interrupted by a customized signal handler for SIGUSR2,
 * allowing different signal handling of SIGUSR2 on concurrent
 * threads of the same process.
 * The new child thread will inherit PTHREAD sighandler if one is set.
 *
 * @param thread Pointer to PTHREAD_T structure of this thread.
 * @param attr Same as the second parameter pf pthread_create.
 * @param start_routine Same as the third parameter of pthread_create.
 * @param arg Same as the fourth parameter of pthread_create.
 * @return Result of PTHREAD creation (see pthread_create).
 */ 
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

/**
 * Signal handler for killing a thread immediately if the PTHREAD
 * structure is marked as cancelable, or mark the thread as
 * terminating so that it can be terminated later.
 * Use this only for setting exit handling for PTHREAD_T.
 *
 * @param sig Not used.
 * @return None
 */ 
void PTHREAD_exit_handler(_UNUSED int sig)
{
	PTHREAD_T *calling_ptr
		= (PTHREAD_T *) pthread_getspecific(PTHREAD_status_key);

	if (calling_ptr == NULL)
		return;

	write_log(10, "Thread cancelable: %d\n", calling_ptr->cancelable);
	if (calling_ptr->cancelable != 0)
		pthread_exit(0);
	else
		calling_ptr->terminating = 1;
}

/**
 * Signal handler for killing a thread immediately if the PTHREAD
 * structure is marked as cancelable, or mark the thread as
 * terminating so that it can be terminated later.
 * Use this only for setting exit handling for PTHREAD_REUSE_T.
 *
 * @param sig Not used.
 * @return None
 */ 
void PTHREAD_REUSE_exit_handler(_UNUSED int sig)
{
	PTHREAD_REUSE_T *calling_ptr
		= (PTHREAD_REUSE_T *) pthread_getspecific(PTHREAD_status_key);

	if (calling_ptr == NULL)
		return;

	if (calling_ptr->cancelable != 0)
		pthread_exit(0);
	else
		calling_ptr->terminating = 1;
}

/**
 * Shortcut for setting exit handler for PTHREAD_T.
 * This should be called before PTHREAD_create is called,
 * and only needs to be called once in the parent thread if
 * PTHREAD_create is called multiple times in that thread.
 *
 * @return None
 */ 
void PTHREAD_set_exithandler()
{
	PTHREAD_sighandler_init(&PTHREAD_exit_handler);
}

/**
 * Shortcut for setting exit handler for PTHREAD_REUSE_T.
 * This should be called before PTHREAD_REUSE_create is called,
 * and only needs to be called once in the parent thread if
 * PTHREAD_REUSE_create is called multiple times in that thread.
 *
 * @return None
 */ 
void PTHREAD_REUSE_set_exithandler()
{
	PTHREAD_sighandler_init(&PTHREAD_REUSE_exit_handler);
}

int PTHREAD_kill(PTHREAD_T *thread, int sig)
{
	return pthread_kill(thread->self, sig);
}
int PTHREAD_join(PTHREAD_T *thread, void **retval)
{
	return pthread_join(thread->self, retval);
}

void *PTHREAD_REUSE_wrapper(void *thread_ptr)
{
	PTHREAD_REUSE_T *this_thread;
	sigset_t sigset;

	this_thread = (PTHREAD_REUSE_T *) thread_ptr;

	PTHREAD_sighandler_init(this_thread->SIGUSR2_handler);
	pthread_setspecific(PTHREAD_status_key, thread_ptr);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
	while (this_thread->terminating == FALSE) {
		this_thread->cancelable = 1;
		if (this_thread->terminating == TRUE)
			pthread_exit(0);
		sem_wait(&(this_thread->run));
		this_thread->thread_routine(this_thread->arg);
		/* If this thread is detachable, just let the
		next round begin without join */
		if (this_thread->detachstate == PTHREAD_CREATE_DETACHED)
			sem_post(&(this_thread->occupied));
		else
			sem_post(&(this_thread->finish));
	}
	return NULL;
}

/* Routine for reusable threads */
int PTHREAD_REUSE_create(PTHREAD_REUSE_T *thread, const pthread_attr_t *attr)
{
	sigset_t sigset, oldset;
	int retval;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigset, &oldset);
	(void) pthread_once(&PTHREAD_sighandler_key_once,
	                    PTHREAD_sighandler_initonce);

	thread->SIGUSR2_handler = pthread_getspecific(PTHREAD_sighandler_key);

	thread->thread_routine = NULL;
	thread->cancelable = TRUE;
	thread->terminating = FALSE;
	thread->arg = NULL;
	/* sem_wait(&run) if thread waiting for PTHREAD_REUSE_run */
	sem_init(&(thread->run), 0, 0);
	/* sem_post(&finish) if thread finished the current task */
	sem_init(&(thread->finish), 0, 0);
	/* sem_wait(&occupied) in REUSE_run, and sem_post(&occupied)
	in REUSE_join */
	sem_init(&(thread->occupied), 0, 1);
	if (attr != NULL)
		pthread_attr_getdetachstate(attr, &(thread->detachstate));
	else
		thread->detachstate = PTHREAD_CREATE_JOINABLE;
	write_log(8, "Reusable thread detachstate status is %s\n",
	          (thread->detachstate == PTHREAD_CREATE_JOINABLE ?
	           "joinable" : "detachable"));
	retval = pthread_create(&(thread->self), NULL, PTHREAD_REUSE_wrapper,
	                        (void *)thread);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);

	return retval;
}

/* REUSE_run will use sem_post(&run) to continue running the thread */
int PTHREAD_REUSE_run(PTHREAD_REUSE_T *thread,
                      void *(*start_routine) (void *), void *arg)
{
	int32_t ret_val;

	ret_val = sem_trywait(&(thread->occupied));
	if (ret_val < 0)
		return ret_val;
	thread->thread_routine = start_routine;
	thread->arg = arg;
	sem_post(&(thread->run));
	return 0;
}

/* REUSE_join will use sem_wait(&finish) to wait for the thread to finish
the current task */
void PTHREAD_REUSE_join(PTHREAD_REUSE_T *thread)
{
	if (thread->detachstate == PTHREAD_CREATE_DETACHED)
		return;
	sem_wait(&(thread->finish));
	sem_post(&(thread->occupied));
}
void PTHREAD_REUSE_terminate(PTHREAD_REUSE_T *thread)
{
	pthread_kill(thread->self, SIGUSR2);
	/* Thread might be canceled with some other threads waiting */
	if (thread->cancelable == 1)
		sem_post(&(thread->finish));
	pthread_join(thread->self, NULL);
}
