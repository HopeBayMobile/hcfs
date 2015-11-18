/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_clouddelete.h
* Abstract: The c header file for deleting meta or data from
*           backend.
*
* Revision History
* 2015/2/13 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
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

#define MAX_DELETE_CONCURRENCY 16
#define MAX_DSYNC_CONCURRENCY 16

typedef struct {
	ino_t inode;
	long long blockno;
#if (DEDUP_ENABLE)
	unsigned char obj_id[OBJID_LENGTH];
#endif
	char is_block;
	int which_curl;
	int which_index;
} DELETE_THREAD_TYPE;

typedef struct {
	ino_t inode;
	mode_t this_mode;
	int which_index;
} DSYNC_THREAD_TYPE;

/*delete threads: used for deleting objects to backends*/
typedef struct {
	/*Initialize this to MAX_DELETE_CONCURRENCY. Decrease when
	created a thread for deletion, and increase when a thread
	finished and is joined by the handler thread*/
	sem_t delete_queue_sem;
	sem_t delete_op_sem;
	pthread_t delete_handler_thread;
	DELETE_THREAD_TYPE delete_threads[MAX_DELETE_CONCURRENCY];
	pthread_t threads_no[MAX_DELETE_CONCURRENCY];
	char threads_in_use[MAX_DELETE_CONCURRENCY];
	char threads_created[MAX_DELETE_CONCURRENCY];
	char threads_finished[MAX_DELETE_CONCURRENCY];
	int total_active_delete_threads;
} DELETE_THREAD_CONTROL;

/*dsync threads: used for dsyncing meta/block in a single inode*/
typedef struct {
	sem_t dsync_op_sem;
	sem_t dsync_queue_sem; /*similar to delete_queue_sem*/
	pthread_t dsync_handler_thread;
	pthread_t inode_dsync_thread[MAX_DSYNC_CONCURRENCY];
	ino_t threads_in_use[MAX_DSYNC_CONCURRENCY];
	char threads_created[MAX_DSYNC_CONCURRENCY];
	char threads_finished[MAX_DSYNC_CONCURRENCY];
	int total_active_dsync_threads;
} DSYNC_THREAD_CONTROL;

DELETE_THREAD_CONTROL delete_ctl;
DSYNC_THREAD_CONTROL dsync_ctl;

int do_block_delete(ino_t this_inode, long long block_no,
#if (DEDUP_ENABLE)
		    unsigned char *obj_id,
#endif
		    CURL_HANDLE *curl_handle);
int do_meta_delete(ino_t this_inode, CURL_HANDLE *curl_handle);

void init_delete_control(void);
void init_dsync_control(void);
void dsync_single_inode(DSYNC_THREAD_TYPE *ptr);
void collect_finished_dsync_threads(void *ptr);
void collect_finished_delete_threads(void *ptr);
void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr);
#ifdef _ANDROID_ENV_
void *delete_loop(void *ptr);
#else
void delete_loop(void);
#endif

#endif  /* GW20_HCFS_HCFS_CLOUDDELETE_H_ */
