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
#ifndef GW20_HCFS_HCFS_FROMCLOUD_H_
#define GW20_HCFS_HCFS_FROMCLOUD_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hcfscurl.h"
#include "global.h"
#include "googledrive_curl.h"

#ifdef _ANDROID_ENV_
#include <pthread.h>
#endif

#include "hcfscurl.h"
#include "global.h"
#include "pthread_control.h"

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
	sem_t th_wait_sem;
	PTHREAD_REUSE_T dthread[MAX_PIN_DL_CONCURRENCY];
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
int32_t fetch_from_cloud(FILE *fptr,
			 char action_from,
			 char *objname,
			 char *fileID);
int32_t fetch_object_busywait_conn(FILE *fptr,
				   char action_from,
				   char *objname,
				   char *objid);

void* download_block_manager(void *arg);
int32_t init_download_control(void);
int32_t destroy_download_control(void);
void* fetch_backend_block(void *ptr);
int32_t fetch_pinned_blocks(ino_t inode);
void fetch_quota_from_cloud(void *ptr, BOOL enable_quota);
int32_t update_quota(void);
int32_t fetch_object_from_cloud(FILE *fptr, char *objname);
#endif  /* GW20_HCFS_HCFS_FROMCLOUD_H_ */
