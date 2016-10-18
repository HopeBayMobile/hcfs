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

#include "global.h"

typedef struct {
	pthread_t self;
	BOOL cancelable;
	BOOL terminating;
	void *(*thread_routine)(void *);
	void (*SIGUSR2_handler)(int);
	void *arg;
} PTHREAD_T;

pthread_key_t PTHREAD_status_key;

void PTHREAD_sighandler_init(void (*handler_ftn)(int));
int PTHREAD_create(PTHREAD_T *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);
void PTHREAD_set_exithandler();
int PTHREAD_kill(PTHREAD_T thread, int sig);
int PTHREAD_join(PTHREAD_T thread, void **retval);

#endif  /* GW20_PTHREAD_CONTROL_H_ */
