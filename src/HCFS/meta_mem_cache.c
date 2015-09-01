/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: meta_mem_cache.c
* Abstract: The c source code file for meta cache operations in HCFS.
*
* Revision History
* 2015/2/9 Jiahong added header for this file, and revising coding style.
* 2015/7/8 Kewei added meta cache processing about symlink.
*
**************************************************************************/
#include "meta_mem_cache.h"

#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "global.h"
#include "params.h"
#include "super_block.h"
#include "dir_entry_btree.h"
#include "macro.h"
#include "logger.h"
#include "utils.h"

/* If cache lock not locked, return -1*/
#define _ASSERT_CACHE_LOCK_IS_LOCKED_(ptr_sem) \
	{ \
		int sem_val; \
		sem_getvalue((ptr_sem), &sem_val); \
		if (sem_val > 0) \
			return -1; \
	}
/* TODO: Consider whether want to use write-back mode for meta caching */

META_CACHE_HEADER_STRUCT *meta_mem_cache;
sem_t num_entry_sem;
long current_meta_mem_cache_entries;

/* Helper function for opening meta file if not already opened */
static inline int _open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret_val = 0;

	if ((body_ptr->meta_opened == FALSE) || (body_ptr->fptr == NULL))
		ret_val = meta_cache_open_file(body_ptr);
	return ret_val;
}

/* Helper function for reading a directory entry page from meta file */
static inline int _load_dir_page(META_CACHE_ENTRY_STRUCT *ptr,
					DIR_ENTRY_PAGE *dir_page)
{
	int ret, errcode;
	size_t ret_size;

	FSEEK(ptr->fptr, dir_page->this_page_pos, SEEK_SET);
	FREAD((ptr->dir_entry_cache[0]), sizeof(DIR_ENTRY_PAGE), 1, ptr->fptr);

	memcpy(dir_page, (ptr->dir_entry_cache[0]), sizeof(DIR_ENTRY_PAGE));

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: meta_cache_open_file
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Open the meta file for the cache entry
*                pointed by "body_ptr".
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	char thismetapath[METAPATHLEN];
	int ret, errcode;

	thismetapath[0] = '\0';
	if ((body_ptr->meta_opened == FALSE) || (body_ptr->fptr == NULL)) {
		ret = fetch_meta_path(thismetapath, body_ptr->inode_num);
		if (ret < 0)
			return ret;

		body_ptr->fptr = fopen(thismetapath, "r+");
		if (body_ptr->fptr == NULL) {
			/*File may not exist*/
			errcode = errno;
			if (access(thismetapath, F_OK) < 0) {
				errcode = errno;
				if (errcode != ENOENT)
					return -errcode;
				body_ptr->fptr = fopen(thismetapath, "w+");
				if (body_ptr->fptr == NULL) {
					errcode = errno;
					write_log(0,
						"IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
					return -errcode;
				}
			} else {
				write_log(0, "IO error in %s. Code %d, %s\n",
						__func__, errcode,
						strerror(errcode));
				return -errcode;
			}
		}

		setbuf(body_ptr->fptr, NULL);
		flock(fileno(body_ptr->fptr), LOCK_EX);
		body_ptr->meta_opened = TRUE;
	}
	return 0;
}

/************************************************************************
*
* Function name: init_meta_cache_headers
*        Inputs: None
*       Summary: Initialize meta cache.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_meta_cache_headers(void)
{
	int count;
	int ret, errcode;

	meta_mem_cache = malloc(sizeof(META_CACHE_HEADER_STRUCT) *
						NUM_META_MEM_CACHE_HEADERS);

	if (meta_mem_cache == NULL)
		return -ENOMEM;

	memset(meta_mem_cache, 0, sizeof(META_CACHE_HEADER_STRUCT) *
						NUM_META_MEM_CACHE_HEADERS);

	current_meta_mem_cache_entries = 0;

	ret = sem_init(&num_entry_sem, 0, 1);
	if (ret < 0) {
		errcode = errno;
		goto errcode_handle;
	}
	for (count = 0; count < NUM_META_MEM_CACHE_HEADERS; count++) {
		ret = sem_init(&(meta_mem_cache[count].header_sem), 0, 1);
		if (ret < 0) {
			errcode = errno;
			goto errcode_handle;
		}
		meta_mem_cache[count].meta_cache_entries = NULL;
		meta_mem_cache[count].last_entry = NULL;
	}

	return 0;

errcode_handle:
	write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
	errcode = -errcode;
	free(meta_mem_cache);
	return errcode;
}

/************************************************************************
*
* Function name: release_meta_cache_headers
*        Inputs: None
*       Summary: Sync dirty meta cache entry and release resource.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int release_meta_cache_headers(void)
{
	int ret_val;

	if (meta_mem_cache == NULL)
		return -EPERM;

	ret_val = flush_clean_all_meta_cache();

	current_meta_mem_cache_entries = 0;

	free(meta_mem_cache);
	return ret_val;
}

/* Helper function for caching a new dir entry page and dropping the last one */
static int _push_page_entry(META_CACHE_ENTRY_STRUCT *body_ptr,
					const DIR_ENTRY_PAGE *temppage)
{
	body_ptr->dir_entry_cache_dirty[1] = body_ptr->dir_entry_cache_dirty[0];
	memcpy(body_ptr->dir_entry_cache[1], body_ptr->dir_entry_cache[0],
						sizeof(DIR_ENTRY_PAGE));
	memcpy((body_ptr->dir_entry_cache[0]), temppage,
						sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache_dirty[0] = TRUE;
	return 0;
}

/************************************************************************
*
* Function name: meta_cache_push_dir_page
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr,
*                const DIR_ENTRY_PAGE *temppage
*       Summary: Cache a new dir entry page (temppage) for this meta cache
*                entry (body_ptr), and drop the last one (with index 1).
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_push_dir_page(META_CACHE_ENTRY_STRUCT *body_ptr,
				const DIR_ENTRY_PAGE *temppage)
{
	int ret;

	ret = 0;
	if (body_ptr->dir_entry_cache[0] == NULL) {
		body_ptr->dir_entry_cache[0] = malloc(sizeof(DIR_ENTRY_PAGE));
		if (body_ptr->dir_entry_cache[0] == NULL)
			return -ENOMEM;
		memcpy((body_ptr->dir_entry_cache[0]), temppage,
							sizeof(DIR_ENTRY_PAGE));
		body_ptr->dir_entry_cache_dirty[0] = TRUE;
	} else {
		/* If the second page entry is empty,
			allocate mem space first */
		if (body_ptr->dir_entry_cache[1] == NULL) {
			body_ptr->dir_entry_cache[1] =
						malloc(sizeof(DIR_ENTRY_PAGE));
			if (body_ptr->dir_entry_cache[1] == NULL)
				return -ENOMEM;
			ret = _push_page_entry(body_ptr, temppage);
		} else {
			/* Need to flush first */
			ret = meta_cache_flush_dir_cache(body_ptr, 1);
			if (ret < 0)
				return ret;
			ret = _push_page_entry(body_ptr, temppage);
		}
	}
	return ret;
}

/************************************************************************
*
* Function name: meta_cache_flush_dir_cache
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, int eindex
*       Summary: Write the content of the cached dir entry page (stored in
*                index "eindex") of this meta cache entry (body_ptr) to the
*                meta file.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_flush_dir_cache(META_CACHE_ENTRY_STRUCT *body_ptr, int eindex)
{
	int ret, errcode;
	size_t ret_size;
	/*Assume meta cache entry access sem is already locked*/

	ret = _open_file(body_ptr);

	if (ret < 0)
		return ret;

	FSEEK(body_ptr->fptr,
		(body_ptr->dir_entry_cache[eindex])->this_page_pos, SEEK_SET);
	FWRITE(body_ptr->dir_entry_cache[eindex], sizeof(DIR_ENTRY_PAGE),
							1, body_ptr->fptr);

	ret = super_block_mark_dirty((body_ptr->this_stat).st_ino);

	return ret;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/* Helper function for syncing content of cached dir entry page to meta file
	if it is dirty. */
static inline int _cache_sync(META_CACHE_ENTRY_STRUCT *body_ptr, int index)
{
	int ret, errcode;
	size_t ret_size;

	if ((body_ptr->dir_entry_cache_dirty[index] == TRUE) &&
				(body_ptr->dir_entry_cache[index] != NULL)) {
		FSEEK(body_ptr->fptr,
			(body_ptr->dir_entry_cache[index])->this_page_pos,
								SEEK_SET);
		FWRITE(body_ptr->dir_entry_cache[index], sizeof(DIR_ENTRY_PAGE),
							1, body_ptr->fptr);
		body_ptr->dir_entry_cache_dirty[index] = FALSE;
	}
	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: flush_single_entry
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Write the dirty content of the meta cache entry (body_ptr)
*                to the meta file.
*  Return value: 0 if successful. Otherwise returns negation of errcode.
*
*************************************************************************/
int flush_single_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret, errcode;
	size_t ret_size;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	if (body_ptr->something_dirty == FALSE)
		return 0;

	ret = _open_file(body_ptr);

	if (ret < 0)
		return ret;

	/* Sync struct stat */
	if (body_ptr->stat_dirty == TRUE) {
		FSEEK(body_ptr->fptr, 0, SEEK_SET);
		FWRITE(&(body_ptr->this_stat), sizeof(struct stat), 1,
							body_ptr->fptr);
		body_ptr->stat_dirty = FALSE;
	}

	/* Sync meta */
	/* TODO Right now, may not set meta_dirty to TRUE if only changes
			pages */
	if (body_ptr->meta_dirty == TRUE) {
		if (S_ISREG(body_ptr->this_stat.st_mode)) {
			FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
			FWRITE((body_ptr->file_meta), sizeof(FILE_META_TYPE),
							1, body_ptr->fptr);
		} else {
			if (S_ISDIR(body_ptr->this_stat.st_mode)) {
				FSEEK(body_ptr->fptr, sizeof(struct stat),
							SEEK_SET);
				FWRITE((body_ptr->dir_meta),
						sizeof(DIR_META_TYPE),
							1, body_ptr->fptr);
			}
			if (S_ISLNK(body_ptr->this_stat.st_mode)) {
				FSEEK(body_ptr->fptr, sizeof(struct stat),
					SEEK_SET);
				FWRITE((body_ptr->symlink_meta),
					sizeof(SYMLINK_META_TYPE), 1,
					body_ptr->fptr);
			}
		}

		body_ptr->meta_dirty = FALSE;
	}

	if (S_ISDIR(body_ptr->this_stat.st_mode)) {
		ret = _cache_sync(body_ptr, 0);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
		ret = _cache_sync(body_ptr, 1);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	/* Update stat info in super inode no matter what so that meta file
		get pushed to cloud */
	/* TODO: May need to simply this so that only dirty status in super
		inode is updated */
	ret = super_block_update_stat(body_ptr->inode_num,
				&(body_ptr->this_stat));
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	body_ptr->something_dirty = FALSE;

	return 0;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: flush_clean_all_meta_cache
*        Inputs: None
*       Summary: Flush all dirty entries and free memory usage for meta cache.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int flush_clean_all_meta_cache(void)
{
	int count;
	META_CACHE_LOOKUP_ENTRY_STRUCT *this_ptr, *old_ptr;

	for (count = 0; count < NUM_META_MEM_CACHE_HEADERS; count++) {
		sem_wait(&(meta_mem_cache[count].header_sem));
		if (meta_mem_cache[count].num_entries <= 0) {
			sem_post(&(meta_mem_cache[count].header_sem));
			continue;
		}
		this_ptr = meta_mem_cache[count].meta_cache_entries;
		while (this_ptr != NULL) {
			sem_wait(&((this_ptr->body).access_sem));
			if ((this_ptr->body).something_dirty == TRUE)
				flush_single_entry(&(this_ptr->body));
			if ((this_ptr->body).meta_opened == TRUE) {
				flock(fileno((this_ptr->body).fptr),
							LOCK_UN);
				fclose((this_ptr->body).fptr);
				(this_ptr->body).meta_opened = FALSE;
			}

			free_single_meta_cache_entry(this_ptr);
			old_ptr = this_ptr;
			this_ptr = this_ptr->next;
			sem_post(&((old_ptr->body).access_sem));
			free(old_ptr);
			sem_wait(&num_entry_sem);
			current_meta_mem_cache_entries--;
			sem_post(&num_entry_sem);
		}
		meta_mem_cache[count].meta_cache_entries = NULL;
		meta_mem_cache[count].num_entries = 0;
		meta_mem_cache[count].last_entry = NULL;
		sem_post(&(meta_mem_cache[count].header_sem));
	}
	return 0;
}

/************************************************************************
*
* Function name: free_single_meta_cache_entry
*        Inputs: None
*       Summary: Free memory usage for a single meta cache entry.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int free_single_meta_cache_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *entry_ptr)
{
	META_CACHE_ENTRY_STRUCT *entry_body;

	entry_body = &(entry_ptr->body);

	if (entry_body->dir_meta != NULL)
		free(entry_body->dir_meta);
	if (entry_body->file_meta != NULL)
		free(entry_body->file_meta);
	if (entry_body->symlink_meta != NULL)
		free(entry_body->symlink_meta);

	if (entry_body->dir_entry_cache[0] != NULL)
		free(entry_body->dir_entry_cache[0]);
	if (entry_body->dir_entry_cache[1] != NULL)
		free(entry_body->dir_entry_cache[1]);
	if (entry_body->meta_opened) {
		if (entry_body->fptr != NULL)
			fclose(entry_body->fptr);
		entry_body->meta_opened = FALSE;
	}

	return 0;
}

/* Helper function for computing the hash index of an inode number to
	the meta cache */
static inline int hash_inode_to_meta_cache(ino_t this_inode)
{
	return ((int) this_inode % NUM_META_MEM_CACHE_HEADERS);
}

/************************************************************************
*
* Function name: meta_cache_update_file_data
*        Inputs: ino_t this_inode, struct stat *inode_stat,
*                FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
*                long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Update the cache content for inode "this_inode". Content
*                entries not updated are passed as "NULL". If write through
*                cache is enabled (the only option now), will also write
*                changes to meta file.
*                This function handles regular files only.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
/* Always change dirty status to TRUE here as we always update */
/* For block entry page lookup or update, only allow one lookup/update at a
time, and will check page_pos input against the two entries in the cache.
If does not match any of the two, flush the older page entry first before
processing the new one */

	int ret, errcode;
	size_t ret_size;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	if (inode_stat != NULL) {
		memcpy(&(body_ptr->this_stat), inode_stat, sizeof(struct stat));
		body_ptr->stat_dirty = TRUE;
	}

	if (file_meta_ptr != NULL) {
		if (body_ptr->file_meta == NULL) {
			body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));
			if (body_ptr->file_meta == NULL)
				return -ENOMEM;
		}
		memcpy((body_ptr->file_meta), file_meta_ptr,
						sizeof(FILE_META_TYPE));
		body_ptr->meta_dirty = TRUE;
	}

	if (block_page != NULL) {
		ret = _open_file(body_ptr);

		if (ret < 0)
			return ret;

		FSEEK(body_ptr->fptr, page_pos, SEEK_SET);
		FWRITE(block_page, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);

		ret = super_block_mark_dirty((body_ptr->this_stat).st_ino);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	gettimeofday(&(body_ptr->last_access_time), NULL);

	if (body_ptr->something_dirty == FALSE)
		body_ptr->something_dirty = TRUE;

	/* Write changes to meta file if write through is enabled */
	if (META_CACHE_FLUSH_NOW == TRUE) {
		ret = flush_single_entry(body_ptr);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	return 0;

/* Exception handling from here */
errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: meta_cache_lookup_file_data
*        Inputs: ino_t this_inode, struct stat *inode_stat,
*                FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
*                long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Read the cache content for inode "this_inode". Content
*                entries not required are passed as "NULL". Will read from
*                meta file if there is no cached content for the requested
*                meta.
*                This function handles regular files only.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
		long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret, errcode;
	size_t ret_size;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	if (inode_stat != NULL)
		memcpy(inode_stat, &(body_ptr->this_stat), sizeof(struct stat));

	if (file_meta_ptr != NULL) {
		if (body_ptr->file_meta == NULL) {
			body_ptr->file_meta = malloc(sizeof(FILE_META_TYPE));
			if (body_ptr->file_meta == NULL)
				return -ENOMEM;
			ret = _open_file(body_ptr);

			if (ret < 0)
				return ret;

			FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
			FREAD(body_ptr->file_meta, sizeof(FILE_META_TYPE),
							1, body_ptr->fptr);
		}
		memcpy(file_meta_ptr, body_ptr->file_meta,
						sizeof(FILE_META_TYPE));
	}

	if (block_page != NULL) {
		ret = _open_file(body_ptr);

		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		FSEEK(body_ptr->fptr, page_pos, SEEK_SET);
		FREAD(block_page, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
	}

	gettimeofday(&(body_ptr->last_access_time), NULL);

	return 0;

/* Exception handling from here */
errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/* Helper function for looking up dir entry pages from meta files */
static inline int _lookup_dir_load_page(META_CACHE_ENTRY_STRUCT *ptr,
					DIR_ENTRY_PAGE *dir_page)
{
	off_t tmp_fpos;
	int ret, errcode;
	size_t ret_size;

	if (ptr->dir_entry_cache[0] == NULL) {
		ptr->dir_entry_cache[0] =
			malloc(sizeof(DIR_ENTRY_PAGE));
		if (ptr->dir_entry_cache[0] == NULL)
			return -ENOMEM;
		ret = _load_dir_page(ptr, dir_page);
		return ret;
	}
	if (ptr->dir_entry_cache[1] == NULL) {
		ptr->dir_entry_cache[1] =
			malloc(sizeof(DIR_ENTRY_PAGE));
		if (ptr->dir_entry_cache[1] == NULL)
			return -ENOMEM;
		ptr->dir_entry_cache_dirty[1] =
			ptr->dir_entry_cache_dirty[0];
		memcpy(ptr->dir_entry_cache[1], ptr->dir_entry_cache[0],
					sizeof(DIR_ENTRY_PAGE));

		ret = _load_dir_page(ptr, dir_page);
		return ret;
	}
	/* Need to flush first */
	if (ptr->dir_entry_cache_dirty[1] == TRUE) {
		tmp_fpos = (ptr->dir_entry_cache[1])->
						this_page_pos;
		FSEEK(ptr->fptr, tmp_fpos, SEEK_SET);
		FWRITE(ptr->dir_entry_cache[1],
			sizeof(DIR_ENTRY_PAGE), 1, ptr->fptr);
		ret = super_block_mark_dirty((ptr->this_stat).st_ino);
		if (ret < 0)
			return ret;
	}

	memcpy(ptr->dir_entry_cache[1], ptr->dir_entry_cache[0],
					sizeof(DIR_ENTRY_PAGE));
	ptr->dir_entry_cache_dirty[1] =
				ptr->dir_entry_cache_dirty[0];

	ret = _load_dir_page(ptr, dir_page);

	return ret;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: meta_cache_lookup_dir_data
*        Inputs: ino_t this_inode, struct stat *inode_stat,
*                DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
*                META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Read the cache content for inode "this_inode". Content
*                entries not required are passed as "NULL". Will read from
*                meta file if there is no cached content for the requested
*                meta.
*                This function handles directories only.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
				META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret, errcode;
	size_t ret_size;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	if (inode_stat != NULL)
		memcpy(inode_stat, &(body_ptr->this_stat), sizeof(struct stat));

	if (dir_meta_ptr != NULL) {
		if (body_ptr->dir_meta == NULL) {
			body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));
			if (body_ptr->dir_meta == NULL)
				return -ENOMEM;

			ret = _open_file(body_ptr);

			if (ret < 0)
				return ret;

			FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
			FREAD(body_ptr->dir_meta, sizeof(DIR_META_TYPE),
							1, body_ptr->fptr);
		}

		memcpy(dir_meta_ptr, body_ptr->dir_meta, sizeof(DIR_META_TYPE));
	}

	if (dir_page != NULL) {
		if ((body_ptr->dir_entry_cache[0] != NULL) &&
			((body_ptr->dir_entry_cache[0])->this_page_pos ==
						dir_page->this_page_pos)) {
			memcpy(dir_page, (body_ptr->dir_entry_cache[0]),
						sizeof(DIR_ENTRY_PAGE));
		} else {
			if ((body_ptr->dir_entry_cache[1] != NULL) &&
				((body_ptr->dir_entry_cache[1])->this_page_pos
						== dir_page->this_page_pos)) {
				/* TODO: consider swapping entries 0 and 1 */
				memcpy(dir_page, (body_ptr->dir_entry_cache[1]),
						sizeof(DIR_ENTRY_PAGE));
			} else {
				/* Cannot find the requested page in cache */
				ret = _open_file(body_ptr);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}

				ret = _lookup_dir_load_page(body_ptr, dir_page);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}

			}
		}
	}

	gettimeofday(&(body_ptr->last_access_time), NULL);

	return 0;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: meta_cache_update_dir_data
*        Inputs: ino_t this_inode, struct stat *inode_stat,
*                DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
*                META_CACHE_ENTRY_STRUCT *bptr
*       Summary: Update the cache content for inode "this_inode". Content
*                entries not updated are passed as "NULL". If write through
*                cache is enabled (the only option now), will also write
*                changes to meta file.
*                This function handles directories only.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
	const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *bptr)
{
/*Always change dirty status to TRUE here as we always update*/
/*For dir entry page lookup or update, only allow one lookup/update at a time,
and will check page_pos input against the two entries in the cache. If does
not match any of the two, flush the older page entry first before processing
the new one */

	int ret;

	write_log(10, "Debug meta cache update dir data\n");

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(bptr->access_sem));

	if (inode_stat != NULL) {
		memcpy(&(bptr->this_stat), inode_stat, sizeof(struct stat));
		bptr->stat_dirty = TRUE;
	}

	if (dir_meta_ptr != NULL) {
		if (bptr->dir_meta == NULL) {
			bptr->dir_meta = malloc(sizeof(DIR_META_TYPE));
			if (bptr->dir_meta == NULL)
				return -ENOMEM;
		}
		memcpy((bptr->dir_meta), dir_meta_ptr,
						sizeof(DIR_META_TYPE));
		bptr->meta_dirty = TRUE;
	}

	if (dir_page != NULL) {
		if ((bptr->dir_entry_cache[0] != NULL) &&
			((bptr->dir_entry_cache[0])->this_page_pos ==
					dir_page->this_page_pos)) {
			memcpy((bptr->dir_entry_cache[0]),
					dir_page, sizeof(DIR_ENTRY_PAGE));
			bptr->dir_entry_cache_dirty[0] = TRUE;
		} else {
			if ((bptr->dir_entry_cache[1] != NULL) &&
				((bptr->dir_entry_cache[1])->this_page_pos
						== dir_page->this_page_pos)) {
				/* TODO: consider swapping entries 0 and 1 */
				memcpy((bptr->dir_entry_cache[1]), dir_page,
						sizeof(DIR_ENTRY_PAGE));
				bptr->dir_entry_cache_dirty[1] = TRUE;
			} else {
				/* Cannot find the requested page in cache */
				ret = meta_cache_push_dir_page(bptr, dir_page);
				if (ret < 0)
					return ret;
			}
		}
	}

	gettimeofday(&(bptr->last_access_time), NULL);


	if (bptr->something_dirty == FALSE)
		bptr->something_dirty = TRUE;

	/* Write changes to meta file if write through is enabled */
	if (META_CACHE_FLUSH_NOW == TRUE) {
		ret = flush_single_entry(bptr);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/************************************************************************
*
* Function name: meta_cache_seek_dir_entry
*        Inputs: ino_t this_inode, DIR_ENTRY_PAGE *result_page,
*                int *result_index, char *childname,
*                META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Find whether the object with name "childname" is the child
*                of "this_inode". If found, returns the dir entry page
*                containing the child (result_page) and the index of the child
*                in this page (result_index).
*  Return value: If successful, returns 0 and *result_index >= 0. If not found,
*                returns 0 and *result_index < 0. If error, returns negation
*                of error code.
*
*************************************************************************/
int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
	int *result_index, const char *childname,
				META_CACHE_ENTRY_STRUCT *body_ptr)
{
	char thismetapath[METAPATHLEN];
	DIR_META_TYPE dir_meta;
	DIR_ENTRY_PAGE temppage, rootpage, tmp_resultpage;
	DIR_ENTRY_PAGE *tmp_page_ptr;
	int ret, errcode;
	size_t ret_size;
	long nextfilepos;
	int can_use_index;
	DIR_ENTRY tmp_entry;
	int tmp_index;
	int cache_idx;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	/* First check if any of the two cached page entries
	  contains the target entry */
	strcpy(tmp_entry.d_name, childname);
	can_use_index = -1;

	for (cache_idx = 0; cache_idx <= 1; cache_idx++) {
		if (body_ptr->dir_entry_cache[cache_idx] == NULL)
			continue;

		tmp_page_ptr = body_ptr->dir_entry_cache[cache_idx];
		ret = dentry_binary_search(tmp_page_ptr->dir_entries,
			tmp_page_ptr->num_entries, &tmp_entry, &tmp_index);
		if (ret >= 0) {
			*result_index = ret;
			memcpy(result_page, tmp_page_ptr,
				sizeof(DIR_ENTRY_PAGE));
			can_use_index = cache_idx;
			gettimeofday(&(body_ptr->last_access_time), NULL);
			return 0;
		}
	}

	/* Cannot find the empty dir entry in any of the
	two cached page entries. Proceed to search from meta file */

	if (body_ptr->dir_meta == NULL) {
		body_ptr->dir_meta = malloc(sizeof(DIR_META_TYPE));
		if (body_ptr->dir_meta == NULL)
			return -ENOMEM;

		if (body_ptr->meta_opened == FALSE) {
			ret = fetch_meta_path(thismetapath,
					body_ptr->inode_num);
			if (ret < 0)
				return ret;

			body_ptr->fptr = fopen(thismetapath, "r+");
			if (body_ptr->fptr == NULL) {
				errcode = errno;
				write_log(0, "IO error in %s. Code %d, %s\n",
					__func__, errcode, strerror(errcode));
				goto errcode_handle;
			}

			setbuf(body_ptr->fptr, NULL);
			flock(fileno(body_ptr->fptr), LOCK_EX);
			body_ptr->meta_opened = TRUE;
		}
		FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
		FREAD(body_ptr->dir_meta, sizeof(DIR_META_TYPE), 1,
							body_ptr->fptr);
	 }

	memcpy(&(dir_meta), body_ptr->dir_meta, sizeof(DIR_META_TYPE));

	nextfilepos = dir_meta.root_entry_page;

	*result_index = -1;
	/* If nothing in the dir. Return 0 */
	if (nextfilepos <= 0)
		return 0;

	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
	memset(&rootpage, 0, sizeof(DIR_ENTRY_PAGE));

	if (body_ptr->meta_opened == FALSE) {
		ret = fetch_meta_path(thismetapath, body_ptr->inode_num);
		if (ret < 0)
			return ret;

		body_ptr->fptr = fopen(thismetapath, "r+");
		if (body_ptr->fptr == NULL) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			goto errcode_handle;
		}

		setbuf(body_ptr->fptr, NULL);
		flock(fileno(body_ptr->fptr), LOCK_EX);
		body_ptr->meta_opened = TRUE;
	}

	/*Read the root node*/
	FSEEK(body_ptr->fptr, nextfilepos, SEEK_SET);
	FREAD(&rootpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	ret = search_dir_entry_btree(childname, &rootpage,
			fileno(body_ptr->fptr), &tmp_index, &tmp_resultpage);
	if ((ret < 0) && (ret != -ENOENT)) {
		errcode = ret;
		goto errcode_handle;
	}

	gettimeofday(&(body_ptr->last_access_time), NULL);

	if (body_ptr->something_dirty == FALSE)
		body_ptr->something_dirty = TRUE;

	if (ret == -ENOENT) { /*Not found*/
		*result_index = -1;
		return 0;
	}
	/*Found the entry */
	ret = meta_cache_push_dir_page(body_ptr, &tmp_resultpage);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	*result_index = tmp_index;
	memcpy(result_page, &tmp_resultpage, sizeof(DIR_ENTRY_PAGE));

	return 0;

/* Exception handling from here */
errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: meta_cache_remove
*        Inputs: ino_t this_inode
*       Summary: Remove the cache entry of inode "this_inode" from meta
*                cache if found.
*  Return value: If successful, returns 0. If error, returns -1.
*
*************************************************************************/
int meta_cache_remove(ino_t this_inode)
{
	int index;
	META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr, *prev_ptr;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	char found_entry;
	int ret;

	index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
	sem_wait(&(meta_mem_cache[index].header_sem));

	current_ptr = meta_mem_cache[index].meta_cache_entries;
	prev_ptr = NULL;
	found_entry = FALSE;
	while (current_ptr != NULL) {
		if (current_ptr->inode_num == this_inode) { /* A hit */
			found_entry = TRUE;
			break;
		}
		prev_ptr = current_ptr;
		current_ptr = current_ptr->next;
	}

	if (found_entry == FALSE) { /*If did not find cache entry*/
		sem_post(&(meta_mem_cache[index].header_sem));
		return 0;
	}

/*Lock body*/
/*TODO: May need to add checkpoint here so that long sem wait will
	free all locks*/

	sem_wait(&((current_ptr->body).access_sem));

	body_ptr = &(current_ptr->body);

	/* TODO: How to handle deletion error due to failed flush */
	/* Flush dirty data */
	ret = flush_single_entry(body_ptr);
	if (ret < 0) {
		sem_post(&((current_ptr->body).access_sem));
		sem_post(&(meta_mem_cache[index].header_sem));
		return ret;
	}

	if (body_ptr->dir_meta != NULL)
		free(body_ptr->dir_meta);

	if (body_ptr->file_meta != NULL)
		free(body_ptr->file_meta);

	if (body_ptr->symlink_meta != NULL)
		free(body_ptr->symlink_meta);

	if (body_ptr->dir_entry_cache[0] != NULL)
		free(body_ptr->dir_entry_cache[0]);

	if (body_ptr->dir_entry_cache[1] != NULL)
		free(body_ptr->dir_entry_cache[1]);


	current_ptr->inode_num = 0;

	sem_post(&((current_ptr->body).access_sem));

	memset(body_ptr, 0, sizeof(META_CACHE_ENTRY_STRUCT));

	if (current_ptr->next != NULL)
		current_ptr->next->prev = prev_ptr;

	if (meta_mem_cache[index].last_entry == current_ptr)
		meta_mem_cache[index].last_entry = prev_ptr;

	if (prev_ptr != NULL)
		prev_ptr->next = current_ptr->next;
	else
		meta_mem_cache[index].meta_cache_entries = current_ptr->next;

	meta_mem_cache[index].num_entries--;

	free(current_ptr);

	sem_wait(&num_entry_sem);
	current_meta_mem_cache_entries--;
	sem_post(&num_entry_sem);

	sem_post(&(meta_mem_cache[index].header_sem));

	return 0;
}

/* Helper function for expiring a cache entry */
static inline int _expire_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *lptr,
							int cindex)
{
	int ret;

	ret = flush_single_entry(&(lptr->body));
	if (ret < 0)
		goto errhandle;

	ret = free_single_meta_cache_entry(lptr);
	if (ret < 0)
		goto errhandle;

	if (lptr->next != NULL)
		lptr->next->prev = lptr->prev;
	if (lptr->prev != NULL)
		lptr->prev->next = lptr->next;
	if (meta_mem_cache[cindex].last_entry == lptr)
		meta_mem_cache[cindex].last_entry = lptr->prev;
	if (meta_mem_cache[cindex].meta_cache_entries == lptr)
		meta_mem_cache[cindex].meta_cache_entries = lptr->next;
	meta_mem_cache[cindex].num_entries--;

	sem_post(&(lptr->body).access_sem);
	free(lptr);
	sem_wait(&num_entry_sem);
	current_meta_mem_cache_entries--;
	sem_post(&num_entry_sem);
	sem_post(&(meta_mem_cache[cindex].header_sem));

	return 0;

errhandle:
	sem_post(&(lptr->body).access_sem);
	sem_post(&(meta_mem_cache[cindex].header_sem));

	return ret;
}

/************************************************************************
*
* Function name: expire_meta_mem_cache_entry
*        Inputs: None
*       Summary: Try to expire some cache entry that has not been used
*                in a given period of time.
*  Return value: If successfully expire something, returns 0.
*                If not, returns -EBUSY.
*                Returns negation of error code if error.
*          Note: How to expire:
*                Starting from some random header index, check from
*                last_entry to find out if can lock some entry. If so,
*                expire it if last_access_time > 0.5 sec.
*                Move to the next index if cannot expire anything,
*                wrap back to 0 if overflow.
*
*************************************************************************/
int expire_meta_mem_cache_entry(void)
{
	int start_index, cindex;
	struct timeval current_time, *atime_ptr;
	META_CACHE_LOOKUP_ENTRY_STRUCT *lptr;
	double float_current_time, float_access_time;
	int ret;
	char isdone;

	gettimeofday(&current_time, NULL);
	srandom((unsigned int)(current_time.tv_usec));
	start_index = (random() % NUM_META_MEM_CACHE_HEADERS);
	cindex = start_index;
	isdone = FALSE;
	/* Go through meta_mem_cache[] */
	do {
		sem_wait(&(meta_mem_cache[cindex].header_sem));
		lptr = meta_mem_cache[cindex].last_entry;
		while (lptr != NULL) {
			if (sem_trywait(&(lptr->body).access_sem) != 0) {
				lptr = lptr->prev;
				continue;
			}
			gettimeofday(&current_time, NULL);
			float_current_time = (current_time.tv_sec * 1.0)
				+ (current_time.tv_usec * 0.000001);
			atime_ptr = &((lptr->body).last_access_time);
			float_access_time = (atime_ptr->tv_sec * 1.0) +
					(atime_ptr->tv_usec * 0.000001);
			if (float_current_time - float_access_time > 0.5) {
				/* Expire the entry */
				ret = _expire_entry(lptr, cindex);
				return ret;
			}
			/* If find that current_time < last_access_time,
				fix last_access_time to be current_time */
			if (float_current_time < float_access_time)
				gettimeofday(atime_ptr, NULL);

			sem_post(&(lptr->body).access_sem);
			lptr = lptr->prev;
		}

		sem_post(&(meta_mem_cache[cindex].header_sem));
		cindex = ((cindex + 1) % NUM_META_MEM_CACHE_HEADERS);
	} while (cindex != start_index);

	/* Nothing was expired */
	return -EBUSY;
}

/************************************************************************
*
* Function name: meta_cache_lock_entry
*        Inputs: ino_t this_inode
*       Summary: Find and lock the meta cache entry for inode "this_inode".
*                If no entry is found, allocate a new entry for this inode,
*                and proceed with the locking
*  Return value: The pointer to the locked entry if successful. Otherwise
*                returns NULL.
*
*************************************************************************/
META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	int ret_val;
	int index;
	META_CACHE_LOOKUP_ENTRY_STRUCT *current_ptr;
	char need_new, expire_done;
	SUPER_BLOCK_ENTRY tempentry;
	META_CACHE_ENTRY_STRUCT *result_ptr;
	struct timespec time_to_sleep;
	int errcount;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	index = hash_inode_to_meta_cache(this_inode);
/*First lock corresponding header*/
	sem_wait(&(meta_mem_cache[index].header_sem));

	need_new = TRUE;

	while (need_new == TRUE) {
		errcount = 0;
		current_ptr = meta_mem_cache[index].meta_cache_entries;
		while (current_ptr != NULL) {
			if (current_ptr->inode_num == this_inode) { /* A hit */
				need_new = FALSE;
				break;
			}
			current_ptr = current_ptr->next;
		}
		if (need_new == FALSE)
			break;

		/*Probe whether entries full and expire some entry if full */
		sem_wait(&num_entry_sem);
		if (current_meta_mem_cache_entries >
						MAX_META_MEM_CACHE_ENTRIES) {
			sem_post(&num_entry_sem);

			/* Will need to release current header_sem, then
				reaquire after */
			sem_post(&(meta_mem_cache[index].header_sem));
			expire_done = FALSE;
			while (expire_done == FALSE) {
				ret_val = expire_meta_mem_cache_entry();
				/* Sleep if cannot find one, then
						retry */
				if (ret_val < 0) {
					if (ret_val != -EBUSY)
						errcount++;
					if (errcount > 5) {
						write_log(0,
							"Lock meta cache err");
						return NULL;
					}
					nanosleep(&time_to_sleep, NULL);
				} else {
					expire_done = TRUE;
				}
			}
			sem_wait(&(meta_mem_cache[index].header_sem));
			/* Try again. Some other thread may have add in
				this entry */
			continue;
		} else {
			sem_post(&num_entry_sem);
		}

		ret_val = super_block_read(this_inode, &tempentry);
		if (ret_val < 0) {
			sem_post(&(meta_mem_cache[index].header_sem));
			return NULL;
		}

		current_ptr = malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
		if (current_ptr == NULL) {
			sem_post(&(meta_mem_cache[index].header_sem));
			return NULL;
		}
		/* TODO: Should return errno as well if needed */
		/* return -EACCES; */

		memset(current_ptr, 0, sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));

		current_ptr->next = meta_mem_cache[index].meta_cache_entries;
		if (meta_mem_cache[index].meta_cache_entries != NULL)
			meta_mem_cache[index].meta_cache_entries->prev =
								current_ptr;
		meta_mem_cache[index].meta_cache_entries = current_ptr;
		if (meta_mem_cache[index].last_entry == NULL)
			meta_mem_cache[index].last_entry = current_ptr;
		current_ptr->inode_num = this_inode;
		sem_init(&((current_ptr->body).access_sem), 0, 1);
		meta_mem_cache[index].num_entries++;
		sem_wait(&num_entry_sem);
		current_meta_mem_cache_entries++;
		sem_post(&num_entry_sem);

		(current_ptr->body).inode_num = this_inode;
		(current_ptr->body).meta_opened = FALSE;
		memcpy(&((current_ptr->body).this_stat),
				&(tempentry.inode_stat), sizeof(struct stat));
		need_new = FALSE;
		break;
	}
	/*Lock body*/
	/*TODO: May need to add checkpoint here so that long sem wait
		will free all locks*/
	result_ptr = &(current_ptr->body);

	sem_wait(&((current_ptr->body).access_sem));
	sem_post(&(meta_mem_cache[index].header_sem));

	/* Lock meta file if opened */
	if ((current_ptr->body).meta_opened == TRUE)
		flock(fileno((current_ptr->body).fptr), LOCK_EX);

	return result_ptr;
}

/************************************************************************
*
* Function name: meta_cache_unlock_entry
*        Inputs: META_CACHE_ENTRY_STRUCT *target_ptr
*       Summary: Unlock the meta cache entry pointed by "target_ptr".
*  Return value: 0 if successful, and -1 if there is an error.
*
*************************************************************************/
int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(target_ptr->access_sem));

	gettimeofday(&(target_ptr->last_access_time), NULL);

	/* Unlock meta file if opened */
	if (target_ptr->meta_opened == TRUE)
		flock(fileno(target_ptr->fptr), LOCK_UN);

	sem_post(&(target_ptr->access_sem));

	return 0;
}

/************************************************************************
*
* Function name: meta_cache_close_file
*        Inputs: META_CACHE_ENTRY_STRUCT *target_ptr
*       Summary: Flush dirty cached content to meta file and close it.
*  Return value: 0 if successful, and negation of error code if there is
*                an error.
*
*************************************************************************/
int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	int ret_val;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(target_ptr->access_sem));

	/* TODO: Use error logging here. Return value may not be useful. */
	gettimeofday(&(target_ptr->last_access_time), NULL);
	if (target_ptr->meta_opened == FALSE)
		return 0;
	if (target_ptr->fptr == NULL)
		return 0;

	ret_val = 0;
	if (META_CACHE_FLUSH_NOW == TRUE)
		ret_val = flush_single_entry(target_ptr);

	flock(fileno(target_ptr->fptr), LOCK_UN);
	fclose(target_ptr->fptr);
	target_ptr->meta_opened = FALSE;

	return ret_val;
}

/************************************************************************
*
* Function name: meta_cache_drop_pages
*        Inputs: META_CACHE_ENTRY_STRUCT *target_ptr
*       Summary: Flush dirty cached dir entry pages to meta file and drop
*                the pages from cache.
*  Return value: 0 if successful, and -1 if there is an error.
*
*************************************************************************/
int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret_val;

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	gettimeofday(&(body_ptr->last_access_time), NULL);

	if (body_ptr->dir_entry_cache[0] != NULL) {
		if (body_ptr->dir_entry_cache_dirty[0] == TRUE)
			ret_val = meta_cache_flush_dir_cache(body_ptr, 0);
		free(body_ptr->dir_entry_cache[0]);
		body_ptr->dir_entry_cache[0] = NULL;
		body_ptr->dir_entry_cache_dirty[0] = FALSE;
	}

	if (body_ptr->dir_entry_cache[1] != NULL) {
		if (body_ptr->dir_entry_cache_dirty[1] == TRUE)
			ret_val = meta_cache_flush_dir_cache(body_ptr, 1);
		free(body_ptr->dir_entry_cache[1]);
		body_ptr->dir_entry_cache[1] = NULL;
		body_ptr->dir_entry_cache_dirty[1] = FALSE;
	}

	return 0;
}

/************************************************************************
*
* Function name: meta_cache_update_symlink_data
*        Inputs: ino_t this_inode, const struct stat *inode_stat,
*                const SYMLINK_META_TYPE *symlink_meta_ptr,
*                META_CACHE_ENTRY_STRUCT *bptr
*       Summary: Update symlink stat or meta data in memory cache.
*  Return value: 0 if successful, otherwise return negative error code.
*
*************************************************************************/
int meta_cache_update_symlink_data(ino_t this_inode,
	const struct stat *inode_stat,
	const SYMLINK_META_TYPE *symlink_meta_ptr,
	META_CACHE_ENTRY_STRUCT *bptr)
{
	int ret;

	write_log(10, "Debug meta cache update symbolic link data\n");

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(bptr->access_sem));

	/* Update stat */
	if (inode_stat != NULL) {
		memcpy(&(bptr->this_stat), inode_stat, sizeof(struct stat));
		bptr->stat_dirty = TRUE;
	}

	/* Update symlink_meta */
	if (symlink_meta_ptr != NULL) {
		if (bptr->symlink_meta == NULL) {
			bptr->symlink_meta = malloc(sizeof(SYMLINK_META_TYPE));
			if (bptr->symlink_meta == NULL)
				return -ENOMEM;
		}
		memcpy((bptr->symlink_meta), symlink_meta_ptr,
			sizeof(SYMLINK_META_TYPE));
		bptr->meta_dirty = TRUE;
	}

	gettimeofday(&(bptr->last_access_time), NULL);

	if (bptr->something_dirty == FALSE)
		bptr->something_dirty = TRUE;

	/* Write changes to meta file if write through is enabled */
	if (META_CACHE_FLUSH_NOW == TRUE) {
		ret = flush_single_entry(bptr);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/************************************************************************
*
* Function name: meta_cache_lookup_symlink_data
*        Inputs: ino_t this_inode, const struct stat *inode_stat,
*                const SYMLINK_META_TYPE *symlink_meta_ptr,
*                META_CACHE_ENTRY_STRUCT *bptr
*       Summary: Lookup symlink stat or meta data in memory cache. If
*                meta is not in memory, then read it from disk.
*  Return value: 0 if successful, otherwise return negative error code.
*
*************************************************************************/
int meta_cache_lookup_symlink_data(ino_t this_inode, struct stat *inode_stat,
	SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	int ret, errcode;
	size_t ret_size;

	write_log(10, "Debug meta cache lookup symbolic link data\n");

	_ASSERT_CACHE_LOCK_IS_LOCKED_(&(body_ptr->access_sem));

	if (inode_stat != NULL)
		memcpy(inode_stat, &(body_ptr->this_stat), sizeof(struct stat));

	if (symlink_meta_ptr != NULL) {
		if (body_ptr->symlink_meta == NULL) {
			body_ptr->symlink_meta =
				malloc(sizeof(SYMLINK_META_TYPE));
			if (body_ptr->symlink_meta == NULL)
				return -ENOMEM;

			ret = _open_file(body_ptr);

			if (ret < 0)
				return ret;

			FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
			FREAD(body_ptr->symlink_meta, sizeof(SYMLINK_META_TYPE),
							1, body_ptr->fptr);
		}

		memcpy(symlink_meta_ptr, body_ptr->symlink_meta,
			sizeof(SYMLINK_META_TYPE));
	}

	gettimeofday(&(body_ptr->last_access_time), NULL);

	return 0;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}
