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

/* TODO: Will need to remove parent update from actual_delete_inode */
/* TODO: Revert ops for partially updated stats / meta */

#include "file_present.h"

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <inttypes.h>

#include "global.h"
#include "super_block.h"
#include "fuseop.h"
#include "params.h"
#include "utils.h"
#include "meta_mem_cache.h"
#include "logger.h"
#include "macro.h"
#include "xattr_ops.h"
#include "metaops.h"
#include "path_reconstruct.h"
#include "dir_statistics.h"
#include "parent_lookup.h"
#include "hcfs_cacheops.h"

/************************************************************************
*
* Function name: meta_forget_inode
*        Inputs: ino_t self_inode
*       Summary: Drop the meta for "self_inode" from the system by deleting
*                corresponding meta files and other entries.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t meta_forget_inode(ino_t self_inode)
{
	char thismetapath[METAPATHLEN];
	int32_t ret;

	ret = fetch_meta_path(thismetapath, self_inode);
	if (ret < 0)
		return ret;

	if (access(thismetapath, F_OK) == 0)
		UNLINK(thismetapath);

/*TODO: Need to remove entry from super block if needed */

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: fetch_inode_stat
*        Inputs: ino_t this_inode, HCFS_STAT *inode_stat
*       Summary: Read inode "HCFS_STAT" from meta cache, and then return
*                to the caller via "inode_stat".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t fetch_inode_stat(ino_t this_inode,
			 HCFS_STAT *inode_stat,
			 uint64_t *ret_gen,
			 char *ret_pin_status)
{
	HCFS_STAT returned_stat;
	int32_t ret_code;
	META_CACHE_ENTRY_STRUCT *temp_entry;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE symlinkmeta;

	/*First will try to lookup meta cache*/
	if (this_inode > 0) {
		temp_entry = meta_cache_lock_entry(this_inode);
		if (temp_entry == NULL)
			return -errno;

		/* Only fetch inode stat, so does not matter if inode is reg
		*  file or dir here*/
		ret_code = meta_cache_lookup_file_data(this_inode,
				&returned_stat, NULL, NULL, 0, temp_entry);
		if (ret_code < 0)
			goto error_handling;

		if (ret_gen != NULL || ret_pin_status != NULL) {
			if (S_ISFILE(returned_stat.mode)) {
				ret_code = meta_cache_lookup_file_data(
						this_inode, NULL, &filemeta,
						NULL, 0, temp_entry);
				if (ret_code < 0)
					goto error_handling;
				if (ret_pin_status)
					*ret_pin_status = filemeta.local_pin;
				if (ret_gen)
					*ret_gen = filemeta.generation;
			}
			if (S_ISDIR(returned_stat.mode)) {
				ret_code = meta_cache_lookup_dir_data(
						this_inode, NULL, &dirmeta,
						NULL, temp_entry);
				if (ret_code < 0)
					goto error_handling;
				if (ret_pin_status)
					*ret_pin_status = dirmeta.local_pin;
				if (ret_gen)
					*ret_gen = dirmeta.generation;
			}
			if (S_ISLNK(returned_stat.mode)) {
				ret_code = meta_cache_lookup_symlink_data(
						this_inode, NULL, &symlinkmeta,
						temp_entry);
				if (ret_code < 0)
					goto error_handling;
				if (ret_pin_status)
					*ret_pin_status = symlinkmeta.local_pin;
				if (ret_gen)
					*ret_gen = symlinkmeta.generation;
			}
		}

		ret_code = meta_cache_close_file(temp_entry);
		if (ret_code < 0) {
			meta_cache_unlock_entry(temp_entry);
			return ret_code;
		}

		ret_code = meta_cache_unlock_entry(temp_entry);

		if ((ret_code == 0) && (inode_stat != NULL)) {
			memcpy(inode_stat, &returned_stat, sizeof(HCFS_STAT));
			write_log(10, "fetch_inode_stat get inode %lld\n",
				inode_stat->ino);
			return 0;
		}

		if (ret_code < 0)
			return ret_code;
	} else {
		return -ENOENT;
	}

	if (ret_gen)
		write_log(10, "fetch_inode_stat get only generation %lld\n",
		          *ret_gen);
	return 0;

error_handling:
	meta_cache_close_file(temp_entry);
	meta_cache_unlock_entry(temp_entry);
	return ret_code;
}

/* Remove entry when this child inode fail to create meta */
static inline int32_t dir_remove_fail_node(ino_t parent_inode, ino_t child_inode,
	const char *childname, mode_t child_mode, BOOL is_external)
{
	META_CACHE_ENTRY_STRUCT *tmp_bodyptr;

	tmp_bodyptr = meta_cache_lock_entry(parent_inode);
	if (tmp_bodyptr == NULL)
		return -errno;
	dir_remove_entry(parent_inode, child_inode, childname, child_mode,
		tmp_bodyptr, is_external);
	meta_cache_unlock_entry(tmp_bodyptr);
	return 0;
}

/************************************************************************
*
* Function name: mknod_update_meta
*        Inputs: ino_t self_inode, ino_t parent_inode, char *selfname,
*                HCFS_STAT *this_stat, ino_t root_ino
*       Summary: Helper of "hfuse_mknod" function. Will save the inode stat
*                of the newly create regular file to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			HCFS_STAT *this_stat, uint64_t this_gen,
			MOUNT_T *mountptr, int64_t *delta_meta_size,
			PIN_t pin, BOOL is_external)
{
	int32_t ret_val;
	FILE_META_TYPE this_meta;
	FILE_STATS_TYPE file_stats;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	int64_t metasize, old_metasize = 0, new_metasize = 0;
	int64_t metasize_blk, old_metasize_blk = 0, new_metasize_blk = 0;
	CLOUD_RELATED_DATA cloud_related_data;
	ino_t root_ino;

	root_ino = mountptr->f_ino;
	*delta_meta_size = 0;

	/* Add "self_inode" to its parent "parent_inode" */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -errno;

	ret_val = meta_cache_open_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = update_meta_seq(body_ptr);
	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	/* Get old meta size before adding new entry */
	meta_cache_get_meta_size(body_ptr, &old_metasize, &old_metasize_blk);

	/* Add path lookup table */
	ret_val = sem_wait(&(pathlookup_data_lock));
	if (ret_val < 0)
		return ret_val;
	ret_val = lookup_add_parent(self_inode, parent_inode);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret_val;
	}
	sem_post(&(pathlookup_data_lock));

	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
				this_stat->mode, body_ptr, is_external);
	if (ret_val < 0)
		goto error_handling;

	/* Get old meta size after adding new entry */
	meta_cache_get_meta_size(body_ptr, &new_metasize, &new_metasize_blk);

	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	meta_cache_unlock_entry(body_ptr);

	/* Init file meta */
	memset(&this_meta, 0, sizeof(FILE_META_TYPE));
	this_meta.generation = this_gen;
        this_meta.finished_seq = 0;
	this_meta.source_arch = ARCH_CODE;
	this_meta.root_inode = root_ino;
	this_meta.local_pin = pin;
	write_log(10, "Debug: File %s inherits parent pin status = %d\n",
		selfname, pin);

	/* Store the inode and file meta of the new file to meta cache */
	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		ret_val = -errno;
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		return ret_val;
	}

	meta_cache_sync_later(body_ptr);
	ret_val = meta_cache_update_file_data(self_inode, this_stat, &this_meta,
							NULL, 0, body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		goto error_handling;
	}

	ret_val = meta_cache_open_file(body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		goto error_handling;
	}
	memset(&file_stats, 0, sizeof(FILE_STATS_TYPE));
	MWRITE(body_ptr, &file_stats, sizeof(FILE_STATS_TYPE),
	       sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE));
	
	memset(&cloud_related_data, 0, sizeof(CLOUD_RELATED_DATA));
	MWRITE(body_ptr, &cloud_related_data, sizeof(CLOUD_RELATED_DATA),
	       sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
	       sizeof(FILE_STATS_TYPE));
#ifdef _ANDROID_ENV_
	/* Inherit xattr from parent */
	if (S_ISFILE(this_stat->mode)) {
		write_log(10, "Debug:inode %"PRIu64" begin to inherit xattrs\n",
				(uint64_t)self_inode);
		ret_val = inherit_xattr(parent_inode, self_inode, body_ptr);
		if (ret_val < 0)
			write_log(0, "Error: inode %"PRIu64" fails to inherit"
				" xattrs from parent inode %"PRIu64".\n",
				(uint64_t)self_inode, (uint64_t)parent_inode);
	}
#endif

	meta_cache_get_meta_size(body_ptr, &metasize, &metasize_blk);

	meta_cache_remove_sync_later(body_ptr);
	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return ret_val;

	/* Mark dirty here so that dirty data size includes dirty meta size. */
	super_block_mark_dirty(self_inode);

	/* Storage location for new file is local */
	DIR_STATS_TYPE tmpstat;
	tmpstat.num_local = 1;
	tmpstat.num_cloud = 0;
	tmpstat.num_hybrid = 0;
	sem_wait(&(pathlookup_data_lock));
	ret_val = update_dirstat_parent(parent_inode, &tmpstat);
	sem_post(&(pathlookup_data_lock));
	if (ret_val < 0)
		return ret_val;

	if (old_metasize > 0 && new_metasize > 0) {
		/* Update local meta round size */
		change_system_meta(0,
			(new_metasize_blk - old_metasize_blk) + metasize_blk,
			0, 0, 0, 0, FALSE);
		*delta_meta_size = (new_metasize - old_metasize) + metasize;
	} else {
		*delta_meta_size = metasize;
	}

	return 0;
errcode_handle:
	dir_remove_fail_node(parent_inode, self_inode,
				selfname, this_stat->mode, is_external);
	ret_val = errcode;
error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	return ret_val;
}

/************************************************************************
*
* Function name: mkdir_update_meta
*        Inputs: ino_t self_inode, ino_t parent_inode, char *selfname,
*                HCFS_STAT *this_stat, ino_t root_ino
*       Summary: Helper of "hfuse_mkdir" function. Will save the inode stat
*                of the newly create directory object to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t mkdir_update_meta(ino_t self_inode,
			  ino_t parent_inode,
			  const char *selfname,
			  HCFS_STAT *this_stat,
			  uint64_t this_gen,
			  MOUNT_T *mountptr,
			  int64_t *delta_meta_size,
			  PIN_t pin,
			  BOOL is_external)
{
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	int32_t ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	int64_t metasize, old_metasize, new_metasize;
	int64_t metasize_blk, old_metasize_blk = 0, new_metasize_blk = 0;
	CLOUD_RELATED_DATA cloud_related_data;
	ino_t root_ino;

	root_ino = mountptr->f_ino;
	*delta_meta_size = 0;
	/* Save the new entry to its parent and update meta */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -errno;

	ret_val = meta_cache_open_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = update_meta_seq(body_ptr);
	if (ret_val < 0)
		goto error_handling;

	meta_cache_get_meta_size(body_ptr, &old_metasize, &old_metasize_blk);

	/* Add parent to lookup db */
	ret_val = sem_wait(&(pathlookup_data_lock));
	if (ret_val < 0)
		return ret_val;
	ret_val = lookup_add_parent(self_inode, parent_inode);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret_val;
	}

	sem_post(&(pathlookup_data_lock));

	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
				this_stat->mode, body_ptr, is_external);
	if (ret_val < 0)
		goto error_handling;

	meta_cache_get_meta_size(body_ptr, &new_metasize, &new_metasize_blk);

	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return ret_val;

	/* Init dir meta and page */
	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	/* Initialize new directory object and save the meta to meta cache */
	this_meta.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE) +
				    sizeof(CLOUD_RELATED_DATA);
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	this_meta.generation = this_gen;
        this_meta.source_arch = ARCH_CODE;
	this_meta.root_inode = root_ino;
	this_meta.finished_seq = 0;
	this_meta.local_pin = pin;
	write_log(10, "Debug: File %s inherits parent pin status = %d\n",
		selfname, pin);
	ret_val = init_dir_page(&temppage, self_inode, parent_inode,
						this_meta.root_entry_page);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		return ret_val;
	}

	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		ret_val = -errno;
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		return ret_val;
	}

	ret_val = meta_cache_open_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		return ret_val;
	}

	meta_cache_sync_later(body_ptr);
	ret_val = meta_cache_update_dir_data(self_inode, this_stat, &this_meta,
							NULL, body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		goto error_handling;
	}

	memset(&cloud_related_data, 0, sizeof(CLOUD_RELATED_DATA));
	MWRITE(body_ptr, &cloud_related_data, sizeof(CLOUD_RELATED_DATA),
	       sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE));

	ret_val = meta_cache_update_dir_data(self_inode, NULL, NULL,
			&temppage, body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->mode, is_external);
		goto error_handling;
	}
#ifdef _ANDROID_ENV_
	write_log(10, "Debug: inode %"PRIu64" begin to inherit xattrs\n",
			(uint64_t)self_inode);
	/* Inherit xattr from parent */
	ret_val = inherit_xattr(parent_inode, self_inode, body_ptr);
	if (ret_val < 0)
		write_log(0, "Error: inode %"PRIu64" fails to inherit"
				" xattrs from parent inode %"PRIu64".\n",
				(uint64_t)self_inode, (uint64_t)parent_inode);
#endif

	meta_cache_get_meta_size(body_ptr, &metasize, &metasize_blk);

	meta_cache_remove_sync_later(body_ptr);
	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0)
		return ret_val;

	/* Mark dirty here so that dirty data size includes dirty meta size. */
	super_block_mark_dirty(self_inode);

	/* Init the dir stat for this node */
	ret_val = reset_dirstat_lookup(self_inode);
	if (ret_val < 0) {
		return ret_val;
	}

	if (old_metasize > 0 && new_metasize > 0) {
		change_system_meta(0,
			(new_metasize_blk - old_metasize_blk) + metasize_blk,
			0, 0, 0, 0, FALSE);
		*delta_meta_size = (new_metasize - old_metasize) + metasize;
	} else {
		*delta_meta_size = metasize;
	}

	return 0;

errcode_handle:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return errcode;
error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;
}

/************************************************************************
*
* Function name: unlink_update_meta
*        Inputs: fuse_req_t req, ino_t parent_inode,
*                const DIR_ENTRY *this_entry
*       Summary: Helper of "hfuse_ll_unlink" function. It removes the inode
*                and name recorded in "this_entry" from "parent_inode".
*                Also will decrease the reference count for the inode.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t unlink_update_meta(fuse_req_t req,
			   ino_t parent_inode,
			   const DIR_ENTRY *const this_entry,
			   BOOL is_external)
{
	int32_t ret_val;
	ino_t this_inode;
	META_CACHE_ENTRY_STRUCT *parent_ptr, *self_ptr;
	DIR_STATS_TYPE tmpstat;
	char entry_type;
	mode_t this_mode;

	this_inode = this_entry->d_ino;

	parent_ptr = meta_cache_lock_entry(parent_inode);
	if (parent_ptr == NULL)
		return -errno;

	ret_val = update_meta_seq(parent_ptr);
	if (ret_val < 0)
		goto error_handling;

	self_ptr = meta_cache_lock_entry(this_inode);
	if (self_ptr == NULL)
		return -errno;

	entry_type = this_entry->d_type;
	/* Remove entry */
	ret_val = 0;
	switch (entry_type) {
	case D_ISREG:
		write_log(10, "Debug unlink_update_meta(): remove regfile.\n");
		this_mode = S_IFREG;
		break;
	case D_ISFIFO:
		write_log(10, "Debug unlink_update_meta(): remove fifo.\n");
		this_mode = S_IFIFO;
		break;
	case D_ISSOCK:
		write_log(10, "Debug unlink_update_meta(): remove socket.\n");
		this_mode = S_IFSOCK;
		break;
	case D_ISLNK:
		write_log(10, "Debug unlink_update_meta(): remove symlink.\n");
		this_mode = S_IFLNK;
		break;
	case D_ISDIR:
		write_log(0, "Error in unlink_update_meta(): unlink a dir.\n");
		ret_val = -EISDIR;
		break;
	default:
		ret_val = -EINVAL;
	}

	if (ret_val < 0)
		goto error_handling;

	/* Remove entry */
	ret_val = dir_remove_entry(parent_inode, this_inode,
				   this_entry->d_name, this_mode, parent_ptr,
				   is_external);
	if (ret_val < 0)
		goto error_handling;

	/* unlock meta cache */
	ret_val = meta_cache_close_file(parent_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(parent_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(parent_ptr);
	if (ret_val < 0)
		return ret_val;

	/* If file being unlinked, need to update dir statistics */
	if (entry_type == D_ISREG) {
		/* Check location for deleted file*/
		ret_val = meta_cache_open_file(self_ptr);
		if (ret_val < 0) {
			meta_cache_unlock_entry(self_ptr);
			return ret_val;
		}

		ret_val = check_file_storage_location(self_ptr->fptr, &tmpstat);
		if (ret_val < 0) {
			meta_cache_close_file(self_ptr);
			meta_cache_unlock_entry(self_ptr);
			return ret_val;
		}
	}

	ret_val = meta_cache_close_file(self_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(self_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(self_ptr);
	if (ret_val < 0)
		return ret_val;

	/* Process unlink for the file / symlink being unlinked */
	ret_val = decrease_nlink_inode_file(req, this_inode);
	if (ret_val < 0)
		return ret_val;

	/* Delete path lookup table */
	ret_val = sem_wait(&(pathlookup_data_lock));
	if (ret_val < 0)
		return ret_val;
	ret_val = lookup_delete_parent(this_inode, parent_inode);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret_val;
	}

	/* If file being unlinked, need to update dir statistics */
	if (entry_type == D_ISREG) {
		tmpstat.num_local = -tmpstat.num_local;
		tmpstat.num_cloud = -tmpstat.num_cloud;
		tmpstat.num_hybrid = -tmpstat.num_hybrid;
		ret_val = update_dirstat_parent(parent_inode, &tmpstat);
		if (ret_val < 0) {
			sem_post(&(pathlookup_data_lock));
			return ret_val;
		}
	}
	sem_post(&(pathlookup_data_lock));

	return ret_val;

error_handling:
	meta_cache_close_file(parent_ptr);
	meta_cache_unlock_entry(parent_ptr);

	return ret_val;
}

/************************************************************************
*
* Function name: rmdir_update_meta
*        Inputs: fuse_req_t req, ino_t parent_inode, ino_t this_inode,
*                char *selfname
*       Summary: Helper of "hfuse_rmdir" function. Will first check if
*                the directory pointed by "this_inode" is indeed empty.
*                If so, the name of "this_inode", "selfname", will be
*                removed from its parent "parent_inode", and meta of
*                "this_inode" will be deleted.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t rmdir_update_meta(fuse_req_t req, ino_t parent_inode, ino_t this_inode,
			const char *selfname, BOOL is_external)
{
	DIR_META_TYPE tempmeta;
	int32_t ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	/* Get meta and check whether it is empty */
	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL)
		return -errno;

	ret_val = meta_cache_lookup_dir_data(this_inode, NULL, &tempmeta,
							NULL, body_ptr);
	if (ret_val < 0)
		goto error_handling;

	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return ret_val;

	write_log(10, "TOTAL CHILDREN is now %lld\n", tempmeta.total_children);

	if (tempmeta.total_children > 0)
		return -ENOTEMPTY;

	/* Remove this directory from its parent */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -errno;
	ret_val = dir_remove_entry(parent_inode, this_inode, selfname, S_IFDIR,
				   body_ptr, is_external);
	if (ret_val < 0)
		goto error_handling;

	ret_val = update_meta_seq(body_ptr);
	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0)
		return ret_val;

	/* Deferring actual deletion to forget */
	ret_val = mark_inode_delete(req, this_inode);
	if (ret_val < 0)
		return ret_val;

	/* Delete parent lookup entry */
	ret_val = sem_wait(&(pathlookup_data_lock));
	if (ret_val < 0)
		return ret_val;
	ret_val = lookup_delete_parent(this_inode, parent_inode);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret_val;
	}

	sem_post(&(pathlookup_data_lock));

	reset_dirstat_lookup(this_inode);

	return ret_val;

error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	return ret_val;
}

/************************************************************************
*
* Function name: symlink_update_meta
*        Inputs: META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
*                const HCFS_STAT *this_stat, const char *link,
*                const uint64_t generation, const char *name
*                ino_t root_ino
*       Summary: Helper of "hfuse_ll_symlink". First prepare symlink_meta
*                and then use meta_cache_update_symlink() to update stat
*                and symlink_meta. After updating self meta, add a new
*                entry to parent dir.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
			    const HCFS_STAT *this_stat,
			    const char *link,
			    const uint64_t generation,
			    const char *name,
			    MOUNT_T *mountptr,
			    int64_t *delta_meta_size,
			    PIN_t pin,
			    BOOL is_external)
{
	META_CACHE_ENTRY_STRUCT *self_meta_cache_entry;
	SYMLINK_META_TYPE symlink_meta;
	ino_t parent_inode, self_inode;
	int32_t ret_code;
	int64_t metasize, old_metasize, new_metasize;
	int64_t metasize_blk, old_metasize_blk = 0, new_metasize_blk = 0;
	CLOUD_RELATED_DATA cloud_related_data;
	ino_t root_ino;

	root_ino = mountptr->f_ino;
	parent_inode = parent_meta_cache_entry->inode_num;
	self_inode = this_stat->ino;
	*delta_meta_size = 0;

	/* Add entry to parent dir. Do NOT need to lock parent meta cache entry
	   because it had been locked before calling this function. Just need to
	   open meta file. */
	ret_code = meta_cache_open_file(parent_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(parent_meta_cache_entry);
		return ret_code;
	}

	meta_cache_get_meta_size(parent_meta_cache_entry, &old_metasize,
			&old_metasize_blk);

	/* Add parent to lookup first */
	ret_code = sem_wait(&(pathlookup_data_lock));
	if (ret_code < 0)
		return ret_code;
	ret_code = lookup_add_parent(self_inode, parent_inode);
	if (ret_code < 0) {
		sem_post(&(pathlookup_data_lock));
		return ret_code;
	}
	sem_post(&(pathlookup_data_lock));

	ret_code = dir_add_entry(parent_inode, self_inode, name,
		this_stat->mode, parent_meta_cache_entry,
		is_external);
	if (ret_code < 0) {
		meta_cache_close_file(parent_meta_cache_entry);
		return ret_code;
	}

	meta_cache_get_meta_size(parent_meta_cache_entry, &new_metasize,
			&new_metasize_blk);

	ret_code = meta_cache_close_file(parent_meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Prepare symlink meta */
	memset(&symlink_meta, 0, sizeof(SYMLINK_META_TYPE));
	symlink_meta.link_len = strlen(link);
	symlink_meta.generation = generation;
        symlink_meta.source_arch = ARCH_CODE;
	symlink_meta.root_inode = root_ino;
	symlink_meta.finished_seq = 0;
	symlink_meta.local_pin = pin;
	memcpy(symlink_meta.link_path, link, sizeof(char) * strlen(link));
	write_log(10, "Debug: File %s inherits parent pin status = %d\n",
		name, pin);

	/* Update self meta data */
	self_meta_cache_entry = meta_cache_lock_entry(self_inode);
	if (self_meta_cache_entry == NULL) {
		ret_code = -errno;
		dir_remove_entry(parent_inode, self_inode, name,
			this_stat->mode, parent_meta_cache_entry,
			is_external);
		return ret_code;
	}

	ret_code = meta_cache_open_file(self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_unlock_entry(self_meta_cache_entry);
		dir_remove_entry(parent_inode, self_inode, name,
			this_stat->mode, parent_meta_cache_entry,
			is_external);
		return ret_code;
	}

	meta_cache_sync_later(self_meta_cache_entry);
	ret_code = meta_cache_update_symlink_data(self_inode, this_stat,
		&symlink_meta, self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(self_meta_cache_entry);
		meta_cache_unlock_entry(self_meta_cache_entry);
		dir_remove_entry(parent_inode, self_inode, name,
			this_stat->mode, parent_meta_cache_entry,
			is_external);
		return ret_code;
	}

	memset(&cloud_related_data, 0, sizeof(CLOUD_RELATED_DATA));
	MWRITE(self_meta_cache_entry, &cloud_related_data,
	       sizeof(CLOUD_RELATED_DATA),
	       sizeof(HCFS_STAT) + sizeof(SYMLINK_META_TYPE));

#ifdef _ANDROID_ENV_
	meta_cache_unlock_entry(parent_meta_cache_entry);

	write_log(10, "Debug: inode %"PRIu64" begin to inherit xattrs\n",
			(uint64_t)self_inode);
	/* Inherit xattr from parent */
	ret_code = inherit_xattr(parent_inode, self_inode,
			self_meta_cache_entry);
	if (ret_code < 0)
		write_log(0, "Error: inode %"PRIu64" fails to inherit"
				" xattrs from parent inode %"PRIu64".\n",
				(uint64_t)self_inode, (uint64_t)parent_inode);

	parent_meta_cache_entry = meta_cache_lock_entry(parent_inode);
	if (!parent_meta_cache_entry) {
		ret_code = -errno;
		meta_cache_close_file(self_meta_cache_entry);
		meta_cache_unlock_entry(self_meta_cache_entry);
		return ret_code;
	}
#endif
	meta_cache_get_meta_size(self_meta_cache_entry, &metasize,
			&metasize_blk);

	meta_cache_remove_sync_later(self_meta_cache_entry);
	ret_code = meta_cache_close_file(self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_unlock_entry(self_meta_cache_entry);
		return ret_code;
	}
	ret_code = meta_cache_unlock_entry(self_meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Mark dirty here so that dirty data size includes dirty meta size. */
	super_block_mark_dirty(self_inode);

	if (old_metasize > 0 && new_metasize > 0) {
		change_system_meta(0,
			(new_metasize_blk - old_metasize_blk) + metasize_blk,
			0, 0, 0, 0, FALSE);
		*delta_meta_size = (new_metasize - old_metasize) + metasize;
	} else {
		*delta_meta_size = metasize;
	}

	return 0;

errcode_handle:
	meta_cache_close_file(self_meta_cache_entry);
	meta_cache_unlock_entry(self_meta_cache_entry);

	return errcode;
}

/************************************************************************
*
* Function name: fetch_xattr_page
*        Inputs: META_CACHE_ENTRY_STRUCT *meta_cache_entry,
*                XATTR_PAGE *xattr_page, int64_t *xattr_pos
*       Summary: Helper of xattr operation in FUSE. The function aims to
*                fetch xattr page and xattr file position and store them
*                in "xattr_page" and "xattr_pos", respectively. Do NOT
*                have to lock and unlock meta cache entry since it will
*                be locked and unlocked in caller function.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, int64_t *xattr_pos, BOOL create_page)
{
	int32_t ret_code;
	ino_t this_inode;
	HCFS_STAT stat_data;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE symlinkmeta;
	int64_t ret_pos;

	this_inode = meta_cache_entry->inode_num;
	if (this_inode <= 0) {
		write_log(0, "Error: inode <= 0 in fetch_xattr_page()\n");
		return -EINVAL;
	}

	if (xattr_page == NULL) {
		write_log(0, "Error: cannot allocate memory of xattr_page\n");
		return -ENOMEM;
	}

	/* First lookup stat to confirm the file type. Do NOT need to
	   lock entry */
	ret_code = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Get metadata by case */
#ifdef _ANDROID_ENV_
	if (S_ISFILE(stat_data.mode)) {
#else
	if (S_ISREG(stat_data.mode)) {
#endif
		ret_code = meta_cache_lookup_file_data(this_inode, NULL,
			&filemeta, NULL, 0, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = filemeta.next_xattr_page;
	} else if (S_ISDIR(stat_data.mode)) {
		ret_code = meta_cache_lookup_dir_data(this_inode, NULL,
			&dirmeta, NULL, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = dirmeta.next_xattr_page;
	} else if (S_ISLNK(stat_data.mode)) {
		ret_code = meta_cache_lookup_symlink_data(this_inode, NULL,
			&symlinkmeta, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = symlinkmeta.next_xattr_page;
	} else { /* fifo, socket is not enabled in non-android env */
		return -EINVAL;
	}

	/* return ENOENT if do not need to create xattr page */
	if (*xattr_pos == 0 && create_page == FALSE)
		return -ENOENT;

	/* It is used to prevent user from forgetting to open meta file */
	ret_code = meta_cache_open_file(meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Allocate a xattr page if it is first time to insert xattr */
	if (*xattr_pos == 0) { /* No xattr before. Allocate new XATTR_PAGE */
		memset(xattr_page, 0, sizeof(XATTR_PAGE));
		FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
		ret_pos = FTELL(meta_cache_entry->fptr);
		*xattr_pos = ret_pos;
		MWRITE(meta_cache_entry, xattr_page, sizeof(XATTR_PAGE),
			*xattr_pos);

		/* Update xattr filepos in meta cache */
#ifdef _ANDROID_ENV_
		if (S_ISFILE(stat_data.mode)) {
#else
		if (S_ISREG(stat_data.mode)) {
#endif
			filemeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_file_data(this_inode, NULL,
				&filemeta, NULL, 0, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in "
				"regfile meta\n");
		}
		if (S_ISDIR(stat_data.mode)) {
			dirmeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_dir_data(this_inode, NULL,
				&dirmeta, NULL, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in dir meta\n");
		}
		if (S_ISLNK(stat_data.mode)) {
			symlinkmeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_symlink_data(this_inode,
				NULL, &symlinkmeta, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in symlink"
				" meta\n");
		}
	} else { /* xattr existed. Just read it. */
		MREAD(meta_cache_entry, xattr_page, sizeof(XATTR_PAGE),
		      *xattr_pos);
	}

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: link_update_meta
*        Inputs: ino_t link_inode, const char *newname,
*                HCFS_STAT *link_stat, uint64_t *generation,
*                META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry
*       Summary: Helper of link operation in FUSE. Given the inode numebr
*                "link_inode", this function will increase link number
*                and add a new entry to parent dir. This function will
*                also fetch inode stat and generation and store them in
*                "link_stat" and "generation", respectively. Do NOT need
*                to unlock parent meta cache entry since it will be
*                handled by caller.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t link_update_meta(ino_t link_inode,
			 const char *newname,
			 HCFS_STAT *link_stat,
			 uint64_t *generation,
			 META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
			 BOOL is_external)
{
	META_CACHE_ENTRY_STRUCT *link_entry;
	FILE_META_TYPE filemeta;
	int32_t ret_val;
	ino_t parent_inode;
	DIR_STATS_TYPE tmpstat;

	parent_inode = parent_meta_cache_entry->inode_num;

	link_entry = meta_cache_lock_entry(link_inode);
	if (!link_entry) {
		ret_val = -errno;
		write_log(0, "Lock entry fails in %s\n", __func__);
		return ret_val;
	}
	/* Fetch stat and file meta */
	ret_val = meta_cache_lookup_file_data(link_inode, link_stat,
			&filemeta, NULL, 0, link_entry);
	if (ret_val < 0)
		goto error_handle;

	*generation = filemeta.generation;

	/* Hard link to dir is not allowed */
	if (S_ISDIR(link_stat->mode)) {
		write_log(0, "Hard link to a dir is not allowed.\n");
		ret_val = -EISDIR;
		goto error_handle;
	}

	/* Check number of links */
	if (link_stat->nlink >= MAX_HARD_LINK) {
		write_log(0, "Too many links for this file, now %ld links",
			link_stat->nlink);
		ret_val = -EMLINK;
		goto error_handle;
	}

	link_stat->nlink++; /* Hard link ++ */
	write_log(10, "Debug: inode %lld has %lld links\n",
		link_inode, link_stat->nlink);

	ret_val = meta_cache_open_file(link_entry);
	if (ret_val < 0)
		goto error_handle;

	ret_val = meta_cache_update_file_data(link_inode, link_stat,
			NULL, NULL, 0, link_entry);
	if (ret_val < 0)
		goto error_handle;

	/* Add path lookup table */
	ret_val = sem_wait(&(pathlookup_data_lock));
	if (ret_val < 0)
		goto error_handle;
	ret_val = lookup_add_parent(link_inode, parent_inode);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		goto error_handle;
	}
	sem_post(&(pathlookup_data_lock));

	/* Add entry to this dir */
	ret_val = dir_add_entry(parent_inode, link_inode, newname,
		link_stat->mode, parent_meta_cache_entry,
		is_external);
	if (ret_val < 0) {
		link_stat->nlink--; /* Recover nlink */
		meta_cache_update_file_data(link_inode, link_stat,
			NULL, NULL, 0, link_entry);
		goto error_handle;
	}

	/* Check location for linked file and update parent to root */
	ret_val = check_file_storage_location(link_entry->fptr, &tmpstat);
	if (ret_val < 0)
		goto error_handle;

	sem_wait(&(pathlookup_data_lock));
	ret_val = update_dirstat_parent(parent_inode, &tmpstat);
	if (ret_val < 0) {
		sem_post(&(pathlookup_data_lock));
		goto error_handle;
	}
	sem_post(&(pathlookup_data_lock));

	/* Unlock meta cache entry */
	ret_val = meta_cache_close_file(link_entry);
	if (ret_val < 0) {
		meta_cache_unlock_entry(link_entry);
		return ret_val;
	}

	ret_val = meta_cache_unlock_entry(link_entry);
	if (ret_val < 0)
		return ret_val;

	return 0;

error_handle:
	meta_cache_close_file(link_entry);
	meta_cache_unlock_entry(link_entry);
	return ret_val;
}

/*
 * Helper used in pinning inode. This function will deduct pinning space from
 * reserved pinned size. If reserved pinned size is insufficient, then it will
 * increase system pinned space.
 */
int32_t increase_pinned_size(int64_t *reserved_pinned_size,
		int64_t file_size, char pin_type)
{
	int32_t ret;
	int64_t max_pinned_size;

	ret = 0;
	*reserved_pinned_size -= file_size; /*Deduct from pre-allocated quota*/
	if (*reserved_pinned_size <= 0) { /* Need more space than expectation */
		ret = 0;
		max_pinned_size = get_pinned_limit(pin_type);
		sem_wait(&(hcfs_system->access_sem));
		if ((max_pinned_size > 0) && (hcfs_system->systemdata.pinned_size -
			(*reserved_pinned_size) <= max_pinned_size)) {
			hcfs_system->systemdata.pinned_size -=
				(*reserved_pinned_size);
			*reserved_pinned_size = 0;

		} else {
			ret = -ENOSPC;
			*reserved_pinned_size += file_size; /* Recover */
		}
		sem_post(&(hcfs_system->access_sem));
	}

	write_log(10, "Debug: file size = %lld, reserved pinned size = %lld,"
		" system pinned size = %lld\n, in %s", file_size,
		*reserved_pinned_size,
		hcfs_system->systemdata.pinned_size, __func__);

	return ret;
}

/**
 * pin_inode
 *
 * Change local pin flag in meta cache to "pin_type" and set pin_status
 * in super block to ST_PINNING in case of regfile, ST_PIN for dir/ symlink.
 * When a regfile is set to ST_PINNING, it will be pushed into pinning queue
 * and all blocks will be fetched from cloud by other thread.
 *
 * @param this_inode The inode number that should be pinned.
 * @param reserved_pinned_size The reserved pinned quota had been deducted.
 *
 * @return 0 on success, 1 on case that regfile/symlink had been pinned,
 *         otherwise negative error code.
 */
int32_t pin_inode(ino_t this_inode,
		  int64_t *reserved_pinned_size,
		  char pin_type)
{
	int32_t ret;
	HCFS_STAT tempstat;
	ino_t *dir_node_list, *nondir_node_list;
	int64_t count, num_dir_node, num_nondir_node;

	memset(&tempstat, 0, sizeof(HCFS_STAT));
	ret = fetch_inode_stat(this_inode, &tempstat, NULL, NULL);
	if (ret < 0) {
		if (ret == -ENOENT) {
			write_log(4, "No entry in %s. Skip pinning meta %"
					PRIu64, __func__, (uint64_t)this_inode);
			return 0;
		}
		return ret;
	}

	ret = change_pin_flag(this_inode, tempstat.mode, pin_type);
	if (ret < 0) {
		return ret;

	} else if (ret > 0) {
	/* Do not need to change pinned size */
		write_log(8, "Debug: inode %"PRIu64" had been pinned\n",
							(uint64_t)this_inode);
	} else { /* Succeed in pinning */
		/* Change pinned size if succeding in pinning this inode. */
		if (S_ISREG(tempstat.mode)) {
			ret = increase_pinned_size(reserved_pinned_size,
					round_size(tempstat.size), pin_type);
			if (ret == -ENOSPC) {
				/* Roll back local_pin flag because the size
				had not been added to system pinned size */
				change_pin_flag(this_inode,
					tempstat.mode, P_UNPIN);
				return ret;
			}
		}

		ret = super_block_mark_pin(this_inode, tempstat.mode);
		if (ret < 0)
			return ret;
	}

	/* After pinning self, pin all its children for dir.
	 * Files(reg, fifo, socket) can be directly returned. */
	if (S_ISFILE(tempstat.mode)) {
		return ret;

	} else if (S_ISLNK(tempstat.mode)) {
		return ret;

	} else { /* expand dir */
		num_dir_node = 0;
		num_nondir_node = 0;
		dir_node_list = NULL;
		nondir_node_list = NULL;
		ret = collect_dir_children(this_inode, &dir_node_list,
			&num_dir_node, &nondir_node_list,
			&num_nondir_node, NULL, FALSE);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(4, "Folder is removed? Skip pinning"
					" meta %"PRIu64, (uint64_t)this_inode);
				return 0;
			}
			return ret;
		}

		/* first pin regfile & symlink */
		ret = 0;
		for (count = 0; count < num_nondir_node; count++) {
			ret = pin_inode(nondir_node_list[count],
				reserved_pinned_size, pin_type);
			if (ret < 0)
				break;
		}
		if (ret < 0) {
			free(nondir_node_list);
			free(dir_node_list);
			return ret; /* Return fail */
		}
		free(nondir_node_list);

		/* pin dir */
		ret = 0;
		for (count = 0; count < num_dir_node; count++) {
			ret = pin_inode(dir_node_list[count],
				reserved_pinned_size, pin_type);
			if (ret < 0)
				break;
		}
		if (ret < 0) {
			free(dir_node_list);
			return ret; /* Retuan fail */
		}
		free(dir_node_list);
	}

	return 0;
}

/*
 * This function will deduct pinned space from reserved_release_size. If space
 * is insufficient, it will decrease system pinned size.
 */
int32_t decrease_pinned_size(int64_t *reserved_release_size, int64_t file_size)
{
	*reserved_release_size -= file_size;
	if (*reserved_release_size < 0) {
		sem_wait(&(hcfs_system->access_sem));

		hcfs_system->systemdata.pinned_size += (*reserved_release_size);
		*reserved_release_size = 0;
		if (hcfs_system->systemdata.pinned_size <= 0)
			hcfs_system->systemdata.pinned_size = 0;

		sem_post(&(hcfs_system->access_sem));
	}

	write_log(10, "Debug: file size = %lld, reserved release size = %lld,"
		" system pinned size = %lld\n, in %s", file_size,
		*reserved_release_size,
		hcfs_system->systemdata.pinned_size, __func__);

	return 0;
}


/**
 * unpin_inode
 *
 * Unpin an pinned file so that it can be paged out and release more
 * available space. An inode will be check pin flag in meta cache
 * and then let pin_status in super block entry be ST_UNPIN if succeeds
 * in unpinning this inode. In case that inode is a regfile with
 * pin_status "ST_PINNING", it will be dequeued from pinning queue in
 * "super_block_unpin". When inode is a dir, all of its children will
 * be unpinned recursively.
 *
 * @param this_inode The inode number that should be unpinned.
 * @param reserved_release_size The reserved pinned quota being
 *        going to release.
 *
 * @return 0 on success, 1 on case that regfile/symlink had been unpinned,
 *         otherwise negative error code.
 */

int32_t unpin_inode(ino_t this_inode, int64_t *reserved_release_size)
{
	int32_t ret, semval;
	HCFS_STAT tempstat;
	ino_t *dir_node_list, *nondir_node_list;
	int64_t count, num_dir_node, num_nondir_node;

	memset(&tempstat, 0, sizeof(HCFS_STAT));
	ret = fetch_inode_stat(this_inode, &tempstat, NULL, NULL);
	if (ret < 0) {
		if (ret == -ENOENT) {
			write_log(4, "No entry in %s. Skip pinning meta %"
					PRIu64, __func__, (uint64_t)this_inode);
			return 0;
		}
		return ret;
	}

	ret = change_pin_flag(this_inode, tempstat.mode, P_UNPIN);
	if (ret < 0) {
		write_log(0, "Error: Fail to unpin inode %"PRIu64"."
			" Code %d\n", (uint64_t)-ret);
		return ret;

	} else if (ret > 0) {
	/* Do not need to change pinned size */
		write_log(5, "Debug: inode %"PRIu64" had been unpinned\n",
			(uint64_t)this_inode);

	} else { /* Succeed in unpinning */

		/* Deduct from reserved size */
		if (S_ISREG(tempstat.mode)) {
			decrease_pinned_size(reserved_release_size,
			 		round_size(tempstat.size));
		}

		ret = super_block_mark_unpin(this_inode, tempstat.mode);
		if (ret < 0)
			return ret;
	}

	/* After unpinning itself, unpin all its children for dir */
	if (S_ISFILE(tempstat.mode)) {
		/* First post the control for cache mangement */
		semval = 0;
		ret = sem_check_and_release(&(hcfs_system->something_to_replace),
		                            &semval);
		return ret;

	} else if (S_ISLNK(tempstat.mode)) {
		return ret;

	} else { /* expand dir */
		num_dir_node = 0;
		num_nondir_node = 0;
		dir_node_list = NULL;
		nondir_node_list = NULL;

		ret = collect_dir_children(this_inode, &dir_node_list,
			&num_dir_node, &nondir_node_list,
			&num_nondir_node, NULL, TRUE);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(4, "Folder is removed? Skip pinning"
					" meta %"PRIu64, (uint64_t)this_inode);
				return 0;
			}
			return ret;
		}

		/* first unpin regfile & symlink */
		ret = 0;
		for (count = 0; count < num_nondir_node; count++) {
			ret = unpin_inode(nondir_node_list[count],
				reserved_release_size);
			if (ret < 0)
				break;
		}
		if (ret < 0) {
			free(nondir_node_list);
			free(dir_node_list);
			return ret;
		}
		free(nondir_node_list);

		/* unpin dir */
		ret = 0;
		for (count = 0; count < num_dir_node; count++) {
			ret = unpin_inode(dir_node_list[count],
				reserved_release_size);
			if (ret < 0)
				break;
		}
		if (ret < 0) {
			free(dir_node_list);
			return ret;
		}
		free(dir_node_list);
	}

	return 0;
}

/**
 * Set uploading data in meta cache.
 *
 * This function aims to clone meta file and set uploading info in meta cache.
 * If is_uploading is TRUE, then:
 *   Case 1: Copy local meta to to-upload meta if NOT revert mode
 *   Case 2: Open progress file and read # of to-upload blocks
 *   - Finally set uploading info in meta cache.
 * else, if is_uploading is FALSE, then:
 *   - Set uploading info in meta cache.
 *   - Update upload_seq if finish_sync is TRUE.
 *
 * @param data Uploading info
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t fuseproc_set_uploading_info(const UPLOADING_COMMUNICATION_DATA *data)
{
	int32_t ret;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	char toupload_metapath[300], local_metapath[300];
	PROGRESS_META progress_meta;
	HCFS_STAT tmpstat;
	int32_t errcode;
	int64_t toupload_blocks;

	meta_cache_entry = meta_cache_lock_entry(data->inode);
	if (!meta_cache_entry) {
		errcode = -errno;
		write_log(0, "Fail to lock meta cache entry in %s\n", __func__);
		return errcode;
	}
	ret = meta_cache_open_file(meta_cache_entry);
	if (ret < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		return ret;
	}

	/* Copy meta if need to upload and is not reverting mode */
	if (data->is_uploading == TRUE) {

		/* Read toupload_blocks in reverting/continuing mode */
		if (data->is_revert == TRUE) {
			flock(data->progress_list_fd, LOCK_EX);
			PREAD(data->progress_list_fd, &progress_meta,
					sizeof(PROGRESS_META), 0);
			flock(data->progress_list_fd, LOCK_UN);
			toupload_blocks = progress_meta.total_toupload_blocks;

		/* clone meta and record # of toupload_blocks */
		} else {
			fetch_meta_path(local_metapath, data->inode);
			ret = access(local_metapath, F_OK);
			if (ret < 0) {
				write_log(2, "meta %"PRIu64" not exist. Code %d in %s\n",
						(uint64_t)data->inode, ret, __func__);
				errcode = ret;
				goto errcode_handle;
			}

			fetch_toupload_meta_path(toupload_metapath,
					data->inode);
			if (access(toupload_metapath, F_OK) == 0) {
				write_log(2, "Cannot copy since "
						"%s exists", toupload_metapath);
				ret = unlink(toupload_metapath);
				if (ret < 0)
					write_log(0, "Error: Fail to unlink in "
						"%s. Code %d", __func__, errno);
			}
			ret = check_and_copy_file(local_metapath,
					toupload_metapath, FALSE, FALSE);
			if (ret < 0) {
				meta_cache_close_file(meta_cache_entry);
				meta_cache_unlock_entry(meta_cache_entry);
				return ret;
			}

			ret = meta_cache_lookup_file_data(data->inode, &tmpstat,
					NULL, NULL, 0, meta_cache_entry);
			if (ret < 0) {
				meta_cache_close_file(meta_cache_entry);
				meta_cache_unlock_entry(meta_cache_entry);
				return ret;
			}

			/* Update info of to-upload blocks and size */
			if (S_ISREG(tmpstat.mode)) {
				flock(data->progress_list_fd, LOCK_EX);
				PREAD(data->progress_list_fd, &progress_meta,
						sizeof(PROGRESS_META), 0);
				progress_meta.toupload_size = tmpstat.size;
				toupload_blocks = BLOCKS_OF_SIZE(
				    tmpstat.size, MAX_BLOCK_SIZE);
				progress_meta.total_toupload_blocks =
						toupload_blocks;
				PWRITE(data->progress_list_fd, &progress_meta,
						sizeof(PROGRESS_META), 0);
				flock(data->progress_list_fd, LOCK_UN);

				write_log(10, "Debug: toupload_size %lld,"
					" total_toupload_blocks %lld\n",
					progress_meta.toupload_size,
					progress_meta.total_toupload_blocks);

			} else {
				toupload_blocks = 0;
			}
		}
	}

	/* Set uploading information */
	if (data->is_uploading == TRUE) {
		ret = meta_cache_set_uploading_info(meta_cache_entry,
			TRUE, data->progress_list_fd,
			toupload_blocks);
	} else {
		ret = meta_cache_set_uploading_info(meta_cache_entry,
			FALSE, 0, 0);
	}
	if (ret < 0) {
		write_log(0, "Fail to set uploading info in %s\n", __func__);
		meta_cache_close_file(meta_cache_entry);
		meta_cache_unlock_entry(meta_cache_entry);
		return ret;
	}

	/* Unlock meta cache */
	ret = meta_cache_close_file(meta_cache_entry);
	if (ret < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		return ret;
	}
	ret = meta_cache_unlock_entry(meta_cache_entry);
	if (ret < 0) {
		write_log(0, "Fail to unlock entry in %s\n", __func__);
		return ret;
	}

	return 0;

errcode_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	return errcode;
}

