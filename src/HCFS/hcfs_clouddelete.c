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

#include "hcfs_clouddelete.h"

#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

#include "hcfs_tocloud.h"
#include "params.h"
#include "super_block.h"
#include "fuseop.h"
#include "global.h"

extern SYSTEM_CONF_STRUCT system_config;

static DSYNC_THREAD_TYPE dsync_thread_info[MAX_DSYNC_CONCURRENCY];

CURL_HANDLE delete_curl_handles[MAX_DELETE_CONCURRENCY];

/* Helper function for terminating threads in deleting backend objects */
/* Dsync threads are the ones that find the objects to be deleted in
	a single filesystem object. */
static inline void _dsync_terminate_thread(int index)
{
	int ret;

	if ((dsync_ctl.threads_in_use[index] != 0) &&
			(dsync_ctl.threads_created[index] == TRUE)) {
		ret = pthread_tryjoin_np(dsync_ctl.inode_dsync_thread[index],
									NULL);
		if (ret == 0) {
			dsync_ctl.threads_in_use[index] = 0;
			dsync_ctl.threads_created[index] = FALSE;
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
	int ret_val;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	/*TODO: Perhaps need to change this flag to allow
		terminating at shutdown*/
	while (TRUE) {
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
				(delete_ctl.threads_created[index] == TRUE)) {
		ret = pthread_tryjoin_np(delete_ctl.threads_no[index],
									NULL);
		if (ret == 0) {
			delete_ctl.threads_in_use[index] = FALSE;
			delete_ctl.threads_created[index] = FALSE;
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
	int ret_val;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	/*TODO: Perhaps need to change this flag to allow terminating
		at shutdown*/
	while (TRUE) {
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
	dsync_ctl.total_active_dsync_threads = 0;

	pthread_create(&(dsync_ctl.dsync_handler_thread), NULL,
				(void *)&collect_finished_dsync_threads, NULL);
}

/* Helper for initializing curl handles for deleting backend objects */
static inline int _init_delete_handle(int index)
{
	int ret_val;

	ret_val = hcfs_init_backend(&(delete_curl_handles[index]));

	while ((ret_val < 200) || (ret_val > 299)) {
		if (delete_curl_handles[index].curl != NULL)
			hcfs_destroy_backend(delete_curl_handles[index].curl);
		ret_val = hcfs_init_backend(&(delete_curl_handles[index]));
	}
	return ret_val;
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
	delete_ctl.total_active_delete_threads = 0;

	pthread_create(&(delete_ctl.delete_handler_thread), NULL,
			(void *)&collect_finished_delete_threads, NULL);
}

/* Helper function for marking a delete thread as in use */
static inline int _use_delete_thread(int index, char is_blk_flag,
				ino_t this_inode, long long blockno)
{
	if (delete_ctl.threads_in_use[index] != FALSE)
		return -1;

	delete_ctl.threads_in_use[index] = TRUE;
	delete_ctl.threads_created[index] = FALSE;
	delete_ctl.delete_threads[index].is_block = is_blk_flag;
	delete_ctl.delete_threads[index].inode = this_inode;
	if (is_blk_flag == TRUE)
		delete_ctl.delete_threads[index].blockno = blockno;
	delete_ctl.delete_threads[index].which_curl = index;

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
	char thismetapath[400];
	ino_t this_inode;
	FILE *metafptr;
	FILE_META_TYPE tempfilemeta;
	BLOCK_ENTRY_PAGE temppage;
	int which_curl;
	long long block_no, current_index;
	long long page_pos;
	long long count, block_count;
	unsigned char block_status;
	char delete_done;
	char in_sync;
	int ret_val;
	struct timespec time_to_sleep;
	pthread_t tmp_t;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	this_inode = ptr->inode;

	fetch_todelete_path(thismetapath, this_inode);

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL)
		return;

	setbuf(metafptr, NULL);
	
	if ((ptr->this_mode) & S_IFREG) {
		flock(fileno(metafptr), LOCK_EX);
		fseek(metafptr, sizeof(struct stat), SEEK_SET);
		fread(&tempfilemeta, sizeof(FILE_META_TYPE), 1, metafptr);
		page_pos = tempfilemeta.next_block_page;
		current_index = 0;

		flock(fileno(metafptr), LOCK_UN);

		/* Delete all blocks */
		for (block_count = 0; page_pos != 0; block_count++) {
			flock(fileno(metafptr), LOCK_EX);

			if (current_index >= MAX_BLOCK_ENTRIES_PER_PAGE) {
				page_pos = temppage.next_page;
				current_index = 0;
				if (page_pos == 0) {
					flock(fileno(metafptr), LOCK_UN);
					break;
				}
			}

			/*TODO: error handling here if cannot read correctly*/

			fseek(metafptr, page_pos, SEEK_SET);
			if (ftell(metafptr) != page_pos) {
				flock(fileno(metafptr), LOCK_UN);
				break;
			}

			ret_val = fread(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
								metafptr);
			if (ret_val < 1) {
				flock(fileno(metafptr), LOCK_UN);
				break;
			}

			block_status =
				temppage.block_entries[current_index].status;

			if ((block_status != ST_LDISK) &&
						(block_status != ST_NONE)) {
				flock(fileno(metafptr), LOCK_UN);
				sem_wait(&(delete_ctl.delete_queue_sem));
				sem_wait(&(delete_ctl.delete_op_sem));
				which_curl = -1;
				for (count = 0; count < MAX_DELETE_CONCURRENCY;
								count++) {
					ret_val = _use_delete_thread(count,
						TRUE, ptr->inode, block_count);
					if (ret_val == 0) {
						which_curl = count;
						break;
					}
				}
				sem_post(&(delete_ctl.delete_op_sem));
				tmp_t = &(delete_ctl.threads_no[which_curl]);
				pthread_create(tmp_t, NULL,
						(void *)&con_object_dsync,
							(void *)&(delete_ctl.delete_threads[which_curl]));
				delete_ctl.threads_created[which_curl] = TRUE;
			} else {
				flock(fileno(metafptr), LOCK_UN);
			}
			current_index++;
		}
		/* Block deletion should be done here. Check if all delete
		threads for this inode has returned before starting meta
		deletion*/

		delete_done = FALSE;
		while (delete_done == FALSE) {
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

	/* Delete meta */
	sem_wait(&(delete_ctl.delete_queue_sem));
	sem_wait(&(delete_ctl.delete_op_sem));
	which_curl = -1;
	for (count = 0; count < MAX_DELETE_CONCURRENCY; count++) {
		ret_val = _use_delete_thread(count, FALSE, ptr->inode, -1);
		if (ret_val == 0) {
			which_curl = count;
			break;
		}
	}
	sem_post(&(delete_ctl.delete_op_sem));

	flock(fileno(metafptr), LOCK_EX);

	pthread_create(&(delete_ctl.threads_no[which_curl]), NULL,
		(void *)&con_object_dsync,
		(void *)&(delete_ctl.delete_threads[which_curl]));

	delete_ctl.threads_created[which_curl] = TRUE;
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);

	pthread_join(delete_ctl.threads_no[which_curl], NULL);

	sem_wait(&(delete_ctl.delete_op_sem));
	delete_ctl.threads_in_use[which_curl] = FALSE;
	delete_ctl.threads_created[which_curl] = FALSE;
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
	unlink(thismetapath);
	super_block_delete(this_inode);
	super_block_reclaim();
}

/************************************************************************
*
* Function name: do_meta_delete
*        Inputs: ino_t this_inode, CURL_HANDLE *curl_handle
*       Summary: Given curl handle "curl_handle", delete the meta object
*                of inode number "this_inode" from backend.
*  Return value: None
*
*************************************************************************/
void do_meta_delete(ino_t this_inode, CURL_HANDLE *curl_handle)
{
	char objname[1000];
	int ret_val;

	sprintf(objname, "meta_%lld", this_inode);
	printf("Debug meta deletion: objname %s, inode %lld\n",
						objname, this_inode);
	sprintf(curl_handle->id, "delete_meta_%lld", this_inode);
	ret_val = hcfs_delete_object(objname, curl_handle);
}

/************************************************************************
*
* Function name: do_block_delete
*        Inputs: ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle
*       Summary: Given curl handle "curl_handle", delete the block object
*                of inode number "this_inode", block no "block_no" from backend.
*  Return value: None
*
*************************************************************************/
void do_block_delete(ino_t this_inode, long long block_no,
						CURL_HANDLE *curl_handle)
{
	char objname[1000];
	int ret_val;

	sprintf(objname, "data_%lld_%lld", this_inode, block_no);
	printf("Debug delete object: objname %s, inode %lld, block %lld\n",
					objname, this_inode, block_no);
	sprintf(curl_handle->id, "delete_blk_%lld_%lld", this_inode, block_no);
	ret_val = hcfs_delete_object(objname, curl_handle);
}

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

	which_curl = delete_thread_ptr->which_curl;
	if (delete_thread_ptr->is_block == TRUE)
		do_block_delete(delete_thread_ptr->inode,
			delete_thread_ptr->blockno,
			&(delete_curl_handles[which_curl]));
	else
		do_meta_delete(delete_thread_ptr->inode,
			&(delete_curl_handles[which_curl]));
}

/* Helper for creating threads for deletion */
int _dsync_use_thread(int index, ino_t this_inode, mode_t this_mode)
{
	if (dsync_ctl.threads_in_use[index] != 0)
		return -1;
	dsync_ctl.threads_in_use[index] = this_inode;
	dsync_ctl.threads_created[index] = FALSE;
	dsync_thread_info[index].inode = this_inode;
	dsync_thread_info[index].this_mode = this_mode;
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
void *delete_loop(void *arg)
{
	ino_t inode_to_dsync, inode_to_check;
	SUPER_BLOCK_ENTRY tempentry;
	int count, sleep_count;
	char in_dsync;
	int ret_val;

	init_delete_control();
	init_dsync_control();

	printf("Start delete loop\n");

	inode_to_check = 0;
	while (TRUE) {
		if (inode_to_check == 0)
			sleep(5);
		
		/* Get the first to-delete inode if inode_to_check is none. */
		sem_wait(&(dsync_ctl.dsync_queue_sem));
		super_block_share_locking();
		if (inode_to_check == 0)
			inode_to_check =
				sys_super_block->head.first_to_delete_inode;
		
		/* Find next to-delete inode if inode_to_check is not the last one. */
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

		/* Delete the meta/block of inode_to_dsync if it finish dsynced. */
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

