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
* 2015/2/6  Jiahong added header for this file, and revising coding style.
* 2015/5/26 Kewei added some error handling about function 
*           super_block_reclaim_fullscan() & super_block_share_release(),
*           and besides modified macro SB_ENTRY_SIZE & SB_HEAD_SIZE to avoid 
*           comparing between signed and unsigned integers.
* 2015/5/27 Jiahong working on improving error handling
* 2015/5/28 Jiahong resolving merges
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
#include <errno.h>

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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int write_super_block_head(void)
{
	ssize_t ret_val;
	int errcode;

	ret_val = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret_val < 0) {
		errcode = errno;
		printf("Error in writing super block head. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_HEAD_SIZE) {
		printf("Short-write in writing super block head\n");
		return -EIO;
	}
	return 0;
}

/************************************************************************
*
* Function name: read_super_block_entry
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Read the super block entry of inode number "this_inode"
*                from disk to the space pointed by "inode_ptr.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	ssize_t ret_val;
	int errcode;

	ret_val = pread(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
			SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < 0) {
		errcode = errno;
		printf("Error in reading super block entry. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_ENTRY_SIZE) {
		printf("Short-read in reading super block entry.\n");
		return -EIO;
	}
	return 0;
}

/************************************************************************
*
* Function name: write_super_block_entry
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Write the super block entry of inode number "this_inode"
*                from the space pointed by "inode_ptr to disk.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	ssize_t ret_val;
	int errcode;

	ret_val = pwrite(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
				SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < 0) {
		errcode = errno;
		printf("Error in writing super block entry. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_ENTRY_SIZE) {
		printf("Short-write in writing super block entry.\n");
		return -EIO;
	}
	return 0;
}

/************************************************************************
*
* Function name: super_block_init
*        Inputs: None
*       Summary: Init super block.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_init(void)
{
	int shm_key;
	int errcode;
	ssize_t ret;

	shm_key = shmget(1234, sizeof(SUPER_BLOCK_CONTROL), IPC_CREAT | 0666);
	if (shm_key < 0) {
		errcode = errno;
		printf("Error in opening super block. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	sys_super_block = (SUPER_BLOCK_CONTROL *) shmat(shm_key, NULL, 0);
	if (sys_super_block == (void *)-1) {
		errcode = errno;
		printf("Error in opening super block. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}

	memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
	sem_init(&(sys_super_block->exclusive_lock_sem), 1, 1);
	sem_init(&(sys_super_block->share_lock_sem), 1, 1);
	sem_init(&(sys_super_block->share_CR_lock_sem), 1, 1);
	sys_super_block->share_counter = 0;

	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	if (sys_super_block->iofptr < 0) {
		sys_super_block->iofptr = open(SUPERBLOCK, O_CREAT | O_RDWR,
									0600);
		if (sys_super_block->iofptr < 0) {
			errcode = errno;
			printf("Error in initializing super block. ");
			printf("Code %d, %s\n", errcode, strerror(errcode));
			return -errcode;
		}
		ret = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
						SB_HEAD_SIZE, 0);
		if (ret < 0) {
			errcode = errno;
			printf("Error in initializing super block. ");
			printf("Code %d, %s\n", errcode, strerror(errcode));
			close(sys_super_block->iofptr);
			return -errcode;
		}

		if (ret < SB_HEAD_SIZE) {
			printf("Error in initializing super block. ");
			close(sys_super_block->iofptr);
			return -EIO;
		}
		close(sys_super_block->iofptr);
		sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);
		if (sys_super_block->iofptr < 0) {
			errcode = errno;
			printf("Error in opening super block. ");
			printf("Code %d, %s\n", errcode, strerror(errcode));
			return -errcode;
		}
	}
	sys_super_block->unclaimed_list_fptr = fopen(UNCLAIMEDFILE, "a+");
	if (sys_super_block->unclaimed_list_fptr == NULL) {
		errcode = errno;
		printf("Error in opening super block. ");
		printf("Code %d, %s\n", errcode, strerror(errcode));
		close(sys_super_block->iofptr);
		return -errcode;
	}
	setbuf(sys_super_block->unclaimed_list_fptr, NULL);

	ret = pread(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret < 0) {
		errcode = errno;
		printf("Error in opening super block. ");
		printf("Code %d, %s\n", errcode, strerror(errcode));
		close(sys_super_block->iofptr);
		fclose(sys_super_block->unclaimed_list_fptr);
		return -errcode;
	}

	if (ret < SB_HEAD_SIZE) {
		printf("Error in opening super block. ");
		close(sys_super_block->iofptr);
		fclose(sys_super_block->unclaimed_list_fptr);
		return -EIO;
	}

	return 0;
}

/************************************************************************
*
* Function name: super_block_destory
*        Inputs: None
*       Summary: Sync super block head to disk at system termination.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_destroy(void)
{
	int errcode;
	ssize_t ret;
	int ret_val;

	ret_val = 0;

	super_block_exclusive_locking();
	ret = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret < 0) {
		errcode = errno;
		printf("Error in closing super block. ");
		printf("Code %d, %s\n", errcode, strerror(errcode));
		ret_val = -errcode;
	} else {
		if (ret < SB_HEAD_SIZE) {
			printf("Error in closing super block. ");
			ret_val = -EIO;
		}
	}

	close(sys_super_block->iofptr);
	fclose(sys_super_block->unclaimed_list_fptr);

	super_block_exclusive_release();

	return ret_val;
}

/************************************************************************
*
* Function name: super_block_read
*        Inputs: ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr
*       Summary: Read the super block entry of inode number "this_inode"
*                from disk to the space pointed by "inode_ptr. This
*                function includes locking for concurrent access control.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;

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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int ret_val;

	ret_val = 0;
	super_block_exclusive_locking();
	if (inode_ptr->status != IS_DIRTY) { /* Add to dirty node list */
		ret_val = ll_dequeue(this_inode, inode_ptr);
		if (ret_val < 0) {
			super_block_exclusive_release();
			return ret_val;
		}
		ret_val = ll_enqueue(this_inode, IS_DIRTY, inode_ptr);
		if (ret_val < 0) {
			super_block_exclusive_release();
			return ret_val;
		}
		ret_val = write_super_block_head();
		if (ret_val < 0) {
			super_block_exclusive_release();
			return ret_val;
		}
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_update_stat(ino_t this_inode, struct stat *newstat)
{
	int ret_val;
	SUPER_BLOCK_ENTRY tempentry;

	ret_val = 0;
	super_block_exclusive_locking();

	/* Read the old content of super block entry first */
	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status != IS_DIRTY) {
			ret_val = ll_dequeue(this_inode, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			ret_val = ll_enqueue(this_inode, IS_DIRTY, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			ret_val = write_super_block_head();
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_mark_dirty(ino_t this_inode)
{
	int ret_val;
	SUPER_BLOCK_ENTRY tempentry;
	char need_write;

	need_write = FALSE;
	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status == NO_LL) {
			ret_val = ll_enqueue(this_inode, IS_DIRTY, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			ret_val = write_super_block_head();
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_update_transit(ino_t this_inode, char is_start_transit)
{
	int ret_val;
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
			ret_val = ll_dequeue(this_inode, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			ret_val = write_super_block_head();
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_to_delete(ino_t this_inode)
{
	int ret_val;
	SUPER_BLOCK_ENTRY tempentry;
	mode_t tempmode;

	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.status != TO_BE_DELETED) {
			ret_val = ll_dequeue(this_inode, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			ret_val = ll_enqueue(this_inode, TO_BE_DELETED,
					&tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
		}
		tempentry.in_transit = FALSE;
		tempmode = tempentry.inode_stat.st_mode;
		memset(&(tempentry.inode_stat), 0, sizeof(struct stat));
		tempentry.inode_stat.st_mode = tempmode;
		ret_val = write_super_block_entry(this_inode, &tempentry);

		if (ret_val >= 0) {
			sys_super_block->head.num_active_inodes--;
			ret_val = write_super_block_head();
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_delete(ino_t this_inode)
{
	int ret_val, errcode;
	SUPER_BLOCK_ENTRY tempentry;
	ino_t temp;
	size_t retsize;

	ret_val = 0;
	super_block_exclusive_locking();
	ret_val = read_super_block_entry(this_inode, &tempentry);

	if (ret_val >= 0) {
		if (tempentry.status != TO_BE_RECLAIMED) {
			ret_val = ll_dequeue(this_inode, &tempentry);
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
			tempentry.status = TO_BE_RECLAIMED;
		}
		tempentry.in_transit = FALSE;
		memset(&(tempentry.inode_stat), 0, sizeof(struct stat));
		ret_val = write_super_block_entry(this_inode, &tempentry);
		if (ret_val < 0) {
			super_block_exclusive_release();
			return ret_val;
		}
	}
	
	/* Add to unclaimed_list file */
	temp = this_inode;
	ret_val = fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_END);
	if (ret_val < 0) {
		errcode = errno;
		printf("Error in writing to unclaimed list. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	retsize = fwrite(&temp, sizeof(ino_t), 1,
				sys_super_block->unclaimed_list_fptr);
	if ((retsize < 1) && (ferror(sys_super_block->unclaimed_list_fptr))) {
		clearerr(sys_super_block->unclaimed_list_fptr);
		printf("IO Error in writing to unclaimed list.\n");
		super_block_exclusive_release();
		return -EIO;
	}

	sys_super_block->head.num_to_be_reclaimed++;
	ret_val = write_super_block_head();

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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_reclaim(void)
{
	int ret_val, errcode;
	SUPER_BLOCK_ENTRY tempentry;
	long long count;
	ino_t last_reclaimed;
	ino_t *unclaimed_list;
	long long num_unclaimed;
	size_t ret_items;
	long total_bytes;

	last_reclaimed = 0;

	ret_val = 0;

	if (sys_super_block->head.num_to_be_reclaimed < RECLAIM_TRIGGER)
		return 0;

	super_block_exclusive_locking();

	ret_val = fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_END);
	if (ret_val < 0) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}
	total_bytes = ftell(sys_super_block->unclaimed_list_fptr);
	if (total_bytes < 0) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	num_unclaimed = total_bytes / (sizeof(ino_t));

	unclaimed_list = (ino_t *) malloc(sizeof(ino_t) * num_unclaimed);
	if (unclaimed_list == NULL) {
		printf("Out of memory in inode reclaiming\n");
		super_block_exclusive_release();
		return -ENOMEM;
	}

	ret_val = fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_SET);
	if (ret_val < 0) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	ret_items = fread(unclaimed_list, sizeof(ino_t),
		num_unclaimed, sys_super_block->unclaimed_list_fptr);

	if (ret_items != num_unclaimed) {
		if (ferror(sys_super_block->unclaimed_list_fptr) != 0) {
			clearerr(sys_super_block->unclaimed_list_fptr);
			printf("IO error in inode reclaiming\n");
			super_block_exclusive_release();
			return -EIO;
		}
			
		printf("Warning: wrong number of items read in ");
		printf("inode reclaiming.\n");
		num_unclaimed = ret_items;
	}
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
		if (ret_val < 0) {
			super_block_exclusive_release();
			return ret_val;
		}

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
			if (ret_val < 0) {
				super_block_exclusive_release();
				return ret_val;
			}
		}
	}

	sys_super_block->head.num_to_be_reclaimed = 0;

	ret_val = write_super_block_head();
	if (ret_val < 0) {
		super_block_exclusive_release();
		return ret_val;
	}

	ret_val = ftruncate(fileno(sys_super_block->unclaimed_list_fptr), 0);
	if (ret_val < 0) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int super_block_reclaim_fullscan(void)
{
	int errcode;
	SUPER_BLOCK_ENTRY tempentry;
	long long count;
	off_t thisfilepos, retval;
	ino_t last_reclaimed, first_reclaimed, old_last_reclaimed;
	ssize_t retsize;

	last_reclaimed = 0;
	first_reclaimed = 0;

	super_block_exclusive_locking();
	sys_super_block->head.num_inode_reclaimed = 0;
	sys_super_block->head.num_to_be_reclaimed = 0;

	retval = lseek(sys_super_block->iofptr, SB_HEAD_SIZE, SEEK_SET);
	if (retval == (off_t) -1) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
			errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}
	/* Traverse all entries and reclaim. */
	for (count = 0; count < sys_super_block->head.num_total_inodes;
								count++) {
		thisfilepos = SB_HEAD_SIZE + count * SB_ENTRY_SIZE;
		retsize = pread(sys_super_block->iofptr, &tempentry,
						SB_ENTRY_SIZE, thisfilepos);
		if (retsize < 0) {
			errcode = errno;
			printf("IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
			break;
		}

		if (retsize < SB_ENTRY_SIZE)
			break;

		if ((tempentry.status == TO_BE_RECLAIMED) ||
				((tempentry.inode_stat.st_ino == 0) &&
					(tempentry.status != TO_BE_DELETED))) {
	
			/* Modify status and reclaim the entry. */
			tempentry.status = RECLAIMED;
			sys_super_block->head.num_inode_reclaimed++;
			tempentry.util_ll_next = 0;
			retsize = pwrite(sys_super_block->iofptr, &tempentry,
						SB_ENTRY_SIZE, thisfilepos);
			if (retsize < 0) {
				errcode = errno;
				printf("IO error in inode reclaiming. ");
				printf("Code %d, %s\n",
					errcode, strerror(errcode));
				break;
			}

			if (retsize < SB_ENTRY_SIZE)
				break;

			if (first_reclaimed == 0) /* Record first reclaimed node */ 
				first_reclaimed = tempentry.this_index;

			/* Save previous and now reclaimed inode */
			old_last_reclaimed = last_reclaimed;
			last_reclaimed = tempentry.this_index;	
			/* Connect previous and now reclaimed entry by setting
			   prev_entry.util_ll_next = now */
			if (old_last_reclaimed > 0) {
				thisfilepos = SB_HEAD_SIZE +
					(old_last_reclaimed-1) * SB_ENTRY_SIZE;
				retsize = pread(sys_super_block->iofptr,
					&tempentry, SB_ENTRY_SIZE, thisfilepos);
				if (retsize < 0) {
					errcode = errno;
					printf("IO error in inode reclaiming.");
					printf(" Code %d, %s\n",
						errcode, strerror(errcode));
					break;
				}

				if (retsize < SB_ENTRY_SIZE)
					break;
				if (tempentry.this_index != old_last_reclaimed)
					break;
				tempentry.util_ll_next = last_reclaimed;
				retsize = pwrite(sys_super_block->iofptr,
					&tempentry, SB_ENTRY_SIZE, thisfilepos);
				if (retsize < 0) {
					errcode = errno;
					printf("IO error in inode reclaiming.");
					printf(" Code %d, %s\n",
						errcode, strerror(errcode));
					break;
				}
				if (retsize < SB_ENTRY_SIZE)
					break;
			}
		}
	}

	sys_super_block->head.first_reclaimed_inode = first_reclaimed;
	sys_super_block->head.last_reclaimed_inode = last_reclaimed;
	sys_super_block->head.num_to_be_reclaimed = 0;
	retsize = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (retsize < 0) {
		errcode = errno;
		printf("IO error in inode reclaiming. Code %d, %s\n",
			errcode, strerror(errcode));
	}

	ftruncate(fileno(sys_super_block->unclaimed_list_fptr), 0);
	
	super_block_exclusive_release();

	/* TODO: Consider how to handle failure in reclaim_fullscan
	better. Now will just stop at any error but still try to return
	partially reclaimed list. */

	return 0;
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
	ssize_t retsize;
	ino_t this_inode;
	SUPER_BLOCK_ENTRY tempentry;
	struct stat tempstat;
	ino_t new_first_reclaimed;
	unsigned long this_generation;
	int errcode;

	super_block_exclusive_locking();

	if (sys_super_block->head.num_inode_reclaimed > 0) {
		this_inode = sys_super_block->head.first_reclaimed_inode;
		retsize = pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
						(this_inode-1) * SB_ENTRY_SIZE);
		if (retsize < 0) {
			errcode = errno;
			printf("IO error in alloc inode. Code %d, %s\n",
				errcode, strerror(errcode));
		}
		if (retsize < SB_ENTRY_SIZE) {
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
	retsize = pwrite(sys_super_block->iofptr, &tempentry, SB_ENTRY_SIZE,
				SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (retsize < 0) {
		errcode = errno;
		printf("IO error in alloc inode. Code %d, %s\n",
			errcode, strerror(errcode));
	}
	if (retsize < SB_ENTRY_SIZE) {
		super_block_exclusive_release();
		return 0;
	}

	retsize = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (retsize < 0) {
		errcode = errno;
		printf("IO error in alloc inode. Code %d, %s\n",
			errcode, strerror(errcode));
	}
	if (retsize < SB_HEAD_SIZE) {
		super_block_exclusive_release();
		return 0;
	}

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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY tempentry;
	int ret, errcode;
	ssize_t retsize;

	if (this_entry->status == which_ll)
		return 0;
	if (this_entry->status != NO_LL) {
		ret = ll_dequeue(thisinode, this_entry);
		if (ret < 0)
			return ret;
	}

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
			retsize = pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			if (retsize < 0) {
				errcode = errno;
				printf("IO error in superblock.");
				printf(" Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				printf("IO error in superblock.");
				return -EIO;
			}

			tempentry.util_ll_next = thisinode;
			retsize = pwrite(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			if (retsize < 0) {
				errcode = errno;
				printf("IO error in superblock.");
				printf(" Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				printf("IO error in superblock.");
				return -EIO;
			}
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
			retsize = pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			if (retsize < 0) {
				errcode = errno;
				printf("IO error in superblock.");
				printf(" Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				printf("IO error in superblock.");
				return -EIO;
			}
			tempentry.util_ll_next = thisinode;
			retsize = pwrite(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			if (retsize < 0) {
				errcode = errno;
				printf("IO error in superblock.");
				printf(" Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				printf("IO error in superblock.");
				return -EIO;
			}
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
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY tempentry;
	char old_which_ll;
	ino_t temp_inode;
	int ret;

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
		ret = read_super_block_entry(temp_inode, &tempentry);
		if (ret < 0)
			return ret;
		tempentry.util_ll_prev = this_entry->util_ll_prev;
		ret = write_super_block_entry(temp_inode, &tempentry);
		if (ret < 0)
			return ret;
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
		ret = read_super_block_entry(temp_inode, &tempentry);
		if (ret < 0)
			return ret;
		tempentry.util_ll_next = this_entry->util_ll_next;
		ret = write_super_block_entry(temp_inode, &tempentry);
		if (ret < 0)
			return ret;
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
	if (sys_super_block->share_counter == 0) { 
		sem_post(&(sys_super_block->share_CR_lock_sem));
		return -1; /* Return error if share_counter==0 before decreasing */
	}
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