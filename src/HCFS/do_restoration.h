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

char restore_metapath[METAPATHLEN];
char restore_blockpath[BLOCKPATHLEN];

sem_t restore_sem;
pthread_attr_t download_minimal_attr;
pthread_t download_minimal_thread;

/* Structure for rebuilding system meta */
SYSTEM_DATA_TYPE restored_system_meta;

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath

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

void cleanup_stage1_data(void);

#endif  /* GW20_DO_RESTORATION_H_ */
