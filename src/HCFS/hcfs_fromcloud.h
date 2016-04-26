/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_fromcloud.h
* Abstract: The c header file for retrieving meta or data from backend.
*
* Revision History
* 2015/2/12 Jiahong created this file (moved part of code from hcfscurl.h)
* 2015/2/12 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_FROMCLOUD_H_
#define GW20_HCFS_HCFS_FROMCLOUD_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fuseop.h"
#include "hcfscurl.h"

#ifdef _ANDROID_ENV_
#include <pthread.h>
#endif

#define MAX_PIN_DL_CONCURRENCY ((MAX_DOWNLOAD_CURL_HANDLE) / 2)
#define READ_BLOCK 0
#define PIN_BLOCK 1

typedef struct {
	ino_t this_inode;
	int64_t block_no;
	off_t page_start_fpos;
	int entry_index;
} PREFETCH_STRUCT_TYPE;

typedef struct {
	ino_t this_inode;
	int64_t block_no;
	off_t page_pos;
	char dl_error;
	char active;
} DOWNLOAD_BLOCK_INFO;

typedef struct {
	sem_t ctl_op_sem;
	sem_t dl_th_sem;
	pthread_t download_thread[MAX_PIN_DL_CONCURRENCY];
	pthread_t manager_thread;
	DOWNLOAD_BLOCK_INFO block_info[MAX_PIN_DL_CONCURRENCY];
	int active_th;
} DOWNLOAD_THREAD_CTL;

typedef struct {
	BOOL active;
	sem_t access_sem;
	pthread_attr_t thread_attr;
	pthread_t download_usermeta_tid;
} DOWNLOAD_USERMETA_CTL;

DOWNLOAD_USERMETA_CTL download_usermeta_ctl;
DOWNLOAD_THREAD_CTL download_thread_ctl;
pthread_attr_t prefetch_thread_attr;
void prefetch_block(PREFETCH_STRUCT_TYPE *ptr);
int fetch_from_cloud(FILE *fptr, char action_from,
#if (DEDUP_ENABLE)
		unsigned char *obj_id);
#else
		ino_t this_inode, int64_t block_no);
#endif

void download_block_manager();
int init_download_control();
int destroy_download_control();
void fetch_backend_block(void *ptr);
int fetch_pinned_blocks(ino_t inode);
void fetch_quota_from_cloud(void *ptr);
int update_quota();

#endif  /* GW20_HCFS_HCFS_FROMCLOUD_H_ */
