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

#define MAX_PINNING_FILE_CONCURRENCY MAX_PIN_DL_CONCURRENCY

typedef struct {
	pthread_t pinning_manager;
	pthread_t pinning_collector; /* Collect threads */
	pthread_t pinning_file_tid[MAX_PINNING_FILE_CONCURRENCY];
	ino_t pinning_inodes[MAX_PINNING_FILE_CONCURRENCY];
	sem_t pinning_sem;
	sem_t ctl_op_sem;
	int total_active_pinning;
} PINNING_SCHEDULER;

PINNING_SCHEDULER pinning_scheduler;

void pinning_loop();
int init_pin_scheduler();
int destroy_pin_scheduler();
