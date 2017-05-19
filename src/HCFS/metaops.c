/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: metaops.c
* Abstract: The c source code file for meta processing involving regular
*           files and directories in HCFS. Functions are called mainly by
*           other functions in file_present.c.
*
* Revision History
* 2015/2/5 Jiahong added header for this file, and revising coding style.
* 2015/2/11 Jiahong moved "seek_page" and "advance_block" from filetables
*           and add hfuse_system.h inclusion.
* 2015/5/11 Jiahong modifying seek_page for new block indexing / searching.
*           Also remove advance_block function.
* 2015/5/11 Jiahong adding "create_page" function for creating new block page
* 2015/5/28 Jiahong adding error handling
* 2015/6/2 Jiahong moving lookup_dir to this file
* 2016/1/18 Jiahong revised actual_delete_inode routine
* 2016/1/19 Jiahong revised disk_markdelete
* 2016/4/26 Jiahong adding routines for snapshotting dir meta before modifying
* 2016/6/7 Jiahong changing code for recovering mode
**************************************************************************/
#include "metaops.h"

#include <sys/file.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <stdint.h>
#include <inttypes.h>

#include "global.h"
#include "utils.h"
#include "file_present.h"
#include "params.h"
#include "dir_entry_btree.h"
#include "hfuse_system.h"
#include "macro.h"
#include "logger.h"
#include "mount_manager.h"
#include "lookup_count.h"
#include "super_block.h"
#include "filetables.h"
#include "xattr_ops.h"
#include "hcfs_fromcloud.h"
#include "utils.h"
#ifdef _ANDROID_ENV_
#include "path_reconstruct.h"
#include "FS_manager.h"
#endif
#include "rebuild_super_block.h"
#include "do_restoration.h"

static inline void logerr(int32_t errcode, char *msg)
{
	if (errcode > 0)
		write_log(0, "%s. Code %d, %s\n", msg, errcode,
				strerror(errcode));
	else
		write_log(0, "%s.\n", msg);
}

/************************************************************************
*
* Function name: init_dir_page
*        Inputs: DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
*                int64_t this_page_pos
*       Summary: Initialize directory entries for a new directory object.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode,
				ino_t parent_inode, int64_t this_page_pos)
{
	memset(tpage, 0, sizeof(DIR_ENTRY_PAGE));

	tpage->num_entries = 2;
	(tpage->dir_entries[0]).d_ino = self_inode;
	strcpy((tpage->dir_entries[0]).d_name, ".");
	(tpage->dir_entries[0]).d_type = D_ISDIR;

	(tpage->dir_entries[1]).d_ino = parent_inode;
	strcpy((tpage->dir_entries[1]).d_name, "..");
	(tpage->dir_entries[1]).d_type = D_ISDIR;
	tpage->this_page_pos = this_page_pos;
	return 0;
}

/************************************************************************
*
* Function name: dir_add_entry
*        Inputs: ino_t parent_inode, ino_t child_inode, const char *childname,
*                mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Add a directory entry with name "childname", inode number
*                "child_inode", and mode "child_mode" to the directory
*                with inode number "parent_inode". Meta cache entry of
*                parent is pointed by "body_ptr".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t dir_add_entry(ino_t parent_inode,
		      ino_t child_inode,
		      const char *childname,
		      mode_t child_mode,
		      META_CACHE_ENTRY_STRUCT *body_ptr,
		      BOOL is_external)
{
	HCFS_STAT parent_stat;
	DIR_META_TYPE parent_meta;
	DIR_ENTRY_PAGE tpage, new_root, tpage2;
	DIR_ENTRY temp_entry, overflow_entry;
	int64_t overflow_new_page;
	int32_t ret, errcode;
	size_t ret_size;
	int32_t sem_val;
	char no_need_rewrite;
	int64_t ret_pos;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE+2)];
	int64_t temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];

	sem_getvalue(&(body_ptr->access_sem), &sem_val);
	if (sem_val > 0) {
		/*Not locked, return -EPERM*/
		return -EPERM;
	}

	memset(&temp_entry, 0, sizeof(DIR_ENTRY));
	memset(&overflow_entry, 0, sizeof(DIR_ENTRY));

	temp_entry.d_ino = child_inode;
	snprintf(temp_entry.d_name, MAX_FILENAME_LEN+1, "%s", childname);
	if (S_ISREG(child_mode))
		temp_entry.d_type = D_ISREG;
	else if (S_ISDIR(child_mode))
		temp_entry.d_type = D_ISDIR;
	else if (S_ISLNK(child_mode))
		temp_entry.d_type = D_ISLNK;
	else if (S_ISFIFO(child_mode))
		temp_entry.d_type = D_ISFIFO;
	else if (S_ISSOCK(child_mode))
		temp_entry.d_type = D_ISSOCK;

	/* Load parent meta from meta cache */
	ret = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
					&parent_meta, NULL, body_ptr);

	if (ret < 0)
		return ret;

	/*
	 * Initialize B-tree insertion by first loading the root of the
	 * B-tree.
	 */
	tpage.this_page_pos = parent_meta.root_entry_page;

	ret = meta_cache_open_file(body_ptr);

	if (ret < 0)
		return ret;

	ret = handle_dirmeta_snapshot(parent_inode, body_ptr->fptr);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	FSEEK(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);

	FREAD(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Drop all cached pages first before inserting */
	/*
	 * TODO: Future changes could remove this limitation if
	 * can update cache with each node change in b-tree
	 */
	ret = meta_cache_drop_pages(body_ptr);
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		return ret;
	}

	/* Recursive routine for B-tree insertion*/
	/* Temp space for traversing the tree is allocated before calling */
	ret = insert_dir_entry_btree(&temp_entry, &tpage,
			fileno(body_ptr->fptr), &overflow_entry,
			&overflow_new_page, &parent_meta, temp_dir_entries,
			temp_child_page_pos, is_external);

	/* An error occured and the routine will terminate now */
	/* TODO: Consider error recovering here */
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		return ret;
	}

	/*
	 * If return value is 1, we need to handle overflow by splitting
	 * the old root node in two and create a new root page to point
	 * to the two splitted nodes. Note that a new node has already been
	 * created in this case and pointed by "overflow_new_page".
	 */
	if (ret == 1) {
		/* Reload old root */
		FSEEK(body_ptr->fptr, parent_meta.root_entry_page,
				SEEK_SET);

		FREAD(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

		/* tpage contains the old root node now */

		/*Need to create a new root node and write to disk*/
		if (parent_meta.entry_page_gc_list != 0) {
			/*Reclaim node from gc list first*/
			FSEEK(body_ptr->fptr,
				parent_meta.entry_page_gc_list, SEEK_SET);

			FREAD(&new_root, sizeof(DIR_ENTRY_PAGE), 1,
						body_ptr->fptr);

			new_root.this_page_pos = parent_meta.entry_page_gc_list;
			parent_meta.entry_page_gc_list = new_root.gc_list_next;
		} else {
			/* If cannot reclaim, extend the meta file */
			memset(&new_root, 0, sizeof(DIR_ENTRY_PAGE));
			FSEEK(body_ptr->fptr, 0, SEEK_END);

			FTELL(body_ptr->fptr);
			new_root.this_page_pos = ret_pos;
			if (new_root.this_page_pos == -1) {
				errcode = errno;
				logerr(errcode,
					"IO error in adding dir entry");
				meta_cache_close_file(body_ptr);
				return -errcode;
			}
		}

		/*
		 * Insert the new root to the head of tree_walk_list. This list
		 * is for listing nodes in the B-tree in readdir operation.
		 */
		new_root.gc_list_next = 0;
		new_root.tree_walk_next = parent_meta.tree_walk_list_head;
		new_root.tree_walk_prev = 0;

		no_need_rewrite = FALSE;
		if (parent_meta.tree_walk_list_head == tpage.this_page_pos) {
			tpage.tree_walk_prev = new_root.this_page_pos;
		} else {
			FSEEK(body_ptr->fptr,
				parent_meta.tree_walk_list_head, SEEK_SET);

			FREAD(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
							body_ptr->fptr);

			tpage2.tree_walk_prev = new_root.this_page_pos;
			if (tpage2.this_page_pos == overflow_new_page) {
				tpage2.parent_page_pos = new_root.this_page_pos;
				no_need_rewrite = TRUE;
			}
			FSEEK(body_ptr->fptr,
				parent_meta.tree_walk_list_head, SEEK_SET);

			FWRITE(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
							body_ptr->fptr);
		}


		parent_meta.tree_walk_list_head = new_root.this_page_pos;

		/* Initialize the new root node */
		new_root.parent_page_pos = 0;
		memset(new_root.child_page_pos, 0,
				sizeof(int64_t)*(MAX_DIR_ENTRIES_PER_PAGE+1));
		new_root.num_entries = 1;
		memcpy(&(new_root.dir_entries[0]), &overflow_entry,
							sizeof(DIR_ENTRY));
		/*
		 * The two children of the new root is the old root and
		 * the new node created by the overflow.
		 */
		new_root.child_page_pos[0] = parent_meta.root_entry_page;
		new_root.child_page_pos[1] = overflow_new_page;

		/*
		 * Set the root of B-tree to the new root, and write the
		 * content of the new root the the meta file.
		 */
		parent_meta.root_entry_page = new_root.this_page_pos;
		FSEEK(body_ptr->fptr, new_root.this_page_pos, SEEK_SET);

		FWRITE(&new_root, sizeof(DIR_ENTRY_PAGE), 1,
				body_ptr->fptr);

		/*
		 * Change the parent of the old root to point to the new
		 * root.  Write to the meta file afterward.
		 */
		tpage.parent_page_pos = new_root.this_page_pos;
		FSEEK(body_ptr->fptr, tpage.this_page_pos, SEEK_SET);
		FWRITE(&tpage, sizeof(DIR_ENTRY_PAGE), 1,
				body_ptr->fptr);

		/*
		 * If no_need_rewrite is true, we have already write
		 * modified content for the new node from the overflow.
		 * Otherwise we need to write it to the meta file here.
		 */
		if (no_need_rewrite == FALSE) {
			FSEEK(body_ptr->fptr, overflow_new_page,
					SEEK_SET);
			FREAD(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
							body_ptr->fptr);
			if (errcode < 0) {
				meta_cache_close_file(body_ptr);
				return errcode;
			}

			tpage2.parent_page_pos = new_root.this_page_pos;
			FSEEK(body_ptr->fptr, overflow_new_page,
					SEEK_SET);
			FWRITE(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
							body_ptr->fptr);
		}

		/*
		 * Complete the splitting by updating the meta of the
		 * directory.
		 */
		FSEEK(body_ptr->fptr, sizeof(HCFS_STAT), SEEK_SET);
		FWRITE(&parent_meta, sizeof(DIR_META_TYPE), 1,
					body_ptr->fptr);
	}

	/*If the new entry is a subdir, increase the hard link of the parent*/

	if (S_ISDIR(child_mode))
		parent_stat.nlink++;

	parent_meta.total_children++;
	write_log(10,
		"TOTAL CHILDREN is now %lld\n", parent_meta.total_children);

	set_timestamp_now(&parent_stat, M_TIME | C_TIME);
	/*
	 * Stat may be dirty after the operation so should write them
	 * back to cache
	 */
	ret = meta_cache_update_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);

	return ret;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: dir_remove_entry
*        Inputs: ino_t parent_inode, ino_t child_inode, const char *childname,
*                mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Remove a directory entry with name "childname", inode number
*                "child_inode", and mode "child_mode" from the directory
*                with inode number "parent_inode". Meta cache entry of
*                parent is pointed by "body_ptr".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t dir_remove_entry(ino_t parent_inode,
			 ino_t child_inode,
			 const char *childname,
			 mode_t child_mode,
			 META_CACHE_ENTRY_STRUCT *body_ptr,
			 BOOL is_external)
{
	HCFS_STAT parent_stat;
	DIR_META_TYPE parent_meta;
	DIR_ENTRY_PAGE tpage;
	int32_t sem_val;
	DIR_ENTRY temp_entry;
	int32_t ret, errcode;
	size_t ret_size;

	DIR_ENTRY temp_dir_entries[2*(MAX_DIR_ENTRIES_PER_PAGE+2)];
	int64_t temp_child_page_pos[2*(MAX_DIR_ENTRIES_PER_PAGE+3)];

	sem_getvalue(&(body_ptr->access_sem), &sem_val);
	if (sem_val > 0) {
		/*Not locked, return -1*/
		return -1;
	}

	memset(&temp_entry, 0, sizeof(DIR_ENTRY));

	temp_entry.d_ino = child_inode;
	strcpy(temp_entry.d_name, childname);
	if (S_ISREG(child_mode))
		temp_entry.d_type = D_ISREG;
	else if (S_ISDIR(child_mode))
		temp_entry.d_type = D_ISDIR;
	else if (S_ISLNK(child_mode))
		temp_entry.d_type = D_ISLNK;
	else if (S_ISFIFO(child_mode))
		temp_entry.d_type = D_ISFIFO;
	else if (S_ISSOCK(child_mode))
		temp_entry.d_type = D_ISSOCK;

	/* Initialize B-tree deletion by first loading the root of B-tree */
	ret = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);
	if (ret < 0)
		return ret;

	tpage.this_page_pos = parent_meta.root_entry_page;

	ret = meta_cache_open_file(body_ptr);
	if (ret < 0)
		return ret;

	ret = handle_dirmeta_snapshot(parent_inode, body_ptr->fptr);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Drop all cached pages first before deleting */
	/*
	 * TODO: Future changes could remove this limitation if can
	 * update cache with each node change in b-tree
	 */

	ret = meta_cache_drop_pages(body_ptr);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Read root node */
	FSEEK(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);
	FREAD(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Recursive B-tree deletion routine*/
	ret = delete_dir_entry_btree(&temp_entry, &tpage,
			fileno(body_ptr->fptr), &parent_meta, temp_dir_entries,
			temp_child_page_pos, is_external);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	write_log(10, "delete dir entry returns %d\n", ret);
	/* tpage might be invalid after calling delete_dir_entry_btree */

	if (ret == 0) {
		/*
		 * If the entry is a subdir, decrease the hard link of
		 * the parent
		 */

		if (S_ISDIR(child_mode))
			parent_stat.nlink--;

		parent_meta.total_children--;
		write_log(10, "TOTAL CHILDREN is now %lld\n",
						parent_meta.total_children);
		set_timestamp_now(&parent_stat, M_TIME | C_TIME);

		ret = meta_cache_update_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	return ret;

errcode_handle:
	meta_cache_close_file(body_ptr);
	return errcode;
}

/************************************************************************
*
* Function name: change_parent_inode
*        Inputs: ino_t self_inode, ino_t parent_inode1,
*                ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: For a directory "self_inode", change its parent from
*                "parent_inode1" to "parent_inode2" in its own ".." entry.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external)
{
	DIR_ENTRY_PAGE tpage;
	int32_t count;
	int32_t ret_val;
	HCFS_STAT tmpstat;

	/* TODO: remove unused parameter ‘parent_inode1’ */
	UNUSED(parent_inode1);
	ret_val = meta_cache_seek_dir_entry(self_inode, &tpage, &count,
						"..", body_ptr, is_external);

	if ((ret_val == 0) && (count >= 0)) {
		/*Found the entry. Change parent inode*/
		ret_val = meta_cache_lookup_dir_data(self_inode, &tmpstat,
					NULL, NULL, body_ptr);
		if (ret_val < 0)
			return ret_val;

		tpage.dir_entries[count].d_ino = parent_inode2;
		set_timestamp_now(&tmpstat, M_TIME | C_TIME);
		ret_val = meta_cache_update_dir_data(self_inode, &tmpstat,
					NULL, &tpage, body_ptr);
		return ret_val;
	}

	if ((ret_val == 0) && (count < 0))  /* Not found */
		ret_val = -ENOENT;

	return ret_val;
}

/*
 * change_entry_name should only be called from a rename situation where
 * the volume is "external" and if the old and the new name are the same
 * if case insensitive
 */
int32_t change_entry_name(ino_t parent_inode,
			  const char *targetname,
			  META_CACHE_ENTRY_STRUCT *body_ptr)
{
	DIR_ENTRY_PAGE tpage;
	int32_t count;
	int32_t ret_val;
	HCFS_STAT tmpstat;

	ret_val = meta_cache_seek_dir_entry(parent_inode, &tpage, &count,
					targetname, body_ptr, TRUE);

	if ((ret_val == 0) && (count >= 0)) {
		/*Found the entry. Change parent inode*/
		ret_val = meta_cache_lookup_dir_data(parent_inode, &tmpstat,
					NULL, NULL, body_ptr);
		if (ret_val < 0)
			return ret_val;

		snprintf(tpage.dir_entries[count].d_name, MAX_FILENAME_LEN + 1,
			 "%s", targetname);
		set_timestamp_now(&tmpstat, M_TIME | C_TIME);
		ret_val = meta_cache_update_dir_data(parent_inode, &tmpstat,
					NULL, &tpage, body_ptr);
		return ret_val;
	}

	if ((ret_val == 0) && (count < 0))  /* Not found */
		ret_val = -ENOENT;

	return ret_val;
}

/************************************************************************
*
* Function name: change_dir_entry_inode
*        Inputs: ino_t self_inode, const char *targetname,
*                ino_t new_inode,
*                META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: For a directory "self_inode", change the inode and mode
*                of entry "targetname" to "new_inode" and "new_mode".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t change_dir_entry_inode(ino_t self_inode,
			       const char *targetname,
			       ino_t new_inode,
			       mode_t new_mode,
			       META_CACHE_ENTRY_STRUCT *body_ptr,
			       BOOL is_external)
{
	DIR_ENTRY_PAGE tpage;
	int32_t count;
	int32_t ret_val;
	HCFS_STAT tmpstat;

	ret_val = meta_cache_seek_dir_entry(self_inode, &tpage, &count,
				targetname, body_ptr, is_external);

	if ((ret_val == 0) && (count >= 0)) {
		/*Found the entry. Change inode*/
		ret_val = meta_cache_lookup_dir_data(self_inode, &tmpstat,
					NULL, NULL, body_ptr);
		if (ret_val < 0)
			return ret_val;
		tpage.dir_entries[count].d_ino = new_inode;
		if (S_ISREG(new_mode)) {
			write_log(10, "Debug: change to type REG\n");
			tpage.dir_entries[count].d_type = D_ISREG;

		} else if (S_ISLNK(new_mode)) {
			write_log(10, "Debug: change to type LNK\n");
			tpage.dir_entries[count].d_type = D_ISLNK;

		} else if (S_ISDIR(new_mode)) {
			write_log(10, "Debug: change to type DIR\n");
			tpage.dir_entries[count].d_type = D_ISDIR;

		} else if (S_ISFIFO(new_mode)) {
			write_log(10, "Debug: change to type FIFO\n");
			tpage.dir_entries[count].d_type = D_ISFIFO;

		} else if (S_ISSOCK(new_mode)) {
			write_log(10, "Debug: change to type SOCK\n");
			tpage.dir_entries[count].d_type = D_ISSOCK;
		} else {
			write_log(0, "Error: Invalid rename type in %s\n",
					__func__);
			return -EINVAL;
		}

		set_timestamp_now(&tmpstat, M_TIME | C_TIME);
		ret_val = meta_cache_update_dir_data(self_inode, &tmpstat,
					NULL, &tpage, body_ptr);
		return ret_val;
	}
	if ((ret_val == 0) && (count < 0))  /* Not found */
		ret_val = -ENOENT;

	return ret_val;
}

/**
 * Remove meta file and reclaim it. If remove meta by involking this
 * function, the meta file will immediately unlink rather than
 * be moved to "todelete" folder.
 *
 * @param this_inode Inode number of the meta file to be removed.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t directly_delete_inode_meta(ino_t this_inode)
{
	char thismetapath[METAPATHLEN];
	int32_t ret, errcode;

	ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;
	ret = unlink(thismetapath);
	if (ret < 0) {
		errcode = errno;
		/* If no entry, do not need to remove it. */
		if (errcode != ENOENT) {
			write_log(0, "Error: Fail to remove meta %"
					PRIu64". Code %d\n",
					(uint64_t)this_inode, errcode);
			return -errcode;
		}
	}
	/* When backend is not set, do not enqueue to delete queue */
	ret = super_block_to_delete(this_inode, FALSE);
	if (ret < 0)
		return ret;
	ret = super_block_delete(this_inode);
	if (ret < 0)
		return ret;
	ret = super_block_reclaim();
	return ret;
}

/************************************************************************
*
* Function name: delete_inode_meta
*        Inputs: ino_t this_inode
*       Summary: For inode "this_inode", delete the entry from super block
*                and move the meta file to "todelete" folder.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t delete_inode_meta(ino_t this_inode)
{
	char todelete_metapath[METAPATHLEN];
	char thismetapath[METAPATHLEN];
	FILE *todeletefptr, *metafptr;
	char filebuf[5000];
	int32_t ret, errcode;
	size_t ret_size, write_size;

	ret = fetch_todelete_path(todelete_metapath, this_inode);
	if (ret < 0)
		return ret;

	/*
	 * Meta file should be downloaded already in unlink, rmdir, or
	 * rename ops
	 */

	ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;

	/*Try a rename first*/
	ret = rename(thismetapath, todelete_metapath);
	write_log(10, "%s, %s, %d\n", thismetapath, todelete_metapath, ret);
	if (ret < 0) {
		/*If not successful, copy the meta*/
		todeletefptr = NULL;
		metafptr = NULL;
		if (access(todelete_metapath, F_OK) == 0)
			UNLINK(todelete_metapath);
		todeletefptr = fopen(todelete_metapath, "w");
		if (todeletefptr == NULL) {
			errcode = errno;
			write_log(0,
				"Unable to open file in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			return -errcode;
		}
		metafptr = fopen(thismetapath, "r");
		if (metafptr == NULL) {
			errcode = errno;
			write_log(0,
				"Unable to open file in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		setbuf(metafptr, NULL);
		flock(fileno(metafptr), LOCK_EX);
		setbuf(todeletefptr, NULL);
		FSEEK(metafptr, 0, SEEK_SET);
		while (!feof(metafptr)) {
			FREAD(filebuf, 1, 4096, metafptr);
			if (ret > 0) {
				write_size = ret_size;
				FWRITE(filebuf, 1, write_size, todeletefptr);
			} else {
				break;
			}
		}
		fclose(todeletefptr);

		unlink(thismetapath);
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);
	}
	return ret;

errcode_handle:
	if (todeletefptr != NULL)
		fclose(todeletefptr);
	if (metafptr != NULL)
		fclose(metafptr);
	return errcode;
}

/************************************************************************
*
* Function name: decrease_nlink_inode_file
*        Inputs: fuse_req_t req, ino_t this_inode
*       Summary: For a regular file pointed by "this_inode", decrease its
*                reference count. If the count drops to zero, delete the
*                file as well.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t decrease_nlink_inode_file(fuse_req_t req, ino_t this_inode)
{
	HCFS_STAT this_inode_stat;
	int32_t ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(this_inode);
	/*
	 * Only fetch inode stat here. Can be replaced by
	 * meta_cache_lookup_file_data()
	 */
	if (body_ptr == NULL)
		return -errno;

	ret_val = meta_cache_lookup_dir_data(this_inode, &this_inode_stat,
							NULL, NULL, body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	if (this_inode_stat.nlink <= 1) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);

		ret_val = mark_inode_delete(req, this_inode);

	} else {
		/* If it is still referenced, update the meta file. */
		this_inode_stat.nlink--;
		set_timestamp_now(&this_inode_stat, C_TIME);
		ret_val = meta_cache_update_dir_data(this_inode,
					&this_inode_stat, NULL, NULL, body_ptr);
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
	}

	return ret_val;
}

static inline int64_t longpow(int64_t base, int32_t power)
{
	int64_t tmp;
	int32_t count;

	tmp = 1;

	for (count = 0; count < power; count++)
		tmp = tmp * base;

	return tmp;
}
/* Checks if page_index belongs to direct or what indirect page */
int32_t check_page_level(int64_t page_index)
{
	int64_t tmp_index;

	if (page_index == 0)
		return 0;   /*direct page (id 0)*/

	tmp_index = page_index - 1;

	if (tmp_index < POINTERS_PER_PAGE) /* if single-indirect */
		return 1;

	tmp_index = tmp_index - POINTERS_PER_PAGE;

	/* double-indirect */
	if (tmp_index < (longpow(POINTERS_PER_PAGE, 2)))
		return 2;

	tmp_index = tmp_index - (longpow(POINTERS_PER_PAGE, 2));

	/* triple-indirect */
	if (tmp_index < (longpow(POINTERS_PER_PAGE, 3)))
		return 3;

	tmp_index = tmp_index - (longpow(POINTERS_PER_PAGE, 3));

	if (tmp_index < (longpow(POINTERS_PER_PAGE, 4)))
		return 4;

	return -EINVAL;
}

int64_t _load_indirect(int64_t target_page, FILE_META_TYPE *temp_meta,
			FILE *fptr, int32_t level)
{
	int64_t tmp_page_index;
	int64_t tmp_pos, tmp_target_pos;
	int64_t tmp_ptr_page_index, tmp_ptr_index;
	PTR_ENTRY_PAGE tmp_ptr_page;
	int32_t count, ret, errcode;
	size_t ret_size;
	int64_t ret_pos;

	tmp_page_index = target_page - 1;

	for (count = 1; count < level; count++)
		tmp_page_index -= (longpow(POINTERS_PER_PAGE, count));

	switch (level) {
	case 1:
		tmp_target_pos = temp_meta->single_indirect;
		break;
	case 2:
		tmp_target_pos = temp_meta->double_indirect;
		break;
	case 3:
		tmp_target_pos = temp_meta->triple_indirect;
		break;
	case 4:
		tmp_target_pos = temp_meta->quadruple_indirect;
		break;
	default:
		return 0;
	}

	tmp_ptr_index = tmp_page_index;

	for (count = level - 1; count >= 0; count--) {
		FSEEK(fptr, tmp_target_pos, SEEK_SET);
		FTELL(fptr);
		tmp_pos = ret_pos;
		if (tmp_pos != tmp_target_pos)
			return 0;
		FREAD(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1, fptr);

		if (count == 0)
			break;

		tmp_ptr_page_index = tmp_ptr_index /
				(longpow(POINTERS_PER_PAGE, count));
		tmp_ptr_index = tmp_ptr_index %
				(longpow(POINTERS_PER_PAGE, count));
		if (tmp_ptr_page.ptr[tmp_ptr_page_index] == 0)
			return 0;

		tmp_target_pos = tmp_ptr_page.ptr[tmp_ptr_page_index];
	}


	return tmp_ptr_page.ptr[tmp_ptr_index];

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: seek_page
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page
*                int64_t hint_page
*       Summary: Given meta cache entry pointed by "body_ptr", find the block
*                entry page "target_page" and return the file pos of the page.
*                "hint_page" is used for quickly finding the position of
*                the new page. This should be the page index before the
*                function call, or 0 if this is the first relevant call.
*  Return value: File pos of the page if successful. Otherwise returns
*                negation of error code.
*                If file pos is 0, the page is not found.
*
*************************************************************************/
int64_t seek_page(META_CACHE_ENTRY_STRUCT *body_ptr,
		  int64_t target_page,
		  int64_t hint_page)
{
	off_t filepos = 0;
	int32_t sem_val;
	FILE_META_TYPE temp_meta;
	int32_t which_indirect;
	int32_t ret;

	/* TODO: hint_page is not used now. Consider how to enhance. */
	UNUSED(hint_page);
	/* First check if meta cache is locked */
	/* Do not actually create page here */

	if (target_page < 0)
		return -EPERM;

	sem_getvalue(&(body_ptr->access_sem), &sem_val);

	/*If meta cache lock is not locked, return -1*/
	if (sem_val > 0)
		return -EPERM;

	ret = meta_cache_lookup_file_data(body_ptr->inode_num, NULL,
				&temp_meta, NULL, 0, body_ptr);
	if (ret < 0)
		return ret;

	which_indirect = check_page_level(target_page);

	switch (which_indirect) {
	case 0:
		filepos = temp_meta.direct;
		break;
	case 1:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		if (temp_meta.single_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, &temp_meta,
						body_ptr->fptr, 1);
		break;
	case 2:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		if (temp_meta.double_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, &temp_meta,
						body_ptr->fptr, 2);
		break;
	case 3:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		if (temp_meta.triple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, &temp_meta,
						body_ptr->fptr, 3);
		break;
	case 4:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		if (temp_meta.quadruple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, &temp_meta,
						body_ptr->fptr, 4);
		break;
	default:
		filepos = -EINVAL;
		break;
	}

	return filepos;
}

/* Helper function for creating new page. */
int64_t _create_indirect(int64_t target_page, FILE_META_TYPE *temp_meta,
			META_CACHE_ENTRY_STRUCT *body_ptr, int32_t level)
{
	int64_t tmp_page_index;
	int64_t tmp_pos, tmp_target_pos;
	int64_t tmp_ptr_page_index, tmp_ptr_index;
	PTR_ENTRY_PAGE tmp_ptr_page, empty_ptr_page;
	int32_t count, ret, errcode;
	BLOCK_ENTRY_PAGE temppage;
	size_t ret_size;
	int64_t ret_pos;

	tmp_page_index = target_page - 1;

	for (count = 1; count < level; count++)
		tmp_page_index -= (longpow(POINTERS_PER_PAGE, count));

	switch (level) {
	case 1:
		tmp_target_pos = temp_meta->single_indirect;
		if (tmp_target_pos == 0) {
			if (NO_META_SPACE())
				return -ENOSPC;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			temp_meta->single_indirect = ret_pos;
			tmp_target_pos = temp_meta->single_indirect;
			memset(&tmp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
						body_ptr->fptr);
			ret = meta_cache_update_file_data(body_ptr->inode_num,
					NULL, temp_meta, NULL, 0, body_ptr);
			if (ret < 0)
				return ret;
		}
		break;
	case 2:
		tmp_target_pos = temp_meta->double_indirect;
		if (tmp_target_pos == 0) {
			if (NO_META_SPACE())
				return -ENOSPC;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			temp_meta->double_indirect = ret_pos;
			tmp_target_pos = temp_meta->double_indirect;
			memset(&tmp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
						body_ptr->fptr);
			ret = meta_cache_update_file_data(body_ptr->inode_num,
					NULL, temp_meta, NULL, 0, body_ptr);
			if (ret < 0)
				return ret;
		}
		break;
	case 3:
		tmp_target_pos = temp_meta->triple_indirect;
		if (tmp_target_pos == 0) {
			if (NO_META_SPACE())
				return -ENOSPC;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			temp_meta->triple_indirect = ret_pos;
			tmp_target_pos = temp_meta->triple_indirect;
			memset(&tmp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
						body_ptr->fptr);
			ret = meta_cache_update_file_data(body_ptr->inode_num,
					NULL, temp_meta, NULL, 0, body_ptr);
			if (ret < 0)
				return ret;
		}
		break;
	case 4:
		tmp_target_pos = temp_meta->quadruple_indirect;
		if (tmp_target_pos == 0) {
			if (NO_META_SPACE())
				return -ENOSPC;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			temp_meta->quadruple_indirect = ret_pos;
			tmp_target_pos = temp_meta->quadruple_indirect;
			memset(&tmp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
				body_ptr->fptr);
			ret = meta_cache_update_file_data(body_ptr->inode_num,
					NULL, temp_meta, NULL, 0, body_ptr);
			if (ret < 0)
				return ret;
		}
		break;
	default:
		return 0;
	}

	tmp_ptr_index = tmp_page_index;

	for (count = level - 1; count >= 0; count--) {
		FSEEK(body_ptr->fptr, tmp_target_pos, SEEK_SET);
		FTELL(body_ptr->fptr);
		tmp_pos = ret_pos;
		if (tmp_pos != tmp_target_pos)
			return 0;
		FREAD(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);

		if (count == 0)
			break;

		tmp_ptr_page_index = tmp_ptr_index /
				(longpow(POINTERS_PER_PAGE, count));
		tmp_ptr_index = tmp_ptr_index %
				(longpow(POINTERS_PER_PAGE, count));
		if (tmp_ptr_page.ptr[tmp_ptr_page_index] == 0) {
			if (NO_META_SPACE())
				return -ENOSPC;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			tmp_ptr_page.ptr[tmp_ptr_page_index] = ret_pos;
			memset(&empty_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			FWRITE(&empty_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
				body_ptr->fptr);
			FSEEK(body_ptr->fptr, tmp_target_pos, SEEK_SET);
			FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
						body_ptr->fptr);
		}
		tmp_target_pos = tmp_ptr_page.ptr[tmp_ptr_page_index];
	}

	if (tmp_ptr_page.ptr[tmp_ptr_index] == 0) {
		if (NO_META_SPACE())
			return -ENOSPC;
		FSEEK(body_ptr->fptr, 0, SEEK_END);
		FTELL(body_ptr->fptr);
		tmp_ptr_page.ptr[tmp_ptr_index] = ret_pos;

		memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
		ret = meta_cache_update_file_data(body_ptr->inode_num, NULL,
				NULL, &temppage,
				tmp_ptr_page.ptr[tmp_ptr_index], body_ptr);
		if (ret < 0)
			return ret;
		FSEEK(body_ptr->fptr, tmp_target_pos, SEEK_SET);
		FWRITE(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1,
						body_ptr->fptr);

	}

	return tmp_ptr_page.ptr[tmp_ptr_index];

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: create_page
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page
*       Summary: Given meta cache entry pointed by "body_ptr", create the block
*                entry page "target_page" and return the file pos of the page.
*  Return value: File pos of the page if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
int64_t create_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page)
{
	off_t filepos = 0;
	BLOCK_ENTRY_PAGE temppage;
	int32_t sem_val;
	FILE_META_TYPE temp_meta;
	int32_t which_indirect;
	int32_t ret, errcode;
	int64_t ret_pos;

	/* First check if meta cache is locked */
	/* Create page here */

	sem_getvalue(&(body_ptr->access_sem), &sem_val);
	/*If meta cache lock is not locked, return -1*/
	if (sem_val > 0)
		return -EPERM;

	if (target_page < 0)
		return -EPERM;

	ret = meta_cache_lookup_file_data(body_ptr->inode_num, NULL,
				&temp_meta, NULL, 0, body_ptr);
	if (ret < 0)
		return ret;

	which_indirect = check_page_level(target_page);
	switch (which_indirect) {
	case 0:
		filepos = temp_meta.direct;
		if (filepos == 0) {
			ret = meta_cache_open_file(body_ptr);
			if (ret < 0)
				return ret;
			FSEEK(body_ptr->fptr, 0, SEEK_END);
			FTELL(body_ptr->fptr);
			temp_meta.direct = ret_pos;
			filepos = temp_meta.direct;
			memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
			ret = meta_cache_update_file_data(body_ptr->inode_num,
					NULL, &temp_meta, &temppage,
					filepos, body_ptr);
			if (ret < 0)
				return ret;
		}
		break;
	case 1:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		filepos = _create_indirect(target_page, &temp_meta,
						body_ptr, 1);
		break;
	case 2:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		filepos = _create_indirect(target_page, &temp_meta,
						body_ptr, 2);
		break;
	case 3:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		filepos = _create_indirect(target_page, &temp_meta,
						body_ptr, 3);
		break;
	case 4:
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0)
			return ret;
		filepos = _create_indirect(target_page, &temp_meta,
						body_ptr, 4);
		break;
	default:
		filepos = -EINVAL;
		break;
	}

	return filepos;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: seek_page2
*        Inputs: FILE_META_TYPE *temp_meta, FILE *fptr, int64_t target_page
*                int64_t hint_page
*       Summary: Given meta file pointed by "fptr", find the block
*                entry page "target_page" and return the file pos of the page.
*                "hint_page" is used for quickly finding the position of
*                the new page. This should be the page index before the
*                function call, or 0 if this is the first relevant call.
*                "temp_meta" is the file meta header from "fptr".
*  Return value: File pos of the page if successful. Otherwise returns
*                negation of error code.
*                If file pos is 0, the page is not found.
*
*************************************************************************/
int64_t seek_page2(FILE_META_TYPE *temp_meta,
		   FILE *fptr,
		   int64_t target_page,
		   int64_t hint_page)
{
	off_t filepos = 0;
	int32_t which_indirect;

	/* TODO: hint_page is not used now. Consider how to enhance. */
	UNUSED(hint_page);
	/* First check if meta cache is locked */
	/* Do not actually create page here */
	/*TODO: put error handling for the read/write ops here*/

	if (target_page < 0)
		return -EPERM;

	which_indirect = check_page_level(target_page);

	switch (which_indirect) {
	case 0:
		filepos = temp_meta->direct;
		break;
	case 1:
		if (temp_meta->single_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 1);
		break;
	case 2:
		if (temp_meta->double_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 2);
		break;
	case 3:
		if (temp_meta->triple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 3);
		break;
	case 4:
		if (temp_meta->quadruple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 4);
		break;
	default:
		filepos = -EINVAL;
		break;
	}

	return filepos;
}

/* Helper function to check if this inode had been synced to cloud. */
int32_t check_meta_on_cloud(ino_t this_inode,
			char d_type, BOOL *meta_on_cloud,
			int64_t *metasize, int64_t *metalocalsize)
{
	char thismetapath[400];
	int32_t ret, errcode;
	FILE *metafptr = NULL;
	CLOUD_RELATED_DATA this_clouddata;
	off_t offset;
	ssize_t ret_ssize;
	struct stat tmpstat;

	if (d_type == D_ISREG)
		ret = fetch_todelete_path(thismetapath, this_inode);
	else
		ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;

	ret = stat(thismetapath, &tmpstat);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		return -errcode;
	}
	*metasize = tmpstat.st_size;
	*metalocalsize = tmpstat.st_blocks * 512;

	if (CURRENT_BACKEND == NONE) {
		*meta_on_cloud = FALSE;
		return 0;
	}

	metafptr = fopen(thismetapath, "r+");
	if (metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		return -errcode;
	}

	flock(fileno(metafptr), LOCK_EX);
	switch (d_type) {
	case D_ISDIR:
		offset = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
		PREAD(fileno(metafptr), &this_clouddata,
				sizeof(CLOUD_RELATED_DATA), offset);
		break;
	case D_ISLNK:
		offset = sizeof(HCFS_STAT) + sizeof(SYMLINK_META_TYPE);
		PREAD(fileno(metafptr), &this_clouddata,
				sizeof(CLOUD_RELATED_DATA), offset);
		break;
	case D_ISREG:
		offset = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE)
				+ sizeof(FILE_STATS_TYPE);
		PREAD(fileno(metafptr), &this_clouddata,
				sizeof(CLOUD_RELATED_DATA), offset);
		break;
	default:
		fclose(metafptr);
		write_log(0, "Error in %s. Unknown type of inode "PRIu64"\n",
				__func__, (uint64_t)this_inode);
		return -EPERM;
	}
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);

	/* Do not check if progress file exist or not. Just check upload seq */
	if (this_clouddata.upload_seq == 0)
		*meta_on_cloud = FALSE;
	else
		*meta_on_cloud = TRUE;

	return 0;

errcode_handle:
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	errcode = errno;
	return -errcode;
}

/************************************************************************
*
* Function name: actual_delete_inode
*        Inputs: ino_t this_inode, char d_type, ino_t root_inode
*                MOUNT_T *mptr
*       Summary: Delete the inode "this_inode" and the data blocks if this
*                is a regular file as well. "mptr" points to the mounted FS
*                structure if the FS is mounted (NULL if not).
*  Return value: 0 if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
int32_t actual_delete_inode(ino_t this_inode, char d_type, ino_t root_inode,
			MOUNT_T *mptr)
{
	char thisblockpath[400];
	char thismetapath[400];
	char targetmetapath[400];
	int32_t ret, errcode;
	int64_t count;
	int64_t total_blocks;
	int64_t current_page;
	int64_t page_pos;
	off_t cache_block_size;
	HCFS_STAT this_inode_stat;
	FILE_META_TYPE file_meta;
	BLOCK_ENTRY_PAGE tmppage;
	FILE *metafptr = NULL;
	int64_t e_index, which_page;
	size_t ret_size;
	struct timeval start_time, end_time;
	float elapsed_time;
	char block_status;
	char rootpath[METAPATHLEN];
	FILE *fptr;
	FS_STAT_T tmpstat;
	char meta_deleted;
	int64_t metasize = 0, metasize_blk = 0, dirty_delta, unpin_dirty_delta;
	int64_t truncate_size = 0;
	BOOL meta_on_cloud;

	meta_deleted = FALSE;
	if (mptr == NULL) {
		ret = fetch_stat_path(rootpath, root_inode);
		if (ret < 0)
			return ret;

		fptr = fopen(rootpath, "r+");
		if (fptr == NULL) {
			errcode = errno;
			write_log(0, "Unexpected error %d, %s\n", errcode,
				strerror(errcode));
			errcode = -errcode;
			return errcode;
		}
		setbuf(fptr, NULL);
		FREAD(&tmpstat, sizeof(FS_STAT_T), 1, fptr);
	}

	gettimeofday(&start_time, NULL);
	switch (d_type) {
	case D_ISDIR:
	case D_ISLNK:
		ret = meta_cache_remove(this_inode);
		if (ret < 0)
			return ret;
		fetch_meta_path(thismetapath, this_inode);

		ret = check_meta_on_cloud(this_inode, d_type,
				&meta_on_cloud, &metasize, &metasize_blk);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(0, "Inode %"PRIu64"%s. %s",
					"marked delete but meta file wasn't existed.",
					"Remove it from markdelete folder.");
				ret = disk_cleardelete(this_inode, root_inode);
			}
			return ret;
		}
		if (!meta_on_cloud) {
			/* Remove it and reclaim inode */
			ret = directly_delete_inode_meta(this_inode);
			if (ret < 0)
				return ret;
		} else {
			/* Push to to-delete queue */
			ret = delete_inode_meta(this_inode);
			if (ret < 0)
				return ret;
			truncate_size = d_type == D_ISDIR
					    ? sizeof(DIR_META_HEADER)
					    : sizeof(SYMLINK_META_HEADER);
			fetch_todelete_path(targetmetapath, this_inode);
			ret = truncate(targetmetapath, truncate_size);
			if (ret < 0) {
				write_log(0, "Error: Fail to truncate meta %"
						PRIu64". Code %d\n",
						(uint64_t)this_inode, errno);
				return ret;
			}
			ret = super_block_to_delete(this_inode, TRUE);
			if (ret < 0)
				return ret;
			/* Remove meta file directly */
			//fetch_meta_path(targetmetapath, this_inode);
			//ret = unlink(targetmetapath);
		}

		if (mptr == NULL)
			tmpstat.num_inodes--;
		else
			change_mount_stat(mptr, 0, -metasize, -1);
		break;

	case D_ISFIFO:
	case D_ISSOCK:
	case D_ISREG:
		ret = meta_cache_remove(this_inode);
		if (ret < 0)
			return ret;

		/* Open meta */
		ret = fetch_meta_path(thismetapath, this_inode);
		if (ret < 0)
			return ret;

		/*
		 * Meta file should be downloaded already in unlink,
		 * rmdir, or rename ops
		 */
		if (access(thismetapath, F_OK) != 0) {
			errcode = errno;
			if (errcode != ENOENT) {
				write_log(0, "IO error, code %d\n", errcode);
				return -errcode;
			}
			meta_deleted = TRUE;
			ret = fetch_todelete_path(thismetapath, this_inode);
			if (ret < 0)
				return ret;
		}

		metafptr = fopen(thismetapath, "r+");
		if (metafptr == NULL) {
			errcode = errno;
			if (errno == ENOENT) {
				write_log(0, "Inode %"PRIu64"%s. %s",
					"marked delete but meta file wasn't existed.",
					"Remove it from markdelete folder.");
				ret = disk_cleardelete(this_inode, root_inode);
			} else {
				write_log(0, "IO error in %s. Code %d, %s\n",
					__func__, errcode, strerror(errcode));
			}
			return -errcode;
		}

		/*Need to delete the meta. Move the meta file to "todelete"*/
		if (meta_deleted == FALSE) {
			ret = delete_inode_meta(this_inode);
			if (ret < 0) {
				fclose(metafptr);
				return ret;
			}
			/* Enqueue later */
			ret = super_block_to_delete(this_inode, FALSE);
			if (ret < 0) {
				fclose(metafptr);
				return ret;
			}
		}

		flock(fileno(metafptr), LOCK_EX);
		FSEEK(metafptr, 0, SEEK_SET);
		memset(&this_inode_stat, 0, sizeof(this_inode_stat));
		memset(&file_meta, 0, sizeof(file_meta));
		FREAD(&this_inode_stat, sizeof(HCFS_STAT), 1, metafptr);
		if (ret_size < 1) {
			write_log(2, "Skipping block deletion (meta gone)\n");
			fclose(metafptr);
			break;
		}

		FREAD(&file_meta, sizeof(FILE_META_TYPE), 1, metafptr);
		if (ret_size < 1) {
			write_log(2, "Skipping block deletion (meta gone)\n");
			fclose(metafptr);
			break;
		}

		/*Need to delete blocks as well*/
		/*
		 * TODO: Perhaps can move the actual block deletion to
		 * the deletion loop as well
		 */
		total_blocks =
		    BLOCKS_OF_SIZE(this_inode_stat.size, MAX_BLOCK_SIZE);

		current_page = -1;
		for (count = 0; count < total_blocks; count++) {
			e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
			which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

			if (current_page != which_page) {
				page_pos = seek_page2(&file_meta, metafptr,
					which_page, 0);
				if (page_pos <= 0) {
					count += (MAX_BLOCK_ENTRIES_PER_PAGE
						- 1);
					continue;
				}
				current_page = which_page;
				FSEEK(metafptr, page_pos, SEEK_SET);
				memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
				FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
					1, metafptr);
			}

			/* Skip if block does not exist */
			block_status = tmppage.block_entries[e_index].status;
			if ((block_status == ST_NONE) ||
				(block_status == ST_CLOUD))
				continue;

			ret = fetch_block_path(thisblockpath, this_inode,
					count);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}

			/*
			 * Do not need to change per-file statistics here
			 * as the file is in the process of being deleted
			 */
			if (access(thisblockpath, F_OK) == 0) {
				cache_block_size =
						check_file_size(thisblockpath);
				UNLINK(thisblockpath);
				if ((block_status == ST_LDISK) ||
				    (block_status == ST_LtoC))
					dirty_delta = -cache_block_size;
				else
					dirty_delta = 0;

				unpin_dirty_delta =
				    (P_IS_UNPIN(file_meta.local_pin)
					 ? dirty_delta
					 : 0);
				change_system_meta_ignore_dirty(
				    this_inode, 0, 0, -cache_block_size, -1,
				    dirty_delta, unpin_dirty_delta, FALSE);
			}
		}
		sem_wait(&(hcfs_system->access_sem));
		if (P_IS_PIN(file_meta.local_pin)) {
			hcfs_system->systemdata.pinned_size -=
					round_size(this_inode_stat.size);
			if (hcfs_system->systemdata.pinned_size < 0)
				hcfs_system->systemdata.pinned_size = 0;
		}

		hcfs_system->systemdata.system_size -= this_inode_stat.size;
		if (hcfs_system->systemdata.system_size < 0)
			hcfs_system->systemdata.system_size = 0;
		sync_hcfs_system_data(FALSE);
		sem_post(&(hcfs_system->access_sem));
		flock(fileno(metafptr), LOCK_UN);
		fclose(metafptr);

		/*
		 * Remove to-delete meta if no backend or this inode
		 * hadn't been synced
		 */
		ret = check_meta_on_cloud(this_inode, d_type,
				&meta_on_cloud, &metasize, &metasize_blk);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(0, "Inode %"PRIu64"%s. %s",
					"marked delete but meta file wasn't existed.",
					"Remove it from markdelete folder.");
				ret = disk_cleardelete(this_inode, root_inode);
			}
			return ret;
		}
		if (!meta_on_cloud) {
			fetch_todelete_path(targetmetapath, this_inode);
			ret = unlink(targetmetapath);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "Error: Fail to remove meta %"
						PRIu64". Code %d\n",
						(uint64_t)this_inode, errcode);
			}
			super_block_delete(this_inode);
			super_block_reclaim();
		} else {
			/* Push to clouddelete queue */
			ret = super_block_enqueue_delete(this_inode);
			if (ret < 0) {
				write_log(
				    0,
				    "Error: Fail to delete meta in %s. Code %d",
				    __func__, -ret);
				return ret;
			}

			/* Remove meta file directly */
			fetch_todelete_path(targetmetapath, this_inode);
			truncate_size = sizeof(FILE_META_HEADER);
			ret = truncate(targetmetapath, truncate_size);
			//ret = unlink(targetmetapath);
			if (ret < 0) {
				write_log(0, "Error: Fail to truncate meta %"
						PRIu64". Code %d\n",
						(uint64_t)this_inode, errno);
				return ret;
			}
		}

		if (mptr != NULL) {
			change_mount_stat(mptr, -this_inode_stat.size,
					-metasize, -1);
		} else {
			tmpstat.num_inodes--;
			tmpstat.system_size -=
				(this_inode_stat.size + metasize);
			tmpstat.meta_size -= metasize;
			tmpstat.num_inodes -= 1;
		}
		break;

	default:
		break;
	}

	change_system_meta(-metasize, -metasize_blk, 0, 0, 0, 0, TRUE);

	if (mptr == NULL) {
		FSEEK(fptr, 0, SEEK_SET);
		FWRITE(&tmpstat, sizeof(FS_STAT_T), 1, fptr);
		fclose(fptr);
	}

	/* unlink markdelete tag because it has been deleted */
	ret = disk_cleardelete(this_inode, root_inode);

	gettimeofday(&end_time, NULL);
	elapsed_time = (end_time.tv_sec + end_time.tv_usec * 0.000001)
		- (start_time.tv_sec + start_time.tv_usec * 0.000001);
	write_log(10, "Debug: Elapsed time = %f in %s\n",
		elapsed_time, __func__);

	return ret;

errcode_handle:
	flock(fileno(metafptr), LOCK_UN);
	fclose(metafptr);
	return errcode;
}

/* Mark inode as to delete on disk and lookup count table */
int32_t mark_inode_delete(fuse_req_t req, ino_t this_inode)
{
	int32_t ret;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	ret = disk_markdelete(this_inode, tmpptr);
	if (ret < 0)
		return ret;
	ret = lookup_markdelete(tmpptr->lookup_table, this_inode);
	return ret;
}

/* Mark inode as to delete on disk */
int32_t disk_markdelete(ino_t this_inode, MOUNT_T *mptr)
{
	char pathname[200];
	int32_t ret, errcode;
#ifdef _ANDROID_ENV_
	char *tmppath = NULL;
	FILE *fptr;
#endif

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) != 0)
		MKDIR(pathname, 0700);

	snprintf(pathname, 200, "%s/markdelete/inode%" PRIu64 "_%" PRIu64 "",
		 METAPATH, (uint64_t)this_inode, (uint64_t)mptr->f_ino);

	/*
	 * In Android env, if need to delete the inode, first remember
	 * the path of the inode if needed
	 */
#ifdef _ANDROID_ENV_
	if (access(pathname, F_OK) != 0) {
		if (IS_ANDROID_EXTERNAL(mptr->volume_type)) {
			if (mptr->vol_path_cache == NULL) {
				MKNOD(pathname, S_IFREG | 0700, 0);
			} else {
				ret = construct_path(mptr->vol_path_cache,
						     this_inode, &tmppath,
						     mptr->f_ino);
				if (ret < 0) {
					if (tmppath != NULL)
						free(tmppath);
					errcode = ret;
					goto errcode_handle;
				}
				fptr = fopen(pathname, "w");
				if (fptr == NULL) {
					errcode = -errno;
					write_log(0, "IO Error\n");
					goto errcode_handle;
				}
				ret = fprintf(fptr, "%s ", tmppath);
				if (ret < 0) {
					errcode = -EIO;
					fclose(fptr);
					write_log(0, "IO Error\n");
					goto errcode_handle;
				}

				fclose(fptr);
				free(tmppath);
			}
		} else {
			MKNOD(pathname, S_IFREG | 0700, 0);
		}
	}
#else
	if (access(pathname, F_OK) != 0)
		MKNOD(pathname, S_IFREG | 0700, 0);
#endif

	return 0;

errcode_handle:
	return errcode;
}

/* Clear inode as to delete on disk */
int32_t disk_cleardelete(ino_t this_inode, ino_t root_inode)
{
	char pathname[200];
	int32_t ret, errcode;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) < 0) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

	snprintf(pathname, 200, "%s/markdelete/inode%" PRIu64 "_%" PRIu64 "",
		 METAPATH, (uint64_t)this_inode, (uint64_t)root_inode);

	if (access(pathname, F_OK) == 0)
		UNLINK(pathname);

	return 0;

errcode_handle:
	return errcode;
}

/* Check if inode is marked as to delete on disk */
int32_t disk_checkdelete(ino_t this_inode, ino_t root_inode)
{
	char pathname[200];
	int32_t errcode;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) < 0) {
		errcode = errno;
		if (errcode != ENOENT)
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

	snprintf(pathname, 200, "%s/markdelete/inode%" PRIu64 "_%" PRIu64 "",
		 METAPATH, (uint64_t)this_inode, (uint64_t)root_inode);

	if (access(pathname, F_OK) == 0)
		return 1;

	return 0;
}

/*
 * At system startup, scan to delete markers on disk to determine if
 * there are inodes to be deleted.
 */
int32_t startup_finish_delete(void)
{
	DIR *dirp;
	struct dirent *de;
	HCFS_STAT tmpstat;
	char pathname[200];
	int32_t ret_val;
	ino_t tmp_ino, root_inode;
	int32_t errcode, ret;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) < 0) {
		errcode = errno;
		if (errcode != ENOENT)
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

	dirp = opendir(pathname);
	if (dirp == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

	errno = 0; de = readdir(dirp);
	if (de == NULL && errno) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		closedir(dirp);
		return -errcode;
	}

	while (de != NULL) {
		ret_val = sscanf(de->d_name, "inode%" PRIu64 "_%" PRIu64 "",
				(uint64_t *)&tmp_ino, (uint64_t *)&root_inode);
		if (ret_val > 0) {
			ret = fetch_inode_stat(tmp_ino, &tmpstat, NULL, NULL);
			if (ret < 0) {
				closedir(dirp);
				return ret;
			}
			if (S_ISFILE(tmpstat.mode))
				ret = actual_delete_inode(tmp_ino, D_ISREG,
						root_inode, NULL);
			if (S_ISDIR(tmpstat.mode))
				ret = actual_delete_inode(tmp_ino, D_ISDIR,
						root_inode, NULL);
			if (S_ISLNK(tmpstat.mode))
				ret = actual_delete_inode(tmp_ino, D_ISLNK,
						root_inode, NULL);

			if (ret < 0) {
				closedir(dirp);
				return ret;
			}
		}
		errno = 0; de = readdir(dirp);
		if (de == NULL && errno) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			closedir(dirp);
			return -errcode;
		}
	}

	closedir(dirp);
	return 0;
}

/*
 * Given parent "parent", search for "childname" in parent and return the
 * directory entry in structure pointed by "dentry" if found. If not or
 * if error, return the negation of error code.
 */
int32_t lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry,
		   BOOL is_external)
{
	META_CACHE_ENTRY_STRUCT *cache_entry;
	DIR_ENTRY_PAGE temp_page;
	int32_t temp_index, ret_val;

	cache_entry = meta_cache_lock_entry(parent);
	if (cache_entry == NULL)
		return -errno;

	ret_val = meta_cache_seek_dir_entry(parent, &temp_page,
			&temp_index, childname, cache_entry, is_external);
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

/**
 * When pin status changes, unpin-dirty size should be modified. Decrease size
 * when change from unpin to pin. Otherwise increase size when change from pin
 * to unpin.
 *
 * @param ptr Meta cache entry pointer.
 * @param pin New pinned status. TRUE means from unpin to pin, and FALSE
 *              means from pin to unpin
 *
 * @return 0 on succes, otherwise negative error code.
 */
static int32_t _change_unpin_dirty_size(META_CACHE_ENTRY_STRUCT *ptr,
					PIN_t pin)
{
	int32_t ret, errcode;
	size_t ret_size;
	FILE_STATS_TYPE filestats;

	ret = meta_cache_open_file(ptr);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	FSEEK(ptr->fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE),
		SEEK_SET);
	FREAD(&filestats, sizeof(FILE_STATS_TYPE), 1, ptr->fptr);

	/*
	 * when from unpin to pin, decrease unpin-dirty size, otherwise
	 * increase unpin-dirty size
	 */
	if (P_IS_PIN(pin))
		change_system_meta_ignore_dirty(ptr->inode_num, 0, 0, 0, 0, 0,
						-filestats.dirty_data_size,
						FALSE);
	else
		change_system_meta_ignore_dirty(ptr->inode_num, 0, 0, 0, 0, 0,
						filestats.dirty_data_size,
						FALSE);

	return 0;

errcode_handle:
	write_log(0, "Error: IO error in %s. Code %d\n", __func__, -errcode);
	return errcode;
}

/**
 * Change "local_pin" flag in meta cache. "new_pin_status" might be 1, 2 or 3
 * local_pin equals 1 or 2 means this inode is pinned at local
 * device but not neccessarily all blocks are local now.
 *
 * @param this_inode Inode of the meta to be changed.
 * @param this_mode Mode of this inode.
 * @param new_pin_status New "local_pin" status that is going to updated.
 *
 * @return 0 when succeding in change pin-flag, 1 when new pin-flag is the
 *         same as old one. Otherwise return negative error code.
 */
int32_t change_pin_flag(ino_t this_inode, mode_t this_mode, char new_pin_status)
{
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	FILE_META_TYPE file_meta;
	DIR_META_TYPE dir_meta;
	SYMLINK_META_TYPE symlink_meta;
	int32_t ret, ret_code;
	char old_pin_status;

	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		if (errno == ENOENT) {
			write_log(4, "No entry in %s. Skip pinning meta %"
					PRIu64, __func__, (uint64_t)this_inode);
			return 0;
		}
		return -errno;
	}

	ret_code = 0;
	/* Case regfile & fifo file */
	if (S_ISFILE(this_mode)) {
		ret = meta_cache_lookup_file_data(this_inode, NULL, &file_meta,
				NULL, 0, meta_cache_entry);
		if (ret < 0) {
			ret_code = ret;
			goto error_handling;
		}

		if (file_meta.local_pin == new_pin_status) {
			ret_code = 1;
		} else {
			old_pin_status = file_meta.local_pin;
			file_meta.local_pin = new_pin_status;
			ret = meta_cache_update_file_data(this_inode, NULL,
				&file_meta, NULL, 0, meta_cache_entry);
			if (ret < 0) {
				ret_code = ret;
				goto error_handling;
			}

			if (P_IS_PIN(old_pin_status) &&
				P_IS_PIN(new_pin_status)) {
				/* Only change pin flag */
				ret_code = 1;
			} else {
				/* Update unpin-dirty data size */
				ret = _change_unpin_dirty_size(meta_cache_entry,
						new_pin_status);
				if (ret < 0) {
					ret_code = ret;
					goto error_handling;
				}
			}
		}
	/* Case dir */
	} else if (S_ISDIR(this_mode)) {
		ret = meta_cache_lookup_dir_data(this_inode, NULL, &dir_meta,
				NULL, meta_cache_entry);
		if (ret < 0) {
			ret_code = ret;
			goto error_handling;
		}

		if (dir_meta.local_pin == new_pin_status) {
			ret_code = 1;
		} else {
			old_pin_status = dir_meta.local_pin;
			dir_meta.local_pin = new_pin_status;
			ret = meta_cache_update_dir_data(this_inode, NULL,
				&dir_meta, NULL, meta_cache_entry);
			if (ret < 0) {
				ret_code = ret;
				goto error_handling;
			}

			if (P_IS_PIN(old_pin_status) &&
				P_IS_PIN(new_pin_status)) {
				/* Only change pin flag */
				ret_code = 1;
			}
		}

	/* Case symlink */
	} else if (S_ISLNK(this_mode)) {
		ret = meta_cache_lookup_symlink_data(this_inode, NULL,
			&symlink_meta, meta_cache_entry);
		if (ret < 0) {
			ret_code = ret;
			goto error_handling;
		}

		if (symlink_meta.local_pin == new_pin_status) {
			ret_code = 1;
		} else {
			symlink_meta.local_pin = new_pin_status;
			ret = meta_cache_update_symlink_data(this_inode, NULL,
					&symlink_meta, meta_cache_entry);
			if (ret < 0) {
				ret_code = ret;
				goto error_handling;
			}
		}
	} else {
		write_log(0, "Error: Invalid type in %s\n", __func__);
		ret_code = -EINVAL;
	}

	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	return ret_code;

error_handling:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (ret_code == -ENOENT) {
		write_log(4, "No entry in %s. Skip pinning meta %"PRIu64,
				__func__, (uint64_t)this_inode);
		return 0;
	}
	return ret_code;
}

/*
 * Subroutin used to check size of the array and extend the
 * array size if it is full.
 */
static int32_t _check_extend_size(ino_t **ptr, int64_t num_elem,
	int64_t *max_elem_size)
{
	ino_t *tmp_ptr;

	if (num_elem >= *max_elem_size) {
		*max_elem_size *= 2;
		tmp_ptr = malloc(sizeof(ino_t) * (*max_elem_size));
		if (tmp_ptr == NULL)
			return -ENOMEM;
		memcpy(tmp_ptr, *ptr, sizeof(ino_t) * num_elem);
		free(*ptr);
		*ptr = tmp_ptr;
	}
	return 0;
}

/*
 * Subroutine used to shrink the array size so that eliminate wasting
 * on memory.
 */
static int32_t _check_shrink_size(ino_t **ptr, int64_t num_elem,
	int64_t max_elem_size)
{
	ino_t *tmp_ptr;

	if (num_elem == 0) { /* free memory when # of elem is zero */
		free(*ptr);
		*ptr = NULL;
		return 0;
	}

	if (num_elem == max_elem_size)/*Do nothing when size is exactly enough*/
		return 0;

	else if (num_elem > max_elem_size) { /* Error when size exceeds limit */
		write_log(0, "Error: Memory out of bound in %s", __func__);
		return -ENOMEM;
	}

	tmp_ptr = malloc(sizeof(ino_t) * num_elem);
	if (tmp_ptr == NULL)
		return -ENOMEM;
	memcpy(tmp_ptr, *ptr, sizeof(ino_t) * num_elem);
	free(*ptr);
	*ptr = tmp_ptr;

	return 0;
}

int32_t collect_dirmeta_children(DIR_META_TYPE *dir_meta, FILE *fptr,
		ino_t **dir_node_list, int64_t *num_dir_node,
		ino_t **nondir_node_list, int64_t *num_nondir_node,
		char **nondir_type_list, BOOL ignore_minapk)
{
	int32_t ret, errcode;
	int32_t count;
	int64_t ret_size, now_page_pos;
	int64_t total_children, half, now_nondir_size, now_dir_size;
	DIR_ENTRY_PAGE dir_page;
	DIR_ENTRY *tmpentry;

	*num_dir_node = 0;
	*num_nondir_node = 0;
	*dir_node_list = NULL;
	*nondir_node_list = NULL;
	if (nondir_type_list)
		*nondir_type_list = NULL;

	total_children = dir_meta->total_children;
	now_page_pos = dir_meta->tree_walk_list_head;
	if (total_children == 0 || now_page_pos == 0)
		return 0;

	half = total_children / 2 + 1; /* Avoid zero malloc */
	now_dir_size = half;
	now_nondir_size = half;
	*dir_node_list = (ino_t *) malloc(sizeof(ino_t) * now_dir_size);
	*nondir_node_list = (ino_t *) malloc(sizeof(ino_t) * now_nondir_size);
	if ((*dir_node_list == NULL) || (*nondir_node_list == NULL)) {
		errcode = -ENOMEM;
		goto errcode_handle;
	}

	if (nondir_type_list) { /* Malloc type array if needed */
		*nondir_type_list = (char *)malloc(sizeof(char) *
				(total_children + 1));
		if (*nondir_type_list == NULL) {
			errcode = -ENOMEM;
			goto errcode_handle;
		}
	}

	/* Collect all file in this dir */
	while (now_page_pos) {
		FSEEK(fptr, now_page_pos, SEEK_SET);
		FREAD(&dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);

		for (count = 0; count < dir_page.num_entries; count++) {

			tmpentry = &(dir_page.dir_entries[count]);
			if (!strcmp(tmpentry->d_name, ".") || /* Ignore */
				!strcmp(tmpentry->d_name, ".."))
				continue;

			/* Skip if need to check minapk and the name is
			minapk */
			if ((ignore_minapk == TRUE) &&
			    ((tmpentry->d_type == D_ISREG) &&
			     (is_minapk(tmpentry->d_name))))
				continue;

			if (tmpentry->d_type == D_ISDIR) {
				ret = _check_extend_size(dir_node_list,
					*num_dir_node, &now_dir_size);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				(*dir_node_list)[*num_dir_node] =
								tmpentry->d_ino;
				(*num_dir_node)++;

			} else {
				ret = _check_extend_size(nondir_node_list,
					*num_nondir_node, &now_nondir_size);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
				if (nondir_type_list) /* Get type if need */
					(*nondir_type_list)[*num_nondir_node] =
							tmpentry->d_type;
				(*nondir_node_list)[*num_nondir_node] =
								tmpentry->d_ino;
				(*num_nondir_node)++;
			}
		}

		now_page_pos = dir_page.tree_walk_next;
	}

	/* Shrink dir_node_list size */
	ret = _check_shrink_size(dir_node_list, *num_dir_node, now_dir_size);
	if (ret < 0) {
		write_log(0, "Error: Fail to malloc in %s\n", __func__);
		if (nondir_type_list && *nondir_type_list) {
			free(*nondir_type_list);
			*nondir_type_list = NULL;
		}
		free(*dir_node_list);
		free(*nondir_node_list);
		*dir_node_list = NULL;
		*nondir_node_list = NULL;
		return -ENOMEM;
	}

	/* Shrink nondir_node_list size */
	ret = _check_shrink_size(nondir_node_list, *num_nondir_node,
			now_nondir_size);
	if (ret < 0) {
		write_log(0, "Error: Fail to malloc in %s\n", __func__);
		if (nondir_type_list && *nondir_type_list) {
			free(*nondir_type_list);
			*nondir_type_list = NULL;
		}
		free(*dir_node_list);
		free(*nondir_node_list);
		*dir_node_list = NULL;
		*nondir_node_list = NULL;
		return -ENOMEM;
	}

	if (nondir_type_list) {
		if (*num_nondir_node == 0) {
			free(*nondir_type_list);
			*nondir_type_list = NULL;
		}
	}

	return 0;

errcode_handle:
	if (nondir_type_list && *nondir_type_list) {
		free(*nondir_type_list);
		*nondir_type_list = NULL;
	}
	free(*dir_node_list);
	free(*nondir_node_list);
	*dir_node_list = NULL;
	*nondir_node_list = NULL;
	write_log(0, "Error: Error occured in %s. Code %d", __func__, -errcode);
	return errcode;
}

/**
 * Update meta sequence number.
 *
 * @param bptr Meta memory cache entry of the meta file.
 *
 * @return 0 on success. Otherwise negative error code.
 */
int32_t update_meta_seq(META_CACHE_ENTRY_STRUCT *bptr)
{
	int32_t ret;
	ino_t this_inode;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE symmeta;

	if (bptr->need_inc_seq == FALSE) {
		write_log(10, "Skipping meta seq update\n");
		return 0;
	}

	this_inode = bptr->inode_num;

	if (S_ISFILE(bptr->this_stat.mode)) {
		ret = meta_cache_lookup_file_data(this_inode, NULL, &filemeta,
				NULL, 0, bptr);
		if (ret < 0)
			goto error_handling;
		filemeta.finished_seq += 1;
		ret = meta_cache_update_file_data(this_inode, NULL, &filemeta,
				NULL, 0, bptr);
		if (ret < 0)
			goto error_handling;
		write_log(10, "Debug: inode %"PRIu64" now seq is %lld\n",
				this_inode, filemeta.finished_seq);
	} else if (S_ISDIR(bptr->this_stat.mode)) {
		ret = meta_cache_lookup_dir_data(this_inode, NULL, &dirmeta,
				NULL, bptr);
		if (ret < 0)
			goto error_handling;
		dirmeta.finished_seq += 1;
		ret = meta_cache_update_dir_data(this_inode, NULL, &dirmeta,
				NULL, bptr);
		if (ret < 0)
			goto error_handling;
		write_log(10, "Debug: inode %"PRIu64" now seq is %lld\n",
				this_inode, dirmeta.finished_seq);

	} else if (S_ISLNK(bptr->this_stat.mode)) {
		ret = meta_cache_lookup_symlink_data(this_inode, NULL, &symmeta,
				bptr);
		if (ret < 0)
			goto error_handling;
		symmeta.finished_seq += 1;
		ret = meta_cache_update_symlink_data(this_inode, NULL, &symmeta,
				bptr);
		if (ret < 0)
			goto error_handling;
		write_log(10, "Debug: inode %"PRIu64" now seq is %lld\n",
				this_inode, symmeta.finished_seq);

	} else {
		ret = -EINVAL;
		goto error_handling;
	}

	bptr->need_inc_seq = FALSE;

	return 0;

error_handling:
	write_log(0, "Error: Fail to update meta seq number. Code %d in %s\n",
			-ret, __func__);
	return ret;
}

/**
 * Update seq number of block "bindex" to "now_seq".
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t update_block_seq(META_CACHE_ENTRY_STRUCT *bptr, off_t page_fpos,
		int64_t eindex, int64_t bindex, int64_t now_seq,
		BLOCK_ENTRY_PAGE *bpage_ptr)
{
	int32_t ret;

	if (!S_ISREG(bptr->this_stat.mode))
		return -EINVAL;

	/* Do nothing if seq number is already the same */
	if (bpage_ptr->block_entries[eindex].seqnum == now_seq)
		return 0;

	/* Update seqnum */
	bpage_ptr->block_entries[eindex].seqnum = now_seq;

	ret = meta_cache_update_file_data(bptr->inode_num, NULL, NULL,
			bpage_ptr, page_fpos, bptr);
	if (ret < 0)
		return ret;

	write_log(10, "Debug: block %"PRIu64"_%lld now seq is %lld",
		bptr->inode_num, bindex, now_seq);

	return 0;
}

/**
 * collect_dir_children
 *
 * Given a dir inode, collect all the children and classify them as
 * dir nodes and non-dir nodes. This function takes advantage of
 * tree-walk pointer in meta of a dir.
 *
 * @param this_inode The inode number of the dir
 * @param dir_node_list Address of a pointer used to point to dir-children-array
 * @param num_dir_node Number of elements in the dir-array
 * @param nondir_node_list Address of a pointer used to point to
 *                         non-dir-children-array
 * @param num_nondir_node Number of elements in the non-dir-array
 *
 * @return 0 on success, otherwise negative error code
 */
int32_t collect_dir_children(ino_t this_inode,
	ino_t **dir_node_list, int64_t *num_dir_node,
	ino_t **nondir_node_list, int64_t *num_nondir_node,
	char **nondir_type_list, BOOL ignore_minapk)
{
	int32_t ret, errcode;
	int64_t ret_size;
	char metapath[300];
	FILE *fptr;
	DIR_META_TYPE dir_meta;

	ret = fetch_meta_path(metapath, this_inode);
	if (ret < 0)
		return ret;

	fptr = fopen(metapath, "r");
	if (fptr == NULL) {
		ret = errno;
		write_log(0, "Fail to open meta %"PRIu64" in %s. Code %d\n",
			(uint64_t)this_inode, __func__, ret);
		return -ret;
	}

	flock(fileno(fptr), LOCK_EX);
	if (access(metapath, F_OK) < 0) {
		write_log(5, "meta %"PRIu64" does not exist in %s\n",
			(uint64_t)this_inode, __func__);
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		return -ENOENT;
	}

	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&dir_meta, sizeof(DIR_META_TYPE), 1, fptr);

	BOOL ignore_minapk1 = FALSE;
	if ((ignore_minapk == TRUE) &&
	    (dir_meta.root_inode == hcfs_system->data_app_root))
		ignore_minapk1 = TRUE;

	ret = collect_dirmeta_children(&dir_meta, fptr, dir_node_list,
			num_dir_node, nondir_node_list, num_nondir_node,
			nondir_type_list, ignore_minapk1);
	if (ret < 0) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		return ret;
	}

	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);

	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	*dir_node_list = NULL;
	*nondir_node_list = NULL;
	write_log(0, "Error: Error occured in %s. Code %d", __func__, -errcode);
	return errcode;
}

static BOOL _skip_inherit_key(char namespace, char *key) /* Only SECURITY now */
{
	if (namespace == SECURITY) {
		if (!strcmp("restorecon_last", key))
			return TRUE; /* Skip this key */
		else
			return FALSE; /* Do not skip. Inherit it */
	} else {
		return TRUE;
	}
}

/**
 * inherit_xattr()
 *
 * Inherit xattrs from parent.
 *
 * @param parent_inode Parent inode number
 * @param this_inode Self inode number
 * @param selbody_ptr Self meta cache entry, which had been locked.
 *
 * @return 0 on success, otherwise negative errcode.
 */
int32_t inherit_xattr(ino_t parent_inode, ino_t this_inode,
		META_CACHE_ENTRY_STRUCT *selbody_ptr)
{
	META_CACHE_ENTRY_STRUCT *pbody_ptr;
	XATTR_PAGE p_xattr_page, sel_xattr_page;
	DIR_META_TYPE dirmeta;
	int64_t p_xattr_pos, sel_xattr_pos;
	size_t total_keysize, total_keysize2;
	const char *key_ptr;
	char now_key[400], namespace, key[300];
	char *key_buf, *value_buf;
	size_t value_buf_size;
	size_t value_size;
	int32_t ret;

	UNUSED(this_inode);
	key_buf = NULL;
	value_buf = NULL;

	/* self fptr should be opened */
	ret = meta_cache_open_file(selbody_ptr);
	if (ret < 0)
		return ret;

	/* Lock parent */
	pbody_ptr = meta_cache_lock_entry(parent_inode);
	if (!pbody_ptr)
		return -errno;
	ret = meta_cache_open_file(pbody_ptr);
	if (ret < 0) {
		meta_cache_unlock_entry(pbody_ptr);
		return ret;
	}

	ret = meta_cache_lookup_dir_data(parent_inode, NULL, &dirmeta,
			NULL, pbody_ptr);
	if (ret < 0)
		goto errcode_handle;

	/* Check xattr_page */
	if (dirmeta.next_xattr_page <= 0) {
		write_log(10, "Debug: parent inode %"PRIu64
				" has no xattr page.\n",
				(uint64_t)parent_inode);
		meta_cache_close_file(pbody_ptr);
		meta_cache_unlock_entry(pbody_ptr);
		return 0;
	}

	ret = fetch_xattr_page(pbody_ptr, &p_xattr_page, &p_xattr_pos, FALSE);
	if (ret < 0) {
		if (ret == -ENOENT) {
			meta_cache_close_file(pbody_ptr);
			meta_cache_unlock_entry(pbody_ptr);
			return 0;
		}
		goto errcode_handle;
	}

	/* Fetch needed size of key buffer */
	ret = list_xattr(pbody_ptr, &p_xattr_page, NULL, 0, &total_keysize);
	if (ret < 0)
		goto errcode_handle;
	if (total_keysize <= 0) {
		write_log(10, "Debug: parent inode %"PRIu64" has no xattrs.\n",
				(uint64_t)parent_inode);
		meta_cache_close_file(pbody_ptr);
		meta_cache_unlock_entry(pbody_ptr);
		return 0;
	}

	key_buf = malloc(sizeof(char) * (total_keysize + 100));
	if (!key_buf) {
		ret = -ENOMEM;
		goto errcode_handle;
	}
	memset(key_buf, 0, total_keysize + 100);

	/* Fetch all namespace.key */
	ret = list_xattr(pbody_ptr, &p_xattr_page, key_buf, total_keysize,
			&total_keysize2);
	if (ret < 0)
		goto errcode_handle;
	key_buf[total_keysize] = 0;

	/* Tmp value buffer size */
	value_buf_size = MAX_VALUE_BLOCK_SIZE + 100;
	value_buf = malloc(value_buf_size);
	if (!value_buf) {
		ret = -ENOMEM;
		goto errcode_handle;
	}

	/* Self xattr page */
	ret = fetch_xattr_page(selbody_ptr, &sel_xattr_page,
			&sel_xattr_pos, TRUE);
	if (ret < 0)
		goto errcode_handle;

	/*
	 * Begin to insert.
	 * Step 1: Copy key to now_key and parse it
	 * Step 2: Get value of now_key from parent
	 * Step 3: Insert key-value pair to this inode
	 */
	key_ptr = key_buf;
	while (*key_ptr) {
		strncpy(now_key, key_ptr, 300);
		key_ptr += (strlen(now_key) + 1);

		ret = parse_xattr_namespace(now_key, &namespace, key);
		if (ret < 0) {
			write_log(0, "Error: Invalid xattr %s. Code %d",
					now_key, -ret);
			continue;
		}

		/* Choose namespace to inherit. Only SECURITY now. */
		if (_skip_inherit_key(namespace, key) == TRUE)
			continue;

		/* Get this xattr value */
		ret = -1;
		while (ret < 0) {
			ret = get_xattr(pbody_ptr, &p_xattr_page, namespace,
					key, value_buf, value_buf_size,
					&value_size);
			if (ret < 0 && ret != -ERANGE) { /* Error */
				goto errcode_handle;

			} else if ((ret < 0 && ret == -ERANGE) || /* Larger */
				(ret == 0 && value_size == value_buf_size)) {
				free(value_buf);
				value_buf = malloc(value_size + 100);
				if (!value_buf) {
					ret = -ENOMEM;
					goto errcode_handle;
				}
				value_buf_size = value_size + 100;
				continue;

			} else { /* ok */
				value_buf[value_size] = 0;
			}
		}

		write_log(10, "Debug: Insert xattr key %s and value %s\n",
				now_key, value_buf);
		ret = insert_xattr(selbody_ptr, &sel_xattr_page,
				sel_xattr_pos, namespace, key,
				value_buf, value_size, 0);
		if (ret < 0) {
			write_log(0, "Error: Fail to insert xattr. Code %d",
					-ret);
			continue;
		}
	}

	/* Free and unlock */
	free(key_buf);
	free(value_buf);

	ret = meta_cache_close_file(pbody_ptr);
	if (ret < 0) {
		meta_cache_unlock_entry(pbody_ptr);
		return ret;
	}
	ret = meta_cache_unlock_entry(pbody_ptr);
	if (ret < 0)
		return ret;

	return 0;

errcode_handle:
	if (key_buf)
		free(key_buf);
	if (value_buf)
		free(value_buf);
	meta_cache_close_file(pbody_ptr);
	meta_cache_unlock_entry(pbody_ptr);
	write_log(0, "Error: IO error in %s. Code %d\n", __func__, -ret);

	return ret;
}

/**
 * When restoring the meta file, system statistics and meta statistics
 * should be updated. All the status of blocks existing on cloud should
 * be modified to ST_CLOUD. Also need to update pin space usage if a file
 * is pinned.
 *
 * @param fptr File pointer of the restored file. It should be locked.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t restore_meta_structure(FILE *fptr)
{
	int32_t errcode, ret;
	HCFS_STAT this_stat;
	struct stat meta_stat; /* Meta file system stat */
	FILE_META_TYPE file_meta;
	BLOCK_ENTRY_PAGE tmppage;
	int64_t page_pos, current_page, which_page;
	int64_t total_blocks, count;
	int64_t pin_size, metasize, metasize_blk;
	int32_t e_index;
	uint8_t block_status;
	BOOL write_page;
	BOOL just_meta;
	size_t ret_size;
	FILE_STATS_TYPE file_stats;
	CLOUD_RELATED_DATA cloud_data;

	just_meta = FALSE;
	FSEEK(fptr, 0, SEEK_SET);
	fstat(fileno(fptr), &meta_stat);
	FREAD(&this_stat, sizeof(HCFS_STAT), 1, fptr);
	if (S_ISDIR(this_stat.mode)) {
		/* Restore cloud related data */
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE),
				SEEK_SET);
		FREAD(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
		cloud_data.size_last_upload = meta_stat.st_size;
		cloud_data.meta_last_upload = meta_stat.st_size;
		cloud_data.upload_seq = 1;
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE),
				SEEK_SET);
		FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
		just_meta = TRUE;

	} else if (S_ISLNK(this_stat.mode)) {
		/* Restore cloud related data */
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(SYMLINK_META_TYPE),
				SEEK_SET);
		FREAD(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
		cloud_data.size_last_upload = meta_stat.st_size;
		cloud_data.meta_last_upload = meta_stat.st_size;
		cloud_data.upload_seq = 1;
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(SYMLINK_META_TYPE),
				SEEK_SET);
		FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
		just_meta = TRUE;
	}

	/*
	 * Re-compute space usage and return when file is either dir or
	 * symlink
	 */
	if (just_meta) {
		/* Update statistics */
		metasize = meta_stat.st_size;
		metasize_blk = meta_stat.st_blocks * 512;
		UPDATE_RECT_SYSMETA(.delta_system_size = -metasize,
				    .delta_meta_size = -metasize_blk,
				    .delta_pinned_size = 0,
				    .delta_backend_size = -metasize,
				    .delta_backend_meta_size = -metasize,
				    .delta_backend_inodes = -1);
		return 0;
	}

	/* Restore status and statistics */
	FREAD(&file_meta, sizeof(FILE_META_TYPE), 1, fptr);
	total_blocks = BLOCKS_OF_SIZE(this_stat.size, MAX_BLOCK_SIZE);

	current_page = -1;
	write_page = FALSE;
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));

	for (count = 0; count < total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			if (write_page == TRUE) {
				FSEEK(fptr, page_pos, SEEK_SET);
				FWRITE(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
						1, fptr);
				write_page = FALSE;
			}
			page_pos = seek_page2(&file_meta, fptr,
					which_page, 0);
			if (page_pos <= 0) {
				count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
				continue;
			}
			current_page = which_page;
			memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));

			FSEEK(fptr, page_pos, SEEK_SET);
			FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
					1, fptr);
		}
		/* TODO: Perhaps check and delete local block? */
		block_status = tmppage.block_entries[e_index].status;
		switch (block_status) {
		case ST_TODELETE:
			tmppage.block_entries[e_index].status = ST_NONE;
			write_page = TRUE;
			break;
		case ST_LDISK:
		case ST_LtoC:
		case ST_CtoL:
		case ST_BOTH:
			tmppage.block_entries[e_index].status = ST_CLOUD;
			write_page = TRUE;
			break;
		default: /* Do nothing when st is ST_CLOUD / ST_NONE */
			break;
		}
	}
	if (write_page == TRUE) {
		FSEEK(fptr, page_pos, SEEK_SET);
		FWRITE(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
				1, fptr);
	}

	/* Restore file statistics */
	FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE), SEEK_SET);
	FREAD(&file_stats, sizeof(FILE_STATS_TYPE), 1, fptr);
	file_stats.num_cached_blocks = 0;
	file_stats.dirty_data_size = 0;
	file_stats.cached_size = 0;
	FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE), SEEK_SET);
	FWRITE(&file_stats, sizeof(FILE_STATS_TYPE), 1, fptr);
	/* Restore cloud related data */
	FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			sizeof(FILE_STATS_TYPE), SEEK_SET);
	FREAD(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	cloud_data.size_last_upload = this_stat.size + meta_stat.st_size;
	cloud_data.meta_last_upload = meta_stat.st_size;
	cloud_data.upload_seq = 1;
	FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			sizeof(FILE_STATS_TYPE), SEEK_SET);
	FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);

	/* Update statistics */
	if (P_IS_PIN(file_meta.local_pin))
		pin_size = round_size(this_stat.size);
	else
		pin_size = 0;
	metasize = meta_stat.st_size;
	metasize_blk = meta_stat.st_blocks * 512;
	UPDATE_RECT_SYSMETA(.delta_system_size = -(metasize + this_stat.size),
			    .delta_meta_size = -metasize_blk,
			    .delta_pinned_size = -pin_size,
			    .delta_backend_size = -(metasize + this_stat.size),
			    .delta_backend_meta_size = -metasize,
			    .delta_backend_inodes = -1);
	return 0;

errcode_handle:
	return errcode;
}

int32_t restore_borrowed_meta_structure(FILE *fptr, int32_t uid, ino_t src_ino,
		ino_t target_ino)
{
	int32_t errcode, ret;
	HCFS_STAT this_stat;
	struct stat meta_stat; /* Meta file system stat */
	struct stat blockstat;
	int64_t file_stats_pos;
	int64_t metasize, metasize_blk;
	int64_t total_blocks, current_page, page_pos;
	int64_t cached_size, num_cached_block, count, e_index, which_page;
	int64_t blkcount, pin_size;
	size_t ret_size;
	CLOUD_RELATED_DATA cloud_data;
	FILE_META_TYPE filemeta;
	FILE_STATS_TYPE file_stats_type;
	BLOCK_ENTRY_PAGE tmppage;
	char srcblockpath[METAPATHLEN], blockpath[METAPATHLEN];
	char block_status;
	BOOL write_page;
	BOOL just_meta = TRUE;

	fstat(fileno(fptr), &meta_stat);
	FSEEK(fptr, 0, SEEK_SET);
	FREAD(&this_stat, sizeof(HCFS_STAT), 1, fptr);
	this_stat.uid = uid;
	this_stat.gid = uid;
	this_stat.ino = target_ino;
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&this_stat, sizeof(HCFS_STAT), 1, fptr);

	/* size_last_upload = 0, meta_last_upload = 0, upload_seq = 0 */
	memset(&cloud_data, 0, sizeof(CLOUD_RELATED_DATA));

	if (S_ISDIR(this_stat.mode)) {
		just_meta = TRUE;
		/* Restore cloud related data */
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE),
				SEEK_SET);
		FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);

	} else if (S_ISLNK(this_stat.mode)) {
		just_meta = TRUE;
		this_stat.nlink = 1;
		FSEEK(fptr, 0, SEEK_SET);
		FWRITE(&this_stat, sizeof(HCFS_STAT), 1, fptr);
		/* Restore cloud related data */
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(SYMLINK_META_TYPE),
				SEEK_SET);
		FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);

	} else if (S_ISREG(this_stat.mode)) {
		just_meta = FALSE;
		this_stat.nlink = 1;
		FSEEK(fptr, 0, SEEK_SET);
		FWRITE(&this_stat, sizeof(HCFS_STAT), 1, fptr);
		/* Restore cloud related data */
		FSEEK(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
				sizeof(FILE_STATS_TYPE), SEEK_SET);
		FWRITE(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	}
	/*
	 * Re-compute space usage and return when file is either dir or
	 * symlink
	 */
	if (just_meta) {
		metasize = meta_stat.st_size;
		metasize_blk = meta_stat.st_blocks * 512;
		UPDATE_RECT_SYSMETA(.delta_system_size = -metasize,
				.delta_meta_size = -metasize_blk,
				.delta_pinned_size = 0,
				.delta_backend_size = 0,
				.delta_backend_meta_size = 0,
				.delta_backend_inodes = 0);
		return 0;
	}

	/* Begin to copy regular file */
	FSEEK(fptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	if (P_IS_UNPIN(filemeta.local_pin)) {
		/* Don't fetch blocks */
		return -EINVAL;
	}

	total_blocks = BLOCKS_OF_SIZE(this_stat.size, MAX_BLOCK_SIZE);
	current_page = -1;
	write_page = FALSE;
	cached_size = 0;
	num_cached_block = 0;
	page_pos = 0;
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));

	for (count = 0; count < total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			if (write_page == TRUE) {
				FSEEK(fptr, page_pos, SEEK_SET);
				FWRITE(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
						1, fptr);
				write_page = FALSE;
			}
			page_pos = seek_page2(&filemeta, fptr,
					which_page, 0);
			if (page_pos <= 0) {
				count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
				continue;
			}
			current_page = which_page;
			memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));

			FSEEK(fptr, page_pos, SEEK_SET);
			FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE),
					1, fptr);
		}

		/* Terminate pin blocks downloading if system is going down */
		if (hcfs_system->system_going_down == TRUE) {
			errcode = -ESHUTDOWN;
			goto errcode_handle;
		}

		block_status = tmppage.block_entries[e_index].status;
		switch (block_status) {
		case ST_TODELETE:
			tmppage.block_entries[e_index].status = ST_NONE;
			write_page = TRUE;
			break;
		case ST_LDISK:
		case ST_LtoC:
		case ST_BOTH:
			fetch_block_path(srcblockpath, src_ino, count);
			fetch_restore_block_path(blockpath, target_ino, count);
			/* Copy block from source */
			ret = copy_file(srcblockpath, blockpath);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}
			tmppage.block_entries[e_index].status = ST_LDISK;
			write_page = TRUE;
			/* Update file stats */
			ret = stat(blockpath, &blockstat);
			if (ret == 0) {
				cached_size += blockstat.st_blocks * 512;
				num_cached_block += 1;
			} else {
				write_log(0, "%s %s. Code %d",
					  "Error: Fail to stat block in",
					  __func__, errno);
			}
			break;
		case ST_CtoL:
		case ST_CLOUD:
			write_log(4, "Warn: Block in high priority pin file"
					" with status CtoL/Cloud\n");
			errcode = -EINVAL;
			goto errcode_handle;
			break;
		default: /* Do nothing when st is ST_NONE */
			break;
		}
	}
	if (write_page == TRUE) {
		FSEEK(fptr, page_pos, SEEK_SET);
		FWRITE(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	}

	/* Update file stats */
	file_stats_pos = offsetof(FILE_META_HEADER, fst);
	FSEEK(fptr, file_stats_pos, SEEK_SET);
	FREAD(&file_stats_type, sizeof(FILE_STATS_TYPE), 1, fptr);
	file_stats_type.num_cached_blocks = num_cached_block;
	file_stats_type.cached_size = cached_size;
	file_stats_type.dirty_data_size = cached_size;
	FSEEK(fptr, file_stats_pos, SEEK_SET);
	FWRITE(&file_stats_type, sizeof(FILE_STATS_TYPE), 1, fptr);

	/* Update rectified statistics */
	pin_size = round_size(this_stat.size);
	metasize = meta_stat.st_size;
	metasize_blk = meta_stat.st_blocks * 512;
	UPDATE_RECT_SYSMETA(.delta_system_size = -(metasize + this_stat.size),
			    .delta_meta_size = -metasize_blk,
			    .delta_pinned_size = -pin_size,
			    .delta_backend_size = 0,
			    .delta_backend_meta_size = 0,
			    .delta_backend_inodes = 0);

	/* Update cache statistics */
	ret = update_restored_cache_usage(cached_size, num_cached_block,
			filemeta.local_pin);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	return 0;
errcode_handle:
	if (just_meta == FALSE) {
		write_log(4, "Cleaning up blocks of broken file\n");
		for (blkcount = 0; blkcount <= count; blkcount++) {
			fetch_restore_block_path(blockpath, target_ino,
						 blkcount);
			unlink(blockpath);
		}
	}
	write_log(0, "Restore Error: Code %d\n", -errcode);
	return errcode;
}

/**
 * Restore a meta file from cloud. Downlaod the meta file
 * to a temp file, and rename to the correct meta file path.
 *
 * @param this_inode Inode number of the meta file to be restored.
 *
 * @return 0 on success or meta file had been restored. Otherwise
 *         negative error code.
 */
int32_t restore_meta_file(ino_t this_inode)
{
	char metapath[METAPATHLEN];
	char restored_metapath[400], objname[100];
	FILE *fptr;
	int32_t errcode, ret;

	fetch_meta_path(metapath, this_inode);
	/* Meta file had been restored. */
	if (!access(metapath, F_OK))
		return 0;

	/* Begin to restore. Download meta file and restore content */
	if (hcfs_system->backend_is_online == FALSE)
		return -ENOTCONN;

	fetch_temp_restored_meta_path(restored_metapath, this_inode);
	fptr = fopen(restored_metapath, "a+");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errno));
		return -EIO;
	}
	fclose(fptr);
	fptr = fopen(restored_metapath, "r+");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errno));
		return -EIO;
	}

	/* Get file lock */
	ret = flock(fileno(fptr), LOCK_EX);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errno));
		fclose(fptr);
		return -EIO;
	}
	if (!access(metapath, F_OK)) { /* Check again when get lock */
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		unlink(restored_metapath);
		return 0;
	}
	setbuf(fptr, NULL);

	/* Fetch meta from cloud */
	sprintf(objname, "meta_%"PRIu64, (uint64_t)this_inode);
	/* TODO: When restoring, querying object id and then download */
	ret = fetch_from_cloud(fptr, RESTORE_FETCH_OBJ, objname, NULL);
	if (ret < 0) {
		write_log(0,
			  "Error: Fail to fetch meta from cloud in %s. Code %d",
			  __func__, -ret);
		goto errcode_handle;
	}

	ret = restore_meta_structure(fptr);
	if (ret < 0) {
		write_log(0,
			  "Error: Fail to restore meta struct in %s. Code %d",
			  __func__, -ret);
		goto errcode_handle;
	}

	ret = rename(restored_metapath, metapath);
	if (ret < 0) {
		write_log(0,
			  "Error: Fail to rename to meta path in %s. Code %d",
			  __func__, -ret);
		goto errcode_handle;
	}

	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);

	return 0;

errcode_handle:
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return ret;
}

/**
 * Check the data location of a filesystem object.
 *
 * @param this_inode Inode number of the file / dir to be checked.
 *
 * @return 0 if a file / data is "local", 1 if "cloud", or 2 if "hybrid".
 *         Otherwise negative error code.
 */
int32_t check_data_location(ino_t this_inode)
{
	int32_t errcode;
	char metapath[METAPATHLEN];
	HCFS_STAT thisstat;
	META_CACHE_ENTRY_STRUCT *thisptr;
	char inode_loc;
	FILE_STATS_TYPE tmpstats;
	ssize_t ret_ssize;

	write_log(10, "Debug checkloc inode %" PRIu64 "\n",
		  (uint64_t)this_inode);
	errcode = fetch_meta_path(metapath, this_inode);
	if (errcode < 0)
		return errcode;

	if (access(metapath, F_OK) != 0)
		return -ENOENT;

	thisptr = meta_cache_lock_entry(this_inode);
	if (thisptr == NULL)
		return -errno;

	errcode = meta_cache_lookup_file_data(this_inode, &thisstat,
						NULL, NULL, 0, thisptr);
	if (errcode < 0)
		goto errcode_handle;

	if (S_ISREG(thisstat.mode)) {
		errcode = meta_cache_open_file(thisptr);
		if (errcode < 0)
			goto errcode_handle;
		PREAD(fileno(thisptr->fptr), &tmpstats, sizeof(FILE_STATS_TYPE),
		      sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE));
		if ((tmpstats.num_blocks == 0) ||
		    (tmpstats.num_blocks == tmpstats.num_cached_blocks))
			inode_loc = 0;  /* If the location is "local" */
		else if (tmpstats.num_cached_blocks == 0)
			inode_loc = 1;  /* If the location is "cloud" */
		else
			inode_loc = 2;  /* If the location is "hybrid" */
	} else {
		inode_loc = 0;  /* Non-file obj defaults to "local" */
	}

	write_log(6, "Location of inode %" PRIu64 " is %d.\n",
	          (uint64_t) this_inode, inode_loc);
	meta_cache_close_file(thisptr);
	meta_cache_unlock_entry(thisptr);

	return inode_loc;

errcode_handle:
	write_log(0, "Cannot read meta when checking loc for inode %"
	          PRIu64 "\n", (uint64_t) this_inode);
	meta_cache_unlock_entry(thisptr);
	return errcode;
}
