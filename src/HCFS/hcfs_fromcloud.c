/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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

/************************************************************************
*
* Function name: fetch_from_cloud
*        Inputs: FILE *fptr, ino_t this_inode, long long block_no
*       Summary: Read block "block_no" of inode "this_inode" from backend,
*                and write to the file pointed by "fptr".
*  Return value: 0 if successful, or negation of error code.
*
*************************************************************************/
int fetch_from_cloud(FILE *fptr,
#if (DEDUP_ENABLE)
		     unsigned char *obj_id)
#else
		     ino_t this_inode, long long block_no)
#endif
{
	char objname[1000];
	char obj_id_str[OBJID_STRING_LENGTH];
	int status;
	int which_curl_handle;
	int ret, errcode;
	long tmplen;

#if (DEDUP_ENABLE)
	/* Get objname by obj_id */
	obj_id_to_string(obj_id, obj_id_str);
	sprintf(objname, "data_%s", obj_id_str);
#else
	sprintf(objname, "data_%"FMT_INO_T"_%lld", this_inode, block_no);
#endif

	sem_wait(&download_curl_sem);
	FSEEK(fptr, 0, SEEK_SET);
	FTRUNCATE(fileno(fptr), 0);

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

#if defined(__ANDROID__) || defined(_ANDROID_ENV_)
	fseek(get_fptr, 0, SEEK_END);
	tmplen = ftell(get_fptr);
	get_fptr_data = calloc(tmplen + 10, sizeof(unsigned char));
	rewind(get_fptr);
	len =
	    fread(get_fptr_data, sizeof(unsigned char), tmplen, get_fptr);
#endif

	fclose(get_fptr);
	unsigned char *object_key = NULL;
#if ENCRYPT_ENABLE
	unsigned char *key = get_key();
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

	sem_wait(&download_curl_control_sem);
	curl_handle_mask[which_curl_handle] = FALSE;
	sem_post(&download_curl_sem);
	sem_post(&download_curl_control_sem);

	/* Already retried in get object if necessary */
	if ((status >= 200) && (status <= 299))
		ret = 0;
	else
		ret = -EIO;

	fflush(fptr);
	return status;

errcode_handle:
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
		ret = fetch_from_cloud(
		    blockfptr, temppage.block_entries[entry_index].obj_id);
#else
		ret =
		    fetch_from_cloud(blockfptr, ptr->this_inode, ptr->block_no);
#endif
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
				errcode = ret;
				goto errcode_handle;
			}
			FSEEK(metafptr, ptr->page_start_fpos, SEEK_SET);
			FWRITE(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
			       metafptr);
			fflush(metafptr);

			sem_wait(&(hcfs_system->access_sem));
			hcfs_system->systemdata.cache_size += tempstat.st_size;
			hcfs_system->systemdata.cache_blocks++;
			sync_hcfs_system_data(FALSE);
			sem_post(&(hcfs_system->access_sem));
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

static void *_fetch_block(void *ptr)
{
	char block_path[400];
	long long blkno;
	FILE *block_ptr;
	ino_t inode;
	DOWNLOAD_BLOCK_INFO *block_info;
	int ret;


	block_info = (DOWNLOAD_BLOCK_INFO *)ptr;
	fetch_block_path(block_path, block_info->this_inode,
						block_info->block_no);
	//if (access(block_path, F_OK) == 0) {
		/* It is downloaded by another thread... */
	//}

	block_ptr = fopen(block_path, "w+"); /* create or erase it */
	if (block_ptr == NULL) {
		write_log(0, "Error: Fail to open block path %s in %s\n",
							block_path, __func__);
		block_info->dl_error = TRUE;
		return;
	}

	flock(block_ptr, LOCK_EX);
	setbuf(block_ptr, NULL);

	ret = fetch_from_cloud(block_ptr, block_info->this_inode,
						block_info->block_no);
	if (ret < 0) {
		write_log(0, "Error: Fail to fetch block in %s\n", __func__);
		block_info->dl_error = TRUE;
		return;
	}
	flock(block_ptr, LOCK_UN);
	fclose(block_ptr);

	return;
}

static inline int _select_thread()
{
	int count;
	for (count = 0; count < MAX_DL_CONCURRENCY; count++) {
		if (download_thread_ctl.block_info[count].this_inode == 0)
			break;
	}
	return count;
}

static int _check_fetch_block(const char *mathpath, FILE *fptr,
	ino_t inode, long long blkno, long long page_pos)
{
	BLOCK_ENTRY_PAGE entry_page;
	BLOCK_ENTRY *temp_entry;
	int e_index;
	int which_th;

	e_index = blkno % MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(fptr), LOCK_EX);
	if (access(metapath, F_OK) < 0) {
		write_log(10, "Debug: %s is removed when pinning.");
		return -ENOENT;
	}

	/* Re-load block entry page */
	FSEEK(fptr, page_pos, SEEK_SET);
	FREAD(&entry_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	temp_entry = &(entry_page.block_entries[e_index]);

	if (temp_entry->status == ST_CLOUD) {
		temp_entry->status = ST_CtoL;
		FSEEK(fptr, page_pos, SEEK_SET);
		FWRITE(&entry_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		flock(fileno(fptr), LOCK_UN);

		/* Create thread to download */
		sem_wait(&dl_th_sem);
		sem_wait(&ctl_op_sem);
		which_th = _select_thread();
		download_thread_ctl.block_info[which_th].this_inode = inode;
		download_thread_ctl.block_info[which_th].block_no = blkno;
		download_thread_ctl.block_info[which_th].page_start_fpos =
								page_pos;
		download_thread_ctl.block_info[which_th].dl_error = FALSE;
		pthread_create(&(download_thread[which_th]),
			NULL, (void *)&_fetch_block, NULL);

		download_thread_ctl.active_th++;
		sem_post(&ctl_op_sem);
	}
	/* TODO: How about ST_CtoL? */

	return 0;
}

int fetch_pinned_blocks(ino_t inode)
{
	char metapath[300];
	FILE *fptr;
	struct stat tempstat;
	off_t total_size;
	long long total_blocks, blkno;
	long long which_page, current_page, page_pos;
	FILE_META_TYPE this_meta;
	int ret, ret_code;

	fetch_meta_path(metapath, inode);
	fptr = fopen(metapath, "r+");
	if (fptr == NULL) {
		write_log(2, "Cannot open %s in %s\n", metapath, __func__);
		return 0;
	}

	flock(fileno(fptr), LOCK_EX);
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&tempstat, sizeof(struct stat), 1, fptr);
	/* Do not need to re-load meta in loop because just need to check those
	blocks that existing before setting local_pin = true.*/
	FREAD(&this_meta, sizeof(FILE_META_TYPE), fptr);
	flock(fileno(fptr), LOCK_UN);

	total_size = tempstat.st_size;
	total_blocks = total_size ? ((total_size - 1) / MAX_BLOCK_SIZE + 1) : 0;

	ret_code = 0;
	current_page = -1;
	for (blkno = 0 ; blkno < total_blocks ; blkno++) {
		if (access(metapath, F_OK) < 0) {
			write_log(10, "Debug: %s is removed when pinning.");
			ret_code = -ENOENT;
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
		ret_code = _check_fetch_block(mathpath, fptr, inode, blkno, page_pos);
		if (ret_code < 0)
			break;

	}

	fclose(fptr);
	return ret_code;
}
