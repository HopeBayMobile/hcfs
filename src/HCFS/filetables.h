/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: filetables.h
* Abstract: The c header file for file table managing.
*
* Revision History
* 2015/2/10 ~ 11 Jiahong added header for this file, and revising coding style.
* 2015/2/11  Jiahong moved "seek_page" and "advance_block" to metaops
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

typedef struct {
	ino_t thisinode;
	int32_t flags;
	META_CACHE_ENTRY_STRUCT *meta_cache_ptr;
	char meta_cache_locked;
	FILE *blockfptr;
	int64_t opened_block;
	int64_t cached_page_index;
	int64_t cached_filepos;
	sem_t block_sem;
} FH_ENTRY;

typedef struct {
	int64_t num_opened_files;
	char *entry_table_flags;
	FH_ENTRY *entry_table;
	int64_t last_available_index;
	sem_t fh_table_sem;
} FH_TABLE_TYPE;

FH_TABLE_TYPE system_fh_table;

int32_t init_system_fh_table(void);
int64_t open_fh(ino_t thisinode, int32_t flags);
int32_t close_fh(int64_t index);

/*END definition of file handle */

#endif  /* GW20_HCFS_FILETABLES_H_ */

