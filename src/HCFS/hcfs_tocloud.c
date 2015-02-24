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
*
**************************************************************************/

/*
TODO: Will need to check mod time of meta file and not upload meta for
	every block status change.
TODO: Check if meta objects will be deleted with the deletion of files/dirs
TODO: error handling for HTTP exceptions
*/

#include "hcfs_tocloud.h"

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/file.h>

#include "hcfs_clouddelete.h"
#include "params.h"
#include "global.h"
#include "super_block.h"
#include "fuseop.h"

extern SYSTEM_CONF_STRUCT system_config;

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

static inline void _sync_terminate_thread(int index)
{
	int ret;

	if ((sync_ctl.threads_in_use[index] != 0) &&
			(sync_ctl.threads_created[index] == TRUE)) {
		ret = pthread_tryjoin_np(sync_ctl.inode_sync_thread[index],
									NULL);
		if (ret == 0) {
			sync_ctl.threads_in_use[index] = 0;
			sync_ctl.threads_created[index] == FALSE;
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
	/*TODO: Perhaps need to change this flag to allow
		terminating at shutdown*/
	while (TRUE) {
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

static inline void _upload_terminate_thread(int index)
{
	int count2;
	int which_curl;
	int ret_val;
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
	size_t tmp_size;
	DELETE_THREAD_TYPE *tmp_del;

	if (upload_ctl.threads_in_use[index] == 0)
		return;

	if (upload_ctl.upload_threads[index].is_block != TRUE)
		return;

	if (upload_ctl.threads_created[index] != TRUE)
		return;

	ret_val = pthread_tryjoin_np(upload_ctl.upload_threads_no[index], NULL);

	if (ret_val != 0)
		return;

	this_inode = upload_ctl.upload_threads[index].inode;
	is_delete = upload_ctl.upload_threads[index].is_delete;
	page_filepos = upload_ctl.upload_threads[index].page_filepos;
	e_index = upload_ctl.upload_threads[index].page_entry_index;
	blockno = upload_ctl.upload_threads[index].blockno;
	fetch_meta_path(thismetapath, this_inode);

	/*Perhaps the file is deleted already*/
	if (!access(thismetapath, F_OK)) {
		metafptr = fopen(thismetapath, "r+");
		if (metafptr != NULL) {
			setbuf(metafptr, NULL);
			flock(fileno(metafptr), LOCK_EX);
			/*Perhaps the file is deleted already*/
			if (!access(thismetapath, F_OK)) {
				fseek(metafptr, page_filepos, SEEK_SET);
				fread(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1,
								metafptr);
				tmp_entry = &(temppage.block_entries[e_index]);
				tmp_size = sizeof(BLOCK_ENTRY_PAGE);
				if ((tmp_entry->status == ST_LtoC) &&
							(is_delete == FALSE)) {
					tmp_entry->status = ST_BOTH;
					fetch_block_path(blockpath, this_inode,
								blockno);
					setxattr(blockpath, "user.dirty", "F",
									1, 0);
					fseek(metafptr, page_filepos, SEEK_SET);
					fwrite(&temppage, tmp_size, 1,
								metafptr);
				} else {
					if ((tmp_entry->status == ST_TODELETE)
						&& (is_delete == TRUE)) {
						tmp_entry->status = ST_NONE;
						fseek(metafptr, page_filepos,
								SEEK_SET);
						fwrite(&temppage, tmp_size,
								1, metafptr);
					}
				}
				/*TODO: Check if status is ST_NONE. If so,
				the block is removed due to truncating.
				Need to schedule block for deletion due to
				truncating*/
			}
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
		}
	}
	/*TODO: If metafile is deleted already, schedule the
			block to be deleted.*/
	/*TODO: This must be before the usage flag is cleared*/

	/*If file is deleted*/
	if (access(thismetapath, F_OK) == -1) {
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

	upload_ctl.threads_in_use[index] = FALSE;
	upload_ctl.threads_created[index] = FALSE;
	upload_ctl.total_active_upload_threads--;
	sem_post(&(upload_ctl.upload_queue_sem));
}

void collect_finished_upload_threads(void *ptr)
{
	int count;
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	/*TODO: Perhaps need to change this flag to allow
		terminating at shutdown*/
	while (TRUE) {
		sem_wait(&(upload_ctl.upload_op_sem));

		if (upload_ctl.total_active_upload_threads <= 0) {
			sem_post(&(upload_ctl.upload_op_sem));
			nanosleep(&time_to_sleep, NULL);
			continue;
		}
		for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++)
			_upload_terminate_thread(count);
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

	for (count = 0; count < MAX_UPLOAD_CONCURRENCY; count++)
		ret_val = hcfs_init_backend(&(upload_curl_handles[count]));

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
	BLOCK_ENTRY_PAGE temppage;
	int which_curl;
	long long page_pos, e_index;
	long long total_blocks, total_pages;
	long long count, block_count;
	unsigned char block_status;
	char upload_done;
	int ret_val;
	off_t tmp_size;
	struct timespec time_to_sleep;
	BLOCK_ENTRY *tmp_entry;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	this_inode = ptr->inode;

	fetch_meta_path(thismetapath, this_inode);

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		super_block_update_transit(ptr->inode, FALSE);
		return;
	}

	setbuf(metafptr, NULL);

	if ((ptr->this_mode) & S_IFREG) {
		flock(fileno(metafptr), LOCK_EX);
		fread(&tempfilestat, sizeof(struct stat), 1, metafptr);
		fread(&tempfilemeta, sizeof(FILE_META_TYPE), 1, metafptr);
		page_pos = tempfilemeta.next_block_page;
		e_index = 0;
		tmp_size = tempfilestat.st_size;
		if (tmp_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((tmp_size - 1) / MAX_BLOCK_SIZE) + 1;

		if (total_blocks == 0)
			total_pages = 0;
		else
			total_pages = ((total_blocks - 1) /
					MAX_BLOCK_ENTRIES_PER_PAGE) + 1;

		flock(fileno(metafptr), LOCK_UN);

		for (block_count = 0; page_pos != 0; block_count++) {
			flock(fileno(metafptr), LOCK_EX);

			/*Perhaps the file is deleted already*/
			if (access(thismetapath, F_OK) < 0) {
				flock(fileno(metafptr), LOCK_UN);
				break;
			}

			if (e_index >= MAX_BLOCK_ENTRIES_PER_PAGE) {
				page_pos = temppage.next_page;
				e_index = 0;
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

			tmp_entry = &(temppage.block_entries[e_index]);
			block_status = tmp_entry->status;

			if (((block_status == ST_LDISK) ||
				(block_status == ST_LtoC)) &&
					(block_count < total_blocks)) {
				if (block_status == ST_LDISK) {
					tmp_entry->status = ST_LtoC;
					fseek(metafptr, page_pos, SEEK_SET);
					fwrite(&temppage,
						sizeof(BLOCK_ENTRY_PAGE),
								1, metafptr);
				}
				flock(fileno(metafptr), LOCK_UN);
				sem_wait(&(upload_ctl.upload_queue_sem));
				sem_wait(&(upload_ctl.upload_op_sem));
				which_curl = _select_upload_thread(TRUE, FALSE,
						ptr->inode, block_count,
							page_pos, e_index);
				sem_post(&(upload_ctl.upload_op_sem));
				dispatch_upload_block(which_curl);
				/*TODO: Maybe should also first copy block
					out first*/
				e_index++;
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

			e_index++;
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

	sem_wait(&(upload_ctl.upload_queue_sem));
	sem_wait(&(upload_ctl.upload_op_sem));
	which_curl = _select_upload_thread(FALSE, FALSE, ptr->inode, 0, 0, 0);

	sem_post(&(upload_ctl.upload_op_sem));

	flock(fileno(metafptr), LOCK_EX);
	/*Check if metafile still exists. If not, forget the meta upload*/
	if (!access(thismetapath, F_OK)) {
		schedule_sync_meta(metafptr, which_curl);
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

		super_block_update_transit(ptr->inode, FALSE);
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
}

void do_block_sync(ino_t this_inode, long long block_no,
			CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	FILE *fptr;
	int ret_val;

	sprintf(objname, "data_%lld_%lld", this_inode, block_no);
	printf("Debug datasync: objname %s, inode %lld, block %lld\n",
					objname, this_inode, block_no);
	sprintf(curl_handle->id, "upload_blk_%lld_%lld", this_inode, block_no);
	fptr = fopen(filename, "r");
	ret_val = hcfs_put_object(fptr, objname, curl_handle);
	fclose(fptr);
}

void do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename)
{
	char objname[1000];
	int ret_val;
	FILE *fptr;

	sprintf(objname, "meta_%lld", this_inode);
	printf("Debug datasync: objname %s, inode %lld\n", objname, this_inode);
	sprintf(curl_handle->id, "upload_meta_%lld", this_inode);
	fptr = fopen(filename, "r");
	ret_val = hcfs_put_object(fptr, objname, curl_handle);
	fclose(fptr);
}

void con_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl;

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE)
		do_block_sync(thread_ptr->inode, thread_ptr->blockno,
				&(upload_curl_handles[which_curl]),
						thread_ptr->tempfilename);
	else
		do_meta_sync(thread_ptr->inode,
				&(upload_curl_handles[which_curl]),
						thread_ptr->tempfilename);

	unlink(thread_ptr->tempfilename);
}

void delete_object_sync(UPLOAD_THREAD_TYPE *thread_ptr)
{
	int which_curl;

	which_curl = thread_ptr->which_curl;
	if (thread_ptr->is_block == TRUE)
		do_block_delete(thread_ptr->inode, thread_ptr->blockno,
					&(upload_curl_handles[which_curl]));
}

void schedule_sync_meta(FILE *metafptr, int which_curl)
{
	char tempfilename[400];
	char filebuf[4100];
	int read_size;
	int count;
	FILE *fptr;

	sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%lld.tmp",
			upload_ctl.upload_threads[which_curl].inode);

	count = 0;
	while (TRUE) {
		if (!access(tempfilename, F_OK)) {
			count++;
			sprintf(tempfilename, "/dev/shm/hcfs_sync_meta_%lld.%d",
				upload_ctl.upload_threads[which_curl].inode,
									count);
		} else {
			break;
		}
	}

	fptr = fopen(tempfilename, "w");
	fseek(metafptr, 0, SEEK_SET);
	while (!feof(metafptr)) {
		read_size = fread(filebuf, 1, 4096, metafptr);
		if (read_size > 0)
			fwrite(filebuf, 1, read_size, fptr);
		else
			break;
	}
	fclose(fptr);

	strcpy(upload_ctl.upload_threads[which_curl].tempfilename,
							tempfilename);
	pthread_create(&(upload_ctl.upload_threads_no[which_curl]), NULL,
			(void *)&con_object_sync,
			(void *)&(upload_ctl.upload_threads[which_curl]));
	upload_ctl.threads_created[which_curl] = TRUE;
}

void dispatch_upload_block(int which_curl)
{
	char tempfilename[400];
	char thisblockpath[400];
	char filebuf[4100];
	int read_size;
	int count;
	FILE *fptr, *blockfptr;
	UPLOAD_THREAD_TYPE *upload_ptr;

	upload_ptr = &(upload_ctl.upload_threads[which_curl]);

	sprintf(tempfilename, "/dev/shm/hcfs_sync_block_%lld_%lld.tmp",
				upload_ptr->inode, upload_ptr->blockno);

	count = 0;
	while (TRUE) {
		if (!access(tempfilename, F_OK)) {
			count++;
			sprintf(tempfilename,
				"/dev/shm/hcfs_sync_meta_%lld_%lld.%d",
				upload_ptr->inode, upload_ptr->blockno, count);
		} else {
			break;
		}
	}

	fetch_block_path(thisblockpath,	upload_ptr->inode, upload_ptr->blockno);

	blockfptr = fopen(thisblockpath, "r");
	if (blockfptr != NULL) {
		flock(fileno(blockfptr), LOCK_EX);
		fptr = fopen(tempfilename, "w");
		while (!feof(blockfptr)) {
			read_size = fread(filebuf, 1, 4096, blockfptr);
			if (read_size > 0)
				fwrite(filebuf, 1, read_size, fptr);
			else
				break;
		}
		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
		fclose(fptr);

		strcpy(upload_ptr->tempfilename, tempfilename);
		pthread_create(&(upload_ctl.upload_threads_no[which_curl]),
			NULL, (void *)&con_object_sync,	(void *)upload_ptr);
		upload_ctl.threads_created[which_curl] = TRUE;
	} else {
		/*Block is gone. Undo changes*/
		sem_wait(&(upload_ctl.upload_op_sem));
		upload_ctl.threads_in_use[which_curl] = FALSE;
		upload_ctl.threads_created[which_curl] = FALSE;
		upload_ctl.total_active_upload_threads--;
		sem_post(&(upload_ctl.upload_op_sem));
		sem_post(&(upload_ctl.upload_queue_sem));
	}
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
			sync_threads[count].inode = this_inode;
			sync_threads[count].this_mode = this_mode;
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
	int ret_val;
	char do_something;
	char is_start_check;

	init_upload_control();
	init_sync_control();
	is_start_check = TRUE;

	printf("Start upload loop\n");

	while (TRUE) {
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
					write_super_block_entry(ino_sync,
								&tempentry);
				 }
			 }
		 }
		super_block_exclusive_release();

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
}

