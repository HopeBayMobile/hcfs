/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseop.h
* Abstract: The header file for FUSE definition
*
* Revision History
* 2015/2/2 Jiahong added header for this file.
* 2015/2/11 Jiahong moved some functions to hfuse_system.h.
* 2015/5/11 Jiahong modifying file meta for new block indexing / searching
* 2015/6/1 Jiahong adding structure for logger.
* 2015/6/30 Jiahong moved dir and file meta defs to other files
*
**************************************************************************/

#ifndef GW20_HCFS_FUSEOP_H_
#define GW20_HCFS_FUSEOP_H_


#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "params.h"

/* Define constants for timestamp changes */
#define ATIME 4
#define MTIME 2
#define CTIME 1

/* Defining the system meta resources */
typedef struct {
	long long system_size;
	long long cache_size;
	long long cache_blocks;
} SYSTEM_DATA_TYPE;

typedef struct {
	FILE *system_val_fptr;
	SYSTEM_DATA_TYPE systemdata;
	char system_going_down;
	sem_t access_sem;
	sem_t num_cache_sleep_sem;
	sem_t check_cache_sem;
	sem_t check_next_sem;
} SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

/* Functions for initializing HCFS */

pthread_t HCFS_mount;
int hook_fuse(int argc, char **argv);

void set_timestamp_now(struct stat *thisstat, char mode);
#endif  /* GW20_HCFS_FUSEOP_H_ */
