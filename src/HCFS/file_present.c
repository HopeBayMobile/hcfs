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

	fetch_meta_path(thismetapath, self_inode);

	if (access(thismetapath, F_OK) == 0)
		unlink(thismetapath);

/*TODO: Need to remove entry from super block if needed */

	return 0;
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
int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat)
{
	struct stat returned_stat;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *temp_entry;

	/*First will try to lookup meta cache*/
	if (this_inode > 0) {
		temp_entry = meta_cache_lock_entry(this_inode);
		/* Only fetch inode stat, so does not matter if inode is reg
		*  file or dir here*/
		ret_code = meta_cache_lookup_file_data(this_inode,
				&returned_stat, NULL, NULL, 0, temp_entry);
		ret_code = meta_cache_close_file(temp_entry);
		ret_code = meta_cache_unlock_entry(temp_entry);

		if (ret_code == 0) {
			memcpy(inode_stat, &returned_stat, sizeof(struct stat));
			return 0;
		}

		if (ret_code < 0)
			return -ENOENT;
	} else {
		return -ENOENT;
	}

/*TODO: What to do if cannot create new meta cache entry? */

	#if DEBUG >= 5
	printf("fetch_inode_stat %lld\n", inode_stat->st_ino);
	#endif	/* DEBUG */

	return 0;
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
int mknod_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
							struct stat *this_stat)
{
	int ret_val;
	FILE_META_TYPE this_meta;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	memset(&this_meta, 0, sizeof(FILE_META_TYPE));

	/* Store the inode and file meta of the new file to meta cache */
	body_ptr = meta_cache_lock_entry(self_inode);
	ret_val = meta_cache_update_file_data(self_inode, this_stat, &this_meta,
							NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return -EACCES;

	/* Add "self_inode" to its parent "parent_inode" */
	body_ptr = meta_cache_lock_entry(parent_inode);
	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
						this_stat->st_mode, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0)
		return -EACCES;

	return 0;
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
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
							struct stat *this_stat)
{
	char thismetapath[METAPATHLEN];
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	/* Initialize new directory object and save the meta to meta cache */
	this_meta.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	init_dir_page(&temppage, self_inode, parent_inode,
						this_meta.root_entry_page);

	body_ptr = meta_cache_lock_entry(self_inode);

	ret_val = meta_cache_update_dir_data(self_inode, this_stat, &this_meta,
							&temppage, body_ptr);

	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return -EACCES;

	/* Save the new entry to its parent and update meta */
	body_ptr = meta_cache_lock_entry(parent_inode);
	ret_val = dir_add_entry(parent_inode, self_inode, selfname,
						this_stat->st_mode, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return -EACCES;

	return 0;
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
int unlink_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname)
{
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(parent_inode);
	ret_val = dir_remove_entry(parent_inode, this_inode, selfname,
							S_IFREG, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0)
		return -EACCES;

	ret_val = decrease_nlink_inode_file(this_inode);

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
int rmdir_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname)
{
	DIR_META_TYPE tempmeta;
	char thismetapath[METAPATHLEN];
	char todelete_metapath[METAPATHLEN];
	int ret_val;
	FILE *todeletefptr, *metafptr;
	char filebuf[5000];
	size_t read_size;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(this_inode);
	ret_val = meta_cache_lookup_dir_data(this_inode, NULL, &tempmeta,
							NULL, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	printf("TOTAL CHILDREN is now %ld\n", tempmeta.total_children);

	if (tempmeta.total_children > 0)
		return -ENOTEMPTY;

	/* Remove this directory from its parent */
	body_ptr = meta_cache_lock_entry(parent_inode);
	ret_val = dir_remove_entry(parent_inode, this_inode, selfname, S_IFDIR,
								body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	if (ret_val < 0)
		return -EACCES;

	/*Need to delete the inode by moving it to "todelete" path*/
	super_block_to_delete(this_inode);
	fetch_todelete_path(todelete_metapath, this_inode);
	/*Try a rename first*/
	ret_val = rename(thismetapath, todelete_metapath);
	if (ret_val < 0) {
		/*If not successful, copy the meta*/
		unlink(todelete_metapath);
		todeletefptr = fopen(todelete_metapath, "w");
		fetch_meta_path(thismetapath, this_inode);
		metafptr = fopen(thismetapath, "r");
		setbuf(metafptr, NULL);
		flock(fileno(metafptr), LOCK_EX);
		setbuf(todeletefptr, NULL);
		fseek(metafptr, 0, SEEK_SET);

		while (!feof(metafptr)) {
			read_size = fread(filebuf, 1, 4096, metafptr);
			if (read_size > 0)
				fwrite(filebuf, 1, read_size, todeletefptr);
			else
				break;
		}
		fclose(todeletefptr);

		unlink(thismetapath);
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
		ret_val = meta_cache_remove(this_inode);
	}

	return ret_val;
}
