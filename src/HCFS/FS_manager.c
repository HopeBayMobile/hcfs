/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.c
* Abstract: The c source file for filesystem manager
*
* Revision History
* 2015/7/1 Jiahong created this file
*
**************************************************************************/

#include "FS_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include "macro.h"
#include "fuseop.h"
#include "super_block.h"

/************************************************************************
*
* Function name: init_fs_manager
*        Inputs: None
*       Summary: Initialize the header for FS manager, creating the FS
*                database if it does not exist.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_fs_manager(void)
{
	int ret, errcode;
	DIR_ENTRY_PAGE tmp_page;
	DIR_META_TYPE tmp_head;

	fs_mgr_path = malloc(sizeof(char) * (strlen(METAPATH)+100));
	if (fs_mgr_path  == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}
	snprintf(fs_mgr_path, strlen(METAPATH)+100, "%s/fsmgr", METAPATH);

	fs_mgr_head = malloc(sizeof(FS_MANAGER_HEAD_TYPE);
	if (fs_mgr_head == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}

	sem_init(&(fs_mgr_head->op_lock), 0, 1);

	errcode = 0;
	if (access(fs_mgr_path, F_OK) != 0) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Unexpected error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		write_log(2, "Cannot find FS manager. Creating one\n");

		/* Initialize header for FS manager on disk and in memory */
		errcode = 0;
		fs_mgr_head->FS_list_fh = open(fs_mgr_path, O_RDWR | O_CREAT,
						S_IRUSR | S_IWUSR);
		if (fs_mgr_head->FS_list_fh < 0) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		fs_mgr_head->num_FS = 0;

		memset(&tmp_head, 0, sizeof(DIR_META_TYPE);

		PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
					sizeof(DIR_META_TYPE), 0);

	} else {
		fs_mgr_head->FS_list_fh = open(fs_mgr_path, O_RDWR);

		if (fs_mgr_head->FS_list_fh < 0) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}

		PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
					sizeof(DIR_META_TYPE), 0);
		fs_mgr_head->num_FS = tmp_head.total_children;
	}

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: destroy_fs_manager
*        Inputs: None
*       Summary: Destroys the header for FS manager.
*  Return value: None
*
*************************************************************************/
void destroy_fs_manager(void)
{
	sem_wait(&(fs_mgr_head->op_lock));
	close(fs_mgr_head->FS_list_fh);
	sem_post(&(fs_mgr_head->op_lock));
	sem_destroy(&(fs_mgr_head->op_lock));
	free(fs_mgr_head);
	free(fs_mgr_path);
}

/* Helper function for allocating a new inode as root */
ino_t _create_root_inode(void)
{
	ino_t root_inode;
	struct stat this_stat;
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	mode_t self_mode;
	FILE *metafptr;

	memset(&this_stat, 0, sizeof(struct stat));
	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	self_mode = S_IFDIR | 0777;
	this_stat.st_mode = self_mode;

	/*One pointed by the parent, another by self*/
	this_stat.st_nlink = 2;
	this_stat.st_uid = getuid();
	this_stat.st_gid = getgid();

	set_timestamp_now(&this_stat, ATIME | MTIME | CTIME);

	root_inode = super_block_new_inode(&this_stat, NULL);

	if (root_inode <= 1) {
		write_log(0, "Error creating new root inode\n");
		return 0;
	}

	this_stat.st_ino = root_inode;

		metafptr = fopen(rootmetapath, "w");
		if (metafptr == NULL) {
			write_log(0, "IO error in initializing system\n");
			return -EIO;
		}

		FWRITE(&this_stat, sizeof(struct stat), 1, metafptr);


		FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		FTELL(metafptr);
		this_meta.root_entry_page = ret_pos;
		this_meta.tree_walk_list_head = this_meta.root_entry_page;
		FSEEK(metafptr, sizeof(struct stat), SEEK_SET);

		FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		ret = init_dir_page(&temppage, 1, 0,
					this_meta.root_entry_page);
		if (ret < 0) {
			fclose(metafptr);
			return ret;
		}

		FWRITE(&temppage, sizeof(DIR_ENTRY_PAGE), 1,
								metafptr);
		fclose(metafptr);
		ret = super_block_mark_dirty(1);
		if (ret < 0)
			return ret;
	}
	return root_inode;

errcode_handle:
	if (metafptr != NULL)
		fclose(metafptr);
	return 0;
}
/************************************************************************
*
* Function name: add_filesystem
*        Inputs: char *fsname, DIR_ENTRY *ret_entry
*       Summary: Destroys the header for FS manager.
*  Return value: None
*
*************************************************************************/
int add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	DIR_ENTRY_PAGE tpage, new_root, tpage2;
	DIR_ENTRY temp_entry, overflow_entry;
	DIR_META_TYPE tmp_head;
	long long overflow_new_page;
	int ret, errcode;
	size_t ret_size;
	char no_need_rewrite;
	off_t ret_pos;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];
	ino_t new_FS_ino;

	if (strlen(fsname) > MAX_FILENAME_LEN) {
		errcode = ENAMETOOLONG;
		write_log(0, "Name of new filesystem (%s) is too long\n",
			fsname);
		errcode = -errcode;
		goto errcode_handle;
	}

	sem_wait(&(fs_mgr_head->op_lock));

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 0);

	memset(&temp_entry, 0, sizeof(DIR_ENTRY));
	memset(&overflow_entry, 0, sizeof(DIR_ENTRY));

	/* TODO: ask for a new inode number from super inode */

	new_FS_ino = _create_root_inode();

	if (new_FS_ino == 0) {
		/*TODO: Error handling */

	temp_entry.d_ino = new_FS_ino;
	snprintf(temp_entry.d_name, MAX_FILENAME_LEN+1, "%s", fsname);

	/* TODO: Check if there is no FS yet. If there is none, init
		B-tree (truncate b-tree and write new root node) */

	/* Initialize B-tree insertion by first loading the root of
	*  the B-tree. */
	PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
			tmp_head.root_entry_page);
	
	/* Recursive routine for B-tree insertion*/
	/* Temp space for traversing the tree is allocated before calling */
	ret = insert_dir_entry_btree(&temp_entry, &tpage,
			fs_mgr_head->FS_list_fh, &overflow_entry,
			&overflow_new_page, &tmp_head, temp_dir_entries,
							temp_child_page_pos);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* If return value is 1, we need to handle overflow by splitting
	*  the old root node in two and create a new root page to point
	*  to the two splitted nodes. Note that a new node has already been
	*  created in this case and pointed by "overflow_new_page". */
	if (ret == 1) {
		/* Reload old root */
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
				tmp_head.root_entry_page);

		/* tpage contains the old root node now */

		/*Need to create a new root node and write to disk*/
		if (parent_meta.entry_page_gc_list != 0) {
			/*Reclaim node from gc list first*/
			PREAD(fs_mgr_head->FS_list_fh, &new_root,
				sizeof(DIR_ENTRY_PAGE),
				tmp_head.entry_page_gc_list);

			new_root.this_page_pos = parent_meta.entry_page_gc_list;
			parent_meta.entry_page_gc_list = new_root.gc_list_next;
		} else {
			/* If cannot reclaim, extend the meta file */
			memset(&new_root, 0, sizeof(DIR_ENTRY_PAGE));
			LSEEK(fs_mgr_head->FS_list_fh, 0, SEEK_END);

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
			PREAD(fs_mgr_head->FS_list_fh, &tpage2,
				sizeof(DIR_ENTRY_PAGE),
				tmp_head.tree_walk_list_head);

			tpage2.tree_walk_prev = new_root.this_page_pos;
			if (tpage2.this_page_pos == overflow_new_page) {
				tpage2.parent_page_pos = new_root.this_page_pos;
				no_need_rewrite = TRUE;
			}
			PWRITE(fs_mgr_head->FS_list_fh, &tpage2,
				sizeof(DIR_ENTRY_PAGE),
				tmp_head.tree_walk_list_head);
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
		PWRITE(fs_mgr_head->FS_list_fh, &new_root,
			sizeof(DIR_ENTRY_PAGE), new_root.this_page_pos);

		/* Change the parent of the old root to point to the new root.
		*  Write to the meta file afterward. */
		tpage.parent_page_pos = new_root.this_page_pos;
		PWRITE(fs_mgr_head->FS_list_fh, &tpage,
			sizeof(DIR_ENTRY_PAGE), tpage.this_page_pos);

		/* If no_need_rewrite is true, we have already write modified
		*  content for the new node from the overflow. Otherwise we need
		*  to write it to the meta file here. */
		if (no_need_rewrite == FALSE) {
			PREAD(fs_mgr_head->FS_list_fh, &tpage2,
				sizeof(DIR_ENTRY_PAGE), overflow_new_page);

			tpage2.parent_page_pos = new_root.this_page_pos;
			PWRITE(fs_mgr_head->FS_list_fh, &tpage2,
				sizeof(DIR_ENTRY_PAGE), overflow_new_page);
		}
	}

	tmp_head.total_children++;
	fs_mgr_head->num_FS++;

	/* Write head back */

	PWRITE(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE), 0);

	write_log(10,
		"Total filesystem is now %lld\n", tmp_head.total_children);

	ret_entry->

	backup_FS_database();
	return ret;

errcode_handle:
	return errcode;
}
