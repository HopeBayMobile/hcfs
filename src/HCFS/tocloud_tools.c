/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: tocloud_tools.c
* Abstract: The c source code file for helping to sync a single file.
*
* Revision History
* 2016/2/15 Kewei create this file
*
**************************************************************************/

#include "tocloud_tools.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>
#include <inttypes.h>

#include "hcfs_clouddelete.h"
#include "params.h"
#include "global.h"
#include "super_block.h"
#include "fuseop.h"
#include "logger.h"
#include "macro.h"
#include "metaops.h"
#include "dedup_table.h"
#include "utils.h"
#include "atomic_tocloud.h"
#include "hfuse_system.h"
#include "FS_manager.h"
#include "hcfs_fromcloud.h"
#include "hcfs_tocloud.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

int32_t change_block_status_to_BOTH(ino_t inode, int64_t blockno,
		int64_t page_pos, int64_t toupload_seq)
{
	BLOCK_ENTRY_PAGE tmp_page;
	int64_t local_seq;
	char local_status;
	int32_t e_index;
	int32_t ret, errcode;
	int32_t semval;
	ssize_t ret_ssize;
	int64_t delta_unpin_dirty_size;
	char blockpath[300], local_metapath[300];
	off_t cache_block_size;
	FILE *local_metafptr;
	FILE_META_TYPE tempfilemeta;
	sem_t *semptr;

	semptr = &(hcfs_system->something_to_replace);
	fetch_meta_path(local_metapath, inode);
	local_metafptr = fopen(local_metapath, "r+");
	if (!local_metafptr) {
		errcode = errno;
		if (errcode == ENOENT)
			write_log(5, "Warn: meta %"PRIu64" is removed"
				". Code %d\n", (uint64_t)inode, errcode);
		else
			write_log(0, "Error: Cannot read meta %"PRIu64
				". Code %d\n", (uint64_t)inode, errcode);
		return -errcode;
	}
	setbuf(local_metafptr, NULL);

	flock(fileno(local_metafptr), LOCK_EX);
	if (access(local_metapath, F_OK) < 0) {
		write_log(5, "meta %"PRIu64" is removed "
				"when changing to BOTH\n", (uint64_t)inode);
		flock(fileno(local_metafptr), LOCK_UN);
		fclose(local_metafptr);
		return -ENOENT;
	}

	PREAD(fileno(local_metafptr), &tempfilemeta, sizeof(FILE_META_TYPE),
			sizeof(HCFS_STAT));
	PREAD(fileno(local_metafptr), &tmp_page,
			sizeof(BLOCK_ENTRY_PAGE), page_pos);
	e_index = blockno % MAX_BLOCK_ENTRIES_PER_PAGE;
	local_status = tmp_page.block_entries[e_index].status;
	local_seq = tmp_page.block_entries[e_index].seqnum;
	/* Change status if local status is ST_LtoC */
	if (local_status == ST_LtoC && local_seq == toupload_seq) {
		tmp_page.block_entries[e_index].status = ST_BOTH;
		tmp_page.block_entries[e_index].uploaded = TRUE;

		ret = fetch_block_path(blockpath, inode, blockno);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
		ret = set_block_dirty_status(blockpath, NULL, FALSE);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
		/* Remember to decrease dirty_cache_size */
		cache_block_size = check_file_size(blockpath);
		if (P_IS_UNPIN(tempfilemeta.local_pin))
			delta_unpin_dirty_size = -cache_block_size;
		else
			delta_unpin_dirty_size = 0;
		change_system_meta_ignore_dirty(inode, 0, 0, 0, 0,
						-cache_block_size,
						delta_unpin_dirty_size, TRUE);
		/* Update dirty size in file meta */
		update_file_stats(local_metafptr, 0, 0, 0,
				-cache_block_size, inode);
		PWRITE(fileno(local_metafptr), &tmp_page,
				sizeof(BLOCK_ENTRY_PAGE), page_pos);
		/* If unpin, post cache management
		 * control */
		if (P_IS_UNPIN(tempfilemeta.local_pin)) {
			semval = 0;
			ret = sem_getvalue(semptr, &semval);
			if ((ret == 0) && (semval == 0))
				sem_post(semptr);
		}
		write_log(10, "Debug sync: block_%"PRIu64"_%lld"
				" is changed to ST_BOTH\n",
				(uint64_t)inode, blockno);
	}

	flock(fileno(local_metafptr), LOCK_UN);
	fclose(local_metafptr);

	return 0;

errcode_handle:
	flock(fileno(local_metafptr), LOCK_UN);
	fclose(local_metafptr);
	return errcode;
}

/**
 * Wait for all specified threads with inode number "inode" and 
 * return if all these threads are terminated.
 *
 * @param inode Inode number to be waited.
 *
 * @return None.
 */
void busy_wait_all_specified_upload_threads(ino_t inode)
{
	char upload_done;
	struct timespec time_to_sleep;
	int count;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/
	upload_done = FALSE;
	while (upload_done == FALSE) {
		nanosleep(&time_to_sleep, NULL);
		upload_done = TRUE;
		sem_wait(&(upload_ctl.upload_op_sem));
		for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
			if ((upload_ctl.threads_in_use[count] == TRUE) &&
				(upload_ctl.upload_threads[count].inode ==
				inode)) { /* Wait for this inode */
				upload_done = FALSE;
				break;
			}
		}
		sem_post(&(upload_ctl.upload_op_sem));
	}

	return;
}

/**
 * Given "block_info" and deletion type "delete_which_one", Choose a
 * block objid or seq number to be deleted on cloud.
 *
 * @param delete_which_one It is either DEL_TOUPLOAD_BLOCKS or
 *                         DEL_BACKEND_BLOCKS. Choose seq numebr of
 *                         to-upload blocks if DEL_TOUPLOAD_BLOCKS.
 *                         Choose another one if DEL_BACKEND_BLOCKS.
 * @param block_info This entry recorded to-upload data and backend data
 *                   for a specified block.
 * @param block_seq Seq number to be returned. 
 * @param block_objid Object id to be returned
 *
 * @return 0 on success, -1 when do not need to delete blocks on cloud.
 */ 
#if ENABLE(DEDUP)
static inline int _choose_deleted_block(char delete_which_one,
	const BLOCK_UPLOADING_STATUS *block_info, unsigned char *block_objid)
{
	char finish_uploading;

	finish_uploading = block_info->finish_uploading;

	/* Delete those blocks just uploaded */
	if (delete_which_one == DEL_TOUPLOAD_BLOCKS) {
		/* Do not delete if not finish */
		if (finish_uploading == FALSE)
			return -1;
		/* Do not delete if it does not exist */
		if (TOUPLOAD_BLOCK_EXIST(block_info->block_exist) == FALSE)
			return -1;
		/* Do not delete if it is the same as backend block */
		if (!memcmp(block_info->to_upload_objid,
				block_info->backend_objid, OBJID_LENGTH))
			return -1;

		memcpy(block_objid, block_info->to_upload_objid, OBJID_LENGTH);
		return 0;
	}

	/* Delete old blocks on cloud */
	if (delete_which_one == DEL_BACKEND_BLOCKS) {
		/* Do not delete if it does not exist */
		if (CLOUD_BLOCK_EXIST(block_info->block_exist) == FALSE)
			return -1;
		/* Do not delete if it is the same as to-upload block */
		if (!memcmp(block_info->to_upload_objid,
				block_info->backend_objid, OBJID_LENGTH))
			return -1;

		memcpy(block_objid, block_info->backend_objid, OBJID_LENGTH);
		return 0;
	}
	return -1; /* unknown type */
}

#else
static inline int _choose_deleted_block(char delete_which_one,
		const BLOCK_UPLOADING_STATUS *block_info,
		long long *block_seq, ino_t inode)
{
	char finish_uploading;
	long long to_upload_seq;
	long long backend_seq;

	UNUSED(inode);
	finish_uploading = block_info->finish_uploading;
	to_upload_seq = block_info->to_upload_seq;
	backend_seq = block_info->backend_seq;

	if (delete_which_one == DEL_TOUPLOAD_BLOCKS) {
		/* Do not delete if not finish */
		if (finish_uploading == FALSE)
			return -1;
		/* Do not need to delete if block does not exist */
		if (TOUPLOAD_BLOCK_EXIST(block_info->block_exist) == FALSE)
			return -1;
		/* Do not need to delete if seq is the same as backend,
		 * because it is not uploaded */
		if (to_upload_seq == backend_seq)
			return -1;

		*block_seq = to_upload_seq;
		return 0;
	}

	/* Do not need to check finish_uploading because backend blocks
	 * exist on cloud. */
	if (delete_which_one == DEL_BACKEND_BLOCKS) {
		if (CLOUD_BLOCK_EXIST(block_info->block_exist) == FALSE)
			return -1;
		if (to_upload_seq == backend_seq)
			return -1;

		*block_seq = backend_seq;
		return 0;
	}
	return -1; /* unknown type */
}
#endif

/**
 * Revert block status to ST_LDISK when cancelling to sync this time.
 *
 * @return 0 on success, -ECANCELED on skipping delete this block on cloud.
 */ 
static int _revert_block_status(FILE *local_metafptr, ino_t this_inode,
		long long blockno, long long page_pos, int eindex)
{
	char status;
	int ret, errcode;
	BLOCK_ENTRY_PAGE bentry_page;
	FILE_META_TYPE filemeta;
	char blockpath[300];
	ssize_t ret_ssize;
	long long cache_block_size, unpin_dirty_delta;

	flock(fileno(local_metafptr), LOCK_EX);
	PREAD(fileno(local_metafptr), &filemeta, sizeof(FILE_META_TYPE),
			sizeof(HCFS_STAT));
	PREAD(fileno(local_metafptr), &bentry_page,
			sizeof(BLOCK_ENTRY_PAGE), page_pos);
	status = bentry_page.block_entries[eindex].status;
	switch (status) {
	case ST_CLOUD:
	case ST_CtoL:
		/* If this block is paged out, then do NOT deleting the
		 * block data on cloud. This case tmply let the block
		 * on cloud be orphan (it cannot be tracked by any meta),
		 * but it will be adopted after finishing syncing next time */
		flock(fileno(local_metafptr), LOCK_UN);
		write_log(10, "Debug: block_%"PRIu64"_%lld is ST_BOTH/ST_CtoL."
				"Cannot revert", (uint64_t)this_inode, blockno);
		return -ECANCELED;
	case ST_BOTH:
		/* Recover dirty size */
		fetch_block_path(blockpath, this_inode, blockno);
		ret = set_block_dirty_status(blockpath, NULL, TRUE);
		if (ret < 0) {
			write_log(0, "Error: Fail to set block dirty"
					" status in %s\n", __func__);
		}
		cache_block_size = check_file_size(blockpath);
		update_file_stats(local_metafptr, 0, 0, 0,
				cache_block_size, this_inode);
		unpin_dirty_delta = (P_IS_UNPIN(filemeta.local_pin) ?
				cache_block_size : 0);
		change_system_meta_ignore_dirty(this_inode, 0, 0, 0, 0,
						cache_block_size,
						unpin_dirty_delta, FALSE);
		/* Keep running following code */
	case ST_LtoC:
		bentry_page.block_entries[eindex].status = ST_LDISK;
		PWRITE(fileno(local_metafptr), &bentry_page,
				sizeof(BLOCK_ENTRY_PAGE), page_pos);
		flock(fileno(local_metafptr), LOCK_UN);
		write_log(10, "Debug: block_%"PRIu64"_%lld is reverted"
				" to ST_LDISK", (uint64_t)this_inode, blockno);
		break;
	default:
		/* If status is ST_NONE/ST_TODELETE/ST_LDISK, then do
		 * nothing because the block had been updated again. */
		flock(fileno(local_metafptr), LOCK_UN);
		break;
	}

	return 0;

errcode_handle:
	flock(fileno(local_metafptr), LOCK_UN);
	return errcode;
}

/**
 * Delete blocks on cloud based on progress file.
 *
 * @param progress_fd File descriptor of this progress file
 * @param total_blocks Total blocks to be checked and deleted
 * @param inode Inode number
 * @param delete_which_one Deletion type. Either DEL_TOUPLOAD_BLOCKS or
 *                         DEL_BACKEND_BLOCKS.
 *
 * @return 0 on success, otherwise negative error code.
 */ 
int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
	char delete_which_one)
{
	BLOCK_UPLOADING_PAGE tmppage;
	BLOCK_UPLOADING_STATUS *block_info;
	long long block_count;
	long long block_seq;
	long long which_page, current_page;
	long long offset;
	int ret, errcode;
	int which_curl;
	int e_index;
	ssize_t ret_ssize;
	char local_metapath[300];
	FILE *local_metafptr;
	ssize_t ret_size;
	long long page_pos;
	FILE_META_TYPE filemeta;
#if ENABLE(DEDUP)
	unsigned char block_objid[OBJID_LENGTH];
#endif

	ret = change_action(progress_fd, delete_which_one);
	if (ret < 0) {
		write_log(0, "Error: Fail to delete old blocks for inode %"
				PRIu64"\n", (uint64_t)inode);
		return ret;
	}

	if (delete_which_one == DEL_TOUPLOAD_BLOCKS)
		write_log(6, "Debug: Delete those blocks uploaded just now for "
			"inode_%"PRIu64"\n", (uint64_t)inode);

	fetch_meta_path(local_metapath, inode);
	local_metafptr = fopen(local_metapath, "r+");
	if (local_metafptr)
		setbuf(local_metafptr, NULL);

	current_page = -1;
	page_pos = 0;
	for (block_count = 0; block_count < total_blocks; block_count++) {
		which_page = block_count / BLK_INCREMENTS;

		if (current_page != which_page) {
			flock(progress_fd, LOCK_EX);
			offset = query_status_page(progress_fd, block_count);
			if (offset <= 0) {
				block_count += (BLK_INCREMENTS - 1);
				flock(progress_fd, LOCK_UN);
				continue;
			}
			PREAD(progress_fd, &tmppage,
					sizeof(BLOCK_UPLOADING_PAGE), offset);
			flock(progress_fd, LOCK_UN);

			/* When delete to-upload blocks, we need page position
			 * to recover status to ST_LDISK. TODO: Maybe do not
			 * need this action? */
			if (delete_which_one == DEL_TOUPLOAD_BLOCKS) {
				if (local_metafptr != NULL) {
					flock(fileno(local_metafptr), LOCK_EX);
					ret_size = pread(fileno(local_metafptr),
						&filemeta, sizeof(FILE_META_TYPE),
						sizeof(HCFS_STAT));
					if (ret_size == sizeof(FILE_META_TYPE))
						page_pos = seek_page2(&filemeta,
							local_metafptr,
							which_page, 0);
					else
						page_pos = 0;
					flock(fileno(local_metafptr), LOCK_UN);
				} else {
					page_pos = 0;
				}
			}

			current_page = which_page;
		}
		e_index = block_count % BLK_INCREMENTS;

		block_info = &(tmppage.status_entry[e_index]);
#if ENABLE(DEDUP)
		ret = _choose_deleted_block(delete_which_one,
			block_info, block_objid, inode);
#else
		block_seq = 0;
		ret = _choose_deleted_block(delete_which_one,
			block_info, &block_seq, inode);
#endif
		if (ret < 0) /* This block do not need to be deleted */
			continue;

		if (delete_which_one == DEL_TOUPLOAD_BLOCKS &&
				page_pos != 0) {
			/* In case of deleting those blocks just uploaded,
			 * try to revert block status if needed. */
			ret = _revert_block_status(local_metafptr, inode,
					block_count, page_pos, e_index);
			if (ret < 0) {
				if (ret == -ECANCELED) {
					continue;
				} else {
					errcode = ret;
					goto errcode_handle;
				}
			}
		}

		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
#if ENABLE(DEDUP)
		which_curl = select_upload_thread(TRUE, FALSE,
			TRUE, block_objid,
			inode, block_count, block_seq,
			page_pos, e_index, progress_fd, delete_which_one);
#else
		/* page_pos is not initiated if not DEL_TOUPLOAD_BLOCKS. That
		 * is because we do not need value of page_pos after deleting
		 * old blocks on cloud.*/
		which_curl = select_upload_thread(TRUE, FALSE,
			inode, block_count, block_seq,
			page_pos, e_index, progress_fd, delete_which_one);
#endif
		dispatch_delete_block(which_curl);
		sem_post(&(upload_ctl.upload_op_sem));
	}

	if (local_metafptr)
		fclose(local_metafptr);

	/* Wait for all deleting threads. */
	busy_wait_all_specified_upload_threads(inode);
	write_log(10, "Debug: Finish deleting unuseful blocks "
			"for inode %"PRIu64" on cloud\n", (uint64_t)inode);
	return 0;

errcode_handle:
	write_log(0, "Error: IO error in %s. Code %d\n", __func__, -errcode);
	if (local_metafptr)
		fclose(local_metafptr);
	flock(progress_fd, LOCK_UN);
	busy_wait_all_specified_upload_threads(inode);
	return errcode;
}

/**
 * Pull an inode number which has higher priority to retry to sync.
 *
 * @return an inode number if retry_list is not empty. Otherwise return 0.
 */
ino_t pull_retry_inode(IMMEDIATELY_RETRY_LIST *list)
{
	int32_t idx;
	ino_t ret_inode;

	if (list->num_retry == 0)
		return 0;

	for (idx = 0; idx < list->list_size; idx++)
		if (list->retry_inode[idx] > 0)
			break;

	if (idx < list->list_size) {
		ret_inode = list->retry_inode[idx];
		list->retry_inode[idx] = 0;
		list->num_retry--;
		return ret_inode;
	} else {
		list->num_retry = 0;
	}

	return 0;
}

/*
 * Push an inode to retry_list.
 *
 * @param inode Inode number to be retried immediately.
 *
 * @return none.
 */
void push_retry_inode(IMMEDIATELY_RETRY_LIST *list, ino_t inode)
{
	int32_t idx;

	for (idx = 0; idx < list->list_size; idx++)
		if (list->retry_inode[idx] == 0)
			break;

	if (idx < list->list_size) {
		list->retry_inode[idx] = inode;
		list->num_retry++;
	} else {
		write_log(4, "Warn: Retry list overflow?");
	}

	return;
}
