/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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
**************************************************************************/
#include "metaops.h"

#include <sys/file.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <semaphore.h>
#include <sys/mman.h>

#include "global.h"
#include "utils.h"
#include "file_present.h"
#include "params.h"
#include "dir_entry_btree.h"
#include "hfuse_system.h"
#include "macro.h"
#include "logger.h"
#include "mount_manager.h"

extern SYSTEM_CONF_STRUCT system_config;

static inline void logerr(int errcode, char *msg)
{
	if (errcode > 0)
		write_log(0, "%s. Code %d, %s\n", msg, errcode, strerror(errcode));
	else
		write_log(0, "%s.\n", msg);
}

/************************************************************************
*
* Function name: init_dir_page
*        Inputs: DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
*                long long this_page_pos
*       Summary: Initialize directory entries for a new directory object.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode,
				ino_t parent_inode, long long this_page_pos)
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
*        Inputs: ino_t parent_inode, ino_t child_inode, char *childname,
*                mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Add a directory entry with name "childname", inode number
*                "child_inode", and mode "child_mode" to the directory
*                with inode number "parent_inode". Meta cache entry of
*                parent is pointed by "body_ptr".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	struct stat parent_stat;
	DIR_META_TYPE parent_meta;
	DIR_ENTRY_PAGE tpage, new_root, tpage2;
	DIR_ENTRY temp_entry, overflow_entry;
	long long overflow_new_page;
	int ret, errcode;
	size_t ret_size;
	int sem_val;
	char no_need_rewrite;
	long ret_pos;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];

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
	if (S_ISDIR(child_mode))
		temp_entry.d_type = D_ISDIR;
	if (S_ISLNK(child_mode))
		temp_entry.d_type = D_ISLNK;

	/* Load parent meta from meta cache */
	ret = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
					&parent_meta, NULL, body_ptr);

	if (ret < 0)
		return ret;

	/* Initialize B-tree insertion by first loading the root of
	*  the B-tree. */
	tpage.this_page_pos = parent_meta.root_entry_page;

	ret = meta_cache_open_file(body_ptr);

	if (ret < 0)
		return ret;

	FSEEK(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);

	FREAD(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	
	/* Drop all cached pages first before inserting */
	/* TODO: Future changes could remove this limitation if can update
	*  cache with each node change in b-tree*/
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
							temp_child_page_pos);

	/* An error occured and the routine will terminate now */
	/* TODO: Consider error recovering here */
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		return ret;
	}

	/* If return value is 1, we need to handle overflow by splitting
	*  the old root node in two and create a new root page to point
	*  to the two splitted nodes. Note that a new node has already been
	*  created in this case and pointed by "overflow_new_page". */
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

		/* Insert the new root to the head of tree_walk_list. This list
		*  is for listing nodes in the B-tree in readdir operation. */
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
				sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
		new_root.num_entries = 1;
		memcpy(&(new_root.dir_entries[0]), &overflow_entry,
							sizeof(DIR_ENTRY));
		/* The two children of the new root is the old root and
		*  the new node created by the overflow. */
		new_root.child_page_pos[0] = parent_meta.root_entry_page;
		new_root.child_page_pos[1] = overflow_new_page;

		/* Set the root of B-tree to the new root, and write the
		*  content of the new root the the meta file. */
		parent_meta.root_entry_page = new_root.this_page_pos;
		FSEEK(body_ptr->fptr, new_root.this_page_pos, SEEK_SET);

		FWRITE(&new_root, sizeof(DIR_ENTRY_PAGE), 1,
				body_ptr->fptr);

		/* Change the parent of the old root to point to the new root.
		*  Write to the meta file afterward. */
		tpage.parent_page_pos = new_root.this_page_pos;
		FSEEK(body_ptr->fptr, tpage.this_page_pos, SEEK_SET);
		FWRITE(&tpage, sizeof(DIR_ENTRY_PAGE), 1,
				body_ptr->fptr);

		/* If no_need_rewrite is true, we have already write modified
		*  content for the new node from the overflow. Otherwise we need
		*  to write it to the meta file here. */
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

		/* Complete the splitting by updating the meta of the
		*  directory. */
		FSEEK(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
		FWRITE(&parent_meta, sizeof(DIR_META_TYPE), 1,
					body_ptr->fptr);
	}

	/*If the new entry is a subdir, increase the hard link of the parent*/

	if (child_mode & S_IFDIR)
		parent_stat.st_nlink++;

	parent_meta.total_children++;
	write_log(10,
		"TOTAL CHILDREN is now %lld\n", parent_meta.total_children);

	set_timestamp_now(&parent_stat, MTIME | CTIME);
	/* Stat may be dirty after the operation so should write them back
	*  to cache*/
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
*        Inputs: ino_t parent_inode, ino_t child_inode, char *childname,
*                mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: Remove a directory entry with name "childname", inode number
*                "child_inode", and mode "child_mode" from the directory
*                with inode number "parent_inode". Meta cache entry of
*                parent is pointed by "body_ptr".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	struct stat parent_stat;
	DIR_META_TYPE parent_meta;
	DIR_ENTRY_PAGE tpage;
	int sem_val;
	DIR_ENTRY temp_entry;
	int ret, errcode;
	size_t ret_size;

	DIR_ENTRY temp_dir_entries[2*(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[2*(MAX_DIR_ENTRIES_PER_PAGE+3)];

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
	if (S_ISDIR(child_mode))
		temp_entry.d_type = D_ISDIR;
	if (S_ISLNK(child_mode))
		temp_entry.d_type = D_ISLNK;

	/* Initialize B-tree deletion by first loading the root of B-tree */
	ret = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);
	if (ret < 0)
		return ret;

	tpage.this_page_pos = parent_meta.root_entry_page;

	ret = meta_cache_open_file(body_ptr);
	if (ret < 0)
		return ret;

	/* Drop all cached pages first before deleting */
	/* TODO: Future changes could remove this limitation if can update cache
	*  with each node change in b-tree*/

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
							temp_child_page_pos);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	write_log(10, "delete dir entry returns %d\n", ret);
	/* tpage might be invalid after calling delete_dir_entry_btree */

	if (ret == 0) {
		/* If the entry is a subdir, decrease the hard link of
		*  the parent*/

		if (child_mode & S_IFDIR)
			parent_stat.st_nlink--;

		parent_meta.total_children--;
		write_log(10, "TOTAL CHILDREN is now %lld\n",
						parent_meta.total_children);
		set_timestamp_now(&parent_stat, MTIME | CTIME);

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
int change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	DIR_ENTRY_PAGE tpage;
	int count;
	int ret_val;
	struct stat tmpstat;

	ret_val = meta_cache_seek_dir_entry(self_inode, &tpage, &count,
								"..", body_ptr);

	if ((ret_val == 0) && (count >= 0)) {
		/*Found the entry. Change parent inode*/
		ret_val = meta_cache_lookup_dir_data(self_inode, &tmpstat,
					NULL, NULL, body_ptr);
		if (ret_val < 0)
			return ret_val;

		tpage.dir_entries[count].d_ino = parent_inode2;
		set_timestamp_now(&tmpstat, MTIME | CTIME);
		ret_val = meta_cache_update_dir_data(self_inode, &tmpstat,
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
*        Inputs: ino_t self_inode, char *targetname,
*                ino_t new_inode, struct stat *thisstat,
*                META_CACHE_ENTRY_STRUCT *body_ptr
*       Summary: For a directory "self_inode", change the inode and mode
*                of entry "targetname" to "new_inode" and "new_mode".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int change_dir_entry_inode(ino_t self_inode, char *targetname,
		ino_t new_inode, mode_t new_mode,
		META_CACHE_ENTRY_STRUCT *body_ptr)
{
	DIR_ENTRY_PAGE tpage;
	int count;
	int ret_val;
	struct stat tmpstat;

	ret_val = meta_cache_seek_dir_entry(self_inode, &tpage, &count,
						targetname, body_ptr);

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
		}
		if (S_ISLNK(new_mode)) {
			write_log(10, "Debug: change to type LNK\n");
			tpage.dir_entries[count].d_type = D_ISLNK;
		}
		if (S_ISDIR(new_mode)) {
			write_log(10, "Debug: change to type DIR\n");
			tpage.dir_entries[count].d_type = D_ISDIR;
		}

		set_timestamp_now(&tmpstat, MTIME | CTIME);
		ret_val = meta_cache_update_dir_data(self_inode, &tmpstat,
					NULL, &tpage, body_ptr);
		return ret_val;
	}
	if ((ret_val == 0) && (count < 0))  /* Not found */
		ret_val = -ENOENT;

	return ret_val;
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
int delete_inode_meta(ino_t this_inode)
{
	char todelete_metapath[METAPATHLEN];
	char thismetapath[METAPATHLEN];
	FILE *todeletefptr, *metafptr;
	char filebuf[5000];
	int ret, errcode;
	size_t ret_size, write_size;

	ret = super_block_to_delete(this_inode);
	if (ret < 0)
		return ret;
	ret = fetch_todelete_path(todelete_metapath, this_inode);
	if (ret < 0)
		return ret;
	
	ret = fetch_meta_path(thismetapath, this_inode);
	if (ret < 0)
		return ret;

	/*Try a rename first*/
	ret = rename(thismetapath, todelete_metapath);
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
	ret = meta_cache_remove(this_inode);
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
int decrease_nlink_inode_file(fuse_req_t req, ino_t this_inode)
{
	struct stat this_inode_stat;
	int ret_val;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(this_inode);
	/* Only fetch inode stat here. Can be replaced by meta_cache_lookup_file_data() */
	if (body_ptr == NULL)
		return -ENOMEM;

	ret_val = meta_cache_lookup_dir_data(this_inode, &this_inode_stat,
							NULL, NULL, body_ptr);
	if (ret_val < 0) {
		meta_cache_unlock_entry(body_ptr);
		return ret_val;
	}

	if (this_inode_stat.st_nlink <= 1) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);

		ret_val = mark_inode_delete(req, this_inode);

	} else {
		/* If it is still referenced, update the meta file. */
		this_inode_stat.st_nlink--;
		set_timestamp_now(&this_inode_stat, CTIME);
		ret_val = meta_cache_update_dir_data(this_inode,
					&this_inode_stat, NULL, NULL, body_ptr);
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
	}

	return ret_val;
}

static inline long long longpow(long long base, int power)
{
	long long tmp;
	int count;

	tmp = 1;

	for (count=0; count < power; count++)
		tmp = tmp * base;

	return tmp;
}
/* Checks if page_index belongs to direct or what indirect page */
int _check_page_level(long long page_index)
{
	long long tmp_index;
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

	/* TODO: boundary handling for quadruple indirect */
	return 4;
}

long long _load_indirect(long long target_page, FILE_META_TYPE *temp_meta,
			FILE *fptr, int level)
{
	long long tmp_page_index;
	long long tmp_pos, tmp_target_pos;
	long long tmp_ptr_page_index, tmp_ptr_index;
	PTR_ENTRY_PAGE tmp_ptr_page;
	int count, ret, errcode;
	size_t ret_size;
	long ret_pos;

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
		break;
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
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page
*                long long hint_page
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
long long seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page,
			long long hint_page)
{
	off_t filepos;
	int sem_val;
	FILE_META_TYPE temp_meta;
	int which_indirect;
	int ret;

	/* TODO: hint_page is not used now. Consider how to enhance. */
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

	which_indirect = _check_page_level(target_page);

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
		break;
	}

	return filepos;
}

/* Helper function for creating new page. */
long long _create_indirect(long long target_page, FILE_META_TYPE *temp_meta,
			META_CACHE_ENTRY_STRUCT *body_ptr, int level)
{
	long long tmp_page_index;
	long long tmp_pos, tmp_target_pos;
	long long tmp_ptr_page_index, tmp_ptr_index;
	PTR_ENTRY_PAGE tmp_ptr_page, empty_ptr_page;
	int count, ret, errcode;
	BLOCK_ENTRY_PAGE temppage;
	size_t ret_size;
	long ret_pos;

	tmp_page_index = target_page - 1;

	for (count = 1; count < level; count++)
		tmp_page_index -= (longpow(POINTERS_PER_PAGE, count));

	switch (level) {
	case 1:
		tmp_target_pos = temp_meta->single_indirect;
		if (tmp_target_pos == 0) {
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
		break;
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
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page
*       Summary: Given meta cache entry pointed by "body_ptr", create the block
*                entry page "target_page" and return the file pos of the page.
*  Return value: File pos of the page if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
long long create_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page)
{
	off_t filepos;
	BLOCK_ENTRY_PAGE temppage;
	int sem_val;
	FILE_META_TYPE temp_meta;
	int which_indirect;
	int ret, errcode;
	long ret_pos;

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

	which_indirect = _check_page_level(target_page);
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
		break;
	}

	return filepos;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: seek_page2
*        Inputs: FILE_META_TYPE *temp_meta, FILE *fptr, long long target_page
*                long long hint_page
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
long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page)
{
	off_t filepos;
	int which_indirect;

	/* TODO: hint_page is not used now. Consider how to enhance. */
	/* First check if meta cache is locked */
	/* Do not actually create page here */
	/*TODO: put error handling for the read/write ops here*/

	if (target_page < 0)
		return -EPERM;

	which_indirect = _check_page_level(target_page);

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
		break;
	}

	return filepos;
}

/************************************************************************
*
* Function name: actual_delete_inode
*        Inputs: ino_t this_inode, char d_type
*       Summary: Delete the inode "this_inode" and the data blocks if this
*                is a regular file as well.
*  Return value: 0 if successful. Otherwise returns
*                negation of error code.
*
*************************************************************************/
int actual_delete_inode(ino_t this_inode, char d_type)
{
	char thisblockpath[400];
	int ret, errcode;
	long long count;
	long long total_blocks;
	off_t cache_block_size;
	struct stat this_inode_stat;

	switch (d_type) {
	case D_ISDIR:
		/*Need to delete the inode by moving it to "todelete" path*/
		ret = delete_inode_meta(this_inode);
		if (ret < 0)
			return ret;
		break;
	
	case D_ISLNK:
		/*Need to delete the inode by moving it to "todelete" path*/
		ret = delete_inode_meta(this_inode);
		if (ret < 0)
			return ret;
		break;

	case D_ISREG:
		ret = fetch_inode_stat(this_inode, &this_inode_stat, NULL);
		if (ret < 0)
			return ret;

		/*Need to delete the meta. Move the meta file to "todelete"*/
		ret = delete_inode_meta(this_inode);
		if (ret < 0)
			return ret;

		/*Need to delete blocks as well*/
		/* TODO: Perhaps can move the actual block deletion to the
		*  deletion loop as well*/
		if (this_inode_stat.st_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((this_inode_stat.st_size-1) /
							MAX_BLOCK_SIZE) + 1;

		
		for (count = 0; count < total_blocks; count++) {
			ret = fetch_block_path(thisblockpath, this_inode,
						count);
			if (ret < 0)
				return ret;
			if (access(thisblockpath, F_OK) == 0) {
				cache_block_size =
						check_file_size(thisblockpath);
				UNLINK(thisblockpath);
				sem_wait(&(hcfs_system->access_sem));
				hcfs_system->systemdata.cache_size -=
						(long long) cache_block_size;
				hcfs_system->systemdata.cache_blocks -= 1;
				sem_post(&(hcfs_system->access_sem));
			}
		}
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.system_size -= this_inode_stat.st_size;
		sync_hcfs_system_data(FALSE);
		sem_post(&(hcfs_system->access_sem));

		break;
	default:
		break;
	}
	
	ret = disk_cleardelete(this_inode);
	return ret;

errcode_handle:
	return errcode;
}

/* Mark inode as to delete on disk and lookup count table */
int mark_inode_delete(fuse_req_t req, ino_t this_inode)
{
	int ret;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	ret = disk_markdelete(this_inode);
	if (ret < 0)
		return ret;
	ret = lookup_markdelete(tmpptr->lookup_table, this_inode);
	return ret;
}

/* Mark inode as to delete on disk */
int disk_markdelete(ino_t this_inode)
{
	char pathname[200];
	int ret, errcode;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) != 0) {
		MKDIR(pathname, 0700);
	}

#ifdef ARM_32bit_
	snprintf(pathname, 200, "%s/markdelete/inode%lld",
                                                METAPATH, this_inode);
#else
	snprintf(pathname, 200, "%s/markdelete/inode%ld",
						METAPATH, this_inode);
#endif

	if (access(pathname, F_OK) != 0) {
		MKNOD(pathname, S_IFREG | 0700, 0);
	}

	return 0;

errcode_handle:
	return errcode;
}

/* Clear inode as to delete on disk */
int disk_cleardelete(ino_t this_inode)
{
	char pathname[200];
	int ret, errcode;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) < 0) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

#ifdef ARM_32bit_
	snprintf(pathname, 200, "%s/markdelete/inode%lld",
                                                METAPATH, this_inode);
#else
     	snprintf(pathname, 200, "%s/markdelete/inode%ld",
                                                METAPATH, this_inode);
#endif

	if (access(pathname, F_OK) == 0) {
		UNLINK(pathname);
	}

	return 0;

errcode_handle:
	return errcode;
}

/* Check if inode is marked as to delete on disk */
int disk_checkdelete(ino_t this_inode)
{
	char pathname[200];
	int errcode;

	snprintf(pathname, 200, "%s/markdelete", METAPATH);

	if (access(pathname, F_OK) < 0) {
		errcode = errno;
		if (errcode != ENOENT)
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		return -errcode;
	}

#ifdef ARM_32bit_
	snprintf(pathname, 200, "%s/markdelete/inode%lld",
                                                METAPATH, this_inode);
#else
     	snprintf(pathname, 200, "%s/markdelete/inode%ld",
                                                METAPATH, this_inode);
#endif

	if (access(pathname, F_OK) == 0)
		return 1;

	return 0;
}

/* At system startup, scan to delete markers on disk to determine if
there are inodes to be deleted. */
int startup_finish_delete()
{
	DIR *dirp;
	struct dirent tmpent, *tmpptr;
	struct stat tmpstat;
	char pathname[200];
	int ret_val;
	ino_t tmp_ino;
	int errcode, ret;

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

	errcode = readdir_r(dirp, &tmpent, &tmpptr);
	if (errcode > 0) {
		write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
		closedir(dirp);
		return -errcode;
	}

	while (tmpptr != NULL) {
#ifdef ARM_32bit_
		ret_val = sscanf(tmpent.d_name, "inode%lld", &tmp_ino);
#else
		ret_val = sscanf(tmpent.d_name, "inode%ld", &tmp_ino);
#endif

		if (ret_val > 0) {
			ret = fetch_inode_stat(tmp_ino, &tmpstat, NULL);
			if (ret < 0) {
				closedir(dirp);
				return ret;
			}
			if (S_ISREG(tmpstat.st_mode))
				ret = actual_delete_inode(tmp_ino, D_ISREG);
			if (S_ISDIR(tmpstat.st_mode))
				ret = actual_delete_inode(tmp_ino, D_ISDIR);
			if (S_ISLNK(tmpstat.st_mode))
				ret = actual_delete_inode(tmp_ino, D_ISLNK);

			if (ret < 0) {
				closedir(dirp);
				return ret;
			}
		}
		errcode = readdir_r(dirp, &tmpent, &tmpptr);
		if (errcode > 0) {
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			closedir(dirp);
			return -errcode;
		}
	}

	closedir(dirp);
	return 0;
}

/* Given parent "parent", search for "childname" in parent and return
the directory entry in structure pointed by "dentry" if found. If not or
if error, return the negation of error code. */
int lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry)
{
	META_CACHE_ENTRY_STRUCT *cache_entry;
	DIR_ENTRY_PAGE temp_page;
	int temp_index, ret_val;

	cache_entry = meta_cache_lock_entry(parent);
	if (cache_entry == NULL)
		return -ENOMEM;

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