/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: super_block.c
* Abstract: The c source code file for meta processing involving super
*           block in HCFS. Super block is used for fast inode accessing
*           and also tracking of status of filesystem objects (data sync
*           and garbage collection).
*
* Revision History
* 2015/2/6 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

/* TODO: Consider to convert super inode to multiple files and use striping
*	for efficiency*/

#include "super_block.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "fuseop.h"
#include "global.h"
#include "params.h"

#define SB_ENTRY_SIZE ((int)sizeof(SUPER_BLOCK_ENTRY))
#define SB_HEAD_SIZE ((int)sizeof(SUPER_BLOCK_HEAD))

extern SYSTEM_CONF_STRUCT system_config;

/************************************************************************
*
* Function name: write_super_block_head
*        Inputs: None
*       Summary: Sync the head of super block to disk.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int write_super_block_head(void)
{
	int ret_val;

	ret_val = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret_val < SB_HEAD_SIZE)
		return -1;
	return 0;
}

/************************************************************************
*
* Function name: read_super_block_entry
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Read the super block entry of inode number "this_inode"
*                from disk to the space pointed by "inode_ptr.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;

	ret_val = pread(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
			SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < SB_ENTRY_SIZE)
		return -1;
	return 0;
}

/************************************************************************
*
* Function name: write_super_block_entry
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Write the super block entry of inode number "this_inode"
*                from the space pointed by "inode_ptr to disk.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;

	ret_val = pwrite(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
				SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < SB_ENTRY_SIZE)
		return -1;
	return 0;
}

/************************************************************************
*
* Function name: super_block_init
*        Inputs: None
*       Summary: Init super block.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_init(void)
{
	/* TODO: Add error handling */
	int shm_key;

	shm_key = shmget(1234, sizeof(SUPER_BLOCK_CONTROL), IPC_CREAT | 0666);
	sys_super_block = (SUPER_BLOCK_CONTROL *) shmat(shm_key, NULL, 0);

	memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
	sem_init(&(sys_super_block->exclusive_lock_sem), 1, 1);
	sem_init(&(sys_super_block->share_lock_sem), 1, 1);
	sem_init(&(sys_super_block->share_CR_lock_sem), 1, 1);
	sys_super_block->share_counter = 0;

	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	if (sys_super_block->iofptr < 0) {
		sys_super_block->iofptr = open(SUPERBLOCK, O_CREAT | O_RDWR,
									0600);
		pwrite(sys_super_block->iofptr, &(sys_super_block->head),
						SB_HEAD_SIZE, 0);
		close(sys_super_block->iofptr);
		sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);
	}
	sys_super_block->unclaimed_list_fptr = fopen(UNCLAIMEDFILE, "a+");
	setbuf(sys_super_block->unclaimed_list_fptr, NULL);

	pread(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);

	return 0;
}

/************************************************************************
*
* Function name: super_block_destory
*        Inputs: None
*       Summary: Sync super block head to disk at system termination.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_destroy(void)
{
	super_block_exclusive_locking();
	pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	close(sys_super_block->iofptr);
	fclose(sys_super_block->unclaimed_list_fptr);

	super_block_exclusive_release();

	return 0;
}

/************************************************************************
*
* Function name: super_block_read
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Read the super block entry of inode number "this_inode"
*                from disk to the space pointed by "inode_ptr. This
*                function includes locking for concurrent access control.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;
	int ret_items;
	int sem_val;

	ret_val = 0;
	super_block_share_locking();
	ret_val = read_super_block_entry(this_inode, inode_ptr);
	super_block_share_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_write
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Write the super block entry of inode number "this_inode"
*                to disk from the space pointed by "inode_ptr. This
*                function includes locking for concurrent access control,
*                and also dirty marking on super block entry.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;
	int ret_items;

	ret_val = 0;
	super_block_exclusive_locking();
	if (inode_ptr->status != IS_DIRTY) {
		ll_dequeue(this_inode, inode_ptr);
		ll_enqueue(this_inode, IS_DIRTY, inode_ptr);
		write_super_block_head();
	}
	if (inode_ptr->in_transit == TRUE)
		inode_ptr->mod_after_in_transit = TRUE;
	ret_val = write_super_block_entry(this_inode, inode_ptr);

	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_update_stat
*        Inputs: ino_t this_inode, struct stat *newstat
*       Summary: Update the stat in the super block entry for inode number
*                "this_inode", from input pointed by "newstat".
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_update_stat(ino_t this_inode, struct stat *newstat)
{
	int ret_val;
	int ret_items;
	SUPER_BLOCK_ENTRY tempentry;

	ret_val = 0;
	super_block_exclusive_locking();

	/* Read the old content of super block entry first */
	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status != IS_DIRTY) {
			ll_dequeue(this_inode, &tempentry);
			ll_enqueue(this_inode, IS_DIRTY, &tempentry);
			write_super_block_head();
		}
		if (tempentry.in_transit == TRUE)
			tempentry.mod_after_in_transit = TRUE;

		memcpy(&(tempentry.inode_stat), newstat, sizeof(struct stat));
		/* Write the updated content back */
		ret_val = write_super_block_entry(this_inode, &tempentry);
	}
	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_mark_dirty
*        Inputs: ino_t this_inode
*       Summary: Mark the super block entry of "this_inode" as dirty.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_mark_dirty(ino_t this_inode)
{
	int ret_val;
	int ret_items;
	SUPER_BLOCK_ENTRY tempentry;
	char need_write;

	need_write = FALSE;
	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status == NO_LL) {
			ll_enqueue(this_inode, IS_DIRTY, &tempentry);
			write_super_block_head();
			need_write = TRUE;
		}
		if (tempentry.in_transit == TRUE) {
			need_write = TRUE;
			tempentry.mod_after_in_transit = TRUE;
		}

		if (need_write == TRUE)
			ret_val = write_super_block_entry(this_inode,
								&tempentry);
	}
	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_update_transit
*        Inputs: ino_t this_inode, char is_start_transit
*       Summary: Update the in_transit status for inode number "this_inode".
*                Mark the in_transit flag using the input "is_start_transit".
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_update_transit(ino_t this_inode, char is_start_transit)
{
	int ret_val;
	int ret_items;
	SUPER_BLOCK_ENTRY tempentry;

	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (((is_start_transit == FALSE) &&
				(tempentry.status == IS_DIRTY)) &&
				(tempentry.mod_after_in_transit == FALSE)) {
			/* If finished syncing and no more mod is done after
			*  queueing the inode for syncing */
			/* Remove from is_dirty list */
			ll_dequeue(this_inode, &tempentry);
			write_super_block_head();
		}
		tempentry.in_transit = is_start_transit;
		tempentry.mod_after_in_transit = FALSE;
		ret_val = write_super_block_entry(this_inode, &tempentry);
	}
	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_to_delete
*        Inputs: ino_t this_inode
*       Summary: Mark the inode "this_inode" as "to be deleted" in the
*                super block. This is the status when a filesystem object
*                is deleted but garbage collection is not yet done.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_to_delete(ino_t this_inode)
{
	int ret_val;
	int ret_items;
	SUPER_BLOCK_ENTRY tempentry;
	mode_t tempmode;

	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status != TO_BE_DELETED) {
			ll_dequeue(this_inode, &tempentry);
			ll_enqueue(this_inode, TO_BE_DELETED, &tempentry);
		}
		tempentry.in_transit = FALSE;
		tempmode = tempentry.inode_stat.st_mode;
		memset(&(tempentry.inode_stat), 0, sizeof(struct stat));
		tempentry.inode_stat.st_mode = tempmode;
		ret_val = write_super_block_entry(this_inode, &tempentry);

		if (ret_val >= 0) {
			sys_super_block->head.num_active_inodes--;
			write_super_block_head();
		}
	}
	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_delete
*        Inputs: ino_t this_inode
*       Summary: Mark the inode "this_inode" as "to be reclaimed" in the
*                super block. This is the status when a filesystem object
*                is deleted and garbage collection is done. The inode number
*                is ready to be recycled and reused.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_delete(ino_t this_inode)
{
	int ret_val;
	int ret_items;
	SUPER_BLOCK_ENTRY tempentry;
	ino_t temp;

	ret_val = 0;
	super_block_exclusive_locking();
	ret_val = read_super_block_entry(this_inode, &tempentry);

	if (ret_val >= 0) {
		if (tempentry.status != TO_BE_RECLAIMED) {
			ll_dequeue(this_inode, &tempentry);
			tempentry.status = TO_BE_RECLAIMED;
		}
		tempentry.in_transit = FALSE;
		memset(&(tempentry.inode_stat), 0, sizeof(struct stat));
		ret_val = write_super_block_entry(this_inode, &tempentry);
	}

	temp = this_inode;
	fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_END);
	fwrite(&temp, sizeof(ino_t), 1, sys_super_block->unclaimed_list_fptr);

	sys_super_block->head.num_to_be_reclaimed++;
	write_super_block_head();

	super_block_exclusive_release();

	return ret_val;
}

/* Helper function for sorting entries in the super block */
static int compino(const void *firstino, const void *secondino)
{
	ino_t temp1, temp2;

	temp1 = *(ino_t *) firstino;
	temp2 = *(ino_t *) secondino;
	if (temp1 > temp2)
		return 1;
	if (temp1 == temp2)
		return 0;
	return -1;
}

/************************************************************************
*
* Function name: super_block_reclaim
*        Inputs: None
*       Summary: Recycle (reclaim) deleted inodes on the unclaimed list.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_reclaim(void)
{
	long long total_inodes_reclaimed;
	int ret_val;
	SUPER_BLOCK_ENTRY tempentry;
	long long count;
	off_t thisfilepos;
	ino_t last_reclaimed, new_last_reclaimed;
	ino_t *unclaimed_list;
	long long num_unclaimed;

	last_reclaimed = 0;

	ret_val = 0;

	if (sys_super_block->head.num_to_be_reclaimed < RECLAIM_TRIGGER)
		return 0;

	super_block_exclusive_locking();

	fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_END);
	num_unclaimed = (ftell(sys_super_block->unclaimed_list_fptr)) /
								(sizeof(ino_t));

	unclaimed_list = (ino_t *) malloc(sizeof(ino_t) * num_unclaimed);
	fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_SET);

	num_unclaimed = fread(unclaimed_list, sizeof(ino_t),
		num_unclaimed, sys_super_block->unclaimed_list_fptr);

	/* TODO: Handle the case if the number of inodes in file is different
	*  from that in superblock head */

	/* Sort the list of unclaimed inodes. */
	qsort(unclaimed_list, num_unclaimed, sizeof(ino_t), compino);

	last_reclaimed = sys_super_block->head.first_reclaimed_inode;

	/* Starting from the largest unclaimed inode number, reclaim the inode
	*  and insert it to the head of the reclaimed list. The inodes on the
	*  relaimed lists for this batch will be sorted according to inode
	*  number when this is done (for performance reason). */
	for (count = num_unclaimed-1; count >= 0; count--) {
		ret_val = read_super_block_entry(unclaimed_list[count],
								&tempentry);
		if (ret_val < 0)
			break;

		if (tempentry.status == TO_BE_RECLAIMED) {
			if (sys_super_block->head.last_reclaimed_inode == 0)
				sys_super_block->head.last_reclaimed_inode =
							unclaimed_list[count];
			tempentry.status = RECLAIMED;
			sys_super_block->head.num_inode_reclaimed++;
			tempentry.util_ll_next = last_reclaimed;
			last_reclaimed = unclaimed_list[count];
			sys_super_block->head.first_reclaimed_inode =
								last_reclaimed;
			ret_val = write_super_block_entry(unclaimed_list[count],
								&tempentry);
			if (ret_val < 0)
				break;
		}
	}

	sys_super_block->head.num_to_be_reclaimed = 0;

	write_super_block_head();

	ftruncate(fileno(sys_super_block->unclaimed_list_fptr), 0);

	free(unclaimed_list);
	super_block_exclusive_release();
	return ret_val;
}

/************************************************************************
*
* Function name: super_block_reclaim_fullscan
*        Inputs: None
*       Summary: Recycle (reclaim) deleted inodes. This function will walk
*                over all entries in the super block and collect the entries
*                that should be reclaimed.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_reclaim_fullscan(void)
{
	long long total_inodes_reclaimed;
	int ret_val, ret_items;
	SUPER_BLOCK_ENTRY tempentry;
	long long count;
	off_t thisfilepos;
	ino_t last_reclaimed, first_reclaimed, old_last_reclaimed;

	last_reclaimed = 0;
	first_reclaimed = 0;

	ret_val = 0;

	super_block_exclusive_locking();
	sys_super_block->head.num_inode_reclaimed = 0;
	sys_super_block->head.num_to_be_reclaimed = 0;

	lseek(sys_super_block->iofptr, SB_HEAD_SIZE, SEEK_SET);
	for (count = 0; count < sys_super_block->head.num_total_inodes;
								count++) {
		thisfilepos = SB_HEAD_SIZE + count * SB_ENTRY_SIZE;
		ret_items = pread(sys_super_block->iofptr, &tempentry,
						SB_ENTRY_SIZE, thisfilepos);

		if (ret_items < SB_ENTRY_SIZE)
			break;

		if ((tempentry.status == TO_BE_RECLAIMED) ||
				((tempentry.inode_stat.st_ino == 0) &&
					(tempentry.status != TO_BE_DELETED))) {
			tempentry.status = RECLAIMED;
			sys_super_block->head.num_inode_reclaimed++;
			tempentry.util_ll_next = 0;
			ret_items = pwrite(sys_super_block->iofptr, &tempentry,
						SB_ENTRY_SIZE, thisfilepos);

			if (ret_items < SB_ENTRY_SIZE)
				break;
			if (first_reclaimed == 0)
				first_reclaimed = tempentry.this_index;
			old_last_reclaimed = last_reclaimed;
			last_reclaimed = tempentry.this_index;

			if (old_last_reclaimed > 0) {
				thisfilepos = SB_HEAD_SIZE +
					(old_last_reclaimed-1) * SB_ENTRY_SIZE;
				ret_items = pread(sys_super_block->iofptr,
					&tempentry, SB_ENTRY_SIZE, thisfilepos);
				if (ret_items < SB_ENTRY_SIZE)
					break;
				if (tempentry.this_index != old_last_reclaimed)
					break;
				tempentry.util_ll_next = last_reclaimed;
				ret_items = pwrite(sys_super_block->iofptr,
					&tempentry, SB_ENTRY_SIZE, thisfilepos);
				if (ret_items < SB_ENTRY_SIZE)
					break;
			}
		}
	}

	sys_super_block->head.first_reclaimed_inode = first_reclaimed;
	sys_super_block->head.last_reclaimed_inode = last_reclaimed;
	sys_super_block->head.num_to_be_reclaimed = 0;
	pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);

	super_block_exclusive_release();
	return ret_val;
}

/************************************************************************
*
* Function name: super_block_new_inode
*        Inputs: struct stat *in_stat
*       Summary: Allocate a new inode (first by checking reclaimed inodes)
*                and initialize the entry in super block using "in_stat".
*  Return value: New inode number if successful. Otherwise returns 0.
*
*************************************************************************/
ino_t super_block_new_inode(struct stat *in_stat,
				unsigned long *ret_generation)
{
	int ret_items;
	ino_t this_inode;
	SUPER_BLOCK_ENTRY tempentry;
	struct stat tempstat;
	ino_t new_first_reclaimed;
	unsigned long this_generation;

	super_block_exclusive_locking();

	if (sys_super_block->head.num_inode_reclaimed > 0) {
		this_inode = sys_super_block->head.first_reclaimed_inode;
		ret_items = pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
						(this_inode-1) * SB_ENTRY_SIZE);
		if (ret_items < SB_ENTRY_SIZE) {
			super_block_exclusive_release();
			return 0;
		}
		new_first_reclaimed = tempentry.util_ll_next;

		/*If there are no more reclaimed inode*/
		if (new_first_reclaimed == 0) {
			/*TODO: Need to check if num_inode_reclaimed is 0.
				If not, need to rescan super inode*/
			sys_super_block->head.num_inode_reclaimed = 0;
			sys_super_block->head.first_reclaimed_inode = 0;
			sys_super_block->head.last_reclaimed_inode = 0;
		} else {
			/*Update super inode head regularly*/
			sys_super_block->head.num_inode_reclaimed--;
			sys_super_block->head.first_reclaimed_inode =
							new_first_reclaimed;
		}
		this_generation = tempentry.generation + 1;
	} else {
		/* If need to append a new super inode and add total
		*  inode count*/
		sys_super_block->head.num_total_inodes++;
		this_inode = sys_super_block->head.num_total_inodes;
		this_generation = 1;
	}
	sys_super_block->head.num_active_inodes++;

	/*Update the new super inode entry*/
	memset(&tempentry, 0, SB_ENTRY_SIZE);
	tempentry.this_index = this_inode;
	tempentry.generation = this_generation;
	if (ret_generation != NULL)
		*ret_generation = this_generation;

	memcpy(&tempstat, in_stat, sizeof(struct stat));
	tempstat.st_ino = this_inode;
	memcpy(&(tempentry.inode_stat), &tempstat, sizeof(struct stat));
	ret_items = pwrite(sys_super_block->iofptr, &tempentry, SB_ENTRY_SIZE,
				SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_items < SB_ENTRY_SIZE) {
		super_block_exclusive_release();
		return 0;
	}
	/*TODO: Error handling here if write to super inode head failed*/
	pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);

	super_block_exclusive_release();

	return this_inode;
}

/************************************************************************
*
* Function name: ll_enqueue
*        Inputs: ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry
*       Summary: Enqueue super block entry "this_entry" (with inode number
*                "thisinode") to the linked list "which_ll".
*                "which_ll" can be one of NO_LL, IS_DIRTY, TO_BE_DELETED,
*                TO_BE_RECLAIMED, or RECLAIMED.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY tempentry;

	if (this_entry->status == which_ll)
		return 0;
	if (this_entry->status != NO_LL)
		ll_dequeue(thisinode, this_entry);

	if (which_ll == NO_LL)
		return 0;

	if (which_ll == TO_BE_RECLAIMED)	/*This has its own list*/
		return 0;

	if (which_ll == RECLAIMED)	/*This has its own operations*/
		return 0;

	switch (which_ll) {
	case IS_DIRTY:
		if (sys_super_block->head.first_dirty_inode == 0) {
			sys_super_block->head.first_dirty_inode = thisinode;
			sys_super_block->head.last_dirty_inode = thisinode;
			this_entry->util_ll_next = 0;
			this_entry->util_ll_prev = 0;
			sys_super_block->head.num_dirty++;
		} else {
			this_entry->util_ll_prev =
					sys_super_block->head.last_dirty_inode;
			sys_super_block->head.last_dirty_inode = thisinode;
			this_entry->util_ll_next = 0;
			sys_super_block->head.num_dirty++;
			pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			tempentry.util_ll_next = thisinode;
			pwrite(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
		}
		break;
	case TO_BE_DELETED:
		if (sys_super_block->head.first_to_delete_inode == 0) {
			sys_super_block->head.first_to_delete_inode = thisinode;
			sys_super_block->head.last_to_delete_inode = thisinode;
			this_entry->util_ll_next = 0;
			this_entry->util_ll_prev = 0;
			sys_super_block->head.num_to_be_deleted++;
		} else {
			this_entry->util_ll_prev =
				sys_super_block->head.last_to_delete_inode;
			sys_super_block->head.last_to_delete_inode = thisinode;
			this_entry->util_ll_next = 0;
			sys_super_block->head.num_to_be_deleted++;
			pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			tempentry.util_ll_next = thisinode;
			pwrite(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
		}
		break;
	default:
		break;
	}

	this_entry->status = which_ll;
	return 0;
}

/************************************************************************
*
* Function name: ll_dequeue
*        Inputs: ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry
*       Summary: Dequeue super block entry "this_entry" (with inode number
*                "thisinode") from the current linked list.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY tempentry;
	char old_which_ll;
	ino_t temp_inode;

	old_which_ll = this_entry->status;

	if (old_which_ll == NO_LL)
		return 0;

	if (old_which_ll == TO_BE_RECLAIMED)  /*This has its own list*/
		return 0;

	if (old_which_ll == RECLAIMED)  /*This has its own operations*/
		return 0;

	if (this_entry->util_ll_next == 0) {
		switch (old_which_ll) {
		case IS_DIRTY:
			sys_super_block->head.last_dirty_inode =
						this_entry->util_ll_prev;
			break;
		case TO_BE_DELETED:
			sys_super_block->head.last_to_delete_inode =
						this_entry->util_ll_prev;
			break;
		default:
			break;
		}
	} else {
		temp_inode = this_entry->util_ll_next;
		read_super_block_entry(temp_inode, &tempentry);
		tempentry.util_ll_prev = this_entry->util_ll_prev;
		write_super_block_entry(temp_inode, &tempentry);
	}

	if (this_entry->util_ll_prev == 0) {
		switch (old_which_ll) {
		case IS_DIRTY:
			sys_super_block->head.first_dirty_inode =
						this_entry->util_ll_next;
			break;
		case TO_BE_DELETED:
			sys_super_block->head.first_to_delete_inode =
						this_entry->util_ll_next;
			break;
		default:
			break;
		}
	} else {
		temp_inode = this_entry->util_ll_prev;
		read_super_block_entry(temp_inode, &tempentry);
		tempentry.util_ll_next = this_entry->util_ll_next;
		write_super_block_entry(temp_inode, &tempentry);
	}

	switch (old_which_ll) {
	case IS_DIRTY:
		sys_super_block->head.num_dirty--;
		break;
	case TO_BE_DELETED:
		sys_super_block->head.num_to_be_deleted--;
		break;
	default:
		break;
	}

	this_entry->status = NO_LL;
	this_entry->util_ll_next = 0;
	this_entry->util_ll_prev = 0;
	return 0;
}

/************************************************************************
*
* Function name: super_block_share_locking
*        Inputs: None
*       Summary: Locks the shared lock for the super block.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_share_locking(void)
{
	sem_wait(&(sys_super_block->exclusive_lock_sem));

	sem_wait(&(sys_super_block->share_CR_lock_sem));
	if (sys_super_block->share_counter == 0)
		sem_wait(&(sys_super_block->share_lock_sem));
	sys_super_block->share_counter++;
	sem_post(&(sys_super_block->share_CR_lock_sem));
	sem_post(&(sys_super_block->exclusive_lock_sem));
	return 0;
}

/************************************************************************
*
* Function name: super_block_share_release
*        Inputs: None
*       Summary: Releases the shared lock for the super block.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_share_release(void)
{
	sem_wait(&(sys_super_block->share_CR_lock_sem));
	sys_super_block->share_counter--;
	if (sys_super_block->share_counter == 0)
		sem_post(&(sys_super_block->share_lock_sem));
	sem_post(&(sys_super_block->share_CR_lock_sem));
	return 0;
}

/************************************************************************
*
* Function name: super_block_exclusive_locking
*        Inputs: None
*       Summary: Locks the exclusive lock for the super block.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_exclusive_locking(void)
{
	sem_wait(&(sys_super_block->exclusive_lock_sem));
	sem_wait(&(sys_super_block->share_lock_sem));
	return 0;
}

/************************************************************************
*
* Function name: super_block_exclusive_release
*        Inputs: None
*       Summary: Releases the exclusive lock for the super block.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int super_block_exclusive_release(void)
{
	sem_post(&(sys_super_block->share_lock_sem));
	sem_post(&(sys_super_block->exclusive_lock_sem));
	return 0;
}
