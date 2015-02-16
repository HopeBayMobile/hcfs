/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_tocloud.h
* Abstract: The c header file for syncing meta or data to
*           backend.
*
* Revision History
* 2015/2/16 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_TOCLOUD_H_
#define GW20_HCFS_HCFS_TOCLOUD_H_

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "hcfscurl.h"

#define MAX_UPLOAD_CONCURRENCY 16
#define MAX_SYNC_CONCURRENCY 16

typedef struct {
	off_t page_filepos;
	long long page_entry_index;
	ino_t inode;
	long long blockno;
	char is_block;
	char is_delete;
	int which_curl;
	char tempfilename[400];
} UPLOAD_THREAD_TYPE;

typedef struct {
	ino_t inode;
	mode_t this_mode;
} SYNC_THREAD_TYPE;

typedef struct {
	/*Initialize this to MAX_UPLOAD_CONCURRENCY. Decrease when
	created a thread for upload, and increase when a thread
	finished and is joined by the handler thread*/
	sem_t upload_queue_sem;
	sem_t upload_op_sem;
	pthread_t upload_handler_thread;
	UPLOAD_THREAD_TYPE upload_threads[MAX_UPLOAD_CONCURRENCY];
	pthread_t upload_threads_no[MAX_UPLOAD_CONCURRENCY];
	char threads_in_use[MAX_UPLOAD_CONCURRENCY];
	char threads_created[MAX_UPLOAD_CONCURRENCY];
	int total_active_upload_threads;
	/*upload threads: used for upload objects to backends*/
} UPLOAD_THREAD_CONTROL;

typedef struct {
	sem_t sync_op_sem;
	sem_t sync_queue_sem; /*similar to upload_queue_sem*/
	pthread_t sync_handler_thread;
	pthread_t inode_sync_thread[MAX_SYNC_CONCURRENCY];
	ino_t threads_in_use[MAX_SYNC_CONCURRENCY];
	char threads_created[MAX_SYNC_CONCURRENCY];
	int total_active_sync_threads;
	/*sync threads: used for syncing meta/block in a single inode*/
} SYNC_THREAD_CONTROL;

UPLOAD_THREAD_CONTROL upload_ctl;
SYNC_THREAD_CONTROL sync_ctl;

void do_block_sync(ino_t this_inode, long long block_no,
				CURL_HANDLE *curl_handle, char *filename);
void do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename);

void init_upload_control(void);
void init_sync_control(void);
void sync_single_inode(SYNC_THREAD_TYPE *ptr);
void collect_finished_sync_threads(void *ptr);
void collect_finished_upload_threads(void *ptr);
void dispatch_upload_block(int which_curl);
void dispatch_delete_block(int which_curl);
void schedule_sync_meta(FILE *metafptr, int which_curl);
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr);
void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr);
void upload_loop(void);

#endif  /* GW20_HCFS_HCFS_TOCLOUD_H_ */
