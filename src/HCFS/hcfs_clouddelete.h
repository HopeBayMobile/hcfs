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
#ifndef GW20_HCFS_HCFS_CLOUDDELETE_H_
#define GW20_HCFS_HCFS_CLOUDDELETE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "hcfscurl.h"
#include "dedup_table.h"
#include "tocloud_tools.h"
#include "global.h"
#include "googledrive_curl.h"
#include "pthread_control.h"

#define MAX_DELETE_CONCURRENCY 8
#define MAX_DSYNC_CONCURRENCY 4

typedef struct {
	ino_t inode;
	int64_t blockno;
	int64_t seq;
#if ENABLE(DEDUP)
	uint8_t obj_id[OBJID_LENGTH];
#endif
	GOOGLEDRIVE_OBJ_INFO gdrive_info;
	//char fileID[GDRIVE_ID_LENGTH];
	BOOL is_block;
	int32_t which_curl;
	int32_t which_index;
	int32_t dsync_index;
} DELETE_THREAD_TYPE;

typedef struct {
	ino_t inode;
	mode_t this_mode;
	int32_t which_index;
} DSYNC_THREAD_TYPE;

/*delete threads: used for deleting objects to backends*/
typedef struct {
	/*Initialize this to MAX_DELETE_CONCURRENCY. Decrease when
	created a thread for deletion, and increase when a thread
	finished and is joined by the handler thread*/
	sem_t delete_queue_sem;
	sem_t delete_op_sem;
	sem_t pause_sem;
	pthread_t delete_handler_thread;
	DELETE_THREAD_TYPE delete_threads[MAX_DELETE_CONCURRENCY];
	PTHREAD_REUSE_T threads_no[MAX_DELETE_CONCURRENCY];
	char threads_in_use[MAX_DELETE_CONCURRENCY];
	char threads_created[MAX_DELETE_CONCURRENCY];
	char threads_finished[MAX_DELETE_CONCURRENCY];
	char threads_error[MAX_DELETE_CONCURRENCY];
	int32_t total_active_delete_threads;
} DELETE_THREAD_CONTROL;

/*dsync threads: used for dsyncing meta/block in a single inode*/
typedef struct {
	sem_t dsync_op_sem;
	sem_t dsync_queue_sem; /*similar to delete_queue_sem*/
	sem_t pause_sem;
	IMMEDIATELY_RETRY_LIST retry_list;
	pthread_t dsync_handler_thread;
	PTHREAD_REUSE_T inode_dsync_thread[MAX_DSYNC_CONCURRENCY];
	ino_t threads_in_use[MAX_DSYNC_CONCURRENCY];
	char threads_created[MAX_DSYNC_CONCURRENCY];
	char threads_finished[MAX_DSYNC_CONCURRENCY];
	char threads_error[MAX_DSYNC_CONCURRENCY];
	BOOL retry_right_now[MAX_DSYNC_CONCURRENCY];
	int32_t total_active_dsync_threads;
} DSYNC_THREAD_CONTROL;

DELETE_THREAD_CONTROL delete_ctl;
DSYNC_THREAD_CONTROL dsync_ctl;

int32_t do_block_delete(ino_t this_inode, int64_t block_no, int64_t seq,
#if ENABLE(DEDUP)
		    uint8_t *obj_id,
#endif
		CURL_HANDLE *curl_handle,
		GOOGLEDRIVE_OBJ_INFO *gdrive_info);
int32_t do_meta_delete(ino_t this_inode, CURL_HANDLE *curl_handle,
		       GOOGLEDRIVE_OBJ_INFO *gdrive_info);

void init_delete_control(void);
void init_dsync_control(void);
void dsync_single_inode(DSYNC_THREAD_TYPE *ptr);
void collect_finished_dsync_threads(void *ptr);
void collect_finished_delete_threads(void *ptr);
void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr);
void *delete_loop(void *ptr);

#endif  /* GW20_HCFS_HCFS_CLOUDDELETE_H_ */
