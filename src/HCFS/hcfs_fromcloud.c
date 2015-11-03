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
