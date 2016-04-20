/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: filetables.h
* Abstract: The c header file for file table managing.
*
* Revision History
* 2015/2/10 ~ 11 Jiahong added header for this file, and revising coding style.
* 2015/2/11  Jiahong moved "seek_page" and "advance_block" to metaops
* 2016/4/20 Jiahong adding operations and structures for dir handle lookup
*
**************************************************************************/
#ifndef GW20_HCFS_FILETABLES_H_
#define GW20_HCFS_FILETABLES_H_

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "meta_mem_cache.h"

/*BEGIN definition of file handle */

#define MAX_OPEN_FILE_ENTRIES 65536

#define IS_FH 1
#define IS_DIRH 2
#define NO_FH 0

typedef struct {
	ino_t thisinode;
	int flags;
	META_CACHE_ENTRY_STRUCT *meta_cache_ptr;
	char meta_cache_locked;
	FILE *blockfptr;
	long long opened_block;
	long long cached_page_index;
	long long cached_filepos;
	sem_t block_sem;
} FH_ENTRY;

typedef struct {
	ino_t thisinode;
	int flags;
	FILE *snapshot_ptr;
} DIRH_ENTRY;

typedef struct {
	long long num_opened_files;
	uint8_t *entry_table_flags;
	FH_ENTRY *entry_table;
	DIRH_ENTRY *direntry_table;
	long long last_available_index;
	sem_t fh_table_sem;
} FH_TABLE_TYPE;

FH_TABLE_TYPE system_fh_table;

int init_system_fh_table(void);
long long open_fh(ino_t thisinode, int flags, BOOL isdir);
int close_fh(long long index);

int32_t handle_dirmeta_snapshot(ino_t thisinode);

/*END definition of file handle */

#endif  /* GW20_HCFS_FILETABLES_H_ */

