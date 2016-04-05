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
* 2016/1/6 Kewei Add flag of continuing uploading.
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

#include "hcfscurl.h"
#include "compress.h"
#include "objmeta.h"
#include "fuseop.h"
#include "dedup_table.h"

#define MAX_UPLOAD_CONCURRENCY 4
#define MAX_SYNC_CONCURRENCY 2

typedef struct {
	off_t page_filepos;
	long long page_entry_index;
	ino_t inode;
	long long blockno;
	long long seq;
	char is_block;
	char is_delete;
	char backend_delete_type; /* FALSE, TOUPLOAD_BLOCKS, BACKEND_BLOCKS */
	int which_curl;
	int progress_fd;
	char tempfilename[400];
	int which_index;
#if (DEDUP_ENABLE)
	char is_upload;
	/* After uploaded, we should increase the refcount of hash_key
	and decrease the refcount of old_hash_key*/
	unsigned char obj_id[OBJID_LENGTH];
#endif
} UPLOAD_THREAD_TYPE;

typedef struct {
	ino_t inode;
	mode_t this_mode;
	int progress_fd;
	char is_revert;
	int which_index;
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
	char threads_finished[MAX_UPLOAD_CONCURRENCY];
	int total_active_upload_threads;
	/*upload threads: used for upload objects to backends*/
} UPLOAD_THREAD_CONTROL;

typedef struct {
	sem_t sync_op_sem;
	sem_t sync_queue_sem; /*similar to upload_queue_sem*/
	pthread_t sync_handler_thread;
	pthread_t inode_sync_thread[MAX_SYNC_CONCURRENCY];

	ino_t threads_in_use[MAX_SYNC_CONCURRENCY]; /* Now syncing inode */

	int progress_fd[MAX_SYNC_CONCURRENCY]; /* File descriptor of uploading
						  progress file */

	char threads_created[MAX_SYNC_CONCURRENCY]; /* This thread is created */

	char threads_finished[MAX_SYNC_CONCURRENCY]; /* This thread finished
							uploading, and it can be
							reclaimed */

	char threads_error[MAX_SYNC_CONCURRENCY]; /* When error occur, cancel
						     uploading this time */

	BOOL is_revert[MAX_SYNC_CONCURRENCY]; /* Check whether syncing this time
						 keeps going syncing last
						 time of given uploaded inode */

	BOOL continue_nexttime[MAX_SYNC_CONCURRENCY]; /* When disconnection or
							system going down,
							continue uploading
							next time */
	int total_active_sync_threads;
	/*sync threads: used for syncing meta/block in a single inode*/
} SYNC_THREAD_CONTROL;

typedef struct {
	sem_t stat_op_sem;
	CURL_HANDLE statcurl;
} STAT_OP_T;

STAT_OP_T sync_stat_ctl;
UPLOAD_THREAD_CONTROL upload_ctl;
SYNC_THREAD_CONTROL sync_ctl;

int do_block_sync(ino_t this_inode, long long block_no,
#if (DEDUP_ENABLE)
		  CURL_HANDLE *curl_handle, char *filename, char uploaded,
		  unsigned char *hash_in_meta);
#else
		  long long seq, CURL_HANDLE *curl_handle, char *filename);
#endif

int do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename);

void init_upload_control(void);
void init_sync_control(void);
void init_sync_stat_control(void);
void sync_single_inode(SYNC_THREAD_TYPE *ptr);
void collect_finished_sync_threads(void *ptr);
void collect_finished_upload_threads(void *ptr);
int dispatch_upload_block(int which_curl);
void dispatch_delete_block(int which_curl);
int schedule_sync_meta(char *toupload_metapath, int which_curl);
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr);
void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr);
#ifdef _ANDROID_ENV_
void *upload_loop(void *ptr);
#else
void upload_loop(void);
#endif
int update_backend_stat(ino_t root_inode, long long system_size_delta,
		long long meta_size_delta, long long num_inodes_delta);

int select_upload_thread(char is_block, char is_delete,
#if (DEDUP_ENABLE)
		char is_upload,
		unsigned char old_obj_id[],
#endif
		ino_t this_inode, long long block_count,
		long long seq, off_t page_pos,
		long long e_index, int progress_fd,
		char backend_delete_type);

#endif  /* GW20_HCFS_HCFS_TOCLOUD_H_ */
