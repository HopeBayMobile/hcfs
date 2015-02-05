/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseop.c
* Abstract: The c source code file for the main FUSE operations for HCFS
*
* Revision History
* 2015/2/2 Jiahong added header for this file, and revising coding style.
*          File is renamed from hfuseops.c to fuseop.c
* 2015/2/3~2/5 (Jiahong) Restructure hfuse_truncate, hfuse_write, hfuse_read
*              functions
*
**************************************************************************/

#define FUSE_USE_VERSION 26

#include "fuseop.h"

#include <time.h>
#include <math.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <sys/mman.h>

/* Headers from the other libraries */
#include <fuse.h>
#include <curl/curl.h>

#include "global.h"
#include "file_present.h"
#include "utils.h"
#include "dir_lookup.h"
#include "super_block.h"
#include "params.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "meta_mem_cache.h"
#include "filetables.h"

extern SYSTEM_CONF_STRUCT system_config;

/* TODO: Need to go over the access rights problem for the ops */
/* TODO: Need to revisit the following problem for all ops: access rights,
/*   timestamp change (a_time, m_time, c_time), and error handling */
/* TODO: For access rights, need to check file permission and/or system acl.
/*   System acl is set in extended attributes. */
/* TODO: The FUSE option "default_permission" should be turned on if there
/*   is no actual file permission check, or turned off if we are checking
/*   system acl. */

/* TODO: Access time may not be changed for file accesses, if noatime is
/*   specified in file opening or mounting. */
/* TODO: Will need to implement rollback or error marking when ops failed*/

/* TODO: Pending design for a single cache device, and use pread/pwrite to
/*   allow multiple threads to access cache concurrently without the need for
/*   file handles */

/* TODO: Need to be able to perform actual operations according to type of
/*   folders (cached, non-cached, local) */
/* TODO: Push actual operations to other source files, especially no actual
/*   file handling in this file */
/* TODO: Multiple paths for read / write / other ops for different folder
/*   policies. Policies to be determined at file or dir open. */

/************************************************************************
*
* Function name: hfuse_getattr
*        Inputs: char *pathname, struct stat *inode_stat
*       Summary: Given the string "path", read the stat of this
*                filesystem object into inode_stat, and return to the
*                caller.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_getattr(const char *path, struct stat *inode_stat)
{
	ino_t hit_inode;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;
	struct fuse_context *temp_context;

	temp_context = fuse_get_context();

	printf("Data passed in is %s\n", (char *) temp_context->private_data);

	gettimeofday(&tmp_time1, NULL);
	hit_inode = lookup_pathname(path, &ret_code);

	gettimeofday(&tmp_time2, NULL);

	printf("getattr lookup_pathname elapse %f\n",
		(tmp_time2.tv_sec - tmp_time1.tv_sec)
		+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));


	if (hit_inode > 0) {
		ret_code = fetch_inode_stat(hit_inode, inode_stat);

		#if DEBUG >= 5
		printf("getattr %lld, returns %d\n", inode_stat->st_ino,
			ret_code);
		#endif  /* DEBUG */

		gettimeofday(&tmp_time2, NULL);

		printf("getattr elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

		return ret_code;
	 } else {
		gettimeofday(&tmp_time2, NULL);

		printf("getattr elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));
	 }
	return ret_code;
}

/************************************************************************
*
* Function name: hfuse_mknod
*        Inputs: const char *path, mode_t mode, dev_t dev
*       Summary: Given the string "path", create a regular file with the
*                permission specified by mode. "dev" is ignored as only
*                regular file will be created.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
	char *parentname;
	char selfname[MAX_FILE_NAME_LEN];
	ino_t self_inode, parent_inode;
	struct stat this_stat;
	mode_t self_mode;
	int ret_val;
	struct fuse_context *temp_context;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;

	gettimeofday(&tmp_time1, NULL);


	parentname = malloc(strlen(path)*sizeof(char));
	parse_parent_self(path, parentname, selfname);

	parent_inode = lookup_pathname(parentname, &ret_code);

	free(parentname);
	if (parent_inode < 1)
		return ret_code;

	memset(&this_stat, 0, sizeof(struct stat));
	temp_context = fuse_get_context();

/* TODO: May need to reject special file creation here */

	self_mode = mode | S_IFREG;
	this_stat.st_mode = self_mode;
	this_stat.st_size = 0;
	this_stat.st_blksize = MAX_BLOCK_SIZE;
	this_stat.st_blocks = 0;
	this_stat.st_dev = dev;
	this_stat.st_nlink = 1;

	/*Use the uid and gid of the fuse caller*/
	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	/* Use the current time for timestamps */
	this_stat.st_atime = time(NULL);
	this_stat.st_mtime = this_stat.st_atime;
	this_stat.st_ctime = this_stat.st_atime;

	self_inode = super_block_new_inode(&this_stat);
	if (self_inode < 1)
		return -EACCES;
	this_stat.st_ino = self_inode;

	ret_code = mknod_update_meta(self_inode, parent_inode, selfname,
			&this_stat);

	if (ret_code < 0)
		meta_forget_inode(self_inode);

	gettimeofday(&tmp_time2, NULL);

	printf("mknod elapse %f\n", (tmp_time2.tv_sec - tmp_time1.tv_sec)
		+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	return ret_code;
}

/************************************************************************
*
* Function name: hfuse_mkdir
*        Inputs: const char *path, mode_t mode
*       Summary: Given the string "path", create a subdirectory with the
*                permission specified by mode.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_mkdir(const char *path, mode_t mode)
{
	char *parentname;
	char selfname[400];
	ino_t self_inode, parent_inode;
	struct stat this_stat;
	mode_t self_mode;
	int ret_val;
	struct fuse_context *temp_context;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;

	gettimeofday(&tmp_time1, NULL);

	parentname = malloc(strlen(path)*sizeof(char));
	parse_parent_self(path, parentname, selfname);

	parent_inode = lookup_pathname(parentname, &ret_code);

	free(parentname);
	if (parent_inode < 1)
		return ret_code;

	memset(&this_stat, 0, sizeof(struct stat));
	temp_context = fuse_get_context();

	self_mode = mode | S_IFDIR;
	this_stat.st_mode = self_mode;
	this_stat.st_nlink = 2; /*One pointed by the parent, another by self*/

	/*Use the uid and gid of the fuse caller*/
	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	this_stat.st_atime = time(NULL);
	this_stat.st_mtime = this_stat.st_atime;
	this_stat.st_ctime = this_stat.st_atime;
	this_stat.st_size = 0;
	this_stat.st_blksize = MAX_BLOCK_SIZE;
	this_stat.st_blocks = 0;

	self_inode = super_block_new_inode(&this_stat);
	if (self_inode < 1)
		return -EACCES;
	this_stat.st_ino = self_inode;

	ret_code = mkdir_update_meta(self_inode, parent_inode,
			selfname, &this_stat);

	if (ret_code < 0)
		meta_forget_inode(self_inode);

	gettimeofday(&tmp_time2, NULL);

	printf("mkdir elapse %f\n", (tmp_time2.tv_sec - tmp_time1.tv_sec)
		+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	return ret_code;
}

/************************************************************************
*
* Function name: hfuse_unlink
*        Inputs: const char *path
*       Summary: Delete the regular file specified by the string "path".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_unlink(const char *path)
{
	char *parentname;
	char selfname[400];
	ino_t this_inode, parent_inode;
	int ret_val;
	int ret_code;

	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	parentname = malloc(strlen(path)*sizeof(char));
	parse_parent_self(path, parentname, selfname);
	parent_inode = lookup_pathname(parentname, &ret_code);

	free(parentname);
	if (parent_inode < 1)
		return ret_code;

	invalidate_pathname_cache_entry(path);

	ret_val = unlink_update_meta(parent_inode, this_inode, selfname);

	return ret_val;
}

/************************************************************************
*
* Function name: hfuse_rmdir
*        Inputs: const char *path
*       Summary: Delete the directory specified by the string "path".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_rmdir(const char *path)
{
	char *parentname;
	char selfname[400];
	ino_t this_inode, parent_inode;
	int ret_val, ret_code;

	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	parentname = malloc(strlen(path)*sizeof(char));
	parse_parent_self(path, parentname, selfname);

	if (!strcmp(selfname, ".")) {
		free(parentname);
		return -EINVAL;
	}
	if (!strcmp(selfname, "..")) {
		free(parentname);
		return -ENOTEMPTY;
	}

	parent_inode = lookup_pathname(parentname, &ret_code);
	free(parentname);

	if (parent_inode < 1)
		return ret_code;

	invalidate_pathname_cache_entry(path);

	ret_val = rmdir_update_meta(parent_inode, this_inode, selfname);

	return ret_val;
}

/************************************************************************
*
* Function name: hfuse_rename
*        Inputs: const char *oldpath, const char *newpath
*       Summary: Rename / move the filesystem object "oldpath" to
*                "newpath", replacing the original object in "newpath" if
*                necessary.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*    Limitation: Current implementation only supports the case when the
*                target does not exist before the operation, and no symlink
*
*************************************************************************/
static int hfuse_rename(const char *oldpath, const char *newpath)
{
	/* TODO: Check how to make this operation atomic */
	char *parentname1;
	char selfname1[400];
	char *parentname2;
	char selfname2[400];
	ino_t parent_inode1, parent_inode2, self_inode;
	int ret_val;
	struct stat tempstat;
	mode_t self_mode;
	int ret_code, ret_code2;
	META_CACHE_ENTRY_STRUCT *body_ptr, *parent_ptr;

	self_inode = lookup_pathname(oldpath, &ret_code);
	if (self_inode < 1)
		return ret_code;

	invalidate_pathname_cache_entry(oldpath);

	if (lookup_pathname(newpath, &ret_code) > 0)
		return -EACCES;

	body_ptr = meta_cache_lock_entry(self_inode);
	ret_val = meta_cache_lookup_file_data(self_inode, &tempstat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(self_inode);
		return -ENOENT;
	}

	self_mode = tempstat.st_mode;

	/*TODO: Will now only handle simple types (that the target is
		empty and no symlinks)*/
	parentname1 = malloc(strlen(oldpath)*sizeof(char));
	parentname2 = malloc(strlen(newpath)*sizeof(char));
	parse_parent_self(oldpath, parentname1, selfname1);
	parse_parent_self(newpath, parentname2, selfname2);

	parent_inode1 = lookup_pathname(parentname1, &ret_code);

	parent_inode2 = lookup_pathname(parentname2, &ret_code2);

	free(parentname1);
	free(parentname2);

	if (parent_inode1 < 1) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(self_inode);
		return ret_code;
	}

	if (parent_inode2 < 1) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(self_inode);
		return ret_code2;
	}

	parent_ptr = meta_cache_lock_entry(parent_inode1);

	ret_val = dir_remove_entry(parent_inode1, self_inode,
			selfname1, self_mode, parent_ptr);
	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_close_file(parent_ptr);
		meta_cache_unlock_entry(parent_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(self_inode);
		return -EACCES;
	}

	if (parent_inode1 != parent_inode2) {
		meta_cache_close_file(parent_ptr);
		meta_cache_unlock_entry(parent_ptr);
		parent_ptr = meta_cache_lock_entry(parent_inode2);
	}

	ret_val = dir_add_entry(parent_inode2, self_inode,
			selfname2, self_mode, parent_ptr);

	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_close_file(parent_ptr);
		meta_cache_unlock_entry(parent_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(self_inode);
		return -EACCES;
	}

	meta_cache_close_file(parent_ptr);
	meta_cache_unlock_entry(parent_ptr);

	if ((self_mode & S_IFDIR) && (parent_inode1 != parent_inode2)) {
		ret_val = change_parent_inode(self_inode, parent_inode1,
				parent_inode2, body_ptr);
		if (ret_val < 0) {
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			meta_cache_remove(self_inode);
			return -EACCES;
		}
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	return 0;
}

/************************************************************************
*
* Function name: hfuse_chmod
*        Inputs: const char *path, mode_t mode
*       Summary: Change the permission of the filesystem object pointed
*                by "path" to "mode".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_chmod(const char *path, mode_t mode)
{
	struct stat temp_inode_stat;
	int ret_val;
	ino_t this_inode;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	printf("Debug chmod\n");
	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	body_ptr = meta_cache_lock_entry(this_inode);
	ret_val = meta_cache_lookup_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {  /* Cannot fetch any meta*/
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(this_inode);
		return -EACCES;
	}

	temp_inode_stat.st_mode = mode;
	temp_inode_stat.st_ctime = time(NULL);

	ret_val = meta_cache_update_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;
}

/************************************************************************
*
* Function name: hfuse_chown
*        Inputs: const char *path, uid_t owner, gid_t group
*       Summary: Change the owner/group of the filesystem object pointed
*                by "path" to "owner" and "group".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_chown(const char *path, uid_t owner, gid_t group)
{
	struct stat temp_inode_stat;
	int ret_val;
	ino_t this_inode;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	printf("Debug chown\n");

	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	body_ptr = meta_cache_lock_entry(this_inode);
	ret_val = meta_cache_lookup_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {  /* Cannot fetch any meta*/
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(this_inode);
		return -EACCES;
	}

	temp_inode_stat.st_uid = owner;
	temp_inode_stat.st_gid = group;
	temp_inode_stat.st_ctime = time(NULL);

	ret_val = meta_cache_update_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;
}

/************************************************************************
*
* Function name: hfuse_utimens
*        Inputs: const char *path, const struct timespec tv[2]
*       Summary: Change the access / modification time of the filesystem
*                object pointed by "path" to "tv".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_utimens(const char *path, const struct timespec tv[2])
{
	struct stat temp_inode_stat;
	int ret_val;
	ino_t this_inode;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	printf("Debug utimens\n");
	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	body_ptr = meta_cache_lock_entry(this_inode);
	ret_val = meta_cache_lookup_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {  /* Cannot fetch any meta*/
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(this_inode);
		return -EACCES;
	}

	temp_inode_stat.st_atime = (time_t)(tv[0].tv_sec);
	temp_inode_stat.st_mtime = (time_t)(tv[1].tv_sec);

	ret_val = meta_cache_update_file_data(this_inode, &temp_inode_stat,
			NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return ret_val;
}

/* Helper function for waiting on full cache in the truncate function */
int truncate_wait_full_cache(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT **body_ptr,
	int entry_index)
{
	while (((block_page)->block_entries[entry_index].status == ST_CLOUD) ||
		((block_page)->block_entries[entry_index].status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			/*Sleep if cache already full*/
			printf("debug truncate waiting on full cache\n");
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);
			sleep_on_cache_full();

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			meta_cache_lookup_file_data(this_inode, inode_stat,
				file_meta_ptr, block_page, page_pos, *body_ptr);
		} else {
			break;
		}
	}
	return 0;
}

/* Helper function for truncate operation. Will delete all blocks in the page
*  pointed by temppage starting from "start_index". Block index is tracked
*  globally using "temp_block_index. "old_last_block" indicates the last block
*  index before the truncate operation (hence we can ignore the blocks after
*  "old_last_block". "inode_index" is the inode number of the file being
*  truncated. */
int truncate_delete_block(BLOCK_ENTRY_PAGE *temppage, int start_index,
			long long *temp_block_index, long long old_last_block,
			ino_t inode_index)
{
	int block_count;
	char thisblockpath[1024];

	for (block_count = start_index; block_count
		< MAX_BLOCK_ENTRIES_PER_PAGE; block_count++) {
		if ((*temp_block_index) > old_last_block)
			break;
		switch ((temppage->block_entries[block_count]).status) {
		case ST_NONE:
		case ST_TODELETE:
			break;
		case ST_LDISK:
			fetch_block_path(thisblockpath, inode_index,
				*temp_block_index);
			unlink(thisblockpath);
			(temppage->block_entries[block_count]).status =
				ST_NONE;
			break;
		case ST_CLOUD:
			(temppage->block_entries[block_count]).status =
				ST_TODELETE;
			break;
		case ST_BOTH:
		case ST_LtoC:
		case ST_CtoL:
			fetch_block_path(thisblockpath, inode_index,
				*temp_block_index);
			if (access(thisblockpath, F_OK) == 0)
				unlink(thisblockpath);
			(temppage->block_entries[block_count]).status =
				ST_TODELETE;
			break;
		default:
			break;
		}
		(*temp_block_index)++;
	}
	return 0;
}

/* Helper function for hfuse_truncate. This will truncate the last block
*  that remains after the truncate operation so that the size of this block
*  fits (offset % MAX_BLOCK_SIZE) */
int truncate_truncate(ino_t this_inode, struct stat *filestat,
	FILE_META_TYPE *tempfilemeta, BLOCK_ENTRY_PAGE *temppage,
	long long currentfilepos, META_CACHE_ENTRY_STRUCT **body_ptr,
	int last_index, long long last_block, off_t offset)

{
	char thisblockpath[1024];
	FILE *blockfptr;
	struct stat tempstat;
	off_t old_block_size, new_block_size;

	/*Offset not on the boundary of the block. Will need to truncate the
	last block*/
	truncate_wait_full_cache(this_inode, filestat, tempfilemeta,
			temppage, currentfilepos, body_ptr, last_index);

	fetch_block_path(thisblockpath, filestat->st_ino, last_block);

	if (((temppage->block_entries[last_index]).status == ST_CLOUD) ||
		((temppage->block_entries[last_index]).status == ST_CtoL)) {
		/*Download from backend */
		blockfptr = fopen(thisblockpath, "a+");
		fclose(blockfptr);
		blockfptr = fopen(thisblockpath, "r+");
		setbuf(blockfptr, NULL);
		flock(fileno(blockfptr), LOCK_EX);

		meta_cache_lookup_file_data(this_inode, NULL, NULL, temppage,
			currentfilepos, *body_ptr);

		if (((temppage->block_entries[last_index]).status ==
			 ST_CLOUD) ||
			((temppage->block_entries[last_index]).status ==
				ST_CtoL)) {
			if ((temppage->block_entries[last_index]).status ==
				ST_CLOUD) {
				(temppage->block_entries[last_index]).status =
					ST_CtoL;
				meta_cache_update_file_data(this_inode, NULL,
					NULL, temppage, currentfilepos,
					*body_ptr);
			}
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);

			fetch_from_cloud(blockfptr, filestat->st_ino,
				last_block);

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			meta_cache_lookup_file_data(this_inode, NULL, NULL,
				temppage, currentfilepos, *body_ptr);

			if (stat(thisblockpath, &tempstat) == 0) {
				(temppage->block_entries[last_index]).status =
					ST_LDISK;
				setxattr(thisblockpath, "user.dirty", "T",
					1, 0);
				meta_cache_update_file_data(this_inode, NULL,
					NULL, temppage, currentfilepos,
					*body_ptr);

				change_system_meta(0, tempstat.st_size, 1);
			}
		} else {
			if (stat(thisblockpath, &tempstat) == 0) {
				(temppage->block_entries[last_index]).status =
					ST_LDISK;
				setxattr(thisblockpath, "user.dirty", "T",
					1, 0);
				meta_cache_update_file_data(this_inode, NULL,
					NULL, temppage, currentfilepos,
					*body_ptr);
			}
		}
		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		change_system_meta(0, new_block_size - old_block_size, 1);

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
	} else {
		blockfptr = fopen(thisblockpath, "r+");
		setbuf(blockfptr, NULL);
		flock(fileno(blockfptr), LOCK_EX);

		if (stat(thisblockpath, &tempstat) == 0) {
			(temppage->block_entries[last_index]).status = ST_LDISK;
			setxattr(thisblockpath, "user.dirty", "T", 1, 0);
			meta_cache_update_file_data(this_inode, NULL, NULL,
				temppage, currentfilepos, *body_ptr);
		}

		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		change_system_meta(0, new_block_size - old_block_size, 1);

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
	}
	return 0;
}

/************************************************************************
*
* Function name: hfuse_truncate
*        Inputs: const char *path, off_t offset
*       Summary: Truncate the regular file pointed by "path to size "offset".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_truncate(const char *path, off_t offset)
{
/* If truncate file smaller, do not truncate metafile, but instead set the
*  affected entries to ST_TODELETE (which will be changed to ST_NONE once
*  object deleted)*/
/* Add ST_TODELETE as a new block status. In truncate, if need to throw away
*  a block, set the status to ST_TODELETE and upload process will handle the
*  actual deletion.*/
/* If need to truncate some block that's ST_CtoL or ST_CLOUD, download it
*  first, mod it, then set to ST_LDISK*/

	struct stat filestat;
	FILE_META_TYPE tempfilemeta;
	int ret_val;
	ino_t this_inode;
	long long last_block, last_page, old_last_block;
	long long current_page;
	off_t nextfilepos, prevfilepos, currentfilepos;
	BLOCK_ENTRY_PAGE temppage;
	int last_index;
	long long temp_block_index;
	int ret_code;
	META_CACHE_ENTRY_STRUCT *body_ptr;

	this_inode = lookup_pathname(path, &ret_code);
	if (this_inode < 1)
		return ret_code;

	body_ptr = meta_cache_lock_entry(this_inode);

	ret_val = meta_cache_lookup_file_data(this_inode, &filestat,
			NULL, NULL, 0, body_ptr);

	/* If the filesystem object is not a regular file, return error */
	if (filestat.st_mode & S_IFREG == FALSE) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);

		if (filestat.st_mode & S_IFDIR)
			return -EISDIR;
		else
			return -EACCES;
	}

	ret_val = meta_cache_lookup_file_data(this_inode, NULL, &tempfilemeta,
			NULL, 0, body_ptr);

	if (filestat.st_size == offset) {
		/*Do nothing if no change needed */
		printf("Debug truncate: no size change. Nothing changed.\n");
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		return 0;
	}

	if (filestat.st_size < offset) {
		/*If need to extend, only need to change st_size*/
		change_system_meta((long long)(offset - filestat.st_size),
			0, 0);

		filestat.st_size = offset;
	} else {
		if (offset == 0) {
			last_block = -1;
			last_page = -1;
		} else {
			/* Block indexing starts at zero */
			last_block = ((offset-1) / MAX_BLOCK_SIZE);

			/*Page indexing starts at zero*/
			last_page = last_block / MAX_BLOCK_ENTRIES_PER_PAGE;
		}

		old_last_block = ((filestat.st_size - 1) / MAX_BLOCK_SIZE);
		nextfilepos = tempfilemeta.next_block_page;

		current_page = 0;
		prevfilepos = 0;

		temp_block_index = last_block+1;

		/*TODO: put error handling for the read/write ops here*/
		while (current_page <= last_page) {
			if (nextfilepos == 0) {
				/*Data after offset does not actually exists.
				Just change file size */
				change_system_meta((long long)(offset -
					filestat.st_size), 0, 0);
				filestat.st_size = offset;
				break;
			}

			meta_cache_lookup_file_data(this_inode, NULL,
				NULL, &temppage, nextfilepos,
				body_ptr);
			prevfilepos = nextfilepos;
			nextfilepos = temppage.next_page;

			if (current_page == last_page) {
				/* Do the actual handling here*/
				currentfilepos = prevfilepos;
				last_index = last_block %
					MAX_BLOCK_ENTRIES_PER_PAGE;
				if ((offset % MAX_BLOCK_SIZE) != 0)
					/* Truncate the last block that remains
					   after the truncate operation */
					truncate_truncate(this_inode, &filestat,
						&tempfilemeta,
						&temppage, currentfilepos,
						&body_ptr, last_index,
						last_block, offset);

				/*Delete the rest of blocks in this same page
				as well*/
				truncate_delete_block(&temppage, last_index+1,
					&temp_block_index, old_last_block,
					filestat.st_ino);

				meta_cache_update_file_data(this_inode, NULL,
					NULL, &temppage, currentfilepos,
					body_ptr);

				change_system_meta((long long)(offset -
						filestat.st_size), 0, 0);
				filestat.st_size = offset;
				break;
			}

			current_page++;
		}

		/*Delete the blocks in the rest of the block status pages*/

		while (nextfilepos != 0) {
			currentfilepos = nextfilepos;
			meta_cache_lookup_file_data(this_inode, NULL, NULL,
				&temppage, currentfilepos, body_ptr);

			nextfilepos = temppage.next_page;

			truncate_delete_block(&temppage, 0,
				&temp_block_index, old_last_block,
				filestat.st_ino);

			meta_cache_update_file_data(this_inode, NULL, NULL,
				&temppage, currentfilepos, body_ptr);
		}
	}

	filestat.st_mtime = time(NULL);
	ret_val = meta_cache_update_file_data(this_inode, &filestat,
			&tempfilemeta, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);

	return 0;
}

/************************************************************************
*
* Function name: hfuse_open
*        Inputs: const char *path, struct fuse_file_info *file_info
*       Summary: Open the regular file pointed by "path", and put file
*                handle info to the structure pointed by "file_info".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_open(const char *path, struct fuse_file_info *file_info)
{
	/*TODO: Need to check permission here*/
	ino_t thisinode;
	long long fh;
	int ret_code;

	thisinode = lookup_pathname(path, &ret_code);
	if (thisinode < 1)
		return ret_code;

	fh = open_fh(thisinode);
	if (fh < 0)
		return -ENFILE;

	file_info->fh = fh;

	return 0;
}

/* Helper function for read operation. Will load file object meta from
*  meta cache or meta file. */
int read_lookup_meta(FH_ENTRY *fh_ptr, BLOCK_ENTRY_PAGE *temppage,
		off_t this_page_fpos)
{
	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	fh_ptr->meta_cache_locked = TRUE;
	meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, temppage,
			this_page_fpos, fh_ptr->meta_cache_ptr);
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	return 0;
}

/* Helper function for read operation. Will wait on cache full and wait
*  until cache is not full. */
int read_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, long long entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos)
{
	while (((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
		((temppage->block_entries[entry_index]).status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			/*Sleep if cache already full*/
			sem_post(&(fh_ptr->block_sem));
			printf("debug read waiting on full cache\n");
			sleep_on_cache_full();
			sem_wait(&(fh_ptr->block_sem));
			/*Re-read status*/
			read_lookup_meta(fh_ptr, temppage, this_page_fpos);
		} else {
			break;
		}
	}
	return 0;
}

/* Helper function for read operation. Will prefetch a block from backend for
*  reading. */
int read_prefetch_cache(BLOCK_ENTRY_PAGE *tpage, long long eindex,
		ino_t this_inode, long long block_index, off_t this_page_fpos)
{
	PREFETCH_STRUCT_TYPE *temp_prefetch;
	pthread_t prefetch_thread;

	if ((eindex+1) < MAX_BLOCK_ENTRIES_PER_PAGE) {
		if (((tpage->block_entries[eindex+1]).status == ST_CLOUD) ||
			((tpage->block_entries[eindex+1]).status == ST_CtoL)) {
			temp_prefetch = malloc(sizeof(PREFETCH_STRUCT_TYPE));
			temp_prefetch->this_inode = this_inode;
			temp_prefetch->block_no = block_index + 1;
			temp_prefetch->page_start_fpos = this_page_fpos;
			temp_prefetch->entry_index = eindex + 1;
			pthread_create(&(prefetch_thread),
				&prefetch_thread_attr, (void *)&prefetch_block,
				((void *)temp_prefetch));
		}
	}
	return 0;
}

/* Helper function for read operation. Will fetch a block from backend for
*  reading. */
int read_fetch_backend(ino_t this_inode, long long bindex, FH_ENTRY *fh_ptr,
		BLOCK_ENTRY_PAGE *tpage, off_t page_fpos, long long eindex)
{
	char thisblockpath[400];
	struct stat tempstat2;

	fetch_block_path(thisblockpath, this_inode, bindex);
	fh_ptr->blockfptr = fopen(thisblockpath, "a+");
	fclose(fh_ptr->blockfptr);
	fh_ptr->blockfptr = fopen(thisblockpath, "r+");
	setbuf(fh_ptr->blockfptr, NULL);
	flock(fileno(fh_ptr->blockfptr), LOCK_EX);

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	fh_ptr->meta_cache_locked = TRUE;
	meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, tpage,
			page_fpos, fh_ptr->meta_cache_ptr);

	if (((tpage->block_entries[eindex]).status == ST_CLOUD) ||
			((tpage->block_entries[eindex]).status == ST_CtoL)) {
		if ((tpage->block_entries[eindex]).status == ST_CLOUD) {
			(tpage->block_entries[eindex]).status = ST_CtoL;
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
					NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
		}
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fetch_from_cloud(fh_ptr->blockfptr, this_inode, bindex);
		/* Do not process cache update and stored_where change if block
		*  is actually deleted by other ops such as truncate*/

		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		fh_ptr->meta_cache_locked = TRUE;
		meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL,
				tpage, page_fpos, fh_ptr->meta_cache_ptr);

		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_BOTH;
			fsetxattr(fileno(fh_ptr->blockfptr), "user.dirty",
					"F", 1, 0);
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
					NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);

			/* Update system meta to reflect correct cache size */
			change_system_meta(0, tempstat2.st_size, 1);
		}
	}
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	setbuf(fh_ptr->blockfptr, NULL);
	fh_ptr->opened_block = bindex;

	return 0;
}

/* Helper function for read operation. Will advance to the next block. */
int read_advance_block(long long *entry_index, FH_ENTRY *fh_ptr,
				off_t *this_page_fpos)
{
	if (((*entry_index)+1) >= MAX_BLOCK_ENTRIES_PER_PAGE) {
		/*If may need to change meta, lock*/
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		fh_ptr->meta_cache_locked = TRUE;

		*this_page_fpos = advance_block(fh_ptr->meta_cache_ptr,
						*this_page_fpos, entry_index);

		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	} else {
		(*entry_index)++;
	}
	return 0;
}

/************************************************************************
*
* Function name: hfuse_read
*        Inputs: const char *path, char *buf, size_t size_org, off_t offset,
*                struct fuse_file_info *file_info
*       Summary: Read "size_org" bytes from the file "path", starting from
*                "offset", into the buffer pointed by "buf". File handle
*                is provided by the structure in "file_info".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_read(const char *path, char *buf, size_t size_org, off_t offset,
	struct fuse_file_info *file_info)
{
	FH_ENTRY *fh_ptr;
	long long start_block, end_block;
	long long start_page, current_page;
	off_t nextfilepos, prevfilepos, currentfilepos;
	BLOCK_ENTRY_PAGE temppage;
	long long entry_index;
	long long block_index;
	char thisblockpath[400];
	int total_bytes_read;
	int this_bytes_read;
	off_t current_offset;
	int target_bytes_read;
	size_t size;
	char fill_zeros;
	off_t this_page_fpos;
	struct stat temp_stat;


/* TODO: Perhaps should do proof-checking on the inode number using pathname
*  lookup and from file_info*/

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
		return 0;

	fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	fh_ptr->meta_cache_locked = TRUE;
	meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat, NULL, NULL,
			0, fh_ptr->meta_cache_ptr);

	/* Decide the true maximum bytes to read */
	/* If read request will exceed the size of the file, need to
	*  reduce the size of request */
	if (temp_stat.st_size < (offset+size_org))
		size = (temp_stat.st_size - offset);
	else
		size = size_org;

	if (size <= 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		return 0;
	}

	total_bytes_read = 0;

	/* Decide the block indices for the first byte and last byte of
	*  the read */
	/* Block indexing starts at zero */
	start_block = (offset / MAX_BLOCK_SIZE);
	end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

	/* Decide the page indices for the first byte and last byte of
	*  the read */
	/*Page indexing starts at zero*/
	start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE;

	/* Will need to change the entry page if it is not the current one */
	if (fh_ptr->cached_page_index != start_page)
		seek_page(fh_ptr, start_page);

	this_page_fpos = fh_ptr->cached_filepos;
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

	/* Read data from each block involved */
	for (block_index = start_block; block_index <= end_block;
				block_index++) {
		sem_wait(&(fh_ptr->block_sem));
		fill_zeros = FALSE;
		while (fh_ptr->opened_block != block_index) {
			if (fh_ptr->opened_block != -1) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->opened_block = -1;
			}

			read_lookup_meta(fh_ptr, &temppage, this_page_fpos);

			read_wait_full_cache(&temppage, entry_index, fh_ptr,
				this_page_fpos);

			read_prefetch_cache(&temppage, entry_index,
				temp_stat.st_ino, block_index, this_page_fpos);

			switch ((temppage).block_entries[entry_index].status) {
			case ST_NONE:
			case ST_TODELETE:
				fill_zeros = TRUE;
				break;
			case ST_LDISK:
			case ST_BOTH:
			case ST_LtoC:
				fill_zeros = FALSE;
				break;
			case ST_CLOUD:
			case ST_CtoL:
				/*Download from backend */
				read_fetch_backend(temp_stat.st_ino,
					block_index, fh_ptr, &temppage,
					this_page_fpos, entry_index);

				fill_zeros = FALSE;
				break;
			default:
				break;
			}

			if ((fill_zeros != TRUE) && (fh_ptr->opened_block !=
					block_index)) {
				fetch_block_path(thisblockpath,
						temp_stat.st_ino, block_index);

				fh_ptr->blockfptr = fopen(thisblockpath, "r+");
				if (fh_ptr->blockfptr != NULL) {
					setbuf(fh_ptr->blockfptr, NULL);
					fh_ptr->opened_block = block_index;
				} else {
				/* Some exception that block file is deleted in
				*  the middle of the status check*/
					printf("Debug read: cannot open block file. Perhaps replaced?\n");
					fh_ptr->opened_block = -1;
				}
			} else {
				break;
			}
		}

		if (fill_zeros != TRUE)
			flock(fileno(fh_ptr->blockfptr), LOCK_SH);

		current_offset = (offset+total_bytes_read) % MAX_BLOCK_SIZE;
		target_bytes_read = MAX_BLOCK_SIZE - current_offset;

		/*Do not need to read that much*/
		if ((size - total_bytes_read) < target_bytes_read)
			target_bytes_read = size - total_bytes_read;

		if (fill_zeros != TRUE) {
			fseek(fh_ptr->blockfptr, current_offset, SEEK_SET);
			this_bytes_read = fread(&buf[total_bytes_read],
					sizeof(char), target_bytes_read,
							fh_ptr->blockfptr);
			if (this_bytes_read < target_bytes_read) {
				/*Need to pad zeros*/
				printf("Short reading? %ld %d\n",
					current_offset, total_bytes_read +
							this_bytes_read);
				memset(&buf[total_bytes_read + this_bytes_read],
					 0, sizeof(char) *
					(target_bytes_read - this_bytes_read));
				this_bytes_read = target_bytes_read;
			}
		} else {
			printf("Padding zeros? %ld %d\n", current_offset,
					total_bytes_read);
			this_bytes_read = target_bytes_read;
			memset(&buf[total_bytes_read], 0, sizeof(char) *
							target_bytes_read);
		}

		total_bytes_read += this_bytes_read;

		if (fill_zeros != TRUE)
			flock(fileno(fh_ptr->blockfptr), LOCK_UN);

		sem_post(&(fh_ptr->block_sem));

		/*Terminate if cannot write as much as we want*/
		if (this_bytes_read < target_bytes_read)
			break;

		/*If this is not the last block, need to advance one more*/
		if (block_index < end_block)
			read_advance_block(&entry_index, fh_ptr,
						&this_page_fpos);
	}

	if (total_bytes_read > 0) {
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		fh_ptr->meta_cache_locked = TRUE;

		/*Update and flush file meta*/

		meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat,
					NULL, NULL, 0, fh_ptr->meta_cache_ptr);

		if (total_bytes_read > 0)
			temp_stat.st_atime = time(NULL);

		meta_cache_update_file_data(fh_ptr->thisinode, &temp_stat,
					NULL, NULL, 0, fh_ptr->meta_cache_ptr);

		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	}

	return total_bytes_read;
}

/* Helper function for write operation. Will wait on cache full and wait
*  until cache is not full. */
int write_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, long long entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos)
{
	while (((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
		((temppage->block_entries[entry_index]).status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			/*Sleep if cache already full*/
			sem_post(&(fh_ptr->block_sem));
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

			printf("debug write waiting on full cache\n");
			sleep_on_cache_full();
			/*Re-read status*/
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			fh_ptr->meta_cache_locked = TRUE;

			sem_wait(&(fh_ptr->block_sem));
			meta_cache_lookup_file_data(fh_ptr->thisinode,
				NULL, NULL, temppage, this_page_fpos,
				fh_ptr->meta_cache_ptr);
		} else {
			break;
		}
	}

	return 0;
}

/* Helper function for the write operation. Will fetch a block from backend. */
int _write_fetch_backend(ino_t this_inode, long long bindex, FH_ENTRY *fh_ptr,
		BLOCK_ENTRY_PAGE *tpage, off_t page_fpos, long long eindex)
{
	char thisblockpath[400];
	struct stat tempstat2;

	fetch_block_path(thisblockpath, this_inode, bindex);
	fh_ptr->blockfptr = fopen(thisblockpath, "a+");
	fclose(fh_ptr->blockfptr);
	fh_ptr->blockfptr = fopen(thisblockpath, "r+");
	setbuf(fh_ptr->blockfptr, NULL);
	flock(fileno(fh_ptr->blockfptr), LOCK_EX);
	meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, tpage,
					page_fpos, fh_ptr->meta_cache_ptr);

	if (((tpage->block_entries[eindex]).status == ST_CLOUD) ||
		((tpage->block_entries[eindex]).status == ST_CtoL)) {
		if ((tpage->block_entries[eindex]).status == ST_CLOUD) {
			(tpage->block_entries[eindex]).status = ST_CtoL;
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
				NULL, tpage, page_fpos, fh_ptr->meta_cache_ptr);
		}
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

		fetch_from_cloud(fh_ptr->blockfptr, this_inode, bindex);
		/*Do not process cache update and stored_where change if block
		* is actually deleted by other ops such as truncate*/

		/*Re-read status*/
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		fh_ptr->meta_cache_locked = TRUE;
		meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL,
				tpage, page_fpos, fh_ptr->meta_cache_ptr);

		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_LDISK;
			setxattr(thisblockpath, "user.dirty", "T", 1, 0);
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
				NULL, tpage, page_fpos, fh_ptr->meta_cache_ptr);

			change_system_meta(0, tempstat2.st_size, 1);
		}
	} else {
		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_LDISK;
			setxattr(thisblockpath, "user.dirty", "T", 1, 0);
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
				NULL, tpage, page_fpos, fh_ptr->meta_cache_ptr);
		}
	}
	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
	fclose(fh_ptr->blockfptr);

	return 0;
}

/* Function for writing to a single block for write operation. Will fetch
*  block from backend if needed. */
size_t _write_block(const char *buf, size_t size, long long bindex,
			off_t offset, FH_ENTRY *fh_ptr, ino_t this_inode)
{
	long long current_page;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	off_t this_page_fpos;
	off_t old_cache_size, new_cache_size;
	size_t this_bytes_written;
	off_t nextfilepos, prevfilepos;
	long long entry_index;


	/* Decide the page index for block "bindex" */
	/*Page indexing starts at zero*/
	current_page = bindex / MAX_BLOCK_ENTRIES_PER_PAGE;

	/* Find the offset of the page if it is not cached */
	if (fh_ptr->cached_page_index != current_page)
		seek_page(fh_ptr, current_page);

	this_page_fpos = fh_ptr->cached_filepos;

	entry_index = bindex % MAX_BLOCK_ENTRIES_PER_PAGE;

	fetch_block_path(thisblockpath, this_inode, bindex);
	sem_wait(&(fh_ptr->block_sem));

	/* Check if we can reuse cached block */
	if (fh_ptr->opened_block != bindex) {
		/* If the cached block is not the one we are writing to,
		*  close the one already opened. */
		if (fh_ptr->opened_block != -1) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
			NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);

		write_wait_full_cache(&temppage, entry_index, fh_ptr,
							this_page_fpos);

		switch ((temppage).block_entries[entry_index].status) {
		case ST_NONE:
		case ST_TODELETE:
			 /*If not stored anywhere, make it on local disk*/
			fh_ptr->blockfptr = fopen(thisblockpath, "a+");
			fclose(fh_ptr->blockfptr);
			(temppage).block_entries[entry_index].status = ST_LDISK;
			setxattr(thisblockpath, "user.dirty", "T", 1, 0);
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
					NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);

			change_system_meta(0, 0, 1);
			break;
		case ST_LDISK:
			break;
		case ST_BOTH:
		case ST_LtoC:
			(temppage).block_entries[entry_index].status = ST_LDISK;
			setxattr(thisblockpath, "user.dirty", "T", 1, 0);
			meta_cache_update_file_data(fh_ptr->thisinode, NULL,
					NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);
			break;
		case ST_CLOUD:
		case ST_CtoL:
			/*Download from backend */
			_write_fetch_backend(this_inode, bindex, fh_ptr,
					&temppage, this_page_fpos, entry_index);
			break;
		default:
			break;
		}

		fh_ptr->blockfptr = fopen(thisblockpath, "r+");
		setbuf(fh_ptr->blockfptr, NULL);
		fh_ptr->opened_block = bindex;
	}
	flock(fileno(fh_ptr->blockfptr), LOCK_EX);

	old_cache_size = check_file_size(thisblockpath);
	if (offset > old_cache_size) {
		printf("Debug write: cache block size smaller than starting offset. Extending\n");
		ftruncate(fileno(fh_ptr->blockfptr), offset);
	}

	fseek(fh_ptr->blockfptr, offset, SEEK_SET);
	this_bytes_written = fwrite(buf, sizeof(char), size, fh_ptr->blockfptr);

	new_cache_size = check_file_size(thisblockpath);

	if (old_cache_size != new_cache_size)
		change_system_meta(0, new_cache_size - old_cache_size, 0);

	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
	sem_post(&(fh_ptr->block_sem));

	return this_bytes_written;
}

/************************************************************************
*
* Function name: hfuse_write
*        Inputs: const char *path, char *buf, size_t size, off_t offset,
*                struct fuse_file_info *file_info
*       Summary: Write "size" bytes to the file "path", starting from
*                "offset", from the buffer pointed by "buf". File handle
*                is provided by the structure in "file_info".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_write(const char *path, const char *buf, size_t size, off_t offset,
			struct fuse_file_info *file_info)
{
	FH_ENTRY *fh_ptr;
	long long start_block, end_block;
	long long block_index;
	size_t total_bytes_written;
	size_t this_bytes_written;
	off_t current_offset;
	size_t target_bytes_written;
	struct stat temp_stat;

/* TODO: Perhaps should do proof-checking on the inode number using pathname
*  lookup and from file_info*/

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
		return 0;

	if (size <= 0)
		return 0;

	/*Sleep if cache already full*/
	if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT)
		sleep_on_cache_full();

	total_bytes_written = 0;

	/* Decide the block indices for the first byte and last byte of
	*  the write */
	/* Block indexing starts at zero */
	start_block = (offset / MAX_BLOCK_SIZE);
	end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

	fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	fh_ptr->meta_cache_locked = TRUE;

	for (block_index = start_block; block_index <= end_block;
							block_index++) {
		current_offset = (offset+total_bytes_written) % MAX_BLOCK_SIZE;

		target_bytes_written = MAX_BLOCK_SIZE - current_offset;

		/* If do not need to write that much */
		if ((size - total_bytes_written) < target_bytes_written)
			target_bytes_written = size - total_bytes_written;

		this_bytes_written = _write_block(&buf[total_bytes_written],
				target_bytes_written, block_index,
				current_offset, fh_ptr, fh_ptr->thisinode);

		total_bytes_written += this_bytes_written;

		/*Terminate if cannot write as much as we want*/
		if (this_bytes_written < target_bytes_written)
			break;
	}

	/*Update and flush file meta*/

	meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat, NULL, NULL,
						0, fh_ptr->meta_cache_ptr);

	if (temp_stat.st_size < (offset + total_bytes_written)) {
		change_system_meta((long long) ((offset + total_bytes_written)
						- temp_stat.st_size), 0, 0);

		temp_stat.st_size = (offset + total_bytes_written);
		temp_stat.st_blocks = (temp_stat.st_size+511) / 512;
	}

	if (total_bytes_written > 0)
		temp_stat.st_mtime = time(NULL);

	meta_cache_update_file_data(fh_ptr->thisinode, &temp_stat, NULL, NULL,
						0, fh_ptr->meta_cache_ptr);

	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	return total_bytes_written;
}

/************************************************************************
*
* Function name: hfuse_statfs
*        Inputs: const char *path, struct statvfs *buf
*       Summary: Lookup the filesystem status. "path" is not used now.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_statfs(const char *path, struct statvfs *buf)
{
	/*Prototype is linux statvfs call*/
	sem_wait(&(hcfs_system->access_sem));
	buf->f_bsize = 4096;
	buf->f_frsize = 4096;
	if (hcfs_system->systemdata.system_size > (50*powl(1024, 3)))
		buf->f_blocks = (2*hcfs_system->systemdata.system_size) / 4096;
	else
		buf->f_blocks = (25*powl(1024, 2));

	buf->f_bfree = buf->f_blocks -
				((hcfs_system->systemdata.system_size) / 4096);
	if (buf->f_bfree < 0)
		buf->f_bfree = 0;
	buf->f_bavail = buf->f_bfree;
	sem_post(&(hcfs_system->access_sem));

	super_block_share_locking();
	if (sys_super_block->head.num_active_inodes > 1000000)
		buf->f_files = (sys_super_block->head.num_active_inodes * 2);
	else
		buf->f_files = 2000000;

	buf->f_ffree = buf->f_files - sys_super_block->head.num_active_inodes;
	if (buf->f_ffree < 0)
		buf->f_ffree = 0;
	buf->f_favail = buf->f_ffree;
	super_block_share_release();
	buf->f_namemax = 256;

	return 0;
}

/************************************************************************
*
* Function name: hfuse_flush
*        Inputs: const char *path, struct fuse_file_info *file_info
*       Summary: Flush the file content. Not used now as cache mode is not
*                write back.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_flush(const char *path, struct fuse_file_info *file_info)
{
	return 0;
}

/************************************************************************
*
* Function name: hfuse_release
*        Inputs: const char *path, struct fuse_file_info *file_info
*       Summary: Close the file handle pointed by "file_info".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_release(const char *path, struct fuse_file_info *file_info)
{
	ino_t thisinode;
	int ret_code;

	thisinode = lookup_pathname(path, &ret_code);
	if (file_info->fh < 0)
		return -EBADF;

	if (file_info->fh >= MAX_OPEN_FILE_ENTRIES)
		return -EBADF;

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
		return -EBADF;

	if (system_fh_table.entry_table[file_info->fh].thisinode != thisinode)
		return -EBADF;

	close_fh(file_info->fh);
	return 0;
}

/************************************************************************
*
* Function name: hfuse_fsync
*        Inputs: const char *path, int isdatasync,
*                struct fuse_file_info *file_info
*       Summary: Conduct "fsync". Do nothing now (see hfuse_flush).
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_fsync(const char *path, int isdatasync,
					struct fuse_file_info *file_info)
{
	return 0;
}

/************************************************************************
*
* Function name: hfuse_opendir
*        Inputs: const char *path, struct fuse_file_info *file_info
*       Summary: Check permission and open directory for access.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_opendir(const char *path, struct fuse_file_info *file_info)
{
	/*TODO: Need to check for access rights */
	return 0;
}

/************************************************************************
*
* Function name: hfuse_readdir
*        Inputs: const char *path, void *buf, fuse_fill_dir_t filler,
*                off_t offset, struct fuse_file_info *file_info
*       Summary: Read directory content starting from "offset". This
*                implementation can return partial results and continue
*                reading in follow-up calls.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *file_info)
{
	/* Now will read partial entries and deal with others later */
	ino_t this_inode;
	int count;
	off_t thisfile_pos;
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE temp_page;
	struct stat tempstat;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	long countn;
	off_t nextentry_pos;
	int page_start;

	gettimeofday(&tmp_time1, NULL);

/*TODO: Need to include symlinks*/
	fprintf(stderr, "DEBUG readdir entering readdir\n");

/* TODO: the following can be skipped if we can use some file handle from
*  file_info input */
	this_inode = lookup_pathname(path, &ret_code);

	if (this_inode == 0)
		return ret_code;

	body_ptr = meta_cache_lock_entry(this_inode);
	meta_cache_lookup_dir_data(this_inode, &tempstat, &tempmeta, NULL,
								body_ptr);

	page_start = 0;
	if (offset >= MAX_DIR_ENTRIES_PER_PAGE) {
		thisfile_pos = offset / (MAX_DIR_ENTRIES_PER_PAGE + 1);
		page_start = offset % (MAX_DIR_ENTRIES_PER_PAGE + 1);
		printf("readdir starts at offset %ld, entry number %d\n",
						thisfile_pos, page_start);
		if (body_ptr->meta_opened == FALSE)
			meta_cache_open_file(body_ptr);
		meta_cache_drop_pages(body_ptr);
	} else {
		thisfile_pos = tempmeta.tree_walk_list_head;

		if (tempmeta.total_children > (MAX_DIR_ENTRIES_PER_PAGE-2)) {
			if (body_ptr->meta_opened == FALSE)
				meta_cache_open_file(body_ptr);
			meta_cache_drop_pages(body_ptr);
		}
	}

	countn = 0;
	while (thisfile_pos != 0) {
		printf("Now %dth iteration\n", countn);
		countn++;
		memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
		temp_page.this_page_pos = thisfile_pos;
		if ((tempmeta.total_children <= (MAX_DIR_ENTRIES_PER_PAGE-2))
							&& (page_start == 0)) {
			meta_cache_lookup_dir_data(this_inode, NULL, NULL,
							&temp_page, body_ptr);
		} else {
			fseek(body_ptr->fptr, thisfile_pos, SEEK_SET);
			fread(&temp_page, sizeof(DIR_ENTRY_PAGE), 1,
								body_ptr->fptr);
		}

		for (count = page_start; count < temp_page.num_entries;
								count++) {
			tempstat.st_ino = temp_page.dir_entries[count].d_ino;
			if (temp_page.dir_entries[count].d_type == D_ISDIR)
				tempstat.st_mode = S_IFDIR;
			if (temp_page.dir_entries[count].d_type == D_ISREG)
				tempstat.st_mode = S_IFREG;
			nextentry_pos = temp_page.this_page_pos *
				(MAX_DIR_ENTRIES_PER_PAGE + 1) + (count+1);
			if (filler(buf, temp_page.dir_entries[count].d_name,
						&tempstat, nextentry_pos)) {
				meta_cache_unlock_entry(body_ptr);
				printf("Readdir breaks, next offset %ld, file pos %ld, entry %d\n", nextentry_pos, temp_page.this_page_pos, (count+1));
				return 0;
			}
		}
		page_start = 0;
		thisfile_pos = temp_page.tree_walk_next;
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	gettimeofday(&tmp_time2, NULL);

	printf("readdir elapse %f\n", (tmp_time2.tv_sec - tmp_time1.tv_sec) + 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	return 0;
}

/************************************************************************
*
* Function name: hfuse_releasedir
*        Inputs: const char *path, struct fuse_file_info *file_info
*       Summary: Close opened directory. Do nothing now.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int hfuse_releasedir(const char *path, struct fuse_file_info *file_info)
{
	return 0;
}

/* A prototype for reporting the current stat of HCFS */
void reporter_module(void)
{
	int fd, fd1, size_msg, msg_len;
	struct sockaddr_un addr;
	char buf[4096];

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	unlink(addr.sun_path);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	bind(fd, &addr, sizeof(struct sockaddr_un));

	listen(fd, 10);
	while (TRUE) {
		fd1 = accept(fd, NULL, NULL);
		msg_len = 0;
		while (TRUE) {
			size_msg = recv(fd1, &buf[msg_len], 512, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len > 3000)
				break;
			if (buf[msg_len-1] == 0)
				break;
		}
		buf[msg_len] = 0;
		if (strcmp(buf, "terminate") == 0)
			break;
		if (strcmp(buf, "stat") == 0) {
			buf[0] = 0;
			sem_wait(&(hcfs_system->access_sem));
			sprintf(buf, "%lld %lld %lld",
				hcfs_system->systemdata.system_size,
				hcfs_system->systemdata.cache_size,
				hcfs_system->systemdata.cache_blocks);
			sem_post(&(hcfs_system->access_sem));
			printf("debug stat hcfs %s\n", buf);
			send(fd1, buf, strlen(buf)+1, 0);
		}
	}
	return;
}

/************************************************************************
*
* Function name: hfuse_init
*        Inputs: struct fuse_file_info *file_info
*       Summary: Initiate a FUSE mount
*  Return value: Pointer to the user data of this mount.
*
*************************************************************************/
void *hfuse_init(struct fuse_conn_info *conn)
{
	struct fuse_context *temp_context;

	temp_context = fuse_get_context();

	printf("Data passed in is %s\n", (char *) temp_context->private_data);

	pthread_attr_init(&prefetch_thread_attr);
	pthread_attr_setdetachstate(&prefetch_thread_attr,
						PTHREAD_CREATE_DETACHED);
	pthread_create(&reporter_thread, NULL, (void *)reporter_module, NULL);
	init_meta_cache_headers();
	/* return ((void*) sys_super_block); */
	return temp_context->private_data;
}

/************************************************************************
*
* Function name: hfuse_destroy
*        Inputs: void *private_data
*       Summary: Destroy a FUSE mount
*  Return value: None
*
*************************************************************************/
void hfuse_destroy(void *private_data)
{
	int dl_count;

	release_meta_cache_headers();
	sync();
	for (dl_count = 0; dl_count < MAX_DOWNLOAD_CURL_HANDLE; dl_count++)
		hcfs_destroy_backend(download_curl_handles[dl_count].curl);

	fclose(logfptr);

	return;
}

/************************************************************************
*
* Function name: hfuse_access
*        Inputs: const char *path, int mode
*       Summary: Checks the permission for object "path" against "mode".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
static int hfuse_access(const char *path, int mode)
{
	/*TODO: finish this*/
	return 0;
}

/* Specify the functions used for the FUSE operations */
static struct fuse_operations hfuse_ops = {
	.getattr = hfuse_getattr,
	.mknod = hfuse_mknod,
	.mkdir = hfuse_mkdir,
	.readdir = hfuse_readdir,
	.opendir = hfuse_opendir,
	.access = hfuse_access,
	.rename = hfuse_rename,
	.open = hfuse_open,
	.release = hfuse_release,
	.write = hfuse_write,
	.read = hfuse_read,
	.init = hfuse_init,
	.destroy = hfuse_destroy,
	.releasedir = hfuse_releasedir,
	.chown = hfuse_chown,
	.chmod = hfuse_chmod,
	.utimens = hfuse_utimens,
	.truncate = hfuse_truncate,
	.flush = hfuse_flush,
	.fsync = hfuse_fsync,
	.unlink = hfuse_unlink,
	.rmdir = hfuse_rmdir,
	.statfs = hfuse_statfs,
};

/*
char **argv_alt;
int argc_alt;
pthread_t alt_mount;
void run_alt(void)
 {
	fuse_main(argc_alt, argv_alt, &hfuse_ops, (void *)argv_alt[1]);
	return;
 }
*/

/* Initiate FUSE */
int hook_fuse(int argc, char **argv)
{
/*
	int count;
	int ret_val;

	argv_alt = malloc(sizeof(char *)*argc);
	argc_alt = argc;
	for (count = 0; count<argc; count++)
	 {
		argv_alt[count] = malloc(strlen(argv[count])+10);
		strcpy(argv_alt[count], argv[count]);
		if (count == 1)
		 strcat(argv_alt[count], "_alt");
	 }
	pthread_create(&alt_mount, NULL, (void *) run_alt, NULL);
	ret_val = fuse_main(argc, argv, &hfuse_ops, (void *)argv[1]);

	return ret_val;
*/
	return fuse_main(argc, argv, &hfuse_ops, NULL);
}

/* Operations to implement
int hfuse_readlink(const char *path, char *buf, size_t buf_size);
int hfuse_symlink(const char *oldpath, const char *newpath);
int hfuse_link(const char *oldpath, const char *newpath);
int hfuse_setxattr(const char *path, const char *name, const char *value,
						size_t size, int flags);
int hfuse_getxattr(const char *path, const char *name, char *value,
							size_t size);
int hfuse_listxattr(const char *path, char *list, size_t size);
int hfuse_removexattr(const char *path, const char *name);
int hfuse_create(const char *path, mode_t mode,
					struct fuse_file_info *file_info);
*/
