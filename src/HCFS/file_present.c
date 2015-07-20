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

		if (ret_code == 0) {
			memcpy(inode_stat, &returned_stat, sizeof(struct stat));
			return 0;
		}

		if (ret_code < 0)
			return ret_code;
	} else {
		return -ENOENT;
	}

	write_log(10, "fetch_inode_stat %lld\n", inode_stat->st_ino);

	return 0;

error_handling:
	meta_cache_close_file(temp_entry);
	meta_cache_unlock_entry(temp_entry);
	return ret_code;
}

/************************************************************************
*
* Function name: mknod_update_meta
*        Inputs: ino_t self_inode, ino_t parent_inode, char *selfname,
*                struct stat *this_stat
*       Summary: Helper of "hfuse_mknod" function. Will save the inode stat
*                of the newly create regular file to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen)
{
	int ret_val;
	FILE_META_TYPE this_meta;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	memset(&this_meta, 0, sizeof(FILE_META_TYPE));

	this_meta.generation = this_gen;
	/* Store the inode and file meta of the new file to meta cache */
	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

	ret_val = meta_cache_update_file_data(self_inode, this_stat, &this_meta,
							NULL, 0, body_ptr);
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

	/* Add "self_inode" to its parent "parent_inode" */
	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;
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

	return 0;

error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	return ret_val;
}

/************************************************************************
*
* Function name: mkdir_update_meta
*        Inputs: ino_t self_inode, ino_t parent_inode, char *selfname,
*                struct stat *this_stat
*       Summary: Helper of "hfuse_mkdir" function. Will save the inode stat
*                of the newly create directory object to meta cache, and also
*                add this new entry to its parent.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen)
{
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	/* Initialize new directory object and save the meta to meta cache */
	this_meta.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	this_meta.generation = this_gen;
	ret_val = init_dir_page(&temppage, self_inode, parent_inode,
						this_meta.root_entry_page);
	if (ret_val < 0)
		return ret_val;

	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

	ret_val = meta_cache_update_dir_data(self_inode, this_stat, &this_meta,
							&temppage, body_ptr);
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

	/* Save the new entry to its parent and update meta */
	body_ptr = meta_cache_lock_entry(parent_inode);
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

	return 0;

error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;

}

/************************************************************************
*
* Function name: unlink_update_meta
*        Inputs: ino_t parent_inode, ino_t this_inode, char *selfname
*       Summary: Helper of "hfuse_unlink" function. Will delete "this_inode"
*                (with name "selfname" from its parent "parent_inode".
*                Also will decrease the reference count for "this_inode".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int unlink_update_meta(ino_t parent_inode, ino_t this_inode,
				const char *selfname)
{
	int ret_val;
	struct stat this_stat;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	
	ret_val = fetch_inode_stat(this_inode, &this_stat, NULL);
	if (ret_val < 0)
		return ret_val;

	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;
	
	/* Remove entry */	
	if (S_ISREG(this_stat.st_mode)) {
		write_log(10, "Debug unlink_update_meta(): remove regfile.\n");
		ret_val = dir_remove_entry(parent_inode, this_inode, selfname,
							S_IFREG, body_ptr);
	}
	if (S_ISLNK(this_stat.st_mode)) {
		write_log(10, "Debug unlink_update_meta(): remove symlink.\n");
		ret_val = dir_remove_entry(parent_inode, this_inode, selfname,
							S_IFLNK, body_ptr);
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

	ret_val = decrease_nlink_inode_file(this_inode);

	return ret_val;

error_handling:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;
}

/************************************************************************
*
* Function name: rmdir_update_meta
*        Inputs: ino_t parent_inode, ino_t this_inode, char *selfname
*       Summary: Helper of "hfuse_rmdir" function. Will first check if
*                the directory pointed by "this_inode" is indeed empty.
*                If so, the name of "this_inode", "selfname", will be
*                removed from its parent "parent_inode", and meta of
*                "this_inode" will be deleted.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int rmdir_update_meta(ino_t parent_inode, ino_t this_inode,
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

	write_log(10, "TOTAL CHILDREN is now %ld\n", tempmeta.total_children);

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
	ret_val = mark_inode_delete(this_inode);

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
	const unsigned long generation, const char *name)
{
	META_CACHE_ENTRY_STRUCT *self_meta_cache_entry;
	SYMLINK_META_TYPE symlink_meta;
	ino_t parent_inode, self_inode;
	int ret_code;

	parent_inode = parent_meta_cache_entry->inode_num;
	self_inode = this_stat->st_ino;
	
	/* Prepare symlink meta */
	memset(&symlink_meta, 0, sizeof(SYMLINK_META_TYPE));
	symlink_meta.link_len = strlen(link);
	symlink_meta.generation = generation;
	memcpy(symlink_meta.link_path, link, sizeof(char) * strlen(link));

	/* Update self meta data */
	self_meta_cache_entry = meta_cache_lock_entry(self_inode);
	if (self_meta_cache_entry == NULL)
		return -ENOMEM;
	
	ret_code = meta_cache_update_symlink_data(self_inode, this_stat, 
		&symlink_meta, self_meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_close_file(self_meta_cache_entry);
		meta_cache_unlock_entry(self_meta_cache_entry);
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
	
	/* Add entry to parent dir. Do NOT need to lock parent meta cache entry
	   because it had been locked before calling this function. Just need to 
	   open meta file. */
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
	
	return 0;
}


