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
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include "macro.h"
#include "fuseop.h"
#include "super_block.h"
#include "global.h"
#include "mount_manager.h"
#include "hcfscurl.h"
#include "metaops.h"
#include "dir_entry_btree.h"
#include "utils.h"

extern SYSTEM_CONF_STRUCT system_config;

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
	int errcode, fd, ret;
	DIR_META_TYPE tmp_head;
	ssize_t ret_ssize;
	struct timeval tmptime;

	fs_mgr_path = malloc(sizeof(char) * (strlen(METAPATH)+100));
	if (fs_mgr_path  == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}
	snprintf(fs_mgr_path, strlen(METAPATH)+100, "%s/fsmgr", METAPATH);

	fs_mgr_head = malloc(sizeof(FS_MANAGER_HEAD_TYPE));
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

		memset(&tmp_head, 0, sizeof(DIR_META_TYPE));

		gettimeofday(&tmptime, NULL);
		fd = open("/dev/urandom", O_RDONLY);

		if (fd < 0) {
			errcode = EIO;
			write_log(0, "Error opening random device\n");
			errcode = -errcode;
			goto errcode_handle;
		}

		/* Generate system UUID */
		ret = read(fd, fs_mgr_head->sys_uuid, 16);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "IO error reading random device. (%s)\n",
				strerror(errcode));
			errcode = -errcode;
			close(fd);
			goto errcode_handle;
		}
		close(fd);

		PWRITE(fs_mgr_head->FS_list_fh, fs_mgr_head->sys_uuid,
					16, 0);

		PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
					sizeof(DIR_META_TYPE), 16);

		ret = backup_FS_database();
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	} else {
		fs_mgr_head->FS_list_fh = open(fs_mgr_path, O_RDWR);

		if (fs_mgr_head->FS_list_fh < 0) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}

		PREAD(fs_mgr_head->FS_list_fh, fs_mgr_head->sys_uuid,
					16, 0);

		PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
					sizeof(DIR_META_TYPE), 16);
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
	fs_mgr_head = NULL;
	fs_mgr_path = NULL;
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
	char metapath[METAPATHLEN];
	int ret, errcode;
	size_t ret_size;
	long ret_pos;
	unsigned long this_gen;

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

	root_inode = super_block_new_inode(&this_stat, &this_gen);

	if (root_inode <= 1) {
		write_log(0, "Error creating new root inode\n");
		return 0;
	}
	ret = fetch_meta_path(metapath, root_inode);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	this_stat.st_ino = root_inode;

	metafptr = fopen(metapath, "w");
	if (metafptr == NULL) {
		write_log(0, "IO error in initializing filesystem\n");
		errcode = -EIO;
		goto errcode_handle;
	}

	FWRITE(&this_stat, sizeof(struct stat), 1, metafptr);
	FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1, metafptr);

	FTELL(metafptr);
	this_meta.generation = this_gen;
	this_meta.root_entry_page = ret_pos;
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	this_meta.root_inode = root_inode;
	FSEEK(metafptr, sizeof(struct stat), SEEK_SET);

	FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1, metafptr);

	ret = init_dir_page(&temppage, root_inode, root_inode,
				this_meta.root_entry_page);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	FWRITE(&temppage, sizeof(DIR_ENTRY_PAGE), 1, metafptr);
	ret = update_FS_statistics(metapath, 0, 1);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	fclose(metafptr);
	metafptr = NULL;
	ret = super_block_mark_dirty(root_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
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
*       Summary: Creates the filesystem "fsname" if it does not exist.
*                The root inode for the new FS is returned in ret_entry.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	DIR_ENTRY_PAGE tpage, new_root, tpage2;
	DIR_ENTRY temp_entry, overflow_entry;
	DIR_META_TYPE tmp_head;
	long long overflow_new_page;
	int ret, errcode, ret_index;
	char no_need_rewrite;
	off_t ret_pos;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE+3)];
	ino_t new_FS_ino;
	ssize_t ret_ssize;

	sem_wait(&(fs_mgr_head->op_lock));

	if (strlen(fsname) > MAX_FILENAME_LEN) {
		errcode = ENAMETOOLONG;
		write_log(2, "Name of new filesystem (%s) is too long\n",
			fsname);
		errcode = -errcode;
		goto errcode_handle;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 16);

	if (fs_mgr_head->num_FS <= 0) {
		ftruncate(fs_mgr_head->FS_list_fh, sizeof(DIR_META_TYPE));
		tmp_head.total_children = 0;
		fs_mgr_head->num_FS = 0;
		tmp_head.root_entry_page = sizeof(DIR_META_TYPE) + 16;
		tmp_head.next_xattr_page = 0;
		tmp_head.entry_page_gc_list = 0;
		tmp_head.tree_walk_list_head = sizeof(DIR_META_TYPE) + 16;
		tmp_head.generation = 0;
		memset(&tpage, 0, sizeof(DIR_ENTRY_PAGE));
		tpage.this_page_pos = sizeof(DIR_META_TYPE) + 16;
		PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
			sizeof(DIR_META_TYPE), 16);
		PWRITE(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
			tpage.this_page_pos);
	} else {
		/* Initialize B-tree insertion by first loading the root of
		*  the B-tree. */
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
			tmp_head.root_entry_page);
	}
	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
		&ret_index, &tpage2);

	if (ret >= 0) {
		errcode = -EEXIST;
		write_log(2, "Filesystem already exists\n");
		goto errcode_handle;
	}

	if (ret != -ENOENT) {
		write_log(0, "Unexpected error in checking FS manager\n");
		errcode = ret;
		goto errcode_handle;
	}

	memset(&temp_entry, 0, sizeof(DIR_ENTRY));
	memset(&overflow_entry, 0, sizeof(DIR_ENTRY));

	new_FS_ino = _create_root_inode();

	if (new_FS_ino == 0) {
		write_log(0, "Error in creating root inode of filesystem\n");
		errcode = -EIO;
		goto errcode_handle;
	}

	temp_entry.d_ino = new_FS_ino;
	snprintf(temp_entry.d_name, MAX_FILENAME_LEN+1, "%s", fsname);


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
		if (tmp_head.entry_page_gc_list != 0) {
			/*Reclaim node from gc list first*/
			PREAD(fs_mgr_head->FS_list_fh, &new_root,
				sizeof(DIR_ENTRY_PAGE),
				tmp_head.entry_page_gc_list);

			new_root.this_page_pos = tmp_head.entry_page_gc_list;
			tmp_head.entry_page_gc_list = new_root.gc_list_next;
		} else {
			/* If cannot reclaim, extend the meta file */
			memset(&new_root, 0, sizeof(DIR_ENTRY_PAGE));
			LSEEK(fs_mgr_head->FS_list_fh, 0, SEEK_END);

			new_root.this_page_pos = ret_pos;
			if (new_root.this_page_pos == -1) {
				errcode = errno;
				write_log(0, "IO error in adding dir entry\n");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
		}

		/* Insert the new root to the head of tree_walk_list. This list
		*  is for listing nodes in the B-tree in readdir operation. */
		new_root.gc_list_next = 0;
		new_root.tree_walk_next = tmp_head.tree_walk_list_head;
		new_root.tree_walk_prev = 0;

		no_need_rewrite = FALSE;
		if (tmp_head.tree_walk_list_head == tpage.this_page_pos) {
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


		tmp_head.tree_walk_list_head = new_root.this_page_pos;

		/* Initialize the new root node */
		new_root.parent_page_pos = 0;
		memset(new_root.child_page_pos, 0,
				sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
		new_root.num_entries = 1;
		memcpy(&(new_root.dir_entries[0]), &overflow_entry,
							sizeof(DIR_ENTRY));
		/* The two children of the new root is the old root and
		*  the new node created by the overflow. */
		new_root.child_page_pos[0] = tmp_head.root_entry_page;
		new_root.child_page_pos[1] = overflow_new_page;

		/* Set the root of B-tree to the new root, and write the
		*  content of the new root the the meta file. */
		tmp_head.root_entry_page = new_root.this_page_pos;
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

	PWRITE(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE), 16);

	write_log(10,
		"Total filesystem is now %lld\n", tmp_head.total_children);

	memcpy(ret_entry, &temp_entry, sizeof(DIR_ENTRY));

	/* TODO: return handling for backup database */
	backup_FS_database();
	sem_post(&(fs_mgr_head->op_lock));

	return 0;

errcode_handle:
	sem_post(&(fs_mgr_head->op_lock));

	return errcode;
}

/************************************************************************
*
* Function name: delete_filesystem
*        Inputs: char *fsname
*       Summary: Delete the filesystem "fsname" if it is not mounted and
*                is empty.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int delete_filesystem(char *fsname)
{
	/* TODO: temp file for atomic operation (see detailed design) */

	DIR_ENTRY_PAGE tpage, tpage2;
	DIR_ENTRY temp_entry;
	DIR_META_TYPE tmp_head, roothead;
	int ret, errcode, ret_index;
	ino_t FS_root;
	ssize_t ret_ssize;
	size_t ret_size;
	char thismetapath[400];
	FILE *metafptr;
	DIR_ENTRY temp_dir_entries[2*(MAX_DIR_ENTRIES_PER_PAGE+2)];
	long long temp_child_page_pos[2*(MAX_DIR_ENTRIES_PER_PAGE+3)];

	sem_wait(&(fs_mgr_head->op_lock));

	metafptr = NULL;

	if (strlen(fsname) > MAX_FILENAME_LEN) {
		errcode = ENAMETOOLONG;
		write_log(2, "Name of filesystem to delete (%s) is too long\n",
			fsname);
		errcode = -errcode;
		goto errcode_handle;
	}

	if (fs_mgr_head->num_FS <= 0) {
		errcode = -ENOENT;
		write_log(2, "No filesystem exists\n");
		goto errcode_handle;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 16);

	/* Initialize B-tree deletion by first loading the root of
	*  the B-tree. */
	PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		tmp_head.root_entry_page);

	/* Check if the FS name exists. If not, return error */

	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
		&ret_index, &tpage2);

	if (ret < 0) {
		if (ret == -ENOENT)
			write_log(2, "Filesystem does not exist\n");
		else
			write_log(0, "Error in deleting FS. Code %d, %s\n",
				-ret, strerror(-ret));
		errcode = ret;
		goto errcode_handle;
	}

	/* Check if filesystem is mounted. If so, returns error */
	sem_wait(&(mount_mgr.mount_lock));
	ret = FS_is_mounted(fsname);
	sem_post(&(mount_mgr.mount_lock));

	if ((ret < 0) && (ret != -ENOENT)) {
		errcode = ret;
		goto errcode_handle;
	}

	if (ret == 0) {
		errcode = -EPERM;
		write_log(2, "Cannot delete mounted filesystem\n");
		goto errcode_handle;
	}

	/* Check if the filesystem is empty. If not, returns error */
	memcpy(&temp_entry, &(tpage2.dir_entries[ret_index]),
		sizeof(DIR_ENTRY));
	FS_root = temp_entry.d_ino;

	ret = fetch_meta_path(thismetapath, FS_root);
	metafptr = NULL;
	metafptr = fopen(thismetapath, "r");
	if (metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO Error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	FSEEK(metafptr, sizeof(struct stat), SEEK_SET);
	FREAD(&roothead, sizeof(DIR_META_TYPE), 1, metafptr);
	fclose(metafptr);
	metafptr = NULL;
	if (roothead.total_children > 0) {
		/* FS not empty */
		errcode = -ENOTEMPTY;
		write_log(2, "Filesystem is not empty.\n");
		goto errcode_handle;
	}

	/* Delete root inode (follow ll_rmdir) */

	ret = delete_inode_meta(FS_root);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Delete FS from database */
	ret = delete_dir_entry_btree(&temp_entry, &tpage,
			fs_mgr_head->FS_list_fh, &tmp_head, temp_dir_entries,
							temp_child_page_pos);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	tmp_head.total_children--;
	write_log(10, "TOTAL CHILDREN is now %lld\n",
					tmp_head.total_children);
	/* Write head back */

	PWRITE(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE), 16);

	fs_mgr_head->num_FS--;
	sem_post(&(fs_mgr_head->op_lock));

	return 0;
errcode_handle:
	if (metafptr != NULL)
		fclose(metafptr);
	sem_post(&(fs_mgr_head->op_lock));

	return errcode;
}

/************************************************************************
*
* Function name: check_filesystem
*        Inputs: char *fsname, DIR_ENTRY *ret_entry
*       Summary: This function checks whether the filesystem "fsname" exists.
*                If so, the entry is returned via "ret_entry".
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int check_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	int ret;
	/* This is the wrapper around check_filesytem_core, and
	also locks FS manager lock */
	sem_wait(&(fs_mgr_head->op_lock));

	ret = check_filesystem_core(fsname, ret_entry);
	sem_post(&(fs_mgr_head->op_lock));
	return ret;
}
int check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry)
{
	DIR_ENTRY_PAGE tpage, tpage2;
	DIR_META_TYPE tmp_head;
	int ret, errcode, ret_index;
	ssize_t ret_ssize;

	if (strlen(fsname) > MAX_FILENAME_LEN) {
		errcode = ENAMETOOLONG;
		write_log(2, "Name of filesystem to delete (%s) is too long\n",
			fsname);
		errcode = -errcode;
		goto errcode_handle;
	}

	if (fs_mgr_head->num_FS <= 0) {
		errcode = -ENOENT;
		write_log(2, "No filesystem exists\n");
		goto errcode_handle;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 16);

	/* Initialize B-tree searching by first loading the root of
	*  the B-tree. */
	PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		tmp_head.root_entry_page);

	/* Check if the FS name exists. If not, return error */

	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
		&ret_index, &tpage2);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	memcpy(ret_entry, &(tpage2.dir_entries[ret_index]),
		sizeof(DIR_ENTRY));

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: list_filesystem
*        Inputs: int buf_num, DIR_ENTRY *ret_entry, int *ret_num
*       Summary: List all filesystems in the buf "ret_entry" provided
*                by the caller. "buf_num" indicates the number of entries.
*                "ret_num" returns the total number of filesystems. If
*                "buf_num" is smaller than "*ret_num", nothing is filled
*                and the total number of filesystems is filled in "ret_num".
*                Note that the return value of the function will be zero in
*                this case. Caller can pass NULL for "ret_entry" and zero
*                for "buf_num" to query the number of entries needed.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int list_filesystem(unsigned long buf_num, DIR_ENTRY *ret_entry,
		unsigned long *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int errcode;
	ssize_t ret_ssize;
	unsigned long num_walked;
	long long next_node_pos;
	int count;

	sem_wait(&(fs_mgr_head->op_lock));

	if (fs_mgr_head->num_FS > buf_num) {
		*ret_num = fs_mgr_head->num_FS;
		sem_post(&(fs_mgr_head->op_lock));
		return 0;
	}

	/* If no filesystem */
	if (fs_mgr_head->num_FS <= 0) {
		*ret_num = 0;
		sem_post(&(fs_mgr_head->op_lock));
		return 0;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 16);


	/* Initialize B-tree walk by first loading the first node
		of the tree walk. */
	next_node_pos = tmp_head.tree_walk_list_head;

	num_walked = 0;

	while (next_node_pos != 0) {
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
				next_node_pos);
		if ((num_walked + tpage.num_entries) > buf_num) {
			/* Only compute the number of FS */
			num_walked += tpage.num_entries;
			next_node_pos = tpage.tree_walk_next;
			continue;
		}
		for (count = 0; count < tpage.num_entries; count++) {
			memcpy(&(ret_entry[num_walked]),
				&(tpage.dir_entries[count]), sizeof(DIR_ENTRY));
			num_walked++;
		}
		next_node_pos = tpage.tree_walk_next;
	}

	if ((fs_mgr_head->num_FS != num_walked) ||
		(tmp_head.total_children != num_walked)) {
		/* Number of FS is wrong. */
		write_log(0, "Error in FS num. Recomputing\n");
		fs_mgr_head->num_FS = num_walked;
		if (tmp_head.total_children != num_walked) {
			tmp_head.total_children = num_walked;
			write_log(0, "Rewriting FS num in database\n");
			PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
				sizeof(DIR_META_TYPE), 16);
		}
	}

	*ret_num = num_walked;
	sem_post(&(fs_mgr_head->op_lock));

	return 0;
errcode_handle:
	sem_post(&(fs_mgr_head->op_lock));

	return errcode;
}

/************************************************************************
*
* Function name: backup_FS_database
*        Inputs: None
*       Summary: Upload FS backup to backend.
*          Note: Assume that the op_lock is locked when calling
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int backup_FS_database(void)
{
	FILE *fptr;
	int ret, errcode;
	CURL_HANDLE upload_handle;
	char buf[4096];
	size_t ret_size;
	ssize_t ret_ssize;
	off_t curpos;

	memset(&upload_handle, 0, sizeof(CURL_HANDLE));

	/* Assume that the access lock is locked already */
	fptr = NULL;
	fptr = fopen("/tmp/FSmgr_upload", "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	curpos = 0;
	ret_ssize = 0;
	while (!feof(fptr)) {
		PREAD(fs_mgr_head->FS_list_fh, buf, 4096, curpos);
		if (ret_ssize <= 0)
			break;
		curpos += ret_ssize;
		FWRITE(buf, 1, ret_ssize, fptr);
	}

	fclose(fptr);

	fptr = NULL;
	snprintf(upload_handle.id, 256, "FSmgr_upload");
	ret = hcfs_init_backend(&upload_handle);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in backing up FS database\n");
		goto errcode_handle;
	}

	fptr = fopen("/tmp/FSmgr_upload", "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	ret = hcfs_put_object(fptr, "FSmgr_backup", &upload_handle);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in backing up FS database\n");
		goto errcode_handle;
	}

	fclose(fptr);

	unlink("/tmp/FSmgr_upload");
	hcfs_destroy_backend(upload_handle.curl);
	return 0;

errcode_handle:
	if (upload_handle.curl != NULL)
		hcfs_destroy_backend(upload_handle.curl);

	if (fptr != NULL)
		fclose(fptr);

	unlink("/tmp/FSmgr_upload");
	return errcode;
}

/************************************************************************
*
* Function name: restore_FS_database
*        Inputs: None
*       Summary: Download FS backup from backend.
*          Note: Assume that FS manager is not started when calling this,
*                but path to the filesystem database should be constructed.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int restore_FS_database(void)
{
	FILE *fptr;
	int ret, errcode;
	CURL_HANDLE download_handle;

	/* Assume that FS manager is not started */

	fptr = NULL;
	memset(&download_handle, 0, sizeof(CURL_HANDLE));

	snprintf(download_handle.id, 256, "FSmgr_download");

	ret = hcfs_init_backend(&download_handle);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in restoring FS database\n");
		goto errcode_handle;
	}

	fptr = fopen(fs_mgr_path, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	ret = hcfs_get_object(fptr, "FSmgr_backup", &download_handle);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in restoring FS database\n");
		goto errcode_handle;
	}

	fclose(fptr);

	hcfs_destroy_backend(download_handle.curl);
	return 0;

errcode_handle:
	if (download_handle.curl != NULL)
		hcfs_destroy_backend(download_handle.curl);

	if (fptr != NULL)
		fclose(fptr);

	return errcode;
}