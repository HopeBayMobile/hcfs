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

typedef struct {
	sem_t logsem;
	FILE *fptr;

} LOG_STRUCT;

LOG_STRUCT *logptr;   /* Pointer to log structure */

int open_log(char *filename);
int write_log(int level, char *format, ...);
int close_log(void);

#endif  /* GW20_HCFS_LOGGER_H_ */
