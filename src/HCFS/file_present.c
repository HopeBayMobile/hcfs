/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: file_present.c
* Abstract: The c source code file for meta processing involving regular
*           files and directories in HCFS. "file_present" means
*           "file-level presentation".
*
* Revision History
* 2015/2/5 Jiahong added header for this file, and revising coding style.
* 2015/6/2 Jiahong added error handling
* 2015/6/16 Kewei added function fetch_xattr_page().
* 2015/7/9 Kewei added function symlink_update_meta().
*
**************************************************************************/

#include "file_present.h"

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>

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
int meta_forget_inode(ino_t self_inode)
{
	char thismetapath[METAPATHLEN];
	int ret, errcode;

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
*        Inputs: ino_t this_inode, struct stat *inode_stat
*       Summary: Read inode "struct stat" from meta cache, and then return
*                to the caller via "inode_stat".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat,
		unsigned long *ret_gen)
{
	struct stat returned_stat;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *temp_entry;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE symlinkmeta;

	/*First will try to lookup meta cache*/
	if (this_inode > 0) {
		temp_entry = meta_cache_lock_entry(this_inode);
		if (temp_entry == NULL)
			return -ENOMEM;

		/* Only fetch inode stat, so does not matter if inode is reg
		*  file or dir here*/
		ret_code = meta_cache_lookup_file_data(this_inode,
				&returned_stat, NULL, NULL, 0, temp_entry);
		if (ret_code < 0)
			goto error_handling;

		if (ret_gen != NULL) {
			if (S_ISREG(returned_stat.st_mode)) {
				ret_code = meta_cache_lookup_file_data(
						this_inode, NULL, &filemeta,
						NULL, 0, temp_entry);
				if (ret_code < 0)
					goto error_handling;
				*ret_gen = filemeta.generation;
			}
			if (S_ISDIR(returned_stat.st_mode)) {
				ret_code = meta_cache_lookup_dir_data(
						this_inode, NULL, &dirmeta,
						NULL, temp_entry);
				if (ret_code < 0)
					goto error_handling;
				*ret_gen = dirmeta.generation;
			}
			if (S_ISLNK(returned_stat.st_mode)) {
				ret_code = meta_cache_lookup_symlink_data(
						this_inode, NULL, &symlinkmeta,
						temp_entry);
				if (ret_code < 0)
					goto error_handling;
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
			memcpy(inode_stat, &returned_stat, sizeof(struct stat));
			write_log(10, "fetch_inode_stat get inode %lld\n",
				inode_stat->st_ino);
			return 0;
		}

		if (ret_code < 0)
			return ret_code;
	} else {
		return -ENOENT;
	}

	write_log(10, "fetch_inode_stat get only generation %lld\n", *ret_gen);
	return 0;

error_handling:
	meta_cache_close_file(temp_entry);
	meta_cache_unlock_entry(temp_entry);
	return ret_code;
}

/* Remove entry when this child inode fail to create meta */
static inline int dir_remove_fail_node(ino_t parent_inode, ino_t child_inode,
	const char *childname, mode_t child_mode)
{
	META_CACHE_ENTRY_STRUCT *tmp_bodyptr;

	tmp_bodyptr = meta_cache_lock_entry(parent_inode);
	dir_remove_entry(parent_inode, child_inode, childname, child_mode,
		tmp_bodyptr);
	meta_cache_unlock_entry(tmp_bodyptr);
	return 0;
}

/************************************************************************
*
* Function name: mknod_update_meta
*        Inputs: ino_t self_inode, ino_t parent_inode, char *selfname,
*                struct stat *this_stat, ino_t root_ino
*       Summary: Helper of "hfuse_mknod" function. Will save the inode stat
*                of the newly create regular file to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen,
			ino_t root_ino)
{
	int ret_val, ret, errcode;
	size_t ret_size;
	FILE_META_TYPE this_meta;
	FILE_STATS_TYPE file_stats;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	char pin_status;
	DIR_META_TYPE parent_meta;

	/* Add "self_inode" to its parent "parent_inode" */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

	ret_val = meta_cache_lookup_dir_data(parent_inode, NULL,
		&parent_meta, NULL, body_ptr);
	if (ret_val < 0)
		goto error_handling;
	pin_status = parent_meta.local_pin; /* Inherit parent pin-status */

	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
						this_stat->st_mode, body_ptr);
	if (ret_val < 0)
		goto error_handling;

	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = meta_cache_unlock_entry(body_ptr);

	/* Init file meta */
	memset(&this_meta, 0, sizeof(FILE_META_TYPE));
	this_meta.generation = this_gen;
	this_meta.metaver = CURRENT_META_VER;
        this_meta.source_arch = ARCH_CODE;
	this_meta.root_inode = root_ino;
	this_meta.local_pin = pin_status;
	write_log(10, "Debug: File %s inherits parent pin status = %s\n",
		selfname, pin_status == TRUE? "PIN" : "UNPIN");

	/* Store the inode and file meta of the new file to meta cache */
	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		return -ENOMEM;
	}

	ret_val = meta_cache_update_file_data(self_inode, this_stat, &this_meta,
							NULL, 0, body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		goto error_handling;
	}

	ret_val = meta_cache_open_file(body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		goto error_handling;
	}
	memset(&file_stats, 0, sizeof(FILE_STATS_TYPE));
	FSEEK(body_ptr->fptr, sizeof(struct stat) + sizeof(FILE_META_TYPE),
		SEEK_SET);
	FWRITE(&file_stats, sizeof(FILE_STATS_TYPE), 1, body_ptr->fptr);
	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return ret_val;

	/* Add path lookup table */
#ifdef _ANDROID_ENV_
	ret_val = pathlookup_write_parent(self_inode, parent_inode);
	if (ret_val < 0)
		return ret_val;
#endif

	return 0;
errcode_handle:
	dir_remove_fail_node(parent_inode, self_inode,
				selfname, this_stat->st_mode);
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
*                struct stat *this_stat, ino_t root_ino
*       Summary: Helper of "hfuse_mkdir" function. Will save the inode stat
*                of the newly create directory object to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen,
			ino_t root_ino)
{
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	char pin_status;
	DIR_META_TYPE parent_meta;

	/* Save the new entry to its parent and update meta */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;
	
	ret_val = meta_cache_lookup_dir_data(parent_inode, NULL,
		&parent_meta, NULL, body_ptr);
	if (ret_val < 0)
		goto error_handling;
	pin_status = parent_meta.local_pin; /* Inherit parent pin-status */
	
	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
						this_stat->st_mode, body_ptr);
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

	/* Init dir meta and page */
	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	/* Initialize new directory object and save the meta to meta cache */
	this_meta.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	this_meta.generation = this_gen;
	this_meta.metaver = CURRENT_META_VER;
        this_meta.source_arch = ARCH_CODE;
	this_meta.root_inode = root_ino;
	this_meta.local_pin = pin_status;	
	write_log(10, "Debug: File %s inherits parent pin status = %s\n",
		selfname, pin_status == TRUE? "PIN" : "UNPIN");
	ret_val = init_dir_page(&temppage, self_inode, parent_inode,
						this_meta.root_entry_page);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		return ret_val;
	}

	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		return -ENOMEM;
	}

	ret_val = meta_cache_update_dir_data(self_inode, this_stat, &this_meta,
							&temppage, body_ptr);
	if (ret_val < 0) {
		dir_remove_fail_node(parent_inode, self_inode,
			selfname, this_stat->st_mode);
		goto error_handling;
	}
	ret_val = meta_cache_close_file(body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	ret_val = meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0)
		return ret_val;

#ifdef _ANDROID_ENV_
	ret_val = pathlookup_write_parent(self_inode, parent_inode);
	if (ret_val < 0)
		return ret_val;
#endif

	return 0;

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
int unlink_update_meta(fuse_req_t req, ino_t parent_inode,
			const DIR_ENTRY *this_entry)
{
	int ret_val;
	ino_t this_inode;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	this_inode = this_entry->d_ino;

	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

	/* Remove entry */
	if (this_entry->d_type == D_ISREG) {
		write_log(10, "Debug unlink_update_meta(): remove regfile.\n");
		ret_val = dir_remove_entry(parent_inode, this_inode,
			this_entry->d_name, S_IFREG, body_ptr);
	}
	if (this_entry->d_type == D_ISLNK) {
		write_log(10, "Debug unlink_update_meta(): remove symlink.\n");
		ret_val = dir_remove_entry(parent_inode, this_inode,
			this_entry->d_name, S_IFLNK, body_ptr);
	}
	if (this_entry->d_type == D_ISDIR) {
		write_log(0, "Error in unlink_update_meta(): unlink a dir.\n");
		ret_val = -EISDIR;
	}
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

	ret_val = decrease_nlink_inode_file(req, this_inode);

	return ret_val;

error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

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
int rmdir_update_meta(fuse_req_t req, ino_t parent_inode, ino_t this_inode,
			const char *selfname)
{
	DIR_META_TYPE tempmeta;
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

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
		return -ENOMEM;
	ret_val = dir_remove_entry(parent_inode, this_inode, selfname, S_IFDIR,
								body_ptr);
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

	/* Deferring actual deletion to forget */
	ret_val = mark_inode_delete(req, this_inode);

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
*                const struct stat *this_stat, const char *link,
*                const unsigned long generation, const char *name
*                ino_t root_ino
*       Summary: Helper of "hfuse_ll_symlink". First prepare symlink_meta
*                and then use meta_cache_update_symlink() to update stat
*                and symlink_meta. After updating self meta, add a new
*                entry to parent dir.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
	const struct stat *this_stat, const char *link,
	const unsigned long generation, const char *name, ino_t root_ino)
{
	META_CACHE_ENTRY_STRUCT *self_meta_cache_entry;
	SYMLINK_META_TYPE symlink_meta;
	ino_t parent_inode, self_inode;
	int ret_code;
	char pin_status;
	DIR_META_TYPE parent_meta;

	parent_inode = parent_meta_cache_entry->inode_num;
	self_inode = this_stat->st_ino;

	/* Add entry to parent dir. Do NOT need to lock parent meta cache entry
	   because it had been locked before calling this function. Just need to
	   open meta file. */	
	ret_code = meta_cache_lookup_dir_data(parent_inode, NULL,
		&parent_meta, NULL, parent_meta_cache_entry);
	if (ret_code < 0)
		return ret_code;
	pin_status = parent_meta.local_pin; /* Inherit parent pin-status */

	ret_code = meta_cache_open_file(parent_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(parent_meta_cache_entry);
		return ret_code;
	}

	ret_code = dir_add_entry(parent_inode, self_inode, name,
		this_stat->st_mode, parent_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(parent_meta_cache_entry);
		return ret_code;
	}

	ret_code = meta_cache_close_file(parent_meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Prepare symlink meta */
	memset(&symlink_meta, 0, sizeof(SYMLINK_META_TYPE));
	symlink_meta.link_len = strlen(link);
	symlink_meta.generation = generation;
	symlink_meta.metaver = CURRENT_META_VER;
        symlink_meta.source_arch = ARCH_CODE;
	symlink_meta.root_inode = root_ino;
	symlink_meta.local_pin = pin_status;
	memcpy(symlink_meta.link_path, link, sizeof(char) * strlen(link));
	write_log(10, "Debug: File %s inherits parent pin status = %s\n",
		name, pin_status == TRUE? "PIN" : "UNPIN");

	/* Update self meta data */
	self_meta_cache_entry = meta_cache_lock_entry(self_inode);
	if (self_meta_cache_entry == NULL) {
		dir_remove_entry(parent_inode, self_inode, name,
			this_stat->st_mode, parent_meta_cache_entry);
		return -ENOMEM;
	}

	ret_code = meta_cache_update_symlink_data(self_inode, this_stat,
		&symlink_meta, self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(self_meta_cache_entry);
		meta_cache_unlock_entry(self_meta_cache_entry);
		dir_remove_entry(parent_inode, self_inode, name,
			this_stat->st_mode, parent_meta_cache_entry);
		return ret_code;
	}

	ret_code = meta_cache_close_file(self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_unlock_entry(self_meta_cache_entry);
		return ret_code;
	}
	ret_code = meta_cache_unlock_entry(self_meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

#ifdef _ANDROID_ENV_
	ret_code = pathlookup_write_parent(self_inode, parent_inode);
	if (ret_code < 0)
		return ret_code;
#endif

	return 0;
}

/************************************************************************
*
* Function name: fetch_xattr_page
*        Inputs: META_CACHE_ENTRY_STRUCT *meta_cache_entry,
*                XATTR_PAGE *xattr_page, long long *xattr_pos
*       Summary: Helper of xattr operation in FUSE. The function aims to
*                fetch xattr page and xattr file position and store them
*                in "xattr_page" and "xattr_pos", respectively. Do NOT
*                have to lock and unlock meta cache entry since it will
*                be locked and unlocked in caller function.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, long long *xattr_pos)
{
	int ret_code;
	ino_t this_inode;
	struct stat stat_data;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE symlinkmeta;
	int errcode;
	int ret;
	long long ret_pos, ret_size;

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
	if (S_ISREG(stat_data.st_mode)) {
		ret_code = meta_cache_lookup_file_data(this_inode, NULL,
			&filemeta, NULL, 0, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = filemeta.next_xattr_page;
	}
	if (S_ISDIR(stat_data.st_mode)) {
		ret_code = meta_cache_lookup_dir_data(this_inode, NULL,
			&dirmeta, NULL, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = dirmeta.next_xattr_page;
	}
	if (S_ISLNK(stat_data.st_mode)) {
		ret_code = meta_cache_lookup_symlink_data(this_inode, NULL,
			&symlinkmeta, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = symlinkmeta.next_xattr_page;
	}

	/* It is used to prevent user from forgetting to open meta file */
	ret_code = meta_cache_open_file(meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	ret_code = flush_single_entry(meta_cache_entry);
	if (ret_code < 0)
		return ret_code;

	/* Allocate a xattr page if it is first time to insert xattr */
	if (*xattr_pos == 0) { /* No xattr before. Allocate new XATTR_PAGE */
		memset(xattr_page, 0, sizeof(XATTR_PAGE));
		FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
		FTELL(meta_cache_entry->fptr);
		*xattr_pos = ret_pos;
		FWRITE(xattr_page, sizeof(XATTR_PAGE), 1,
			meta_cache_entry->fptr);

		/* Update xattr filepos in meta cache */
		if (S_ISREG(stat_data.st_mode)) {
			filemeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_file_data(this_inode, NULL,
				&filemeta, NULL, 0, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in "
				"regfile meta\n");
		}
		if (S_ISDIR(stat_data.st_mode)) {
			dirmeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_dir_data(this_inode, NULL,
				&dirmeta, NULL, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in dir meta\n");
		}
		if (S_ISLNK(stat_data.st_mode)) {
			symlinkmeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_symlink_data(this_inode,
				NULL, &symlinkmeta, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
			write_log(10, "Debug: A new xattr page in symlink"
				" meta\n");
		}
	} else { /* xattr has been existed. Just read it. */
		FSEEK(meta_cache_entry->fptr, *xattr_pos, SEEK_SET);
		FREAD(xattr_page, sizeof(XATTR_PAGE), 1,
			meta_cache_entry->fptr);
	}

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: link_update_meta
*        Inputs: ino_t link_inode, const char *newname,
*                struct stat *link_stat, unsigned long *generation,
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
int link_update_meta(ino_t link_inode, const char *newname,
	struct stat *link_stat, unsigned long *generation,
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry)
{
	META_CACHE_ENTRY_STRUCT *link_meta_cache_entry;
	struct link_stat;
	int ret_val;
	ino_t parent_inode;

	parent_inode = parent_meta_cache_entry->inode_num;

	/* Fetch stat and generation */
	ret_val = fetch_inode_stat(link_inode, link_stat, generation);
	if (ret_val < 0)
		return ret_val;

	/* Hard link to dir is not allowed */
	if (S_ISDIR(link_stat->st_mode)) {
		write_log(0, "Hard link to a dir is not allowed.\n");
		return -EISDIR;
	}

	/* Check number of links */
	if (link_stat->st_nlink >= MAX_HARD_LINK) {
		write_log(0, "Too many links for this file, now %ld links",
			link_stat->st_nlink);
		return -EMLINK;
	}

	link_stat->st_nlink++; /* Hard link ++ */
	write_log(10, "Debug: inode %lld has %lld links\n",
		link_inode, link_stat->st_nlink);

	/* Update only stat */
	link_meta_cache_entry = meta_cache_lock_entry(link_inode);
	if (!link_meta_cache_entry) {
		write_log(0, "Lock entry fails in %s\n", __func__);
		return -ENOMEM;
	}
	ret_val = meta_cache_update_file_data(link_inode, link_stat,
			NULL, NULL, 0, link_meta_cache_entry);
	if (ret_val < 0)
		goto error_handle;

	/* Add entry to this dir */
	ret_val = dir_add_entry(parent_inode, link_inode, newname,
		link_stat->st_mode, parent_meta_cache_entry);
	if (ret_val < 0) {
		link_stat->st_nlink--; /* Recover nlink */
		meta_cache_update_file_data(link_inode, link_stat,
			NULL, NULL, 0, link_meta_cache_entry);
		goto error_handle;
	}

	/* Unlock meta cache entry */
	ret_val = meta_cache_close_file(link_meta_cache_entry);
	if (ret_val < 0) {
		meta_cache_unlock_entry(link_meta_cache_entry);
		return ret_val;
	}
	ret_val = meta_cache_unlock_entry(link_meta_cache_entry);
	if (ret_val < 0)
		return ret_val;

	return 0;

error_handle:
	meta_cache_close_file(link_meta_cache_entry);
	meta_cache_unlock_entry(link_meta_cache_entry);
	return ret_val;
}

int pin_inode(ino_t this_inode)
{
	int ret, ret2;
	struct stat tempstat;

	ret = fetch_inode_stat(this_inode, &tempstat, NULL);
	if (ret < 0)
		return ret;
	
	ret = change_pin_flag(this_inode, tempstat.st_mode, TRUE);
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
#ifdef ARM_32bit
		write_log(5, "Debug: inode %lld had been pinned\n", this_inode);
#else
		write_log(5, "Debug: inode %ld had been pinned\n", this_inode);
#endif
		return ret;
	}

	/* Check all blocks are in local cache if it is regfile */
	if (S_ISREG(tempstat.st_mode)) {
		ret = fetch_pinned_blocks(this_inode);
		if (ret < 0) {
			ret2 = change_pin_flag(this_inode, tempstat.st_mode, FALSE);
#ifdef ARM_32bit
			write_log(0, "Error: Fail to pin inode %lld.\n", this_inode);
#else
			write_log(0, "Error: Fail to pin inode %ld.\n", this_inode);
#endif
			write_log(0, "Code %d in %s.\n", -ret, __func__);
			return ret;
		}
	}

	return 0;
}

int unpin_inode(ino_t this_inode)
{
	int ret, ret2;
	struct stat tempstat;

	ret = fetch_inode_stat(this_inode, &tempstat, NULL);
	if (ret < 0)
		return ret;
	
	ret = change_pin_flag(this_inode, tempstat.st_mode, FALSE);
	if (ret < 0) {
#ifdef ARM_32bit
		write_log(0, "Error: Fail to unpin inode %lld. Code %d\n", -ret);
#else
		write_log(0, "Error: Fail to unpin inode %ld. Code %d\n", -ret);
#endif
		return ret;

	} else if (ret > 0) {
#ifdef ARM_32bit
		write_log(5, "Debug: inode %lld had been unpinned\n", this_inode);
#else
		write_log(5, "Debug: inode %ld had been unpinned\n", this_inode);
#endif
		return ret;
	}

	return 0;
}

