/*************************************************************************

* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: syncpoint_control.h
* Abstract: The c header file for sync point processing.
*
* Revision History
* 2015/2/6 Kewei created this header file so as to define structure.
*
**************************************************************************/

#ifndef GW20_HCFS_SYNCPOINT_CONTROL_H_
#define GW20_HCFS_SYNCPOINT_CONTROL_H_

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "params.h"
#include "super_block.h"
#include "utils.h"

#define SYNC_RETRY_TIMES 2

//extern SYSTEM_CONF_STRUCT *system_config;
struct SUPER_BLOCK_ENTRY;

/* Infomation and data of sync point */
typedef struct {
	ino_t upload_sync_point;
	ino_t delete_sync_point;
	BOOL upload_sync_complete;
	BOOL delete_sync_complete;
} SYNC_POINT_DATA;

typedef struct SYNC_POINT_INFO {
	FILE *fptr;
	int32_t sync_retry_times;
	SYNC_POINT_DATA data;
	sem_t ctl_sem;
} SYNC_POINT_INFO;

int32_t init_syncpoint_resource();
void free_syncpoint_resource(BOOL remove_file);
int32_t write_syncpoint_data();
void fetch_syncpoint_data_path(char *path);
void check_sync_complete();
void move_sync_point(char which_ll, ino_t this_inode,
		struct SUPER_BLOCK_ENTRY *this_entry);

#endif
