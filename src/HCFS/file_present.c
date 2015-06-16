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
#include "xattr_ops.h"

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
			/* TODO: Add case for symlink */
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
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(parent_inode);
	if (body_ptr == NULL)
		return -ENOMEM;

	ret_val = dir_remove_entry(parent_inode, this_inode, selfname,
							S_IFREG, body_ptr);
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


int fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *xattr_pos)
{
	int ret_code;
	ino_t this_inode;
	struct stat stat_data;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	int errcode;
	int ret;
	long long ret_pos;
	long long ret_size; 

	this_inode = meta_cache_entry->inode_num;
	if (this_inode <= 0)
		return -EINVAL;

	/* First lookup stat to confirm the file type. */
	ret_code = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (ret_code < 0)
		return ret_code;
	
	/* Get metadata by case */
	if (S_ISREG(stat_data.st_mode)) {
		ret_code = meta_cache_lookup_file_data(this_inode, NULL, &filemeta,
				NULL, 0, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = filemeta.next_xattr_page; /* Get xattr file position */
	}
	if (S_ISDIR(stat_data.st_mode)) {
		ret_code = meta_cache_lookup_dir_data(this_inode, NULL, &dirmeta,
				NULL, meta_cache_entry);
		if (ret_code < 0)
			return ret_code;
		*xattr_pos = dirmeta.next_xattr_page; /* Get xattr file position */
	}
	/* TODO: case symlink */
	
	/* Allocate a xattr page if it is first time to insert xattr */
	if (*xattr_pos == 0) { /* No xattr before. Allocate new XATTR_PAGE */
		memset(&xattr_page, 0, sizeof(XATTR_PAGE));
		FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
		FTELL(meta_cache_entry->fptr);
		*xattr_pos = ret_pos;
		FWRITE(&xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);

		/* Update xattr filepos in meta cache */
		if (S_ISREG(stat_data.st_mode)) {
			filemeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_file_data(this_inode, NULL, 
				&filemeta, NULL, 0, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
		}
		if (S_ISDIR(stat_data.st_mode)) {
			dirmeta.next_xattr_page = *xattr_pos;
			ret_code = meta_cache_update_dir_data(this_inode, NULL, 
				&dirmeta, NULL, meta_cache_entry);
			if (ret_code < 0)
				return ret_code;
		}
		/* TODO: case symlink */

	} else { /* xattr has been existed. Just read it. */
		FSEEK(meta_cache_entry->fptr, *xattr_pos, SEEK_SET);
		FREAD(&xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);	
	}

	return 0;

errcode_handle:
	return errcode;	
}
