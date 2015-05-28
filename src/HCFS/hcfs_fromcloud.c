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
*
**************************************************************************/

#include "hcfs_fromcloud.h"

/*
TODO: error handling for HTTP exceptions
*/
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
#include "hcfscurl.h"
#include "fuseop.h"
#include "global.h"
#include "hfuse_system.h"

/************************************************************************
*
* Function name: fetch_from_cloud
*        Inputs: FILE *fptr, ino_t this_inode, long long block_no
*       Summary: Read block "block_no" of inode "this_inode" from backend,
*                and write to the file pointed by "fptr".
*  Return value: HTTP status code, or negation of error code.
*
*************************************************************************/
int fetch_from_cloud(FILE *fptr, ino_t this_inode, long long block_no)
{
	char objname[1000];
	int status;
	int which_curl_handle;
	char idname[256];

	sprintf(objname, "data_%lld_%lld", this_inode, block_no);
	while (TRUE) {
		sem_wait(&download_curl_sem);
		fseek(fptr, 0, SEEK_SET);
		ftruncate(fileno(fptr), 0);
		sem_wait(&download_curl_control_sem);
		for (which_curl_handle = 0; which_curl_handle <
			MAX_DOWNLOAD_CURL_HANDLE; which_curl_handle++) {
			if (curl_handle_mask[which_curl_handle] == FALSE) {
				curl_handle_mask[which_curl_handle] = TRUE;
				break;
			}
		}
		sem_post(&download_curl_control_sem);
		printf("Debug: downloading using curl handle %d\n",
							which_curl_handle);
		sprintf(idname, "download_thread_%d", which_curl_handle);
		strcpy(download_curl_handles[which_curl_handle].id, idname);
		status = hcfs_get_object(fptr, objname,
				&(download_curl_handles[which_curl_handle]));

		sem_wait(&download_curl_control_sem);
		curl_handle_mask[which_curl_handle] = FALSE;
		sem_post(&download_curl_sem);
		sem_post(&download_curl_control_sem);

/* TODO: Fix handling in retrying. Now will retry for any HTTP error*/

		if ((status >= 200) && (status <= 299))
			break;
	}

	fflush(fptr);
	return status;
}

/************************************************************************
*
* Function name: prefetch_block
*        Inputs: PREFETCH_STRUCT_TYPE *ptr
*       Summary: Prefetch the block specified in "ptr" to local cache.
*  Return value: None
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

	entry_index = ptr->entry_index;
	/*Download from backend */
	fetch_meta_path(thismetapath, ptr->this_inode);
	fetch_block_path(thisblockpath, ptr->this_inode, ptr->block_no);

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		free(ptr);
		return;
	}
	setbuf(metafptr, NULL);

	blockfptr = fopen(thisblockpath, "a+");
	fclose(blockfptr);

	blockfptr = fopen(thisblockpath, "r+");
	setbuf(blockfptr, NULL);
	flock(fileno(blockfptr), LOCK_EX);

	flock(fileno(metafptr), LOCK_EX);
	fseek(metafptr, ptr->page_start_fpos, SEEK_SET);
	fread(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);

	if (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
		((temppage).block_entries[entry_index].status == ST_CtoL)) {
		if ((temppage).block_entries[entry_index].status == ST_CLOUD) {
			(temppage).block_entries[entry_index].status = ST_CtoL;
			fseek(metafptr, ptr->page_start_fpos, SEEK_SET);
			fwrite(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
								metafptr);
			fflush(metafptr);
		}
		flock(fileno(metafptr), LOCK_UN);
		fetch_from_cloud(blockfptr, ptr->this_inode, ptr->block_no);
		/*Do not process cache update and stored_where change if block
			is actually deleted by other ops such as truncate*/
		flock(fileno(metafptr), LOCK_EX);
		fseek(metafptr, ptr->page_start_fpos, SEEK_SET);
		fread(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
		if (stat(thisblockpath, &tempstat) == 0) {
			(temppage).block_entries[entry_index].status = ST_BOTH;
			fsetxattr(fileno(blockfptr), "user.dirty", "F", 1, 0);
			fseek(metafptr, ptr->page_start_fpos, SEEK_SET);
			fwrite(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
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
}