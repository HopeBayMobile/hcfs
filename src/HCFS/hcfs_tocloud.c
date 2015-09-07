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

#include "hcfs_clouddelete.h"
#include "params.h"
#include "global.h"
#include "super_block.h"
#include "fuseop.h"
#include "logger.h"
#include "macro.h"
#include "metaops.h"
#include "utils.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

/* Don't need to collect return code for the per-inode sync thread, as
the error handling for syncing this inode will be handled in
sync_single_inode. */
static inline void _sync_terminate_thread(int index)
{
	int ret;

	if ((sync_ctl.threads_in_use[index] != 0) &&
			(sync_ctl.threads_created[index] == TRUE)) {
		ret = pthread_tryjoin_np(sync_ctl.inode_sync_thread[index],
					NULL);
		if (ret == 0) {
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

static inline int _select_upload_thread(char is_block, char is_delete,
				ino_t this_inode, long long block_count,
					off_t page_pos, long long e_index)
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
			upload_ctl.upload_threads[count].page_filepos =
								page_pos;
			upload_ctl.upload_threads[count].page_entry_index =
									e_index;
			upload_ctl.upload_threads[count].which_curl = count;

			upload_ctl.total_active_upload_threads++;
			which_curl = count;
			break;
		 }
	 }
	return which_curl;
}

void sync_single_inode(SYNC_THREAD_TYPE *ptr)
{
	char thismetapath[METAPATHLEN];
	ino_t this_inode;
	FILE *metafptr;
	struct stat tempfilestat;
	FILE_META_TYPE tempfilemeta;
	SYMLINK_META_TYPE tempsymmeta;
	DIR_META_TYPE tempdirmeta;
	BLOCK_ENTRY_PAGE temppage;
	int which_curl;
	long long page_pos, e_index, which_page, current_page;
	long long total_blocks;
	long long count, block_count;
	unsigned char block_status;
	char upload_done;
	int ret, errcode;
	off_t tmp_size;
	struct timespec time_to_sleep;
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

	sync_error = FALSE;
	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	this_inode = ptr->inode;

	ret = fetch_meta_path(thismetapath, this_inode);

#ifdef ARM_32bit_
	write_log(10, "Sync inode %lld, mode %d\n", ptr->inode, ptr->this_mode);
#else
	write_log(10, "Sync inode %ld, mode %d\n", ptr->inode, ptr->this_mode);
#endif
	if (ret < 0) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		return;
	}

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			super_block_update_transit(ptr->inode, FALSE, TRUE);
		}
		/* If meta file is gone, the inode is deleted and we don't need
		to sync this object anymore. */
		return;
	}

	setbuf(metafptr, NULL);

	/* Upload block if mode is regular file */
	if (S_ISREG(ptr->this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		FREAD(&tempfilestat, sizeof(struct stat), 1, metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1, metafptr);

		tmp_size = tempfilestat.st_size;
		size_last_upload = tempfilemeta.size_last_upload;
		size_diff = tmp_size - size_last_upload;
		root_inode = tempfilemeta.root_inode;
		upload_seq = tempfilemeta.upload_seq;

		/* Check if need to sync past the current size */
		ret_ssize = fgetxattr(fileno(metafptr), "user.trunc_size",
				&temp_trunc_size, sizeof(long long));

		if ((ret_ssize >= 0) && (tmp_size < temp_trunc_size)) {
			tmp_size = temp_trunc_size;
			fremovexattr(fileno(metafptr), "user.trunc_size");
		}

		if (tmp_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((tmp_size - 1) / MAX_BLOCK_SIZE) + 1;

		flock(fileno(metafptr), LOCK_UN);

		current_page = -1;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			flock(fileno(metafptr), LOCK_EX);

			/*Perhaps the file is deleted already*/
			if (access(thismetapath, F_OK) < 0) {
				flock(fileno(metafptr), LOCK_UN);
				break;
			}

			e_index = block_count % BLK_INCREMENTS;
			which_page = block_count / BLK_INCREMENTS;

			if (current_page != which_page) {
				page_pos = seek_page2(&tempfilemeta, metafptr,
					which_page, 0);
				if (page_pos <= 0) {
					block_count += BLK_INCREMENTS - 1;
					flock(fileno(metafptr), LOCK_UN);
					continue;
				}
				current_page = which_page;
			}

			/*TODO: error handling here if cannot read correctly*/

			ret = fseek(metafptr, page_pos, SEEK_SET);

			if (ret < 0) {
				errcode = errno;
				write_log(0, "IO error in %s. Code %d, %s\n",
					__func__, errcode, strerror(errcode));
				sync_error = TRUE;
				flock(fileno(metafptr), LOCK_UN);
				break;
			}

			ret = fread(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
								metafptr);
			if (ret < 1) {
				errcode = ferror(metafptr);
				write_log(0, "IO error in %s.\n",
					__func__);
				if (errcode != 0)
					write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				sync_error = TRUE;
				flock(fileno(metafptr), LOCK_UN);
				break;
			}
			tmp_entry = &(temppage.block_entries[e_index]);
			block_status = tmp_entry->status;

			if (((block_status == ST_LDISK) ||
				(block_status == ST_LtoC)) &&
					(block_count < total_blocks)) {
				if (block_status == ST_LDISK) {
					tmp_entry->status = ST_LtoC;
					ret = fseek(metafptr, page_pos,
						SEEK_SET);
					if (ret < 0) {
						errcode = errno;
						write_log(0,
							"IO error in %s.\n",
							__func__);
						write_log(0,
							"Code %d, %s\n",
							errcode,
							strerror(errcode));
						sync_error = TRUE;
						flock(fileno(metafptr),
							LOCK_UN);
						break;
					}

					ret = fwrite(&temppage,
						sizeof(BLOCK_ENTRY_PAGE),
								1, metafptr);
					if (ret < 1) {
						errcode = ferror(metafptr);
						write_log(0,
							"IO error in %s.\n",
							__func__);
						write_log(0, "Code %d, %s\n",
							errcode,
							strerror(errcode));
						sync_error = TRUE;
						flock(fileno(metafptr),
							LOCK_UN);
						break;
					}
				}
				flock(fileno(metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
							page_pos, e_index);
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
			if (block_status == ST_TODELETE) {
				flock(fileno(metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
				which_curl = _select_upload_thread(TRUE, TRUE,
						ptr->inode, block_count,
							page_pos, e_index);
				sem_post(&(upload_ctl.upload_op_sem));
				dispatch_delete_block(which_curl);
				/* TODO: Maybe should also first copy
					block out first*/
			} else {
				flock(fileno(metafptr), LOCK_UN);
			}
		}
		/* Block sync should be done here. Check if all upload
		threads for this inode has returned before starting meta sync*/

		upload_done = FALSE;
		while (upload_done == FALSE) {
			nanosleep(&time_to_sleep, NULL);
			upload_done = TRUE;
			sem_wait(&(upload_ctl.upload_op_sem));
			for (count = 0; count < MAX_UPLOAD_CONCURRENCY;
								count++) {
				if ((upload_ctl.threads_in_use[count] == TRUE)
					&&
					(upload_ctl.upload_threads[count].inode
						== ptr->inode)) {
					upload_done = FALSE;
					break;
				}
			}
			sem_post(&(upload_ctl.upload_op_sem));
		}
	}

	/*Check if metafile still exists. If not, forget the meta upload*/
	if (access(thismetapath, F_OK) < 0)
		return;

	/* Abort sync to cloud if error occured */
	if (sync_error == TRUE) {
		super_block_update_transit(ptr->inode, FALSE, TRUE);
		return;
	}

	sem_wait(&(upload_ctl.upload_queue_sem));
	sem_wait(&(upload_ctl.upload_op_sem));
	which_curl = _select_upload_thread(FALSE, FALSE, ptr->inode, 0, 0, 0);

	sem_post(&(upload_ctl.upload_op_sem));

	flock(fileno(metafptr), LOCK_EX);
	/*Check if metafile still exists. If not, forget the meta upload*/
	if (!access(thismetapath, F_OK)) {
		FSEEK(metafptr, sizeof(struct stat), SEEK_SET);

		if (S_ISREG(ptr->this_mode)) {
			FREAD(&tempfilemeta, sizeof(FILE_META_TYPE),
				1, metafptr);
			tempfilemeta.size_last_upload = tempfilestat.st_size;
			tempfilemeta.upload_seq++;
			FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
			FWRITE(&tempfilemeta, sizeof(FILE_META_TYPE),
				1, metafptr);
		}

		if (S_ISDIR(ptr->this_mode)) {
			FREAD(&tempdirmeta, sizeof(DIR_META_TYPE),
				1, metafptr);
			root_inode = tempdirmeta.root_inode;
			upload_seq = tempdirmeta.upload_seq;
			tempdirmeta.upload_seq++;
			size_diff = 0;
			FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
			FWRITE(&tempdirmeta, sizeof(DIR_META_TYPE),
				1, metafptr);
		}

		if (S_ISLNK(ptr->this_mode)) {
			FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE),
				1, metafptr);
			root_inode = tempsymmeta.root_inode;
			upload_seq = tempsymmeta.upload_seq;
			size_diff = 0;
			tempsymmeta.upload_seq++;
			FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
			FWRITE(&tempsymmeta, sizeof(SYMLINK_META_TYPE),
				1, metafptr);
		}

		ret = schedule_sync_meta(metafptr, which_curl);
		if (ret < 0)
			sync_error = TRUE;
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);

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
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);

		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));
	}

	return;

errcode_handle:
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	super_block_update_transit(ptr->inode, FALSE, TRUE);
}

int do_block_sync(ino_t this_inode, long long block_no,
			CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	FILE *fptr;
	int ret_val, errcode, ret;

#ifdef ARM_32bit_
	sprintf(objname, "data_%lld_%lld", this_inode, block_no);
	write_log(10, "Debug datasync: objname %s, inode %lld, block %lld\n",
					objname, this_inode, block_no);
	sprintf(curl_handle->id, "upload_blk_%lld_%lld", this_inode, block_no);
#else
	sprintf(objname, "data_%ld_%lld", this_inode, block_no);
	write_log(10, "Debug datasync: objname %s, inode %ld, block %lld\n",
					objname, this_inode, block_no);
	sprintf(curl_handle->id, "upload_blk_%ld_%lld", this_inode, block_no);
#endif
	fptr = fopen(filename, "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}

	unsigned char *key = NULL;
	unsigned char *data = NULL;
#if ENCRYPT_ENABLE
	key = get_key();
#endif

	FILE *new_fptr = transform_fd(fptr, key, &data, ENCRYPT_ENABLE, COMPRESS_ENABLE);
	ret_val = hcfs_put_object(new_fptr, objname, curl_handle);
	fclose(new_fptr);
    if(fptr != new_fptr)
        fclose(fptr);
    if(data != NULL)
        free(data);
    if(key != NULL)
        OPENSSL_free(key);


	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else
		ret = -EIO;
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

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE)
		ret = do_block_sync(thread_ptr->inode, thread_ptr->blockno,
				&(upload_curl_handles[which_curl]),
						thread_ptr->tempfilename);
	else
		ret = do_meta_sync(thread_ptr->inode,
				&(upload_curl_handles[which_curl]),
						thread_ptr->tempfilename);
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

int schedule_sync_meta(FILE *metafptr, int which_curl)
{
	char tempfilename[400];
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

	/* Find a appropriate copied-meta name */
	count = 0;
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

	/* Copy meta file */
	FSEEK(metafptr, 0, SEEK_SET);
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

	strcpy(upload_ctl.upload_threads[which_curl].tempfilename,
							tempfilename);
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
			(void *)&con_object_sync,
			(void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;

	return 0;

errcode_handle:
	sem_wait(&(upload_ctl.upload_op_sem));
	upload_ctl.threads_in_use[which_curl] = FALSE;
	upload_ctl.threads_created[which_curl] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_op_sem));
	sem_post(&(upload_ctl.upload_queue_sem));

	if (topen == TRUE)
		fclose(fptr);
	return errcode;
}

int dispatch_upload_block(int which_curl)
{
	char tempfilename[400];
	char thisblockpath[400];
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

#ifdef ARM_32bit_
	sprintf(tempfilename, "/dev/shm/hcfs_sync_block_%lld_%lld.tmp",
				upload_ptr->inode, upload_ptr->blockno);
#else
	sprintf(tempfilename, "/dev/shm/hcfs_sync_block_%ld_%lld.tmp",
				upload_ptr->inode, upload_ptr->blockno);
#endif

	/* Find an appropriate dispatch-name */
	count = 0;
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

	/* Open source block (origin block in blockpath) */
	ret = fetch_block_path(thisblockpath,
			upload_ptr->inode, upload_ptr->blockno);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	blockfptr = fopen(thisblockpath, "r");
	if (blockfptr == NULL) {
		errcode = errno;
		if (errcode == ENOENT) {
			/* Block deleted already, log and skip */
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

	flock(fileno(blockfptr), LOCK_EX);
	/* Open target block and prepare to copy */
	fptr = fopen(tempfilename, "w");
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
	topen = TRUE;
	/* Copy block */
	while (!feof(blockfptr)) {
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

	strcpy(upload_ptr->tempfilename, tempfilename);
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
	if (bopen == TRUE)
		fclose(blockfptr);
	if (topen == TRUE)
		fclose(fptr);
	return errcode;
}

void dispatch_delete_block(int which_curl)
{
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
			(void *)&delete_object_sync,
			(void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;
}

static inline int _sync_mark(ino_t this_inode, mode_t this_mode,
					SYNC_THREAD_TYPE *sync_threads)
{
	int count;

	for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
		if (sync_ctl.threads_in_use[count] == 0) {
			sync_ctl.threads_in_use[count] = this_inode;
			sync_ctl.threads_created[count] = FALSE;
			sync_ctl.threads_error[count] = FALSE;
			sync_threads[count].inode = this_inode;
			sync_threads[count].this_mode = this_mode;

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
			break;
		}
	}

	return count;
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
		if (ino_check == 0)
			ino_check = sys_super_block->head.first_dirty_inode;
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
				ret_val = _sync_mark(ino_sync,
						tempentry.inode_stat.st_mode,
								sync_threads);
				do_something = TRUE;
				sem_post(&(sync_ctl.sync_op_sem));
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

	write_log(10, "Debug entering update backend stat\n");

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
