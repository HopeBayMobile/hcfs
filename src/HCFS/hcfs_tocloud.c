/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_tocloud.c
* Abstract: The c source code file for syncing meta or data to
*           backend.
*
* Revision History
* 2015/2/13,16 Jiahong revised coding style.
* 2015/2/16 Jiahong added header for this file.
* 2015/5/14 Jiahong changed code so that process will terminate with fuse
*           unmount.
* 2015/6/4, 6/5 Jiahong added error handling.
* 2015/8/5, 8/6 Jiahong added routines for updating FS statistics
* 2015/2/18, Kewei finish atomic upload.
* 2016/5/23 Jiahong added control for cache mgmt
* 2016/6/7 Jiahong changing code for recovering mode
*
**************************************************************************/

/*
TODO: Will need to check mod time of meta file and not upload meta for
	every block status change.
TODO: Need to consider how to better handle meta deletion after sync, then
recreate (perhaps due to reusing inode.) Potential race conditions:
1. Create a file, sync, then delete right after meta is being uploaded.
(already a todo in sync_single_inode)
2. If the meta is being deleted, but the inode of the meta is reused and a new
meta is created and going to be synced.
)
TODO: Cleanup temp files in /dev/shm at system startup
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hcfs_tocloud.h"

#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/xattr.h>
#include <openssl/sha.h>
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
#include "tocloud_tools.h"
#include "utils.h"
#include "hcfs_cacheops.h"
#include "rebuild_super_block.h"
#include "do_restoration.h"
#include "recover_super_block.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

/**
 * When remove a temp file used to uplaod to backend, the file size
 * should be considered in system size and cache size after unlink it.
 *
 * @param filename File path to be remove.
 *
 * return 0 on success. Otherwise negative error code.
 */
int32_t unlink_upload_file(char *filename)
{
	struct stat filestat; /* raw file ops */
	int64_t filesize;
	int32_t ret, errcode;
	int64_t old_cachesize, new_cachesize, threshold;

	ret = stat(filename, &filestat);
	if (ret == 0) {
		filesize = filestat.st_blocks * 512;
		UNLINK(filename);
		old_cachesize = hcfs_system->systemdata.cache_size;
		change_system_meta(0, 0, -filesize,
				0, 0, 0, FALSE);
		if (old_cachesize >= CACHE_SOFT_LIMIT) {
			/* Once temp block is removed, check if cache_size
			 * drop below hard_limit - delta. */
			new_cachesize = hcfs_system->systemdata.cache_size;
			threshold = CACHE_HARD_LIMIT - CACHE_DELTA;
			if (old_cachesize >= threshold &&
					new_cachesize <= threshold) {
				/* Some ops may sleep because these
				 * temp blocks occupied cache space. */
				notify_sleep_on_cache(0);
				write_log(10, "Debug: Notify sleeping threads"
					" in %s", __func__);
			}
		}
	} else {
		int32_t errcode;
		errcode = errno;
		write_log(0, "Fail to stat file in %s. Code %d\n",
				__func__, errcode);
		return -errcode;
	}
	return 0;

errcode_handle:
	return errcode;
}

static inline int32_t _set_inode_sync_error(ino_t inode)
{
	int32_t count1;

	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] == inode)
			break;
	}

	if (count1 < MAX_SYNC_CONCURRENCY) {
		sync_ctl.threads_error[count1] = TRUE;
		sem_post(&(sync_ctl.sync_op_sem));
		return 0;
	} else {
		sem_post(&(sync_ctl.sync_op_sem));
		return -1;
	}
}

static inline int32_t _set_inode_continue_nexttime(ino_t inode)
{
	int32_t count1;

	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] == inode)
			break;
	}

	if (count1 < MAX_SYNC_CONCURRENCY) {
		sync_ctl.continue_nexttime[count1] = TRUE;
		sem_post(&(sync_ctl.sync_op_sem));
		return 0;
	} else {
		sem_post(&(sync_ctl.sync_op_sem));
		return -1;
	}
}


/*
 * _del_toupload_blocks()
 *
 * When sync error, perhaps there are many copied blocks prepared to be
 * uploaded, so remove all of them because uploading this time is cancelled.
 *
 */
static inline int32_t _del_toupload_blocks(const char *toupload_metapath,
	ino_t inode, mode_t *this_mode)
{
	FILE *fptr;
	int64_t num_blocks, bcount;
	char block_path[300];
	HCFS_STAT tmpstat;
	int32_t ret, errcode;
	ssize_t ret_ssize;
	FILE_META_TYPE tmpmeta;
	int64_t current_page, which_page, page_pos;

	*this_mode = 0;
	fptr = fopen(toupload_metapath, "r");
	if (fptr == NULL) {
		SUPER_BLOCK_ENTRY sb_entry;

		/* Get inode mode */
		ret = super_block_read(inode, &sb_entry);
		if (ret < 0)
			return ret;
		*this_mode = sb_entry.inode_stat.mode;

	} else {
		flock(fileno(fptr), LOCK_EX);
		PREAD(fileno(fptr), &tmpstat, sizeof(HCFS_STAT), 0);
		*this_mode = tmpstat.mode;
		if (!S_ISREG(tmpstat.mode)) { /* Return when not regfile */
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
			return 0;
		}

		PREAD(fileno(fptr), &tmpmeta, sizeof(FILE_META_TYPE),
				sizeof(HCFS_STAT));

		num_blocks = BLOCKS_OF_SIZE(tmpstat.size, MAX_BLOCK_SIZE);

		current_page = -1;
		for (bcount = 0; bcount < num_blocks; bcount++) {
			which_page = bcount / MAX_BLOCK_ENTRIES_PER_PAGE;
			if (current_page != which_page) {
				page_pos = seek_page2(&tmpmeta, fptr,
						which_page, 0);
				current_page = which_page;
				if (page_pos <= 0) {
					bcount += (BLK_INCREMENTS - 1);
					continue;
				}
			}
			/* TODO: consider truncating situation */
			fetch_toupload_block_path(block_path, inode, bcount, 0);
			if (access(block_path, F_OK) == 0)
				unlink_upload_file(block_path);
		}
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	}

	return 0;

errcode_handle:
	return errcode;
}

/* Don't need to collect return code for the per-inode sync thread, as
the error handling for syncing this inode will be handled in
sync_single_inode. */
static inline void _sync_terminate_thread(int32_t index)
{
	int32_t ret;
	int32_t tag_ret;
	ino_t inode;
	char toupload_metapath[300], local_metapath[400];
	char finish_sync;
	mode_t this_mode;

	if ((sync_ctl.threads_in_use[index] != 0) &&
	    ((sync_ctl.threads_finished[index] == TRUE) &&
	     (sync_ctl.threads_created[index] == TRUE))) {
		ret = pthread_join(sync_ctl.inode_sync_thread[index], NULL);
		if (ret == 0) {
			inode = sync_ctl.threads_in_use[index];
			this_mode = 0;
			finish_sync = sync_ctl.threads_error[index] == TRUE ?
					FALSE : TRUE;
			fetch_toupload_meta_path(toupload_metapath, inode);

			/* Tell memory cache that finish uploading if
			 * meta exist */
			fetch_meta_path(local_metapath, inode);
			if (access(local_metapath, F_OK) == 0) {
				if (hcfs_system->system_going_down == FALSE) {
					tag_ret = comm2fuseproc(inode, FALSE, 0,
						sync_ctl.is_revert[index],
						finish_sync);
					if (tag_ret < 0) {
						write_log(0, "Fail to tag"
							" inode %lld as "
							"NOT_UPLOADING in %s\n",
							inode, __func__);
					}
				}
			} else {
				sync_ctl.threads_error[index] = TRUE;
				sync_ctl.continue_nexttime[index] = FALSE;
			}

			/* Dequeue from dirty list */
			if (finish_sync == TRUE)
				super_block_update_transit(inode, FALSE, FALSE);
			else
				super_block_update_transit(inode, FALSE, TRUE);

			/* When threads error occur (no matter whether it is
			 * caused by disconnection or not), delete all to-upload
			 * blocks. */
			if (sync_ctl.threads_error[index] == TRUE) {
				ret = _del_toupload_blocks(toupload_metapath,
						inode, &this_mode);
				if (ret < 0)
					write_log(0, "Error: Fail to remove"
						"toupload block. Code %d\n",
						-ret);
			}

			/* If need to continue nexttime, then just close
			 * progress file. Otherwise delete it. */
			if (sync_ctl.continue_nexttime[index] == TRUE) {
				if (this_mode && S_ISREG(this_mode)) {
					close(sync_ctl.progress_fd[index]);
				} else {
					if (this_mode == 0)
						write_log(4, "Warn: No file "
							"mode in %s", __func__);
					del_progress_file(
						sync_ctl.progress_fd[index],
						inode);
					unlink_upload_file(toupload_metapath);
				}

				/* Retry immediately */
				write_log(8, "Debug: Immediately retry to sync "
					"inode %"PRIu64, (uint64_t)inode);
				push_retry_inode(&(sync_ctl.retry_list), inode);

			} else { /* Successfully sync, remove progress file */
				del_progress_file(sync_ctl.progress_fd[index],
						inode);
				if (access(toupload_metapath, F_OK) == 0)
					unlink_upload_file(toupload_metapath);
			}

			sync_ctl.threads_in_use[index] = 0;
			sync_ctl.threads_created[index] = FALSE;
			sync_ctl.threads_finished[index] = FALSE;
			sync_ctl.threads_error[index] = FALSE;
			sync_ctl.is_revert[index] = FALSE;
			sync_ctl.total_active_sync_threads--;
			sem_post(&(sync_ctl.sync_queue_sem));
		}
	}
}

void collect_finished_sync_threads(void *ptr)
{
	int32_t count;
	struct timespec time_to_sleep;

	UNUSED(ptr);
	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while ((hcfs_system->system_going_down == FALSE) ||
	       (sync_ctl.total_active_sync_threads > 0)) {
		sem_wait(&(sync_ctl.sync_op_sem));

		if (sync_ctl.total_active_sync_threads <= 0) {
			/* No active upload threads */
			if (hcfs_system->xfer_upload_in_progress) {
				sem_wait(&(hcfs_system->access_sem));
				hcfs_system->xfer_upload_in_progress = FALSE;
				sem_post(&(hcfs_system->access_sem));
				write_log(10, "Set upload in progress to FALSE\n");
			}
			sem_post(&(sync_ctl.sync_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
		}

		/* Some upload threads are working */
		if (!hcfs_system->xfer_upload_in_progress) {
			sem_wait(&(hcfs_system->access_sem));
			hcfs_system->xfer_upload_in_progress = TRUE;
			sem_post(&(hcfs_system->access_sem));
			write_log(10, "Set upload in progress to TRUE\n");
		}

		for (count = 0; count < MAX_SYNC_CONCURRENCY; count++)
			_sync_terminate_thread(count);

		sem_post(&(sync_ctl.sync_op_sem));
		nanosleep(&time_to_sleep, NULL);
		continue;
	}
}

/* On error, need to alert thread that dispatch the block upload
using threads_error in sync control. */
static inline int32_t _upload_terminate_thread(int32_t index)
{
	int32_t count1;
	int32_t ret;
	char toupload_blockpath[400];
#if (DEDUP_ENABLE)
	int32_t errcode;
	uint8_t blk_obj_id[OBJID_LENGTH];
#endif
	ino_t this_inode;
	off_t page_filepos;
	int64_t blockno;
	int64_t toupload_block_seq;
	int32_t progress_fd;
	char toupload_exist, finish_uploading;

	if (upload_ctl.threads_in_use[index] == FALSE)
		return 0;

	if (upload_ctl.upload_threads[index].is_block != TRUE)
		return 0;

	if (upload_ctl.threads_created[index] != TRUE)
		return 0;

	if (upload_ctl.threads_finished[index] != TRUE)
		return 0;

	ret = pthread_join(upload_ctl.upload_threads_no[index], NULL);

	/* TODO: If thread join failed but not EBUSY, perhaps should try to
	terminate the thread and mark fail? */
	if (ret != 0) {
		if (ret != EBUSY) {
			/* Perhaps can't join. Mark the thread as not in use */
			write_log(0, "Error in upload thread. Code %d, %s\n",
				  ret, strerror(ret));
			return -ret;
		}
		/* Thread is busy. Wait some more */
		return ret;
	}

	this_inode = upload_ctl.upload_threads[index].inode;
	//is_delete = upload_ctl.upload_threads[index].is_delete;
	page_filepos = upload_ctl.upload_threads[index].page_filepos;
	//e_index = upload_ctl.upload_threads[index].page_entry_index;
	blockno = upload_ctl.upload_threads[index].blockno;
	progress_fd = upload_ctl.upload_threads[index].progress_fd;
	toupload_block_seq = upload_ctl.upload_threads[index].seq;

	/* Terminate it directly when thread is used to delete
	 * old data on cloud */
	if (upload_ctl.upload_threads[index].backend_delete_type != FALSE) {

		/* TODO: Maybe we don't care about deleting backend
		 * blocks when re-connecting */
		if (upload_ctl.upload_threads[index].backend_delete_type ==
				DEL_BACKEND_BLOCKS) {
			/*backend_exist = FALSE;
			set_progress_info(progress_fd, blockno, NULL,
					&backend_exist, NULL, NULL, NULL);*/
		} else if (upload_ctl.upload_threads[index].backend_delete_type
				== DEL_TOUPLOAD_BLOCKS) {
			/*toupload_exist = FALSE;
			set_progress_info(progress_fd, blockno, &toupload_exist,
					NULL, NULL, NULL, NULL);*/
		}

		/* Do NOT need to lock upload_op_sem. It is locked by caller. */
		upload_ctl.threads_in_use[index] = FALSE;
		upload_ctl.threads_created[index] = FALSE;
		upload_ctl.threads_finished[index] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_queue_sem));

		return 0;
	}

	/* Find the sync-inode correspond to the block-inode */
	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] ==
		    upload_ctl.upload_threads[index].inode)
			break;
	}
	/* Check whether the sync-inode-thread raise error or not. */
	if (count1 < MAX_SYNC_CONCURRENCY) {
		if (sync_ctl.threads_error[count1] == TRUE) {
			sem_post(&(sync_ctl.sync_op_sem));

			upload_ctl.threads_in_use[index] = FALSE;
			upload_ctl.threads_created[index] = FALSE;
			upload_ctl.threads_finished[index] = FALSE;
			upload_ctl.total_active_upload_threads--;

			sem_post(&(upload_ctl.upload_queue_sem));
			return 0; /* Error already marked */
		}
	}
	sem_post(&(sync_ctl.sync_op_sem));

#if (DEDUP_ENABLE)
	/* copy new object id to toupload meta */
	memcpy(blk_obj_id, upload_ctl.upload_threads[index].obj_id,
		OBJID_LENGTH);

	fetch_toupload_meta_path(toupload_metapath, this_inode);
	toupload_metafptr = fopen(toupload_metapath, "r+");
	if (toupload_metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}
	setbuf(toupload_metafptr, NULL);

	flock(fileno(toupload_metafptr), LOCK_EX);
	is_toupload_meta_lock = TRUE;

	/* Record object id on to-upload meta */
	FSEEK(toupload_metafptr, page_filepos, SEEK_SET);
	FREAD(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, toupload_metafptr);
	memcpy(temppage.block_entries[e_index].obj_id, blk_obj_id,
		OBJID_LENGTH);
	FSEEK(toupload_metafptr, page_filepos, SEEK_SET);
	FWRITE(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, toupload_metafptr);

	flock(fileno(toupload_metafptr), LOCK_UN);
	fclose(toupload_metafptr);
	is_toupload_meta_lock = FALSE;

	/* Set this block as FINISH */
	finish_uploading = TRUE;
	toupload_exist = TRUE;
	set_progress_info(progress_fd, blockno, &toupload_exist, NULL,
		blk_obj_id, NULL, &finish_uploading);

#else
	toupload_exist = TRUE;
	finish_uploading = TRUE;
	set_progress_info(progress_fd, blockno, &toupload_exist, NULL,
		&toupload_block_seq, NULL, &finish_uploading);
#endif
	fetch_toupload_block_path(toupload_blockpath, this_inode,
			blockno, toupload_block_seq);
	if (access(toupload_blockpath, F_OK) == 0)
		unlink_upload_file(toupload_blockpath);

	ret = change_block_status_to_BOTH(this_inode, blockno, page_filepos,
			toupload_block_seq);
	if (ret < 0) {
		if (ret != -ENOENT) {
			write_log(0, "Error: Fail to change status to BOTH\n");
			return ret;
		}
	}
/*
#if (DEDUP_ENABLE)
			 Store hash in block meta too */
/*			memcpy(tmp_entry->obj_id, blk_obj_id, OBJID_LENGTH);
#endif */

	/* Finally reclaim the uploaded-thread. */
	upload_ctl.threads_in_use[index] = FALSE;
	upload_ctl.threads_created[index] = FALSE;
	upload_ctl.threads_finished[index] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_queue_sem));

	return 0;

#if (DEDUP_ENABLE)
errcode_handle:
	if (is_toupload_meta_lock == TRUE) {
		flock(fileno(toupload_metafptr), LOCK_UN);
		fclose(toupload_metafptr);
	}
	return errcode;
#endif
}

void collect_finished_upload_threads(void *ptr)
{
	int32_t count, ret;
	struct timespec time_to_sleep;

	UNUSED(ptr);
	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while ((hcfs_system->system_going_down == FALSE) ||
	       (upload_ctl.total_active_upload_threads > 0)) {
		sem_wait(&(upload_ctl.upload_op_sem));

		if (upload_ctl.total_active_upload_threads <= 0) {
			sem_post(&(upload_ctl.upload_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
		}
		for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
			ret = _upload_terminate_thread(count);
			/* Return code could be due to thread joining,
			and the value will be greater than zero in this
			case */
			if (ret >= 0)
				continue;
			/* Record the error in sync_thread */
			_set_inode_sync_error(
				upload_ctl.upload_threads[count].inode);
			write_log(10, "Recording error in %s\n", __func__);

#if (DEDUP_ENABLE)
			/* Reset uploaded flag for upload thread */
			upload_ctl.upload_threads[count].is_upload = FALSE;
#endif
			upload_ctl.threads_in_use[count] = FALSE;
			upload_ctl.threads_created[count] = FALSE;
			upload_ctl.threads_finished[count] = FALSE;
			upload_ctl.total_active_upload_threads--;
			sem_post(&(upload_ctl.upload_queue_sem));
		}

		sem_post(&(upload_ctl.upload_op_sem));
		nanosleep(&time_to_sleep, NULL);
		continue;
	}
}

void init_sync_control(void)
{
	memset(&sync_ctl, 0, sizeof(SYNC_THREAD_CONTROL));
	sem_init(&(sync_ctl.sync_op_sem), 0, 1);
	sem_init(&(sync_ctl.sync_queue_sem), 0, MAX_SYNC_CONCURRENCY);
	memset(&(sync_ctl.threads_in_use), 0,
	       sizeof(ino_t) * MAX_SYNC_CONCURRENCY);
	memset(&(sync_ctl.threads_created), 0,
	       sizeof(char) * MAX_SYNC_CONCURRENCY);
	memset(&(sync_ctl.threads_finished), 0,
	       sizeof(char) * MAX_SYNC_CONCURRENCY);
	sync_ctl.total_active_sync_threads = 0;
	sync_ctl.retry_list.list_size = MAX_SYNC_CONCURRENCY;
	sync_ctl.retry_list.num_retry = 0;
	sync_ctl.retry_list.retry_inode = (ino_t *)
			calloc(MAX_SYNC_CONCURRENCY, sizeof(ino_t));

	pthread_create(&(sync_ctl.sync_handler_thread), NULL,
		       (void *)&collect_finished_sync_threads, NULL);
}

void init_upload_control(void)
{
	int32_t count;
	/* int32_t ret_val; */

	memset(&upload_ctl, 0, sizeof(UPLOAD_THREAD_CONTROL));
	memset(&upload_curl_handles, 0,
	       sizeof(CURL_HANDLE) * MAX_UPLOAD_CONCURRENCY);

	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
		snprintf(upload_curl_handles[count].id,
			 sizeof(((CURL_HANDLE *)0)->id), "upload_thread_%d",
			 count);
		upload_curl_handles[count].curl_backend = NONE;
		upload_curl_handles[count].curl = NULL;
		/* Do not actually init backend until needed */
		/*
				ret_val =
		   hcfs_init_backend(&(upload_curl_handles[count]));
		*/
	}

	sem_init(&(upload_ctl.upload_op_sem), 0, 1);
	sem_init(&(upload_ctl.upload_queue_sem), 0, MAX_UPLOAD_CONCURRENCY);
	memset(&(upload_ctl.threads_in_use), 0,
	       sizeof(char) * MAX_UPLOAD_CONCURRENCY);
	memset(&(upload_ctl.threads_created), 0,
	       sizeof(char) * MAX_UPLOAD_CONCURRENCY);
	memset(&(upload_ctl.threads_finished), 0,
	       sizeof(char) * MAX_UPLOAD_CONCURRENCY);
	upload_ctl.total_active_upload_threads = 0;

	pthread_create(&(upload_ctl.upload_handler_thread), NULL,
		       (void *)&collect_finished_upload_threads, NULL);
}

void init_sync_stat_control(void)
{
	char *FS_stat_path, *fname;
	DIR *dirp;
	struct dirent *de;
	int32_t ret, errcode;

	FS_stat_path = (char *)malloc(METAPATHLEN);
	fname = (char *)malloc(METAPATHLEN);

	snprintf(FS_stat_path, METAPATHLEN - 1, "%s/FS_sync", METAPATH);

	if (access(FS_stat_path, F_OK) == -1) {
		MKDIR(FS_stat_path, 0700);
	} else {
		dirp = opendir(FS_stat_path);
		if (dirp == NULL) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				  errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		/* Delete all existing temp FS stat */
		while ((de = readdir(dirp)) != NULL) {
			if (strncmp(de->d_name, "tmpFSstat", 9) == 0) {
				snprintf(fname, METAPATHLEN - 1, "%s/%s",
					 FS_stat_path, de->d_name);
				unlink(fname);
			}
		}
		closedir(dirp);
	}

	memset(&(sync_stat_ctl.statcurl), 0, sizeof(CURL_HANDLE));
	sem_init(&(sync_stat_ctl.stat_op_sem), 0, 1);
	snprintf(sync_stat_ctl.statcurl.id, sizeof(sync_stat_ctl.statcurl.id),
		 "sync_stat_ctl");
	sync_stat_ctl.statcurl.curl_backend = NONE;
	sync_stat_ctl.statcurl.curl = NULL;
	/* Do not init backend until actually needed */
	/* hcfs_init_backend(&(sync_stat_ctl.statcurl)); */

	free(FS_stat_path);
	free(fname);
	return;
errcode_handle:
	/* TODO: better error handling here if init failed */
	free(FS_stat_path);
	free(fname);
	UNUSED(errcode);
}

/**
 * select_upload_thread()
 *
 * Select an appropriate thread to upload/delete a block or meta.
 *
 * @return usable curl index
 */ 
int32_t select_upload_thread(BOOL is_block, BOOL is_delete,
#if (DEDUP_ENABLE)
				BOOL is_upload,
				uint8_t old_obj_id[],
#endif
				ino_t this_inode, int64_t block_count,
				int64_t seq, off_t page_pos,
				int64_t e_index, int32_t progress_fd,
				char backend_delete_type)
{
	int32_t which_curl, count;

	which_curl = -1;
	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
		if (upload_ctl.threads_in_use[count] == FALSE) {
			upload_ctl.threads_in_use[count] = TRUE;
			upload_ctl.threads_created[count] = FALSE;
			upload_ctl.threads_finished[count] = FALSE;
			upload_ctl.upload_threads[count].is_block = is_block;
			upload_ctl.upload_threads[count].is_delete = is_delete;
			upload_ctl.upload_threads[count].inode = this_inode;
			upload_ctl.upload_threads[count].blockno = block_count;
			upload_ctl.upload_threads[count].seq = seq;
			upload_ctl.upload_threads[count].progress_fd =
								progress_fd;
			upload_ctl.upload_threads[count].page_filepos =
			    page_pos;
			upload_ctl.upload_threads[count].page_entry_index =
			    e_index;
			upload_ctl.upload_threads[count].which_curl = count;
			upload_ctl.upload_threads[count].backend_delete_type =
							backend_delete_type;
			upload_ctl.upload_threads[count].which_index = count;
#if (DEDUP_ENABLE)
			upload_ctl.upload_threads[count].is_upload = is_upload;
			if (is_upload == TRUE) {
				memcpy(upload_ctl.upload_threads[count].obj_id,
				       old_obj_id, OBJID_LENGTH);
			}
#endif

			upload_ctl.total_active_upload_threads++;
			which_curl = count;
			break;
		}
	}
	return which_curl;
}

/**
 * Check block status from to-upload meta, and compare the status with
 * corresponding status in local meta. Finally decide if this block should
 * be uploaded and record in progress file
 *
 * @param toupload_metafptr File pointer of toupload meta.
 * @param local_metafptr File pointer of local meta.
 * @param local_metapath Local meta path.
 * @param block_count Block number to be checked and uploaded this time.
 * @param current_page Current page, which is corresponding to "toupload_temppage"
 * @param page_pos Page poisition of current page
 * @param toupload_temppage Cached block entry page of toupload meta
 * @param toupload_meta To-upload meta, which cannot be changed.
 * @param ptr Data of synced inode.
 *
 * @return 0 on success, -ENOENT when page of this block does not exist,
 *         -EACCES when local meta is gone, -ECANCELED when cancelling
 *         syncing this time.
 */
static int32_t _check_block_sync(FILE *toupload_metafptr, FILE *local_metafptr,
		char *local_metapath, int64_t block_count,
		int64_t *current_page, int64_t *page_pos,
		BLOCK_ENTRY_PAGE *toupload_temppage,
		FILE_META_TYPE *toupload_meta, SYNC_THREAD_TYPE *ptr)
{
	int64_t which_page;
	int64_t toupload_block_seq, local_block_seq;
	uint8_t local_block_status, toupload_block_status;
	BLOCK_ENTRY *tmp_entry;
	BLOCK_ENTRY_PAGE local_temppage;
	int32_t e_index;
	char toupload_bpath[400];
	size_t ret_size;
	int32_t ret, errcode;
	int32_t which_curl;
	BOOL llock, ulock;
	char finish_uploading, toupload_exist;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	llock = FALSE;
	ulock = FALSE;

	e_index = block_count % BLK_INCREMENTS;
	which_page = block_count / BLK_INCREMENTS;

	if (*current_page != which_page) {
		flock(fileno(toupload_metafptr), LOCK_EX);
		ulock = TRUE;
		*page_pos = seek_page2(toupload_meta, toupload_metafptr,
				which_page, 0);
		if (*page_pos <= 0) {
			flock(fileno(toupload_metafptr), LOCK_UN);
			return -ENOENT;
		}
		*current_page = which_page;
		/* Do not need to read again in the same
		   page position because toupload_meta cannot
		   be modified by other processes. */
		FSEEK(toupload_metafptr, *page_pos, SEEK_SET);
		FREAD(toupload_temppage, sizeof(BLOCK_ENTRY_PAGE),
				1, toupload_metafptr);
		flock(fileno(toupload_metafptr), LOCK_UN);
		ulock = FALSE;
	}
	tmp_entry = &(toupload_temppage->block_entries[e_index]);
	toupload_block_status = tmp_entry->status;
	toupload_block_seq = tmp_entry->seqnum;

	/* Lock local meta. Read local meta and update status.
	   This should be read again even in the same page pos
	   because someone may modify it. */
	flock(fileno(local_metafptr), LOCK_EX);
	llock = TRUE;
	if (access(local_metapath, F_OK) < 0) {
		flock(fileno(local_metafptr), LOCK_UN);
		return -EACCES;
	}

	FSEEK(local_metafptr, *page_pos, SEEK_SET);
	FREAD(&local_temppage, sizeof(BLOCK_ENTRY_PAGE), 1, local_metafptr);
	tmp_entry = &(local_temppage.block_entries[e_index]);
	local_block_status = tmp_entry->status;
	local_block_seq = tmp_entry->seqnum;

	/*** Case 1: Local is dirty. Update status & upload ***/
	switch(toupload_block_status) {
	case ST_LDISK:
	case ST_LtoC:
		/* Update status if this block is
		   not deleted or is NOT newer than to-upload
		   version. */
		if ((local_block_status == ST_LDISK) &&
				(local_block_seq == toupload_block_seq)) {

			tmp_entry->status = ST_LtoC;
			FSEEK(local_metafptr, *page_pos, SEEK_SET);
			FWRITE(&local_temppage, sizeof(BLOCK_ENTRY_PAGE),
					1, local_metafptr);

		} else if ((local_block_status == ST_LtoC) &&
				(local_block_seq == toupload_block_seq)) {
			/* Tmp do nothing */

		} else {
			/* In continue mode, directly
			 * return when to-upload meta does
			 * not match local meta */
			if (ptr->is_revert == TRUE) {
				write_log(4, "When continue uploading inode %"
					PRIu64", cancel to continue uploading"
					" because block %lld has local_seq %lld"
					" and toupload seq %lld\n", (uint64_t)
					ptr->inode, block_count,
					local_block_seq, toupload_block_seq);
				flock(fileno(local_metafptr), LOCK_UN);
				return -ECANCELED;
			}
		}

		flock(fileno(local_metafptr), LOCK_UN);
		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
		which_curl = select_upload_thread(TRUE, FALSE, TRUE,
				tmp_entry->obj_id, ptr->inode, block_count,
				toupload_block_seq, *page_pos, e_index,
				ptr->progress_fd, FALSE);
#else
		which_curl = select_upload_thread(TRUE, FALSE, ptr->inode,
				block_count, toupload_block_seq, *page_pos,
				e_index, ptr->progress_fd, FALSE);
#endif

		sem_post(&(upload_ctl.upload_op_sem));
		ret = dispatch_upload_block(which_curl);
		if (ret < 0) {
			write_log(4, "Error: Fail to dipatch and upload"
				" block_%"PRIu64"_%lld. Code %d",
				(uint64_t)ptr->inode, block_count, -ret);
			if (ret == -ENOENT)
				ret = -ECANCELED;
			sync_ctl.threads_error[ptr->which_index] = TRUE;
			return ret;
		}
		
		break;
	/*** Case 2: Local block is deleted or none. Do nothing ***/
	case ST_TODELETE:
	case ST_NONE:
		write_log(10, "Debug: block_%"PRIu64"_%lld is %s\n",
				(uint64_t)ptr->inode, block_count,
				toupload_block_status == ST_NONE ?
				"ST_NONE" : "ST_TODELETE");
		if (local_block_status == ST_TODELETE) {
			write_log(10, "Debug: change ST_TODELETE to ST_NONE\n");
			memset(tmp_entry, 0, sizeof(BLOCK_ENTRY));
			tmp_entry->status = ST_NONE;
			FSEEK(local_metafptr, *page_pos, SEEK_SET);
			FWRITE(&local_temppage, sizeof(BLOCK_ENTRY_PAGE),
					1, local_metafptr);
		}

		finish_uploading = TRUE;
		toupload_exist = FALSE;
		set_progress_info(ptr->progress_fd, block_count,
			&toupload_exist, NULL, NULL, NULL, &finish_uploading);
		flock(fileno(local_metafptr), LOCK_UN);

		fetch_toupload_block_path(toupload_bpath, ptr->inode,
				block_count, toupload_block_seq);
		if (access(toupload_bpath, F_OK) == 0)
			unlink_upload_file(toupload_bpath);

		break;
	/*** Case 3: ST_BOTH, ST_CtoL, ST_CLOUD. Do nothing ***/
	default:
		write_log(10, "Debug: Status of block_%"PRIu64"_%lld is %d\n",
				(uint64_t)ptr->inode,
				block_count,
				toupload_block_status);
		/* Check seq num and just print log when not match */
		get_progress_info(ptr->progress_fd, block_count,
				&block_uploading_status);
		if (CLOUD_BLOCK_EXIST(block_uploading_status.block_exist) &&
		    toupload_block_seq != block_uploading_status.backend_seq) {
			write_log(0, "Error: Seq of block_%"PRIu64"_%lld does "
				"not match. Local seq %lld, cloud seq %lld. But"
				" status is %d\n",
				(uint64_t)ptr->inode, block_count,
				toupload_block_seq,
				block_uploading_status.backend_seq,
				toupload_block_status);
		}

		/* Set progress file */
		finish_uploading = TRUE;
		toupload_exist = TRUE;
#if (DEDUP_ENABLE)
		set_progress_info(ptr->progress_fd, block_count,
			&toupload_exist, NULL, tmp_entry->obj_id, NULL,
			&finish_uploading);
#else
		set_progress_info(ptr->progress_fd, block_count,
			&toupload_exist, NULL, &toupload_block_seq, NULL,
			&finish_uploading);
#endif
		flock(fileno(local_metafptr), LOCK_UN);
		fetch_toupload_block_path(toupload_bpath, ptr->inode,
			block_count, toupload_block_seq);
		if (access(toupload_bpath, F_OK) == 0)
			unlink_upload_file(toupload_bpath);

		break;
	}

	return 0;

errcode_handle:
	write_log(0, "IO error in %s\n", __func__);
	if (llock)
		flock(fileno(local_metafptr), LOCK_UN);
	if (ulock)
		flock(fileno(toupload_metafptr), LOCK_UN);
	return errcode;
}

/**
 * Main function to upload all block and meta
 *
 * This function aims to upload all meta data and block data to cloud.
 * If it is a regfile, upload all blocks first and then upload meta when finish
 * uploading all blocks. Finally delete old blocks on backend.
 * If it is not a regfile, then just upload metadata to cloud.
 *
 * @param ptr Pointer of data of inode uploading this time.
 *
 * @return none
 */
void sync_single_inode(SYNC_THREAD_TYPE *ptr)
{
	char toupload_metapath[400];
	char local_metapath[METAPATHLEN];
	ino_t this_inode;
	FILE *toupload_metafptr, *local_metafptr;
	HCFS_STAT tempfilestat;
	FILE_META_TYPE tempfilemeta;
	SYMLINK_META_TYPE tempsymmeta;
	DIR_META_TYPE tempdirmeta;
	BLOCK_ENTRY_PAGE toupload_temppage;
	int32_t which_curl;
	int64_t page_pos, current_page;
	int64_t total_blocks = 0, total_backend_blocks;
	int64_t block_count;
	int32_t ret, errcode;
	off_t toupload_size;
	size_t ret_size;
	BOOL sync_error;
	ino_t root_inode = 0;
	int64_t backend_size;
	int64_t size_diff = 0;
	int32_t progress_fd;
	BOOL is_local_meta_deleted;
	BOOL is_revert;
	int64_t meta_size_diff = 0;
	int64_t upload_seq = 0;
	CLOUD_RELATED_DATA cloud_related_data;
	int64_t pos;
	int64_t temp_trunc_size;
#ifdef _ANDROID_ENV_
	char truncpath[METAPATHLEN];
	FILE *truncfptr;
#else
	ssize_t ret_ssize;
#endif
	uint8_t now_pin_status = NUM_PIN_TYPES + 1;
	uint8_t last_pin_status = NUM_PIN_TYPES + 1;
	int64_t pin_size_delta = 0, last_file_size = 0;
	int64_t size_diff_blk = 0, meta_size_diff_blk = 0;
	int64_t disk_pin_size_delta = 0;
	BOOL cleanup_meta = FALSE;

	progress_fd = ptr->progress_fd;
	this_inode = ptr->inode;
	is_revert = ptr->is_revert;
	sync_error = FALSE;

	ret = fetch_toupload_meta_path(toupload_metapath, this_inode);
	if (ret < 0) {
		sync_ctl.threads_error[ptr->which_index] = TRUE;
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	ret = fetch_meta_path(local_metapath, this_inode);
	write_log(10, "Sync inode %" PRIu64 ", mode %d\n", (uint64_t)ptr->inode,
		  ptr->this_mode);

	if (ret < 0) {
		sync_ctl.threads_error[ptr->which_index] = TRUE;
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	/* Open temp meta to be uploaded. */
	toupload_metafptr = fopen(toupload_metapath, "r");
	if (toupload_metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		sync_ctl.threads_error[ptr->which_index] = TRUE;
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	/* Open local meta */
	local_metafptr = fopen(local_metapath, "r+");
	if (local_metafptr == NULL) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				  errcode, strerror(errcode));
			sync_ctl.threads_error[ptr->which_index] = TRUE;
		}
		/* If meta file is gone, the inode is deleted and we don't need
		to sync this object anymore. */
		fclose(toupload_metafptr);
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}
	setbuf(local_metafptr, NULL);

	memset(&tempfilestat, 0, sizeof(HCFS_STAT));

	/* Upload block if mode is regular file */
	if (S_ISREG(ptr->this_mode)) {
		/* First download backend meta and init backend block info in
		 * upload progress file. If it is revert mode now, then
		 * just read progress meta. */
		pos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
				sizeof(FILE_STATS_TYPE);
		FSEEK(toupload_metafptr, pos, SEEK_SET);
		FREAD(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
				1, toupload_metafptr);
		ret = init_backend_file_info(ptr, &backend_size,
				&total_backend_blocks,
				cloud_related_data.upload_seq,
				&last_pin_status);
		if (ret < 0) {
			fclose(toupload_metafptr);
			fclose(local_metafptr);
			sync_ctl.threads_error[ptr->which_index] = TRUE;
			sync_ctl.threads_finished[ptr->which_index] = TRUE;
			return;
		}

		FSEEK(toupload_metafptr, 0, SEEK_SET);
		FREAD(&tempfilestat, sizeof(HCFS_STAT), 1,
			toupload_metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			toupload_metafptr);

		now_pin_status = tempfilemeta.local_pin;
		toupload_size = tempfilestat.size;
		root_inode = tempfilemeta.root_inode;

/* Check if need to sync past the current size */
/* If can use xattr, use it to store trunc_size. Otherwise
store in some other file */
#ifdef _ANDROID_ENV_
		ret = fetch_trunc_path(truncpath, this_inode);

		truncfptr = NULL;
		if (ret >= 0)
			truncfptr = fopen(truncpath, "r+");
		if (truncfptr != NULL) {
			setbuf(truncfptr, NULL);
			flock(fileno(truncfptr), LOCK_EX);
			FREAD(&temp_trunc_size, sizeof(int64_t), 1,
			      truncfptr);

			if (toupload_size < temp_trunc_size) {
				toupload_size = temp_trunc_size;
				UNLINK(truncpath);
			}
			fclose(truncfptr);
		}
#else
		/* TODO: fix error of missing metafptr*/
		ret_ssize = fgetxattr(fileno(metafptr), "user.trunc_size",
				&temp_trunc_size, sizeof(int64_t));
		if ((ret_ssize >= 0) && (toupload_size < temp_trunc_size)) {
			toupload_size = temp_trunc_size;
		}
#endif

		/* Compute number of blocks */
		total_blocks = BLOCKS_OF_SIZE(toupload_size, MAX_BLOCK_SIZE);

		/* Begin to upload blocks */
		page_pos = 0;
		current_page = -1;
		is_local_meta_deleted = FALSE;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			if (hcfs_system->system_going_down == TRUE)
				break;

			if (sync_ctl.threads_error[ptr->which_index] == TRUE)
				break;

			if (is_revert == TRUE) {
				if (block_finish_uploading(progress_fd,
					block_count) == TRUE)
					continue;
			}

			ret = _check_block_sync(toupload_metafptr,
					local_metafptr, local_metapath,
					block_count, &current_page, &page_pos,
					&toupload_temppage, &tempfilemeta,
					ptr);
			if (ret < 0) {
				if (ret == -ENOENT) {
					block_count += (BLK_INCREMENTS - 1);
					continue;
				} else if (ret == -EACCES) {
					is_local_meta_deleted = TRUE;
					break;
				} else if (ret == -ECANCELED) {
					sync_ctl.threads_error[ptr->which_index]
					       = TRUE;
					break;
				} else {
					sync_ctl.threads_error[ptr->which_index]
					       = TRUE;
					break;
				}
			}

		}
		/* ---End of syncing blocks loop--- */

		/* Block sync should be done here. Check if all upload
		threads for this inode has returned before starting meta sync*/
		busy_wait_all_specified_upload_threads(ptr->inode);

		/* If meta is deleted when uploading, delete backend blocks */
		if (is_local_meta_deleted == TRUE) {
			sync_ctl.continue_nexttime[ptr->which_index] = FALSE;
			delete_backend_blocks(progress_fd, total_blocks,
				ptr->inode, DEL_TOUPLOAD_BLOCKS);
			fclose(local_metafptr);
			fclose(toupload_metafptr);
			sync_ctl.threads_finished[ptr->which_index] = TRUE;
			return;
		}

		/* If sync error, then return and delete to-upload blocks
		 * if needed */
		sync_error = sync_ctl.threads_error[ptr->which_index];
		if (sync_error == TRUE) {
			if (sync_ctl.continue_nexttime[ptr->which_index] ==
					FALSE) {
				/* Restore cloud usage statistics */
				pos = sizeof(HCFS_STAT) +
					sizeof(FILE_META_TYPE) +
					sizeof(FILE_STATS_TYPE);
				FSEEK(toupload_metafptr, pos, SEEK_SET);
				FREAD(&cloud_related_data,
					sizeof(CLOUD_RELATED_DATA),
					1, toupload_metafptr);
				flock(fileno(local_metafptr), LOCK_EX);
				FSEEK(local_metafptr, pos, SEEK_SET);
				FWRITE(&cloud_related_data,
					sizeof(CLOUD_RELATED_DATA),
					1, local_metafptr);
				flock(fileno(local_metafptr), LOCK_UN);
				/* Delete pre-upload blocks */
				delete_backend_blocks(progress_fd, total_blocks,
					ptr->inode, DEL_TOUPLOAD_BLOCKS);

			}
			fclose(local_metafptr);
			fclose(toupload_metafptr);
			sync_ctl.threads_finished[ptr->which_index] = TRUE;
			return;
		}
	}

	/* Abort sync to cloud if system is going down */
	if (hcfs_system->system_going_down == TRUE) {
		/* When system going down, re-upload it later */
		sync_ctl.continue_nexttime[ptr->which_index] = TRUE;
		fclose(toupload_metafptr);
		fclose(local_metafptr);
		sync_ctl.threads_error[ptr->which_index] = TRUE;
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	/* Begin to upload meta */
	sem_wait(&(upload_ctl.upload_queue_sem));
	sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
	which_curl = select_upload_thread(FALSE, FALSE, FALSE, NULL,
		ptr->inode, 0, 0, 0, 0, progress_fd, FALSE);
#else
	which_curl = select_upload_thread(FALSE, FALSE,
		ptr->inode, 0, 0, 0, 0, progress_fd, FALSE);
#endif

	sem_post(&(upload_ctl.upload_op_sem));

	/*Check if metafile still exists. If not, forget the meta upload*/
	flock(fileno(local_metafptr), LOCK_EX);

	if (!access(local_metapath, F_OK)) {
		struct stat metastat; /* meta file ops */
		int64_t now_meta_size, now_meta_size_blk, tmpval;

		cleanup_meta = TRUE;

		/* TODO: Refactor following code */
		ret = fstat(fileno(toupload_metafptr), &metastat);
		if (ret < 0) {
			write_log(0, "Fail to fetch meta %"PRIu64" stat\n",
					(uint64_t)this_inode);
			errcode = ret;
			goto errcode_handle;
		}
		now_meta_size = metastat.st_size;
		now_meta_size_blk = metastat.st_blocks * 512;

		if (S_ISFILE(ptr->this_mode)) {
			FSEEK(toupload_metafptr, sizeof(HCFS_STAT), SEEK_SET);
			FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			      toupload_metafptr);

			pos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			      sizeof(FILE_STATS_TYPE);
			FSEEK(toupload_metafptr, pos, SEEK_SET);
			FREAD(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, toupload_metafptr);
			root_inode = tempfilemeta.root_inode;
			upload_seq = cloud_related_data.upload_seq;
			size_diff = (tempfilestat.size + now_meta_size) -
					cloud_related_data.size_last_upload;
			tmpval = cloud_related_data.meta_last_upload;
			meta_size_diff = now_meta_size - tmpval;
			last_file_size = cloud_related_data.size_last_upload -
					 tmpval;
			size_diff_blk = (round_size(tempfilestat.size) +
			                 now_meta_size_blk) -
					(round_size(last_file_size) +
					 round_size(tmpval));
			meta_size_diff_blk = now_meta_size_blk -
			                     round_size(tmpval);
			/* Update cloud related data to local meta */
			cloud_related_data.size_last_upload =
					tempfilestat.size + now_meta_size;
			cloud_related_data.meta_last_upload = now_meta_size;
			cloud_related_data.upload_seq++;
			FSEEK(local_metafptr, pos, SEEK_SET);
			FWRITE(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, local_metafptr);
		}
		if (S_ISDIR(ptr->this_mode)) {
			FSEEK(toupload_metafptr, sizeof(HCFS_STAT), SEEK_SET);
			FREAD(&tempdirmeta, sizeof(DIR_META_TYPE),
					1, toupload_metafptr);
			FREAD(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, toupload_metafptr);
			root_inode = tempdirmeta.root_inode;
			upload_seq = cloud_related_data.upload_seq;
			size_diff = now_meta_size -
					cloud_related_data.size_last_upload;
			meta_size_diff = now_meta_size -
					cloud_related_data.meta_last_upload;
			tmpval = cloud_related_data.size_last_upload;
			size_diff_blk = now_meta_size_blk -
					round_size(tmpval);
			tmpval = cloud_related_data.meta_last_upload;
			meta_size_diff_blk = now_meta_size_blk -
					round_size(tmpval);
			/* Update cloud related data to local meta */
			cloud_related_data.size_last_upload = now_meta_size;
			cloud_related_data.meta_last_upload = now_meta_size;
			cloud_related_data.upload_seq++;
			FSEEK(local_metafptr, sizeof(HCFS_STAT) +
					sizeof(DIR_META_TYPE), SEEK_SET);
			FWRITE(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, local_metafptr);
		}
		if (S_ISLNK(ptr->this_mode)) {
			FSEEK(toupload_metafptr, sizeof(HCFS_STAT), SEEK_SET);
			FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE), 1,
					toupload_metafptr);
			FREAD(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, toupload_metafptr);
			root_inode = tempsymmeta.root_inode;
			upload_seq = cloud_related_data.upload_seq;
			size_diff = now_meta_size -
					cloud_related_data.size_last_upload;
			meta_size_diff = now_meta_size -
					cloud_related_data.meta_last_upload;
			tmpval = cloud_related_data.size_last_upload;
			size_diff_blk = now_meta_size_blk -
					round_size(tmpval);
			tmpval = cloud_related_data.meta_last_upload;
			meta_size_diff_blk = now_meta_size_blk -
					round_size(tmpval);
			/* Update cloud related data to local meta */
			cloud_related_data.size_last_upload = now_meta_size;
			cloud_related_data.meta_last_upload = now_meta_size;
			cloud_related_data.upload_seq++;
			FSEEK(local_metafptr, sizeof(HCFS_STAT) +
					sizeof(SYMLINK_META_TYPE), SEEK_SET);
			FWRITE(&cloud_related_data, sizeof(CLOUD_RELATED_DATA),
					1, local_metafptr);
		}

		flock(fileno(local_metafptr), LOCK_UN);
		cleanup_meta = FALSE;

		schedule_sync_meta(toupload_metapath, which_curl);

		pthread_join(upload_ctl.upload_threads_no[which_curl], NULL);

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.threads_finished[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));

		if (sync_error == FALSE) {
			/* Check the error in sync_thread */
			write_log(10, "Checking for other error\n");
			sync_error = sync_ctl.threads_error[ptr->which_index];
		}
	} else { /* meta is removed */
		flock(fileno(local_metafptr), LOCK_UN);
		fclose(local_metafptr);
		fclose(toupload_metafptr);

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.threads_finished[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));

		/* Delete those uploaded blocks if local meta is removed */
		if (S_ISREG(ptr->this_mode))
			delete_backend_blocks(progress_fd, total_blocks,
					ptr->inode, DEL_TOUPLOAD_BLOCKS);

		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	if (sync_error == TRUE) {
		write_log(0, "Sync inode %"PRIu64" to backend incomplete.\n",
				(uint64_t)ptr->inode);
		/* Delete to-upload blocks when it fails by anything but
		 * disconnection */
		if (S_ISREG(ptr->this_mode)) {
			if (sync_ctl.continue_nexttime[ptr->which_index] ==
					FALSE) {
				/* Restore cloud usage statistics */
				pos = sizeof(HCFS_STAT) +
					sizeof(FILE_META_TYPE) +
					sizeof(FILE_STATS_TYPE);
				FSEEK(toupload_metafptr, pos, SEEK_SET);
				FREAD(&cloud_related_data,
					sizeof(CLOUD_RELATED_DATA),
					1, toupload_metafptr);
				flock(fileno(local_metafptr), LOCK_EX);
				FSEEK(local_metafptr, pos, SEEK_SET);
				FWRITE(&cloud_related_data,
					sizeof(CLOUD_RELATED_DATA),
					1, local_metafptr);
				flock(fileno(local_metafptr), LOCK_UN);

				/* Delete pre-upload blocks */
				delete_backend_blocks(progress_fd, total_blocks,
					ptr->inode, DEL_TOUPLOAD_BLOCKS);
			}
		}
		fclose(toupload_metafptr);
		fclose(local_metafptr);
		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	fclose(toupload_metafptr);
	fclose(local_metafptr);

	/* Upload successfully. Update FS stat in backend */
	/* First check if pin status changed since the last upload */
	pin_size_delta = 0;
	disk_pin_size_delta = 0;
	if (P_IS_PIN(now_pin_status)) {
		pin_size_delta = size_diff - meta_size_diff;
		disk_pin_size_delta = size_diff_blk - meta_size_diff_blk;
	}

	if (P_IS_VALID_PIN(last_pin_status) && (S_ISREG(ptr->this_mode))) {
		if (P_IS_UNPIN(last_pin_status) && P_IS_PIN(now_pin_status)) {
			/* Change from unpin to pin, need to add file size
			to pin size */
			pin_size_delta = tempfilestat.size;
			disk_pin_size_delta = round_size(tempfilestat.size);
		}
		if (P_IS_PIN(last_pin_status) && P_IS_UNPIN(now_pin_status)) {
			/* Change from pin to unpin, need to substract file size
			of the last upload from pin size */
			pin_size_delta = -last_file_size;
			disk_pin_size_delta = -round_size(last_file_size);
		}
	}

	if (upload_seq <= 0)
		update_backend_stat(root_inode, size_diff, meta_size_diff,
		                    1, pin_size_delta, disk_pin_size_delta,
		                    meta_size_diff_blk);
	else
		if ((size_diff != 0) || (meta_size_diff != 0) ||
		    (pin_size_delta != 0))
			update_backend_stat(root_inode, size_diff,
					meta_size_diff, 0, pin_size_delta,
					disk_pin_size_delta,
					meta_size_diff_blk);

	/* Delete old block data on backend and wait for those threads */
	if (S_ISREG(ptr->this_mode)) {
		delete_backend_blocks(progress_fd, total_backend_blocks,
				ptr->inode, DEL_BACKEND_BLOCKS);
	}
	sync_ctl.threads_finished[ptr->which_index] = TRUE;
	return;

errcode_handle:
	if (cleanup_meta == TRUE) {
		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.threads_finished[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));
	}

	flock(fileno(local_metafptr), LOCK_UN);
	fclose(local_metafptr);
	flock(fileno(toupload_metafptr), LOCK_UN);
	fclose(toupload_metafptr);
	delete_backend_blocks(progress_fd, total_blocks,
			ptr->inode, DEL_TOUPLOAD_BLOCKS);
	sync_ctl.threads_error[ptr->which_index] = TRUE;
	sync_ctl.threads_finished[ptr->which_index] = TRUE;
	UNUSED(errcode);
	return;
}

int32_t do_block_sync(ino_t this_inode, int64_t block_no,
#if (DEDUP_ENABLE)
		CURL_HANDLE *curl_handle, char *filename, char uploaded,
		uint8_t id_in_meta[])
#else
		int64_t seq, CURL_HANDLE *curl_handle, char *filename)
#endif
{
	char objname[400];
	FILE *fptr;
	int32_t ret_val, errcode, ret;
#if (DEDUP_ENABLE)
	DDT_BTREE_NODE result_node;
	int32_t ddt_fd = -1;
	int32_t result_idx = -1;
	char obj_id_str[OBJID_STRING_LENGTH];
	uint8_t old_obj_id[OBJID_LENGTH];
	uint8_t obj_id[OBJID_LENGTH];
	uint8_t start_bytes[BYTES_TO_CHECK];
	uint8_t end_bytes[BYTES_TO_CHECK];
	off_t obj_size;
	FILE *ddt_fptr;
	DDT_BTREE_NODE tree_root;
	DDT_BTREE_META ddt_meta;
#endif

	write_log(10, "Debug datasync: inode %" PRIu64 ", block %lld\n",
		  (uint64_t)this_inode, block_no);
	snprintf(curl_handle->id, sizeof(curl_handle->id),
		 "upload_blk_%" PRIu64 "_%" PRId64, (uint64_t)this_inode, block_no);
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -errcode;
	}

#if (DEDUP_ENABLE)
	/* Compute hash of block */
	get_obj_id(filename, obj_id, start_bytes, end_bytes, &obj_size);
	/* compute_hash(filename, hash_key); */

	/* Get dedup table meta */
	ddt_fptr = get_ddt_btree_meta(obj_id, &tree_root, &ddt_meta);
	if (ddt_fptr == NULL) {
		/* Can't access ddt btree file */
		fclose(fptr);
		return -EBADF;
	}
	ddt_fd = fileno(ddt_fptr);

	/* Copy new obj_id and reserve old one */
	memcpy(old_obj_id, id_in_meta, OBJID_LENGTH);
	memcpy(&(obj_id[SHA256_DIGEST_LENGTH]), start_bytes, BYTES_TO_CHECK);
	memcpy(&(obj_id[SHA256_DIGEST_LENGTH + BYTES_TO_CHECK]), end_bytes,
	       BYTES_TO_CHECK);
	memcpy(id_in_meta, obj_id, OBJID_LENGTH);

	/* Check if upload is needed */
	ret = search_ddt_btree(obj_id, &tree_root, ddt_fd, &result_node,
			       &result_idx);

	/* Get objname - Object named by hash key */
	obj_id_to_string(obj_id, obj_id_str);

	/* hash_to_string(hash_key, hash_key_str); */
	snprintf(objname, sizeof(objname), "data_%s", obj_id_str);

#else
	fetch_backend_block_objname(objname, this_inode, block_no, seq);
	//snprintf(objname, sizeof(objname), "data_%" PRIu64 "_%lld_%lld",
	//	 (uint64_t)this_inode, block_no, seq);
	/* Force to upload */
	ret = 1;
#endif

#if (DEDUP_ENABLE)
	if (ret == 0) {
		/* Find a same object in cloud
		 * Just increase the refcount of the origin block
		 */
		write_log(10,
			"Debug datasync: find same obj %s - Aborted to upload",
			objname);

		if (!memcmp(old_obj_id, id_in_meta, OBJID_LENGTH)) {
			write_log(10, "Debug datasync: old obj id the same as"
				"new obj id for block_%ld_%lld\n", this_inode,
				block_no);
			flock(ddt_fd, LOCK_UN);
			fclose(ddt_fptr);
			fclose(fptr);
			return ret;
		}

		increase_ddt_el_refcount(&result_node, result_idx, ddt_fd);

	} else
#endif
	{
		write_log(10, "Debug datasync: start to sync obj %s\n", objname);

		uint8_t *data = NULL;
		HCFS_encode_object_meta *object_meta = NULL;
		HTTP_meta *http_meta = NULL;
		uint8_t *object_key = NULL;

#if ENCRYPT_ENABLE
		uint8_t *key = NULL;
		key = get_key("this is hopebay testing");
		object_meta = calloc(1, sizeof(HCFS_encode_object_meta));
		object_key = calloc(KEY_SIZE, sizeof(uint8_t));
		get_decode_meta(object_meta, object_key, key, ENCRYPT_ENABLE,
				COMPRESS_ENABLE);
		http_meta = new_http_meta();
		write_log(10, "transform header start...\n");
		transform_objdata_to_header(http_meta, object_meta);
		write_log(10, "transform header end...\n");
		OPENSSL_free(key);
#endif

		FILE *new_fptr = transform_fd(fptr, object_key, &data,
					      ENCRYPT_ENABLE, COMPRESS_ENABLE);
		write_log(10, "start to put..\n");
		ret_val =
		    hcfs_put_object(new_fptr, objname, curl_handle, http_meta);

		fclose(new_fptr);
		if (object_key != NULL)
			OPENSSL_free(object_key);
		if (object_meta != NULL)
			free_object_meta(object_meta);
		if (http_meta != NULL)
			delete_http_meta(http_meta);
		if (fptr != new_fptr)
			fclose(fptr);
		if (data != NULL)
			free(data);

		/* Already retried in get object if necessary */
		if ((ret_val >= 200) && (ret_val <= 299))
			ret = 0;
		else
			ret = -ENOTCONN;

#if (DEDUP_ENABLE)
		/* Upload finished - Need to update dedup table */
		if (ret == 0) {
			insert_ddt_btree(obj_id, obj_size, &tree_root, ddt_fd,
					 &ddt_meta);
		}
	}

	flock(ddt_fd, LOCK_UN);
	fclose(ddt_fptr);
#else
	}
#endif

	return ret;
}

int32_t do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	int32_t ret_val, errcode, ret;
	FILE *fptr;

	snprintf(objname, sizeof(objname), "meta_%" PRIu64 "",
		 (uint64_t)this_inode);
	write_log(10, "Debug datasync: objname %s, inode %" PRIu64 "\n",
		  objname, (uint64_t)this_inode);
	snprintf(curl_handle->id, sizeof(curl_handle->id),
		 "upload_meta_%" PRIu64 "", (uint64_t)this_inode);
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -errcode;
	}
	uint8_t *key = NULL;
	uint8_t *data = NULL;

#if ENCRYPT_ENABLE
	key = get_key("this is hopebay testing");
#endif
	FILE *new_fptr = transform_fd(fptr, key, &data,
			ENCRYPT_ENABLE, COMPRESS_ENABLE);

	if (new_fptr == NULL) {
		if (data != NULL)
			free(data);
		return -EIO;
	}

	ret_val = hcfs_put_object(new_fptr, objname, curl_handle, NULL);

	fclose(fptr);
	/* Already retried in get object if necessary */
	if ((200 <= ret_val) && (ret_val <= 299))
		ret = 0;
	else
		ret = -ENOTCONN;

	if (fptr != new_fptr)
		fclose(new_fptr);
	if (data != NULL)
		free(data);
	return ret;
}

/* TODO: use pthread_exit to pass error code here. */
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int32_t which_curl, ret, errcode, which_index = 0;
	char local_metapath[300];
	struct stat filestat; /* raw file ops */
	int64_t filesize;

	filesize = 0;
	ret = stat(thread_ptr->tempfilename, &filestat);
	if (ret == 0) {
		filesize = filestat.st_blocks * 512;
	} else {
		errcode = errno;
		write_log(0, "Error: Fail to stat file in %s. Code %d\n",
				__func__, errcode);
		goto errcode_handle;
	}

	which_curl = thread_ptr->which_curl;
	which_index = thread_ptr->which_index;
	if (thread_ptr->is_block == TRUE) {
#if (DEDUP_ENABLE)
		/* Get old object id (object id on cloud) */
		ret = get_progress_info(thread_ptr->progress_fd,
			thread_ptr->blockno, &temp_block_uploading_status);
		if (ret < 0)
			goto errcode_handle;

		/* The obj id should be read from progress file. This is used
		 * to compare between old and new object id, and then decide
		 * whether needs to upload the object. thread_ptr->obj_id
		 * will be replaced with new object id after uploading */
		memcpy(thread_ptr->obj_id,
			temp_block_uploading_status.backend_objid,
			OBJID_LENGTH);
		ret = do_block_sync(thread_ptr->inode, thread_ptr->blockno,
				&(upload_curl_handles[which_curl]),
				thread_ptr->tempfilename,
				thread_ptr->is_upload, thread_ptr->obj_id);

#else
		ret = do_block_sync(thread_ptr->inode, thread_ptr->blockno,
				thread_ptr->seq,
				&(upload_curl_handles[which_curl]),
				thread_ptr->tempfilename);
#endif
	} else {
		ret = do_meta_sync(thread_ptr->inode,
				&(upload_curl_handles[which_curl]),
				thread_ptr->tempfilename);
	}

	if (ret < 0)
		goto errcode_handle;

	UNLINK(thread_ptr->tempfilename);
	change_system_meta(0, 0, -filesize, 0, 0, 0, FALSE);
	upload_ctl.threads_finished[which_index] = TRUE;
	return;

errcode_handle:
	write_log(10, "Recording error in %s\n", __func__);

	/* If upload caused by disconnection, then upload next time */
	fetch_meta_path(local_metapath, thread_ptr->inode);
	if (access(local_metapath, F_OK) == 0) {
		if (hcfs_system->sync_paused == TRUE || ret == -ENOTCONN)
			_set_inode_continue_nexttime(thread_ptr->inode);
	}
	_set_inode_sync_error(thread_ptr->inode);

	/* Unlink toupload block if we terminates uploading, but
	 * do NOT unlink toupload meta because it will be re-upload
	 * next time.*/
	if (thread_ptr->is_block == TRUE) {
		ret = unlink(thread_ptr->tempfilename);
		if (ret == 0)
			change_system_meta(0, 0, -filesize,
					0, 0, 0, FALSE);
	}
	upload_ctl.threads_finished[which_index] = TRUE;
	return;
}

void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int32_t which_curl, ret, which_index;
	char local_metapath[200];

	which_curl = thread_ptr->which_curl;
	which_index = thread_ptr->which_index;
	if (thread_ptr->is_block == TRUE) {
#if (DEDUP_ENABLE)
		ret = do_block_delete(thread_ptr->inode, thread_ptr->blockno,
					thread_ptr->seq,
					thread_ptr->obj_id,
					&(upload_curl_handles[which_curl]));
#else
		ret = do_block_delete(thread_ptr->inode, thread_ptr->blockno,
					thread_ptr->seq,
					&(upload_curl_handles[which_curl]));
#endif
	} else {
		ret = 0;
		write_log(4, "Warn: Try to delete meta in %s? Skip it.",
				__func__);
	}

	/* Do not care about object not found on cloud. */
	if (ret < 0)
		if (ret != -ENOENT)
			goto errcode_handle;

	upload_ctl.threads_finished[which_index] = TRUE;
	return;

errcode_handle:
	write_log(10, "Recording error in %s\n", __func__);
	_set_inode_sync_error(thread_ptr->inode);

	/* If upload caused by disconnection, then upload next time */
	fetch_meta_path(local_metapath, thread_ptr->inode);
	if (access(local_metapath, F_OK) == 0) {
		if (hcfs_system->sync_paused == TRUE)
			_set_inode_continue_nexttime(thread_ptr->inode);
	}

	upload_ctl.threads_finished[which_index] = TRUE;
	return;
}

int32_t schedule_sync_meta(char *toupload_metapath, int32_t which_curl)
{
	strncpy(upload_ctl.upload_threads[which_curl].tempfilename,
		toupload_metapath, sizeof(((UPLOAD_THREAD_TYPE *)0)->tempfilename));
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
		       (void *)&con_object_sync,
		       (void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;

	return 0;
}

int32_t dispatch_upload_block(int32_t which_curl)
{
	char thisblockpath[400];
	char toupload_blockpath[400];
	char local_metapath[400];
	int32_t ret, errcode;
	UPLOAD_THREAD_TYPE *upload_ptr;

	upload_ptr = &(upload_ctl.upload_threads[which_curl]);

	/* Open source block (origin block in blockpath) */
	ret = fetch_block_path(thisblockpath, upload_ptr->inode,
			       upload_ptr->blockno);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	ret = fetch_toupload_block_path(toupload_blockpath,
		upload_ptr->inode, upload_ptr->blockno, upload_ptr->seq);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	ret = check_and_copy_file(thisblockpath, toupload_blockpath,
			TRUE, FALSE);
	if (ret < 0) {
		/* -EEXIST means target had been copied when writing */
		/* If ret == -ENOENT, it means file is deleted. */
		if (ret == -ENOENT) {
			/* Return if meta is deleted */
			fetch_meta_path(local_metapath, upload_ptr->inode);
			if (access(local_metapath, F_OK) < 0) {
				errcode = 0;
				goto errcode_handle;
			}
			/* block is deleted but not be copied, return error */
			if (access(toupload_blockpath, F_OK) < 0) {
				errcode = errno;
				goto errcode_handle;
			}
		}
		if (ret != -EEXIST) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	strcpy(upload_ptr->tempfilename, toupload_blockpath);
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]),
		NULL, (void *)&con_object_sync,	(void *)upload_ptr);

	upload_ctl.threads_created[which_curl] = TRUE;
	return 0;

errcode_handle:
	sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
	upload_ctl.upload_threads[count].is_upload = FALSE;
#endif
	upload_ctl.threads_in_use[which_curl] = FALSE;
	upload_ctl.threads_created[which_curl] = FALSE;
	upload_ctl.threads_finished[which_curl] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_op_sem));
	sem_post(&(upload_ctl.upload_queue_sem));
	return errcode;
}

void dispatch_delete_block(int32_t which_curl)
{
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
		       (void *)&delete_object_sync,
		       (void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;
}

/**
 * Find a thread and let it start uploading inode
 *
 * This function is used to find an available thread and then nominate it
 * to upload data of "this_inode". sync_single_inode() is main function for
 * uploading a inode.
 *
 * @return thread index when succeeding in starting uploading.
 *         Otherwise return -1.
 */
static inline int32_t _sync_mark(ino_t this_inode, mode_t this_mode,
			     SYNC_THREAD_TYPE *sync_threads)
{
	int32_t count, ret;
	int32_t progress_fd;
	char progress_file_path[300];

	ret = 0;

	for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
		if (sync_ctl.threads_in_use[count] == 0) {
			/* Open progress file. If it exist, then revert
			uploading. Otherwise open a new progress file */
			fetch_progress_file_path(progress_file_path,
				this_inode);
			if (access(progress_file_path, F_OK) == 0) {
				progress_fd = open(progress_file_path, O_RDWR);
				if (progress_fd < 0) {
					ret = -errno;
					break;
				}
				sync_ctl.is_revert[count] = TRUE;
				sync_threads[count].is_revert = TRUE;
			} else {
				progress_fd = create_progress_file(this_inode);
				if (progress_fd < 0) {
					ret = -1;
					break;
				}
				sync_ctl.is_revert[count] = FALSE;
				sync_threads[count].is_revert = FALSE;
			}

			/* Notify fuse process that it is going to upload */
			ret = comm2fuseproc(this_inode, TRUE,
					progress_fd, sync_ctl.is_revert[count],
					FALSE);
			if (ret < 0) {
				write_log(2, "Fail to tagging inode %lld as "
					"UPLOADING.\n", this_inode);
				del_progress_file(progress_fd, this_inode);
				break;
			}
			/* Prepare data */
			sync_ctl.threads_in_use[count] = this_inode;
			sync_ctl.threads_created[count] = FALSE;
			sync_ctl.threads_finished[count] = FALSE;
			sync_ctl.threads_error[count] = FALSE;
			sync_ctl.continue_nexttime[count] = FALSE;
			sync_ctl.progress_fd[count] = progress_fd;
			sync_threads[count].inode = this_inode;
			sync_threads[count].this_mode = this_mode;
			sync_threads[count].progress_fd = progress_fd;
			sync_threads[count].which_index = count;

			write_log(10, "Before syncing: inode %" PRIu64
				      ", mode %d\n",
				  (uint64_t)sync_threads[count].inode,
				  sync_threads[count].this_mode);

			if (sync_ctl.is_revert[count] == TRUE)
				pthread_create(
					&(sync_ctl.inode_sync_thread[count]),
					NULL, (void *)&continue_inode_sync,
					(void *)&(sync_threads[count]));
			else
				pthread_create(
					&(sync_ctl.inode_sync_thread[count]),
					NULL, (void *)&sync_single_inode,
					(void *)&(sync_threads[count]));
			sync_ctl.threads_created[count] = TRUE;
			sync_ctl.total_active_sync_threads++;
			break;
		}
	}

	return ret;
}

static inline void _write_upload_loop_status_log(char *sync_paused_status)
{
	/* log about sleep & resume */
	if (hcfs_system->sync_paused != *sync_paused_status) {
		*sync_paused_status = hcfs_system->sync_paused;
		write_log(10, "Debug: upload_loop %s (sync %s)\n",
			  *sync_paused_status ? "sleep" : "resume",
			  *sync_paused_status ? "paused" : "start");
	}
}

/* Mark the inodes in dirty list from restoration as dirty if needed */
void _update_restore_dirty_list()
{
	int64_t count2, num_tosync;
	ino_t tosync_inode;
	char restore_tosync_list[METAPATHLEN];
	FILE *to_sync_fptr = NULL;
	struct stat tmpstat;
	int32_t ret, errcode;
	size_t ret_size;

	snprintf(restore_tosync_list, METAPATHLEN, "%s/tosync_list",
	         METAPATH);
	/* Skip the tosync list if does not exist or cannot access */
	if (access(restore_tosync_list, F_OK) < 0)
		return;
	ret = stat(restore_tosync_list, &tmpstat);
	if (ret < 0)
		return;
	if (tmpstat.st_size == 0) {
		unlink(restore_tosync_list);
		return;
	}
	num_tosync = (int64_t) (tmpstat.st_size / sizeof(ino_t));
	to_sync_fptr = fopen(restore_tosync_list, "r");
	if (to_sync_fptr == NULL)
		return;

	/* Mark as dirty inodes in the list */
	for (count2 = 0; count2 < num_tosync; count2++) {
		FREAD(&tosync_inode, sizeof(ino_t), 1,
		      to_sync_fptr);
		super_block_mark_dirty(tosync_inode);
		write_log(10, "Marked %" PRIu64 " as to sync\n",
		          (uint64_t) tosync_inode);
errcode_handle:
		/* If error occurs, just continue */
		continue;
	}
	fclose(to_sync_fptr);
	unlink(restore_tosync_list);
}
#ifdef _ANDROID_ENV_
void *upload_loop(void *ptr)
#else
void upload_loop(void)
#endif
{
	ino_t ino_sync, ino_check, retry_inode;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
	SUPER_BLOCK_ENTRY tempentry;
	int32_t count, sleep_count;
	char in_sync;
	int32_t ret_val, ret;
	BOOL is_start_check;
	char sync_paused_status = FALSE;
	char need_retry_backup;
	struct timeval last_retry_time, current_time;

#ifdef _ANDROID_ENV_
	UNUSED(ptr);
#endif
	init_upload_control();
	init_sync_control();
	/*	init_sync_stat_control(); */
	is_start_check = TRUE;

	write_log(2, "Start upload loop\n");

	/* If dirty list from restoration exists, need to mark them as dirty */
	_update_restore_dirty_list();

	need_retry_backup = FALSE;
	while (hcfs_system->system_going_down == FALSE) {
		if (is_start_check) {
			/* Backup FS db if needed at the beginning of a round
			of to-upload inode scanning */
			ret = backup_FS_database();
			if (ret < 0) {
				need_retry_backup = TRUE;
				gettimeofday(&last_retry_time, NULL);
			}

			/* Start to recovery dirty queue if needed */
			if (need_recover_sb())
				start_sb_recovery();

			for (sleep_count = 0; sleep_count < 10; sleep_count++) {
				/* Break if system going down */
				if (hcfs_system->system_going_down == TRUE)
					break;

				/* Avoid busy polling */
				if (sys_super_block->head.num_dirty <=
				    sync_ctl.total_active_sync_threads) {
					sleep(1);
					continue;
				}

				/*Sleep for a while if we are not really
				in a hurry*/
				if ((hcfs_system->systemdata.cache_size <
				    CACHE_SOFT_LIMIT) ||
				    (hcfs_system->systemdata.dirty_cache_size
				    <= 0))
					sleep(1);
				else
					break;
			}

			ino_check = 0;
		}
		/* Break immediately if system going down */
		if (hcfs_system->system_going_down == TRUE)
			break;

		is_start_check = FALSE;

		/* log about sleep & resume */
		_write_upload_loop_status_log(&sync_paused_status);

		/* sleep until backend is back */
		if (hcfs_system->sync_paused) {
			sleep(1);
			continue;
		}

		/* If FSmgr backup failed, retry if network
		connection is on with at least 5 seconds
		if between */
		if (need_retry_backup == TRUE) {
			gettimeofday(&current_time, NULL);
			if (current_time.tv_sec >
			    (last_retry_time.tv_sec + 5)) {
				ret = backup_FS_database();
				if (ret == 0)
					need_retry_backup = FALSE;
				else
					last_retry_time = current_time;
			}
		}

		/* Get first dirty inode or next inode. Before getting dirty
		 * inode, it should get the queue lock and check whether
		 * system is going down. */
		sem_wait(&(sync_ctl.sync_queue_sem));
		if (hcfs_system->system_going_down == TRUE) {
			sem_post(&(sync_ctl.sync_queue_sem));
			break;
		}

		/* Check if any inode should be retried right now. */
		sem_wait(&(sync_ctl.sync_op_sem));
		retry_inode = pull_retry_inode(&(sync_ctl.retry_list));
		sem_post(&(sync_ctl.sync_op_sem));

		super_block_exclusive_locking();
		if (retry_inode > 0) { /* Retried inode has higher priority */
			ino_check = retry_inode;
			write_log(6, "Info: Retry to sync inode %"PRIu64,
					(uint64_t)ino_check);
		} else {
			if (ino_check == 0) {
				ino_check =
					sys_super_block->head.first_dirty_inode;
				write_log(10, "Debug: first dirty inode"
					" is inode %"PRIu64,
					(uint64_t)ino_check);
			}
		}

		ino_sync = 0;
		if (ino_check != 0) {
			ino_sync = ino_check;

/* FEATURE TODO: double check that super block entry will be reconstructed here */
			ret_val = read_super_block_entry(ino_sync, &tempentry);
			if ((ret_val < 0) || (tempentry.status != IS_DIRTY)) {
				if (ret_val == 0)
					write_log(4, "Warn: Status of inode %"
						PRIu64" is %d",
						(uint64_t)ino_sync,
						tempentry.status);
				ino_sync = 0;
				ino_check = 0;
			} else {
				if (tempentry.in_transit == TRUE) {
					/* TODO: Revert in_transit inode after
					crashing. (Maybe in superblock?) */
					ino_check = tempentry.util_ll_next;
				} else {
					tempentry.in_transit = TRUE;
					tempentry.mod_after_in_transit = FALSE;
					ino_check = tempentry.util_ll_next;
					ret = write_super_block_entry(
					    ino_sync, &tempentry);
					if (ret < 0)
						ino_sync = 0;
				}
			}
		}
		super_block_exclusive_release();
		write_log(6, "Inode to sync is %" PRIu64 "\n",
			  (uint64_t)ino_sync);
		/* Begin to sync the inode */
		if (ino_sync != 0) {
			sem_wait(&(sync_ctl.sync_op_sem));
			/*First check if this inode is actually being
				synced now*/
			in_sync = FALSE;
			for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
				if (sync_ctl.threads_in_use[count] ==
				    ino_sync) {
					in_sync = TRUE;
					break;
				}
			}

			if (in_sync == FALSE) {
				ret_val = _sync_mark(ino_sync,
						tempentry.inode_stat.mode,
								sync_threads);
				if (ret_val < 0) { /* Sync next time */
					ret = super_block_update_transit(
						ino_sync, FALSE, TRUE);
					if (ret < 0) {
						ino_check = 0;
					}
					sem_post(&(sync_ctl.sync_queue_sem));
					sem_post(&(sync_ctl.sync_op_sem));
				} else {
					sem_post(&(sync_ctl.sync_op_sem));
				}
			} else {  /*If already syncing to cloud*/

				sem_post(&(sync_ctl.sync_op_sem));
				sem_post(&(sync_ctl.sync_queue_sem));
			}
		} else {
			sem_post(&(sync_ctl.sync_queue_sem));
		}
		if (ino_check == 0)
			is_start_check = TRUE;
	}

	pthread_join(upload_ctl.upload_handler_thread, NULL);
	pthread_join(sync_ctl.sync_handler_thread, NULL);

#ifdef _ANDROID_ENV_
	return NULL;
#endif
}

/* Helper function for backing up package list if needed */
void _try_backup_package_list(CURL_HANDLE *thiscurl)
{
	int32_t errcode, ret;
	char backup_xml[METAPATHLEN];
	FILE *fptr = NULL;

	sem_wait(&backup_pkg_sem);

	/* Return immediately if no new package list */
	if (have_new_pkgbackup == FALSE) {
		sem_post(&backup_pkg_sem);
		return;
	}
	have_new_pkgbackup = FALSE;
	sem_post(&backup_pkg_sem);

	write_log(4, "Backing up package list\n");
	snprintf(backup_xml, METAPATHLEN, "%s/backup_pkg", METAPATH);

	if (access(PACKAGE_XML, F_OK) != 0) {
		write_log(0, "Unable to locate package list\n");
		return;
	}

	ret = copy_file(PACKAGE_XML, backup_xml);
	if (ret < 0) {
		if (access(backup_xml, F_OK) == 0)
			unlink(backup_xml);
		return;
	}

	fptr = fopen(backup_xml, "r");
	if (fptr == NULL) {
		write_log(0, "Unable to open backed-up pkg list\n");
		return;
	}

	setbuf(fptr, NULL);

	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_SET);
	ret = hcfs_put_object(fptr, "backup_pkg", thiscurl, NULL);
	if ((ret < 200) || (ret > 299))
		goto errcode_handle;
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	fptr = NULL;

	return;
errcode_handle:
	if (fptr != NULL) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		fptr = NULL;
	}
}


/************************************************************************
*
* Function name: update_backend_stat
*        Inputs: ino_t root_inode, int64_t system_size_delta,
*                int64_t meta_size_delta, int64_t num_inodes_delta,
*                BOOL is_reg_pin
*       Summary: Updates per-FS statistics stored in the backend.
*  Return value: 0 if successful, or negation of error code.
*
*************************************************************************/
int32_t update_backend_stat(ino_t root_inode, int64_t system_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta,
		int64_t pin_size_delta, int64_t disk_pin_size_delta,
		int64_t disk_meta_size_delta)
{
	int32_t ret, errcode;
	char fname[METAPATHLEN];
	char objname[METAPATHLEN];
	FILE *fptr;
	BOOL is_fopen;
	size_t ret_size;
	FS_CLOUD_STAT_T fs_cloud_stat;

	write_log(10, "Debug: entering update backend stat\n");
	write_log(10, "Debug: root %"PRIu64" change %lld bytes and %lld "
		"inodes on backend\n", (uint64_t)root_inode, system_size_delta,
		num_inodes_delta);

	/* TODO: Perhaps need to backup sum of backend data to backend as well
	*/
	/* Change statistics for summary statistics */
	update_backend_usage(system_size_delta, meta_size_delta,
			num_inodes_delta);

	is_fopen = FALSE;
	sem_wait(&(sync_stat_ctl.stat_op_sem));

	snprintf(fname, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 METAPATH, (uint64_t)root_inode);
	snprintf(objname, METAPATHLEN - 1, "FSstat%" PRIu64 "",
		 (uint64_t)root_inode);

	write_log(10, "Objname %s\n", objname);
	if (access(fname, F_OK) == -1) {
		fptr = fopen(fname, "w");
		if (fptr == NULL) {
			errcode = errno;
			write_log(0, "Open error in %s. Code %d, %s\n",
				  __func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		setbuf(fptr, NULL);
		flock(fileno(fptr), LOCK_EX);
		is_fopen = TRUE;
		/* First try to download one */
		ret = hcfs_get_object(fptr, objname,
		                      &(sync_stat_ctl.statcurl), NULL);
		if ((ret < 200) || (ret > 299)) {
			if (ret != 404) {
				errcode = -EIO;
				goto errcode_handle;
			}
			/* Write a new one to disk if not found */
			write_log(4, "Writing a new cloud stat\n");
			memset(&fs_cloud_stat, 0, sizeof(FS_CLOUD_STAT_T));
			FTRUNCATE(fileno(fptr), 0);
			FSEEK(fptr, 0, SEEK_SET);
			FWRITE(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
		}
		/* TODO: How to validate cloud statistics if this
		happens */
		fsync(fileno(fptr));
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		is_fopen = FALSE;
			
	}

	fptr = fopen(fname, "r+");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	is_fopen = TRUE;
	setbuf(fptr, NULL);
	/* File lock is in update_fs_backend_usage() */
	ret = update_fs_backend_usage(fptr, system_size_delta, meta_size_delta,
			num_inodes_delta, pin_size_delta, disk_pin_size_delta,
			disk_meta_size_delta);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_SET);
	ret = hcfs_put_object(fptr, objname, &(sync_stat_ctl.statcurl), NULL);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		goto errcode_handle;
	}
	flock(fileno(fptr), LOCK_UN);

	/* Also backup package list if any */
	_try_backup_package_list(&(sync_stat_ctl.statcurl));

	sem_post(&(sync_stat_ctl.stat_op_sem));
	fclose(fptr);

	return 0;

errcode_handle:
	if (is_fopen == TRUE) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	}
	sem_post(&(sync_stat_ctl.stat_op_sem));
	return errcode;
}

void force_backup_package(void)
{
	/* Do not backup now if network is down or system restoring*/
	if (hcfs_system->sync_paused == TRUE ||
	    hcfs_system->system_restoring == RESTORING_STAGE1 ||
	    hcfs_system->system_restoring == RESTORING_STAGE2)
		return;

	sem_wait(&(sync_stat_ctl.stat_op_sem));
	_try_backup_package_list(&(sync_stat_ctl.statcurl));
	sem_post(&(sync_stat_ctl.stat_op_sem));
}
