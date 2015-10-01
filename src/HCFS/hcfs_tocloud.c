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
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/un.h>
#include <openssl/sha.h>

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

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

static inline int _get_inode_sync_error(ino_t inode, char *sync_error)
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

/* Don't need to collect return code for the per-inode sync thread, as
the error handling for syncing this inode will be handled in
sync_single_inode. */
static inline void _sync_terminate_thread(int index)
{
	int ret;
	int tag_ret;
	ino_t inode;

	if ((sync_ctl.threads_in_use[index] != 0) &&
	    (sync_ctl.threads_created[index] == TRUE)) {
		ret = pthread_tryjoin_np(sync_ctl.inode_sync_thread[index],
			NULL);
		if (ret == 0) {
			inode = sync_ctl.threads_in_use[index];
			/* Reverting do not need communicate with fuse */
			if (sync_ctl.is_revert[index] == FALSE) {
				tag_ret = tag_status_on_fuse(inode, FALSE, 0);
				if (tag_ret < 0) {
					write_log(0, "Fail to tag inode %lld "
						"as NOT_UPLOADING in %s\n",
						inode, __func__);
				}
			}
			close_progress_info(sync_ctl.progress_fd[index], inode);

			sync_ctl.threads_in_use[index] = 0;
			sync_ctl.threads_created[index] = FALSE;
			sync_ctl.total_active_sync_threads--;
			sem_post(&(sync_ctl.sync_queue_sem));
		}
	}
}

void collect_finished_sync_threads(void *ptr)
{
	int count;
	struct timespec time_to_sleep;

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
	FILE *metafptr, *toupload_metafptr;
	char thismetapath[METAPATHLEN], toupload_metapath[200];
	char blockpath[400];
	unsigned char blk_obj_id[OBJID_LENGTH];
	ino_t this_inode;
	off_t page_filepos;
	long long e_index;
	long long blockno;
	BLOCK_ENTRY_PAGE temppage;
	char is_delete;
	BLOCK_ENTRY *tmp_entry;
	size_t tmp_size, ret_size;
	DELETE_THREAD_TYPE *tmp_del;
	char need_delete_object, is_toupload_meta_lock;
	BLOCK_UPLOADING_STATUS temp_block_uploading_status;
	int progress_fd;
	char toupload_exist, finish_uploading;

	if (upload_ctl.threads_in_use[index] == 0)
		return 0;

	if (upload_ctl.upload_threads[index].is_block != TRUE)
		return 0;

	if (upload_ctl.threads_created[index] != TRUE)
		return 0;

	ret = pthread_tryjoin_np(upload_ctl.upload_threads_no[index], NULL);

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
			upload_ctl.total_active_upload_threads--;

			sem_post(&(upload_ctl.upload_queue_sem));
			return 0; /* Error already marked */
		}
	}
	sem_post(&(sync_ctl.sync_op_sem));

	this_inode = upload_ctl.upload_threads[index].inode;
	is_delete = upload_ctl.upload_threads[index].is_delete;
	page_filepos = upload_ctl.upload_threads[index].page_filepos;
	e_index = upload_ctl.upload_threads[index].page_entry_index;
	blockno = upload_ctl.upload_threads[index].blockno;
	progress_fd = upload_ctl.upload_threads[index].progress_fd;

	/* Terminate directly whenthread is used to delete old data on cloud */
	if (upload_ctl.upload_threads[index].is_backend_delete == TRUE) {
		char backend_exist;

		/* Do NOT need to lock upload_op_sem. It is locked by caller. */
		upload_ctl.threads_in_use[index] = FALSE;
		upload_ctl.threads_created[index] = FALSE;
		upload_ctl.total_active_upload_threads--;

		sem_post(&(upload_ctl.upload_queue_sem));
		backend_exist = FALSE;
		set_progress_info(progress_fd, blockno, NULL, &backend_exist,
			NULL, NULL, NULL);

		return 0;
	}

	ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;

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
	memset(&temp_block_uploading_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	temp_block_uploading_status.finish_uploading = TRUE;
	temp_block_uploading_status.to_upload_seq = 0; /* temp */
	SET_TOUPLOAD_BLOCK_EXIST(temp_block_uploading_status.block_exist);
	toupload_exist = TRUE;
	finish_uploading = TRUE;
	set_progress_info(progress_fd, blockno, &temp_block_uploading_status,
		TOUPLOAD_BLOCKS);
	/* TODO: if to_upload_seq == backend_seq, then return ? */
#endif

	need_delete_object = FALSE;
	/* Perhaps the file is deleted already. If not, modify the block
	  status in file meta. */
	if (access(thismetapath, F_OK) == 0) {
		metafptr = fopen(thismetapath, "r+");
		if (metafptr == NULL) {
			errcode = errno;
			if (errcode != ENOENT) {
				write_log(0, "IO error in %s. Code %d, %s\n",
					  __func__, errcode, strerror(errcode));
				return -errcode;
			}
		}
		if (metafptr != NULL) {
			setbuf(metafptr, NULL);
			flock(fileno(metafptr), LOCK_EX);
			/*Perhaps the file is deleted already*/
			if (!access(thismetapath, F_OK)) {
				FSEEK(metafptr, page_filepos, SEEK_SET);
				FREAD(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
				      metafptr);
				tmp_entry = &(temppage.block_entries[e_index]);
				tmp_size = sizeof(BLOCK_ENTRY_PAGE);
				if ((tmp_entry->status == ST_LtoC) &&
				    (is_delete == FALSE)) {
					tmp_entry->status = ST_BOTH;
					tmp_entry->uploaded = TRUE;
#if (DEDUP_ENABLE)
					// Store hash in block meta too
					memcpy(tmp_entry->obj_id, blk_obj_id,
								OBJID_LENGTH);
#endif
					ret = fetch_block_path(blockpath,
						this_inode, blockno);
					if (ret < 0) {
						errcode = ret;
						goto errcode_handle;
					}
					SETXATTR(blockpath, "user.dirty", "F",
						 1, 0);

					FSEEK(metafptr, page_filepos, SEEK_SET);
					FWRITE(&temppage, tmp_size, 1,
								metafptr);
				} else {
					if ((tmp_entry->status ==
					     ST_TODELETE) &&
					    (is_delete == TRUE)) {
						tmp_entry->status = ST_NONE;
						tmp_entry->uploaded = FALSE;
						FSEEK(metafptr, page_filepos,
						      SEEK_SET);
						FWRITE(&temppage, tmp_size, 1,
						       metafptr);
					}
				}
				/*Check if status is ST_NONE. If so,
				the block is removed due to truncating.
				(And perhaps block deletion thread finished
				earlier than upload, and deleted nothing.)
				Need to schedule block for deletion due to
				truncating*/
				if ((tmp_entry->status == ST_NONE) &&
				    (is_delete == FALSE)) {
					write_log(5,
						  "Debug upload block gone\n");
					need_delete_object = TRUE;
				}
			}
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
		}
	}

	/* If file is deleted or block already deleted, create deleted-thread
	   to delete cloud block data. */
/*	if ((access(thismetapath, F_OK) == -1) || (need_delete_object)) {
		sem_wait(&(delete_ctl.delete_queue_sem));
		sem_wait(&(delete_ctl.delete_op_sem));
		which_curl = -1;
		for (count2 = 0; count2 < MAX_DELETE_CONCURRENCY; count2++) {
			if (delete_ctl.threads_in_use[count2] == FALSE) {
				delete_ctl.threads_in_use[count2] = TRUE;
				delete_ctl.threads_created[count2] = FALSE;

				tmp_del = &(delete_ctl.delete_threads[count2]);
				tmp_del->is_block = TRUE;
				tmp_del->inode = this_inode;
				tmp_del->blockno = blockno;
				tmp_del->which_curl = count2;
#if (DEDUP_ENABLE)
				memcpy(tmp_del->obj_id, blk_obj_id, OBJID_LENGTH);
#endif

				delete_ctl.total_active_delete_threads++;
				which_curl = count2;
				break;
			}
		}
		sem_post(&(delete_ctl.delete_op_sem));
		pthread_create(
		    &(delete_ctl.threads_no[which_curl]), NULL,
		    (void *)&con_object_dsync,
		    (void *)&(delete_ctl.delete_threads[which_curl]));

		delete_ctl.threads_created[which_curl] = TRUE;
	}
*/
	/* Finally reclaim the uploaded-thread. */
	upload_ctl.threads_in_use[index] = FALSE;
	upload_ctl.threads_created[index] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_queue_sem));

	return 0;

errcode_handle:
#ifdef DEDUP_ENABLE
	if (is_toupload_meta_lock == TRUE) {
		flock(fileno(toupload_metafptr), LOCK_UN);
		fclose(toupload_metafptr);	
	}
#endif
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	return errcode;
}

void collect_finished_upload_threads(void *ptr)
{
	int count, ret, count1;
	struct timespec time_to_sleep;

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
			// Reset uploaded flag for upload thread
			upload_ctl.upload_threads[count].is_upload = FALSE;
#endif
			upload_ctl.threads_in_use[count] = FALSE;
			upload_ctl.threads_created[count] = FALSE;
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
	sync_ctl.total_active_sync_threads = 0;

	pthread_create(&(sync_ctl.sync_handler_thread), NULL,
		       (void *)&collect_finished_sync_threads, NULL);
}

void init_upload_control(void)
{
	int count, ret_val;

	memset(&upload_ctl, 0, sizeof(UPLOAD_THREAD_CONTROL));
	memset(&upload_curl_handles, 0,
	       sizeof(CURL_HANDLE) * MAX_UPLOAD_CONCURRENCY);

	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
		snprintf(upload_curl_handles[count].id, 255, "upload_thread_%d",
			 count);
		ret_val = hcfs_init_backend(&(upload_curl_handles[count]));
	}

	sem_init(&(upload_ctl.upload_op_sem), 0, 1);
	sem_init(&(upload_ctl.upload_queue_sem), 0, MAX_UPLOAD_CONCURRENCY);
	memset(&(upload_ctl.threads_in_use), 0,
	       sizeof(char) * MAX_UPLOAD_CONCURRENCY);
	memset(&(upload_ctl.threads_created), 0,
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
		/* Delete all previously cached FS stat */
		while ((ret == 0) && (tmpptr != NULL)) {
			if (strncmp(tmp_entry.d_name, "FSstat", 6) == 0) {
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
	snprintf(sync_stat_ctl.statcurl.id, 255, "sync_stat_ctl");

	hcfs_init_backend(&(sync_stat_ctl.statcurl));

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
				char is_upload, unsigned char old_obj_id[],
#endif
				ino_t this_inode, long long block_count,
				long long seq, off_t page_pos,
				long long e_index, int progress_fd,
				char is_backend_delete)
{
	int which_curl, count;

	which_curl = -1;
	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
		if (upload_ctl.threads_in_use[count] == FALSE) {
			upload_ctl.threads_in_use[count] = TRUE;
			upload_ctl.threads_created[count] = FALSE;
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
			upload_ctl.upload_threads[count].is_backend_delete =
							is_backend_delete;
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
	*upload_seq -= 1; /* Return old seq so that backend stat works */

	return 0;
}



#ifdef DEDUP_ENABLE
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
	const BLOCK_UPLOADING_STATUS *block_info, long long *block_seq)
{
	char finish_uploading;
	long long to_upload_seq;
	long long backend_seq;

	finish_uploading = block_info->finish_uploading;
	to_upload_seq = block_info->to_upload_seq;
	backend_seq = block_info->backend_seq;

	if (delete_which_one == TOUPLOAD_BLOCKS) {
		if (finish_uploading == FALSE)
			return -1;
		if (to_upload_seq == backend_seq)
			return -1;
		if (to_upload_seq == 0)
			return -1;

		*block_seq = to_upload_seq;
		return 0;
	}

	if (delete_which_one == BACKEND_BLOCKS) {
		if (backend_seq == 0)
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
	BLOCK_UPLOADING_STATUS block_info;
	long long block_count;
	long long block_seq;
	unsigned char block_objid[OBJID_LENGTH];
	int ret;
	int which_curl;

	if (delete_which_one == TOUPLOAD_BLOCKS)
	write_log(10, "Debug: Delete those blocks uploaded just now for "
		"inode_%ld because local meta disapper\n", inode);

	for (block_count = 0; block_count < total_blocks; block_count++) {
		ret = get_progress_info(progress_fd, block_count,
			&block_info); // TODO: read just one time for a page?
		if (ret == 0) { /* Both block entry and whole page are empty */
			block_count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
			continue;
		} else if (ret < 0) { /* TODO: error handling */
			continue;
		}

#if (DEDUP_ENABLE)
		ret = _choose_deleted_block(delete_which_one,
			&block_info, block_objid);
#else
		block_seq = 0;
		ret = _choose_deleted_block(delete_which_one,
			&block_info, &block_seq);
#endif
		if (ret < 0)
			continue;

		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
		which_curl = _select_upload_thread(TRUE, FALSE,
			TRUE, block_objid,
			inode, block_count, block_seq,
			0, 0, progress_fd, TRUE);
#else
		which_curl = _select_upload_thread(TRUE, FALSE,
			inode, block_count, block_seq,
			0, 0, progress_fd, TRUE);
#endif
		sem_post(&(upload_ctl.upload_op_sem));
		dispatch_delete_block(which_curl);
	}

	_busy_wait_all_specified_upload_threads(inode);
	return 0;
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
	char toupload_metapath[400];
	char backend_metapath[500];
	char local_metapath[METAPATHLEN];
	ino_t this_inode;
	FILE *toupload_metafptr, *local_metafptr, *backend_metafptr;
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
	off_t toupload_size, toupload_trunc_block_size;
	BLOCK_ENTRY *tmp_entry;
	long long temp_trunc_size;
	ssize_t ret_ssize;
	size_t ret_size;
	char sync_error;
	int count1;
	long long upload_seq;
	ino_t root_inode;
	long long backend_size;
	long long size_diff;
	int progress_fd;
	char first_upload, is_local_meta_deleted;
	BLOCK_UPLOADING_STATUS temp_uploading_status;
	char toupload_exist, finish_uploading;
	char is_revert;

	progress_fd = ptr->progress_fd;
	this_inode = ptr->inode;
	is_revert = ptr->is_revert;
	sync_error = FALSE;

	ret = fetch_toupload_meta_path(toupload_metapath, this_inode);
	if (ret < 0) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		return;
	}

	ret = fetch_meta_path(local_metapath, this_inode);
#ifdef ARM_32bit_
	write_log(10, "Sync inode %lld, mode %d\n", ptr->inode, ptr->this_mode);
#else
	write_log(10, "Sync inode %ld, mode %d\n", ptr->inode, ptr->this_mode);
#endif
	if (ret < 0) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		return;
	}

	/* Copy local meta, -EEXIST means it has been copied */
	if (is_revert == FALSE) {
		ret = check_and_copy_file(local_metapath, toupload_metapath);
		if (ret < 0) {
			if (ret != -EEXIST) {
				super_block_update_transit(ptr->inode,
					FALSE, TRUE);
				return;
			}
		}
	}

	/* Open temp meta to be uploaded */
	toupload_metafptr = fopen(toupload_metapath, "r");
	if (toupload_metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		return;
	}

	/* Open local meta */
	local_metafptr = fopen(local_metapath, "r+");
	if (local_metafptr == NULL) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				  errcode, strerror(errcode));
			super_block_update_transit(ptr->inode, FALSE, TRUE);
		}
		/* If meta file is gone, the inode is deleted and we don't need
		to sync this object anymore. */
		fclose(toupload_metafptr);
		unlink(toupload_metapath);
		return;
	}
	setbuf(local_metafptr, NULL);

	/* Download backend meta and fetch seq number if it is regfile 
	and is not reverting mode */
	if (S_ISREG(ptr->this_mode) && (is_revert == FALSE)) {
		first_upload = FALSE;
		backend_metafptr = NULL;
		fetch_backend_meta_path(backend_metapath, this_inode);
		ret = download_meta_from_backend(this_inode, backend_metapath,
				&backend_metafptr);
		if (ret < 0) {
			super_block_update_transit(ptr->inode, FALSE, TRUE);
			fclose(local_metafptr);
			fclose(toupload_metafptr);
			unlink(toupload_metapath);
			return;
		} else {
		/* backend_metafptr may be NULL when first uploading */
			if (backend_metafptr == NULL) {
				write_log(10, "Debug: upload first time\n");
				first_upload = TRUE;
			}
		}

		/* Init backend info and close */
		if (first_upload == FALSE) {
			PREAD(fileno(backend_metafptr), &tempfilestat,
				sizeof(struct stat), 0);
			backend_size = tempfilestat.st_size;
			total_backend_blocks = (backend_size == 0) ? 
				0 : (backend_size - 1) / MAX_BLOCK_SIZE + 1;
			ret = init_progress_info(progress_fd,
				total_backend_blocks, backend_size,
				backend_metafptr);

			fclose(backend_metafptr);
			backend_metafptr = NULL;
			UNLINK(backend_metapath);
		} else {
			ret = init_progress_info(progress_fd, 0, 0,
				backend_metafptr);
			backend_size = 0;
			total_backend_blocks = 0; 
		}

		if (ret < 0) { /* init progress fail */
			super_block_update_transit(ptr->inode, FALSE, TRUE);
			fclose(toupload_metafptr);
			unlink(toupload_metapath);
			fclose(local_metafptr);
			return;
		}
	}

	/* If it is reverting mode, read progress meta and get info */
	if (is_revert == TRUE) {
		PROGRESS_META progress_meta;

		memset(&progress_meta, 0, sizeof(PROGRESS_META));
		PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
		backend_size = progress_meta.backend_size;
		total_backend_blocks = progress_meta.total_backend_blocks;
		if (progress_meta.finish_init_backend_data == FALSE) {
			write_log(2, "Crash before uploading, do nothing and"
				" cancel uploading\n");
			fclose(toupload_metafptr);
			unlink(toupload_metapath);
			fclose(local_metafptr);		
			super_block_update_transit(ptr->inode, FALSE, TRUE);
			return;
		}
	}

	/* Upload block if mode is regular file */
	if (S_ISREG(ptr->this_mode)) {
		FSEEK(toupload_metafptr, 0, SEEK_SET);
		FREAD(&tempfilestat, sizeof(struct stat), 1,
			toupload_metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1,
			toupload_metafptr);

		toupload_size = tempfilestat.st_size;
		root_inode = tempfilemeta.root_inode;

		/* Check if need to sync past the current size */
		ret_ssize = fgetxattr(fileno(toupload_metafptr),
			"user.trunc_size", &temp_trunc_size, sizeof(long long));

		toupload_trunc_block_size = toupload_size;
		if ((ret_ssize >= 0) && (toupload_size < temp_trunc_size)) {
			toupload_trunc_block_size = temp_trunc_size;
		}

		/* Compute number of blocks */
		if (toupload_trunc_block_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((toupload_trunc_block_size - 1)
				/ MAX_BLOCK_SIZE) + 1;

		/* Begin to upload blocks */
		current_page = -1;
		is_local_meta_deleted = FALSE;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {

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
			/*TODO: error handling here if cannot read correctly*/

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

			tmp_entry = &(local_temppage.
					block_entries[e_index]);
			local_block_status = tmp_entry->status;

			/*** Case 1: Local is dirty. Update status & upload ***/
			if (toupload_block_status == ST_LDISK) {
				if (local_block_status != ST_TODELETE) {
					tmp_entry->status = ST_LtoC;
					/* Update local meta */
					FSEEK_ADHOC_SYNC_LOOP(local_metafptr,
						page_pos, SEEK_SET, TRUE);
					FWRITE_ADHOC_SYNC_LOOP(&local_temppage,
						sizeof(BLOCK_ENTRY_PAGE),
						1, local_metafptr, TRUE);
				}
				flock(fileno(local_metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
				which_curl = _select_upload_thread(TRUE, FALSE,
						TRUE,
						tmp_entry->obj_id,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
#else
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
#endif

				sem_post(&(upload_ctl.upload_op_sem));
				ret = dispatch_upload_block(which_curl);
				if (ret < 0) {
					sync_error = TRUE;
					break;
				}
				/*TODO: Maybe should also first copy block
					out first*/
				continue;
			}

			/*** Case 2: Fail to upload last time, re-upload it.***/
			if (toupload_block_status == ST_LtoC) {
				flock(fileno(local_metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
#if (DEDUP_ENABLE)
				which_curl = _select_upload_thread(TRUE, FALSE,
						TRUE,
						tmp_entry->obj_id,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
#else
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
#endif

				sem_post(&(upload_ctl.upload_op_sem));
				ret = dispatch_upload_block(which_curl);
				if (ret < 0) {
					sync_error = TRUE;
					break;
				}
				continue;
			}

			/*** Case 3: Local block is deleted. Delete backend
			   block data, too. ***/
			if (toupload_block_status == ST_TODELETE) {
				write_log(10, "Debug: block_%lld is TO_DELETE\n", block_count);
				/*tmp_entry->status = ST_LtoC;*/
				/* Update local meta */
				/*FSEEK_ADHOC_SYNC_LOOP(local_metafptr,
					page_pos, SEEK_SET, TRUE);
				FWRITE_ADHOC_SYNC_LOOP(&local_temppage,
					sizeof(BLOCK_ENTRY_PAGE),
					1, local_metafptr, TRUE);
				flock(fileno(local_metafptr), LOCK_UN);
				set_progress_info(progress_fd, block_count,
					TRUE, 0, 0);*/
#if (DEDUP_ENABLE)
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
#else
				flock(fileno(local_metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
				which_curl = _select_upload_thread(TRUE, TRUE,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
				sem_post(&(upload_ctl.upload_op_sem));
				dispatch_delete_block(which_curl);
#endif
				continue;	
			}

			if (toupload_block_status == ST_NONE) {
				write_log(10, "Debug: block_%lld is ST_NONE\n",
					block_count);
				flock(fileno(local_metafptr), LOCK_UN);
				finish_uploading = TRUE;
				toupload_exist = FALSE;
				set_progress_info(progress_fd, block_count,
					&toupload_exist, NULL, NULL, NULL,
					&finish_uploading);

			} else { /* ST_BOTH, ST_CtoL */
				write_log(10, "Debug: block_%lld is %d\n",
						block_count, toupload_block_status);
				flock(fileno(local_metafptr), LOCK_UN);

				finish_uploading = TRUE;
				toupload_exist = TRUE;
#ifdef DEDUP_ENABLE
				set_progress_info(progress_fd,
						block_count, &toupload_exist,
						NULL, tmp_entry->obj_id, NULL,
						&finish_uploading);	
#else
				memset(&temp_uploading_status, 0,
					sizeof(BLOCK_UPLOADING_STATUS));
				temp_uploading_status.to_upload_seq = 0; /*temp*/
				set_progress_info(progress_fd,
					block_count, &toupload_exist, NULL,
					&temp_block_uploading_status.to_upload_seq, NULL,
					&finish_uploading);
#endif
			}
		}
		/* ---End of syncing blocks loop--- */

		/* Block sync should be done here. Check if all upload
		threads for this inode has returned before starting meta sync*/
		_busy_wait_all_specified_upload_threads(ptr->inode);

		/* If meta is deleted when uploading, delete backend blocks */
		if (is_local_meta_deleted == TRUE) {
			delete_backend_blocks(progress_fd, total_blocks,
				ptr->inode, TOUPLOAD_BLOCKS);
			fclose(local_metafptr);
			fclose(toupload_metafptr);
			unlink(toupload_metapath);
			return;
		}
	}

	/*Check if metafile still exists. If not, forget the meta upload*/
	/*if (access(thismetapath, F_OK) < 0)
		return;*/

	/* Abort sync to cloud if error occured */
	if (sync_error == TRUE) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		fclose(local_metafptr);
		fclose(toupload_metafptr);
		unlink(toupload_metapath);
		return;
	}

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
		increment_upload_seq(local_metafptr, &upload_seq);

		/* Get root_inode from meta */
		if (S_ISREG(ptr->this_mode)) {
			size_diff = toupload_size - backend_size;
		}
		if (S_ISDIR(ptr->this_mode)) {
			FSEEK(local_metafptr, sizeof(struct stat), SEEK_SET);
			FREAD(&tempdirmeta, sizeof(DIR_META_TYPE),
				1, local_metafptr);
			root_inode = tempdirmeta.root_inode;
			size_diff = 0;
		}
		if (S_ISLNK(ptr->this_mode)) {
			FSEEK(local_metafptr, sizeof(struct stat), SEEK_SET);
			FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE),
				1, local_metafptr);

			root_inode = tempsymmeta.root_inode;
			size_diff = 0;
		}

		write_log(10, "Debug: Now inode %ld has upload_seq = %lld\n", ptr->inode, upload_seq);
		ret = schedule_sync_meta(toupload_metapath, which_curl);

		if (ret < 0)
			sync_error = TRUE;
		flock(fileno(local_metafptr), LOCK_UN);
		fclose(local_metafptr);
		fclose(toupload_metafptr);

		pthread_join(upload_ctl.upload_threads_no[which_curl], NULL);
		/*TODO: Need to check if metafile still exists.
			If not, schedule the deletion of meta*/

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));

		if (sync_error == FALSE) {
			/* Check the error in sync_thread */
			write_log(10, "Checking for other error\n");
			_get_inode_sync_error(ptr->inode, &sync_error);	
		}
		if (sync_error == TRUE) {
#ifdef ARM_32bit_
			write_log(10,
				  "Sync inode %lld to backend incomplete.\n",
				  ptr->inode);
#else
			write_log(10, "Sync inode %ld to backend incomplete.\n",
				  ptr->inode);
#endif
			/* TODO: Revert info re last upload if upload
				fails */
		}
		super_block_update_transit(ptr->inode, FALSE, sync_error);
	} else {

		flock(fileno(local_metafptr), LOCK_UN);
		fclose(local_metafptr);
		fclose(toupload_metafptr);
		unlink(toupload_metapath);

		/* Delete those uploaded blocks if local meta is removed */
		delete_backend_blocks(progress_fd, total_blocks,
			ptr->inode, TOUPLOAD_BLOCKS);

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));
	}

	if (sync_error == TRUE) /* TODO: Something has to be done? */
		return;

	if (S_ISREG(ptr->this_mode)) {
	/* Delete old block data on backend and wait for those threads */
		delete_backend_blocks(progress_fd, total_backend_blocks, 
				ptr->inode, BACKEND_BLOCKS);
	}

	/* Upload successfully. Update FS stat in backend */
	if (upload_seq == 0)
		update_backend_stat(root_inode, size_diff, 1);
	else if (size_diff != 0)
		update_backend_stat(root_inode, size_diff, 0);

	return;

errcode_handle:
	if (backend_metafptr != NULL) {
		fclose(backend_metafptr);
		unlink(backend_metapath);
	}
	flock(fileno(local_metafptr), LOCK_UN);
	fclose(local_metafptr);
	flock(fileno(toupload_metafptr), LOCK_UN);
	fclose(toupload_metafptr);
	super_block_update_transit(ptr->inode, FALSE, TRUE);
}

int do_block_sync(ino_t this_inode, long long block_no,
#if (DEDUP_ENABLE)
			CURL_HANDLE *curl_handle, char *filename,
			char uploaded, unsigned char id_in_meta[])
#else
			long long seq, CURL_HANDLE *curl_handle, char *filename)
#endif
{
	char objname[400];
	char obj_id_str[OBJID_STRING_LENGTH];
	unsigned char old_obj_id[OBJID_LENGTH];
	unsigned char obj_id[OBJID_LENGTH];
	unsigned char start_bytes[BYTES_TO_CHECK];
	unsigned char end_bytes[BYTES_TO_CHECK];
	off_t obj_size;
	FILE *fptr, *ddt_fptr;
	int ret_val, errcode, ret;
	int ddt_fd;
	int ver_tag, result_idx;
	DDT_BTREE_NODE tree_root, result_node;
	DDT_BTREE_META ddt_meta;

#ifdef ARM_32bit_
	sprintf(curl_handle->id, "upload_blk_%lld_%lld", this_inode, block_no);
#else
	sprintf(curl_handle->id, "upload_blk_%ld_%lld", this_inode, block_no);
#endif

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
	//compute_hash(filename, hash_key);

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
	ret = search_ddt_btree(obj_id, &tree_root, ddt_fd,
			&result_node, &result_idx);

	/* Get objname - Object named by hash key */
	obj_id_to_string(obj_id, obj_id_str);
	//hash_to_string(hash_key, hash_key_str);
	sprintf(objname, "data_%s", obj_id_str);
#elif defined(ARM_32bit_)
	sprintf(objname, "data_%lld_%lld_%lld", this_inode, block_no, seq);
	write_log(10, "Debug datasync: objname %s, inode %lld, block %lld,"
		" seq %lld\n", objname, this_inode, block_no, seq);
	/* Force to upload */
	ret = 1;
#else
	sprintf(objname, "data_%ld_%lld_%lld", this_inode, block_no, seq);
	write_log(10, "Debug datasync: objname %s, inode %ld, block %lld,"
		" seq %lld\n", objname, this_inode, block_no, seq);
	/* Force to upload */
	ret = 1;
#endif

	if (ret == 0) {
		/* Find a same object in cloud
		 * Just increase the refcount of the origin block
		 */
		write_log(10, "Debug datasync: find same obj %s - Aborted to upload",
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

	} else {
		write_log(10, "Debug datasync: start to sync obj %s", objname);

		unsigned char *key = NULL;
		unsigned char *data = NULL;

#ifdef ENCRYPT_ENABLE
		key = get_key();
#endif

		FILE *new_fptr =
		    transform_fd(fptr, key, &data, ENCRYPT_ENABLE, COMPRESS_ENABLE);
		ret_val = hcfs_put_object(new_fptr, objname, curl_handle);
		fclose(new_fptr);
		if (fptr != new_fptr)
			fclose(fptr);
		if (data != NULL)
			free(data);
		if (key != NULL)
			OPENSSL_free(key);

		/* Already retried in get object if necessary */
		if ((ret_val >= 200) && (ret_val <= 299))
			ret = 0;
		else
			ret = -EIO;

#if (DEDUP_ENABLE)
		/* Upload finished - Need to update dedup table */
		if (ret == 0) {
			insert_ddt_btree(obj_id, obj_size, &tree_root,
					ddt_fd, &ddt_meta);
		}
	}

	flock(ddt_fd, LOCK_UN);
	fclose(ddt_fptr);

	/* Since the object mapped by this block is changed, need to remove old object
	 * Sync was successful
	 */
	/*if (ret == 0 && uploaded) {
		printf("Start to delete obj %02x...%02x\n", old_obj_id[0], old_obj_id[31]);
		// Delete old object in cloud
		do_block_delete(this_inode, block_no, old_obj_id,
					curl_handle);

		printf("Delete result - %d\n", ret);
		printf("Delete obj - %02x...%02x\n", old_obj_id[0], old_obj_id[31]);
	}*/
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

#ifdef ARM_32bit_
	sprintf(objname, "meta_%lld", this_inode);
	write_log(10, "Debug datasync: objname %s, inode %lld\n", objname,
		  this_inode);
	sprintf(curl_handle->id, "upload_meta_%lld", this_inode);
#else
	sprintf(objname, "meta_%ld", this_inode);
	write_log(10, "Debug datasync: objname %s, inode %ld\n", objname,
		  this_inode);
	sprintf(curl_handle->id, "upload_meta_%ld", this_inode);
#endif
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -errcode;
	}
	unsigned char *key = get_key();
	unsigned char *data = NULL;
	FILE *new_fptr = transform_encrypt_fd(fptr, key, &data);

	fclose(fptr);
	ret_val = hcfs_put_object(new_fptr, objname, curl_handle);
	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else
		ret = -EIO;
	fclose(new_fptr);
	if (data != NULL)
		free(data);
	return ret;
}

/* TODO: use pthread_exit to pass error code here. */
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, errcode;
	int count1;
	char finish_uploading;
	BLOCK_UPLOADING_STATUS temp_block_uploading_status;

	which_curl = thread_ptr->which_curl;

	if (thread_ptr->is_block == TRUE) {
#if (DEDUP_ENABLE)
		/* Get old object id (object id on cloud) */
		ret = get_progress_info(thread_ptr->progress_fd,
			thread_ptr->blockno, &temp_block_uploading_status);
		if (ret < 0)
			goto errcode_handle;
		memcpy(thread_ptr->obj_id, /* The obj id should be read from cloud */
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
	return;

errcode_handle:
	UNLINK(thread_ptr->tempfilename);
	write_log(10, "Recording error in %s\n", __func__);
	_set_inode_sync_error(thread_ptr->inode);	
}

void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, count1;

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE)
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
	if (ret < 0)
		goto errcode_handle;

	return;

errcode_handle:
	write_log(10, "Recording error in %s\n", __func__);
	_set_inode_sync_error(thread_ptr->inode);	
}

int schedule_sync_meta(char *toupload_metapath, int which_curl)
{
/*	char tempfilename[400];
	char filebuf[4100];
	char topen;
	int read_size;
	int count, ret, errcode;
	size_t ret_size;
	FILE *fptr;


	topen = FALSE;
#ifdef ARM_32bit_
	sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%lld.tmp",
		upload_ctl.upload_threads[which_curl].inode);
#else
	sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%ld.tmp",
		upload_ctl.upload_threads[which_curl].inode);
#endif

*/	/* Find a appropriate copied-meta name */
/*	count = 0;
	while (TRUE) {
		ret = access(tempfilename, F_OK);
		if (ret == 0) {
			count++;
#ifdef ARM_32bit_
			sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%lld.%d",
				upload_ctl.upload_threads[which_curl].inode,
				count);
#else
			sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%ld.%d",
				upload_ctl.upload_threads[which_curl].inode,
				count);
#endif
		} else {
			errcode = errno;
			break;
		}
	}

	if (errcode != ENOENT) {

		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	fptr = fopen(tempfilename, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	topen = TRUE;
*/
	/* Copy meta file */
/*	FSEEK(metafptr, 0, SEEK_SET);
	while (!feof(metafptr)) {
		FREAD(filebuf, 1, 4096, metafptr);
		read_size = ret_size;
		if (read_size > 0) {
			FWRITE(filebuf, 1, read_size, fptr);
		} else {
			break;
		}
	}
	fclose(fptr);
*/
	strcpy(upload_ctl.upload_threads[which_curl].tempfilename,
							toupload_metapath);
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
		       (void *)&con_object_sync,
		       (void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;

	return 0;
/*
errcode_handle:
	sem_wait(&(upload_ctl.upload_op_sem));
	upload_ctl.threads_in_use[which_curl] = FALSE;
	upload_ctl.threads_created[which_curl] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_op_sem));
	sem_post(&(upload_ctl.upload_queue_sem));

//	if (topen == TRUE)
//		fclose(fptr);
	return errcode;*/
}

int dispatch_upload_block(int which_curl)
{
	char thisblockpath[400];
	char toupload_blockpath[400];
	char filebuf[4100];
	int read_size;
	int count, ret, errcode;
	size_t ret_size;
	FILE *fptr, *blockfptr;
	UPLOAD_THREAD_TYPE *upload_ptr;
	char bopen, topen;

	bopen = FALSE;
	topen = FALSE;

	upload_ptr = &(upload_ctl.upload_threads[which_curl]);
/*
#ifdef ARM_32bit_
	sprintf(tempfilename, "/dev/shm/hcfs_sync_block_%lld_%lld.tmp",
		upload_ptr->inode, upload_ptr->blockno);
#else
	sprintf(tempfilename, "/dev/shm/hcfs_sync_block_%ld_%lld.tmp",
		upload_ptr->inode, upload_ptr->blockno);
#endif
*/
	/* Find an appropriate dispatch-name */
/*	count = 0;
	while (TRUE) {
		ret = access(tempfilename, F_OK);
		if (ret == 0) {
			count++;
#ifdef ARM_32bit_
			sprintf(tempfilename,
				"/dev/shm/hcfs_sync_block_%lld_%lld.%d",
				upload_ptr->inode, upload_ptr->blockno, count);
#else
			sprintf(tempfilename,
				"/dev/shm/hcfs_sync_block_%ld_%lld.%d",
				upload_ptr->inode, upload_ptr->blockno, count);
#endif
		} else {
			errcode = errno;
			break;
		}
	}

	if (errcode != ENOENT) {
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
*/
	/* Open source block (origin block in blockpath) */
	ret = fetch_block_path(thisblockpath, upload_ptr->inode,
			       upload_ptr->blockno);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	ret = fetch_toupload_block_path(toupload_blockpath,
		upload_ptr->inode, upload_ptr->blockno, 0);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	ret = check_and_copy_file(thisblockpath, toupload_blockpath);
	if (ret < 0) {
		if (ret != -EEXIST) {
			errcode = ret;
			goto errcode_handle;
		}
	}
/*
	blockfptr = fopen(thisblockpath, "r");
	if (blockfptr == NULL) {
		errcode = errno;
		if (errcode == ENOENT) {*/
			/* Block deleted already, log and skip */
/*
			write_log(10, "Block file %s gone. Perhaps deleted.\n",
				  thisblockpath);
			errcode = 0;
			goto errcode_handle;
		}
		write_log(0, "Open error in %s. Code %d, %s\n", __func__,
			  errcode, strerror(errcode));
		write_log(10, "Debug path %s\n", thisblockpath);
		errcode = -errcode;
		goto errcode_handle;
	}

	bopen = TRUE;

	flock(fileno(blockfptr), LOCK_EX);*/
	/* Open target block and prepare to copy */
/*	fptr = fopen(tempfilename, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "Open error in %s. Code %d, %s\n", __func__,
			  errcode, strerror(errcode));
		write_log(10, "Debug path %s\n", tempfilename);
		write_log(10, "Double check %d\n", access(tempfilename, F_OK));
		errcode = -errcode;
		goto errcode_handle;
	}
	topen = TRUE;*/
	/* Copy block */
/*	while (!feof(blockfptr)) {
		FREAD(filebuf, 1, 4096, blockfptr);
		read_size = ret_size;
		if (read_size > 0) {
			FWRITE(filebuf, 1, read_size, fptr);
		} else {
			break;
		}
	}
	flock(fileno(blockfptr), LOCK_UN);
	fclose(blockfptr);
	fclose(fptr);
*/
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
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_op_sem));
	sem_post(&(upload_ctl.upload_queue_sem));
/*	if (bopen == TRUE)
		fclose(blockfptr);
	if (topen == TRUE)
		fclose(fptr);
		*/
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

	ret = -1;

	for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
		if (sync_ctl.threads_in_use[count] == 0) {
			progress_fd = open_progress_info(this_inode);
			if (progress_fd < 0)
				break;
			ret = tag_status_on_fuse(this_inode, TRUE, progress_fd);
			if (ret < 0) {
				write_log(0, "Error on tagging inode %lld as "
					"UPLOADING.\n", this_inode);
				close_progress_info(progress_fd, this_inode);
				ret = -1;
				break;
			}
			sync_ctl.threads_in_use[count] = this_inode;
			sync_ctl.threads_created[count] = FALSE;
			sync_ctl.threads_error[count] = FALSE;
			sync_ctl.progress_fd[count] = progress_fd;
			sync_ctl.is_revert[count] = FALSE;
			sync_threads[count].inode = this_inode;
			sync_threads[count].this_mode = this_mode;
			sync_threads[count].progress_fd = progress_fd;
			sync_threads[count].is_revert = FALSE;

#ifdef ARM_32bit_
			write_log(10, "Before syncing: inode %lld, mode %d\n",
				  sync_threads[count].inode,
				  sync_threads[count].this_mode);
#else
			write_log(10, "Before syncing: inode %ld, mode %d\n",
				  sync_threads[count].inode,
				  sync_threads[count].this_mode);
#endif
			pthread_create(&(sync_ctl.inode_sync_thread[count]),
				       NULL, (void *)&sync_single_inode,
				       (void *)&(sync_threads[count]));
			sync_ctl.threads_created[count] = TRUE;
			sync_ctl.total_active_sync_threads++;
			ret = count;
			break;
		}
	}

	return ret;
}

void upload_loop(void)
{
	ino_t ino_sync, ino_check;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
	SUPER_BLOCK_ENTRY tempentry;
	int count, sleep_count;
	char in_sync;
	int ret_val, ret;
	char do_something;
	char is_start_check;

	init_upload_control();
	init_sync_control();
	init_sync_stat_control();
	is_start_check = TRUE;

	write_log(2, "Start upload loop\n");

	while (hcfs_system->system_going_down == FALSE) {
		if (is_start_check) {
			for (sleep_count = 0; sleep_count < 30; sleep_count++) {
				/*Sleep for a while if we are not really
					in a hurry*/
				if (hcfs_system->systemdata.cache_size <
				    CACHE_SOFT_LIMIT)
					sleep(1);
				else
					break;
			}

			ino_check = 0;
			do_something = FALSE;
		}

		is_start_check = FALSE;

		/* Get first dirty inode or next inode */
		sem_wait(&(sync_ctl.sync_queue_sem));
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
#ifdef ARM_32bit_
		write_log(10, "Inode to sync is %lld\n", ino_sync);
#else
		write_log(10, "Inode to sync is %ld\n", ino_sync);
#endif
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
				do_something = TRUE;

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
					do_something = FALSE;
					sem_post(&(sync_ctl.sync_op_sem));
					sem_post(&(sync_ctl.sync_queue_sem));
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
		if (ino_check == 0) {
			if (do_something == FALSE)
				sleep(5);
			is_start_check = TRUE;
		}
	}

	pthread_join(upload_ctl.upload_handler_thread, NULL);
	pthread_join(sync_ctl.sync_handler_thread, NULL);
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
	char fname[METAPATHLEN];
	char objname[METAPATHLEN];
	FILE *fptr;
	long long system_size, num_inodes;
	char is_fopen;
	size_t ret_size;

	write_log(10, "Debug: entering update backend stat\n");
	write_log(10, "Debug: root %ld change %lld bytes and %lld inodes on "
		"backend\n", root_inode, system_size_delta, num_inodes_delta);

	is_fopen = FALSE;
	sem_wait(&(sync_stat_ctl.stat_op_sem));

#ifdef ARM_32bit_
	snprintf(fname, METAPATHLEN - 1, "%s/FS_sync/FSstat%lld", METAPATH,
		 root_inode);
	snprintf(objname, METAPATHLEN - 1, "FSstat%lld", root_inode);
#else
	snprintf(fname, METAPATHLEN - 1, "%s/FS_sync/FSstat%ld", METAPATH,
		 root_inode);
	snprintf(objname, METAPATHLEN - 1, "FSstat%ld", root_inode);
#endif

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
		is_fopen = TRUE;
		ret = hcfs_get_object(fptr, objname, &(sync_stat_ctl.statcurl));
		if ((ret >= 200) && (ret <= 299)) {
			ret = 0;
			errcode = 0;
		} else if (ret != 404) {
			errcode = -EIO;
			goto errcode_handle;
		} else {
			/* Not found, init a new one */
			write_log(10, "Debug update stat: nothing stored\n");
			fseek(fptr, 0, SEEK_SET);
			system_size = 0;
			num_inodes = 0;
			fwrite(&system_size, sizeof(long long), 1, fptr);
			fwrite(&num_inodes, sizeof(long long), 1, fptr);
		}
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
	FSEEK(fptr, 0, SEEK_SET);
	ret = hcfs_put_object(fptr, objname, &(sync_stat_ctl.statcurl));
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		goto errcode_handle;
	}

	sem_post(&(sync_stat_ctl.stat_op_sem));
	fclose(fptr);
	is_fopen = FALSE;

	return 0;

errcode_handle:
	sem_post(&(sync_stat_ctl.stat_op_sem));
	if (is_fopen == TRUE)
		fclose(fptr);
	return errcode;
}
