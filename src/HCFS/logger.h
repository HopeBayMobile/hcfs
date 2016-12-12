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
#include <sys/time.h>

#include "global.h"
#include "pthread.h"

#define MAX_LOG_FILE_SIZE 20971520 /* 20MB */
#define NUM_LOG_FILE 5
#define LOG_MSG_SIZE 128
#define FLUSH_TIME_INTERVAL 3

struct LOG_internal;
typedef struct LOG_internal LOG_STRUCT;

/* FIXME: introduce dedicated function to retrieve the instance of logger */
LOG_STRUCT *logptr;   /* Pointer to log structure */

int32_t open_log(char *filename);
int32_t write_log(int32_t level, const char *format, ...);
int32_t close_log(void);

#ifdef UNITTEST
FILE *logger_get_fileptr(LOG_STRUCT *);
sem_t *logger_get_semaphore(LOG_STRUCT *);
#endif  /* UNITTEST */

#endif  /* GW20_HCFS_LOGGER_H_ */
