/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: logger.h
* Abstract: The header file for logger functions
*
* Revision History
* 2015/6/1 Jiahong created this file and added structure for logger.
*
**************************************************************************/

#ifndef GW20_HCFS_LOGGER_H_
#define GW20_HCFS_LOGGER_H_

#include <semaphore.h>
#include <stdio.h>

#include "global.h"
#include "pthread.h"

#define MAX_LOG_FILE_SIZE 20971520 /* 20MB */
#define NUM_LOG_FILE 5
#define LOG_MSG_SIZE 128
#define FLUSH_TIME_INTERVAL 3

typedef struct {
	sem_t logsem;
	FILE *fptr;
	int32_t now_log_size;
	char *log_filename;
	char *latest_log_msg;
	char *now_log_msg;
	int32_t repeated_times;
	struct timeval latest_log_time;
	time_t latest_log_start_time;
	pthread_t tid;
	pthread_attr_t flusher_attr;
	BOOL flusher_is_created;
} LOG_STRUCT;

LOG_STRUCT *logptr;   /* Pointer to log structure */

int32_t open_log(char *filename);
int32_t write_log(int32_t level, const char *format, ...);
int32_t close_log(void);

#endif  /* GW20_HCFS_LOGGER_H_ */
