/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
void check_sync_complete(char which_ll, ino_t this_inode);
void move_sync_point(char which_ll, ino_t this_inode,
		struct SUPER_BLOCK_ENTRY *this_entry);

#endif
