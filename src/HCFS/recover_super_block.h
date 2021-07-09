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
#ifndef GW20_HCFS_RECOVER_SB_H_
#define GW20_HCFS_RECOVER_SB_H_

#include "global.h"
#include "super_block.h"
#include "meta_mem_cache.h"

#define PROGRESS_FILE "recover_dirty_in_progress"
#define MAX_NUM_ENTRY_HANDLE 512
#define MIN_RECOVERY_INTERVAL 3600 /* Only trigger recovery once in this interval */

typedef struct recovery_round_data {
	/* Last dirty entry in previous reconstruct round */
	SUPER_BLOCK_ENTRY prev_last_entry;
	/* First dirty entry in this round */
	SUPER_BLOCK_ENTRY this_first_entry;
	/* Last dirty entry in this round */
	SUPER_BLOCK_ENTRY this_last_entry;
	/* Total dirty entries processed this round */
	int64_t this_num_dirty;
	int64_t dirty_size_delta;
	int64_t unpin_dirty_size_delta;
} RECOVERY_ROUND_DATA;

#define IS_SBENTRY_BEING_RECOVER_LATER(thisinode)                              \
	((sys_super_block->sb_recovery_meta.is_ongoing) &&                     \
	 (thisinode >= sys_super_block->sb_recovery_meta.start_inode) &&       \
	 (thisinode <= sys_super_block->sb_recovery_meta.end_inode))

BOOL need_recover_sb();

void fetch_recover_progressf_path(char *pathname);

int32_t fetch_last_recover_progress(ino_t *start_inode, ino_t *end_inode);

int32_t update_recover_progress(ino_t start_inode, ino_t end_inode);

void unlink_recover_progress_file();

void set_recovery_flag(BOOL is_ongoing, ino_t start_inode, ino_t end_inode);

void reset_queue_and_stat();

int32_t reconstruct_sb_entries(SUPER_BLOCK_ENTRY *sb_entry_arr,
			       META_CACHE_ENTRY_STRUCT **meta_cache_arr,
			       int64_t num_entry_handle,
			       RECOVERY_ROUND_DATA *this_round);

int32_t update_reconstruct_result(RECOVERY_ROUND_DATA round_data);

void *recover_sb_queue_worker(void *ptr);

void start_sb_recovery(void);

void wait_sb_recovery_terminate(void);

#endif /* GW20_HCFS_RECOVER_SB_H_ */
