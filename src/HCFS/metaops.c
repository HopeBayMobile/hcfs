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
*
**************************************************************************/
#include "metaops.h"

#include <sys/file.h>
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

extern SYSTEM_CONF_STRUCT system_config;

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
	int ret_items;
	int ret_val;
	long page_pos;
	int entry_index;
	int sem_val;
	char no_need_rewrite;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];

	sem_getvalue(&(body_ptr->access_sem), &sem_val);
	if (sem_val > 0) {
		/*Not locked, return -1*/
		return -1;
	}

	memset(&temp_entry, 0, sizeof(DIR_ENTRY));
	memset(&overflow_entry, 0, sizeof(DIR_ENTRY));

	temp_entry.d_ino = child_inode;
	strcpy(temp_entry.d_name, childname);
	if (S_ISREG(child_mode))
		temp_entry.d_type = D_ISREG;
	if (S_ISDIR(child_mode))
		temp_entry.d_type = D_ISDIR;

	/* Load parent meta from meta cache */
	ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
					&parent_meta, NULL, body_ptr);

	/* Initialize B-tree insertion by first loading the root of
	*  the B-tree. */
	tpage.this_page_pos = parent_meta.root_entry_page;

	ret_val = meta_cache_open_file(body_ptr);

	fseek(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);
	fread(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Drop all cached pages first before inserting */
	/* TODO: Future changes could remove this limitation if can update
	*  cache with each node change in b-tree*/
	ret_val = meta_cache_drop_pages(body_ptr);

	/* Recursive routine for B-tree insertion*/
	/* Temp space for traversing the tree is allocated before calling */
	ret_val = insert_dir_entry_btree(&temp_entry, &tpage,
			fileno(body_ptr->fptr), &overflow_entry,
			&overflow_new_page, &parent_meta, temp_dir_entries,
							temp_child_page_pos);

	/* An error occured and the routine will terminate now */
	/* TODO: Consider error recovering here */
	if (ret_val < 0)
		return ret_val;

	/* If return value is 1, we need to handle overflow by splitting
	*  the old root node in two and create a new root page to point
	*  to the two splitted nodes. Note that a new node has already been
	*  created in this case and pointed by "overflow_new_page". */
	if (ret_val == 1) {
		/* Reload old root */
		fseek(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);
		fread(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

		/* tpage contains the old root node now */

		/*Need to create a new root node and write to disk*/
		if (parent_meta.entry_page_gc_list != 0) {
			/*Reclaim node from gc list first*/
			fseek(body_ptr->fptr, parent_meta.entry_page_gc_list,
								SEEK_SET);
			fread(&new_root, sizeof(DIR_ENTRY_PAGE), 1,
								body_ptr->fptr);
			new_root.this_page_pos = parent_meta.entry_page_gc_list;
			parent_meta.entry_page_gc_list = new_root.gc_list_next;
		} else {
			/* If cannot reclaim, extend the meta file */
			memset(&new_root, 0, sizeof(DIR_ENTRY_PAGE));
			fseek(body_ptr->fptr, 0, SEEK_END);
			new_root.this_page_pos = ftell(body_ptr->fptr);
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
			fseek(body_ptr->fptr, parent_meta.tree_walk_list_head,
								SEEK_SET);
			fread(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
								body_ptr->fptr);
			tpage2.tree_walk_prev = new_root.this_page_pos;
			if (tpage2.this_page_pos == overflow_new_page) {
				tpage2.parent_page_pos = new_root.this_page_pos;
				no_need_rewrite = TRUE;
			}
			fseek(body_ptr->fptr, parent_meta.tree_walk_list_head,
								SEEK_SET);
			fwrite(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
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
		fseek(body_ptr->fptr, new_root.this_page_pos, SEEK_SET);
		fwrite(&new_root, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

		/* Change the parent of the old root to point to the new root.
		*  Write to the meta file afterward. */
		tpage.parent_page_pos = new_root.this_page_pos;
		fseek(body_ptr->fptr, tpage.this_page_pos, SEEK_SET);
		fwrite(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

		/* If no_need_rewrite is true, we have already write modified
		*  content for the new node from the overflow. Otherwise we need
		*  to write it to the meta file here. */
		if (no_need_rewrite == FALSE) {
			fseek(body_ptr->fptr, overflow_new_page, SEEK_SET);
			fread(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
								body_ptr->fptr);
			tpage2.parent_page_pos = new_root.this_page_pos;
			fseek(body_ptr->fptr, overflow_new_page, SEEK_SET);
			fwrite(&tpage2, sizeof(DIR_ENTRY_PAGE), 1,
								body_ptr->fptr);
		}

		/* Complete the splitting by updating the meta of the
		*  directory. */
		fseek(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
		fwrite(&parent_meta, sizeof(DIR_META_TYPE), 1, body_ptr->fptr);
	}

	/*If the new entry is a subdir, increase the hard link of the parent*/

	if (child_mode & S_IFDIR)
		parent_stat.st_nlink++;

	parent_meta.total_children++;
	printf("TOTAL CHILDREN is now %ld\n", parent_meta.total_children);

	/* Stat may be dirty after the operation so should write them back
	*  to cache*/
	ret_val = meta_cache_update_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);

	printf("debug dir_add_entry page_pos 2 %ld\n", page_pos);

	return ret_val;
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
	int ret_items;
	off_t nextfilepos, oldfilepos;
	long page_pos;
	int count, ret_val;
	int sem_val;
	DIR_ENTRY temp_entry;

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

	/* Initialize B-tree deletion by first loading the root of B-tree */
	ret_val = meta_cache_lookup_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);

	tpage.this_page_pos = parent_meta.root_entry_page;

	ret_val = meta_cache_open_file(body_ptr);

	fseek(body_ptr->fptr, parent_meta.root_entry_page, SEEK_SET);
	fread(&tpage, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Drop all cached pages first before deleting */
	/* TODO: Future changes could remove this limitation if can update cache
	*  with each node change in b-tree*/

	ret_val = meta_cache_drop_pages(body_ptr);

	/* Recursive B-tree deletion routine*/
	ret_val = delete_dir_entry_btree(&temp_entry, &tpage,
			fileno(body_ptr->fptr), &parent_meta, temp_dir_entries,
							temp_child_page_pos);

	printf("delete dir entry returns %d\n", ret_val);
	/* tpage might be invalid after calling delete_dir_entry_btree */

	if (ret_val == 0) {
		/* If the new entry is a subdir, decrease the hard link of
		*  the parent*/

		if (child_mode & S_IFDIR)
			parent_stat.st_nlink--;

		parent_meta.total_children--;
		printf("TOTAL CHILDREN is now %ld\n",
						parent_meta.total_children);
		ret_val = meta_cache_update_dir_data(parent_inode, &parent_stat,
						&parent_meta, NULL, body_ptr);
		return 0;
	}

	return -1;
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
	DIR_META_TYPE self_meta_head;
	DIR_ENTRY_PAGE tpage;
	int ret_items;
	int count;
	int ret_val;

	ret_val = meta_cache_seek_dir_entry(self_inode, &tpage, &count,
								"..", body_ptr);

	if ((ret_val == 0) && (count >= 0)) {
		/*Found the entry. Change parent inode*/
		tpage.dir_entries[count].d_ino = parent_inode2;
		ret_val = meta_cache_update_dir_data(self_inode, NULL, NULL,
							&tpage, body_ptr);
		return 0;
	}

	return -1;
}

/************************************************************************
*
* Function name: decrease_nlink_inode_file
*        Inputs: ino_t this_inode
*       Summary: For a regular file pointed by "this_inode", decrease its
*                reference count. If the count drops to zero, delete the
*                file as well.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int decrease_nlink_inode_file(ino_t this_inode)
{
	char todelete_metapath[METAPATHLEN];
	char thismetapath[METAPATHLEN];
	char thisblockpath[400];
	char filebuf[5000];
	struct stat this_inode_stat;
	FILE *todeletefptr, *metafptr;
	int ret_val;
	long long count;
	long long total_blocks;
	off_t cache_block_size;
	size_t read_size;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	body_ptr = meta_cache_lock_entry(this_inode);
	ret_val = meta_cache_lookup_dir_data(this_inode, &this_inode_stat,
							NULL, NULL, body_ptr);

	if (this_inode_stat.st_nlink <= 1) {
		meta_cache_close_file(body_ptr);
		ret_val = meta_cache_unlock_entry(body_ptr);

		/*Need to delete the meta. Move the meta file to "todelete"*/
		super_block_to_delete(this_inode);
		fetch_todelete_path(todelete_metapath, this_inode);
		fetch_meta_path(thismetapath, this_inode);
		/*Try a rename first*/
		ret_val = rename(thismetapath, todelete_metapath);
		if (ret_val < 0) {
			/*If not successful, copy the meta*/
			unlink(todelete_metapath);
			todeletefptr = fopen(todelete_metapath, "w");
			metafptr = fopen(thismetapath, "r");
			setbuf(metafptr, NULL);
			flock(fileno(metafptr), LOCK_EX);
			setbuf(todeletefptr, NULL);
			fseek(metafptr, 0, SEEK_SET);
			while (!feof(metafptr)) {
				read_size = fread(filebuf, 1, 4096, metafptr);
				if (read_size > 0)
					fwrite(filebuf, 1, read_size,
								todeletefptr);
				else
					break;
			}
			fclose(todeletefptr);

			unlink(thismetapath);
			flock(fileno(metafptr), LOCK_UN);
			fclose(metafptr);
		}

		/*Need to delete blocks as well*/
		/* TODO: Perhaps can move the actual block deletion to the
		*  deletion loop as well*/
		if (this_inode_stat.st_size == 0)
			total_blocks = 0;
		else
			total_blocks = ((this_inode_stat.st_size-1) /
							MAX_BLOCK_SIZE) + 1;

		for (count = 0; count < total_blocks; count++) {
			fetch_block_path(thisblockpath, this_inode, count);
			if (!access(thisblockpath, F_OK)) {
				cache_block_size =
						check_file_size(thisblockpath);
				unlink(thisblockpath);
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

		ret_val = meta_cache_remove(this_inode);
	} else {
		/* If it is still referenced, update the meta file. */
		this_inode_stat.st_nlink--;
		ret_val = meta_cache_update_dir_data(this_inode,
					&this_inode_stat, NULL, NULL, body_ptr);
		meta_cache_close_file(body_ptr);
		ret_val = meta_cache_unlock_entry(body_ptr);
	}

	return 0;
}

/************************************************************************
*
* Function name: seek_page
*        Inputs: FH_ENTRY *fh_ptr, long long target_page
*       Summary: Given file table entry pointed by "fh_ptr", find the block
*                entry page "target_page" and move the cached pos pointer
*                in "fh_ptr" to the beginning of this page. If entry pages
*                have not yet been created along the search, create them
*                as well.
*  Return value: 0 if successful. Otherwise returns -1.
*
*************************************************************************/
int seek_page(FH_ENTRY *fh_ptr, long long target_page)
{
	long long current_page;
	off_t nextfilepos, prevfilepos, currentfilepos;
	BLOCK_ENTRY_PAGE temppage;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	int sem_val;
	FILE_META_TYPE temp_meta;

	/* First check if meta cache is locked */

	body_ptr = fh_ptr->meta_cache_ptr;

	sem_getvalue(&(body_ptr->access_sem), &sem_val);

	/*If meta cache lock is not locked, return -1*/
	if (sem_val > 0)
		return -1;

	meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, &temp_meta,
							NULL, 0, body_ptr);

	nextfilepos = temp_meta.next_block_page;
	current_page = 0;
	prevfilepos = 0;

	meta_cache_open_file(body_ptr);

	/*TODO: put error handling for the read/write ops here*/
	while (current_page <= target_page) {
		if (nextfilepos == 0) {
			/*Need to append a new block entry page */
			if (prevfilepos == 0) {
				/* If not even the first page is generated */
				fseek(body_ptr->fptr, 0, SEEK_END);
				prevfilepos = ftell(body_ptr->fptr);
				temp_meta.next_block_page = prevfilepos;
				memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
				meta_cache_update_file_data(fh_ptr->thisinode,
						NULL, &temp_meta, &temppage,
						prevfilepos, body_ptr);
			} else {
				fseek(body_ptr->fptr, 0, SEEK_END);
				currentfilepos = ftell(body_ptr->fptr);
				meta_cache_lookup_file_data(fh_ptr->thisinode,
						NULL, NULL, &temppage,
							prevfilepos, body_ptr);
				temppage.next_page = currentfilepos;
				meta_cache_update_file_data(fh_ptr->thisinode,
						NULL, NULL, &temppage,
							prevfilepos, body_ptr);

				memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
				meta_cache_update_file_data(fh_ptr->thisinode,
						NULL, NULL, &temppage,
						currentfilepos, body_ptr);

				prevfilepos = currentfilepos;
			}
		} else {
			meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
					NULL, &temppage, nextfilepos, body_ptr);

			prevfilepos = nextfilepos;
			nextfilepos = temppage.next_page;
		}
		if (current_page == target_page)
			break;
		current_page++;
	}
	fh_ptr->cached_page_index = target_page;
	fh_ptr->cached_filepos = prevfilepos;

	return 0;
}

/************************************************************************
*
* Function name: advance_block
*        Inputs: META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,
*                long long *entry_index
*       Summary: Given the file meta cache entry "body_ptr", the file offset
*                of the current block status page in the meta file
*                "thisfilepos", and the index of a block in this status page
*                "*entry_index", return the next block index in "*entry_index"
*                and the file offset of the page the next block is in.
*  Return value: If successful, the file offset of the page that the next
*                block is in. Otherwise returns -1.
*
*************************************************************************/
long long advance_block(META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,
						long long *entry_index)
{
	long long temp_index;
	off_t nextfilepos;
	BLOCK_ENTRY_PAGE temppage;
	int ret_val;
	/*First handle the case that nothing needs to be changed,
						just add entry_index*/

	temp_index = *entry_index;
	if ((temp_index+1) < MAX_BLOCK_ENTRIES_PER_PAGE) {
		temp_index++;
		*entry_index = temp_index;
		return thisfilepos;
	}

	/*We need to change to another page*/

	ret_val = meta_cache_open_file(body_ptr);

	fseek(body_ptr->fptr, thisfilepos, SEEK_SET);
	fread(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
	nextfilepos = temppage.next_page;

	if (nextfilepos == 0) {  /*Need to allocate a new page*/
		fseek(body_ptr->fptr, 0, SEEK_END);
		nextfilepos = ftell(body_ptr->fptr);
		temppage.next_page = nextfilepos;
		fseek(body_ptr->fptr, thisfilepos, SEEK_SET);
		fwrite(&(temppage), sizeof(BLOCK_ENTRY_PAGE), 1,
							body_ptr->fptr);
		fseek(body_ptr->fptr, nextfilepos, SEEK_SET);
		memset(&temppage, 0, sizeof(BLOCK_ENTRY_PAGE));
		fwrite(&temppage, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
	}

	*entry_index = 0;
	return nextfilepos;
}
