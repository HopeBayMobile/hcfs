/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: recover_super_block.c
* Abstract: The c source file for recovering entry queues in super block.
*
* Revision History
* 2016/10/24 Yuxun created this file.
**************************************************************************/

#include "recover_super_block.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "global.h"
#include "params.h"
#include "logger.h"
#include "fuseop.h"
#include "meta_mem_cache.h"
#include "hfuse_system.h"

#define MIN_INODE_NO 2

/* To check if hcfs needs to invoke SB entry queue recovery.
 *
 * @return TRUE if recovery is needed. Otherwise, return FALSE.
 */
BOOL need_recover_sb()
{
	BOOL need_recovery = FALSE;
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	double diff_t;
	time_t now_t;

	super_block_share_locking();
	sem_wait(&(hcfs_system->access_sem));

	/* Sync point is set or recovery is ongoing or
	 * restoration is ongoing. No need to run recovery again. */
	if (sys_super_block->sb_recovery_meta.is_ongoing ||
	    sys_super_block->sync_point_is_set ||
	    hcfs_system->system_restoring != NOT_RESTORING) {
		need_recovery = FALSE;
		goto cleanup;
	}

	/* Only allowed recovery once in an interval */
	now_t = time(NULL);
	diff_t =
	    difftime(now_t, sys_super_block->sb_recovery_meta.last_recovery_ts);
	if ((int32_t)diff_t <= MIN_RECOVERY_INTERVAL) {
		need_recovery = FALSE;
		goto cleanup;
	}

	/* Find incomplete recovery job */
	fetch_recover_progressf_path(progressf_path);
	if (access(progressf_path, F_OK) != -1) {
		need_recovery = TRUE;
		goto cleanup;
	}

	/* Num_dirty & dirty cache size not matched */
	if (sys_super_block->head.num_dirty == 0 &&
	    (hcfs_system->systemdata.dirty_cache_size != 0 ||
	     hcfs_system->systemdata.unpin_dirty_data_size != 0)) {
		need_recovery = TRUE;
		goto cleanup;
	}

	/* Num_dirty & dirty_inode_queue not matched. Only check first dirty
	 * inode here because of last dirty inode is always updated with first
	 * dirty inode in one write operation. */
	if ((sys_super_block->head.num_dirty == 0 &&
	     sys_super_block->head.first_dirty_inode != 0) ||
	    (sys_super_block->head.num_dirty != 0 &&
	     sys_super_block->head.first_dirty_inode == 0)) {
		need_recovery = TRUE;
		goto cleanup;
	}

cleanup:
	sem_post(&(hcfs_system->access_sem));
	super_block_share_release();

	return need_recovery;
}

/* Helper function to get recovery_progress_file path
 */
void fetch_recover_progressf_path(char *pathname)
{
	sprintf(pathname, "%s/%s", METAPATH, PROGRESS_FILE);
	return;
}

/* To fetch last recovery progress. To check if there were existed
 * "recovery_progress_file " and read the progress record in it.
 *
 * @return 0 if successful. Otherwise, return -1 on errors.
 */
int32_t fetch_last_recover_progress(ino_t *start_inode, ino_t *end_inode)
{
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	int64_t rsize;
	FILE *fptr;
	ino_t inodes[2];

	rsize = 0;
	*start_inode = *end_inode = 0;

	fetch_recover_progressf_path(progressf_path);

	fptr = fopen(progressf_path, "r");
	if (fptr == NULL) {
		write_log(0, "In %s, error - %d", __func__, errno);
		return -1;
	}

	rsize = fread(inodes, sizeof(ino_t) * 2, 1, fptr);
	if (rsize <= 0) {
		write_log(0, "In %s, error - %d", __func__, errno);
		fclose(fptr);
		return -1;
	}

	*start_inode = inodes[0];
	*end_inode = inodes[1];

	fclose(fptr);
	return 0;
}

/* To log progress of this recovery. Will create/update a file named
 * "recover_dirty_in_progress" to record the inode number now processed.
 *
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t update_recover_progress(ino_t start_inode, ino_t end_inode)
{
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	char tmp_progressf_path[METAPATHLEN + strlen(PROGRESS_FILE) + 4];
	int64_t wsize = 0;
	FILE *fptr;
	ino_t inodes[2];

	fetch_recover_progressf_path(progressf_path);
	sprintf(tmp_progressf_path, "%s.tmp", progressf_path);

	fptr = fopen(tmp_progressf_path, "w");
	if (fptr == NULL) {
		write_log(0, "In %s, error - %d", __func__, errno);
		return -1;
	}
	setbuf(fptr, NULL);

	inodes[0] = start_inode;
	inodes[1] = end_inode;
	wsize = fwrite(inodes, sizeof(ino_t) * 2, 1, fptr);
	if (wsize <= 0) {
		write_log(0, "In %s, error - %d", __func__, errno);
		fclose(fptr);
		unlink(tmp_progressf_path);
		return -1;
	}

	fclose(fptr);
	rename(tmp_progressf_path, progressf_path);
	return 0;
}

/* Helper function to unlink recover progress file.
 */
void unlink_recover_progress_file()
{
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];

	fetch_recover_progressf_path(progressf_path);
	unlink(progressf_path);
}

/* To set the recovery variables in superblock control
 */
void set_recovery_flag(BOOL is_ongoing, ino_t start_inode, ino_t end_inode)
{
	BOOL old_ongoing = sys_super_block->sb_recovery_meta.is_ongoing;

	/* superblock control */
	sys_super_block->sb_recovery_meta.is_ongoing = is_ongoing;
	sys_super_block->sb_recovery_meta.start_inode =
	    (is_ongoing) ? start_inode : 0;
	sys_super_block->sb_recovery_meta.end_inode =
	    (is_ongoing) ? end_inode : 0;

	/* Record the timestamp when status ongoing => not ongoing */
	if (old_ongoing && !is_ongoing)
		sys_super_block->sb_recovery_meta.last_recovery_ts = time(NULL);

	return;
}

/* To reset all parameters about dirty queue.
 * In superblock head -
 * 	num_dirty, first_dirty_inode, last_dirty_inode
 * In systemdata -
 * 	dirty_cache_size, unpin_dirty_data_size
 */
void reset_queue_and_stat()
{
	/* superblock */
	sys_super_block->head.num_dirty = 0;
	sys_super_block->head.first_dirty_inode = 0;
	sys_super_block->head.last_dirty_inode = 0;

	/* systemdata */
	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.dirty_cache_size = 0;
	hcfs_system->systemdata.unpin_dirty_data_size = 0;
	sync_hcfs_system_data(FALSE);
	sem_post(&(hcfs_system->access_sem));

	return;
}

/* Helper function to batch read sb entries to buf.
 *
 * @param buf         Entries read from sb file will store in buf.
 *        start_inode The first inode of this read.
 *        num         Read num entries at once.
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t _batch_read_sb_entries(SUPER_BLOCK_ENTRY *buf,
			       ino_t start_inode,
			       int64_t num)
{
	int32_t rsize, errcode;

	rsize = pread(sys_super_block->iofptr, buf, SB_ENTRY_SIZE * num,
		      SB_HEAD_SIZE + (start_inode - 1) * SB_ENTRY_SIZE);
	if (rsize < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		return -errcode;
	}
	if (rsize < (SB_ENTRY_SIZE * num)) {
		write_log(0, "Error in %s. Short-read in reading sb entry.\n",
			  __func__);
		return -EIO;
	}
	return 0;
}

/* Helper function to batch write sb entries to sb file.
 *
 * @param buf         Writes bytes in buf to sb file.
 *        start_inode The first inode of this read.
 *        num         Write num entries at once.
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t _batch_write_sb_entries(SUPER_BLOCK_ENTRY *buf,
				ino_t start_inode,
				int64_t num)
{
	int32_t wsize, errcode;

	wsize = pwrite(sys_super_block->iofptr, buf, SB_ENTRY_SIZE * num,
		       SB_HEAD_SIZE + (start_inode - 1) * SB_ENTRY_SIZE);
	if (wsize < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		return -errcode;
	}
	if (wsize < (SB_ENTRY_SIZE * num)) {
		write_log(0, "Error in %s. Short-write in reading sb entry.\n",
			  __func__);
		return -EIO;
	}
	return 0;
}

/* Helper function to read struct FILE_STATS_TYPE recorded in regfile
 * meta file.
 */
int32_t _read_regfile_meta(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
			   FILE_STATS_TYPE *file_stats)
{
	BOOL open_file = FALSE;
	int32_t ret_code = 0;
	int64_t rsize = 0;

	if ((meta_cache_entry->meta_opened == FALSE) ||
	    (meta_cache_entry->fptr == NULL)) {
		ret_code = meta_cache_open_file(meta_cache_entry);
		if (ret_code < 0) {
			write_log(0,
				  "Error open meta file of inode %" PRIu64 ".",
				  meta_cache_entry->inode_num);
			return -1;
		}
		open_file = TRUE;
	}

	rsize = pread(fileno(meta_cache_entry->fptr), file_stats,
		      sizeof(FILE_STATS_TYPE),
		      sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE));
	if (rsize < (int64_t)sizeof(FILE_STATS_TYPE)) {
		write_log(0,
			  "Error read meta of inode %" PRIu64 ". Error code %d",
			  meta_cache_entry->inode_num, errno);
		if (open_file)
			meta_cache_close_file(meta_cache_entry);
		return -1;
	}

	if (open_file)
		meta_cache_close_file(meta_cache_entry);

	return 0;
}

/* To walk all entries in superblock and reconstruct the entry
 * queue. The entries in queue will be linked in sequentail
 * order by inode number after reconstructed.
 *
 * Note - Only recovery dirty entries now. Will support all types
 *        of entry queues later.
 *
 * Original Queue: (inode 1) -> (inode 3) -> (inode 2) -> (inode 4)
 * After reconstruct: (inode 1) -> (inode 2) -> (inode 3) -> (inode 4)
 *
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t reconstruct_sb_entries(SUPER_BLOCK_ENTRY *sb_entry_arr,
			       META_CACHE_ENTRY_STRUCT **meta_cache_arr,
			       int64_t num_entry_handle,
			       RECOVERY_ROUND_DATA *this_round)
{
	BOOL find_first_dirty = FALSE;
	int32_t ret_code;
	int64_t round_dirty_data_size, round_dirty_meta_size, idx;
	SUPER_BLOCK_ENTRY tempentry;
	SUPER_BLOCK_ENTRY *sbentry_prev, *sbentry_cur;
	FILE_STATS_TYPE file_stats;

	if (!sb_entry_arr || !meta_cache_arr || !this_round ||
	    num_entry_handle > MAX_NUM_ENTRY_HANDLE)
		return -EINVAL;

	memset(this_round, 0, sizeof(RECOVERY_ROUND_DATA));
	memset(&tempentry, 0, SB_ENTRY_SIZE);

	if (sys_super_block->head.last_dirty_inode != 0) {
		ret_code = read_super_block_entry(
		    sys_super_block->head.last_dirty_inode, &tempentry);
		if (ret_code < 0)
			return ret_code;
	}
	sbentry_prev = &tempentry;

	for (idx = 0; idx < num_entry_handle; idx++) {
		sbentry_cur = &sb_entry_arr[idx];
		if (sbentry_cur->status != IS_DIRTY)
			continue;
		/* Already check file type of this inode when locking
		 * meta_cache_entry. Pointer in array is NULL means that no need
		 * to read the dirty_data_size from meta file.
		 */
		if (meta_cache_arr[idx] != NULL) {
			ret_code = _read_regfile_meta(meta_cache_arr[idx],
						      &file_stats);
			if (ret_code < 0) {
				write_log(0,
					  "Error when recovering inode %" PRIu64
					  ". "
					  "Skip this entry and continue.",
					  sbentry_cur->this_index);
				continue;
			}
			write_log(8,
				  "In %s, Inode %" PRIu64 " has dirty size %ld",
				  __func__, sbentry_cur->this_index,
				  file_stats.dirty_data_size);

			/* Record dirty stats */
			round_dirty_data_size =
			    round_size(file_stats.dirty_data_size);
			round_dirty_meta_size =
			    round_size(sbentry_cur->dirty_meta_size);

			this_round->dirty_size_delta +=
			    (round_dirty_data_size + round_dirty_meta_size);

			if (sbentry_cur->pin_status != ST_PINNING &&
			    sbentry_cur->pin_status != ST_PIN)
				this_round->unpin_dirty_size_delta +=
				    round_dirty_data_size;
		} else {
			round_dirty_meta_size =
			    round_size(sbentry_cur->dirty_meta_size);
			this_round->dirty_size_delta += round_dirty_meta_size;
		}

		sbentry_cur->util_ll_prev = sbentry_prev->this_index;
		sbentry_prev->util_ll_next = sbentry_cur->this_index;
		if (!find_first_dirty) {
			/* Record stats/parms changes this round */
			memcpy(&(this_round->prev_last_entry), sbentry_prev,
			       SB_ENTRY_SIZE);
			memcpy(&(this_round->this_first_entry), sbentry_cur,
			       SB_ENTRY_SIZE);
			find_first_dirty = TRUE;
		}
		sbentry_prev = sbentry_cur;
		this_round->this_num_dirty += 1;
	}
	/* sbentry_prev will be the last dirty entry in array */
	sbentry_prev->util_ll_next = 0;

	/* Record stats/parms changes this round */
	memcpy(&(this_round->this_last_entry), sbentry_prev, SB_ENTRY_SIZE);

	return 0;
}

/* Helper function to update reconstruct result every round.
 * Following items will be updated by this function -
 *   1. Parameters in super block head
 *   2. Statistics in systemdata
 *   3. Last dirty entry processed in previous round
 *
 * @param round_data RECOVERY_ROUND_DATA returned by
 *                   reconstruct_dirty_entries fn.
 *
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t update_reconstruct_result(RECOVERY_ROUND_DATA round_data)
{
	int32_t ret_code;
	ino_t old_first_dirty, old_last_dirty, old_num_dirty;
	SUPER_BLOCK_ENTRY *tmpentry;

	/* No dirty entry processed */
	if (round_data.this_num_dirty == 0)
		return 0;

	/* Update parms of sb head and previous last dirty entry */
	tmpentry = &(round_data.prev_last_entry);
	if (tmpentry->this_index != 0) {
		ret_code =
		    write_super_block_entry(tmpentry->this_index, tmpentry);
		if (ret_code < 0) {
			write_log(0, "In %s. %s",
				  "Error when updating last dirty entry.",
				  __func__);
			return ret_code;
		}
	}

	/* Preserve older SB head */
	old_first_dirty = sys_super_block->head.first_dirty_inode;
	old_last_dirty = sys_super_block->head.last_dirty_inode;
	old_num_dirty = sys_super_block->head.num_dirty;

	/* Update new SB head */
	tmpentry = &(round_data.this_first_entry);
	if (sys_super_block->head.first_dirty_inode == 0)
		sys_super_block->head.first_dirty_inode = tmpentry->this_index;

	tmpentry = &(round_data.this_last_entry);
	sys_super_block->head.last_dirty_inode = tmpentry->this_index;
	sys_super_block->head.num_dirty += round_data.this_num_dirty;
	ret_code = write_super_block_head();
	if (ret_code < 0) {
		write_log(0, "In %s. Error when updating sb head", __func__);
		/* Rollback SB head */
		sys_super_block->head.first_dirty_inode = old_first_dirty;
		sys_super_block->head.last_dirty_inode = old_last_dirty;
		sys_super_block->head.num_dirty = old_num_dirty;
		return ret_code;
	}

	/* System statistics */
	change_system_meta(0, 0, 0, 0, round_data.dirty_size_delta,
			   round_data.unpin_dirty_size_delta, TRUE);

	return 0;
}

#define _CALL_N_CHECK_RETCODE(func_call, errmsg)                               \
	do {                                                                   \
		ret_code = func_call;                                          \
		if (ret_code < 0) {                                            \
			write_log(0, "%s %s", error_msg, errmsg);              \
			super_block_exclusive_release();                       \
			free(sb_entry_arr);                                    \
			free(meta_cache_arr);                                  \
			set_recovery_flag(FALSE, 0, 0);                        \
			pthread_exit((void *)-1);                              \
			return NULL;                                           \
		}                                                              \
	} while (0)

void *recover_sb_queue_worker(void *ptr __attribute__((unused)))
{
	char error_msg[] = "SB queue recovery worker aborted.";
	int32_t ret_code, count;
	int64_t buf_size, num_entry_handle;
	ino_t start_inode, end_inode;
	SUPER_BLOCK_ENTRY *sb_entry_arr = NULL;
	META_CACHE_ENTRY_STRUCT **meta_cache_arr = NULL;
	META_CACHE_ENTRY_STRUCT *tmp_meta_cache_entry = NULL;

	RECOVERY_ROUND_DATA recover_round;

	write_log(4, "Start to recover SB entries.");

	super_block_exclusive_locking();
	if (sys_super_block->sb_recovery_meta.is_ongoing ||
	    sys_super_block->sync_point_is_set ||
	    hcfs_system->system_restoring != NOT_RESTORING) {
		write_log(0, "%s Recovery not allowed. (sync point flag - %d, "
			     "recovery flag - %d, restoration flag - %d)",
			  error_msg, sys_super_block->sync_point_is_set,
			  sys_super_block->sb_recovery_meta.is_ongoing,
			  hcfs_system->system_restoring);
		super_block_exclusive_release();
		pthread_exit((void *)-1);
		return NULL;
	}

	buf_size = MAX_NUM_ENTRY_HANDLE * SB_ENTRY_SIZE;
	sb_entry_arr = (SUPER_BLOCK_ENTRY*)calloc(1, buf_size);
	if (sb_entry_arr == NULL) {
		write_log(0, "%s Error allocate memory.", error_msg);
		super_block_exclusive_release();
		pthread_exit((void *)-1);
		return NULL;
	}

	buf_size = MAX_NUM_ENTRY_HANDLE * sizeof(META_CACHE_ENTRY_STRUCT*);
	meta_cache_arr = (META_CACHE_ENTRY_STRUCT**)calloc(1, buf_size);
	if (meta_cache_arr == NULL) {
		write_log(0, "%s Error allocate memory.", error_msg);
		super_block_exclusive_release();
		free(sb_entry_arr);
		pthread_exit((void *)-1);
		return NULL;
	}

	/* Is there an unfinished recovery job? */
	ret_code = fetch_last_recover_progress(&start_inode, &end_inode);
	if (ret_code < 0) {
		/* New recovery process */
		start_inode = MIN_INODE_NO;
		end_inode =
		    sys_super_block->head.num_total_inodes + MIN_INODE_NO - 1;
		_CALL_N_CHECK_RETCODE(update_recover_progress(start_inode, end_inode),
				      "Error log recovery progress.");
		/* Reset parameters about dirty queue */
		reset_queue_and_stat();
	}
	/* Set recovery ongoing flag */
	set_recovery_flag(TRUE, start_inode, end_inode);
	super_block_exclusive_release();

	while ((start_inode < end_inode) &&
	       (hcfs_system->system_going_down == FALSE)) {
		if ((start_inode + MAX_NUM_ENTRY_HANDLE) <= end_inode)
			num_entry_handle = MAX_NUM_ENTRY_HANDLE;
		else
			num_entry_handle = end_inode - start_inode + 1;

		/* Lock meta_cache_entry if target inode is regular file.
		 * Many fuse operations lock meta_mem_cache first and
		 * superblock next, this will cause deadlock if we don't
		 * lock meta_mem_cache before lock superblock.*/
		for (count = 0; count < num_entry_handle; count++) {
			tmp_meta_cache_entry =
				meta_cache_lock_entry(start_inode + count);
			if (tmp_meta_cache_entry == NULL ||
			    !S_ISFILE(tmp_meta_cache_entry->this_stat.mode)) {
				meta_cache_arr[count] = NULL;
				meta_cache_unlock_entry(tmp_meta_cache_entry);
				continue;
			}
			meta_cache_arr[count] = tmp_meta_cache_entry;
		}

		super_block_exclusive_locking();

		_CALL_N_CHECK_RETCODE(_batch_read_sb_entries(sb_entry_arr,
							     start_inode,
							     num_entry_handle),
				      "Error read sb entries.");

		_CALL_N_CHECK_RETCODE(
		    reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					   num_entry_handle, &recover_round),
		    "Error reconstruct dirty entries.");

		_CALL_N_CHECK_RETCODE(_batch_write_sb_entries(sb_entry_arr,
							      start_inode,
							      num_entry_handle),
				      "Error write sb entries.");

		_CALL_N_CHECK_RETCODE(update_reconstruct_result(recover_round),
				      "Error update reconstruct result.");

		/* Prepare for next round */
		start_inode += num_entry_handle;

		_CALL_N_CHECK_RETCODE(update_recover_progress(start_inode, end_inode),
				      "Error log recovery progress.");

		set_recovery_flag(TRUE, start_inode, end_inode);

		/* Unlock for a while to avoid block other ops too much time */
		super_block_exclusive_release();

		/* Unlock all locked meta_cache_entry */
		for (count = 0; count < num_entry_handle; count++) {
			if (meta_cache_arr[count] != NULL) {
				meta_cache_unlock_entry(meta_cache_arr[count]);
			}
		}
	}
	/* Recovery was finished */
	free(sb_entry_arr);
	free(meta_cache_arr);
	set_recovery_flag(FALSE, 0, 0);
	unlink_recover_progress_file();

	write_log(4, "Recover SB entries finished.");

	pthread_exit((void*)0);
	return NULL;
}

/* To start superblock queue recovery worker. */
void start_sb_recovery()
{
	pthread_t recover_thread;
	pthread_create(&recover_thread, NULL, &recover_sb_queue_worker, NULL);
	return;
}

