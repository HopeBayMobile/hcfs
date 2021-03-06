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

/*Flow for delete loop is similar to that of upload loop:
First scan the to_be_deleted linked list in super inode and then check if
the meta is in the to_delete temp dir. Open it and and start to delete the
blocks first, then delete the meta last.*/

/*before actually moving the inode from to_be_deleted to deleted, must first
check the upload threads and sync threads to find out if there are any pending
uploads. It must wait until those are cleared. It must then wait for any
additional pending meta or block deletion for this inode to finish.*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hcfs_clouddelete.h"

#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#include <linux/xattr.h>
#include <inttypes.h>
#include <jansson.h>

#include "hcfs_tocloud.h"
#include "params.h"
#include "super_block.h"
#include "fuseop.h"
#include "global.h"
#include "logger.h"
#include "macro.h"
#include "dedup_table.h"
#include "metaops.h"
#include "utils.h"
#include "hcfs_fromcloud.h"
#include "atomic_tocloud.h"
#include "backend_generic.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

static DSYNC_THREAD_TYPE dsync_thread_info[MAX_DSYNC_CONCURRENCY];

CURL_HANDLE delete_curl_handles[MAX_DELETE_CONCURRENCY];

/* Helper function for terminating threads in deleting backend objects */
/* Dsync threads are the ones that find the objects to be deleted in
	a single filesystem object. */
/* Don't need to collect return code for the per-inode dsync thread, as
the error handling for sync-for-deletion for this inode will be handled in
dsync_single_inode. */
static inline void _dsync_terminate_thread(int32_t index)
{
	int32_t retry_inode;
	int32_t pause_status;

	if ((dsync_ctl.threads_in_use[index] != 0) &&
			((dsync_ctl.threads_finished[index] == TRUE) &&
			(dsync_ctl.threads_created[index] == TRUE))) {
		PTHREAD_REUSE_join(&(dsync_ctl.inode_dsync_thread[index]));

		if (dsync_ctl.retry_right_now[index] == TRUE)
			retry_inode = dsync_ctl.threads_in_use[index];
		else
			retry_inode = 0;

		dsync_ctl.threads_in_use[index] = 0;
		dsync_ctl.threads_created[index] = FALSE;
		dsync_ctl.threads_finished[index] = FALSE;
		dsync_ctl.retry_right_now[index] = FALSE;
		dsync_ctl.threads_error[index] = FALSE;
		dsync_ctl.total_active_dsync_threads--;
		sem_check_and_release(&(hcfs_system->dsync_wait_sem),
		                      &pause_status);
		sem_post(&(dsync_ctl.dsync_queue_sem));

		if (retry_inode > 0) {
			write_log(8, "Debug: Immediately retry to delete"
				" inode %"PRIu64, (uint64_t)retry_inode);
			push_retry_inode(&(dsync_ctl.retry_list), retry_inode);
		}
	}
}

/************************************************************************
*
* Function name: collect_finished_dsync_threads
*        Inputs: void *ptr
*       Summary: Collect finished dsync threads and terminate them.
*                Dsync threads are the ones that find the objects to
*                be deleted in a single filesystem object.
*  Return value: None
*
*************************************************************************/
void collect_finished_dsync_threads(void *ptr)
{
	int32_t count;

	UNUSED(ptr);

	while ((hcfs_system->system_going_down == FALSE) ||
		(dsync_ctl.total_active_dsync_threads > 0)) {
		sem_wait(&(dsync_ctl.pause_sem));
		if ((hcfs_system->system_going_down == TRUE) &&
		    (dsync_ctl.total_active_dsync_threads <= 0))
			break;
		sem_wait(&(dsync_ctl.dsync_op_sem));

		for (count = 0; count < MAX_DSYNC_CONCURRENCY; count++)
			_dsync_terminate_thread(count);

		sem_post(&(dsync_ctl.dsync_op_sem));
		continue;
	}
	for (count = 0; count < MAX_DSYNC_CONCURRENCY; count++)
		PTHREAD_REUSE_terminate(&(dsync_ctl.inode_dsync_thread[count]));
}

/* Helper function for terminating threads in deleting backend objects */
/* Delete threads are the ones that delete a single backend object. */
static inline void _delete_terminate_thread(int32_t index)
{
	int32_t dsync_index;

	if (((delete_ctl.threads_in_use[index] != 0) &&
		(delete_ctl.delete_threads[index].is_block == TRUE)) &&
		((delete_ctl.threads_finished[index] == TRUE) &&
			(delete_ctl.threads_created[index] == TRUE))) {
		PTHREAD_REUSE_join(&(delete_ctl.threads_no[index]));
		/* Mark error on dsync_ctl.threads_error when failed */
		if (delete_ctl.threads_error[index] == TRUE) {
			dsync_index =
				delete_ctl.delete_threads[index].dsync_index;
			dsync_ctl.threads_error[dsync_index] = TRUE;
		}
		delete_ctl.threads_in_use[index] = FALSE;
		delete_ctl.threads_created[index] = FALSE;
		delete_ctl.threads_finished[index] = FALSE;
		delete_ctl.threads_error[index] = FALSE;
		delete_ctl.total_active_delete_threads--;
		sem_post(&(delete_ctl.delete_queue_sem));
	}
}

/************************************************************************
*
* Function name: collect_finished_delete_threads
*        Inputs: void *ptr
*       Summary: Collect finished delete threads and terminate them.
*                Delete threads are the ones that delete a single
*                backend object.
*  Return value: None
*
*************************************************************************/
void collect_finished_delete_threads(void *ptr)
{
	int32_t count;

	UNUSED(ptr);

	while ((hcfs_system->system_going_down == FALSE) ||
		(delete_ctl.total_active_delete_threads > 0)) {
		sem_wait(&(delete_ctl.pause_sem));
		if ((hcfs_system->system_going_down == TRUE) &&
			(delete_ctl.total_active_delete_threads <= 0))
			break;

		sem_wait(&(delete_ctl.delete_op_sem));

		for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
			_delete_terminate_thread(count);

		sem_post(&(delete_ctl.delete_op_sem));
		continue;
	}
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
		PTHREAD_REUSE_terminate(&(delete_ctl.threads_no[count]));
}

/************************************************************************
*
* Function name: init_dsync_control
*        Inputs: None
*       Summary: Initialize control for dsync threads and create the threads.
*                Dsync threads are the ones that find the objects to
*                be deleted in a single filesystem object.
*  Return value: None
*
*************************************************************************/
void init_dsync_control(void)
{
	memset(&dsync_ctl, 0, sizeof(DSYNC_THREAD_CONTROL));
	sem_init(&(dsync_ctl.dsync_op_sem), 0, 1);
	sem_init(&(dsync_ctl.dsync_queue_sem), 0, MAX_DSYNC_CONCURRENCY);
	sem_init(&(dsync_ctl.pause_sem), 0, 0);
	memset(&(dsync_ctl.threads_in_use), 0,
					sizeof(ino_t) * MAX_DSYNC_CONCURRENCY);
	memset(&(dsync_ctl.threads_created), 0,
					sizeof(char) * MAX_DSYNC_CONCURRENCY);
	memset(&(dsync_ctl.threads_finished), 0,
					sizeof(char) * MAX_DSYNC_CONCURRENCY);
	dsync_ctl.total_active_dsync_threads = 0;
	dsync_ctl.retry_list.list_size = MAX_DSYNC_CONCURRENCY;
	dsync_ctl.retry_list.num_retry = 0;
	dsync_ctl.retry_list.retry_inode = (ino_t *)
			calloc(MAX_DSYNC_CONCURRENCY, sizeof(ino_t));

	pthread_create(&(dsync_ctl.dsync_handler_thread), NULL,
				(void *)&collect_finished_dsync_threads, NULL);
	/* Set reusable threads */
	PTHREAD_REUSE_set_exithandler();
	int32_t count;
	for (count = 0; count < MAX_DSYNC_CONCURRENCY; count++)
		PTHREAD_REUSE_create(&(dsync_ctl.inode_dsync_thread[count]),
		                     NULL);
}

static inline void _destroy_delete_controls(void)
{
	sem_post(&(delete_ctl.pause_sem));
	sem_post(&(dsync_ctl.pause_sem));
	pthread_join(dsync_ctl.dsync_handler_thread, NULL);
	pthread_join(delete_ctl.delete_handler_thread, NULL);
	free(dsync_ctl.retry_list.retry_inode);
}
/* Helper for initializing curl handles for deleting backend objects */
static inline int32_t _init_delete_handle(int32_t index)
{
	/* int32_t ret_val; */

	snprintf(delete_curl_handles[index].id, 255,
				"delete_thread_%d", index);
	delete_curl_handles[index].curl_backend = NONE;
	delete_curl_handles[index].curl = NULL;
	/* Do not actually init backend until needed */
/*
	ret_val = hcfs_init_backend(&(delete_curl_handles[index]));

	while ((ret_val < 200) || (ret_val > 299)) {
		write_log(0, "Error initializing delete threads. Retrying.\n");
		if (delete_curl_handles[index].curl != NULL)
			hcfs_destroy_backend(delete_curl_handles[index].curl);
		ret_val = hcfs_init_backend(&(delete_curl_handles[index]));
	}
	return ret_val;
*/
	return 0;
}

/************************************************************************
*
* Function name: init_delete_control
*        Inputs: None
*       Summary: Initialize control for delete threads and create the threads.
*                Delete threads are the ones that delete a single
*                backend object.
*  Return value: None
*
*************************************************************************/
void init_delete_control(void)
{
	int32_t count;

	memset(&delete_ctl, 0, sizeof(DELETE_THREAD_CONTROL));
	memset(&delete_curl_handles, 0,
			sizeof(CURL_HANDLE) * MAX_DELETE_CONCURRENCY);
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
		_init_delete_handle(count);

	sem_init(&(delete_ctl.delete_op_sem), 0, 1);
	sem_init(&(delete_ctl.delete_queue_sem), 0, MAX_DELETE_CONCURRENCY);
	sem_init(&(delete_ctl.pause_sem), 0, 0);
	memset(&(delete_ctl.threads_in_use), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	memset(&(delete_ctl.threads_created), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	memset(&(delete_ctl.threads_finished), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	delete_ctl.total_active_delete_threads = 0;

	pthread_create(&(delete_ctl.delete_handler_thread), NULL,
			(void *)&collect_finished_delete_threads, NULL);

	/* Set reusable threads */
	PTHREAD_REUSE_set_exithandler();
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
		PTHREAD_REUSE_create(&(delete_ctl.threads_no[count]),
		                     NULL);
}

/* Helper function for marking a delete thread as in use */
static inline int32_t _use_delete_thread(int32_t index, int32_t dsync_index,
				BOOL is_blk_flag,
#if ENABLE(DEDUP)
		ino_t this_inode, int64_t blockno, int64_t seq,
		uint8_t *obj_id)
#else
		ino_t this_inode, int64_t blockno, int64_t seq)
#endif
{
	if (delete_ctl.threads_in_use[index] != FALSE)
		return -1;

	delete_ctl.threads_in_use[index] = TRUE;
	delete_ctl.threads_created[index] = FALSE;
	delete_ctl.threads_finished[index] = FALSE;
	delete_ctl.delete_threads[index].is_block = is_blk_flag;
	delete_ctl.delete_threads[index].inode = this_inode;
	delete_ctl.delete_threads[index].seq = seq;
	if (is_blk_flag == TRUE) {
#if ENABLE(DEDUP)
		memcpy(delete_ctl.delete_threads[index].obj_id, obj_id, OBJID_LENGTH);
#endif
		delete_ctl.delete_threads[index].blockno = blockno;
	}
	delete_ctl.delete_threads[index].which_curl = index;
	delete_ctl.delete_threads[index].which_index = index;
	delete_ctl.delete_threads[index].dsync_index = dsync_index;

	delete_ctl.total_active_delete_threads++;

	return 0;
}

static inline void _check_del_progress_file(ino_t inode)
{
	BOOL now_sync;
	char toupload_metapath[300], backend_metapath[300];
	char progress_path[300];
	int32_t idx, ret[3] = {0};

	now_sync = FALSE;
	sem_wait(&sync_ctl.sync_op_sem);
	for (idx = 0; idx < MAX_SYNC_CONCURRENCY ; idx++) {
		if (sync_ctl.threads_in_use[idx] == inode) {
			now_sync = TRUE;
			break;
		}
	}
	sem_post(&sync_ctl.sync_op_sem);

	if (now_sync == FALSE) {
		/* TODO: Maybe delete blocks on cloud if progress file exist */
		fetch_toupload_meta_path(toupload_metapath, inode);
		fetch_backend_meta_path(backend_metapath, inode);
		fetch_progress_file_path(progress_path, inode);
		if (!access(toupload_metapath, F_OK))
			ret[0] = unlink_upload_file(toupload_metapath);
		if (!access(backend_metapath, F_OK))
			ret[1] = unlink(backend_metapath);
		if (!access(progress_path, F_OK))
			ret[2] = unlink(progress_path);

		if ((ret[0] | ret[1] | ret[2]))
			write_log(0, "Error: Fail to unlink in %s\n",
					__func__);
	}
}

static inline int32_t _read_backend_meta(char *backend_metapath,
					 mode_t this_mode,
					 ino_t *root_inode,
					 int64_t *backend_size_change,
					 int64_t *meta_size_change)
{
	FILE *metafptr;
	FILE_META_TYPE tempfilemeta;
	DIR_META_TYPE tempdirmeta;
	SYMLINK_META_TYPE tempsymmeta;
	HCFS_STAT temphcfsstat;
	int32_t ret;
	struct stat tempstat;
	int32_t errcode;

	ret = stat(backend_metapath, &tempstat);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}

	metafptr = fopen(backend_metapath, "r+");
	if (metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}
	setbuf(metafptr, NULL);

	FSEEK(metafptr, 0, SEEK_SET);
	if (S_ISFILE(this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		FREAD(&temphcfsstat, sizeof(HCFS_STAT), 1, metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1, metafptr);
		flock(fileno(metafptr), LOCK_UN);
		*root_inode = tempfilemeta.root_inode;
		*meta_size_change = tempstat.st_size;
		*backend_size_change = temphcfsstat.size + *meta_size_change;

	} else if (S_ISDIR(this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		FREAD(&temphcfsstat, sizeof(HCFS_STAT), 1, metafptr);
		FREAD(&tempdirmeta, sizeof(DIR_META_TYPE), 1, metafptr);
		flock(fileno(metafptr), LOCK_UN);
		*root_inode = tempdirmeta.root_inode;
		*meta_size_change = tempstat.st_size;
		*backend_size_change = temphcfsstat.size + *meta_size_change;

	} else if (S_ISLNK(this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		FREAD(&temphcfsstat, sizeof(HCFS_STAT), 1, metafptr);
		FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE), 1, metafptr);
		flock(fileno(metafptr), LOCK_UN);
		*root_inode = tempsymmeta.root_inode;
		*meta_size_change = tempstat.st_size;
		*backend_size_change = *meta_size_change;

	} else {
		write_log(0, "Error: Unknown type %d\n", this_mode);
		fclose(metafptr);
		return -EPERM;
	}

	fclose(metafptr);

	return 0;

errcode_handle:
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	return errcode;
}

/************************************************************************
*
* Function name: dsync_single_inode
*        Inputs: DSYNC_THREAD_TYPE *ptr
*       Summary: For the filesystem object marked in "ptr", scan the meta
*                file to check if we need to schedule backend items for
*                deletion.
*  Return value: None
*
*************************************************************************/
void dsync_single_inode(DSYNC_THREAD_TYPE *ptr)
{
	char todel_metapath[400];
	char backend_metapath[400];
	char objname[500];
	ino_t this_inode;
	FILE *backend_metafptr = NULL, *todel_fptr = NULL;
	HCFS_STAT tempfilestat;
	FILE_META_TYPE tempfilemeta;
	BLOCK_ENTRY_PAGE temppage;
	int32_t curl_id, which_dsync_index;
	int64_t current_index;
	int64_t page_pos, which_page, current_page;
	int64_t count, block_count;
	int64_t total_blocks;
	uint8_t block_status;
	char delete_done;
	int32_t ret_val, ret;
	struct timespec time_to_sleep;
	PTHREAD_REUSE_T *tmp_tn;
	DELETE_THREAD_TYPE *tmp_dt;
	off_t tmp_size;
	char backend_mlock;
	int64_t backend_size_change, meta_size_change;
	int64_t pin_size_delta;
	int64_t pin_size_delta_blk;
	int64_t block_seq;
	ino_t root_inode;
	char metaID[GDRIVE_ID_LENGTH + 1];
	int32_t pause_status;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	backend_mlock = FALSE;
	this_inode = ptr->inode;
	which_dsync_index = ptr->which_index;
	backend_size_change = 0;
	pin_size_delta = 0;
	pin_size_delta_blk = 0;

	ret = fetch_todelete_path(todel_metapath, this_inode);
	if (ret < 0) {
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	/* Since there are no more todelete meta files, should download
	 * backend meta for all d_type */
	fetch_del_backend_meta_path(backend_metapath, this_inode);
	backend_metafptr = fopen(backend_metapath, "w+");
	if (backend_metafptr == NULL) {
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_check_and_release(&(dsync_ctl.pause_sem), &pause_status);
		return;
	}
	setbuf(backend_metafptr, NULL);

	fetch_backend_meta_objname(objname, this_inode);
	if (CURRENT_BACKEND == GOOGLEDRIVE) {
		int64_t offset;
		CLOUD_RELATED_DATA cloud_data;

		todel_fptr = fopen(todel_metapath, "r");
		if (!todel_fptr) {
			if (errno == ENOENT) {
				super_block_delete(this_inode);
				super_block_reclaim();
			}
			write_log(0, "Fail to open %s in %s. Code %d",
				  todel_metapath, __func__, errno);
			fclose(backend_metafptr);
			unlink(backend_metapath);
			dsync_ctl.threads_finished[which_dsync_index] = TRUE;
			return;
		}
		if (S_ISFILE(ptr->this_mode)) {
			offset = sizeof(FILE_META_HEADER) -
				 sizeof(CLOUD_RELATED_DATA);
		} else if (S_ISDIR(ptr->this_mode)) {
			offset = sizeof(DIR_META_HEADER) -
				 sizeof(CLOUD_RELATED_DATA);
		} else if (S_ISLNK(ptr->this_mode)) {
			offset = sizeof(SYMLINK_META_HEADER) -
				 sizeof(CLOUD_RELATED_DATA);
		} else {
			write_log(0, "Type error %d in %s. Code %d",
				  ptr->this_mode, __func__, errno);
			fclose(backend_metafptr);
			unlink(backend_metapath);
			dsync_ctl.threads_finished[which_dsync_index] = TRUE;
			return;
		}
		FSEEK(todel_fptr, offset, SEEK_SET);
		FREAD(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, todel_fptr);
		fclose(todel_fptr);

		if (cloud_data.metaID[0]) {
			strncpy(metaID, cloud_data.metaID, GDRIVE_ID_LENGTH);
			ret = 0;
		} else {
			/* if value of ID is empty, fetch it using list
			 * operation. This case may occur after restoration
			 */
			GOOGLEDRIVE_OBJ_INFO obj_info;

			write_log(4,
				  "ID of Object %s not found, try to list it",
				  objname);
			ret = backend_ops.download_fill_object_info(
			    &obj_info, objname, NULL);
			if (ret == 0)
				strncpy(metaID, obj_info.fileID,
					GDRIVE_ID_LENGTH);
		}

		if (ret == 0)
			ret = fetch_from_cloud(
			    backend_metafptr, FETCH_FILE_META, objname, metaID);

	} else {
		ret = fetch_from_cloud(backend_metafptr, FETCH_FILE_META,
				       objname, NULL);
	}
	if (ret < 0) {
		if (ret == -EIO) {
			write_log(4, "Error: Fail to download "
					"backend meta_%"PRIu64". "
					"Delete next time.\n",
					(uint64_t)this_inode);

		} else if (ret == -ENOENT) {
			write_log(4, "Debug: Nothing on"
					" cloud to be deleted for"
					" inode_%"PRIu64"\n",
					(uint64_t)this_inode);
			_check_del_progress_file(this_inode);
			super_block_delete(this_inode);
			super_block_reclaim();
			/* todelete meta file may existed if system had been
			 * upgraded */
			unlink(todel_metapath);
		}

		fclose(backend_metafptr);
		unlink(backend_metapath);
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	ret =
	    _read_backend_meta(backend_metapath, ptr->this_mode, &root_inode,
			       &backend_size_change, &meta_size_change);
	if (ret < 0) {
		write_log(0, "Error: Meta %s cannot be read. Code %d\n",
				backend_metapath, -ret);
		_check_del_progress_file(this_inode);
		fclose(backend_metafptr);
		unlink(backend_metapath);
		/* todelete meta file may existed if system had been upgraded */
		unlink(todel_metapath);
		super_block_delete(this_inode);
		super_block_reclaim();
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	if (S_ISREG(ptr->this_mode)) {
		/* Delete blocks */
		flock(fileno(backend_metafptr), LOCK_EX);
		backend_mlock = TRUE;
		FSEEK(backend_metafptr, 0, SEEK_SET);
		FREAD(&tempfilestat, sizeof(HCFS_STAT), 1, backend_metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
							backend_metafptr);
		if (P_IS_PIN(tempfilemeta.local_pin)) {
			pin_size_delta = -backend_size_change +
			                  meta_size_change;
			pin_size_delta_blk = -round_size(backend_size_change) +
			                     round_size(meta_size_change);
		} else {
			pin_size_delta = 0;
			pin_size_delta_blk = 0;
		}
		tmp_size = tempfilestat.size;

		/* Check if need to sync past the current size */

		total_blocks = BLOCKS_OF_SIZE(tmp_size, MAX_BLOCK_SIZE);

		/* Delete all blocks */
		current_page = -1;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			if (hcfs_system->system_going_down == TRUE)
				break;

			current_index = block_count % BLK_INCREMENTS;
			which_page = block_count / BLK_INCREMENTS;

			if (current_page != which_page) {
				page_pos = seek_page2(&tempfilemeta,
					backend_metafptr, which_page, 0);

				if (page_pos <= 0) {
					block_count += BLK_INCREMENTS - 1;
					continue;
				}
				current_page = which_page;
				FSEEK(backend_metafptr, page_pos, SEEK_SET);
				FREAD(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
							backend_metafptr);
			}

			/*TODO: error handling here if cannot read correctly*/

			block_status =
				temppage.block_entries[current_index].status;
			block_seq =
				temppage.block_entries[current_index].seqnum;

			/* Delete backend object if uploaded */
			if ((block_status != ST_NONE) &&
					(block_status != ST_TODELETE)) {
				sem_wait(&(delete_ctl.delete_queue_sem));
				sem_wait(&(delete_ctl.delete_op_sem));
				curl_id = -1;
				for (count = 0; count < MAX_DELETE_CONCURRENCY;
								count++) {
#if ENABLE(DEDUP)
					ret_val = _use_delete_thread(count,
						which_dsync_index, TRUE,
						ptr->inode, block_count,
						block_seq,
						temppage.block_entries[current_index].obj_id);
#else
					ret_val = _use_delete_thread(count,
						which_dsync_index, TRUE,
						ptr->inode, block_count,
						block_seq);
#endif
					if (ret_val == 0) {
						curl_id = count;
						break;
					}
				}
				sem_post(&(delete_ctl.delete_op_sem));
				tmp_tn = &(delete_ctl.threads_no[curl_id]);
				tmp_dt = &(delete_ctl.delete_threads[curl_id]);
				if (CURRENT_BACKEND == GOOGLEDRIVE) {
					memset(&(tmp_dt->gdrive_info), 0,
					       sizeof(GOOGLEDRIVE_OBJ_INFO));
					strncpy(
					    tmp_dt->gdrive_info.fileID,
					    temppage
						.block_entries[current_index]
						.blockID,
					    GDRIVE_ID_LENGTH);
				}
				PTHREAD_REUSE_run(tmp_tn,
						(void *)&con_object_dsync,
							(void *)tmp_dt);
				delete_ctl.threads_created[curl_id] = TRUE;
			}

			/* Check error and delete next time */
			if (dsync_ctl.threads_error[which_dsync_index] == TRUE)
				break;
		}
		/* Block deletion should be done here. Check if all delete
		threads for this inode has returned before starting meta
		deletion*/

		delete_done = FALSE;
		while ((delete_done == FALSE) &&
			(hcfs_system->system_going_down == FALSE)) {
			nanosleep(&time_to_sleep, NULL);
			delete_done = TRUE;
			sem_wait(&(delete_ctl.delete_op_sem));
			for (count = 0; count < MAX_DELETE_CONCURRENCY;
								count++) {
				if ((delete_ctl.threads_in_use[count] == TRUE)
					&&
					(delete_ctl.delete_threads[count].inode
							== ptr->inode)) {
					delete_done = FALSE;
					break;
				}
			}
			sem_post(&(delete_ctl.delete_op_sem));
		}

		flock(fileno(backend_metafptr), LOCK_UN);
		backend_mlock = FALSE;
	}


errcode_handle:
	/* TODO: If cannot handle object deletion from metaptr, need to scrub */
	if (backend_metafptr != NULL) {
		if (backend_mlock == TRUE)
			flock(fileno(backend_metafptr), LOCK_UN);
		fclose(backend_metafptr);
		unlink(backend_metapath);
	}

	/* Check threads error */
	if (dsync_ctl.threads_error[which_dsync_index] == TRUE) {
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	if (hcfs_system->system_going_down == TRUE) {
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	/* Delete meta */
	sem_wait(&(delete_ctl.delete_queue_sem));
	sem_wait(&(delete_ctl.delete_op_sem));
	curl_id = -1;
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++) {
#if ENABLE(DEDUP)
		ret_val = _use_delete_thread(count, which_dsync_index, FALSE,
				ptr->inode, -1, 0, NULL);
#else
		ret_val = _use_delete_thread(count, which_dsync_index, FALSE,
				ptr->inode, -1, 0);
#endif
		if (ret_val == 0) {
			curl_id = count;
			break;
		}
	}
	if (CURRENT_BACKEND == GOOGLEDRIVE) {
		tmp_dt = &(delete_ctl.delete_threads[curl_id]);
		memset(&(tmp_dt->gdrive_info), 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
		strncpy(tmp_dt->gdrive_info.fileID, metaID, GDRIVE_ID_LENGTH);
	}
	PTHREAD_REUSE_run(&(delete_ctl.threads_no[curl_id]),
		(void *)&con_object_dsync,
		(void *)&(delete_ctl.delete_threads[curl_id]));

	delete_ctl.threads_created[curl_id] = TRUE;
	delete_ctl.threads_finished[curl_id] = FALSE;
	sem_post(&(delete_ctl.delete_op_sem));

	PTHREAD_REUSE_join(&(delete_ctl.threads_no[curl_id]));

	sem_wait(&(delete_ctl.delete_op_sem));
	delete_ctl.threads_in_use[curl_id] = FALSE;
	delete_ctl.threads_created[curl_id] = FALSE;
	delete_ctl.total_active_delete_threads--;
	sem_post(&(delete_ctl.delete_op_sem));
	sem_post(&(delete_ctl.delete_queue_sem));

	/*Wait for any upload to complete and change super inode
		from to_delete to deleted*/

	/* Check threads error */
	if (dsync_ctl.threads_error[which_dsync_index] == TRUE) {
		dsync_ctl.threads_finished[which_dsync_index] = TRUE;
		sem_post(&(dsync_ctl.pause_sem));
		return;
	}

	/* todelete meta file may existed if system had been upgraded */
	unlink(todel_metapath);

	/* Update FS stat in the backend if updated previously */
	update_backend_stat(root_inode, -backend_size_change,
			-meta_size_change, -1, pin_size_delta,
			pin_size_delta_blk,
			-round_size(meta_size_change));

	_check_del_progress_file(this_inode);
	super_block_delete(this_inode);
	super_block_reclaim();
	dsync_ctl.threads_finished[which_dsync_index] = TRUE;
	sem_post(&(dsync_ctl.pause_sem));
	return;
}

/************************************************************************
*
* Function name: do_meta_delete
*        Inputs: ino_t this_inode, CURL_HANDLE *curl_handle
*       Summary: Given curl handle "curl_handle", delete the meta object
*                of inode number "this_inode" from backend.
*  Return value: 0 if successful, and negation of errcode if not.
*
*************************************************************************/
int32_t do_meta_delete(ino_t this_inode,
		       CURL_HANDLE *curl_handle,
		       GOOGLEDRIVE_OBJ_INFO *gdrive_info)
{
	char objname[1000];
	int32_t ret_val, ret;

	sprintf(objname, "meta_%" PRIu64 "", (uint64_t)this_inode);
	write_log(10, "Debug meta deletion: objname %s, inode %" PRIu64 "\n",
						objname, (uint64_t)this_inode);
	sprintf(curl_handle->id, "delete_meta_%" PRIu64 "", (uint64_t)this_inode);
	ret_val = hcfs_delete_object(objname, curl_handle, gdrive_info);
	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else if (ret_val == 404)
		ret = -ENOENT;
	else
		ret = -EIO;

	return ret;
}

/************************************************************************
*
* Function name: do_block_delete
*        Inputs: ino_t this_inode, int64_t block_no, CURL_HANDLE *curl_handle
*       Summary: Given curl handle "curl_handle", delete the block object
*                of inode number "this_inode", block no "block_no" from backend.
*  Return value: 0 if successful, and negation of errcode if not.
*
*************************************************************************/
int32_t do_block_delete(ino_t this_inode, int64_t block_no, int64_t seq,
#if ENABLE(DEDUP)
				uint8_t *obj_id, CURL_HANDLE *curl_handle)
#else
				CURL_HANDLE *curl_handle,
				GOOGLEDRIVE_OBJ_INFO *gdrive_info)
#endif
{
	char objname[400];
	int32_t ret_val, ret;
#if ENABLE(DEDUP)
	DDT_BTREE_NODE tree_root;
	DDT_BTREE_META ddt_meta;
	char obj_id_str[OBJID_STRING_LENGTH];
	FILE *ddt_fptr;
	int32_t ddt_fd;
#endif

/* Handle objname - consider platforms, dedup flag  */
#if ENABLE(DEDUP)
	/* Object named by block hashkey */
	fetch_backend_block_objname(objname, obj_id);

	/* Get dedup table meta */
	ddt_fptr = get_ddt_btree_meta(obj_id, &tree_root, &ddt_meta);
	if (ddt_fptr == NULL) {
		/* Can't access ddt btree file */
		return -EBADF;
	}
	ddt_fd = fileno(ddt_fptr);

	/* Update ddt */
	int32_t ddt_ret = decrease_ddt_el_refcount(obj_id, &tree_root, ddt_fd, &ddt_meta);
#else
	fetch_backend_block_objname(objname, this_inode, block_no, seq);
	/* Force to delete */
#endif


#if ENABLE(DEDUP)
	if (ddt_ret == 0)
#endif
	{
		write_log(10,
			"Debug delete object: objname %s, inode %" PRIu64 ", block %lld\n",
			objname, (uint64_t)this_inode, block_no);
		sprintf(curl_handle->id, "delete_blk_%" PRIu64 "_%" PRId64"_%"PRId64,
				(uint64_t)this_inode, block_no, seq);
		ret_val = hcfs_delete_object(objname, curl_handle, gdrive_info);
		/* Already retried in get object if necessary */
		if ((ret_val >= 200) && (ret_val <= 299))
			ret = 0;
		else if (ret_val == 404)
			ret = -ENOENT;
		else
			ret = -EIO;
	}
#if ENABLE(DEDUP)
	else if (ddt_ret == 1) {
		write_log(10, "Only decrease refcount of object - %s", objname);
		ret = 0;
	} else if (ddt_ret == 2) {
		printf("Can't find this element, maybe deleted already?\n");
		ret = 0;
	} else {
		printf("ERROR delete el tree\n");
		ret = -EIO;
	}

	flock(ddt_fd, LOCK_UN);
	fclose(ddt_fptr);
#endif

	if (ret < 0 && ret != -ENOENT)
		write_log(4, "Fail to delete object %s. Code %d. Http ret code "
			     "%d.", objname, ret, ret_val);
	return ret;
}
/* TODO: How to retry object deletion later if failed at some point */

/************************************************************************
*
* Function name: con_object_dsync
*        Inputs: DELETE_THREAD_TYPE *delete_thread_ptr
*       Summary: For the info marked in delete thread "delete_thread_ptr",
*                check whether this is a block object or meta object deletion.
*  Return value: None
*
*************************************************************************/
void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr)
{
	int32_t which_curl;
	int32_t which_index;
	int32_t dsync_index;
	int32_t ret, pause_status;

	which_curl = delete_thread_ptr->which_curl;
	which_index = delete_thread_ptr->which_index;
	dsync_index = delete_thread_ptr->dsync_index;

	if (delete_thread_ptr->is_block == TRUE)
		ret = do_block_delete(delete_thread_ptr->inode,
			delete_thread_ptr->blockno, delete_thread_ptr->seq,
#if ENABLE(DEDUP)
			delete_thread_ptr->obj_id,
#endif
			&(delete_curl_handles[which_curl]),
			&(delete_thread_ptr->gdrive_info));
	else
		ret = do_meta_delete(delete_thread_ptr->inode,
			&(delete_curl_handles[which_curl]),
			&(delete_thread_ptr->gdrive_info));

	if (ret < 0 && ret != -ENOENT) { /* do not care 404 not found */
		delete_ctl.threads_error[which_index] = TRUE;
		dsync_ctl.retry_right_now[dsync_index] = TRUE;
	} else {
		delete_ctl.threads_error[which_index] = FALSE;
	}

	delete_ctl.threads_finished[which_index] = TRUE;
	sem_check_and_release(&(delete_ctl.pause_sem), &pause_status);
}

/* Helper for creating threads for deletion */
int32_t _dsync_use_thread(int32_t index, ino_t this_inode, mode_t this_mode)
{
	if (dsync_ctl.threads_in_use[index] != 0)
		return -1;
	dsync_ctl.threads_in_use[index] = this_inode;
	dsync_ctl.threads_created[index] = FALSE;
	dsync_ctl.threads_finished[index] = FALSE;
	dsync_thread_info[index].inode = this_inode;
	dsync_thread_info[index].this_mode = this_mode;
	dsync_thread_info[index].which_index = index;
	PTHREAD_REUSE_run(&(dsync_ctl.inode_dsync_thread[index]),
		(void *)&dsync_single_inode,
				(void *)&(dsync_thread_info[index]));
	dsync_ctl.threads_created[index] = TRUE;
	dsync_ctl.total_active_dsync_threads++;
	return 0;
}

/************************************************************************
*
* Function name: delete_loop
*        Inputs: void *arg
*       Summary: Main function for checking whether there is a need to
*                delete objects from backend.
*  Return value: None
*
*************************************************************************/

void *delete_loop(void *ptr)
{
	ino_t inode_to_dsync, inode_to_check, retry_inode;
	SUPER_BLOCK_ENTRY tempentry;
	int32_t count;
	char in_dsync;
	int32_t ret_val;

	UNUSED(ptr);
	init_delete_control();
	init_dsync_control();

	write_log(2, "Start delete loop\n");

	inode_to_check = 0;
	while (hcfs_system->system_going_down == FALSE) {
		/* sleep and continue if backend is offline */
		if (hcfs_system->sync_paused == TRUE) {
			sem_wait(&(hcfs_system->dsync_wait_sem));
			continue;
		}

		write_log(10, "Debug statistics %d, %d\n",
		          sys_super_block->head.num_to_be_deleted,
		          dsync_ctl.total_active_dsync_threads);
		/* Sleep if backend is online and system is running */
		if (sys_super_block->head.num_to_be_deleted <=
		    dsync_ctl.total_active_dsync_threads) {
			sem_wait(&(hcfs_system->dsync_wait_sem));
			continue;
		}

		/* Get the first to-delete inode if inode_to_check is none. */
		sem_wait(&(dsync_ctl.dsync_queue_sem));
		/* Terminate immediately if system is going down */
		if (hcfs_system->system_going_down == TRUE) {
			sem_post(&(dsync_ctl.dsync_queue_sem));
			break;
		}

		sem_wait(&(dsync_ctl.dsync_op_sem));
		retry_inode = pull_retry_inode(&(dsync_ctl.retry_list));
		sem_post(&(dsync_ctl.dsync_op_sem));

		super_block_share_locking();
		if (retry_inode > 0) { /* Jump to retried inode */
			inode_to_check = retry_inode;
		} else {
			ino_t tmp_ino;

			/* Go to first to-delete inode if it is now
			 * end of queue */
			tmp_ino = sys_super_block->head.first_to_delete_inode;
			if (inode_to_check == 0)
				inode_to_check = tmp_ino;
		}
		/* Find next to-delete inode if inode_to_check is not the
			last one. */
		inode_to_dsync = 0;
		if (inode_to_check != 0) {
			inode_to_dsync = inode_to_check;
/* FEATURE TODO: double check that super block rebuild will happen before
anything can be deleted */
			ret_val = read_super_block_entry(inode_to_dsync,
								&tempentry);

			if ((ret_val < 0) ||
				(tempentry.status != TO_BE_DELETED)) {
				write_log(4, "Warn: Delete backend inode %"
						PRIu64" but status is %d",
						(uint64_t)inode_to_dsync,
						tempentry.status);
				inode_to_dsync = 0;
				inode_to_check = 0;
			} else {
				inode_to_check = tempentry.util_ll_next;
			}
		}
		super_block_share_release();

		write_log(6, "Inode to delete is %" PRIu64 "\n",
		          (uint64_t) inode_to_dsync);

		/* Delete the meta/block of inode_to_dsync if it
			finish dsynced. */
		if (inode_to_dsync != 0) {
			sem_wait(&(dsync_ctl.dsync_op_sem));
			/*First check if this inode is actually being
				dsynced now*/
			in_dsync = FALSE;
			for (count = 0; count < MAX_DSYNC_CONCURRENCY;
								count++) {
				if (dsync_ctl.threads_in_use[count] ==
							inode_to_dsync) {
					in_dsync = TRUE;
					break;
				}
			}

			if (in_dsync == FALSE) {
				for (count = 0; count < MAX_DSYNC_CONCURRENCY;
								count++) {
					ret_val = _dsync_use_thread(count,
							inode_to_dsync,
						tempentry.inode_stat.mode);
					if (ret_val == 0)
						break;
				}
				sem_post(&(dsync_ctl.dsync_op_sem));
			} else {  /*If already dsyncing to cloud*/
				sem_post(&(dsync_ctl.dsync_op_sem));
				sem_post(&(dsync_ctl.dsync_queue_sem));
			}
		} else {
			sem_post(&(dsync_ctl.dsync_queue_sem));
		}
	}
	_destroy_delete_controls();
	return NULL;
}
