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

#define MAX_LOG_SIZE 1048576
#define NUM_LOG_FILE 5

typedef struct {
	sem_t logsem;
	FILE *fptr;
	int32_t now_log_size;
	char *log_filename;

} LOG_STRUCT;

LOG_STRUCT *logptr;   /* Pointer to log structure */

int32_t open_log(char *filename);
int32_t write_log(int32_t level, char *format, ...);
int32_t close_log(void);

#endif  /* GW20_HCFS_LOGGER_H_ */
