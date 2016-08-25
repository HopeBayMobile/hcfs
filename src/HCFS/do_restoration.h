/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: do_restoration.h
* Abstract: The header file for restore operations
*
* Revision History
* 2016/7/25 Jiahong created this file.
*
**************************************************************************/

#ifndef GW20_DO_RESTORATION_H_
#define GW20_DO_RESTORATION_H_

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"

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

sem_t restore_sem, backup_pkg_sem;
BOOL have_new_pkgbackup;
pthread_attr_t download_minimal_attr;
pthread_t download_minimal_thread;

/* Structure for rebuilding system meta */
typedef struct {
	sem_t sysmeta_sem;
	SYSTEM_DATA_TYPE restored_system_meta;
	SYSTEM_DATA_TYPE estimated_system_meta;
} HCFS_RESTORED_SYSTEM_META;

HCFS_RESTORED_SYSTEM_META hcfs_restored_system_meta;

#define LOCK_RESTORED_SYSMETA() \
	sem_wait(&(hcfs_restored_system_meta.sysmeta_sem));
#define UNLOCK_RESTORED_SYSMETA() \
	sem_post(&(hcfs_restored_system_meta.sysmeta_sem));
#define UPDATE_EST_SYSMETA(...) \
	update_estimated_system_meta((DELTA_SYSTEM_META) {__VA_ARGS__})

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath
#define PACKAGE_XML "/data/system/packages.xml"
#define PACKAGE_LIST "/data/system/packages.list"

void init_restore_path(void);

/* Returns path to status file on system restoring */
int32_t fetch_restore_stat_path(char *pathname);

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

void update_estimated_system_meta(DELTA_SYSTEM_META delta_system_meta);
void update_restored_cache_usage(int64_t delta_cache_size,
		int64_t delta_cache_blocks);
#endif  /* GW20_DO_RESTORATION_H_ */
