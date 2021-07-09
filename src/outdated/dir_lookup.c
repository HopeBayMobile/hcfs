/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dir_lookup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "meta_mem_cache.h"

/*TODO: need to modify pathname lookup to handle symlink*/
/* TODO: need to check for search permissions for the node encountered on
	the path, and to check for depth of recursive */

PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

/************************************************************************
*
* Function name: init_pathname_cache
*        Inputs: None
*       Summary: Initialize cache for pathname lookup.
*  Return value: 0 if successful.
*
*************************************************************************/
int32_t init_pathname_cache(void)
{
	int64_t count;

	for (count = 0; count < PATHNAME_CACHE_ENTRY_NUM; count++) {
		memset(&(pathname_cache[count]), 0,
					sizeof(PATHNAME_CACHE_ENTRY));
		sem_init(&(pathname_cache[count].cache_entry_sem), 0, 1);
	}
	return 0;
}

/************************************************************************
*
* Function name: compute_hash
*        Inputs: const char *path
*       Summary: Compute the hash value for lookup table.
*  Return value: Hash value for input string "path".
*
*************************************************************************/
uint64_t compute_hash(const char *path)
{
	uint64_t seed = 0;
	int64_t count;
	uint8_t temp;
	uint64_t temp1;
	uint64_t mode_base;

	if (PATHNAME_CACHE_ENTRY_NUM > 65536)
		mode_base = PATHNAME_CACHE_ENTRY_NUM;
	else
		mode_base = 65536;

	for (count = 0; count < strlen(path); count++) {
		temp = (uint8_t) path[count];
		temp1 = (uint64_t) temp;
		seed = (3 * seed + (temp1 * 13)) % mode_base;
	}
	seed = seed % PATHNAME_CACHE_ENTRY_NUM;

	return seed;
}

/************************************************************************
*
* Function name: replace_pathname_cache
*        Inputs: int64_t index, char *path, ino_t inode_number
*       Summary: Replace hash table entry "index" with the provided path
*                "path" and corresponding inode number "inode_number".
*  Return value: 0 if replaced, -1 if pathname is too long.
*
*************************************************************************/
int32_t replace_pathname_cache(int64_t index, char *path, ino_t inode_number)
{
	if (index < 0 || index >= PATHNAME_CACHE_ENTRY_NUM)
		return -1;
	if (strlen(path) > MAX_PATHNAME)
		return -1;
	sem_wait(&(pathname_cache[index].cache_entry_sem));
	strcpy(pathname_cache[index].pathname, path);
	pathname_cache[index].inode_number = inode_number;
	sem_post(&(pathname_cache[index].cache_entry_sem));

	return 0;
}

/************************************************************************
*
* Function name: invalidate_pathname_cache_entry
*        Inputs: const char *path
*       Summary: Invalidate the cache entry for pathname "path" if it is
*                in the cache.
*  Return value: 0 if invalidated or not found, -1 if pathname is too long.
*
*************************************************************************/
int32_t invalidate_pathname_cache_entry(const char *path)
{
	uint64_t index;

	if (strlen(path) > MAX_PATHNAME)
		return -1;

	index = compute_hash(path);
	sem_wait(&(pathname_cache[index].cache_entry_sem));
	if (strcmp(pathname_cache[index].pathname, path) == 0) {
		/*Occupant is this path name */
		pathname_cache[index].pathname[0] = 0;
		pathname_cache[index].inode_number = 0;
	}
	sem_post(&(pathname_cache[index].cache_entry_sem));
	return 0;
}

/************************************************************************
*
* Function name: check_cached_path
*        Inputs: const char *path
*       Summary: Returns the cached inode number for pathname "path"
*                (if cached).
*  Return value: Inode number if "path" is in cache, or 0 otherwise.
*
*************************************************************************/
ino_t check_cached_path(const char *path)
{
	uint64_t index;
	ino_t return_val;

	if (strlen(path) > MAX_PATHNAME)
		return 0;

	index = compute_hash(path);
	sem_wait(&(pathname_cache[index].cache_entry_sem));
	if (strcmp(pathname_cache[index].pathname, path) != 0)
		return_val = 0;
	else
		return_val = pathname_cache[index].inode_number;
	sem_post(&(pathname_cache[index].cache_entry_sem));
	return return_val;
}

/************************************************************************
*
* Function name: lookup_pathname
*        Inputs: const char *path, int32_t *errcode
*       Summary: Resolve the pathname to find the inode number.
*  Return value: Inode number if "path" is found in the system,
*                or 0 otherwise with the error code copied to "*errcode".
*
*************************************************************************/
/* TODO: resolving pathname may result in EACCES */
ino_t lookup_pathname(const char *path, int32_t *errcode)
{
	int32_t strptr;
	char tempdir[MAX_PATHNAME+10];
	ino_t cached_inode;

	*errcode = -ENOENT;

	if (strcmp(path, "/") == 0)  /*Root of the FUSE system has inode 1. */
		return 1;

	strptr = strlen(path);
	while (strptr > 1) {
		if (strptr > MAX_PATHNAME) {
			strptr--;
			continue;
		}
		if (strptr == strlen(path)) {
			cached_inode = check_cached_path(path);
			if (cached_inode > 0)
				return cached_inode;
			strptr--;
			continue;
		}
		if (path[strptr-1] != '/') {
			strptr--;
			continue;
		}
		strncpy(tempdir, path, strptr);
		tempdir[strptr-1] = 0;
		cached_inode =
			check_cached_path(tempdir);
		if (cached_inode > 0)
			return lookup_pathname_recursive(cached_inode,
					strptr-1, &path[strptr-1], path,
								errcode);
		strptr--;
	}
	return lookup_pathname_recursive(1, 0, path, path, errcode);
}

/************************************************************************
*
* Function name: lookup_pathname_recursive
*        Inputs: ino_t subroot, int32_t prefix_len, const char *partialpath,
*                const char *fullpath, int32_t *errcode
*       Summary: Resolve the path "partialpath" from the directory "subroot".
*                Full path name to resolve is pointed by "fullpath".
*                Length for the name of "subroot" is "prefix_len".
*  Return value: Inode number if "path" is found in the system,
*                or 0 otherwise with the error code copied to "*errcode".
*
*************************************************************************/
ino_t lookup_pathname_recursive(ino_t subroot, int32_t prefix_len,
		const char *partialpath, const char *fullpath, int32_t *errcode)
{
	int32_t count;
	int32_t new_prefix_len;
	char search_subdir_only;
	char target_entry_name[400];
	char tempname[400];
	ino_t hit_inode;
	DIR_ENTRY_PAGE temp_page;
	int32_t temp_index;
	int32_t ret_val;
	META_CACHE_ENTRY_STRUCT *cache_entry;

	if ((partialpath[0] == '/') && (strlen(partialpath) == 1))
		return subroot;

	search_subdir_only = FALSE;
	for (count = 1; count < strlen(partialpath); count++) {
		if (partialpath[count] == '/') {
			search_subdir_only = TRUE;
			strncpy(target_entry_name, &(partialpath[1]), count-1);
			target_entry_name[count-1] = 0;
			break;
		}
	}

	if (search_subdir_only) {
		cache_entry = meta_cache_lock_entry(subroot);
		ret_val = meta_cache_seek_dir_entry(subroot, &temp_page,
				&temp_index, target_entry_name, cache_entry);
		meta_cache_close_file(cache_entry);
		meta_cache_unlock_entry(cache_entry);

		if ((ret_val == 0) && (temp_index < 0)) {
			*errcode = -ENOENT;
			return 0;
		}

		if ((ret_val == 0) &&
			(temp_page.dir_entries[temp_index].d_ino > 0)) {
			hit_inode = temp_page.dir_entries[temp_index].d_ino;
			if ((prefix_len+strlen(target_entry_name)) < 256) {
				new_prefix_len = prefix_len + 1 +
						strlen(target_entry_name);
				strncpy(tempname, fullpath, new_prefix_len);
				tempname[new_prefix_len] = 0;
				replace_pathname_cache(compute_hash(tempname),
							tempname, hit_inode);
			 }
			return lookup_pathname_recursive(hit_inode,
				new_prefix_len, &(fullpath[new_prefix_len]),
							fullpath, errcode);
		}
		if ((ret_val == 0) &&
			(temp_page.dir_entries[temp_index].d_ino == 0)) {
			*errcode = -ENOENT;
			return 0;
		}
		*errcode = ret_val;
		return 0;	 /*Cannot find this entry*/
	}

	strcpy(target_entry_name, &(partialpath[1]));

	cache_entry = meta_cache_lock_entry(subroot);
	ret_val = meta_cache_seek_dir_entry(subroot, &temp_page, &temp_index,
						target_entry_name, cache_entry);
	meta_cache_close_file(cache_entry);
	meta_cache_unlock_entry(cache_entry);

	if (ret_val < 0) {
		*errcode = ret_val;
		return 0;
	}

	if (temp_index < 0) {
		*errcode = -ENOENT;
		return 0;
	}

	if ((ret_val == 0) && (temp_page.dir_entries[temp_index].d_ino > 0)) {
		hit_inode = temp_page.dir_entries[temp_index].d_ino;
		if ((prefix_len + strlen(target_entry_name)) < 256) {
			new_prefix_len = prefix_len + 1 +
						strlen(target_entry_name);
			strncpy(tempname, fullpath, new_prefix_len);
			tempname[new_prefix_len] = 0;
			replace_pathname_cache(compute_hash(tempname), tempname,
								hit_inode);
		}
		return hit_inode;
	}

	if ((ret_val == 0) && (temp_page.dir_entries[temp_index].d_ino == 0)) {
		*errcode = -ENOENT;
		return 0;
	}
	*errcode = ret_val;
	return 0;
}

/* Given parent "parent", search for "childname" in parent and return
the directory entry in structure pointed by "dentry" if found. If not or
if error, return the negation of error code. */
int32_t lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry)
{
	META_CACHE_ENTRY_STRUCT *cache_entry;
	DIR_ENTRY_PAGE temp_page;
	int32_t temp_index, ret_val;

	cache_entry = meta_cache_lock_entry(parent);
	ret_val = meta_cache_seek_dir_entry(parent, &temp_page,
				&temp_index, childname, cache_entry);
	meta_cache_close_file(cache_entry);
	meta_cache_unlock_entry(cache_entry);

	if (ret_val < 0)
		return ret_val;
	if (temp_index < 0)
		return -ENOENT;
	if (temp_page.dir_entries[temp_index].d_ino == 0)
		return -ENOENT;

	memcpy(dentry, &(temp_page.dir_entries[temp_index]),
			sizeof(DIR_ENTRY));
	return 0;
}
