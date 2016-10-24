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
#include "hcfs_fromcloud.h"
#include "pthread_control.h"

#define MAX_PINNING_FILE_CONCURRENCY ((MAX_PIN_DL_CONCURRENCY) / 2)

typedef struct {
	ino_t this_inode;
	int32_t t_idx;
} PINNING_INFO;

typedef struct {
	pthread_t pinning_manager; /* Polling manager */
	pthread_t pinning_collector; /* Collector for joining threads */
	PTHREAD_REUSE_T pinfile_tid[MAX_PINNING_FILE_CONCURRENCY];
	PINNING_INFO pinning_info[MAX_PINNING_FILE_CONCURRENCY];
	BOOL thread_active[MAX_PINNING_FILE_CONCURRENCY]; /* T or F */
	BOOL thread_finish[MAX_PINNING_FILE_CONCURRENCY];
	sem_t pinning_sem; /* Max active threads simultaneously */
	sem_t ctl_op_sem; /* Control semaphore */
	sem_t pin_active_sem;
	int32_t total_active_pinning;
	BOOL deep_sleep;
} PINNING_SCHEDULER;

PINNING_SCHEDULER pinning_scheduler;

void pinning_loop(void);
int32_t init_pin_scheduler(void);
int32_t destroy_pin_scheduler(void);
void* pinning_collect(void*);
void pinning_worker(void *ptr);
