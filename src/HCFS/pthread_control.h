/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pthread_control.h
* Abstract: The header file for handling self-defined pthread structure
*
* Revision History
* 2016/10/18 Jiahong created this file.
*
**************************************************************************/

#ifndef GW20_PTHREAD_CONTROL_H_
#define GW20_PTHREAD_CONTROL_H_

#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>

#include "global.h"

typedef struct {
	pthread_t self;
	BOOL cancelable;
	BOOL terminating;
	void *(*thread_routine)(void *);
	void (*SIGUSR2_handler)(int);
	void *arg;
} PTHREAD_T;

typedef struct {
	pthread_t self;
	BOOL cancelable;
	BOOL terminating;
	void *(*thread_routine)(void *);
	void (*SIGUSR2_handler)(int);
	void *arg;

	/* sem_wait(&run) if thread waiting for PTHREAD_REUSE_run */
	sem_t run;
	/* sem_post(&finish) if thread finished the current task */
	sem_t finish;
	/* sem_wait(&occupied) in REUSE_run, and sem_post(&occupied)
	in REUSE_join */
	sem_t occupied;
} PTHREAD_REUSE_T;

pthread_key_t PTHREAD_status_key;

void PTHREAD_sighandler_init(void (*handler_ftn)(int));
int PTHREAD_create(PTHREAD_T *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);
void PTHREAD_set_exithandler();
void PTHREAD_REUSE_set_exithandler();
int PTHREAD_kill(PTHREAD_T *thread, int sig);
int PTHREAD_join(PTHREAD_T *thread, void **retval);

/* Routine for reusable threads */
int PTHREAD_REUSE_create(PTHREAD_REUSE_T *thread, const pthread_attr_t *attr);
/* REUSE_run will use sem_post(&run) to continue running the thread */
int PTHREAD_REUSE_run(PTHREAD_REUSE_T *thread,
                      void *(*start_routine) (void *), void *arg);
/* REUSE_join will use sem_wait(&finish) to wait for the thread to finish
the current task */
void PTHREAD_REUSE_join(PTHREAD_REUSE_T *thread);
void PTHREAD_REUSE_terminate(PTHREAD_REUSE_T *thread);

#endif  /* GW20_PTHREAD_CONTROL_H_ */
