/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
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
*           comparing between int32_t and uint32_t integers.
* 2015/5/27 Jiahong working on improving error handling
* 2015/5/28 Jiahong resolving merges
**************************************************************************/

/* TODO: Consider to convert super inode to multiple files and use striping
*	for efficiency*/

#include "super_block.h"

#ifndef _ANDROID_ENV_
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "logger.h"
#include "utils.h"
#include "macro.h"
#include "hcfs_cacheops.h"
#include "syncpoint_control.h"

#define SB_ENTRY_SIZE ((int32_t)sizeof(SUPER_BLOCK_ENTRY))
#define SB_HEAD_SIZE ((int32_t)sizeof(SUPER_BLOCK_HEAD))

/************************************************************************
*
* Function name: write_super_block_head
*        Inputs: None
*       Summary: Sync the head of super block to disk.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t write_super_block_head(void)
{
	ssize_t ret_val;
	int32_t errcode;

	ret_val = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in writing super block head. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_HEAD_SIZE) {
		write_log(0, "Short-write in writing super block head\n");
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
int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	ssize_t ret_val;
	int32_t errcode;

	if (this_inode <= 0) {
		errcode = EINVAL;
		write_log(0,
			"Error in %s, inode number is %"PRIu64". Code %d,"
			" %s\n", __func__, (uint64_t)this_inode, errcode,
			strerror(errcode));
		return -errcode;
	}
	ret_val = pread(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
			SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0,
			"Error in reading super block entry. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_ENTRY_SIZE) {
		write_log(0, "Short-read in reading super block entry.\n");
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
int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	ssize_t ret_val;
	int32_t errcode;

	ret_val = pwrite(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
				SB_HEAD_SIZE + (this_inode-1) * SB_ENTRY_SIZE);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0,
			"Error in writing super block entry. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	if (ret_val < SB_ENTRY_SIZE) {
		write_log(0, "Short-write in writing super block entry.\n");
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
int32_t super_block_init(void)
{
	int32_t errcode;
	ssize_t ret;
#ifndef _ANDROID_ENV_
	int32_t shm_key;
#endif

#ifdef _ANDROID_ENV_
	sys_super_block = malloc(sizeof(SUPER_BLOCK_CONTROL));
#else
	shm_key = shmget(1234, sizeof(SUPER_BLOCK_CONTROL), IPC_CREAT | 0666);
	if (shm_key < 0) {
		errcode = errno;
		write_log(0, "Error in opening super block. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
	sys_super_block = (SUPER_BLOCK_CONTROL *) shmat(shm_key, NULL, 0);
	if (sys_super_block == (void *)-1) {
		errcode = errno;
		write_log(0, "Error in opening super block. Code %d, %s\n",
			errcode, strerror(errcode));
		return -errcode;
	}
#endif

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
			write_log(0, "Error in initializing super block. ");
			write_log(0,
				"Code %d, %s\n", errcode, strerror(errcode));
			return -errcode;
		}
		ret = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
						SB_HEAD_SIZE, 0);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "Error in initializing super block. ");
			write_log(0,
				"Code %d, %s\n", errcode, strerror(errcode));
			close(sys_super_block->iofptr);
			return -errcode;
		}

		if (ret < SB_HEAD_SIZE) {
			write_log(0, "Error in initializing super block. ");
			close(sys_super_block->iofptr);
			return -EIO;
		}
		close(sys_super_block->iofptr);
		sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);
		if (sys_super_block->iofptr < 0) {
			errcode = errno;
			write_log(0, "Error in opening super block. ");
			write_log(0,
				"Code %d, %s\n", errcode, strerror(errcode));
			return -errcode;
		}

		ret = update_sb_size();
		if (ret < 0) {
			write_log(0, "Fail to update superblock size in %s\n",
					__func__);
			close(sys_super_block->iofptr);
			return ret;
		}
	}
	sys_super_block->unclaimed_list_fptr = fopen(UNCLAIMEDFILE, "a+");
	if (sys_super_block->unclaimed_list_fptr == NULL) {
		errcode = errno;
		write_log(0, "Error in opening super block. ");
		write_log(0, "Code %d, %s\n", errcode, strerror(errcode));
		close(sys_super_block->iofptr);
		return -errcode;
	}
	setbuf(sys_super_block->unclaimed_list_fptr, NULL);

	ret = pread(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Error in opening super block. ");
		write_log(0, "Code %d, %s\n", errcode, strerror(errcode));
		close(sys_super_block->iofptr);
		fclose(sys_super_block->unclaimed_list_fptr);
		return -errcode;
	}

	if (ret < SB_HEAD_SIZE) {
		write_log(0, "Error in opening super block. ");
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
int32_t super_block_destroy(void)
{
	int32_t errcode;
	ssize_t ret;
	int32_t ret_val;

	ret_val = 0;

	super_block_exclusive_locking();
	ret = pwrite(sys_super_block->iofptr, &(sys_super_block->head),
							SB_HEAD_SIZE, 0);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Error in closing super block. ");
		write_log(0, "Code %d, %s\n", errcode, strerror(errcode));
		ret_val = -errcode;
	} else {
		if (ret < SB_HEAD_SIZE) {
			write_log(0, "Error in closing super block. ");
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
int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int32_t ret_val;

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
int32_t super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	int32_t ret_val;

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
*        Inputs: ino_t this_inode, struct stat *newstat, BOOL no_sync
*       Summary: Update the stat in the super block entry for inode number
*                "this_inode", from input pointed by "newstat". Do not sync
*                to backend if no_sync is true.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t super_block_update_stat(ino_t this_inode, struct stat *newstat,
                                BOOL no_sync)
{
	int32_t ret_val;
	SUPER_BLOCK_ENTRY tempentry;

	super_block_exclusive_locking();

	/* Read the old content of super block entry first */
	ret_val = read_super_block_entry(this_inode, &tempentry);
	if ((ret_val >= 0) && (no_sync == FALSE)) {
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
	} else if ((ret_val >= 0) && (no_sync == TRUE)) {
		/* Only update the copy of stat in super block, but do not
		sync to the backend */
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
int32_t super_block_mark_dirty(ino_t this_inode)
{
	int32_t ret_val;
	SUPER_BLOCK_ENTRY tempentry;
	char need_write;
	int64_t now_meta_size, dirty_delta_meta_size;

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
		} else if (tempentry.status == IS_DIRTY) {
			/* When marking dirty again, just update
			 * dirty meta size */
			get_meta_size(this_inode, &now_meta_size);
			if (now_meta_size > 0) {
				dirty_delta_meta_size = now_meta_size -
						tempentry.dirty_meta_size;
				if (dirty_delta_meta_size != 0) {
					tempentry.dirty_meta_size =
							now_meta_size;
					change_system_meta(0, 0, 0, 0,
						dirty_delta_meta_size,
						0, TRUE);
					need_write = TRUE;
				}
			}
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
*                "transit_incomplete" indicates whether the transfer is not
*                finished completely and we should not clear dirty bit.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t super_block_update_transit(ino_t this_inode, char is_start_transit,
	char transit_incomplete)
{
	int32_t ret_val;
	SUPER_BLOCK_ENTRY tempentry;

	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (((is_start_transit == FALSE) && /* End of transit */
				(tempentry.status == IS_DIRTY)) &&
				((transit_incomplete != TRUE))) { /* Complete */
			if (tempentry.mod_after_in_transit == TRUE) {
				/* If sync point is set, relocate this inode
				 * so that it is going to be last one. */
				if (sys_super_block->sync_point_is_set
						== TRUE) {
					write_log(10, "Debug: Re-queue inode %"
						PRIu64, (uint64_t)this_inode);
					ret_val = ll_dequeue(this_inode,
						&tempentry);
					if (ret_val < 0) {
						super_block_exclusive_release();
						return ret_val;
					}
					ret_val = ll_enqueue(this_inode,
						IS_DIRTY, &tempentry);
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
			} else {
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
int32_t super_block_to_delete(ino_t this_inode)
{
	int32_t ret_val;
	SUPER_BLOCK_ENTRY tempentry;
	mode_t tempmode;

	ret_val = 0;
	super_block_exclusive_locking();

	ret_val = read_super_block_entry(this_inode, &tempentry);
	if (ret_val >= 0) {
		if (tempentry.pin_status == ST_PINNING) {
			pin_ll_dequeue(this_inode, &tempentry);
		}
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
		sys_super_block->head.num_active_inodes--;
		ret_val = write_super_block_head();

		if (ret_val >= 0) {
			tempentry.in_transit = FALSE;
			tempmode = tempentry.inode_stat.st_mode;
			memset(&(tempentry.inode_stat), 0, sizeof(struct stat));
			tempentry.inode_stat.st_mode = tempmode;
			tempentry.pin_status = ST_DEL;
			ret_val = write_super_block_entry(this_inode, &tempentry);
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
int32_t super_block_delete(ino_t this_inode)
{
	int32_t ret_val, errcode;
	SUPER_BLOCK_ENTRY tempentry;
	ino_t temp;
	size_t retsize;

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
		write_log(0,
			"Error in writing to unclaimed list. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	retsize = fwrite(&temp, sizeof(ino_t), 1,
				sys_super_block->unclaimed_list_fptr);
	if ((retsize < 1) && (ferror(sys_super_block->unclaimed_list_fptr))) {
		clearerr(sys_super_block->unclaimed_list_fptr);
		write_log(0, "IO Error in writing to unclaimed list.\n");
		super_block_exclusive_release();
		return -EIO;
	}

	sys_super_block->head.num_to_be_reclaimed++;
	ret_val = write_super_block_head();

	super_block_exclusive_release();

	return ret_val;
}

/* Helper function for sorting entries in the super block */
static int32_t compino(const void *firstino, const void *secondino)
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
int32_t super_block_reclaim(void)
{
	int32_t ret_val, errcode;
	SUPER_BLOCK_ENTRY tempentry;
	int64_t count;
	ino_t last_reclaimed;
	ino_t *unclaimed_list;
	size_t num_unclaimed;
	size_t ret_items;
	int64_t total_bytes;
	ino_t now_ino;

	if (sys_super_block->head.num_to_be_reclaimed < RECLAIM_TRIGGER)
		return 0;

	super_block_exclusive_locking();

	ret_val = fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_END);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}
	total_bytes = ftell(sys_super_block->unclaimed_list_fptr);
	if (total_bytes < 0) {
		errcode = errno;
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	num_unclaimed = total_bytes / (sizeof(ino_t));

	unclaimed_list = (ino_t *) malloc(sizeof(ino_t) * num_unclaimed);
	if (unclaimed_list == NULL) {
		write_log(0, "Out of memory in inode reclaiming\n");
		super_block_exclusive_release();
		return -ENOMEM;
	}

	ret_val = fseek(sys_super_block->unclaimed_list_fptr, 0, SEEK_SET);
	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
				errcode, strerror(errcode));
		super_block_exclusive_release();
		return -errcode;
	}

	ret_items = fread(unclaimed_list, sizeof(ino_t),
		num_unclaimed, sys_super_block->unclaimed_list_fptr);

	if (ret_items != num_unclaimed) {
		if (ferror(sys_super_block->unclaimed_list_fptr) != 0) {
			clearerr(sys_super_block->unclaimed_list_fptr);
			write_log(0, "IO error in inode reclaiming\n");
			super_block_exclusive_release();
			return -EIO;
		}

		write_log(0, "Warning: wrong number of items read in ");
		write_log(0, "inode reclaiming.\n");
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
	for (count = num_unclaimed - 1; count >= 0; count--) {
		now_ino = unclaimed_list[count];

		/* Skip if inode number is illegal. */
		if ((now_ino <= 0) ||
		    (now_ino >
		     (ino_t) (sys_super_block->head.num_total_inodes + 1))) {
			write_log(0, "Error: unclaimed inode number is %"PRIu64
				". Skip it.", (uint64_t)unclaimed_list[count]);
			continue;
		}

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
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
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
int32_t super_block_reclaim_fullscan(void)
{
	int32_t errcode;
	SUPER_BLOCK_ENTRY tempentry;
	int64_t count;
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
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
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
			write_log(0,
				"IO error in inode reclaiming. Code %d, %s\n",
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
				write_log(0, "IO error in inode reclaiming. ");
				write_log(0, "Code %d, %s\n",
					errcode, strerror(errcode));
				break;
			}

			if (retsize < SB_ENTRY_SIZE)
				break;

			/* Record first reclaimed node */
			if (first_reclaimed == 0)
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
					write_log(0,
						"IO error in inode reclaiming.");
					write_log(0, " Code %d, %s\n",
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
					write_log(0,
						"IO error in inode reclaiming.");
					write_log(0, " Code %d, %s\n",
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
		write_log(0, "IO error in inode reclaiming. Code %d, %s\n",
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
				uint64_t *ret_generation, char local_pin)
{
	ssize_t retsize;
	ino_t this_inode;
	SUPER_BLOCK_ENTRY tempentry;
	struct stat tempstat;
	ino_t new_first_reclaimed;
	uint64_t this_generation;
	int32_t errcode, ret;
	BOOL update_size;
	int64_t max_pinned_size;

	super_block_exclusive_locking();

	if (sys_super_block->head.num_inode_reclaimed > 0) {
		this_inode = sys_super_block->head.first_reclaimed_inode;
		retsize = pread(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
						(this_inode-1) * SB_ENTRY_SIZE);
		if (retsize < 0) {
			errcode = errno;
			write_log(0, "IO error in alloc inode. Code %d, %s\n",
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
		update_size = FALSE;
	} else {
		max_pinned_size = get_pinned_limit(local_pin);
		if (max_pinned_size < 0) {
			super_block_exclusive_release();
			return 0;
		}
		if (hcfs_system->systemdata.pinned_size > max_pinned_size) {
			super_block_exclusive_release();
			return 0;
		}
		/* If need to append a new super inode and add total
		*  inode count*/
		sys_super_block->head.num_total_inodes++;
		this_inode = sys_super_block->head.num_total_inodes + 1;
		/* Inode starts from 2 */
		this_generation = 1;
		update_size = TRUE;
	}
	sys_super_block->head.num_active_inodes++;

	/*Update the new super inode entry*/
	memset(&tempentry, 0, SB_ENTRY_SIZE);
	tempentry.pin_status = (P_IS_PIN(local_pin) ? ST_PIN : ST_UNPIN);
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
		write_log(0, "IO error in alloc inode. Code %d, %s\n",
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
		write_log(0, "IO error in alloc inode. Code %d, %s\n",
			errcode, strerror(errcode));
	}
	if (retsize < SB_HEAD_SIZE) {
		super_block_exclusive_release();
		return 0;
	}

	/* Update superblock size if a new inode is born. */
	if (update_size == TRUE) {
		ret = update_sb_size();
		if (ret < 0)
			return 0;
	}

	super_block_exclusive_release();

	return this_inode;
}

/************************************************************************
*
* Function name: ll_rebuild_dirty
*        Inputs: Inode number of the inode that is dirty but not in the 
*                linked list.
*       Summary: To traveling dirty linked list and rebuild if corrupted.
*                First look forward to fix error previous pointers.
*                Then look backward to fix error next pointers.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t ll_rebuild_dirty(ino_t missing_inode)
{
	int32_t ret;
	SUPER_BLOCK_ENTRY entry1, entry2;
	ino_t first_dirty, last_dirty;
	BOOL is_missing;

	write_log(0, "Start to rebuild dirty inode linked list.\n");
	if (missing_inode != 0)
		is_missing = TRUE;
	else
		is_missing = FALSE;

	/* Traveling forward dirty linked list */
	first_dirty = sys_super_block->head.first_dirty_inode;
	last_dirty = sys_super_block->head.last_dirty_inode;
	if (first_dirty != 0) {
		ret = read_super_block_entry(first_dirty, &entry1);
		if (ret < 0)
			return ret;
	} else {
		memset(&entry1, 0, sizeof(SUPER_BLOCK_ENTRY));
	}
	if (is_missing) {
		if (first_dirty == missing_inode)
			is_missing = FALSE;
		if (last_dirty == missing_inode)
			is_missing = FALSE;
	}
	/* Now just fix the case that first and last dirty inode is 0 */
	if (((first_dirty == 0) && (last_dirty == 0)) && (is_missing)) {
		/* Set the missing inode as both the first and the last */
		sys_super_block->head.first_dirty_inode = missing_inode;
		sys_super_block->head.last_dirty_inode = missing_inode;
		/* TODO: Now assume that the other inodes are clean. This
		should be fixed later */
		ret = read_super_block_entry(missing_inode, &entry1);
		if (ret < 0)
			return ret;
		entry1.util_ll_prev = 0;
		entry1.util_ll_next = 0;
		ret = write_super_block_entry(missing_inode, &entry1);
		if (ret < 0)
			return ret;

		sys_super_block->head.num_dirty = 1;
		ret = write_super_block_head();
		if (ret < 0)
			return ret;
		return 0;
	}


	/* TODO: If the dirty inode is missing from the linked list, need
	to insert it */

	/* TODO: Perhaps should check the prev linked list to find out if
	can recover dirty inodes on the prev linked list that could not
	be recovered later */
	if (entry1.util_ll_prev != 0) {
		entry1.util_ll_prev = 0;
		ret = write_super_block_entry(entry1.this_index, &entry1);
		if (ret < 0)
			return ret;
	}

	/* TODO: Did not consider the situation when super_block_head data
	is corrupted so that head and tail of the linked list is missing */
	while (entry1.util_ll_next != 0) {
		ret = read_super_block_entry(entry1.util_ll_next, &entry2);
		if (ret < 0)
			return ret;

		if (entry2.util_ll_prev != entry1.this_index) {
			/* Need to fix corrupted link */
			entry2.util_ll_prev = entry1.this_index;
			ret = write_super_block_entry(entry2.this_index, &entry2);
			if (ret < 0)
				return ret;
		}

		entry1 = entry2;
	}

	/* Traveling backward the dirty linked list */
	if (last_dirty != 0) {
		ret = read_super_block_entry(last_dirty, &entry1);
		if (ret < 0)
			return ret;
	} else {
		memset(&entry1, 0, sizeof(SUPER_BLOCK_ENTRY));
	}

	if (entry1.util_ll_next != 0) {
		/* First check if there was an interrupted enqueue operation */
		SUPER_BLOCK_ENTRY tempentry;
		ret = read_super_block_entry(entry1.util_ll_next, &tempentry);
			if (ret < 0)
				return ret;

		tempentry.status = IS_DIRTY;
		tempentry.util_ll_next = 0;
		tempentry.util_ll_prev = entry1.this_index;

		ret = write_super_block_entry(tempentry.this_index, &tempentry);
		if (ret < 0)
			return ret;

		sys_super_block->head.last_dirty_inode = tempentry.this_index;
		sys_super_block->head.num_dirty += 1;
		ret = write_super_block_head();
		if (ret < 0)
			return ret;

	} else if (entry1.util_ll_prev != 0) {
		ret = read_super_block_entry(entry1.util_ll_prev, &entry2);
		if (ret < 0)
			return ret;
		if (entry2.util_ll_next != entry1.this_index) {
			/* Need to fix corrupted link */
			entry2.util_ll_next = entry1.this_index;
			ret = write_super_block_entry(entry2.this_index, &entry2);
			if (ret < 0)
				return ret;
		}
	}

	write_log(0, "Finish rebuilding dirty linked list.\n");
	return 0;
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
int32_t ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY tempentry, tempentry2;
	int32_t ret, errcode;
	ssize_t retsize;
	int64_t now_meta_size, dirty_delta_meta_size;
	int32_t need_rebuild;

	if (this_entry->status == which_ll) {
		/* Update dirty meta if needs (from DIRTY to DIRTY) */
		if (which_ll == IS_DIRTY) {
			get_meta_size(thisinode, &now_meta_size);
			if (now_meta_size == 0)
				return 0;
			dirty_delta_meta_size = now_meta_size -
					this_entry->dirty_meta_size;
			this_entry->dirty_meta_size = now_meta_size;
			change_system_meta(0, 0, 0, 0,
				dirty_delta_meta_size, 0, TRUE);
		}
		return 0;
	}
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
			ret = read_super_block_entry(sys_super_block->head.last_dirty_inode, &tempentry);
			if (ret < 0)
				return ret;

			need_rebuild = FALSE;
			/* To check if the superblock was corrupted, and try to fix it. */
			if (tempentry.util_ll_next != 0)
				need_rebuild = TRUE;

			if (!need_rebuild && tempentry.util_ll_prev != 0) {
				/* Need to check the second last entry too */
				ret = read_super_block_entry(tempentry.util_ll_prev, &tempentry2);
				if (ret < 0)
					return ret;
				if (tempentry2.util_ll_next != sys_super_block->head.last_dirty_inode)
					need_rebuild = TRUE;
			}

			if (need_rebuild) {
				write_log(0, "Detect corrupted dirty inode linked list in %s.\n", __func__);
				ret = ll_rebuild_dirty(0);
				if (ret < 0)
					return ret;
				/* reload this_entry and last entry */
				ret = read_super_block_entry(this_entry->this_index, this_entry);
				if (ret < 0)
					return ret;
				ret = read_super_block_entry(sys_super_block->head.last_dirty_inode, &tempentry);
				if (ret < 0)
					return ret;
			}

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
				write_log(0, "IO error in superblock.");
				write_log(0, " Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				write_log(0, "IO error in superblock.");
				return -EIO;
			}

			tempentry.util_ll_next = thisinode;
			ret = write_super_block_entry(this_entry->util_ll_prev, &tempentry);
			if (ret < 0)
				return ret;
		}
		/* Update dirty meta size (from X to DIRTY) */
		get_meta_size(thisinode, &now_meta_size);
		if (now_meta_size) {
			this_entry->dirty_meta_size = now_meta_size;
			change_system_meta(0, 0, 0, 0, now_meta_size, 0, TRUE);
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
				write_log(0, "IO error in superblock.");
				write_log(0, " Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				write_log(0, "IO error in superblock.");
				return -EIO;
			}
			tempentry.util_ll_next = thisinode;
			retsize = pwrite(sys_super_block->iofptr, &tempentry,
				SB_ENTRY_SIZE, SB_HEAD_SIZE +
				((this_entry->util_ll_prev-1) * SB_ENTRY_SIZE));
			if (retsize < 0) {
				errcode = errno;
				write_log(0, "IO error in superblock.");
				write_log(0, " Code %d, %s\n",
					errcode, strerror(errcode));
				return -errcode;
			}
			if (retsize < SB_ENTRY_SIZE) {
				write_log(0, "IO error in superblock.");
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
int32_t ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY prev, next;
	char old_which_ll;
	ino_t temp_inode;
	int32_t ret;
	int32_t need_rebuild = FALSE;

	UNUSED(thisinode);
	old_which_ll = this_entry->status;

	if (old_which_ll == NO_LL)
		return 0;

	if (old_which_ll == TO_BE_RECLAIMED)  /*This has its own list*/
		return 0;

	if (old_which_ll == RECLAIMED)  /*This has its own operations*/
		return 0;

	if (old_which_ll == IS_DIRTY) {
		/* Need to check if the dirty linked list in superblock was corrupted */
		if (this_entry->util_ll_next == 0) {
			if (sys_super_block->head.last_dirty_inode != thisinode)
				need_rebuild = TRUE;
		} else {
			ret = read_super_block_entry(this_entry->util_ll_next, &next);
			if (ret < 0)
				return ret;
			if (next.util_ll_prev != thisinode)
				need_rebuild = TRUE;
		}

		if (this_entry->util_ll_prev == 0) {
			if (sys_super_block->head.first_dirty_inode != thisinode)
				need_rebuild = TRUE;
		} else {
			ret = read_super_block_entry(this_entry->util_ll_prev, &prev);
			if (ret < 0)
				return ret;
			if (prev.util_ll_next != thisinode)
				need_rebuild = TRUE;
		}

		if (need_rebuild) {
			write_log(0, "Detect corrupted dirty inode linked list in %s.\n", __func__);
			ret = ll_rebuild_dirty(thisinode);
			if (ret < 0)
				return ret;
			/* reload this_entry*/
			ret = read_super_block_entry(thisinode, this_entry);
			if (ret < 0)
				return ret;
		}
	}

	/* After trying to rebuild, check if sync point is set. */
	if (sys_super_block->sync_point_is_set == TRUE)
		move_sync_point(old_which_ll, thisinode, this_entry);

	/* Begin to dequeue */
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
		if (this_entry->status != IS_DIRTY || need_rebuild) {
			ret = read_super_block_entry(temp_inode, &next);
			if (ret < 0)
				return ret;
		}
		next.util_ll_prev = this_entry->util_ll_prev;
		ret = write_super_block_entry(temp_inode, &next);
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
		if (this_entry->status != IS_DIRTY || need_rebuild) {
			ret = read_super_block_entry(temp_inode, &prev);
			if (ret < 0)
				return ret;
		}
		prev.util_ll_next = this_entry->util_ll_next;
		ret = write_super_block_entry(temp_inode, &prev);
		if (ret < 0)
			return ret;
	}

	switch (old_which_ll) {
	case IS_DIRTY:
		/* Update dirty meta size */
		sys_super_block->head.num_dirty--;
		change_system_meta(0, 0, 0, 0,
			-(this_entry->dirty_meta_size), 0, TRUE);
		this_entry->dirty_meta_size = 0;
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
int32_t super_block_share_locking(void)
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
int32_t super_block_share_release(void)
{
	sem_wait(&(sys_super_block->share_CR_lock_sem));
	if (sys_super_block->share_counter == 0) {
		sem_post(&(sys_super_block->share_CR_lock_sem));
		return -1;
		/* Return error if share_counter==0 before decreasing */
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
int32_t super_block_exclusive_locking(void)
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
int32_t super_block_exclusive_release(void)
{
	sem_post(&(sys_super_block->share_lock_sem));
	sem_post(&(sys_super_block->exclusive_lock_sem));
	return 0;
}

/**
 * Mark pin_status from ST_PINNING to ST_PIN
 *
 * This function is used when a file in super block pinning queue
 * finishes fetching all block from cloud to local. If the pin_status
 * is still ST_PINNING, it is dequeued from pinning queue and pin_status
 * will be changed to ST_PIN. In case that pin_status is ST_UNPIN,
 * ST_PIN, ST_DEL, ignore them and do nothing.
 *
 * @param this_inode Inode number to be mark as ST_PIN
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t super_block_finish_pinning(ino_t this_inode)
{
	SUPER_BLOCK_ENTRY this_entry;
	int32_t ret;

	super_block_exclusive_locking();

	ret = read_super_block_entry(this_inode, &this_entry);
	if (ret < 0) {
		super_block_exclusive_release();
		return ret;
	}
	switch (this_entry.pin_status) {
	case ST_PINNING:
		ret = pin_ll_dequeue(this_inode, &this_entry);
		if (ret < 0)
			break;
		this_entry.pin_status = ST_PIN;
		ret = write_super_block_entry(this_inode, &this_entry);
		break;
	case ST_UNPIN: /* It may be unpinned by others when pinnning */
		write_log(5, "inode %"PRIu64 " is ST_UNPIN in %s",
				(uint64_t)this_inode, __func__);
		break;
	case ST_PIN: /* What happened? */
		write_log(4, "inode %"PRIu64 " is ST_PIN in %s",
				(uint64_t)this_inode, __func__);
		break;
	case ST_DEL: /* It may be deleted by others */
		break;
	}

	super_block_exclusive_release();

	if (ret < 0)
		return ret;

	return 0;
}

/**
 * Let status of an inode be ST_PIN or ST_PINNING
 *
 * If the inode number with status ST_UNPIN, then it will be push into
 * pinning queue and set pin_status as ST_PINNING when it is a regfile.
 * Otherwise pin_status will be directly set to ST_PIN.
 *
 * @param this_inode Inode number to be handled
 * @param this_mode Mode of "this_inode"
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t super_block_mark_pin(ino_t this_inode, mode_t this_mode)
{
	SUPER_BLOCK_ENTRY this_entry;
	int32_t ret;

	super_block_exclusive_locking();
	ret = read_super_block_entry(this_inode, &this_entry);
	if (ret < 0) {
		super_block_exclusive_release();
		return ret;
	}

	switch (this_entry.pin_status) {
	case ST_UNPIN:
		if (S_ISREG(this_mode)) {
			/* Enqueue and set as ST_PINNING */
			ret = pin_ll_enqueue(this_inode, &this_entry);
			this_entry.pin_status = ST_PINNING;
		} else {
			this_entry.pin_status = ST_PIN;
		}
		ret = write_super_block_entry(this_inode, &this_entry);
		break;
	case ST_DEL: /* It may be deleted by others */
	case ST_PINNING: /* It may be pinned by others */
	case ST_PIN: /* It may be pinned by others */
		break;
	}
	super_block_exclusive_release();

	if (ret < 0)
		return ret;

	return 0;
}

/**
 * Let status of an inode be ST_UNPIN
 *
 * If pin_status is ST_PINNING, it means the inode is in pinning queue, so
 * dequeue from the pinning queue and set status to ST_UNPIN. On the other
 * hand, ST_PIN means all blocks of the file are local, so just change the
 * status to ST_UNPIN directly.
 *
 * @param this_inode Inode number to be handled
 * @param this_mode Mode of "this_inode"
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t super_block_mark_unpin(ino_t this_inode, mode_t this_mode)
{
	SUPER_BLOCK_ENTRY this_entry;
	int32_t ret;

	super_block_exclusive_locking();
	ret = read_super_block_entry(this_inode, &this_entry);
	if (ret < 0) {
		super_block_exclusive_release();
		return ret;
	}

	switch (this_entry.pin_status) {
	case ST_PINNING: /* regfile in pinning queue */
		/* Enqueue and set as ST_UNPIN */
		ret = pin_ll_dequeue(this_inode, &this_entry);
		if(ret < 0)
			break;
		this_entry.pin_status = ST_UNPIN;
		ret = write_super_block_entry(this_inode, &this_entry);
		if (!S_ISREG(this_mode)) {
			write_log(2, "Error: why non-regfile has pin status"
				" ST_PINNING? In %s\n", __func__);
		}
		break;
	case ST_PIN: /* Case dir, symlnk, or regfile with all local blocks */
		this_entry.pin_status = ST_UNPIN;
		ret = write_super_block_entry(this_inode, &this_entry);
		break;
	case ST_DEL: /* To be deleted */
	case ST_UNPIN: /* It had been unpinned */
		break;
	}
	super_block_exclusive_release();

	if (ret < 0)
		return ret;

	return 0;
}

int32_t pin_ll_enqueue(ino_t this_inode, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY last_entry;
	int32_t ret;

	/* Return pin-status if this status is not UNPIN */
	if (this_entry->pin_status != ST_UNPIN) {
		return this_entry->pin_status;
	}

	if (sys_super_block->head.last_pin_inode == 0) {
		this_entry->pin_ll_next = 0;
		this_entry->pin_ll_prev = 0;
		this_entry->pin_status = ST_PINNING; /* Pinning */
		ret = write_super_block_entry(this_inode, this_entry);
		if (ret < 0)
			goto error_handling;

		sys_super_block->head.first_pin_inode = this_inode;
		sys_super_block->head.last_pin_inode = this_inode;

	} else {
		this_entry->pin_ll_next = 0;
		this_entry->pin_ll_prev = sys_super_block->head.last_pin_inode;
		this_entry->pin_status = ST_PINNING; /* Pinning */
		ret = write_super_block_entry(this_inode, this_entry);
		if (ret < 0)
			goto error_handling;

		ret = read_super_block_entry(
			sys_super_block->head.last_pin_inode, &last_entry);
		last_entry.pin_ll_next = this_inode;
		ret = write_super_block_entry(
			sys_super_block->head.last_pin_inode, &last_entry);
		if (ret < 0)
			goto error_handling;

		sys_super_block->head.last_pin_inode = this_inode;
	}

	sys_super_block->head.num_pinning_inodes++;
	ret = write_super_block_head();
	if (ret < 0)
		goto error_handling;

	return 0;

error_handling:
	return ret;
}

int32_t pin_ll_dequeue(ino_t this_inode, SUPER_BLOCK_ENTRY *this_entry)
{
	SUPER_BLOCK_ENTRY prev_entry, next_entry;
	ino_t prev_inode, next_inode;
	int32_t ret;

	/* Return pin-status if this status is not PINNING */
	if (this_entry->pin_status != ST_PINNING) {
		return this_entry->pin_status;
	}

	prev_inode = this_entry->pin_ll_prev;
	next_inode = this_entry->pin_ll_next;
	if ((next_inode == 0) && (prev_inode == 0) &&
		(sys_super_block->head.first_pin_inode != this_inode)) {
		write_log(0, "Error: Inode %"PRIu64" is not in pinning "
			"queue but be requested to dequeue. In %s\n",
			(uint64_t)this_inode, __func__);
		return -EIO;
	}

	/* Handle prev pointer */
	if (prev_inode != 0) {
		ret = read_super_block_entry(prev_inode, &prev_entry);
		if (ret < 0)
			goto error_handling;
		prev_entry.pin_ll_next = next_inode;
		ret = write_super_block_entry(prev_inode, &prev_entry);
		if (ret < 0)
			goto error_handling;
	} else {
		sys_super_block->head.first_pin_inode = next_inode;
	}

	/* Handle next pointer */
	if (next_inode != 0) {
		ret = read_super_block_entry(next_inode, &next_entry);
		if (ret < 0)
			goto error_handling;
		next_entry.pin_ll_prev = prev_inode;
		ret = write_super_block_entry(next_inode, &next_entry);
		if (ret < 0)
			goto error_handling;
	} else {
		sys_super_block->head.last_pin_inode = prev_inode;
	}

	/* Handle itself */
	this_entry->pin_ll_next = 0;
	this_entry->pin_ll_prev = 0;
	ret = write_super_block_entry(this_inode, this_entry);
	if (ret < 0)
		goto error_handling;

	sys_super_block->head.num_pinning_inodes--;
	ret = write_super_block_head();
	if (ret < 0)
		goto error_handling;

	return 0;

error_handling:
	return ret;
}

/**
 * Set sync point. If the sync point had been set, then just overwrite
 * old sync point information. Otherwise allocate new resource it needs.
 *
 * @return 0 on success, and 1 when all data is clean now. Otherwise return
 *         negative error code.
 */
int32_t super_block_set_syncpoint()
{
	SYNC_POINT_DATA *tmp_syncpoint_data;
	int32_t ret;

	super_block_exclusive_locking();
	/* Do nothing when data is clean */
	if (sys_super_block->head.last_dirty_inode == 0 &&
		sys_super_block->head.last_to_delete_inode == 0) {
		super_block_exclusive_release();
		return 1;
	}

	/* Init memory if sync point is not set */
	if (sys_super_block->sync_point_is_set == FALSE) {
		write_log(10, "Debug: Allocate sync point resource\n");
		ret = init_syncpoint_resource();
		if (ret < 0) {
			super_block_exclusive_release();
			return ret;
		}
	} else {
		write_log(10, "Debug: Sync point had been set."
				" Cover old info.\n");
	}

	/* Set sync point */
	sys_super_block->sync_point_info->sync_retry_times = SYNC_RETRY_TIMES;

	tmp_syncpoint_data = &(sys_super_block->sync_point_info->data);
	if (sys_super_block->head.last_dirty_inode) {
		tmp_syncpoint_data->upload_sync_point =
				sys_super_block->head.last_dirty_inode;
		tmp_syncpoint_data->upload_sync_complete = FALSE;
	} else {
		tmp_syncpoint_data->upload_sync_complete = TRUE;
	}

	if (sys_super_block->head.last_to_delete_inode) {
		tmp_syncpoint_data->delete_sync_point =
				sys_super_block->head.last_to_delete_inode;
		tmp_syncpoint_data->delete_sync_complete = FALSE;
	} else {
		tmp_syncpoint_data->delete_sync_complete = TRUE;
	}

	/* Write to disk */
	ret = write_syncpoint_data();
	if (ret < 0) {
		free_syncpoint_resource(TRUE);
		super_block_exclusive_release();
		return ret;
	}

	super_block_exclusive_release();
	return 0;
}

/**
 * Cancel the setting of sync point and free all resource about it.
 *
 * @return 0 on success, and 1 when no sync point is set.
 */
int32_t super_block_cancel_syncpoint()
{
	super_block_exclusive_locking();
	if (sys_super_block->sync_point_is_set == FALSE) {
		super_block_exclusive_release();
		return 1;
	}

	free_syncpoint_resource(TRUE);
	super_block_exclusive_release();
	return 0;
}
