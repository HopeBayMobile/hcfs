/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_clouddelete.c
* Abstract: The c source code file for deleting meta or data from
*           backend.
*
* Revision History
* 2015/2/12~13 Jiahong added header for this file, and revising coding style.
* 2015/5/14 Jiahong changed code so that process will terminate with fuse
*           unmount.
* 2015/6/5 Jiahong added error handling.
* 2015/8/5 Jiahong added routines for updating FS statistics
*
**************************************************************************/

/*Flow for delete loop is similar to that of upload loop:
First scan the to_be_deleted linked list in super inode and then check if
the meta is in the to_delete temp dir. Open it and and start to delete the
blocks first, then delete the meta last.*/

/*before actually moving the inode from to_be_deleted to deleted, must first
check the upload threads and sync threads to find out if there are any pending
uploads. It must wait until those are cleared. It must then wait for any
additional pending meta or block deletion for this inode to finish.*/

#define _GNU_SOURCE
#include "hcfs_clouddelete.h"

#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mman.h>
#ifndef _ANDROID_ENV_
#include <attr/xattr.h>
#endif
#include <inttypes.h>

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

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE

extern SYSTEM_CONF_STRUCT system_config;

static DSYNC_THREAD_TYPE dsync_thread_info[MAX_DSYNC_CONCURRENCY];

CURL_HANDLE delete_curl_handles[MAX_DELETE_CONCURRENCY];

/* Helper function for terminating threads in deleting backend objects */
/* Dsync threads are the ones that find the objects to be deleted in
	a single filesystem object. */
/* Don't need to collect return code for the per-inode dsync thread, as
the error handling for sync-for-deletion for this inode will be handled in
dsync_single_inode. */
static inline void _dsync_terminate_thread(int index)
{
	int ret;

	if ((dsync_ctl.threads_in_use[index] != 0) &&
			((dsync_ctl.threads_finished[index] == TRUE) &&
			(dsync_ctl.threads_created[index] == TRUE))) {
		ret = pthread_join(dsync_ctl.inode_dsync_thread[index],
					NULL);
		if (ret == 0) {
			dsync_ctl.threads_in_use[index] = 0;
			dsync_ctl.threads_created[index] = FALSE;
			dsync_ctl.threads_finished[index] = FALSE;
			dsync_ctl.total_active_dsync_threads--;
			sem_post(&(dsync_ctl.dsync_queue_sem));
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
	int count;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while ((hcfs_system->system_going_down == FALSE) ||
		(dsync_ctl.total_active_dsync_threads > 0)) {
		sem_wait(&(dsync_ctl.dsync_op_sem));

		if (dsync_ctl.total_active_dsync_threads <= 0) {
			sem_post(&(dsync_ctl.dsync_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
		}
		for (count = 0; count < MAX_DSYNC_CONCURRENCY; count++)
			_dsync_terminate_thread(count);

		sem_post(&(dsync_ctl.dsync_op_sem));
		nanosleep(&time_to_sleep, NULL);
		continue;
	}
}

/* Helper function for terminating threads in deleting backend objects */
/* Delete threads are the ones that delete a single backend object. */
static inline void _delete_terminate_thread(int index)
{
	int ret;

	if (((delete_ctl.threads_in_use[index] != 0) &&
		(delete_ctl.delete_threads[index].is_block == TRUE)) &&
		((delete_ctl.threads_finished[index] == TRUE) &&
			(delete_ctl.threads_created[index] == TRUE))) {
		ret = pthread_join(delete_ctl.threads_no[index], NULL);
		if (ret == 0) {
			delete_ctl.threads_in_use[index] = FALSE;
			delete_ctl.threads_created[index] = FALSE;
			delete_ctl.threads_finished[index] = FALSE;
			delete_ctl.total_active_delete_threads--;
			sem_post(&(delete_ctl.delete_queue_sem));
		}
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
	int count;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while ((hcfs_system->system_going_down == FALSE) ||
		(delete_ctl.total_active_delete_threads > 0)) {
		sem_wait(&(delete_ctl.delete_op_sem));

		if (delete_ctl.total_active_delete_threads <= 0) {
			sem_post(&(delete_ctl.delete_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
		}
		for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
			_delete_terminate_thread(count);

		sem_post(&(delete_ctl.delete_op_sem));
		nanosleep(&time_to_sleep, NULL);
		continue;
	}
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
	memset(&(dsync_ctl.threads_in_use), 0,
					sizeof(ino_t) * MAX_DSYNC_CONCURRENCY);
	memset(&(dsync_ctl.threads_created), 0,
					sizeof(char) * MAX_DSYNC_CONCURRENCY);
	memset(&(dsync_ctl.threads_finished), 0,
					sizeof(char) * MAX_DSYNC_CONCURRENCY);
	dsync_ctl.total_active_dsync_threads = 0;

	pthread_create(&(dsync_ctl.dsync_handler_thread), NULL,
				(void *)&collect_finished_dsync_threads, NULL);
}

/* Helper for initializing curl handles for deleting backend objects */
static inline int _init_delete_handle(int index)
{
	int ret_val;

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
	int count, ret_val;

	memset(&delete_ctl, 0, sizeof(DELETE_THREAD_CONTROL));
	memset(&delete_curl_handles, 0,
			sizeof(CURL_HANDLE) * MAX_DELETE_CONCURRENCY);
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++)
		ret_val = _init_delete_handle(count);

	sem_init(&(delete_ctl.delete_op_sem), 0, 1);
	sem_init(&(delete_ctl.delete_queue_sem), 0, MAX_DELETE_CONCURRENCY);
	memset(&(delete_ctl.threads_in_use), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	memset(&(delete_ctl.threads_created), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	memset(&(delete_ctl.threads_finished), 0,
					sizeof(char) * MAX_DELETE_CONCURRENCY);
	delete_ctl.total_active_delete_threads = 0;

	pthread_create(&(delete_ctl.delete_handler_thread), NULL,
			(void *)&collect_finished_delete_threads, NULL);
}

/* Helper function for marking a delete thread as in use */
static inline int _use_delete_thread(int index, char is_blk_flag,
#if (DEDUP_ENABLE)
				ino_t this_inode, long long blockno,
				unsigned char *obj_id)
#else
				ino_t this_inode, long long blockno)
#endif
{
	if (delete_ctl.threads_in_use[index] != FALSE)
		return -1;

	delete_ctl.threads_in_use[index] = TRUE;
	delete_ctl.threads_created[index] = FALSE;
	delete_ctl.threads_finished[index] = FALSE;
	delete_ctl.delete_threads[index].is_block = is_blk_flag;
	delete_ctl.delete_threads[index].inode = this_inode;
	if (is_blk_flag == TRUE) {
#if (DEDUP_ENABLE)
		memcpy(delete_ctl.delete_threads[index].obj_id, obj_id, OBJID_LENGTH);
#endif
		delete_ctl.delete_threads[index].blockno = blockno;
	}
	delete_ctl.delete_threads[index].which_curl = index;
	delete_ctl.delete_threads[index].which_index = index;

	delete_ctl.total_active_delete_threads++;

	return 0;
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
	char thismetapath[METAPATHLEN];
	char truncpath[METAPATHLEN];
	ino_t this_inode;
	FILE *metafptr, *truncfptr;
	struct stat tempfilestat;
	FILE_META_TYPE tempfilemeta;
	DIR_META_TYPE tempdirmeta;
	SYMLINK_META_TYPE tempsymmeta;
	BLOCK_ENTRY_PAGE temppage;
	int curl_id, which_index;
	long long current_index;
	long long page_pos, which_page, current_page;
	long long count, block_count;
	long long total_blocks;
	long long ret_ssize;
	long long temp_trunc_size;
	unsigned char block_status;
	char delete_done;
	char in_sync;
	int ret_val, errcode, ret;
	size_t ret_size;
	struct timespec time_to_sleep;
	pthread_t *tmp_tn;
	DELETE_THREAD_TYPE *tmp_dt;
	off_t tmp_size;
	char mlock;
	long long system_size_change;
	long long upload_seq;
	ino_t root_inode;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	mlock = FALSE;
	this_inode = ptr->inode;
	system_size_change = 0;

	fetch_todelete_path(thismetapath, this_inode);

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		goto errcode_handle;
	}

	setbuf(metafptr, NULL);
	if (S_ISDIR(ptr->this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		mlock = TRUE;
		FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
		FREAD(&tempdirmeta, sizeof(DIR_META_TYPE), 1, metafptr);

		root_inode = tempdirmeta.root_inode;
		upload_seq = tempdirmeta.upload_seq;
		flock(fileno(metafptr), LOCK_UN);
		mlock = FALSE;
	}

	if (S_ISLNK(ptr->this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		mlock = TRUE;
		FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
		FREAD(&tempsymmeta, sizeof(SYMLINK_META_TYPE), 1, metafptr);

		root_inode = tempsymmeta.root_inode;
		upload_seq = tempsymmeta.upload_seq;
		flock(fileno(metafptr), LOCK_UN);
		mlock = FALSE;
	}

	if (S_ISREG(ptr->this_mode)) {
		flock(fileno(metafptr), LOCK_EX);
		mlock = TRUE;
		FREAD(&tempfilestat, sizeof(struct stat), 1, metafptr);
		FREAD(&tempfilemeta, sizeof(FILE_META_TYPE), 1, metafptr);

		system_size_change = tempfilemeta.size_last_upload;
		root_inode = tempfilemeta.root_inode;
		upload_seq = tempfilemeta.upload_seq;

		tmp_size = tempfilestat.st_size;

		/* Check if need to sync past the current size */
#ifdef _ANDROID_ENV_
		ret = fetch_trunc_path(truncpath, this_inode);

		truncfptr = fopen(truncpath, "r+");
		if (truncfptr != NULL) {
			setbuf(truncfptr, NULL);
			flock(fileno(truncfptr), LOCK_EX);
			FREAD(&temp_trunc_size, sizeof(long long), 1,
				truncfptr);

			if (tmp_size < temp_trunc_size) {
				tmp_size = temp_trunc_size;
				UNLINK(truncpath);
			}
			fclose(truncfptr);
		}
#else
		ret_ssize = fgetxattr(fileno(metafptr), "user.trunc_size",
				&temp_trunc_size, sizeof(long long));

		if ((ret_ssize >= 0) && (tmp_size < temp_trunc_size)) {
			tmp_size = temp_trunc_size;
			fremovexattr(fileno(metafptr), "user.trunc_size");
		}
#endif

		if (tmp_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((tmp_size - 1) / MAX_BLOCK_SIZE) + 1;

		flock(fileno(metafptr), LOCK_UN);
		mlock = FALSE;

		/* Delete all blocks */
		current_page = -1;
		for (block_count = 0; block_count < total_blocks;
							block_count++) {
			if (hcfs_system->system_going_down == TRUE)
				break;
			flock(fileno(metafptr), LOCK_EX);
			mlock = TRUE;

			current_index = block_count % BLK_INCREMENTS;
			which_page = block_count / BLK_INCREMENTS;

			if (current_page != which_page) {
				page_pos = seek_page2(&tempfilemeta, metafptr,
					which_page, 0);

				if (page_pos <= 0) {
					block_count += BLK_INCREMENTS - 1;
					flock(fileno(metafptr), LOCK_UN);
					mlock = FALSE;
					continue;
				}
				current_page = which_page;
			}

			/*TODO: error handling here if cannot read correctly*/

			FSEEK(metafptr, page_pos, SEEK_SET);

			FREAD(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
								metafptr);
			block_status =
				temppage.block_entries[current_index].uploaded;

			/* Delete backend object if uploaded */
			if (block_status == TRUE) {
				flock(fileno(metafptr), LOCK_UN);
				mlock = FALSE;
				sem_wait(&(delete_ctl.delete_queue_sem));
				sem_wait(&(delete_ctl.delete_op_sem));
				curl_id = -1;
				for (count = 0; count < MAX_DELETE_CONCURRENCY;
								count++) {
#if (DEDUP_ENABLE)
					ret_val = _use_delete_thread(count,
						TRUE, ptr->inode, block_count,
						temppage.block_entries[current_index].obj_id);
#else
					ret_val = _use_delete_thread(count,
						TRUE, ptr->inode, block_count);
#endif
					if (ret_val == 0) {
						curl_id = count;
						break;
					}
				}
				sem_post(&(delete_ctl.delete_op_sem));
				tmp_tn = &(delete_ctl.threads_no[curl_id]);
				tmp_dt = &(delete_ctl.delete_threads[curl_id]);
				pthread_create(tmp_tn, NULL,
						(void *)&con_object_dsync,
							(void *)tmp_dt);
				delete_ctl.threads_created[curl_id] = TRUE;
			} else {
				flock(fileno(metafptr), LOCK_UN);
				mlock = FALSE;
			}
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
	}

errcode_handle:
	/* TODO: If cannot handle object deletion from metaptr, need to scrub */
	if (metafptr != NULL) {
		if (mlock == TRUE)
			flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
	}

	if (hcfs_system->system_going_down == TRUE)
		return;

	/* Delete meta */
	sem_wait(&(delete_ctl.delete_queue_sem));
	sem_wait(&(delete_ctl.delete_op_sem));
	curl_id = -1;
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++) {
#if (DEDUP_ENABLE)
		ret_val = _use_delete_thread(count, FALSE, ptr->inode, -1, NULL);
#else
		ret_val = _use_delete_thread(count, FALSE, ptr->inode, -1);
#endif
		if (ret_val == 0) {
			curl_id = count;
			break;
		}
	}
	sem_post(&(delete_ctl.delete_op_sem));

	pthread_create(&(delete_ctl.threads_no[curl_id]), NULL,
		(void *)&con_object_dsync,
		(void *)&(delete_ctl.delete_threads[curl_id]));

	delete_ctl.threads_created[curl_id] = TRUE;
	delete_ctl.threads_finished[curl_id] = FALSE;

	pthread_join(delete_ctl.threads_no[curl_id], NULL);

	sem_wait(&(delete_ctl.delete_op_sem));
	delete_ctl.threads_in_use[curl_id] = FALSE;
	delete_ctl.threads_created[curl_id] = FALSE;
	delete_ctl.total_active_delete_threads--;
	sem_post(&(delete_ctl.delete_op_sem));
	sem_post(&(delete_ctl.delete_queue_sem));

	/*Wait for any upload to complete and change super inode
		from to_delete to deleted*/

	while (TRUE) {
		in_sync = FALSE;
		sem_wait(&(sync_ctl.sync_op_sem));
		/*Check if this inode is being synced now*/
		for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
			if (sync_ctl.threads_in_use[count] ==
					this_inode) {
				in_sync = TRUE;
				break;
			}
		}
		sem_post(&(sync_ctl.sync_op_sem));
		if (in_sync == TRUE)
			sleep(10);
		else
			break;
	}
	/* Update FS stat in the backend if updated previously */
	if (upload_seq > 0)
		update_backend_stat(root_inode, -system_size_change, -1);

	unlink(thismetapath);
	super_block_delete(this_inode);
	super_block_reclaim();

	which_index = ptr->which_index;
	dsync_ctl.threads_finished[which_index] = TRUE;

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
int do_meta_delete(ino_t this_inode, CURL_HANDLE *curl_handle)
{
	char objname[1000];
	int ret_val, ret;

	sprintf(objname, "meta_%ju", (uintmax_t)this_inode);
	write_log(10, "Debug meta deletion: objname %s, inode %ju\n",
						objname, this_inode);
	sprintf(curl_handle->id, "delete_meta_%ju", (uintmax_t)this_inode);
	ret_val = hcfs_delete_object(objname, curl_handle);
	/* Already retried in get object if necessary */
	if ((ret_val >= 200) && (ret_val <= 299))
		ret = 0;
	else
		ret = -EIO;
	return ret;
}

/************************************************************************
*
* Function name: do_block_delete
*        Inputs: ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle
*       Summary: Given curl handle "curl_handle", delete the block object
*                of inode number "this_inode", block no "block_no" from backend.
*  Return value: 0 if successful, and negation of errcode if not.
*
*************************************************************************/
int do_block_delete(ino_t this_inode, long long block_no,
#if (DEDUP_ENABLE)
				unsigned char *obj_id, CURL_HANDLE *curl_handle)
#else
				CURL_HANDLE *curl_handle)
#endif
{
	char objname[400];
	char obj_id_str[OBJID_STRING_LENGTH];
	int ret_val, ret, ddt_ret;
	FILE *ddt_fptr;
	int ddt_fd;
	DDT_BTREE_NODE tree_root;
	DDT_BTREE_META ddt_meta;

/* Handle objname - consider platforms, dedup flag  */
#if (DEDUP_ENABLE)
	/* Object named by block hashkey */
	obj_id_to_string(obj_id, obj_id_str);
	sprintf(objname, "data_%s", obj_id_str);

	/* Get dedup table meta */
	ddt_fptr = get_ddt_btree_meta(obj_id, &tree_root, &ddt_meta);
	if (ddt_fptr == NULL) {
		/* Can't access ddt btree file */
		return -EBADF;
	}
	ddt_fd = fileno(ddt_fptr);

	/* Update ddt */
	ddt_ret = decrease_ddt_el_refcount(obj_id, &tree_root, ddt_fd, &ddt_meta);
#else
	sprintf(objname, "data_%ju_%lld", (uintmax_t)this_inode, block_no);
	/* Force to delete */
	ddt_ret = 0;
#endif

	if (ddt_ret == 0) {
		write_log(10,
			"Debug delete object: objname %s, inode %ju, block %lld\n",
			objname, (uintmax_t)this_inode, block_no);
		sprintf(curl_handle->id, "delete_blk_%ju_%lld", this_inode, block_no);
		ret_val = hcfs_delete_object(objname, curl_handle);
		/* Already retried in get object if necessary */
		if ((ret_val >= 200) && (ret_val <= 299))
			ret = 0;
		else
			ret = -EIO;
	} else if (ddt_ret == 1) {
		write_log(10, "Only decrease refcount of object - %s", objname);
		ret = 0;
	} else if (ddt_ret == 2) {
		printf("Can't find this element, maybe deleted already?\n");
		ret = 0;
	} else {
		printf("ERROR delete el tree\n");
		ret = -EIO;
	}

#if (DEDUP_ENABLE)
	flock(ddt_fd, LOCK_UN);
	fclose(ddt_fptr);
#endif

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
	int which_curl;
	int which_index;

	which_curl = delete_thread_ptr->which_curl;
	which_index = delete_thread_ptr->which_index;
	if (delete_thread_ptr->is_block == TRUE)
		do_block_delete(delete_thread_ptr->inode,
			delete_thread_ptr->blockno,
#if (DEDUP_ENABLE)
			delete_thread_ptr->obj_id,
#endif
			&(delete_curl_handles[which_curl]));
	else
		do_meta_delete(delete_thread_ptr->inode,
			&(delete_curl_handles[which_curl]));

	delete_ctl.threads_finished[which_index] = TRUE;
}

/* Helper for creating threads for deletion */
int _dsync_use_thread(int index, ino_t this_inode, mode_t this_mode)
{
	if (dsync_ctl.threads_in_use[index] != 0)
		return -1;
	dsync_ctl.threads_in_use[index] = this_inode;
	dsync_ctl.threads_created[index] = FALSE;
	dsync_ctl.threads_finished[index] = FALSE;
	dsync_thread_info[index].inode = this_inode;
	dsync_thread_info[index].this_mode = this_mode;
	dsync_thread_info[index].which_index = index;
	pthread_create(&(dsync_ctl.inode_dsync_thread[index]), NULL,
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
#ifdef _ANDROID_ENV_
void *delete_loop(void *ptr)
#else
void delete_loop(void)
#endif
{
	ino_t inode_to_dsync, inode_to_check;
	SUPER_BLOCK_ENTRY tempentry;
	int count;
	char in_dsync;
	int ret_val;

	init_delete_control();
	init_dsync_control();

	write_log(2, "Start delete loop\n");

	inode_to_check = 0;
	while (hcfs_system->system_going_down == FALSE) {
		if (inode_to_check == 0)
			sleep(5);

		/* Get the first to-delete inode if inode_to_check is none. */
		sem_wait(&(dsync_ctl.dsync_queue_sem));
		super_block_share_locking();
		if (inode_to_check == 0)
			inode_to_check =
				sys_super_block->head.first_to_delete_inode;

		/* Find next to-delete inode if inode_to_check is not the
			last one. */
		inode_to_dsync = 0;
		if (inode_to_check != 0) {
			inode_to_dsync = inode_to_check;

			ret_val = read_super_block_entry(inode_to_dsync,
								&tempentry);

			if ((ret_val < 0) ||
				(tempentry.status != TO_BE_DELETED)) {
				inode_to_dsync = 0;
				inode_to_check = 0;
			} else {
				inode_to_check = tempentry.util_ll_next;
			}
		}
		super_block_share_release();

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
						tempentry.inode_stat.st_mode);
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
}
