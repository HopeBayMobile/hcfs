/*************************************************************************

* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: super_block.h
* Abstract: The c header file for meta processing involving super
*           block in HCFS. Super block is used for fast inode accessing
*           and also tracking of status of filesystem objects (data sync
*           and garbage collection).
*
* Revision History
* 2015/2/6 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

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

/* pin-status in super block */
#define ST_DEL 0
#define ST_UNPIN 1
#define ST_PINNING 2
#define ST_PIN 3

/* SUPER_BLOCK_ENTRY defines the structure for an entry in super block */
typedef struct SUPER_BLOCK_ENTRY {
	struct stat inode_stat;
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
	sem_t exclusive_lock_sem;
	sem_t share_lock_sem;
	sem_t share_CR_lock_sem;
	int32_t share_counter;
	BOOL sync_point_is_set; /* Indicate if need to sync all data */
	struct SYNC_POINT_INFO *sync_point_info; /* NULL if no sync point */
} SUPER_BLOCK_CONTROL;

SUPER_BLOCK_CONTROL *sys_super_block;

int32_t super_block_init(void);
int32_t super_block_destroy(void);
int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t super_block_to_delete(ino_t this_inode);
int32_t super_block_delete(ino_t this_inode);
int32_t super_block_reclaim(void);
int32_t super_block_reclaim_fullscan(void);
ino_t super_block_new_inode(struct stat *in_stat,
				uint64_t *ret_generation, char local_pin);
int32_t super_block_update_stat(ino_t this_inode, struct stat *newstat,
                                BOOL no_sync);

int32_t ll_rebuild_dirty(ino_t missing_inode);
int32_t ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry);
int32_t ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry);
int32_t write_super_block_head(void);
int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);

int32_t super_block_update_transit(ino_t this_inode, char is_start_transit,
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

#endif  /* GW20_HCFS_SUPER_BLOCK_H_ */
