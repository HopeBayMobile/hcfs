/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GW20_PTHREAD_CONTROL_H_
#define GW20_PTHREAD_CONTROL_H_

#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>

#include "global.h"

typedef struct {
	pthread_t self;  /* The real thread */
	BOOL cancelable;  /* Specify if the thread can be canceled */
	BOOL terminating; /* Specify if someone wants to terminate the thread */
	void *(*thread_routine)(void *);  /* Routine to be run in the thread */
	void (*SIGUSR2_handler)(int);  /* SIGUSR2 signal handler routine */
	void *arg;  /* Argument to the routine to be run in the thread */
} PTHREAD_T;

typedef struct {
	pthread_t self;  /* The real thread */
	BOOL cancelable;  /* Specify if the thread can be canceled */
	BOOL terminating; /* Specify if someone wants to terminate the thread */
	void *(*thread_routine)(void *);  /* Routine to be run in the thread */
	void (*SIGUSR2_handler)(int);  /* SIGUSR2 signal handler routine */
	void *arg;  /* Argument to the routine to be run in the thread */

	/* sem_wait(&run) if thread waiting for PTHREAD_REUSE_run */
	sem_t run;
	/* sem_post(&finish) if thread finished the current task */
	sem_t finish;
	/* sem_wait(&occupied) in REUSE_run, and sem_post(&occupied)
	in REUSE_join */
	sem_t occupied;
	int detachstate;  /* Specify if the thread is detached */
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
