/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: recover_super_block.c
* Abstract: The c source file for recovering queues in super block.
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

#include "global.h"
#include "params.h"
#include "logger.h"
#include "fuseop.h"
#include "meta_mem_cache.h"
#include "hfuse_system.h"

#define MIN_INODE_NO 2

/* To check if recovery is needed.
 *
 * @return TRUE if needed. Otherwise, FALSE.
 */
BOOL need_recover_sb()
{
	BOOL need_recovery = FALSE;
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	double diff_t;
	time_t now_t;

	super_block_share_locking();
	sem_wait(&(hcfs_system->access_sem));

	/* Sync point is set or recovery is ongoing.
	 * No need to run recovery again. */
	if (sys_super_block->sb_recovery_meta.is_ongoing ||
			sys_super_block->sync_point_is_set) {
		need_recovery = FALSE;
		goto cleanup;
	}

	/* Only allowed recovery once in an interval */
	now_t = time(NULL);
	diff_t = difftime(now_t,
			sys_super_block->sb_recovery_meta.last_recovery_ts);
	if ((int32_t)diff_t <= MIN_RECOVERY_INTERVAL) {
		need_recovery = FALSE;
		goto cleanup;
	}

	/* Restoration is ongoing */
	if (hcfs_system->system_restoring != NOT_RESTORING) {
		need_recovery = FALSE;
		goto cleanup;
	}

	/* Find incomplete recovery job */
	fetch_recover_progressf_path(progressf_path);
	if (!sys_super_block->sb_recovery_meta.is_ongoing &&
			(access(progressf_path, F_OK) != -1)) {
		need_recovery = TRUE;
		goto cleanup;
	}

	/* Num_dirty & dirty cache size not matched */
	if (sys_super_block->head.num_dirty == 0 &&
			hcfs_system->systemdata.dirty_cache_size != 0) {
		need_recovery = TRUE;
		goto cleanup;
	}

	/* Num_dirty & dirty_inode_queue not matched.
	 * First dirty inode will always be equal to
	 * last dirty inode since they had been written
	 * in one write operation */
	if (sys_super_block->head.num_dirty == 0 ||
			sys_super_block->head.first_dirty_inode == 0) {
		if ((sys_super_block->head.num_dirty +
					sys_super_block->head.first_dirty_inode) != 0) {
			need_recovery = TRUE;
			goto cleanup;
		}
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
 * @return last inode number processed, 0 if no unfinished recovery
 *         and 0 on error.
 */
ino_t fetch_last_recover_progress()
{
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	int64_t rsize;
	FILE *fptr;
	ino_t inode;

	rsize = 0;

	fetch_recover_progressf_path(progressf_path);

	fptr = fopen(progressf_path, "r");
	if (fptr == NULL) {
		write_log(0, "In %s, error - %d", __func__, errno);
		return 0;
	}

	rsize = fread(&inode, sizeof(ino_t), 1, fptr);
	if (rsize <= 0) {
		write_log(0, "In %s, error - %d", __func__, errno);
		fclose(fptr);
		return 0;
	}

	fclose(fptr);
	return inode;
}

/* To log progress of this recovery. Will create/update a file named
 * "recover_dirty_in_progress" to record the inode number now processed.
 *
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t log_recover_progress(ino_t now_inode)
{
	char progressf_path[METAPATHLEN + strlen(PROGRESS_FILE)];
	int64_t wsize = 0;
	FILE *fptr;

	fetch_recover_progressf_path(progressf_path);

	fptr = fopen(progressf_path, "w");
	if (fptr == NULL) {
		write_log(0, "In %s, error - %d", __func__, errno);
		return -1;
	}
	setbuf(fptr, NULL);

	wsize = fwrite(&now_inode, sizeof(ino_t), 1, fptr);
	if (wsize <= 0) {
		write_log(0, "In %s, error - %d", __func__, errno);
		fclose(fptr);
		unlink(progressf_path);
		return -1;
	}

	fclose(fptr);
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
void set_recovery_flag(BOOL is_ongoing, ino_t start_inode,
		ino_t end_inode)
{
	BOOL old_ongoing =
		sys_super_block->sb_recovery_meta.is_ongoing;

	/* superblock control */
	sys_super_block->sb_recovery_meta.is_ongoing = is_ongoing;
	sys_super_block->sb_recovery_meta.start_inode =
		(is_ongoing) ? start_inode : 0;
	sys_super_block->sb_recovery_meta.end_inode =
		(is_ongoing) ? end_inode : 0;

	/* Record the timestamp when status ongoing => not ongoing */
	if (old_ongoing && !is_ongoing)
		sys_super_block->sb_recovery_meta.last_recovery_ts
			= time(NULL);

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
	/* TODO - Maybe need to consider cache size too */
	/* TODO - Need to check if deadlock will occur here */
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
		ino_t start_inode, int64_t num)
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
		ino_t start_inode, int64_t num)
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
int32_t _read_regfile_meta(ino_t this_inode, FILE_STATS_TYPE *file_stats)
{
	int32_t ret_code = 0;
	int64_t rsize = 0;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;

	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "Error lock meta cache of inode %"PRIu64".",
				this_inode);
		goto error_handle;
	}

	if ((meta_cache_entry->meta_opened == FALSE) || (meta_cache_entry->fptr == NULL))
		ret_code = meta_cache_open_file(meta_cache_entry);
		if (ret_code < 0) {
			write_log(0, "Error open meta file of inode %"PRIu64".",
				this_inode);
		        goto error_handle;
		}

        rsize = pread(fileno(meta_cache_entry->fptr), file_stats, sizeof(FILE_STATS_TYPE),
			sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE));
        if (rsize < (int64_t)sizeof(FILE_STATS_TYPE)) {
		write_log(0, "Error read meta of inode %"PRIu64". Error code %d",
				this_inode, errno);
		goto error_handle;
	}

	goto done;

error_handle:
	ret_code = -1;
done:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	return ret_code;
}

/* To walk all entries in superblock and reconstruct the entry
 * queue. The entries in queue will be linked in sequentail
 * order by inode number after reconstructed.
 *
 * Note - Only recovery dirty entries now. Will support all types
 *        of entry queues later.
 *
 * Original Queue: 1 -> 3 -> 2 -> 3
 * After reconstruct: 1 -> 2 -> 3 -> 4
 *
 * @return 0 on success. Otherwise, the negation of error code.
 */
int32_t reconstruct_sb_entries(SUPER_BLOCK_ENTRY *sb_entry_arr,
		int64_t num_entry_handle,
		RECOVERY_ROUND_DATA *this_round)
{
	BOOL find_first_dirty = FALSE;
	int32_t ret_code;
	int64_t round_dirty_data_size, round_dirty_meta_size, idx;
	SUPER_BLOCK_ENTRY tempentry;
	SUPER_BLOCK_ENTRY *sbentry1, *sbentry2;
	FILE_STATS_TYPE file_stats;

	memset(this_round, 0, sizeof(RECOVERY_ROUND_DATA));
	memset(&tempentry, 0, SB_ENTRY_SIZE);

	if (sys_super_block->head.last_dirty_inode != 0) {
		ret_code = read_super_block_entry(
				sys_super_block->head.last_dirty_inode,
				&tempentry);
		if (ret_code < 0)
			return ret_code;
	}
	sbentry1 = &tempentry;

	for (idx = 0; idx < num_entry_handle; idx++) {
		sbentry2 = (sb_entry_arr + idx);
		if (sbentry2->status != IS_DIRTY)
			continue;
		/* Read meta data here */
		if (S_ISFILE(sbentry2->inode_stat.mode)) {
			ret_code = _read_regfile_meta(sbentry2->this_index, &file_stats);
			if (ret_code < 0) {
				write_log(0, "Error when recovering inode %"PRIu64". "
						"Skip this entry and continue.",
						sbentry2->this_index);
				continue;
			}
			write_log(8, "In %s, Inode %"PRIu64" has dirty size %ld",
					__func__, sbentry2->this_index,
					file_stats.dirty_data_size);

			/* Record dirty stats */
			round_dirty_data_size = round_size(file_stats.dirty_data_size);
			round_dirty_meta_size = round_size(sbentry2->dirty_meta_size);

			this_round->dirty_size_delta +=
				(round_dirty_data_size +
				 round_dirty_meta_size);
			if (sbentry2->pin_status != ST_PINNING &&
					sbentry2->pin_status != ST_PIN)
				this_round->unpin_dirty_size_delta += round_dirty_data_size;
		} else {
			round_dirty_meta_size = round_size(sbentry2->dirty_meta_size);
			this_round->dirty_size_delta += round_dirty_meta_size;
		}

		sbentry2->util_ll_prev = sbentry1->this_index;
		sbentry1->util_ll_next = sbentry2->this_index;
		if (!find_first_dirty) {
			/* Record stats/parms changes this round */
			memcpy(&(this_round->prev_last_entry),
					sbentry1, SB_ENTRY_SIZE);
			memcpy(&(this_round->this_first_entry),
					sbentry2, SB_ENTRY_SIZE);
			find_first_dirty = TRUE;
		}
		sbentry1 = sbentry2;
		this_round->this_num_dirty += 1;
	}
	/* sbentry1 will be the last dirty entry in array */
	sbentry1->util_ll_next = 0;

	/* Record stats/parms changes this round */
	memcpy(&(this_round->this_last_entry), sbentry1, SB_ENTRY_SIZE);

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
	SUPER_BLOCK_ENTRY *tmpentry;

	/* Update parms of sb head and previous last dirty entry */
	tmpentry = &(round_data.prev_last_entry);
	if (tmpentry->this_index != 0) {
		ret_code =
			write_super_block_entry(tmpentry->this_index,
					tmpentry);
		if (ret_code < 0) {
			write_log(0, "In %s. %s",
					"Error when updating last dirty entry.",
					__func__);
			return ret_code;
		}
	}

	tmpentry = &(round_data.this_first_entry);
	if (sys_super_block->head.first_dirty_inode == 0)
		sys_super_block->head.first_dirty_inode =
			tmpentry->this_index;

	tmpentry = &(round_data.this_last_entry);
	sys_super_block->head.last_dirty_inode = tmpentry->this_index;
	sys_super_block->head.num_dirty += round_data.this_num_dirty;
	ret_code = write_super_block_head();
	if (ret_code < 0) {
		write_log(0, "In %s. Error when updating sb head", __func__);
		return ret_code;
	}

	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.dirty_cache_size +=
		round_data.dirty_size_delta;
	hcfs_system->systemdata.unpin_dirty_data_size +=
		round_data.unpin_dirty_size_delta;
	sync_hcfs_system_data(FALSE);
	sem_post(&(hcfs_system->access_sem));

	return 0;
}

#define _CALL_N_CHECK_RETCODE(func_call, errmsg) \
	do {\
		ret_code = func_call;\
		if (ret_code < 0) {\
			write_log(0, "%s %s", error_msg, errmsg);\
			super_block_exclusive_release();\
			pthread_exit((void*)-1);\
		}\
	} while (0)

void dump_sb_entries();
void *recover_sb_queue_worker(void *ptr __attribute__((unused)))
{
	char error_msg[] = "Dirty queue recovery worker aborted.";
	int32_t ret_code;
	int64_t buf_size, num_entry_handle;
	ino_t start_inode, end_inode;
	SUPER_BLOCK_ENTRY *sb_entry_arr = NULL;
	RECOVERY_ROUND_DATA recover_round;

	write_log(4, "Start to recvoer dirty queue in superblock.");

	super_block_exclusive_locking();
	if (sys_super_block->sb_recovery_meta.is_ongoing ||
			sys_super_block->sync_point_is_set) {
		write_log(0, "%s Sync point is set or another recovery is ongoing. "
				"Sync point flag - %d, recovery flag - %d.",
				error_msg,
				sys_super_block->sync_point_is_set,
				sys_super_block->sb_recovery_meta.is_ongoing);
		super_block_exclusive_release();
		pthread_exit((void *)-1);
	}

	buf_size = MAX_NUM_ENTRY_HANDLE * SB_ENTRY_SIZE;
	sb_entry_arr = (SUPER_BLOCK_ENTRY*)calloc(1, buf_size);
	if (sb_entry_arr == NULL) {
		write_log(0, "%s Error allocate memory.", error_msg);
		super_block_exclusive_release();
		pthread_exit((void *)-1);
	}

	/* Is there an unfinished recovery job? */
	start_inode = fetch_last_recover_progress();
	if (start_inode == 0) {
		/* Init recovery */
		start_inode = MIN_INODE_NO;
		_CALL_N_CHECK_RETCODE(
				log_recover_progress(start_inode),
				"Error log recovery progress.");

		/* Reset parameters about dirty queue */
		reset_queue_and_stat();
	}
	end_inode = sys_super_block->head.num_total_inodes + MIN_INODE_NO - 1;
	/* Set recovery ongoing flag */
	set_recovery_flag(TRUE, start_inode, end_inode);
	super_block_exclusive_release();

	while (start_inode < end_inode) {
		super_block_exclusive_locking();

		if ((start_inode + MAX_NUM_ENTRY_HANDLE) <= end_inode)
			num_entry_handle = MAX_NUM_ENTRY_HANDLE;
		else
			num_entry_handle = end_inode - start_inode + 1;

		_CALL_N_CHECK_RETCODE(
				_batch_read_sb_entries(sb_entry_arr, start_inode,
					num_entry_handle),
				"Error read sb entries.");

		_CALL_N_CHECK_RETCODE(
				reconstruct_sb_entries(sb_entry_arr, num_entry_handle,
					&recover_round),
				"Error reconstruct dirty entries.");

		_CALL_N_CHECK_RETCODE(
				_batch_write_sb_entries(sb_entry_arr, start_inode,
					num_entry_handle),
				"Error write sb entries.");

		_CALL_N_CHECK_RETCODE(
				update_reconstruct_result(recover_round),
				"Error update reconstruct result.");

		start_inode += num_entry_handle;

		_CALL_N_CHECK_RETCODE(
				log_recover_progress(start_inode),
				"Error log recovery progress.");

		/* Unlock for a while to avoid block other ops too much time */
		super_block_exclusive_release();
	}
	/* Recovery was finished */
	free(sb_entry_arr);
	set_recovery_flag(FALSE, 0, 0);
	unlink_recover_progress_file();

	write_log(4, "Recvoer dirty queue in superblock finished.");

	/* Test code */
	dump_sb_entries();
	/* End test code */

	pthread_exit((void*)0);
	return NULL;
}

/* To start superblock queue recovery worker.
 */
void start_sb_recovery()
{
	pthread_t recover_thread;
	pthread_create(&recover_thread, NULL,
			&recover_sb_queue_worker, NULL);
	return;
}

/* Test code */
void dump_sb_entries()
{
	char error_msg[] = "DUMP SB ERROR.";
	char dump_file_path[] = "/data/dump_sb.dat";
	char buf[128];
	int32_t ret_code;
	int64_t buf_size, num_entry_handle, idx;
	ino_t start_inode, end_inode;
	FILE *dump_fptr;
	SUPER_BLOCK_ENTRY *sb_entry_arr = NULL;
	SUPER_BLOCK_ENTRY *sbentry2;

	dump_fptr = fopen(dump_file_path, "w");
	if (dump_fptr == NULL) 	return;
	setbuf(dump_fptr, NULL);

	super_block_exclusive_locking();

	buf_size = MAX_NUM_ENTRY_HANDLE * SB_ENTRY_SIZE;
	sb_entry_arr = (SUPER_BLOCK_ENTRY*)calloc(1, buf_size);
	if (sb_entry_arr == NULL) {
		fprintf(dump_fptr, "%s Error allocate memory.\n", error_msg);
		write_log(0, "%s Error allocate memory.", error_msg);
		super_block_exclusive_release();
		return;
	}
	start_inode = MIN_INODE_NO;
	end_inode = sys_super_block->head.num_total_inodes + MIN_INODE_NO - 1;

	fprintf(dump_fptr, "SYSTEM DATA:\n");
	fprintf(dump_fptr,
		"Cache Size - %ld, Dirty Cache Size - %ld, Unpin Dirty Size - %ld\n\n",
			hcfs_system->systemdata.cache_size,
			hcfs_system->systemdata.dirty_cache_size,
			hcfs_system->systemdata.unpin_dirty_data_size);

	strftime(buf, 128, "%F %T",
			localtime(&sys_super_block->sb_recovery_meta.last_recovery_ts));
	fprintf(dump_fptr, "SB CONTROL:\n");
	fprintf(dump_fptr, "Recovery sb is ongoing - %s, "
			"Recovery sb start_inode - %ld, "
			"Recovery sb end_inode - %ld, "
			"Last recovery timestamp - %s\n\n",
			(sys_super_block->sb_recovery_meta.is_ongoing) ? "TRUE" : "FALSE",
			sys_super_block->sb_recovery_meta.start_inode,
			sys_super_block->sb_recovery_meta.end_inode,
			buf);

	fprintf(dump_fptr, "SB HEAD:\n");
	fprintf(dump_fptr, "First Dirty - %ld, Last Dirty - %ld, Num Dirty - %ld\n\n",
			sys_super_block->head.first_dirty_inode,
			sys_super_block->head.last_dirty_inode,
			sys_super_block->head.num_dirty);
	fprintf(dump_fptr, "SB ENTRIES:\n");

	while (start_inode < end_inode) {
		if ((start_inode + MAX_NUM_ENTRY_HANDLE) <= end_inode)
			num_entry_handle = MAX_NUM_ENTRY_HANDLE;
		else
			num_entry_handle = end_inode - start_inode + 1;

		ret_code = _batch_read_sb_entries(sb_entry_arr, start_inode,
				num_entry_handle);
		if (ret_code < 0) {
			fprintf(dump_fptr, "%s Error read sb entries.\n", error_msg);
			write_log(0, "%s Error read sb entries", error_msg);
			super_block_exclusive_release();
			return;
		}

		for (idx = 0; idx < num_entry_handle; idx++) {
			sbentry2 = (sb_entry_arr+idx);
			fprintf(dump_fptr,
				"Ino %ld,\t\tstatus %d,\t\tpin status %d,\t\tprev %ld,\t\tnext %ld\n",
					sbentry2->this_index,
					sbentry2->status,
					sbentry2->pin_status,
					sbentry2->util_ll_prev,
					sbentry2->util_ll_next);
			write_log(0, "DUMPSB - ino %ld with prev %ld, next %ld",
					sbentry2->this_index,
					sbentry2->util_ll_prev,
					sbentry2->util_ll_next);
		}
		start_inode += num_entry_handle;
	}
	super_block_exclusive_release();
	fclose(dump_fptr);
}
/* End test code */
