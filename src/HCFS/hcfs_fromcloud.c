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
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <curl/curl.h>

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

#if (DEDUP_ENABLE)
	/* Get objname by obj_id */
	obj_id_to_string(obj_id, obj_id_str);
	sprintf(objname, "data_%s", obj_id_str);
#elif ARM_32bit_
	sprintf(objname, "data_%lld_%lld_0", this_inode, block_no);
#else
	sprintf(objname, "data_%ld_%lld_0", this_inode, block_no);
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
	FILE *get_fptr = open_memstream(&get_fptr_data, &len);

	status = hcfs_get_object(get_fptr, objname,
				 &(download_curl_handles[which_curl_handle]));

	fclose(get_fptr);
	unsigned char *key = NULL;
#if ENCRYPT_ENABLE
	key = get_key();
#endif

	decode_to_fd(fptr, key, (unsigned char *)get_fptr_data, len,
		     ENCRYPT_ENABLE, COMPRESS_ENABLE);

	free(get_fptr_data);
	if (key != NULL)
		OPENSSL_free(key);

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
		ret = fetch_from_cloud(blockfptr,
				temppage.block_entries[entry_index].obj_id);
#else
		ret = fetch_from_cloud(blockfptr,
				ptr->this_inode, ptr->block_no);
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
			fsetxattr(fileno(blockfptr), "user.dirty", "F", 1, 0);
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

/**
 * Download meta from backend
 *
 * @inode Inode number of downloaded meta
 * @download_metapath Path that place the downloaded meta object
 * @backend_fptr Address of file pointer used to open downloaded meta
 *
 * This function is now used in sync_single_inode() of hcfs_tocloud.c, which
 * chooses a usable thread in download_curl_handles[] and then download the
 * meta of given inode number. If succeed to download the meta, "backend_fptr"
 * will be a file pointer of "download_metapath" with w+ flag. Otherwise
 * "backend_fptr" will be NULL.
 *
 * @return 0 if succeed to download or object not found. Otherwise -EIO on error
 */
int download_meta_from_backend(ino_t inode, const char *download_metapath,
	FILE **backend_fptr)
{
	char backend_meta_name[500];
	int ret, errcode;
	int curl_idx;

	fetch_backend_meta_objname(inode, backend_meta_name);

	*backend_fptr = fopen(download_metapath, "w+");
	if (*backend_fptr == NULL) {
		write_log(0, "Error: Fail to open file in %s\n", __func__);
		return -1;
	}
	setbuf(*backend_fptr, NULL);

	sem_wait(&download_curl_sem);
	sem_wait(&download_curl_control_sem);
	for (curl_idx = 0; curl_idx < MAX_DOWNLOAD_CURL_HANDLE; curl_idx++)
		if (curl_handle_mask[curl_idx] == FALSE)
			break;
	curl_handle_mask[curl_idx] = TRUE;
	sem_post(&download_curl_control_sem);


#ifdef ENCRYPT_ENABLE
	char  *get_fptr_data = NULL;
	size_t len = 0;
	FILE *get_fptr = open_memstream(&get_fptr_data, &len);

	ret = hcfs_get_object(get_fptr, backend_meta_name,
		&(download_curl_handles[curl_idx]));
#else
	ret = hcfs_get_object(*backend_fptr, backend_meta_name,
		&(download_curl_handles[curl_idx]));
#endif
#ifdef ENCRYPT_ENABLE
	fclose(get_fptr);
	unsigned char *key = get_key();
	decrypt_to_fd(*backend_fptr, key, get_fptr_data, len);
	free(get_fptr_data);
	free(key);
#endif
	sem_wait(&download_curl_control_sem);
	curl_handle_mask[curl_idx] = FALSE;
	sem_post(&download_curl_control_sem);

	sem_post(&download_curl_sem);

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
