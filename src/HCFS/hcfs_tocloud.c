/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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

#define _GNU_SOURCE
#include "hcfs_tocloud.h"

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/un.h>
#ifndef _ANDROID_ENV_
#include <attr/xattr.h>
#endif
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

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

static inline int _get_inode_sync_error(ino_t inode, BOOL *sync_error)
{
	int count1;

	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] == inode)
			break;
	}

	if (count1 < MAX_SYNC_CONCURRENCY) {
		*sync_error = sync_ctl.threads_error[count1];
		sem_post(&(sync_ctl.sync_op_sem));
		return 0;
	} else {
		sem_post(&(sync_ctl.sync_op_sem));
		return -1;
	}
}

static inline int _set_inode_sync_error(ino_t inode)
{
	int count1;

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

static inline int _set_inode_continue_nexttime(ino_t inode)
{
	int count1;

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

/**
 * _revert_block_status_LDISK
 *
 * When cancelling uploading this time, revert the status from ST_LtoC to
 * ST_LDISK if needed.
 *
 * @return 0 on success, otherwise negative errcode.
 */
int _revert_block_status_LDISK(ino_t this_inode, long long blockno,
		int e_index, long long page_filepos)
{
	BLOCK_ENTRY_PAGE tmppage;
	char local_metapath[300];
	FILE *fptr;
	int errcode, ret;
	ssize_t ret_ssize;

	fetch_meta_path(local_metapath, this_inode);

	fptr = fopen(local_metapath, "r+");
	if (fptr == NULL) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error: Fail to open meta in %s. Code %d\n",
				__func__, errcode);
			return -errcode;
		} else {
			write_log(8, "Meta is deleted in %s. Code %d\n",
				__func__, errcode);
			return 0;
		}
	}
	flock(fileno(fptr), LOCK_EX);
	setbuf(fptr, NULL);
	if (access(local_metapath, F_OK) < 0) {
		fclose(fptr);
		return 0;
	}

	PREAD(fileno(fptr), &tmppage, sizeof(BLOCK_ENTRY_PAGE), page_filepos);
	if (tmppage.block_entries[e_index].status == ST_LtoC) {
		tmppage.block_entries[e_index].status = ST_LDISK;
		write_log(8, "Debug: block_%"PRIu64"_%lld is reverted"
				" to ST_LDISK", (uint64_t)this_inode, blockno);
		PWRITE(fileno(fptr), &tmppage, sizeof(BLOCK_ENTRY_PAGE),
				page_filepos);
	}

	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return errcode;
}

/*
 * _del_toupload_blocks()
 *
 * When sync error, perhaps there are many copied blocks prepared to be
 * uploaded, so remove all of them because uploading this time is cancelled.
 *
 * */
static inline int _del_toupload_blocks(char *toupload_metapath, ino_t inode)
{
	FILE *fptr;
	long long num_blocks, bcount;
	char block_path[300];
	struct stat tmpstat;
	int ret, errcode;
	ssize_t ret_ssize;

	fptr = fopen(toupload_metapath, "r");
	if (fptr != NULL) {
		PREAD(fileno(fptr), &tmpstat, sizeof(struct stat), 0);

		if (!S_ISREG(tmpstat.st_mode)) { /* Return when not regfile */
			fclose(fptr);
			return 0;
		}

		num_blocks = ((tmpstat.st_size == 0) ? 0
				: (tmpstat.st_size - 1) / MAX_BLOCK_SIZE + 1);
		for (bcount = 0; bcount < num_blocks; bcount++) {
			fetch_toupload_block_path(block_path, inode, bcount, 0);
			if (access(block_path, F_OK) == 0)
				unlink(block_path);
		}
		fclose(fptr);
	}

	return 0;

errcode_handle:
	return errcode;
}

/* Don't need to collect return code for the per-inode sync thread, as
the error handling for syncing this inode will be handled in
sync_single_inode. */
static inline void _sync_terminate_thread(int index)
{
	int ret;
	int tag_ret;
	ino_t inode;
	char toupload_metapath[300], local_metapath[400];
	char finish_sync;

	if ((sync_ctl.threads_in_use[index] != 0) &&
	    ((sync_ctl.threads_finished[index] == TRUE) &&
	     (sync_ctl.threads_created[index] == TRUE))) {
		ret = pthread_join(sync_ctl.inode_sync_thread[index], NULL);
		if (ret == 0) {
			inode = sync_ctl.threads_in_use[index];
			finish_sync = sync_ctl.threads_error[index] == TRUE ?
					FALSE : TRUE;
			fetch_toupload_meta_path(toupload_metapath, inode);

			/* Tell memory cache that finish uploading if
			 * meta exist */
			fetch_meta_path(local_metapath, inode);
			if (access(local_metapath, F_OK) == 0) {
				tag_ret = tag_status_on_fuse(inode, FALSE, 0,
					sync_ctl.is_revert[index], finish_sync);
				if (tag_ret < 0) {
					write_log(0, "Fail to tag inode %lld "
						"as NOT_UPLOADING in %s\n",
						inode, __func__);
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
						inode);
				if (ret < 0)
					write_log(0, "Error: Fail to remove"
						"toupload block. Code %d\n",
						-ret);
			}

			/* If need to continue nexttime, then just close
			 * progress file. Otherwise delete it. */
			if (sync_ctl.continue_nexttime[index] == TRUE) {
				close(sync_ctl.progress_fd[index]);

			} else {
				del_progress_file(sync_ctl.progress_fd[index],
						inode);
				if (access(toupload_metapath, F_OK) == 0)
					unlink(toupload_metapath);
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
	int count;
	struct timespec time_to_sleep;

	UNUSED(ptr);
	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while ((hcfs_system->system_going_down == FALSE) ||
	       (sync_ctl.total_active_sync_threads > 0)) {
		sem_wait(&(sync_ctl.sync_op_sem));

		if (sync_ctl.total_active_sync_threads <= 0) {
			sem_post(&(sync_ctl.sync_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
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
static inline int _upload_terminate_thread(int index)
{
	int count2, count1;
	int which_curl;
	int ret, errcode;
	FILE *toupload_metafptr;
	char thismetapath[METAPATHLEN], toupload_metapath[200];
	char blockpath[400], toupload_blockpath[400];
#if (DEDUP_ENABLE)
	unsigned char blk_obj_id[OBJID_LENGTH];
#endif
	ino_t this_inode;
	off_t page_filepos;
	long long e_index;
	long long blockno;
	long long toupload_block_seq;
	BLOCK_ENTRY_PAGE temppage;
	char is_delete;
	BLOCK_ENTRY *tmp_entry;
	size_t tmp_size, ret_size;
	DELETE_THREAD_TYPE *tmp_del;
	char need_delete_object, is_toupload_meta_lock;
	BLOCK_UPLOADING_STATUS temp_block_uploading_status;
	int progress_fd;
	char toupload_exist, finish_uploading;
	SYSTEM_DATA_TYPE *statptr;
	off_t cache_block_size;

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
	is_delete = upload_ctl.upload_threads[index].is_delete;
	page_filepos = upload_ctl.upload_threads[index].page_filepos;
	e_index = upload_ctl.upload_threads[index].page_entry_index;
	blockno = upload_ctl.upload_threads[index].blockno;
	progress_fd = upload_ctl.upload_threads[index].progress_fd;
	toupload_block_seq = upload_ctl.upload_threads[index].seq;

	/* Terminate it directly when thread is used to delete
	 * old data on cloud */
	if (upload_ctl.upload_threads[index].backend_delete_type != FALSE) {
		char backend_exist, toupload_exist;

		/* TODO: Maybe we don't care about deleting backend
		 * blocks when re-connecting */
		if (upload_ctl.upload_threads[index].backend_delete_type ==
				BACKEND_BLOCKS) {
			backend_exist = FALSE;
			set_progress_info(progress_fd, blockno, NULL,
					&backend_exist, NULL, NULL, NULL);
		/* When deleting to-upload blocks, it is important to recover
		 * the block status to ST_LDISK */
		} else if (upload_ctl.upload_threads[index].backend_delete_type
				== TOUPLOAD_BLOCKS) {
			toupload_exist = FALSE;
			set_progress_info(progress_fd, blockno, &toupload_exist,
					NULL, NULL, NULL, NULL);
			ret = _revert_block_status_LDISK(this_inode, blockno,
					e_index, page_filepos);
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
		UNLINK(toupload_blockpath);

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

errcode_handle:
#if (DEDUP_ENABLE)
	if (is_toupload_meta_lock == TRUE) {
		flock(fileno(toupload_metafptr), LOCK_UN);
		fclose(toupload_metafptr);
	}
#endif
	return errcode;
}

void collect_finished_upload_threads(void *ptr)
{
	int count, ret, count1;
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

	pthread_create(&(sync_ctl.sync_handler_thread), NULL,
		       (void *)&collect_finished_sync_threads, NULL);
}

void init_upload_control(void)
{
	int count;
	/* int ret_val; */

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
	struct dirent tmp_entry, *tmpptr;
	int ret, errcode;

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
		tmpptr = NULL;
		ret = readdir_r(dirp, &tmp_entry, &tmpptr);
		/* Delete all existing temp FS stat */
		while ((ret == 0) && (tmpptr != NULL)) {
			if (strncmp(tmp_entry.d_name, "tmpFSstat", 9) == 0) {
				snprintf(fname, METAPATHLEN - 1, "%s/%s",
					 FS_stat_path, tmp_entry.d_name);
				unlink(fname);
			}
			ret = readdir_r(dirp, &tmp_entry, &tmpptr);
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
}

/**
 *
 * Following are some inline macros and functions used in sync_single_inode()
 *
 */

#define FSEEK_ADHOC_SYNC_LOOP(A, B, C, UNLOCK_ON_ERROR)\
	{\
		ret = fseek(A, B, C);\
		if (ret < 0) {\
			errcode = errno;\
			write_log(0, "IO error in %s. Code %d, %s\n",\
				__func__, errcode, strerror(errcode));\
			sync_error = TRUE;\
			if (UNLOCK_ON_ERROR == TRUE) {\
				flock(fileno(A), LOCK_UN);\
			}\
			break;\
		}\
	}

#define FREAD_ADHOC_SYNC_LOOP(A, B, C, D, UNLOCK_ON_ERROR)\
	{\
		ret = fread(A, B, C, D);\
		if (ret < 1) {\
			errcode = ferror(D);\
			write_log(0, "IO error in %s.\n", __func__);\
			if (errcode != 0)\
				write_log(0, "Code %d, %s\n", errcode,\
					strerror(errcode));\
			sync_error = TRUE;\
			if (UNLOCK_ON_ERROR == TRUE) {\
				flock(fileno(D), LOCK_UN);\
			}\
			break;\
		}\
	}

#define FWRITE_ADHOC_SYNC_LOOP(A, B, C, D, UNLOCK_ON_ERROR)\
	{\
		ret = fwrite(A, B, C, D);\
		if (ret < 1) {\
			errcode = ferror(D);\
			write_log(0, "IO error in %s.\n", __func__);\
			write_log(0, "Code %d, %s\n", errcode,\
				strerror(errcode));\
			sync_error = TRUE;\
			if (UNLOCK_ON_ERROR == TRUE) {\
				flock(fileno(D), LOCK_UN);\
			}\
			break;\
		}\
	}

static inline int _select_upload_thread(char is_block, char is_delete,
#if (DEDUP_ENABLE)
				char is_upload,
				unsigned char old_obj_id[],
#endif
				ino_t this_inode, long long block_count,
				long long seq, off_t page_pos,
				long long e_index, int progress_fd,
				char backend_delete_type)
{
	int which_curl, count;

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

static inline void _busy_wait_all_specified_upload_threads(ino_t inode)
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

/*
static int increment_upload_seq(FILE *fptr, long long *upload_seq)
{
	ssize_t ret_ssize;
	int errcode;

	ret_ssize = fgetxattr(fileno(fptr),
			"user.upload_seq", upload_seq, sizeof(long long));
	if (ret_ssize >= 0) {
		*upload_seq += 1;
		fsetxattr(fileno(fptr), "user.upload_seq",
			upload_seq, sizeof(long long), 0);
	} else {
		errcode = errno;
		*upload_seq = 1;
		if (errcode == ENOATTR) {
			fsetxattr(fileno(fptr),
				"user.upload_seq", upload_seq,
				sizeof(long long), 0);
		} else {
			write_log(0, "Error: Get xattr error in %s."
					" Code %d\n", __func__, errcode);
			*upload_seq = 0;
			return -errcode;
		}
	}
	*upload_seq -= 1;

	return 0;
}
*/


#if (DEDUP_ENABLE)
static inline int _choose_deleted_block(char delete_which_one,
	const BLOCK_UPLOADING_STATUS *block_info, unsigned char *block_objid)
{
	char finish_uploading;

	finish_uploading = block_info->finish_uploading;

	/* Delete those blocks just uploaded */
	if (delete_which_one == TOUPLOAD_BLOCKS) {
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
	if (delete_which_one == BACKEND_BLOCKS) {
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

	finish_uploading = block_info->finish_uploading;
	to_upload_seq = block_info->to_upload_seq;
	backend_seq = block_info->backend_seq;

	write_log(10, "Debug: inode %"PRIu64", toupload_seq = %lld, "
			"backend_seq = %lld\n", (uint64_t)inode,
			to_upload_seq, backend_seq);

	if (delete_which_one == TOUPLOAD_BLOCKS) {
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
	if (delete_which_one == BACKEND_BLOCKS) {
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

int delete_backend_blocks(int progress_fd, long long total_blocks, ino_t inode,
	char delete_which_one)
{
	BLOCK_UPLOADING_PAGE tmppage;
	BLOCK_UPLOADING_STATUS *block_info;
	long long block_count;
	long long block_seq;
	long long which_page, current_page;
	long long offset;
	unsigned char block_objid[OBJID_LENGTH];
	int ret, errcode;
	int which_curl;
	int e_index;
	BOOL sync_error;
	ssize_t ret_ssize;
	char local_metapath[300];
	FILE *local_metafptr;
	ssize_t ret_size;
	long long page_pos;
	FILE_META_TYPE filemeta;

	if (delete_which_one == TOUPLOAD_BLOCKS)
	write_log(4, "Debug: Delete those blocks uploaded just now for "
		"inode_%"PRIu64"\n", (uint64_t)inode);

	fetch_meta_path(local_metapath, inode);

	page_pos = 0;
	current_page = -1;
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
			 * to recover status to ST_LDISK */
			if (delete_which_one == TOUPLOAD_BLOCKS) {
				local_metafptr = fopen(local_metapath, "r");
				if (local_metafptr != NULL) {
					flock(fileno(local_metafptr), LOCK_EX);
					ret_size = pread(fileno(local_metafptr),
						&filemeta, sizeof(FILE_META_TYPE),
						sizeof(struct stat));
					if (ret_size == sizeof(FILE_META_TYPE))
						page_pos = seek_page2(&filemeta,
							local_metafptr,
							which_page, 0);
					flock(fileno(local_metafptr), LOCK_UN);
					fclose(local_metafptr);
				} else {
					page_pos = 0;
				}
			}

			current_page = which_page;
		}

		e_index = block_count % BLK_INCREMENTS;
		block_info = &(tmppage.status_entry[e_index]);

#if (DEDUP_ENABLE)
		ret = _choose_deleted_block(delete_which_one,
			block_info, block_objid, inode);
#else
		block_seq = 0;
		ret = _choose_deleted_block(delete_which_one,
			block_info, &block_seq, inode);
#endif
		if (ret < 0)
			continue;

		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
		which_curl = _select_upload_thread(TRUE, FALSE,
			TRUE, block_objid,
			inode, block_count, block_seq,
			page_pos, e_index, progress_fd, delete_which_one);
#else
		which_curl = _select_upload_thread(TRUE, FALSE,
			inode, block_count, block_seq,
			page_pos, e_index, progress_fd, delete_which_one);
#endif
		sem_post(&(upload_ctl.upload_op_sem));
		dispatch_delete_block(which_curl);
	}

	/* Wait for all deleting threads. TODO: error handling when
	 * fail to delete cloud data */
	_busy_wait_all_specified_upload_threads(inode);
	write_log(10, "Debug: Finish deleting unuseful blocks "
			"for inode %"PRIu64" on cloud\n", (uint64_t)inode);
	return 0;

errcode_handle:
	return errcode;
}

/**
 * _change_status_to_BOTH()
 *
 * After finishing syncing, change status of those blocks from ST_LtoC
 * to ST_BOTH. If block status is ST_LDISK or ST_TODELETE or ST_NONE,
 * then it means the block is changed after syncing, so do NOT change
 * the status of the block.
 *
 * @return 0 on success
 */ 
int _change_status_to_BOTH(ino_t inode, int progress_fd,
		FILE *local_metafptr, char *local_metapath)
{
	PROGRESS_META progress_meta;
	BLOCK_UPLOADING_STATUS block_info;
	FILE_META_TYPE tempfilemeta;
	BLOCK_ENTRY_PAGE tmp_page;
	long long block_count, total_blocks, which_page, current_page;
	long long page_pos;
	long long local_seq;
	char local_status;
	int e_index;
	int ret, errcode;
	ssize_t ret_ssize;
	char blockpath[300];
	SYSTEM_DATA_TYPE *statptr;
	off_t cache_block_size;

	PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
	total_blocks = progress_meta.total_toupload_blocks;

	current_page = -1;
	for (block_count = 0; block_count < total_blocks; block_count++) {
		ret = get_progress_info(progress_fd, block_count, &block_info);
		if (ret < 0) {
			if (ret == -ENOENT) {
				block_count += (BLK_INCREMENTS - 1);
				continue;
			} else {
				break;
			}
		}

		if (TOUPLOAD_BLOCK_EXIST(block_info.block_exist) == TRUE) {
			/* It did not upload anything. (CLOUD/CtoL/BOTH) */
			if (block_info.to_upload_seq == block_info.backend_seq)
				continue;
		} else {
			/* TO_DELETE/ NONE */
			continue;
		}

		/* Change status if local status is ST_LtoC */
		flock(fileno(local_metafptr), LOCK_EX);
		if (access(local_metapath, F_OK) < 0) {
			flock(fileno(local_metafptr), LOCK_UN);
			break;
		}

		e_index = block_count % BLK_INCREMENTS;
		which_page = block_count / BLK_INCREMENTS;
		if (which_page != current_page) {
			PREAD(fileno(local_metafptr), &tempfilemeta,
				sizeof(FILE_META_TYPE), sizeof(struct stat));
			page_pos = seek_page2(&tempfilemeta,
				local_metafptr, which_page, 0);
			if (page_pos <= 0) {
				flock(fileno(local_metafptr), LOCK_UN);
				continue;
			}
			current_page = which_page;
		}

		PREAD(fileno(local_metafptr), &tmp_page,
				sizeof(BLOCK_ENTRY_PAGE), page_pos);
		local_status = tmp_page.block_entries[e_index].status;
		local_seq = tmp_page.block_entries[e_index].seqnum;
		/* Change status if status is ST_LtoC */
		if (local_status == ST_LtoC &&
				local_seq == block_info.to_upload_seq) {
			tmp_page.block_entries[e_index].status = ST_BOTH;
			tmp_page.block_entries[e_index].uploaded = TRUE;

			ret = fetch_block_path(blockpath, inode, block_count);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			ret = set_block_dirty_status(blockpath, NULL, FALSE);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			/* Remember dirty_cache_size */
			cache_block_size = check_file_size(blockpath);
			sem_wait(&(hcfs_system->access_sem));
			statptr = &(hcfs_system->systemdata);
			statptr->dirty_cache_size -= cache_block_size;
			if (statptr->dirty_cache_size < 0)
				statptr->dirty_cache_size = 0;
			sem_post(&(hcfs_system->access_sem));

			PWRITE(fileno(local_metafptr), &tmp_page,
					sizeof(BLOCK_ENTRY_PAGE), page_pos);
		}

		flock(fileno(local_metafptr), LOCK_UN);
	}

	return 0;

errcode_handle:
	flock(fileno(local_metafptr), LOCK_UN);
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
 * @return none
 */
void sync_single_inode(SYNC_THREAD_TYPE *ptr)
{
	char toupload_metapath[400], toupload_bpath[400];
	char objname[500];
	char local_metapath[METAPATHLEN];
	ino_t this_inode;
	FILE *toupload_metafptr, *local_metafptr;
	char truncpath[METAPATHLEN];
	FILE *truncfptr;
	struct stat tempfilestat;
	FILE_META_TYPE tempfilemeta;
	SYMLINK_META_TYPE tempsymmeta;
	DIR_META_TYPE tempdirmeta;
	BLOCK_ENTRY_PAGE local_temppage, toupload_temppage;
	int which_curl;
	long long page_pos, e_index, which_page, current_page;
	long long total_blocks, total_backend_blocks;
	long long block_count;
	unsigned char local_block_status, toupload_block_status;
	int ret, errcode;
	off_t toupload_size;
	BLOCK_ENTRY *tmp_entry;
	long long temp_trunc_size;
	ssize_t ret_ssize;
	size_t ret_size;
	BOOL sync_error;
	int count1;
	long long upload_seq;
	ino_t root_inode;
	long long backend_size;
	long long size_diff;
	long long toupload_block_seq, local_block_seq;
	int progress_fd;
	BOOL first_upload, is_local_meta_deleted;
	BLOCK_UPLOADING_STATUS temp_uploading_status;
	char toupload_exist, finish_uploading;
	char is_revert;

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

	/* Open temp meta to be uploaded. TODO: Maybe copy meta here? */
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

	first_upload = FALSE;
	/* Upload block if mode is regular file */
	if (S_ISREG(ptr->this_mode)) {
		/* First download backend meta and init backend block info in
		 * upload progress file. If it is revert mode now, then
		 * just read progress meta. */
		ret = init_backend_file_info(ptr, &backend_size,
				&total_backend_blocks);
		if (ret < 0) {
			fclose(toupload_metafptr);
			fclose(local_metafptr);
			sync_ctl.threads_error[ptr->which_index] = TRUE;
			sync_ctl.threads_finished[ptr->which_index] = TRUE;
			return;
		}

		FSEEK(toupload_metafptr, 0, SEEK_SET);
		FREAD(&tempfilestat, sizeof(struct stat), 1,
			toupload_metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			toupload_metafptr);

		toupload_size = tempfilestat.st_size;
		root_inode = tempfilemeta.root_inode;

/* Check if need to sync past the current size */
/* If can use xattr, use it to store trunc_size. Otherwise
store in some other file */
#ifdef _ANDROID_ENV_
		ret = fetch_trunc_path(truncpath, this_inode);

		truncfptr = fopen(truncpath, "r+");
		if (truncfptr != NULL) {
			setbuf(truncfptr, NULL);
			flock(fileno(truncfptr), LOCK_EX);
			FREAD(&temp_trunc_size, sizeof(long long), 1,
			      truncfptr);

			if (toupload_size < temp_trunc_size) {
				toupload_size = temp_trunc_size;
				UNLINK(truncpath);
			}
			fclose(truncfptr);
		}
#else
		ret_ssize = fgetxattr(fileno(metafptr), "user.trunc_size",
				&temp_trunc_size, sizeof(long long));

		if ((ret_ssize >= 0) && (toupload_size < temp_trunc_size)) {
			toupload_size = temp_trunc_size;
		}
#endif

		/* Compute number of blocks */
		if (toupload_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((toupload_size - 1)
				/ MAX_BLOCK_SIZE) + 1;

		/* Begin to upload blocks */
		current_page = -1;
		is_local_meta_deleted = FALSE;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			if (hcfs_system->system_going_down == TRUE)
				break;

			/* Check if needs to continue uploading next time */
			if (sync_ctl.continue_nexttime[ptr->which_index] ==
					TRUE)
				break;

			if (is_revert == TRUE) {
				if (did_block_finish_uploading(progress_fd,
					block_count) == TRUE)
					continue;
			}

			e_index = block_count % BLK_INCREMENTS;
			which_page = block_count / BLK_INCREMENTS;

			if (current_page != which_page) {
				flock(fileno(toupload_metafptr), LOCK_EX);
				page_pos = seek_page2(&tempfilemeta,
					toupload_metafptr, which_page, 0);
				if (page_pos <= 0) {
					block_count += (BLK_INCREMENTS - 1);
					flock(fileno(toupload_metafptr),
								LOCK_UN);
					continue;
				}
				current_page = which_page;
				/* Do not need to read again in the same
				   page position because toupload_meta cannot
				   be modified by other processes. */
				FSEEK_ADHOC_SYNC_LOOP(toupload_metafptr,
					page_pos, SEEK_SET, FALSE);

				FREAD_ADHOC_SYNC_LOOP(&toupload_temppage,
					sizeof(BLOCK_ENTRY_PAGE),
					1, toupload_metafptr, FALSE);
				flock(fileno(toupload_metafptr), LOCK_UN);
			}
			tmp_entry = &(toupload_temppage.block_entries[e_index]);
			toupload_block_status = tmp_entry->status;
			toupload_block_seq = tmp_entry->seqnum;

			/* Lock local meta. Read local meta and update status.
			   This should be read again even in the same page pos
			   because someone may modify it. */
			if (is_local_meta_deleted == FALSE) {
				flock(fileno(local_metafptr), LOCK_EX);
				if (access(local_metapath, F_OK) < 0) {
					is_local_meta_deleted = TRUE;
					flock(fileno(local_metafptr), LOCK_UN);
					break;
				}
			}

			FSEEK_ADHOC_SYNC_LOOP(local_metafptr, page_pos,
					SEEK_SET, TRUE);
			FREAD_ADHOC_SYNC_LOOP(&local_temppage,
					sizeof(BLOCK_ENTRY_PAGE), 1,
					local_metafptr, TRUE);

			tmp_entry = &(local_temppage.block_entries[e_index]);
			local_block_status = tmp_entry->status;
			local_block_seq = tmp_entry->seqnum;

			/*** Case 1: Local is dirty. Update status & upload ***/
			if (toupload_block_status == ST_LDISK ||
					toupload_block_status == ST_LtoC) {
				/* Important: Update status if this block is
				not deleted or is NOT newer than to-upload
				version. */
				if ((local_block_status == ST_LDISK) &&
					(local_block_seq == toupload_block_seq)) {

					tmp_entry->status = ST_LtoC;
					/* Update local meta */
					FSEEK_ADHOC_SYNC_LOOP(local_metafptr,
						page_pos, SEEK_SET, TRUE);
					FWRITE_ADHOC_SYNC_LOOP(&local_temppage,
						sizeof(BLOCK_ENTRY_PAGE),
						1, local_metafptr, TRUE);

				} else if ((local_block_status == ST_LtoC) &&
					(local_block_seq == toupload_block_seq)) {
					/* Tmp do nothing */

				} else {
					/* In continue mode, directly
					 * return when to-upload meta does
					 * not match local meta */
					if (is_revert == TRUE) {
						write_log(4, "When continue"
							"uploading inode %"
							PRIu64", cancel to"
							" continue uploading"
							" because block %lld"
							" has local_seq %lld"
							" and toupload seq"
							" %lld\n", (uint64_t)
							this_inode,
							block_count,
							local_block_seq,
							toupload_block_seq);
						sync_error = TRUE;
						sync_ctl.threads_error[ptr->which_index]
							= TRUE;
						break;
					}
				}
				flock(fileno(local_metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
				which_curl = _select_upload_thread(TRUE, FALSE,
						TRUE,
						tmp_entry->obj_id,
						ptr->inode, block_count,
						toupload_block_seq, page_pos,
						e_index, progress_fd,
						FALSE);
#else
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
						toupload_block_seq, page_pos,
						e_index, progress_fd,
						FALSE);
#endif

				sem_post(&(upload_ctl.upload_op_sem));
				ret = dispatch_upload_block(which_curl);
				if (ret < 0) {
					sync_error = TRUE;
					sync_ctl.threads_error[ptr->which_index]
							= TRUE;
					break;
				}
				continue;

			/*** Case 2: Local block is deleted or none.
			 * Do nothing ***/
			} else if (toupload_block_status == ST_TODELETE ||
					toupload_block_status == ST_NONE) {
				write_log(10, "Debug: block_%lld is TO_DELETE\n", block_count);

				if (local_block_status == ST_TODELETE) {
					memset(tmp_entry, 0,
							sizeof(BLOCK_ENTRY));
					tmp_entry->status = ST_NONE;
					FSEEK_ADHOC_SYNC_LOOP(local_metafptr,
						page_pos, SEEK_SET, TRUE);
					FWRITE_ADHOC_SYNC_LOOP(&local_temppage,
						sizeof(BLOCK_ENTRY_PAGE),
						1, local_metafptr, TRUE);
				}
				flock(fileno(local_metafptr), LOCK_UN);

				finish_uploading = TRUE;
				toupload_exist = FALSE;
				set_progress_info(progress_fd, block_count,
					&toupload_exist, NULL, NULL, NULL,
					&finish_uploading);

				fetch_toupload_block_path(toupload_bpath,
					this_inode, block_count,
					toupload_block_seq);
				if (access(toupload_bpath, F_OK) == 0)
					unlink(toupload_bpath);

				continue;

			/*** Case 3: ST_BOTH, ST_CtoL, ST_CLOUD. Do nothing ***/
			} else {
				write_log(10, "Debug: block_%lld is %d\n",
						block_count, toupload_block_status);
				flock(fileno(local_metafptr), LOCK_UN);

				finish_uploading = TRUE;
				toupload_exist = TRUE;
#if (DEDUP_ENABLE)
				set_progress_info(progress_fd,
					block_count, &toupload_exist, NULL,
					tmp_entry->obj_id, NULL,
					&finish_uploading);
#else
				set_progress_info(progress_fd,
					block_count, &toupload_exist, NULL,
					&toupload_block_seq, NULL,
					&finish_uploading);
#endif
				fetch_toupload_block_path(toupload_bpath,
					this_inode, block_count,
					toupload_block_seq);
				if (access(toupload_bpath, F_OK) == 0)
					unlink(toupload_bpath);
			}
		}
		/* ---End of syncing blocks loop--- */

		/* Block sync should be done here. Check if all upload
		threads for this inode has returned before starting meta sync*/
		_busy_wait_all_specified_upload_threads(ptr->inode);

		/* If meta is deleted when uploading, delete backend blocks */
		if (is_local_meta_deleted == TRUE) {
			sync_ctl.continue_nexttime[ptr->which_index] = FALSE;
			delete_backend_blocks(progress_fd, total_blocks,
				ptr->inode, TOUPLOAD_BLOCKS);
			fclose(local_metafptr);
			fclose(toupload_metafptr);
			sync_ctl.threads_finished[ptr->which_index] = TRUE;
			return;
		}
	}

	if (sync_error == FALSE)
		sync_error = sync_ctl.threads_error[ptr->which_index];

	/* Abort sync to cloud if error occured or system is going down */
	if ((sync_error == TRUE) || (hcfs_system->system_going_down == TRUE)) {
		/* When system going down, re-upload it later */
		if (hcfs_system->system_going_down == TRUE) {
			sync_ctl.continue_nexttime[ptr->which_index] = TRUE;

		/* If it is just sync error, then delete backend */
		} else {
			/* Do not unlink to-upload meta if needs continue
			 * next time */
			if (sync_ctl.continue_nexttime[ptr->which_index] ==
					FALSE)
				delete_backend_blocks(progress_fd, total_blocks,
					ptr->inode, TOUPLOAD_BLOCKS);
		}

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
	which_curl = _select_upload_thread(FALSE, FALSE, FALSE, NULL,
		ptr->inode, 0, 0, 0, 0, progress_fd, FALSE);
#else
	which_curl = _select_upload_thread(FALSE, FALSE,
		ptr->inode, 0, 0, 0, 0, progress_fd, FALSE);
#endif

	sem_post(&(upload_ctl.upload_op_sem));

	flock(fileno(local_metafptr), LOCK_EX);
	/*Check if metafile still exists. If not, forget the meta upload*/
	if (!access(local_metapath, F_OK)) {
		FSEEK(local_metafptr, sizeof(struct stat), SEEK_SET);

		if (S_ISFILE(ptr->this_mode)) {
			FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			      local_metafptr);
			root_inode = tempfilemeta.root_inode;
			tempfilemeta.size_last_upload = tempfilestat.st_size;
			FSEEK(local_metafptr, sizeof(struct stat), SEEK_SET);
			FWRITE(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			       local_metafptr);
			size_diff = toupload_size - backend_size;
			first_upload = tempfilemeta.upload_seq <= 0 ?
				TRUE : FALSE;
		}
		if (S_ISDIR(ptr->this_mode)) {
			FREAD(&tempdirmeta, sizeof(DIR_META_TYPE),
				1, local_metafptr);
			root_inode = tempdirmeta.root_inode;
			size_diff = 0;
			first_upload = tempdirmeta.upload_seq <= 0 ?
				TRUE : FALSE;
		}
		if (S_ISLNK(ptr->this_mode)) {
			FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE),
				1, local_metafptr);

			root_inode = tempsymmeta.root_inode;
			size_diff = 0;
			first_upload = tempsymmeta.upload_seq <= 0 ?
				TRUE : FALSE;
		}

		flock(fileno(local_metafptr), LOCK_UN);
		fclose(toupload_metafptr);

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
		delete_backend_blocks(progress_fd, total_blocks,
			ptr->inode, TOUPLOAD_BLOCKS);

		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	if (sync_error == TRUE) {
		fclose(local_metafptr);
		write_log(0, "Sync inode %"PRIu64" to backend incomplete.\n",
				(uint64_t)ptr->inode);
		/* Delete to-upload blocks when it fails by anything but
		 * disconnection */
		if (S_ISREG(ptr->this_mode)) {
			if (sync_ctl.continue_nexttime[ptr->which_index] ==
					FALSE) {
				delete_backend_blocks(progress_fd, total_blocks,
					ptr->inode, TOUPLOAD_BLOCKS);
			}
		} else { /* Just re-upload for dir/slnk/fifo/socket */
			sync_ctl.continue_nexttime[ptr->which_index] = FALSE;
		}

		sync_ctl.threads_finished[ptr->which_index] = TRUE;
		return;
	}

	/* Delete old block data on backend and wait for those threads */
	if (S_ISREG(ptr->this_mode)) {
		_change_status_to_BOTH(ptr->inode, progress_fd,
				local_metafptr, local_metapath);
		delete_backend_blocks(progress_fd, total_backend_blocks,
				ptr->inode, BACKEND_BLOCKS);
		fclose(local_metafptr);

	} else {
		fclose(local_metafptr);
	}

	/* Upload successfully. Update FS stat in backend */
	if (first_upload == TRUE)
		update_backend_stat(root_inode, size_diff, 1);
	else
		if (size_diff != 0)
			update_backend_stat(root_inode, size_diff, 0);

	sync_ctl.threads_finished[ptr->which_index] = TRUE;
	return;

errcode_handle:
	flock(fileno(local_metafptr), LOCK_UN);
	fclose(local_metafptr);
	flock(fileno(toupload_metafptr), LOCK_UN);
	fclose(toupload_metafptr);
	delete_backend_blocks(progress_fd, total_blocks,
			ptr->inode, TOUPLOAD_BLOCKS);
	sync_ctl.threads_error[ptr->which_index] = TRUE;
	sync_ctl.threads_finished[ptr->which_index] = TRUE;
}

int do_block_sync(ino_t this_inode, long long block_no,
#if (DEDUP_ENABLE)
		CURL_HANDLE *curl_handle, char *filename, char uploaded,
		unsigned char id_in_meta[])
#else
		long long seq, CURL_HANDLE *curl_handle, char *filename)
#endif
{
	char objname[400];
	FILE *fptr;
	int ret_val, errcode, ret;
	int ddt_fd = -1;
	int result_idx = -1;
	DDT_BTREE_NODE result_node;
#if (DEDUP_ENABLE)
	char obj_id_str[OBJID_STRING_LENGTH];
	unsigned char old_obj_id[OBJID_LENGTH];
	unsigned char obj_id[OBJID_LENGTH];
	unsigned char start_bytes[BYTES_TO_CHECK];
	unsigned char end_bytes[BYTES_TO_CHECK];
	off_t obj_size;
	FILE *ddt_fptr;
	DDT_BTREE_NODE tree_root;
	DDT_BTREE_META ddt_meta;
#endif

	write_log(10, "Debug datasync: inode %" PRIu64 ", block %lld\n",
		  (uint64_t)this_inode, block_no);
	snprintf(curl_handle->id, sizeof(curl_handle->id),
		 "upload_blk_%" PRIu64 "_%lld", (uint64_t)this_inode, block_no);
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
	snprintf(objname, sizeof(objname), "data_%" PRIu64 "_%lld_%lld",
		 (uint64_t)this_inode, block_no, seq);
	/* Force to upload */
	ret = 1;
#endif

	if (ret == 0) {
		/* Find a same object in cloud
		 * Just increase the refcount of the origin block
		 */
#if (DEDUP_ENABLE)
		write_log(10,
			"Debug datasync: find same obj %s - Aborted to upload",
			objname);

		if (!memcmp(old_obj_id, id_in_meta, OBJID_LENGTH)) {
			write_log(10, "Debug datasync: old obj id the same as"
				"new obj id for block_%ld_%lld\n", this_inode,
				block_no);
			flock(ddt_fd, LOCK_UN);
			fclose(ddt_fptr);
			return ret;
		}

		increase_ddt_el_refcount(&result_node, result_idx, ddt_fd);
#endif

	} else {
		write_log(10, "Debug datasync: start to sync obj %s", objname);

		unsigned char *key = NULL;
		unsigned char *data = NULL;
		HCFS_encode_object_meta *object_meta = NULL;
		HTTP_meta *http_meta = NULL;
		unsigned char *object_key = NULL;

#if ENCRYPT_ENABLE
		key = get_key();
		object_meta = calloc(1, sizeof(HCFS_encode_object_meta));
		object_key = calloc(KEY_SIZE, sizeof(unsigned char));
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
			ret = -EIO;

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

int do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	int ret_val, errcode, ret;
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
	unsigned char *key = NULL;
	unsigned char *data = NULL;

#if ENCRYPT_ENABLE
	key = get_key();
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
		ret = -EIO;

	if (fptr != new_fptr)
		fclose(new_fptr);
	if (data != NULL)
		free(data);
	return ret;
}

/* TODO: use pthread_exit to pass error code here. */
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, errcode, which_index;
	BLOCK_UPLOADING_STATUS temp_block_uploading_status;
	int count1;
	char finish_uploading;
	char local_metapath[300];

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

	/* Unlink toupload block if we terminates uploading, but
	 * do NOT unlink toupload meta because it will be re-upload
	 * next time.*/
	if (thread_ptr->is_block == TRUE)
		unlink(thread_ptr->tempfilename);
	upload_ctl.threads_finished[which_index] = TRUE;
	return;
}

void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, count1, which_index;
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
	}

	if (ret < 0)
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

int schedule_sync_meta(char *toupload_metapath, int which_curl)
{

	strncpy(upload_ctl.upload_threads[which_curl].tempfilename,
		toupload_metapath, sizeof(((UPLOAD_THREAD_TYPE *)0)->tempfilename));
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
		       (void *)&con_object_sync,
		       (void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;

	return 0;
}

int dispatch_upload_block(int which_curl)
{
	char thisblockpath[400];
	char toupload_blockpath[400];
	char local_metapath[400];
	int read_size;
	int count, ret, errcode;
	size_t ret_size;
	FILE *fptr, *blockfptr;
	UPLOAD_THREAD_TYPE *upload_ptr;
	char bopen, topen;

	bopen = FALSE;
	topen = FALSE;

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

	ret = check_and_copy_file(thisblockpath, toupload_blockpath, TRUE);
	if (ret < 0) {
		/* -EEXIST means target had been copied when writing */
		/* If ret == -ENOENT, it means file is deleted.
		 * Just set sync error and cancel uploading */
		if (ret == -ENOENT) {
			fetch_meta_path(local_metapath, upload_ptr->inode);
			if (access(local_metapath, F_OK) < 0) {
				errcode = 0;
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

void dispatch_delete_block(int which_curl)
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
static inline int _sync_mark(ino_t this_inode, mode_t this_mode,
			     SYNC_THREAD_TYPE *sync_threads)
{
	int count, ret;
	int progress_fd;
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
			ret = tag_status_on_fuse(this_inode, TRUE,
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
					NULL, (void *)&revert_inode_uploading,
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

#ifdef _ANDROID_ENV_
void *upload_loop(void *ptr)
#else
void upload_loop(void)
#endif
{
	ino_t ino_sync, ino_check;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
	SUPER_BLOCK_ENTRY tempentry;
	int count, sleep_count;
	char in_sync;
	int ret_val, ret;
	char is_start_check;
	char sync_paused_status = FALSE;

#ifdef _ANDROID_ENV_
	UNUSED(ptr);
#endif
	init_upload_control();
	init_sync_control();
	/*	init_sync_stat_control(); */
	is_start_check = TRUE;

	write_log(2, "Start upload loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		if (is_start_check) {
			/* Backup FS db if needed at the beginning of a round
			of to-upload inode scanning */
			backup_FS_database();
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

		/* Get first dirty inode or next inode. Before getting dirty
		 * inode, it should get the queue lock and check whether
		 * system is going down. */
		sem_wait(&(sync_ctl.sync_queue_sem));
		if (hcfs_system->system_going_down == TRUE) {
			sem_post(&(sync_ctl.sync_queue_sem));
			break;
		}

		super_block_exclusive_locking();
		if (ino_check == 0) {
			ino_check = sys_super_block->head.first_dirty_inode;
			write_log(10, "Debug: first dirty inode is inode %lld\n"
				, ino_check);
		}
		ino_sync = 0;
		if (ino_check != 0) {
			ino_sync = ino_check;

			ret_val = read_super_block_entry(ino_sync, &tempentry);

			if ((ret_val < 0) || (tempentry.status != IS_DIRTY)) {
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
		write_log(10, "Inode to sync is %" PRIu64 "\n",
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
						tempentry.inode_stat.st_mode,
								sync_threads);
				if (ret_val < 0) { /* Sync next time */
					ret = super_block_update_transit(
						ino_sync, FALSE, TRUE);
					if (ret < 0) {
						ino_sync = 0;
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

/************************************************************************
*
* Function name: update_backend_stat
*        Inputs: ino_t root_inode, long long system_size_delta,
*                long long num_inodes_delta
*       Summary: Updates per-FS statistics stored in the backend.
*  Return value: 0 if successful, or negation of error code.
*
*************************************************************************/
int update_backend_stat(ino_t root_inode, long long system_size_delta,
			long long num_inodes_delta)
{
	int ret, errcode;
	char fname[METAPATHLEN], tmpname[METAPATHLEN];
	char objname[METAPATHLEN];
	FILE *fptr;
	long long system_size, num_inodes;
	char is_fopen, is_backedup;
	size_t ret_size;

	write_log(10, "Debug: entering update backend stat\n");
	write_log(10, "Debug: root %"PRIu64" change %lld bytes and %lld "
		"inodes on backend\n", (uint64_t)root_inode, system_size_delta,
		num_inodes_delta);

	is_fopen = FALSE;
	sem_wait(&(sync_stat_ctl.stat_op_sem));

	snprintf(fname, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64 "",
		 METAPATH, (uint64_t)root_inode);
	snprintf(tmpname, METAPATHLEN - 1, "%s/FS_sync/tmpFSstat%" PRIu64,
		 METAPATH, (uint64_t)root_inode);
	snprintf(objname, METAPATHLEN - 1, "FSstat%" PRIu64 "",
		 (uint64_t)root_inode);

	/* If updating backend statistics for the first time, delete local
	copy for this volume */
	/* Note: tmpname is used as a tag to indicate whether the update
	occurred for the first time. It is also a backup of the old cached
	statistics since the last system shutdown for this volume */

	is_backedup = FALSE;
	if (access(tmpname, F_OK) != 0) {
		errcode = errno;
		if (errno == ENOENT) {
			if (access(fname, F_OK) == 0) {
				rename(fname, tmpname);
				is_backedup = TRUE;
			} else {
				MKNOD(tmpname, S_IFREG | 0700, 0);
			}
		}
	}

	write_log(10, "Objname %s\n", objname);
	if (access(fname, F_OK) == -1) {
		/* Download the object first if any */
		write_log(10, "Checking for FS stat in backend\n");
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
		ret = hcfs_get_object(fptr, objname, &(sync_stat_ctl.statcurl),
				      NULL);
		if ((ret >= 200) && (ret <= 299)) {
			ret = 0;
			errcode = 0;
		} else if (ret != 404) {
			errcode = -EIO;
			/* If cannot download the previous backed-up copy,
			revert to the cached one */
			if (is_backedup == TRUE)
				rename(tmpname, fname);
			goto errcode_handle;
		} else {
			/* Not found, init a new one */
			write_log(10, "Debug update stat: nothing stored\n");
			FTRUNCATE(fileno(fptr), 0);
			FSEEK(fptr, 0, SEEK_SET);
			system_size = 0;
			num_inodes = 0;
			FWRITE(&system_size, sizeof(long long), 1, fptr);
			FWRITE(&num_inodes, sizeof(long long), 1, fptr);
		}
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
	setbuf(fptr, NULL);
	flock(fileno(fptr), LOCK_EX);
	is_fopen = TRUE;
	FREAD(&system_size, sizeof(long long), 1, fptr);
	FREAD(&num_inodes, sizeof(long long), 1, fptr);
	system_size += system_size_delta;
	if (system_size < 0)
		system_size = 0;
	num_inodes += num_inodes_delta;
	if (num_inodes < 0)
		num_inodes = 0;
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&system_size, sizeof(long long), 1, fptr);
	FWRITE(&num_inodes, sizeof(long long), 1, fptr);

	/* TODO: Perhaps need to backup sum of backend data to backend as well
	*/
	/* Change statistics for summary statistics */
	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.backend_size += system_size_delta;
	if (hcfs_system->systemdata.backend_size < 0)
		hcfs_system->systemdata.backend_size = 0;
	hcfs_system->systemdata.backend_inodes += num_inodes_delta;
	if (hcfs_system->systemdata.backend_inodes < 0)
		hcfs_system->systemdata.backend_inodes = 0;
	sync_hcfs_system_data(FALSE);
	sem_post(&(hcfs_system->access_sem));

	FSEEK(fptr, 0, SEEK_SET);
	ret = hcfs_put_object(fptr, objname, &(sync_stat_ctl.statcurl), NULL);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		goto errcode_handle;
	}

	flock(fileno(fptr), LOCK_UN);
	sem_post(&(sync_stat_ctl.stat_op_sem));
	fclose(fptr);
	is_fopen = FALSE;

	return 0;

errcode_handle:
	if (is_fopen == TRUE) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	}
	sem_post(&(sync_stat_ctl.stat_op_sem));
	return errcode;
}
