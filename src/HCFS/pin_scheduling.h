/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pin_scheduling.h
* Abstract: The c header file about pin_scheduling.h
*
* Revision History
* 2015/11/6 Kewei create this file.
*
**************************************************************************/

#include <pthread.h>

typedef struct {
	pthread_t pinning_manager;
} PINNING_SCHEDULER;

PINNING_SCHEDULER pinning_scheduler;

void pinning_loop();
int init_pin_scheduler();
int destroy_pin_scheduler();
