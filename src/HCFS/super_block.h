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

#ifndef GW20_HCFS_SUPER_BLOCK_H_
#define GW20_HCFS_SUPER_BLOCK_H_

#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "global.h"
#include "syncpoint_control.h"
#include "meta.h"

#define NUM_SCAN_RECLAIMED 512

#define SB_ENTRY_SIZE ((int32_t)sizeof(SUPER_BLOCK_ENTRY))
#define SB_HEAD_SIZE ((int32_t)sizeof(SUPER_BLOCK_HEAD))

/* pin-status in super block */
#define ST_DEL 0
#define ST_UNPIN 1
#define ST_PINNING 2
#define ST_PIN 3

/* Struct to track the status of queue recovery in superblock */
typedef struct SB_RECOVERY_META {
	BOOL is_ongoing; /* Super Block entries recovery ongoing */
	ino_t start_inode; /* Inode now processed */
	ino_t end_inode; /* Last inode will processed */
	time_t last_recovery_ts; /* Timestamp last recovery finished */
} SB_RECOVERY_META;

/* SUPER_BLOCK_ENTRY defines the structure for an entry in super block */
typedef struct SUPER_BLOCK_ENTRY {
	HCFS_STAT inode_stat;
	ino_t util_ll_next;
	ino_t util_ll_prev;
	ino_t pin_ll_next; /* Next file to be pinned */
	ino_t pin_ll_prev;
	char pin_status; /* ST_DEL, ST_UNPIN, ST_PINNING, ST_PIN */

	/* status is one of NO_LL, IS_DIRTY, TO_BE_DELETED, TO_BE_RECLAIMED,
	or RECLAIMED */
	char status;

	/* Indicate that the data in this inode is being synced to cloud,
	but not finished */
	char in_transit;
	char mod_after_in_transit;
	ino_t this_index;
	uint64_t generation;
	int64_t dirty_meta_size;
	int64_t lastsync_time;
} SUPER_BLOCK_ENTRY;

/* SUPER_BLOCK_HEAD defines the structure for the head of super block */
typedef struct {
	int64_t num_inode_reclaimed;
	ino_t first_reclaimed_inode;
	ino_t last_reclaimed_inode;
	ino_t first_dirty_inode;
	ino_t last_dirty_inode;
	ino_t first_to_delete_inode;
	ino_t last_to_delete_inode;
	ino_t first_pin_inode; /* first element in pin-queue */
	ino_t last_pin_inode; /* last element in pin-queue */

	int64_t num_pinning_inodes; /* Num inode to be pinned */
	int64_t num_to_be_reclaimed;
	int64_t num_to_be_deleted;
	int64_t num_dirty;
	int64_t num_block_cached;

	/* This defines the total number of inode numbers allocated, including
	in use and deleted but to be reclaimed */
	int64_t num_total_inodes;

	/* This defines the number of inodes that are currently being used */
	int64_t num_active_inodes;
} SUPER_BLOCK_HEAD;

/* SUPER_BLOCK_CONTROL defines the structure for controling super block */
typedef struct {
	SUPER_BLOCK_HEAD head;
	int32_t iofptr;
	FILE *unclaimed_list_fptr;
	FILE *temp_unclaimed_fptr;
	sem_t exclusive_lock_sem;
	sem_t share_lock_sem;
	sem_t share_CR_lock_sem;
	int32_t share_counter;
	BOOL now_reclaim_fullscan;
	BOOL sync_point_is_set; /* Indicate if need to sync all data */
	struct SYNC_POINT_INFO *sync_point_info; /* NULL if no sync point */
	SB_RECOVERY_META sb_recovery_meta; /* Info about sb recovery */
} SUPER_BLOCK_CONTROL;

SUPER_BLOCK_CONTROL *sys_super_block;

int32_t super_block_init(void);
int32_t super_block_destroy(void);
int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t super_block_to_delete(ino_t this_inode, BOOL enqueue_now);
int32_t super_block_enqueue_delete(ino_t this_inode);
int32_t super_block_delete(ino_t this_inode);
int32_t super_block_reclaim(void);
int32_t super_block_reclaim_fullscan(void);
ino_t super_block_new_inode(HCFS_STAT *in_stat,
			    uint64_t *ret_generation,
			    char local_pin);
int32_t super_block_update_stat(ino_t this_inode,
				HCFS_STAT *newstat,
				BOOL no_sync);

int32_t ll_rebuild_dirty(ino_t missing_inode);
int32_t ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry);
int32_t ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry);
int32_t write_super_block_head(void);
int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);

int32_t super_block_update_transit(ino_t this_inode, BOOL is_start_transit,
	char transit_incomplete);
int32_t super_block_mark_dirty(ino_t this_inode);
int32_t super_block_share_locking(void);
int32_t super_block_share_release(void);
int32_t super_block_exclusive_locking(void);
int32_t super_block_exclusive_release(void);

int32_t super_block_finish_pinning(ino_t this_inode);
int32_t super_block_mark_pin(ino_t this_inode, mode_t this_mode);
int32_t super_block_mark_unpin(ino_t this_inode, mode_t this_mode);

int32_t pin_ll_enqueue(ino_t this_inode, SUPER_BLOCK_ENTRY *this_entry);
int32_t pin_ll_dequeue(ino_t this_inode, SUPER_BLOCK_ENTRY *this_entry);

int32_t super_block_set_syncpoint();
int32_t super_block_cancel_syncpoint();

int32_t check_init_super_block();
#endif  /* GW20_HCFS_SUPER_BLOCK_H_ */
