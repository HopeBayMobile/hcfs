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
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/un.h>

#include "hcfs_clouddelete.h"
#include "params.h"
#include "global.h"
#include "super_block.h"
#include "fuseop.h"
#include "logger.h"
#include "macro.h"
#include "metaops.h"
#include "utils.h"
#include "atomic_tocloud.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

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
			tag_ret = tag_status_on_fuse(inode, FALSE, 0);
			if (tag_ret < 0) {
				write_log(0, "Fail to tag inode %lld as "
					"NOT_UPLOADING in %s\n",
					inode, __func__);
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
	FILE *metafptr;
	char thismetapath[METAPATHLEN];
	char blockpath[400];
	ino_t this_inode;
	off_t page_filepos;
	long long e_index;
	long long blockno;
	BLOCK_ENTRY_PAGE temppage;
	char is_delete;
	BLOCK_ENTRY *tmp_entry;
	size_t tmp_size, ret_size;
	DELETE_THREAD_TYPE *tmp_del;
	char need_delete_object;

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
		} else {
			/* Thread is busy. Wait some more */
			return ret;
		}
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
			return 0;  /* Error already marked */
		}
	}
	sem_post(&(sync_ctl.sync_op_sem));

	/* Terminate directly when delete old data on backend */
	if (upload_ctl.upload_threads[index].is_backend_delete == TRUE) {
		sem_wait(&(sync_ctl.sync_op_sem));
		upload_ctl.threads_in_use[index] = FALSE;
		upload_ctl.threads_created[index] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(sync_ctl.sync_op_sem));

		sem_post(&(upload_ctl.upload_queue_sem));
		return 0;
	}

	this_inode = upload_ctl.upload_threads[index].inode;
	is_delete = upload_ctl.upload_threads[index].is_delete;
	page_filepos = upload_ctl.upload_threads[index].page_filepos;
	e_index = upload_ctl.upload_threads[index].page_entry_index;
	blockno = upload_ctl.upload_threads[index].blockno;
	ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;

	need_delete_object = FALSE;

	/* Perhaps the file is deleted already. If not, modify the block
	  status in file meta. */
	if (access(thismetapath, F_OK) == 0) {
		metafptr = fopen(thismetapath, "r+");
		if (metafptr == NULL) {
			errcode = errno;
			if (errcode != ENOENT) {
				write_log(0, "IO error in %s. Code %d, %s\n",
					__func__, errcode,
					strerror(errcode));
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
					ret = fetch_block_path(blockpath,
						this_inode, blockno);
					if (ret < 0) {
						errcode = ret;
						goto errcode_handle;
					}
					SETXATTR(blockpath, "user.dirty",
							"F", 1, 0);

					FSEEK(metafptr, page_filepos, SEEK_SET);
					FWRITE(&temppage, tmp_size, 1,
								metafptr);
					//write_log(10, "Debug: block is set as ST_BOTH\n");
				} else {
					if ((tmp_entry->status == ST_TODELETE)
						&& (is_delete == TRUE)) {
						tmp_entry->status = ST_NONE;
						tmp_entry->uploaded = FALSE;
						FSEEK(metafptr, page_filepos,
								SEEK_SET);
						FWRITE(&temppage, tmp_size,
								1, metafptr);
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
	if ((access(thismetapath, F_OK) == -1) || (need_delete_object)) {
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

				delete_ctl.total_active_delete_threads++;
				which_curl = count2;
				break;
			}
		}
		sem_post(&(delete_ctl.delete_op_sem));
		pthread_create(&(delete_ctl.threads_no[which_curl]), NULL,
			(void *)&con_object_dsync,
			(void *)&(delete_ctl.delete_threads[which_curl]));

		delete_ctl.threads_created[which_curl] = TRUE;
	}

	/* Finally reclaim the uploaded-thread. */
	upload_ctl.threads_in_use[index] = FALSE;
	upload_ctl.threads_created[index] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_queue_sem));

	return 0;

errcode_handle:
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
			sem_wait(&(sync_ctl.sync_op_sem));
			for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY;
				count1++) {
				if (sync_ctl.threads_in_use[count1] ==
					upload_ctl.upload_threads[count].inode)
					break;
			}
			write_log(10, "Recording error in %s\n", __func__);
			if (count1 < MAX_SYNC_CONCURRENCY)
				sync_ctl.threads_error[count1] = TRUE;
			sem_post(&(sync_ctl.sync_op_sem));

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
	memset(&upload_curl_handles,
			0, sizeof(CURL_HANDLE) * MAX_UPLOAD_CONCURRENCY);

	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++) {
		snprintf(upload_curl_handles[count].id, 255,
				"upload_thread_%d", count);
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

	FS_stat_path = (char *) malloc(METAPATHLEN);
	fname = (char *) malloc(METAPATHLEN);
	snprintf(FS_stat_path, METAPATHLEN - 1, "%s/FS_sync", METAPATH);
	if (access(FS_stat_path, F_OK) == -1) {
		MKDIR(FS_stat_path, 0700);
	} else {
		dirp = opendir(FS_stat_path);
		if (dirp == NULL) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
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
	snprintf(sync_stat_ctl.statcurl.id, 255,
				"sync_stat_ctl");

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

int download_meta_from_backend(ino_t inode, const char *download_metapath,
	FILE **backend_fptr)
{
	char backend_meta_name[500];
	int ret, errcode;

	fetch_backend_meta_name(inode, backend_meta_name);

	*backend_fptr = fopen(download_metapath, "w+");
	if (*backend_fptr == NULL) {
		write_log(0, "Error: Fail to open file in %s\n", __func__);
		return -1;
	}
	setbuf(*backend_fptr, NULL);

	sem_wait(&(sync_stat_ctl.stat_op_sem));

#ifdef ENCRYPT_ENABLE
	char  *get_fptr_data = NULL;
	size_t len = 0;
	FILE *get_fptr = open_memstream(&get_fptr_data, &len);

	ret = hcfs_get_object(get_fptr, backend_meta_name,
		&(sync_stat_ctl.statcurl));
#else
	ret = hcfs_get_object(*backend_fptr, backend_meta_name,
		&(sync_stat_ctl.statcurl));
#endif
#ifdef ENCRYPT_ENABLE
	fclose(get_fptr);
	unsigned char *key = get_key();
	decrypt_to_fd(*backend_fptr, key, get_fptr_data, len);
	free(get_fptr_data);
	free(key);
#endif
	sem_post(&(sync_stat_ctl.stat_op_sem));

	if ((ret >= 200) && (ret <= 299)) {
		errcode = 0;
		write_log(10, "Debug: Download meta %ld from backend\n", inode);
	} else if (ret != 404) {
		errcode = -EIO;
		fclose(*backend_fptr);
		unlink(download_metapath);
		*backend_fptr = NULL;
	} else {
		errcode = 0;
		fclose(*backend_fptr);
		unlink(download_metapath);
		*backend_fptr = NULL;
		write_log(10, "Debug: meta %ld does not exist on cloud\n",
			inode);
	}

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
	long long total_blocks;
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
	long long size_last_upload;
	long long size_diff;
	int progress_fd;
	char first_upload, is_local_meta_deleted;

	progress_fd = ptr->progress_fd;
	this_inode = ptr->inode;
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
	ret = check_and_copy_file(local_metapath, toupload_metapath);
	if (ret < 0) {
		if (ret != -EEXIST) {
			super_block_update_transit(ptr->inode, FALSE, TRUE);
			return;
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
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			super_block_update_transit(ptr->inode, FALSE, TRUE);
		}
		/* If meta file is gone, the inode is deleted and we don't need
		to sync this object anymore. */
		fclose(toupload_metafptr);
		unlink(toupload_metapath);
		return;
	}
	setbuf(local_metafptr, NULL);

	/* Download backend meta if it is regfile */
	if (S_ISREG(ptr->this_mode)) {
		first_upload = FALSE;
		backend_metafptr = NULL;
		sprintf(backend_metapath, "upload_bullpen/backend_meta_%ld",
				this_inode);
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
	}

	flock(fileno(toupload_metafptr), LOCK_EX);
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
			fremovexattr(fileno(toupload_metafptr),
				"user.trunc_size");
		}

		/* Compute number of blocks */
		if (toupload_trunc_block_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((toupload_trunc_block_size - 1)
				/ MAX_BLOCK_SIZE) + 1;

		/* Init progress info based on # of blocks */
		init_progress_info(progress_fd, total_blocks, backend_metafptr);
		if (backend_metafptr != NULL) {
			fclose(backend_metafptr);
			backend_metafptr = NULL;
			UNLINK(backend_metapath);
		}

		/* Begin to upload blocks */
		current_page = -1;
		is_local_meta_deleted = FALSE;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			//flock(fileno(metafptr), LOCK_EX);

			/*Perhaps the file is deleted already*/
			/*if (access(thismetapath, F_OK) < 0) {
				flock(fileno(metafptr), LOCK_UN);
				break;
			}*/

			e_index = block_count % BLK_INCREMENTS;
			which_page = block_count / BLK_INCREMENTS;

			if (current_page != which_page) {
				page_pos = seek_page2(&tempfilemeta,
					toupload_metafptr, which_page, 0);
				if (page_pos <= 0) {
					block_count += (BLK_INCREMENTS - 1);
					//flock(fileno(metafptr), LOCK_UN);
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
					flock(fileno(local_metafptr), LOCK_UN);
					is_local_meta_deleted = TRUE;
				}
			}

			FSEEK_ADHOC_SYNC_LOOP(local_metafptr, page_pos,
				SEEK_SET, TRUE);
			FREAD_ADHOC_SYNC_LOOP(&local_temppage,
				sizeof(BLOCK_ENTRY_PAGE), 1,
				local_metafptr, TRUE);

			tmp_entry = &(local_temppage.block_entries[e_index]);
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
				/* Unlock local meta */
				flock(fileno(local_metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
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
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
						0, page_pos,
						e_index, progress_fd,
						FALSE);
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

			/*** Case 3: Local block is deleted. Delete backend
			   block data, too. ***/
			/* TODO: delete backend data here or
			   after meta being uploaded? */
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
				/* TODO: Maybe should also first copy
					block out first*/
			} else {
				write_log(10, "Debug: block_%lld is %d\n", block_count, toupload_block_status);
				flock(fileno(local_metafptr), LOCK_UN);
				set_progress_info(progress_fd, block_count,
					TRUE, 0, 0);
			}
		}
		/* ---End of syncing blocks loop--- */

		/* Block sync should be done here. Check if all upload
		threads for this inode has returned before starting meta sync*/
		_busy_wait_all_specified_upload_threads(ptr->inode);

	}

	/*Check if metafile still exists. If not, forget the meta upload*/
	/*if (access(thismetapath, F_OK) < 0)
		return;*/

	/* Abort sync to cloud if error occured */
	if (sync_error == TRUE) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		flock(fileno(toupload_metafptr), LOCK_UN);
		fclose(toupload_metafptr);
		fclose(local_metafptr);
		return;
	}

	sem_wait(&(upload_ctl.upload_queue_sem));
	sem_wait(&(upload_ctl.upload_op_sem));
	which_curl = _select_upload_thread(FALSE, FALSE, ptr->inode,
		0, 0, 0, 0, progress_fd, FALSE);

	sem_post(&(upload_ctl.upload_op_sem));

	flock(fileno(local_metafptr), LOCK_EX);
	/*Check if metafile still exists. If not, forget the meta upload*/
	if (!access(local_metapath, F_OK)) {
		increment_upload_seq(local_metafptr, &upload_seq);

		/* Get root_inode from meta */
		if (S_ISREG(ptr->this_mode)) {
			ret_ssize = fgetxattr(fileno(local_metafptr),
				"user.size_last_upload", &size_last_upload,
				sizeof(long long));
			if (ret_ssize < 0) {
				errcode = errno;
				size_last_upload = 0;
				if (errcode != ENOATTR)
					write_log(0, "Error: getxattr failed "
						"in %s. Code %d\n", __func__,
						errcode);
			}

			size_diff = toupload_size - size_last_upload;

			fsetxattr(fileno(local_metafptr),
				"user.size_last_upload",
				&toupload_size, sizeof(long long), 0);
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
		flock(fileno(toupload_metafptr), LOCK_UN);
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
			sem_wait(&(sync_ctl.sync_op_sem));
			for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY;
				count1++) {
				if (sync_ctl.threads_in_use[count1] ==
					ptr->inode)
					break;
			}
			if (count1 < MAX_SYNC_CONCURRENCY)
				sync_error = sync_ctl.threads_error[count1];
			sem_post(&(sync_ctl.sync_op_sem));
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
		} else {
			/* Upload successfully. Update FS stat in backend */
			if (upload_seq == 0)
				update_backend_stat(root_inode, size_diff, 1);
			else if (size_diff != 0)
				update_backend_stat(root_inode, size_diff, 0);
		}
		super_block_update_transit(ptr->inode, FALSE, sync_error);
	} else {
		flock(fileno(local_metafptr), LOCK_UN);
		fclose(local_metafptr);
		flock(fileno(toupload_metafptr), LOCK_UN);
		fclose(toupload_metafptr);

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));
	}

	if (!S_ISREG(ptr->this_mode))
		return;

	/* Delete old block data on backend */
	BLOCK_UPLOADING_STATUS block_info;
	char finish_uploading;
	long long to_upload_seq;
	long long backend_seq;

	for (block_count = 0; block_count < total_blocks; block_count++) {
		ret = get_progress_info_nonlock(progress_fd, block_count,
			&block_info);
		if (ret == 0) /* Remaining are empty blocks */
			break;
		else if (ret < 0) /* error */
			continue;

		finish_uploading = block_info.finish_uploading;
		to_upload_seq = block_info.to_upload_seq;
		backend_seq = block_info.backend_seq;

		if (finish_uploading == FALSE) {
			//write_log(10, "Debug: block_%ld_%lld did not finish?\n",
			//	ptr->inode, block_count);
			continue;
		}

		if (backend_seq == 0) {
			write_log(10, "Debug: block_%ld_%lld is uploaded first time\n",
					ptr->inode, block_count);
			continue;
		}

		if (to_upload_seq == backend_seq) {
			write_log(10, "Debug: block_%ld_%lld has the same seq?\n",
				ptr->inode, block_count);
			continue;
		}

		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
		which_curl = _select_upload_thread(TRUE, FALSE,
			ptr->inode, block_count, backend_seq,
			0, 0, progress_fd, TRUE);
		sem_post(&(upload_ctl.upload_op_sem));
		dispatch_delete_block(which_curl);
	}

	/* Wait for those threads */
	_busy_wait_all_specified_upload_threads(ptr->inode);

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

int do_block_sync(ino_t this_inode, long long block_no, long long seq,
			CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	FILE *fptr;
	int ret_val, errcode, ret;

#ifdef ARM_32bit_
	sprintf(objname, "data_%lld_%lld_%lld", this_inode, block_no, seq);
	write_log(10, "Debug datasync: objname %s, inode %lld, block %lld,"
		" seq %lld\n", objname, this_inode, block_no, seq);
	sprintf(curl_handle->id, "upload_blk_%lld_%lld", this_inode, block_no);
#else
	sprintf(objname, "data_%ld_%lld_%lld", this_inode, block_no, seq);
	write_log(10, "Debug datasync: objname %s, inode %ld, block %lld,"
		" seq %lld\n", objname, this_inode, block_no, seq);
	sprintf(curl_handle->id, "upload_blk_%ld_%lld", this_inode, block_no);
#endif
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}
#ifdef ENCRYPT_ENABLE
	/* write_log(10, "start to encrypt...\n"); */
	unsigned char* key = get_key();
	unsigned char* data = NULL;
	FILE* new_fptr = transform_encrypt_fd(fptr, key, &data);

	fclose(fptr);
	ret_val = hcfs_put_object(new_fptr, objname, curl_handle);
	fclose(new_fptr);
#else
	ret_val = hcfs_put_object(fptr, objname, curl_handle);
	fclose(fptr);

#endif

	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else
		ret = -EIO;
#ifdef ENCRYPT_ENABLE
	free(key);
	if (data != NULL) {
		free(data);
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
	write_log(10,
		"Debug datasync: objname %s, inode %lld\n",
		objname, this_inode);
	sprintf(curl_handle->id, "upload_meta_%lld", this_inode);
#else
	sprintf(objname, "meta_%ld", this_inode);
	write_log(10,
		"Debug datasync: objname %s, inode %ld\n",
		objname, this_inode);
	sprintf(curl_handle->id, "upload_meta_%ld", this_inode);
#endif
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}
	unsigned char* key = get_key();
	unsigned char* data = NULL;
	FILE* new_fptr = transform_encrypt_fd(fptr, key, &data);
	fclose(fptr);
	ret_val = hcfs_put_object(new_fptr, objname, curl_handle);
	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else
		ret = -EIO;
	fclose(new_fptr);
	if(data != NULL)
		free(data);
	return ret;
}

/* TODO: use pthread_exit to pass error code here. */
void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, errcode;
	int count1;
	char finish_uploading;

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE) {
		ret = do_block_sync(thread_ptr->inode, thread_ptr->blockno,
			thread_ptr->seq, &(upload_curl_handles[which_curl]),
			thread_ptr->tempfilename);

		/* This block finished uploading */
		if (ret >= 0) {
			finish_uploading = TRUE;
			set_progress_info(thread_ptr->progress_fd,
				thread_ptr->blockno, finish_uploading,
				0, 0);
		}
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
	write_log(10, "Recording error in %s\n", __func__);
	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] == thread_ptr->inode)
			break;
	}
	if (count1 < MAX_SYNC_CONCURRENCY)
		sync_ctl.threads_error[count1] = TRUE;
	sem_post(&(sync_ctl.sync_op_sem));
}

void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl, ret, count1;

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE)
		ret = do_block_delete(thread_ptr->inode, thread_ptr->blockno,
					thread_ptr->seq,
					&(upload_curl_handles[which_curl]));
	if (ret < 0)
		goto errcode_handle;

	return;

errcode_handle:
	write_log(10, "Recording error in %s\n", __func__);
	sem_wait(&(sync_ctl.sync_op_sem));
	for (count1 = 0; count1 < MAX_SYNC_CONCURRENCY; count1++) {
		if (sync_ctl.threads_in_use[count1] == thread_ptr->inode)
			break;
	}
	if (count1 < MAX_SYNC_CONCURRENCY)
		sync_ctl.threads_error[count1] = TRUE;
	sem_post(&(sync_ctl.sync_op_sem));
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
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
	errcode = -errcode;
		goto errcode_handle;
	}

	fptr = fopen(tempfilename, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
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
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
*/
	/* Open source block (origin block in blockpath) */
	ret = fetch_block_path(thisblockpath,
			upload_ptr->inode, upload_ptr->blockno);
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
/*			write_log(10, "Block file %s gone. Perhaps deleted.\n",
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
		write_log(10, "Double check %d\n", access(tempfilename,
			F_OK));
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
			sync_threads[count].inode = this_inode;
			sync_threads[count].this_mode = this_mode;
			sync_threads[count].progress_fd = progress_fd;

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
			for (sleep_count = 0; sleep_count < 30;
							sleep_count++) {
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
					ret = write_super_block_entry(ino_sync,
							&tempentry);
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
		write_log(0, "IO error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
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
