/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
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

#include "hcfscurl.h"
#include "global.h"

#ifdef _ANDROID_ENV_
#include <pthread.h>
#endif

#define MAX_PIN_DL_CONCURRENCY ((MAX_DOWNLOAD_CURL_HANDLE) / 2)

/* Download action type */
#define READ_BLOCK 0 /* Read block. High priority */
#define PIN_BLOCK 1 /* Pin a block */
#define FETCH_FILE_META 2 /* Download meta file by sync thread */
#define RESTORE_FETCH_OBJ 3 /* Download object in restoring mode */

typedef struct {
	ino_t this_inode;
	int64_t block_no;
	int64_t seqnum;
	off_t page_start_fpos;
	int32_t entry_index;
} PREFETCH_STRUCT_TYPE;

typedef struct {
	ino_t this_inode;
	int64_t block_no;
	int64_t seqnum;
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
	int32_t active_th;
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
int32_t fetch_from_cloud(FILE *fptr, char action_from, char *objname);
int32_t fetch_object_busywait_conn(FILE *fptr, char action_from, char *objname);

void* download_block_manager(void *arg);
int32_t init_download_control(void);
int32_t destroy_download_control(void);
void* fetch_backend_block(void *ptr);
int32_t fetch_pinned_blocks(ino_t inode);
void fetch_quota_from_cloud(void *ptr);
int32_t update_quota(void);
int32_t fetch_object_from_cloud(FILE *fptr, char *objname);
#endif  /* GW20_HCFS_HCFS_FROMCLOUD_H_ */
