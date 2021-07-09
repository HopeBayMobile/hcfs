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
	int32_t flags;
	META_CACHE_ENTRY_STRUCT *meta_cache_ptr;
	char meta_cache_locked;
	FILE *blockfptr;
	int64_t opened_block;
	int64_t cached_page_index;
	int64_t cached_filepos;
	uint32_t cached_paged_out_count;
	sem_t block_sem;
} FH_ENTRY;

typedef struct {
	ino_t thisinode;
	int flags;
	FILE *snapshot_ptr;
	/* snap_ref_sem value = how many threads are using the snapshot */
	sem_t snap_ref_sem;
	/* wait_ref_sem = whether any thread is waiting for other refs to
	finish. 0 = Yes, 1 = No */
	sem_t wait_ref_sem;
} DIRH_ENTRY;

typedef struct {
	int64_t num_opened_files;
	uint8_t *entry_table_flags;
	FH_ENTRY *entry_table;
	DIRH_ENTRY *direntry_table;
	/* Use nonsnap flag to indicate if we will need to scan filetable
	for the need to create dir snapshot if a dir changing op occurs */
	BOOL have_nonsnap_dir;
	int64_t last_available_index;
	sem_t fh_table_sem;
} FH_TABLE_TYPE;

FH_TABLE_TYPE system_fh_table;

int32_t init_system_fh_table(void);
int64_t open_fh(ino_t thisinode, int32_t flags, BOOL isdir);
int32_t close_fh(int64_t index);

int32_t handle_dirmeta_snapshot(ino_t thisinode, FILE *metafptr);

/*END definition of file handle */

#endif  /* GW20_HCFS_FILETABLES_H_ */

