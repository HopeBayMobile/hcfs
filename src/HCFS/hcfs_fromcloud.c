/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_fromcloud.c
* Abstract: The c source code file for retrieving meta or data from
*           backend.
*
* Revision History
* 2015/2/12 Jiahong added header for this file, and revising coding style.
* 2015/6/4 Jiahong added error handling
*
**************************************************************************/

#include "hcfs_fromcloud.h"

#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <inttypes.h>
#include <jansson.h>

#include "params.h"
#include "enc.h"
#include "compress.h"
#include "hcfscurl.h"
#include "fuseop.h"
#include "global.h"
#include "hfuse_system.h"
#include "logger.h"
#include "macro.h"
#include "utils.h"
#include "dedup_table.h"
#include "metaops.h"
#include "super_block.h"

/************************************************************************
*
* Function name: fetch_from_cloud
*        Inputs: FILE *fptr, ino_t this_inode, long long block_no
*       Summary: Read block "block_no" of inode "this_inode" from backend,
*                and write to the file pointed by "fptr".
*  Return value: 0 if successful, or negation of error code.
*
*************************************************************************/
int fetch_from_cloud(FILE *fptr, char action_from, char *objname)
{
#if (DEDUP_ENABLE)
	char obj_id_str[OBJID_STRING_LENGTH];
#endif
	int status;
	int which_curl_handle;
	int ret, errcode;
	long tmplen;

	if (hcfs_system->sync_paused)
		return -EIO;

	/* Get sem if action is from pinning file or from download meta. */
	if (action_from == PIN_BLOCK || action_from == FETCH_FILE_META) 
		sem_wait(&pin_download_curl_sem);
	sem_wait(&download_curl_sem);
	sem_wait(&download_curl_control_sem);
	for (which_curl_handle = 0;
	     which_curl_handle < MAX_DOWNLOAD_CURL_HANDLE;
	     which_curl_handle++) {
		if (curl_handle_mask[which_curl_handle] == FALSE) {
			curl_handle_mask[which_curl_handle] = TRUE;
			break;
		}
	}
	sem_post(&download_curl_control_sem);
	write_log(10, "Debug: downloading using curl handle %d\n",
		  which_curl_handle);

	FSEEK(fptr, 0, SEEK_SET);
	FTRUNCATE(fileno(fptr), 0);

	char *get_fptr_data = NULL;
	size_t len = 0;

#if defined(__ANDROID__) || defined(_ANDROID_ENV_)
	FILE *get_fptr = tmpfile();
#else
	FILE *get_fptr = open_memstream(&get_fptr_data, &len);
#endif

        HCFS_encode_object_meta *object_meta =
            calloc(1, sizeof(HCFS_encode_object_meta));

        status = hcfs_get_object(get_fptr, objname,
                                 &(download_curl_handles[which_curl_handle]),
                                 object_meta);

	/* process failed get here */
	if ((status >= 200) && (status <= 299)) {
		errcode = 0;
	} else {
		if (status == 404) {
			errcode = -ENOENT;
		} else {
			write_log(4, "Warn: http code %d when get %s\n", status,
				objname);
			errcode = -EIO;
		}
		free_object_meta(object_meta);
		fclose(get_fptr);
		goto errcode_handle;
	}

#if defined(__ANDROID__) || defined(_ANDROID_ENV_)
	fseek(get_fptr, 0, SEEK_END);
	tmplen = ftell(get_fptr);
	get_fptr_data = calloc(tmplen + 10, sizeof(char));
	rewind(get_fptr);
	len =
	    fread(get_fptr_data, sizeof(char), tmplen, get_fptr);
#endif

	fclose(get_fptr);

	unsigned char *object_key = NULL;
#if ENCRYPT_ENABLE
	unsigned char *key = get_key("this is hopebay testing");
	object_key = calloc(KEY_SIZE, sizeof(unsigned char));
	decrypt_session_key(object_key, object_meta->enc_session_key, key);
	OPENSSL_free(key);
#endif

	decode_to_fd(fptr, object_key, (unsigned char *)get_fptr_data, len,
		     object_meta->enc_alg, object_meta->comp_alg);

	free_object_meta(object_meta);
	free(get_fptr_data);
	if (object_key != NULL)
		OPENSSL_free(object_key);
	fflush(fptr);

	/* Finally free download sem */

errcode_handle:
	sem_wait(&download_curl_control_sem);
	curl_handle_mask[which_curl_handle] = FALSE;

	/*Release sem if action from pinning file*/
	if (action_from == PIN_BLOCK || action_from == FETCH_FILE_META) 
		sem_post(&pin_download_curl_sem);
	sem_post(&download_curl_sem);
	sem_post(&download_curl_control_sem);

	return errcode;
}

/************************************************************************
*
* Function name: prefetch_block
*        Inputs: PREFETCH_STRUCT_TYPE *ptr
*       Summary: Prefetch the block specified in "ptr" to local cache.
*  Return value: None
*
* Note: For prefetch, will not attempt to return error code to others,
*       but will just log error and give up prefetching.
*
*************************************************************************/
void prefetch_block(PREFETCH_STRUCT_TYPE *ptr)
{
	FILE *metafptr;
	FILE *blockfptr;
	char thisblockpath[400];
	char objname[1000];
	char thismetapath[METAPATHLEN];
	BLOCK_ENTRY_PAGE temppage;
	int entry_index;
	struct stat tempstat;
	int ret, errcode;
	size_t ret_size;
	char block, mlock, bopen, mopen;

	block = FALSE;
	mlock = FALSE;
	bopen = FALSE;
	mopen = FALSE;

	entry_index = ptr->entry_index;
	/*Download from backend */
	fetch_meta_path(thismetapath, ptr->this_inode);
	fetch_block_path(thisblockpath, ptr->this_inode, ptr->block_no);

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		free(ptr);
		return;
	}
	mopen = TRUE;
	setbuf(metafptr, NULL);

	blockfptr = fopen(thisblockpath, "a+");
	if (blockfptr == NULL) {
		free(ptr);
		fclose(metafptr);
		return;
	}
	fclose(blockfptr);

	blockfptr = fopen(thisblockpath, "r+");
	if (blockfptr == NULL) {
		free(ptr);
		fclose(metafptr);
		return;
	}
	setbuf(blockfptr, NULL);
	flock(fileno(blockfptr), LOCK_EX);
	bopen = TRUE;
	block = TRUE;

	flock(fileno(metafptr), LOCK_EX);
	mlock = TRUE;

	FSEEK(metafptr, ptr->page_start_fpos, SEEK_SET);
	FREAD(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);

	if (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
	    ((temppage).block_entries[entry_index].status == ST_CtoL)) {
		if ((temppage).block_entries[entry_index].status == ST_CLOUD) {
			(temppage).block_entries[entry_index].status = ST_CtoL;
			FSEEK(metafptr, ptr->page_start_fpos, SEEK_SET);
			FWRITE(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
			       metafptr);
			fflush(metafptr);
		}
		flock(fileno(metafptr), LOCK_UN);
		mlock = FALSE;

#if (DEDUP_ENABLE)
		fetch_backend_block_objname(objname,
				temppage.block_entries[entry_index].obj_id);
#else
		fetch_backend_block_objname(objname, ptr->this_inode,
				ptr->block_no, ptr->seqnum);
#endif
		ret = fetch_from_cloud(blockfptr, READ_BLOCK, objname);
		if (ret < 0) {
			write_log(0, "Error prefetching\n");
			goto errcode_handle;
		}
		/*Do not process cache update and stored_where change if block
			is actually deleted by other ops such as truncate*/
		flock(fileno(metafptr), LOCK_EX);
		mlock = TRUE;
		FSEEK(metafptr, ptr->page_start_fpos, SEEK_SET);
		FREAD(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
		if (stat(thisblockpath, &tempstat) == 0) {
			(temppage).block_entries[entry_index].status = ST_BOTH;
			ret = set_block_dirty_status(NULL, blockfptr, FALSE);
			if (ret < 0) {
				goto errcode_handle;
			}
			FSEEK(metafptr, ptr->page_start_fpos, SEEK_SET);
			FWRITE(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
			       metafptr);
			ret = update_file_stats(metafptr, 0, 1,
						tempstat.st_size,
						0, ptr->this_inode);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}

			fflush(metafptr);

			sem_wait(&(hcfs_system->access_sem));
			hcfs_system->systemdata.cache_size += tempstat.st_size;
			hcfs_system->systemdata.cache_blocks++;
			sync_hcfs_system_data(FALSE);
			sem_post(&(hcfs_system->access_sem));
			ret = super_block_mark_dirty(ptr->this_inode);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
		}
	}
	flock(fileno(blockfptr), LOCK_UN);
	fclose(blockfptr);
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	free(ptr);

	return;

errcode_handle:
	if (block == TRUE)
		flock(fileno(blockfptr), LOCK_UN);
	if (bopen == TRUE)
		fclose(blockfptr);
	if (mlock == TRUE)
		flock(fileno(metafptr), LOCK_UN);
	if (mopen == TRUE)
		fclose(metafptr);
	free(ptr);
}

int init_download_control()
{
	memset(&download_thread_ctl, 0, sizeof(DOWNLOAD_THREAD_CTL));
	sem_init(&(download_thread_ctl.ctl_op_sem), 0, 1);
	sem_init(&(download_thread_ctl.dl_th_sem), 0, MAX_PIN_DL_CONCURRENCY);

	pthread_create(&(download_thread_ctl.manager_thread), NULL,
		(void *)&download_block_manager, NULL);

	write_log(5, "Init download thread control\n");
	return 0;
}

int destroy_download_control()
{
	pthread_join(download_thread_ctl.manager_thread, NULL);
	sem_destroy(&(download_thread_ctl.ctl_op_sem));
	sem_destroy(&(download_thread_ctl.dl_th_sem));

	write_log(5, "Terminate manager thread of downloading\n");

	return 0;
}

/**
 * download_block_manager
 *
 * This routine is a manager collecting and terminating those threads
 * downloading block. It waits for all active threads terminating when number
 * of active threads > 0. Otherwise, it will sleep for a while and check
 * active threads later.
 *
 */
void download_block_manager()
{
	int t_idx;
	int ret;
	pthread_t *tid;
	DOWNLOAD_BLOCK_INFO *block_info;
	struct timespec time_to_sleep;
	char error_path[200];
	FILE *fptr;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	while(TRUE) {
		sem_wait(&(download_thread_ctl.ctl_op_sem));

		/* Wait all threads when system going down */
		if (hcfs_system->system_going_down == TRUE) {
			if (download_thread_ctl.active_th <= 0) {
				sem_post(&(download_thread_ctl.ctl_op_sem));
				break;
			}
		}

		/* Sleep when number of active threads <= 0 */
		if (download_thread_ctl.active_th <= 0) {
			sem_post(&(download_thread_ctl.ctl_op_sem));
			sleep(1);
			continue;
		}

		for (t_idx = 0; t_idx < MAX_PIN_DL_CONCURRENCY; t_idx++) {
			/* Skip if non-active */
			if (download_thread_ctl.block_info[t_idx].active ==
								FALSE)
				continue;
			/* Try to terminate thread */
			tid = &(download_thread_ctl.download_thread[t_idx]);
			/* Do not lock download-control */
			sem_post(&(download_thread_ctl.ctl_op_sem));
			ret = pthread_join(*tid, NULL);
			sem_wait(&(download_thread_ctl.ctl_op_sem));
			if (ret < 0) {
				if (ret == EBUSY)
					continue;
				else
					write_log(0, "Error: Join thread "
						"error in %s. Code %d.\n",
						__func__, ret);
			}
			block_info = &(download_thread_ctl.block_info[t_idx]);

			/* Create empty file to record failure */
			if (block_info->dl_error == TRUE) {
				fetch_error_download_path(error_path,
					block_info->this_inode);
				fptr = fopen(error_path, "a+");
				if (fptr == NULL)
					write_log(0, "Error: Connot open "
						"error path in %s\n",
						__func__);
				else
					fclose(fptr);
			}

			/* Reset thread */
			memset(&(download_thread_ctl.block_info[t_idx]),
					0, sizeof(DOWNLOAD_BLOCK_INFO));
			download_thread_ctl.block_info[t_idx].active =
				FALSE;

			download_thread_ctl.active_th--;
			sem_post(&(download_thread_ctl.dl_th_sem));
		}
		sem_post(&(download_thread_ctl.ctl_op_sem));

		nanosleep(&time_to_sleep, NULL);
	}
}

static int _modify_block_status(const DOWNLOAD_BLOCK_INFO *block_info,
	char from_st, char to_st, long long cache_size_delta)
{
	BLOCK_ENTRY_PAGE block_page;
	int e_index, ret;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	char metapath[300];

	e_index = block_info->block_no % MAX_BLOCK_ENTRIES_PER_PAGE;

	meta_cache_entry = meta_cache_lock_entry(block_info->this_inode);
	if (meta_cache_entry == NULL) {
		return -ENOMEM;
	}

	/* Check whether meta exists or not. Stop to update block status
	if meta is removed. */
	fetch_meta_path(metapath, block_info->this_inode);
	if (access(metapath, F_OK) < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		write_log(2, "Warn: meta %"PRIu64" does not exist. In %s\n",
			(uint64_t)block_info->this_inode, __func__);
		return -ENOENT;
	}

	ret = meta_cache_lookup_file_data(block_info->this_inode, NULL, NULL,
		&block_page, block_info->page_pos, meta_cache_entry);
	if (ret < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		return ret;
	}

	if (block_page.block_entries[e_index].status == from_st) {
		block_page.block_entries[e_index].status = to_st;
	} else {
		meta_cache_unlock_entry(meta_cache_entry);
		/* return status if not match "from_st" */
		return block_page.block_entries[e_index].status;
	}

	ret = meta_cache_update_file_data(block_info->this_inode, NULL, NULL,
		&block_page, block_info->page_pos, meta_cache_entry);
	if (ret < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		return ret;
	}

	if (to_st == ST_BOTH) {
		/* If the download is completed, need to update
		per-file statistics */
		ret = meta_cache_open_file(meta_cache_entry);
		if (ret < 0) {
			return ret;
		}
		ret = update_file_stats(meta_cache_entry->fptr, 0,
					1, cache_size_delta, 0,
					block_info->this_inode);
		if (ret < 0) {
			return ret;
		}
	}
	ret = meta_cache_unlock_entry(meta_cache_entry);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

/**
 * fetch_backend_block
 *
 * This function aims to download a block from cloud and modify status
 * from ST_CLOUD to ST_LDISK. It may directly return when this block
 * is downloaded by other threads.
 *
 * @param ptr A pointer with type DOWNLOAD_BLOCK_INFO.
 *
 * @return none.
 */
void fetch_backend_block(void *ptr)
{
	char block_path[400];
	char objname[600];
	FILE *block_fptr;
	DOWNLOAD_BLOCK_INFO *block_info;
	int ret;
	struct stat blockstat;

	block_info = (DOWNLOAD_BLOCK_INFO *)ptr;
	fetch_block_path(block_path, block_info->this_inode,
						block_info->block_no);

	/* Create it if it does not exists, or do nothing */
	block_fptr = fopen(block_path, "a+");
	if (block_fptr == NULL) {
		write_log(0, "Error: Fail to open block path %s in %s\n",
							block_path, __func__);
		block_info->dl_error = TRUE;
		return;
	}
	fclose(block_fptr);

	block_fptr = fopen(block_path, "r+"); /* Write it */
	if (block_fptr == NULL) {
		write_log(0, "Error: Fail to open block path %s in %s\n",
							block_path, __func__);
		block_info->dl_error = TRUE;
		return;
	}

	/* Lock block ptr so that status ST_CtoL cannot be met */
	flock(fileno(block_fptr), LOCK_EX);
	setbuf(block_fptr, NULL);

	ret = _modify_block_status(block_info, ST_CLOUD, ST_CtoL, 0);
	if (ret < 0) {
		/* When file is removed, unlink this empty block directly
		because status of this block is st_cloud */
		if (ret == -ENOENT)
			unlink(block_path);
		else
			write_log(0, "Error: Fail to modify block status in %s."
				" Code %d", __func__, ret);
		goto thread_error;

	} else if (ret > 0) {
		/* Status does not match ST_CLOUD, it may be
		modified by another one */
		flock(fileno(block_fptr), LOCK_UN);
		fclose(block_fptr);
		return;
	}

	/* Fetch block from cloud */
	fetch_backend_block_objname(objname, block_info->this_inode,
			block_info->block_no, block_info->seqnum);
	ret = fetch_from_cloud(block_fptr, PIN_BLOCK, objname);
	if (ret < 0) {
		write_log(0, "Error: Fail to fetch block in %s\n", __func__);
		goto thread_error;
	}

	/* TODO: Check if error handling here is fixed later
	(a block delete op can happen due to a truncate and that's not
	an error) */
	/* Update dirty status and system meta */
	if (stat(block_path, &blockstat) == 0) {
		set_block_dirty_status(NULL, block_fptr, FALSE);
		change_system_meta(0, 0, blockstat.st_size, 1, 0, 0, TRUE);
		write_log(10, "Debug: Now cache size %lld",
			hcfs_system->systemdata.cache_size);
	} else {
		ret = errno;
		if (ret != ENOENT) {
			write_log(0, "Error: IO error in %s. Code %d",
				__func__, ret);
			goto thread_error;
		}

		ret = _modify_block_status(block_info, ST_CtoL, ST_CLOUD, 0);
		if (ret < 0) /* IO/memory error */
			goto thread_error;

		if (ret == 0) { /* Block disappear? Fetch again later */
			write_log(0, "Error: block_%"PRIu64"_%lld "
				"disappered. Fetch again later\n",
				(uint64_t)block_info->this_inode,
				block_info->block_no);
			goto thread_error;

		} else if (ret == ST_TODELETE) { /* Block is truncated. Finish */
			write_log(5, "block_%"PRIu64"_%lld is truncated when"
				" pinning in %s\n",
				(uint64_t)block_info->this_inode,
				block_info->block_no, __func__);
			flock(fileno(block_fptr), LOCK_UN);
			fclose(block_fptr);
			return;

		} else { /* Strange.. */
			write_log(5, "block_%"PRIu64"_%lld has status %d when"
				" pinning in %s\n",
				(uint64_t)block_info->this_inode,
				block_info->block_no, ret, __func__);
			goto thread_error;
		}

	}

	/* Update status */
	ret = _modify_block_status(block_info, ST_CtoL, ST_BOTH,
				blockstat.st_size);
	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(5, "Fail to modify block status in %s because"
				" meta is removed. Code %d", __func__, ret);
		else
			write_log(0, "Error: Fail to modify block status in %s."
				" Code %d", __func__, ret);
		goto thread_error;
	} else if (ret > 0) {
		/* Status does not match ST_CtoL. It may be truncated */
		flock(fileno(block_fptr), LOCK_UN);
		fclose(block_fptr);
		return;
	}

	flock(fileno(block_fptr), LOCK_UN);
	fclose(block_fptr);

	ret = super_block_mark_dirty(block_info->this_inode);
	if (ret < 0)
		block_info->dl_error = TRUE;

	return;

thread_error:
	block_info->dl_error = TRUE;
	flock(fileno(block_fptr), LOCK_UN);
	fclose(block_fptr);
	return;
}

static inline int _select_thread()
{
	int count;
	for (count = 0; count < MAX_PIN_DL_CONCURRENCY; count++) {
		if (download_thread_ctl.block_info[count].active == FALSE)
			break;
	}
	write_log(10, "Debug: Using downloading thread %d\n", count);
	return count;
}

static int _check_fetch_block(const char *metapath, FILE *fptr,
	ino_t inode, long long blkno, long long page_pos)
{
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE entry_page;
	BLOCK_ENTRY *temp_entry;
	int e_index;
	int which_th;
	int ret, errcode;
	size_t ret_size;

	e_index = blkno % MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(fptr), LOCK_EX);
	if (access(metapath, F_OK) < 0) {
		write_log(0, "Error: %s is removed when pinning.", metapath);
		return -ENOENT;
	}

	/* Check pin-status. It may be modified to UNPIN by other processes */
	FSEEK(fptr, sizeof(struct stat), SEEK_SET);
	FREAD(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	if (filemeta.local_pin == FALSE) {
		flock(fileno(fptr), LOCK_UN);
		write_log(5, "Warning: Inode %"PRIu64" is detected to be "
			"unpinned in pin-process.\n", (uint64_t)inode);
		return -EPERM;
	}

	/* Re-load block entry page */
	FSEEK(fptr, page_pos, SEEK_SET);
	FREAD(&entry_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	flock(fileno(fptr), LOCK_UN);

	temp_entry = &(entry_page.block_entries[e_index]);
	if (temp_entry->status == ST_CLOUD) {
		/* Create thread to download */
		sem_wait(&(download_thread_ctl.dl_th_sem));
		sem_wait(&(download_thread_ctl.ctl_op_sem));
		which_th = _select_thread();
		download_thread_ctl.block_info[which_th].this_inode = inode;
		download_thread_ctl.block_info[which_th].block_no = blkno;
		download_thread_ctl.block_info[which_th].seqnum = temp_entry->seqnum;
		download_thread_ctl.block_info[which_th].page_pos = page_pos;
		download_thread_ctl.block_info[which_th].dl_error = FALSE;
		download_thread_ctl.block_info[which_th].active = TRUE;
		pthread_create(&(download_thread_ctl.download_thread[which_th]),
				NULL, (void *)&fetch_backend_block,
				(void *)&(download_thread_ctl.block_info[which_th]));

		download_thread_ctl.active_th++;
		sem_post(&(download_thread_ctl.ctl_op_sem));
	}
	/* TODO: How about ST_CtoL? */

	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	return errcode;
}

/**
 * fetch_pinned_blocks
 *
 * Given a inode, arrange those blocks with status ST_CLOUD to be downloaded.
 * When system is going shutdown, it will rapidly stop and return.
 *
 * @param inode The inode number to be pinned.
 *
 * @return 0 on success, otherwise negative error code.
 */
int fetch_pinned_blocks(ino_t inode)
{
	char metapath[300];
	FILE *fptr;
	struct stat tempstat;
	off_t total_size;
	long long total_blocks, blkno;
	long long which_page, current_page, page_pos;
	long long cache_size;
	size_t ret_size;
	FILE_META_TYPE this_meta;
	int ret, ret_code, errcode, t_idx;
	char all_thread_terminate;
	struct timespec time_to_sleep;
	char error_path[200];
	DOWNLOAD_BLOCK_INFO *temp_info;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	fetch_meta_path(metapath, inode);
	fptr = fopen(metapath, "r+");
	if (fptr == NULL) {
		write_log(2, "Cannot open %s in %s\n", metapath, __func__);
		return -ENOENT;
	}

	flock(fileno(fptr), LOCK_EX);
	setbuf(fptr, NULL);
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&tempstat, sizeof(struct stat), 1, fptr);
	if (!S_ISREG(tempstat.st_mode)) { /* It is not a regfile? wtf */
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		return 0;
	}
	/* Do not need to re-load meta in loop because just need to check those
	blocks that existing before setting local_pin = true.*/
	FREAD(&this_meta, sizeof(FILE_META_TYPE), 1, fptr);
	flock(fileno(fptr), LOCK_UN);

	total_size = tempstat.st_size;
	total_blocks = total_size ? ((total_size - 1) / MAX_BLOCK_SIZE + 1) : 0;

	fetch_error_download_path(error_path, inode);
	if (access(error_path, F_OK) == 0) /* Delete error path */
		UNLINK(error_path);
	ret_code = 0;
	current_page = -1;
	write_log(10, "Debug: Begin to check all blocks\n");
	for (blkno = 0 ; blkno < total_blocks ; blkno++) {
		if (access(error_path, F_OK) == 0) { /* Some error happened */
			ret_code = -EIO;
			break;
		}

		if (hcfs_system->system_going_down == TRUE) {
			ret_code = -ESHUTDOWN;
			break;
		}

		get_system_size(&cache_size, NULL);
		if (cache_size >= CACHE_HARD_LIMIT) {
			write_log(0, "Error: Cache space is full.\n");
			ret_code = -ENOSPC;
			break;
		}

		which_page = blkno / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			flock(fileno(fptr), LOCK_EX);
			page_pos = seek_page2(&this_meta, fptr, which_page, 0);
			if (page_pos <= 0) {
				blkno += MAX_BLOCK_ENTRIES_PER_PAGE - 1;
				flock(fileno(fptr), LOCK_UN);
				continue;
			}
			flock(fileno(fptr), LOCK_UN);
			current_page = which_page;
		}
		ret_code = _check_fetch_block(metapath, fptr, inode,
			blkno, page_pos);
		if (ret_code < 0)
			break;
	}

	fclose(fptr);

	/* Wait for all threads */
	all_thread_terminate = FALSE;
	while (all_thread_terminate == FALSE &&
			hcfs_system->system_going_down == FALSE) {
		all_thread_terminate = TRUE;
		sem_wait(&(download_thread_ctl.ctl_op_sem));
		for (t_idx = 0; t_idx < MAX_PIN_DL_CONCURRENCY ; t_idx++) {
			temp_info = &(download_thread_ctl.block_info[t_idx]);
			if ((temp_info->active == TRUE) &&
				(temp_info->this_inode == inode)) {
				all_thread_terminate = FALSE;
				break;
			}
		}
		sem_post(&(download_thread_ctl.ctl_op_sem));
		nanosleep(&time_to_sleep, NULL);
	}

	if (hcfs_system->system_going_down == TRUE) {
		if (access(error_path, F_OK) == 0)
			unlink(error_path);
		return -ESHUTDOWN;
	}
	/* Check cache size again */
	if (ret_code == 0) {
		get_system_size(&cache_size, NULL);
		if (cache_size > CACHE_HARD_LIMIT) {
			write_log(0, "Error: Cache space is full.\n");
			ret_code = -ENOSPC;
		}
	}

	/* Delete error object */
	if (access(error_path, F_OK) == 0) {
		write_log(0, "Error: Fail to pin inode %"PRIu64"\n",
			(uint64_t)inode);
		if (ret_code == 0)
			ret_code = -EIO;
		unlink(error_path);
	}

	/* If meta is removed, don't care errors above */
	if (access(metapath, F_OK) < 0)
		ret_code = -ENOENT;

	return ret_code;

errcode_handle:
	fclose(fptr);
	return errcode;
}

/**
 * Condition to wake the thread up.
 */
static BOOL quota_wakeup()
{
	if ((hcfs_system->system_going_down == TRUE) ||
			(hcfs_system->sync_paused == FALSE))
		return TRUE;
	else
		return FALSE;
}

/**
 * Fetch quota from usermeta on cloud
 *
 * This function aims to download usermeta.json from arkflex backend,
 * and then parse the json file to get quota. Finally backup the usermeta
 * locally and encrypt the file.
 *
 */
void fetch_quota_from_cloud(void *ptr)
{
	int status;
	char objname[100];
	char download_path[256];
	FILE *fptr;
	char *buf;
	int ret, errcode;
	long long quota;
	json_error_t jerror;
	json_t *json_data, *json_quota;

	UNUSED(ptr);
	strncpy(objname, "usermeta.json", 100);
	sprintf(download_path, "%s/new_usermeta", METAPATH);

	/* Download usermeta.json from cloud */
	fptr = NULL;
	while (hcfs_system->system_going_down == FALSE) {
		if (hcfs_system->sync_paused) {
			nonblock_sleep(5, quota_wakeup);
			continue;
		}

		if (access(download_path, F_OK) == 0)
			unlink(download_path);
		fptr = fopen(download_path, "w+");
		if (!fptr) {
			write_log(0, "Error: Fail to open file %s in %s\n",
					download_path, __func__);
			goto errcode_handle;
		}
		setbuf(fptr, NULL);
		flock(fileno(fptr), LOCK_EX);

		status = hcfs_get_object(fptr, objname,
				&(download_usermeta_curl_handle),
				NULL);
		if (200 <= status && status <= 299) {
			break;
		} else if (status == 404) {
			write_log(0, "Error: Usermeta is not found"
					" on cloud.\n");
			goto errcode_handle;
		} else { /* Retry, Perhaps disconnect */
			flock(fileno(fptr), LOCK_UN);
			fclose(fptr);
			unlink(download_path);
			fptr = NULL;
			write_log(5, "Return code %d. Retry fetch"
					" quota later\n", status);
		}
	}

	if (hcfs_system->system_going_down == TRUE)
		goto errcode_handle;

	/* Parse json file */
	FSEEK(fptr, 0, SEEK_SET);
	json_data = NULL;
	json_data = json_loadf(fptr, JSON_DISABLE_EOF_CHECK, &jerror);
	if (!json_data) {
		write_log(0, "Error: Fail to parse json file\n");
		goto errcode_handle;
	}

	json_quota = json_object_get(json_data, "quota");
	if (!json_quota || !json_is_integer(json_quota)) {
		json_delete(json_data);
		write_log(0, "Error: Json file is corrupt\n");
		goto errcode_handle;
	}
	quota = json_integer_value(json_quota);
	if (quota < 0) {
		json_delete(json_data);
		write_log(0, "Error: Quota from cloud is less than zero");
		goto errcode_handle;
	}
	sem_wait(&(hcfs_system->access_sem));
	hcfs_system->systemdata.system_quota = quota;
	sem_post(&(hcfs_system->access_sem));
	write_log(10, "Now system quota is %lld\n",
			hcfs_system->systemdata.system_quota);

	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	unlink(download_path);

	buf = json_dumps(json_data, 0);
	enc_backup_usermeta(buf); /* Backup json usermeta */
	json_delete(json_data);
	free(buf);

	sem_wait(&(download_usermeta_ctl.access_sem));
	download_usermeta_ctl.active = FALSE;
	sem_post(&(download_usermeta_ctl.access_sem));
	return;

errcode_handle:
	if (fptr) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
	}
	unlink(download_path);

	sem_wait(&(download_usermeta_ctl.access_sem));
	download_usermeta_ctl.active = FALSE;
	sem_post(&(download_usermeta_ctl.access_sem));
	return;
}

/**
 * Trigger updating quota from cloud
 *
 * This function creates a thread to update quota and backup it. If backend
 * is NONE, then reject to update quota from cloud. When this thread is active,
 * reject to create again and return -EBUSY. Do not need to join this thread
 * because it is detached thread.
 *
 * @return 0 on success, otherwise negative error code.
 */
int update_quota()
{
	if (CURRENT_BACKEND == NONE) {
		write_log(5, "Cannot trigger updating quota without backend\n");
		return -EPERM;
	}

	sem_wait(&(download_usermeta_ctl.access_sem));
	if (download_usermeta_ctl.active == TRUE) {
		sem_post(&(download_usermeta_ctl.access_sem));
		write_log(5, "Quota thread is running\n");
		return -EBUSY;
	} else {
		download_usermeta_ctl.active = TRUE;
	}

	pthread_attr_init(&(download_usermeta_ctl.thread_attr));
	pthread_attr_setdetachstate(&(download_usermeta_ctl.thread_attr),
			PTHREAD_CREATE_DETACHED);
	pthread_create(&(download_usermeta_ctl.download_usermeta_tid),
			&(download_usermeta_ctl.thread_attr),
			(void *)fetch_quota_from_cloud, NULL);
	sem_post(&(download_usermeta_ctl.access_sem));
	return 0;
}
