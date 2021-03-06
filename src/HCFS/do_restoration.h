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

#ifndef SRC_HCFS_DO_RESTORATION_H_
#define SRC_HCFS_DO_RESTORATION_H_

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "meta.h"

#define NO_FETCH 0
#define NEED_FETCH 1
#define EMULATED_ROOT 2
#define APP_DATA_FOLDER 3
#define EMULATED_USER_ROOT 4
#define IS_APP_BIN 5

#define RESERVED_META_MARGIN 20971520 /* 20M */

typedef struct {
	int64_t delta_system_size;
	int64_t delta_meta_size;
	int64_t delta_pinned_size;
	int64_t delta_backend_size;
	int64_t delta_backend_meta_size;
	int64_t delta_backend_inodes;
} DELTA_SYSTEM_META;

char restore_metapath[METAPATHLEN];
char restore_blockpath[BLOCKPATHLEN];

sem_t restore_sem;
BOOL use_old_cloud_stat;
pthread_attr_t download_minimal_attr;
pthread_t download_minimal_thread;

typedef struct {
	FILE_META_HEADER restored_smartcache_header; /* Original header */
	/* Original smart cache inode in restoration system */
	ino_t origin_smartcache_ino;
	/* Smart cache inode in active hcfs system */
	ino_t inject_smartcache_ino;
	/* Size of this smart cache */
	int64_t smart_cache_size;
} RESTORED_SMARTCACHE_DATA;

RESTORED_SMARTCACHE_DATA *sc_data;
ino_t restored_smartcache_ino; /* If this inode is 0, it means no smartcache */
ino_t restored_datadata_ino;
int64_t origin_hard_limit;
int64_t origin_meta_limit;
FILE *to_delete_fptr;
FILE *to_sync_fptr;

/* Structure for rebuilding system meta */
typedef struct {
	sem_t sysmeta_sem;
	SYSTEM_DATA_TYPE restored_system_meta;
	SYSTEM_DATA_TYPE rectified_system_meta;
	ino_t system_max_inode;
	FILE *rect_fptr;
} HCFS_RESTORED_SYSTEM_META;

HCFS_RESTORED_SYSTEM_META *hcfs_restored_system_meta;

#define LOCK_RESTORED_SYSMETA() \
	sem_wait(&(hcfs_restored_system_meta->sysmeta_sem));
#define UNLOCK_RESTORED_SYSMETA() \
	sem_post(&(hcfs_restored_system_meta->sysmeta_sem));

#define UPDATE_RECT_SYSMETA(...) \
	update_rectified_system_meta((DELTA_SYSTEM_META) {__VA_ARGS__})

#define UPDATE_RESTORE_SYSMETA(...) \
	update_restored_system_meta((DELTA_SYSTEM_META) {__VA_ARGS__})

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath
#define PACKAGE_XML "/data/system/packages.xml"
#define PACKAGE_LIST "/data/system/packages.list"

#define SMARTCACHE_IS_MISSING() (restored_smartcache_ino == 0 ? TRUE : FALSE)

typedef struct {
	DIR_ENTRY entry;
} PRUNE_T;


void init_restore_path(void);
int32_t write_system_max_inode(ino_t ino_num);
int32_t read_system_max_inode(ino_t *ino_num);

/* Returns path to status file on system restoring */
int32_t fetch_restore_stat_path(char *pathname);
int32_t fetch_restore_todelete_path(char *pathname, ino_t this_inode);

int32_t tag_restoration(char *content);

int32_t initiate_restoration(void);

int32_t check_restoration_status(void);

int32_t notify_restoration_result(int8_t stage, int32_t result);
int32_t restore_stage1_reduce_cache(void);
void start_download_minimal(void);
int32_t run_download_minimal(void);

int32_t fetch_restore_meta_path(char *pathname, ino_t this_inode);
int32_t fetch_restore_block_path(char *pathname, ino_t this_inode, int64_t block_num);

int32_t backup_package_list(void);

void cleanup_stage1_data(void);

void update_rectified_system_meta(DELTA_SYSTEM_META delta_system_meta);
void update_restored_system_meta(DELTA_SYSTEM_META delta_system_meta);
int32_t update_restored_cache_usage(int64_t delta_cache_size,
		int64_t delta_cache_blocks, char pin_type);
int32_t rectify_space_usage();
int32_t init_rectified_system_meta(char restoration_stage);
int32_t check_network_connection(void);
#endif  /* SRC_HCFS_DO_RESTORATION_H_ */
