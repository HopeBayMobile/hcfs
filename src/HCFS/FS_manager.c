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

#include "FS_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#include "macro.h"
#include "fuseop.h"
#include "super_block.h"
#include "global.h"
#include "mount_manager.h"
#include "hcfscurl.h"
#include "metaops.h"
#include "dir_entry_btree.h"
#include "utils.h"
#include "meta.h"
#include "path_reconstruct.h"
#include "params.h"
#include "rebuild_super_block.h"

int64_t meta_pos = offsetof(FS_MANAGER_HEADER_LAYOUT, fs_dir_meta);
int64_t clouddata_pos = offsetof(FS_MANAGER_HEADER_LAYOUT, fs_clouddata);

/************************************************************************
*
* Function name: init_fs_manager
*        Inputs: None
*       Summary: Initialize the header for FS manager, creating the FS
*                database if it does not exist.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t init_fs_manager(void)
{
	int32_t errcode, fd, ret;
	DIR_META_TYPE tmp_head;
	CLOUD_RELATED_DATA tmp_clouddata;
	struct timeval tmptime;
	FS_MANAGER_HEADER_LAYOUT fs_mgr_header;

	fs_mgr_path = malloc(sizeof(char) * (strlen(METAPATH) + 100));
	if (fs_mgr_path == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}
	snprintf(fs_mgr_path, strlen(METAPATH) + 100, "%s/fsmgr", METAPATH);

	fs_mgr_head = malloc(sizeof(FS_MANAGER_HEAD_TYPE));
	if (fs_mgr_head == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory in %s.\n", __func__);
		goto errcode_handle;
	}

	sem_init(&(fs_mgr_head->op_lock), 0, 1);

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
		fs_mgr_head->FS_list_fh =
		    open(fs_mgr_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fs_mgr_head->FS_list_fh < 0) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				  errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		fs_mgr_head->num_FS = 0;

		memset(&tmp_head, 0, sizeof(DIR_META_TYPE));
		memset(&tmp_clouddata, 0, sizeof(CLOUD_RELATED_DATA));

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

		memcpy(fs_mgr_header.sys_uuid, fs_mgr_head->sys_uuid,
		       sizeof(uint8_t) * 16);
		memcpy(&(fs_mgr_header.fs_dir_meta), &tmp_head,
		       sizeof(DIR_META_TYPE));
		memcpy(&(fs_mgr_header.fs_clouddata), &tmp_clouddata,
		       sizeof(CLOUD_RELATED_DATA));

		PWRITE(fs_mgr_head->FS_list_fh, &fs_mgr_header,
		       sizeof(FS_MANAGER_HEADER_LAYOUT), 0);

		ret = prepare_FS_database_backup();
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	} else {
		fs_mgr_head->FS_list_fh = open(fs_mgr_path, O_RDWR);

		if (fs_mgr_head->FS_list_fh < 0) {
			errcode = errno;
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				  errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}

		PREAD(fs_mgr_head->FS_list_fh, &fs_mgr_header,
		       sizeof(FS_MANAGER_HEADER_LAYOUT), 0);
		memcpy(fs_mgr_head->sys_uuid, fs_mgr_header.sys_uuid,
		       sizeof(uint8_t) * 16);
		memcpy(&tmp_head, &(fs_mgr_header.fs_dir_meta),
		       sizeof(DIR_META_TYPE));

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

/* Helper function for initializing a file for collecting statistics for
data stored at backend */
int32_t _init_backend_stat(ino_t root_inode)
{
	int32_t errcode;
	char fname[METAPATHLEN];
	FILE *fptr;
	BOOL is_fopen = FALSE;
	FS_CLOUD_STAT_T cloud_fs_stat;

	write_log(10, "Debug creating backend stat file\n");

	snprintf(fname, METAPATHLEN - 1, "%s/FS_sync/FSstat%" PRIu64,
		 METAPATH, (uint64_t)root_inode);
	write_log(10, "Creating stat for backend\n");
	fptr = fopen(fname, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "Open error in %s. Code %d, %s\n", __func__,
			  errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	is_fopen = TRUE;
	memset(&cloud_fs_stat, 0, sizeof(FS_CLOUD_STAT_T));
	FSEEK(fptr, 0, SEEK_SET);
	FWRITE(&cloud_fs_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);	
	fclose(fptr);

	return 0;
errcode_handle:
	if (is_fopen)
		fclose(fptr);
	return errcode;
}

/* Helper function for allocating a new inode as root */
#ifdef _ANDROID_ENV_
ino_t _create_root_inode(char voltype)
#else
ino_t _create_root_inode()
#endif
{
	ino_t root_inode;
	HCFS_STAT this_stat;
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	mode_t self_mode;
	FILE *metafptr = NULL, *statfptr = NULL;
	char metapath[METAPATHLEN];
	char temppath[METAPATHLEN];
	int32_t ret, errcode;
	int64_t ret_pos;
	uint64_t this_gen;
	FS_STAT_T tmp_stat;
	CLOUD_RELATED_DATA cloud_related_data;
	PIN_t pin;

	statfptr = NULL;

	init_hcfs_stat(&this_stat);
	memset(&this_meta, 0, sizeof(DIR_META_TYPE));
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

#ifdef _ANDROID_ENV_
	self_mode = S_IFDIR | 0770;
	if (voltype == ANDROID_INTERNAL) /* Default pin internal storage */
		pin = P_PIN;
	else
		pin = DEFAULT_PIN;
#else
	self_mode = S_IFDIR | 0775;
	pin = DEFAULT_PIN;
#endif
	this_stat.mode = self_mode;

	/*One pointed by the parent, another by self*/
	this_stat.nlink = 2;
	this_stat.uid = getuid();
	this_stat.gid = getgid();

	set_timestamp_now(&this_stat, A_TIME | M_TIME | C_TIME);

	root_inode = super_block_new_inode(&this_stat, &this_gen, pin);

	if (root_inode <= 1) {
		write_log(0, "Error creating new root inode\n");
		return 0;
	}
	ret = fetch_meta_path(metapath, root_inode);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	this_stat.ino = root_inode;

	metafptr = fopen(metapath, "w");
	if (metafptr == NULL) {
		write_log(0, "IO error in initializing filesystem\n");
		errcode = -EIO;
		goto errcode_handle;
	}

	memset(&cloud_related_data, 0, sizeof(CLOUD_RELATED_DATA));
	FWRITE(&this_stat, sizeof(HCFS_STAT), 1, metafptr);
	FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1, metafptr);
	FWRITE(&cloud_related_data, sizeof(CLOUD_RELATED_DATA), 1, metafptr);

	ret_pos = FTELL(metafptr);
	this_meta.generation = this_gen;
	this_meta.root_entry_page = ret_pos;
	this_meta.tree_walk_list_head = this_meta.root_entry_page;
	this_meta.root_inode = root_inode;
	this_meta.local_pin = pin;
	FSEEK(metafptr, sizeof(HCFS_STAT), SEEK_SET);

	/* Init parent lookup. Root inode does not have a parent */
	ret = pathlookup_write_parent(root_inode, 0);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}
	/* Init the dir stat for this node */
	ret = reset_dirstat_lookup(root_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1, metafptr);

	ret = init_dir_page(&temppage, root_inode, root_inode,
			    this_meta.root_entry_page);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	FSEEK(metafptr, this_meta.root_entry_page, SEEK_SET);
	FWRITE(&temppage, sizeof(DIR_ENTRY_PAGE), 1, metafptr);

	ret = fetch_stat_path(temppath, root_inode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	statfptr = fopen(temppath, "w");
	if (statfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error %d (%s)\n", errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	memset(&tmp_stat, 0, sizeof(FS_STAT_T));
	tmp_stat.num_inodes = 1;

	FWRITE(&tmp_stat, sizeof(FS_STAT_T), 1, statfptr);
	FSYNC(fileno(statfptr));

	fclose(statfptr);
	statfptr = NULL;

	/* Create stat file for backend statistics */
	ret = _init_backend_stat(root_inode);
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
	if (statfptr != NULL)
		fclose(statfptr);
	UNUSED(errcode);
	return 0;
}

/************************************************************************
*
* Function name: add_filesystem
*        Inputs: char *fsname, DIR_ENTRY *ret_entry
*        Inputs: (Android) char *fsname, char voltype, DIR_ENTRY *ret_entry
*       Summary: Creates the filesystem "fsname" if it does not exist.
*                The root inode for the new FS is returned in ret_entry.
*                For Android system, will also need to specify if the type
*                of volume is internal or external storage
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
#ifdef _ANDROID_ENV_
int32_t add_filesystem(char *fsname, char voltype, DIR_ENTRY *ret_entry)
#else
int32_t add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
#endif
{
	DIR_ENTRY_PAGE tpage, new_root, tpage2;
	DIR_ENTRY temp_entry, overflow_entry;
	DIR_META_TYPE tmp_head;
	int64_t overflow_new_page;
	int32_t ret, errcode, ret_index;
	char no_need_rewrite;
	off_t ret_pos;
	DIR_ENTRY temp_dir_entries[(MAX_DIR_ENTRIES_PER_PAGE + 2)];
	int64_t temp_child_page_pos[(MAX_DIR_ENTRIES_PER_PAGE + 3)];
	ino_t new_FS_ino;
	int64_t metasize, metasize_blk;

	sem_wait(&(fs_mgr_head->op_lock));

	if (strlen(fsname) > MAX_FILENAME_LEN) {
		errcode = ENAMETOOLONG;
		write_log(2, "Name of new filesystem (%s) is too long\n",
			  fsname);
		errcode = -errcode;
		goto errcode_handle;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	      meta_pos);

	if (fs_mgr_head->num_FS <= 0) {
		ftruncate(fs_mgr_head->FS_list_fh,
			  sizeof(FS_MANAGER_HEADER_LAYOUT));
		tmp_head.total_children = 0;
		fs_mgr_head->num_FS = 0;
		tmp_head.root_entry_page = sizeof(FS_MANAGER_HEADER_LAYOUT);
		tmp_head.next_xattr_page = 0;
		tmp_head.entry_page_gc_list = 0;
		tmp_head.tree_walk_list_head = sizeof(FS_MANAGER_HEADER_LAYOUT);
		tmp_head.generation = 0;
		memset(&tpage, 0, sizeof(DIR_ENTRY_PAGE));
		tpage.this_page_pos = sizeof(FS_MANAGER_HEADER_LAYOUT);
		PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
		       sizeof(DIR_META_TYPE), meta_pos);
		PWRITE(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		       tpage.this_page_pos);
	} else {
		/* Initialize B-tree insertion by first loading the root of
		*  the B-tree. */
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		      tmp_head.root_entry_page);
	}
	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
				     &ret_index, &tpage2, FALSE);

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

#ifdef _ANDROID_ENV_
	new_FS_ino = _create_root_inode(voltype);
#else
	new_FS_ino = _create_root_inode();
#endif

	if (new_FS_ino == 0) {
		write_log(0, "Error in creating root inode of filesystem\n");
		errcode = -EIO;
		goto errcode_handle;
	}

	/* Update meta size */
	ret = get_meta_size(new_FS_ino, &metasize, &metasize_blk);
	if (ret < 0) {
		write_log(0, "Error: Fail to get meta size\n");
		errcode = ret;
		goto errcode_handle;
	}
	change_system_meta(metasize, metasize_blk, 0, 0, 0, 0, TRUE);

	temp_entry.d_ino = new_FS_ino;
#ifdef _ANDROID_ENV_
	temp_entry.d_type = voltype;
#endif
	snprintf(temp_entry.d_name, MAX_FILENAME_LEN + 1, "%s", fsname);

	/* Recursive routine for B-tree insertion*/
	/* Temp space for traversing the tree is allocated before calling */
	ret = insert_dir_entry_btree(&temp_entry, &tpage,
				     fs_mgr_head->FS_list_fh, &overflow_entry,
				     &overflow_new_page, &tmp_head,
				     temp_dir_entries, temp_child_page_pos,
				     FALSE, meta_pos);

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
			ret_pos = LSEEK(fs_mgr_head->FS_list_fh, 0, SEEK_END);

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
		       sizeof(int64_t) * (MAX_DIR_ENTRIES_PER_PAGE + 1));
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
		PWRITE(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		       tpage.this_page_pos);

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

	PWRITE(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	       meta_pos);

	write_log(10, "Total filesystem is now %" PRId64 "\n",
		  tmp_head.total_children);

	memcpy(ret_entry, &temp_entry, sizeof(DIR_ENTRY));

	/* TODO: return handling for backup database */
	prepare_FS_database_backup();
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
int32_t delete_filesystem(char *fsname)
{
	/* TODO: temp file for atomic operation (see detailed design) */

	DIR_ENTRY_PAGE tpage, tpage2;
	DIR_ENTRY temp_entry;
	DIR_META_TYPE tmp_head, roothead;
	int32_t ret, errcode, ret_index;
	ino_t FS_root;
	char thismetapath[400];
	FILE *metafptr;
	DIR_ENTRY temp_dir_entries[2 * (MAX_DIR_ENTRIES_PER_PAGE + 2)];
	int64_t temp_child_page_pos[2 * (MAX_DIR_ENTRIES_PER_PAGE + 3)];
	int64_t metasize, metasize_blk;

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

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	      meta_pos);

	/* Initialize B-tree deletion by first loading the root of
	*  the B-tree. */
	PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
	      tmp_head.root_entry_page);

	/* Check if the FS name exists. If not, return error */

	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
				     &ret_index, &tpage2, FALSE);

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

	/* Root meta will always be fetched in stage 1, so don't need
	to worry if it is stored locally in stage 2 */

	ret = fetch_meta_path(thismetapath, FS_root);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	metafptr = fopen(thismetapath, "r");
	if (metafptr == NULL) {
		errcode = errno;
		write_log(0, "IO Error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	FSEEK(metafptr, sizeof(HCFS_STAT), SEEK_SET);
	FREAD(&roothead, sizeof(DIR_META_TYPE), 1, metafptr);
	fclose(metafptr);
	metafptr = NULL;
	if (roothead.total_children > 0) {
		/* FS not empty */
		errcode = -ENOTEMPTY;
		write_log(2, "Filesystem is not empty.\n");
		goto errcode_handle;
	}

	/* Update meta size */
	ret = get_meta_size(FS_root, &metasize, &metasize_blk);
	if (ret < 0) {
		write_log(0, "Error: Fail to get meta size\n");
		errcode = ret;
		goto errcode_handle;
	}
	change_system_meta(-metasize, -metasize_blk, 0, 0, 0, 0, TRUE);

	/* Delete root inode (follow ll_rmdir) */
	ret = delete_inode_meta(FS_root);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	/* Delete FS from database */
	ret = delete_dir_entry_btree(&temp_entry, &tpage,
				     fs_mgr_head->FS_list_fh, &tmp_head,
				     temp_dir_entries, temp_child_page_pos,
				     FALSE, meta_pos);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	tmp_head.total_children--;
	write_log(10, "TOTAL CHILDREN is now %" PRId64 "\n",
	          tmp_head.total_children);

	/* Write head back */
	PWRITE(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	       meta_pos);

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
int32_t check_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	int32_t ret;
	/* This is the wrapper around check_filesytem_core, and
	also locks FS manager lock */
	sem_wait(&(fs_mgr_head->op_lock));

	ret = check_filesystem_core(fsname, ret_entry);
	sem_post(&(fs_mgr_head->op_lock));
	return ret;
}
int32_t check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry)
{
	DIR_ENTRY_PAGE tpage, tpage2;
	DIR_META_TYPE tmp_head;
	int32_t ret, errcode, ret_index;

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

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	      meta_pos);

	/* Initialize B-tree searching by first loading the root of
	*  the B-tree. */
	PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
	      tmp_head.root_entry_page);

	/* Check if the FS name exists. If not, return error */

	ret = search_dir_entry_btree(fsname, &tpage, fs_mgr_head->FS_list_fh,
				     &ret_index, &tpage2, FALSE);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	memcpy(ret_entry, &(tpage2.dir_entries[ret_index]), sizeof(DIR_ENTRY));

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: list_filesystem
*        Inputs: int32_t buf_num, DIR_ENTRY *ret_entry, int32_t *ret_num
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
int32_t list_filesystem(uint64_t buf_num, DIR_ENTRY *ret_entry,
		    uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;

	sem_wait(&(fs_mgr_head->op_lock));

	if (fs_mgr_head->num_FS > (int) buf_num) {
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

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE),
	      meta_pos);

	/* Initialize B-tree walk by first loading the first node
		of the tree walk. */
	next_node_pos = tmp_head.tree_walk_list_head;

	num_walked = 0;

	while (next_node_pos != 0) {
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		      next_node_pos);
		if ((num_walked + tpage.num_entries) > (int64_t)buf_num) {
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
			       sizeof(DIR_META_TYPE), meta_pos);
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
* Function name: prepare_FS_database_backup
*        Inputs: None
*       Summary: Write DB to a tmp file for backup to backend.
*          Note: Assume that the op_lock is locked when calling
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t prepare_FS_database_backup(void)
{
	char tmppath[METAPATHLEN];
	FILE *fptr;
	int32_t errcode;
	char buf[4096];
	ssize_t ret_ssize;
	off_t curpos;

	snprintf(tmppath, METAPATHLEN, "%s/fsmgr_upload", METAPATH);

	/* Assume that the access lock is locked already */

	fptr = NULL;
	if (access(tmppath, F_OK) == 0)
		fptr = fopen(tmppath, "r+");
	else
		fptr = fopen(tmppath, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	flock(fileno(fptr), LOCK_EX);
	setbuf(fptr, NULL);
	FTRUNCATE(fileno(fptr), 0);

	curpos = 0;
	while (!feof(fptr)) {
		ret_ssize = PREAD(fs_mgr_head->FS_list_fh, buf, 4096, curpos);
		if (ret_ssize <= 0)
			break;
		curpos += ret_ssize;
		FWRITE(buf, 1, (size_t)ret_ssize, fptr);
	}

	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
	return 0;

errcode_handle:
	if (fptr != NULL)
		fclose(fptr);

	unlink(tmppath);
	return errcode;
}

/************************************************************************
*
* Function name: backup_FS_database
*        Inputs: None
*       Summary: Upload FS backup to backend if the temp backup file exists.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t backup_FS_database(void)
{
	char tmppath[METAPATHLEN];
	FILE *fptr = NULL, *tmpdbfptr = NULL;
	int32_t ret, errcode;
	CURL_HANDLE upload_handle;
	char buf[4096];
	size_t ret_size;
	off_t ret_pos;
	GOOGLEDRIVE_OBJ_INFO gdrive_info;
	CLOUD_RELATED_DATA clouddata;
	BOOL need_record_id = FALSE;

	if (CURRENT_BACKEND == NONE)
		return 0;
	memset(&upload_handle, 0, sizeof(CURL_HANDLE));

	snprintf(tmppath, METAPATHLEN, "%s/fsmgr_upload", METAPATH);

	/* Check if temp db exists and needs to be uploaded */
	if (access(tmppath, F_OK) != 0)
		return 0;

	tmpdbfptr = fopen(tmppath, "r");
	if (tmpdbfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	flock(fileno(tmpdbfptr), LOCK_EX);
	/* Check if file size is zero. If so, do nothing */
	FSEEK(tmpdbfptr, 0, SEEK_END);
	ret_pos = FTELL(tmpdbfptr);
	if (ret_pos == 0) {
		/* Empty file, do nothing */
		flock(fileno(tmpdbfptr), LOCK_UN);
		fclose(tmpdbfptr);
		return 0;
	}
	FSEEK(tmpdbfptr, 0, SEEK_SET);

	fptr = fopen("/tmp/FSmgr_upload", "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	while (!feof(fptr)) {
		ret_size = FREAD(buf, 1, 4096, tmpdbfptr);
		if (ret_size == 0)
			break;
		FWRITE(buf, 1, ret_size, fptr);
	}
	flock(fileno(tmpdbfptr), LOCK_UN);
	fclose(tmpdbfptr);
	fclose(fptr);

	snprintf(upload_handle.id, sizeof(upload_handle.id), "FSmgr_upload");
	upload_handle.curl_backend = NONE;
	upload_handle.curl = NULL;

	fptr = fopen("/tmp/FSmgr_upload", "r");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	if (CURRENT_BACKEND == GOOGLEDRIVE) {
		/* Read meta ID */
		PREAD(fileno(fptr), &clouddata, sizeof(CLOUD_RELATED_DATA),
		      clouddata_pos);
		memset(&gdrive_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
		strncpy(gdrive_info.file_title, FSMGR_BACKUP, 50);
		get_parent_id(gdrive_info.parentID, FSMGR_BACKUP);
		if (clouddata.metaID[0]) {
			strncpy(gdrive_info.fileID, clouddata.metaID,
				GDRIVE_ID_LENGTH);
			need_record_id = FALSE;
			write_log(0, "TEST: Old id %s", gdrive_info.fileID);
		} else {
			need_record_id = TRUE;
		}
		ret = hcfs_put_object(fptr, FSMGR_BACKUP, &upload_handle,
				      NULL, &gdrive_info);
	} else {
		ret = hcfs_put_object(fptr, FSMGR_BACKUP, &upload_handle,
				      NULL, NULL);
	}
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in backing up FS database\n");
		goto errcode_handle;
	}
	fclose(fptr);

	/* Check if need to record google drive obj id */
	if (need_record_id) {
		write_log(0, "TEST: New id %s", gdrive_info.fileID);
		strncpy(clouddata.metaID, gdrive_info.fileID, GDRIVE_ID_LENGTH);
		sem_wait(&(fs_mgr_head->op_lock));
		PWRITE(fs_mgr_head->FS_list_fh, &clouddata,
		       sizeof(CLOUD_RELATED_DATA), clouddata_pos);
		sem_post(&(fs_mgr_head->op_lock));
	}

	unlink("/tmp/FSmgr_upload");
	hcfs_destroy_backend(&upload_handle);
	unlink(tmppath);
	return 0;

errcode_handle:
	if (upload_handle.curl != NULL)
		hcfs_destroy_backend(&upload_handle);

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
int32_t restore_FS_database(void)
{
	FILE *fptr;
	int32_t ret, errcode;
	CURL_HANDLE download_handle;

	/* Assume that FS manager is not started */

	memset(&download_handle, 0, sizeof(CURL_HANDLE));

	snprintf(download_handle.id, sizeof(download_handle.id),
		 "FSmgr_download");
	download_handle.curl_backend = NONE;
	download_handle.curl = NULL;
	/* Do not actually init backend until needed */
	/*
		ret = hcfs_init_backend(&download_handle);
		if ((ret < 200) || (ret > 299)) {
			errcode = -EIO;
			write_log(0, "Error in restoring FS database\n");
			goto errcode_handle;
		}
	*/
	fptr = fopen(fs_mgr_path, "w");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	ret =
	    hcfs_get_object(fptr, FSMGR_BACKUP, &download_handle, NULL, NULL);
	if ((ret < 200) || (ret > 299)) {
		errcode = -EIO;
		write_log(0, "Error in restoring FS database\n");
		goto errcode_handle;
	}

	fclose(fptr);

	hcfs_destroy_backend(&download_handle);
	return 0;

errcode_handle:
	if (download_handle.curl != NULL)
		hcfs_destroy_backend(&download_handle);

	if (fptr != NULL)
		fclose(fptr);

	return errcode;
}
