/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseop.c
* Abstract: The c source code file for the main FUSE operations for HCFS.
*           All fuse functions now use fuse_reply_xxxx to pass back data
*           and returned status, and the functions themselves do not
*           return anything (void return type).
*
* Revision History
* 2015/2/2 Jiahong added header for this file, and revising coding style.
*          File is renamed from hfuseops.c to fuseop.c
* 2015/2/3~2/5 (Jiahong) Restructure hfuse_truncate, hfuse_write, hfuse_read
*              functions
* 2015/2/11 Jiahong added inclusion of hcfs_cacheops.h
* 2015/2/12 Jiahong added inclusion of hcfs_fromcloud.h
* 2015/3/2~3 Jiahong revised rename function to allow existing file as target.
* 2015/3/12 Jiahong restructure hfuse_read
* 2015/4/30 ~  Jiahong changing to FUSE low-level interface
* 2015/5/20 Jiahong Adding permission checking for operations
* 2015/5/25, 5/26  Jiahong adding error handling
* 2015/6/29 Kewei finished xattr operations.
* 2015/7/7 Kewei began to add ops about symbolic link
*
**************************************************************************/

#define FUSE_USE_VERSION 29
#define _GNU_SOURCE

#include "fuseop.h"

#include <time.h>
#include <math.h>

#ifdef STAT_VFS_H
#include STAT_VFS_H
#else
#include <sys/statvfs.h>
#endif

#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#ifndef _ANDROID_ENV_
#include <attr/xattr.h>
#endif

/* Headers from the other libraries */
#include <fuse/fuse_lowlevel.h>
#include <fuse/fuse_common.h>
#include <fuse/fuse_opt.h>

#include "global.h"
#include "file_present.h"
#include "utils.h"
#include "super_block.h"
#include "params.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "meta_mem_cache.h"
#include "filetables.h"
#include "hcfs_cacheops.h"
#include "hcfs_fromcloud.h"
#include "logger.h"
#include "macro.h"
#include "xattr_ops.h"
#include "mount_manager.h"
#include "api_interface.h"
#include "metaops.h"
#include "lookup_count.h"
#include "FS_manager.h"
#include "path_reconstruct.h"

extern SYSTEM_CONF_STRUCT system_config;

/* Steps for allowing opened files / dirs to be accessed after deletion

	1. in lookup_count, add a field "to_delete". rmdir, unlink
will first mark this as true and if in forget() the count is dropped
to zero, the inode is deleted.
	2. to allow inode deletion fixes due to system crashing, a subfolder
will be created so that the inode number of inodes to be deleted can be
touched here, and removed when actually deleted.
	3. in lookup_decrease, should delete nodes when lookup drops
to zero (to save space in the long run).
	4. in unmount, can pick either scanning lookup table for inodes
to delete or list the folder.
*/
/* TODO: for mechanisms that needs timer, use per-process or per-thread
cpu clock instead of using current time to avoid time changes due to
ntpdate / ntpd or manual changes*/

/* TODO: Go over the details involving S_ISUID, S_ISGID, and linux capability
	problems (involving chmod, chown, etc) */

/* TODO: system acl. System acl is set in extended attributes. */

/* TODO: Will need to implement rollback or error marking when ops failed*/

/* TODO: Pending design for a single cache device, and use pread/pwrite to
   allow multiple threads to access cache concurrently without the need for
   file handles */

/* TODO: Need to be able to perform actual operations according to type of
	folders (cached, non-cached, local) */
/* TODO: Push actual operations to other source files, especially no actual
	file handling in this file */
/* TODO: Multiple paths for read / write / other ops for different folder
	policies. Policies to be determined at file or dir open. */
/* TODO: Check why du in HCFS and in ext4 behave differently in timestamp
	changes */

/* Helper function for setting timestamp(s) to the current time, in
nanosecond precision.
   "mode" is the bit-wise OR of ATIME, MTIME, CTIME.
*/
void set_timestamp_now(struct stat *thisstat, char mode)
{
	struct timespec timenow;
	int ret;

	ret = clock_gettime(CLOCK_REALTIME, &timenow);

	write_log(10, "Current time %s, ret %d\n",
		ctime(&(timenow.tv_sec)), ret);
	if (mode & ATIME) {
		thisstat->st_atime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->st_atim), &timenow,
			sizeof(struct timespec));
#endif
	}

	if (mode & MTIME) {
		thisstat->st_mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->st_mtim), &timenow,
			sizeof(struct timespec));
#endif
	}

	if (mode & CTIME) {
		thisstat->st_ctime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(thisstat->st_ctim), &timenow,
			sizeof(struct timespec));
#endif
	}
}
/* Helper function for checking permissions.
   Inputs: fuse_req_t req, struct stat *thisstat, char mode
     Note: Mode is bitwise ORs of read, write, exec (4, 2, 1)
*/
int check_permission(fuse_req_t req, struct stat *thisstat, char mode)
{
	struct fuse_ctx *temp_context;
	gid_t *tmp_list, tmp1_list[10];
	int num_groups, count;
	char is_in_group;

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL)
		return -ENOMEM;

	if (temp_context->uid == 0)  /*If this is the root grant any req */
		return 0;

	/* First check owner permission */
	if (temp_context->uid == thisstat->st_uid) {
		if (mode & 4)
			if (!(thisstat->st_mode & S_IRUSR))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->st_mode & S_IWUSR))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->st_mode & S_IXUSR))
				return -EACCES;
		return 0;
	}

	/* Check group permission */
	if (temp_context->gid == thisstat->st_gid) {
		if (mode & 4)
			if (!(thisstat->st_mode & S_IRGRP))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->st_mode & S_IWGRP))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->st_mode & S_IXGRP))
				return -EACCES;
		return 0;
	}

	/* Check supplementary group ID */

	num_groups = fuse_req_getgroups(req, 10, tmp1_list);

	if (num_groups <= 10) {
		tmp_list = tmp1_list;
	} else {
		tmp_list = malloc(sizeof(gid_t) * num_groups);
		if (tmp_list == NULL)
			return -ENOMEM;
		num_groups = fuse_req_getgroups(req,
					sizeof(gid_t) * num_groups, tmp_list);
	}

	if (num_groups < 0) {
		write_log(5,
			"Debug check permission getgroups failed, skipping\n");
		num_groups = 0;
	}

	is_in_group = FALSE;
	write_log(10, "Debug permission number of groups %d\n", num_groups);
	for (count = 0; count < num_groups; count++) {
		write_log(10, "group gid %d, %d\n", tmp_list[count],
					thisstat->st_gid);
		if (tmp_list[count] == thisstat->st_gid) {
			is_in_group = TRUE;
			break;
		}
	}

	if (is_in_group == TRUE) {
		if (mode & 4)
			if (!(thisstat->st_mode & S_IRGRP))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->st_mode & S_IWGRP))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->st_mode & S_IXGRP))
				return -EACCES;
		return 0;
	}

	/* Check others */

	if (mode & 4)
		if (!(thisstat->st_mode & S_IROTH))
			return -EACCES;
	if (mode & 2)
		if (!(thisstat->st_mode & S_IWOTH))
			return -EACCES;
	if (mode & 1)
		if (!(thisstat->st_mode & S_IXOTH))
			return -EACCES;
	return 0;
}

/* Check permission routine for ll_access only */
int check_permission_access(fuse_req_t req, struct stat *thisstat, int mode)
{
	struct fuse_ctx *temp_context;
	gid_t *tmp_list, tmp1_list[10];
	int num_groups, count;
	char is_in_group;

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL)
		return -ENOMEM;

	/*If this is the root check if exec is set for any for reg files*/
	if (temp_context->uid == 0) {
		if ((S_ISREG(thisstat->st_mode)) && (mode & X_OK)) {
			if (!(thisstat->st_mode &
				(S_IXUSR | S_IXGRP | S_IXOTH)))
				return -EACCES;
		}
		return 0;
	}

	/* First check owner permission */
	if (temp_context->uid == thisstat->st_uid) {
		if (mode & R_OK)
			if (!(thisstat->st_mode & S_IRUSR))
				return -EACCES;
		if (mode & W_OK)
			if (!(thisstat->st_mode & S_IWUSR))
				return -EACCES;
		if (mode & X_OK)
			if (!(thisstat->st_mode & S_IXUSR))
				return -EACCES;
		return 0;
	}

	/* Check group permission */
	if (temp_context->gid == thisstat->st_gid) {
		if (mode & R_OK)
			if (!(thisstat->st_mode & S_IRGRP))
				return -EACCES;
		if (mode & W_OK)
			if (!(thisstat->st_mode & S_IWGRP))
				return -EACCES;
		if (mode & X_OK)
			if (!(thisstat->st_mode & S_IXGRP))
				return -EACCES;
		return 0;
	}

	/* Check supplementary group ID */

	num_groups = fuse_req_getgroups(req, 10, tmp1_list);

	if (num_groups <= 10) {
		tmp_list = tmp1_list;
	} else {
		tmp_list = malloc(sizeof(gid_t) * num_groups);
		if (tmp_list == NULL)
			return -ENOMEM;
		num_groups = fuse_req_getgroups(req,
					sizeof(gid_t) * num_groups, tmp_list);
	}

	if (num_groups < 0) {
		write_log(5,
			"Debug check permission getgroups failed, skipping\n");
		num_groups = 0;
	}

	is_in_group = FALSE;
	write_log(10, "Debug permission number of groups %d\n", num_groups);
	for (count = 0; count < num_groups; count++) {
		write_log(10, "group gid %d, %d\n", tmp_list[count],
					thisstat->st_gid);
		if (tmp_list[count] == thisstat->st_gid) {
			is_in_group = TRUE;
			break;
		}
	}

	if (is_in_group == TRUE) {
		if (mode & R_OK)
			if (!(thisstat->st_mode & S_IRGRP))
				return -EACCES;
		if (mode & W_OK)
			if (!(thisstat->st_mode & S_IWGRP))
				return -EACCES;
		if (mode & X_OK)
			if (!(thisstat->st_mode & S_IXGRP))
				return -EACCES;
		return 0;
	}

	/* Check others */

	if (mode & R_OK)
		if (!(thisstat->st_mode & S_IROTH))
			return -EACCES;
	if (mode & W_OK)
		if (!(thisstat->st_mode & S_IWOTH))
			return -EACCES;
	if (mode & X_OK)
		if (!(thisstat->st_mode & S_IXOTH))
			return -EACCES;
	return 0;
}


int is_member(fuse_req_t req, gid_t this_gid, gid_t target_gid)
{
	gid_t *tmp_list, tmp1_list[10];
	int num_groups, count;
	char is_in_group;


	if (this_gid == target_gid)
		return 1;

	/* Check supplementary group ID */

	num_groups = fuse_req_getgroups(req, 10, tmp1_list);

	if (num_groups <= 10) {
		tmp_list = tmp1_list;
	} else {
		tmp_list = malloc(sizeof(gid_t) * num_groups);
		if (tmp_list == NULL)
			return -ENOMEM;
		num_groups = fuse_req_getgroups(req,
					sizeof(gid_t) * num_groups, tmp_list);
	}

	if (num_groups < 0) {
		if (tmp_list != tmp1_list)
			free(tmp_list);
		return 0;
	}

	is_in_group = FALSE;
	for (count = 0; count < num_groups; count++) {
		if (tmp_list[count] == target_gid) {
			is_in_group = TRUE;
			break;
		}
	}

	if (tmp_list != tmp1_list)
		free(tmp_list);

	if (is_in_group == TRUE)
		return 1;

	return 0;
}

/* Helper function for translating from FUSE inode number to real one */
ino_t real_ino(fuse_req_t req, fuse_ino_t ino)
{
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Root inode is %"FMT_INO_T"\n", tmpptr->f_ino);

	if (ino == 1)
		return tmpptr->f_ino;
	else
		return (ino_t) ino;
}

#ifdef _ANDROID_ENV_
/* Internal function for generating the ownership / permission for
Android external storage */
uid_t lookup_pkg(char *pkgname)
{
/* TODO: This is now a fake. Need to link to real function */
	return 1010;
}

int _rewrite_stat(MOUNT_T *tmpptr, struct stat *thisstat)
{
	int ret, errcode;
	char *tmppath;
	mode_t tmpmask, newpermission;
	char *tmptok, *tmptoksave, *tmptok_prev;
	int count;
	uid_t tmpuid;

	tmppath = NULL;
	tmptok_prev = NULL;
	tmptok = NULL;
	tmptoksave = NULL;
	ret = construct_path(tmpptr->vol_path_cache, thisstat->st_ino,
				&tmppath);
	if (ret < 0) {
		if (tmppath != NULL)
			free(tmppath);
		errcode = ret;
		goto errcode_handle;
	}
	write_log(10, "Debug path lookup %s\n", tmppath);

	for (count = 0; count < 4; count++) {
		if (count == 0)
			tmptok = strtok_r(tmppath, "/", &tmptoksave);
		else
			tmptok = strtok_r(NULL, "/", &tmptoksave);

		write_log(10, "%s, %s\n", tmptok, tmptok_prev);
		if (tmptok == NULL) {
			switch (count) {
			case 0:
				/* Is root */
				thisstat->st_uid = 0;
				thisstat->st_gid = 1028;
				newpermission = 0770;
				break;
			case 1:
				/* Is /Android */
				if (!S_ISDIR(thisstat->st_mode)) {
					/* If not a directory */
					thisstat->st_uid = 0;
					thisstat->st_gid = 1028;
					newpermission = 0770;
				} else {
					thisstat->st_uid = 0;
					thisstat->st_gid = 1028;
					newpermission = 0771;
				}
				break;
			case 2:
				if (!S_ISDIR(thisstat->st_mode)) {
					/* If not a directory */
					thisstat->st_uid = 0;
					thisstat->st_gid = 1028;
					newpermission = 0770;
				} else {
					/* If a subfolder under /Android */
					thisstat->st_uid = 0;
					thisstat->st_gid = 1028;
					newpermission = 0771;
				}
				break;
			case 3:
				if (!S_ISDIR(thisstat->st_mode)) {
					/* If not a directory */
					thisstat->st_uid = 0;
					thisstat->st_gid = 1028;
					newpermission = 0770;
				} else {
					/* If this is a package folder */
					/* Need to lookup package uid */
					tmpuid = lookup_pkg(tmptok_prev);
					thisstat->st_uid = tmpuid;
					thisstat->st_gid = 1028;
					newpermission = 0770;
				}
				break;
			default:
				thisstat->st_uid = 0;
				thisstat->st_gid = 1028;
				newpermission = 0770;
				break;
			}
			break;
		}
		if (count == 3) {
			tmpuid = lookup_pkg(tmptok_prev);
			thisstat->st_uid = tmpuid;
			thisstat->st_gid = 1028;
			newpermission = 0770;
			break;
		}
		tmptok_prev = tmptok;
		if ((count == 0) && (strcmp(tmptok, "Android") != 0)) {
			/* Not under /Android */
			thisstat->st_uid = 0;
			thisstat->st_gid = 1028;
			newpermission = 0770;
			break;
		}
	}

	tmpmask = 0777;
	tmpmask = ~tmpmask;
	tmpmask = tmpmask & thisstat->st_mode;
	thisstat->st_mode = tmpmask | newpermission;
	free(tmppath);
	return 0;
errcode_handle:
	return errcode;
}
#endif
/************************************************************************
*
* Function name: hfuse_ll_getattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi
*       Summary: Read the stat of the inode "ino" and reply to FUSE
*
*************************************************************************/
static void hfuse_ll_getattr(fuse_req_t req, fuse_ino_t ino,
					struct fuse_file_info *fi)
{
	ino_t hit_inode;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;
	struct stat tmp_stat;
	MOUNT_T *tmpptr;

	write_log(10, "Debug getattr inode %ld\n", ino);
	hit_inode = real_ino(req, ino);

	write_log(10, "Debug getattr hit inode %"FMT_INO_T"\n", hit_inode);

	if (hit_inode > 0) {
		tmpptr = (MOUNT_T *) fuse_req_userdata(req);

		ret_code = fetch_inode_stat(hit_inode, &tmp_stat, NULL);
		if (ret_code < 0) {
			fuse_reply_err(req, -ret_code);
			return;
		}

#ifdef _ANDROID_ENV_
		if (tmpptr->volume_type == ANDROID_EXTERNAL) {
			if (tmpptr->vol_path_cache == NULL) {
				fuse_reply_err(req, EIO);
				return;
			}
			_rewrite_stat(tmpptr, &tmp_stat);
		}
#endif

		write_log(10, "Debug getattr return inode %"FMT_INO_T"\n",
				tmp_stat.st_ino);
		gettimeofday(&tmp_time2, NULL);

		write_log(10, "getattr elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));
		fuse_reply_attr(req, &tmp_stat, 0);
	} else {
		gettimeofday(&tmp_time2, NULL);

		write_log(10, "getattr elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));
		fuse_reply_err(req, ENOENT);
	}
}

/************************************************************************
*
* Function name: hfuse_ll_mknod
*        Inputs: fuse_req_t req, fuse_ino_t parent, const char *selfname,
*                mode_t mode, dev_t dev
*       Summary: Under inode "parent", create a regular file "selfname"
*                with the permission specified by mode. "dev" is ignored
*                as only regular file will be created.
*
*************************************************************************/
static void hfuse_ll_mknod(fuse_req_t req, fuse_ino_t parent,
		const char *selfname, mode_t mode, dev_t dev)
{
	ino_t self_inode, parent_inode;
	struct stat this_stat;
	mode_t self_mode;
	int ret_val;
	struct fuse_ctx *temp_context;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;
	struct fuse_entry_param tmp_param;
	struct stat parent_stat;
	unsigned long this_generation;
	MOUNT_T *tmpptr;

	write_log(10,
		"DEBUG parent %ld, name %s mode %d\n", parent, selfname, mode);
	gettimeofday(&tmp_time1, NULL);

	/* Reject if not creating a regular file */
	if (!S_ISREG(mode)) {
		fuse_reply_err(req, EPERM);
		return;
	}

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	parent_inode = real_ino(req, parent);

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	memset(&this_stat, 0, sizeof(struct stat));

	this_stat.st_size = 0;
	this_stat.st_blksize = MAX_BLOCK_SIZE;
	this_stat.st_blocks = 0;
	this_stat.st_dev = dev;
	this_stat.st_nlink = 1;

        self_mode = mode | S_IFREG;
        this_stat.st_mode = self_mode;

	/*Use the uid and gid of the fuse caller*/

	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	/* Use the current time for timestamps */
	set_timestamp_now(&this_stat, ATIME | MTIME | CTIME);

	self_inode = super_block_new_inode(&this_stat, &this_generation);

	/* If cannot get new inode number, error is ENOSPC */
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	this_stat.st_ino = self_inode;

	ret_code = mknod_update_meta(self_inode, parent_inode, selfname,
			&this_stat, this_generation, tmpptr->f_ino);

	/* TODO: May need to delete from super block and parent if failed. */
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}

	gettimeofday(&tmp_time2, NULL);

	write_log(10,
		"mknod elapse %f\n", (tmp_time2.tv_sec - tmp_time1.tv_sec)
		+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_generation;
	tmp_param.ino = (fuse_ino_t) self_inode;
	memcpy(&(tmp_param.attr), &this_stat, sizeof(struct stat));

	ret_code = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISREG);
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}
	ret_val = change_mount_stat(tmpptr, 0, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
	}

	fuse_reply_entry(req, &(tmp_param));
}

/************************************************************************
*
* Function name: hfuse_ll_mkdir
*        Inputs: fuse_req_t req, fuse_ino_t parent, const char *selfname,
*                mode_t mode
*       Summary: Create a subdirectory "selfname" under "parent" with the
*                permission specified by mode.
*
*************************************************************************/
static void hfuse_ll_mkdir(fuse_req_t req, fuse_ino_t parent,
				const char *selfname, mode_t mode)
{
	ino_t self_inode, parent_inode;
	struct stat this_stat;
	mode_t self_mode;
	int ret_val;
	struct fuse_ctx *temp_context;
	int ret_code;
	struct timeval tmp_time1, tmp_time2;
	struct fuse_entry_param tmp_param;
	struct stat parent_stat;
	unsigned long this_gen;
	MOUNT_T *tmpptr;

	gettimeofday(&tmp_time1, NULL);

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	parent_inode = real_ino(req, parent);

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	memset(&this_stat, 0, sizeof(struct stat));
	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	this_stat.st_nlink = 2; /*One pointed by the parent, another by self*/

        self_mode = mode | S_IFDIR;
        this_stat.st_mode = self_mode;

	/*Use the uid and gid of the fuse caller*/
	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	set_timestamp_now(&this_stat, ATIME | MTIME | CTIME);

	this_stat.st_size = 0;
	this_stat.st_blksize = MAX_BLOCK_SIZE;
	this_stat.st_blocks = 0;

	self_inode = super_block_new_inode(&this_stat, &this_gen);
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	this_stat.st_ino = self_inode;

	ret_code = mkdir_update_meta(self_inode, parent_inode,
			selfname, &this_stat, this_gen, tmpptr->f_ino);

	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_gen;
	tmp_param.ino = (fuse_ino_t) self_inode;
	memcpy(&(tmp_param.attr), &this_stat, sizeof(struct stat));

	ret_code = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISDIR);
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}
	ret_val = change_mount_stat(tmpptr, 0, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
	}

	fuse_reply_entry(req, &(tmp_param));

	gettimeofday(&tmp_time2, NULL);

	write_log(10, "mkdir elapse %f\n", (tmp_time2.tv_sec - tmp_time1.tv_sec)
		+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));
}

/************************************************************************
*
* Function name: hfuse_ll_unlink
*        Inputs: fuse_req_t req, fuse_ino_t parent_inode, const char *selfname
*       Summary: Delete the regular file specified by parent "parent_inode"
*                and name "selfname".
*
*************************************************************************/
void hfuse_ll_unlink(fuse_req_t req, fuse_ino_t parent,
			const char *selfname)
{
	ino_t parent_inode;
	int ret_val;
	DIR_ENTRY temp_dentry;
	struct stat parent_stat;
	MOUNT_T *tmpptr;

	parent_inode = real_ino(req, parent);

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = lookup_dir(parent_inode, selfname, &temp_dentry);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = unlink_update_meta(req, parent_inode, &temp_dentry);
	if (ret_val < 0)
		ret_val = -ret_val;

	fuse_reply_err(req, ret_val);
}

/************************************************************************
*
* Function name: hfuse_ll_rmdir
*        Inputs: fuse_req_t req, fuse_ino_t parent_inode, const char *selfname
*       Summary: Delete the directory specified by parent "parent_inode" and
*                name "selfname".
*
*************************************************************************/
void hfuse_ll_rmdir(fuse_req_t req, fuse_ino_t parent,
			const char *selfname)
{
	ino_t this_inode, parent_inode;
	int ret_val;
	DIR_ENTRY temp_dentry;
	struct stat parent_stat;
	MOUNT_T *tmpptr;

	parent_inode = real_ino(req, parent);
	write_log(10, "Debug rmdir: name %s, parent %"FMT_INO_T"\n", selfname,
			parent_inode);
	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	if (!strcmp(selfname, ".")) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	if (!strcmp(selfname, "..")) {
		fuse_reply_err(req, ENOTEMPTY);
		return;
	}

	ret_val = lookup_dir(parent_inode, selfname, &temp_dentry);
	if (ret_val < 0) {
		ret_val = -ret_val;
		fuse_reply_err(req, ret_val);
		return;
	}

	this_inode = temp_dentry.d_ino;
	write_log(10, "Debug rmdir: name %s, %"FMT_INO_T"\n",
			temp_dentry.d_name, this_inode);
	ret_val = rmdir_update_meta(req, parent_inode, this_inode, selfname);

	if (ret_val < 0)
		ret_val = -ret_val;
	fuse_reply_err(req, ret_val);
}

/************************************************************************
*
* Function name: hfuse_ll_lookup
*        Inputs: fuse_req_t req, fuse_ino_t parent_inode, const char *selfname
*       Summary: Lookup inode stat and generation info given parent
*                "parent_inode" and name "selfname". Will return proper
*                error value if cannot find the name.
*
*************************************************************************/
void hfuse_ll_lookup(fuse_req_t req, fuse_ino_t parent,
			const char *selfname)
{
/* TODO: Special lookup for the name ".", even when parent_inode is not
a directory (for NFS) */
/* TODO: error handling if parent_inode is not a directory and name is not "."
*/

	ino_t this_inode, parent_inode;
	int ret_val;
	DIR_ENTRY temp_dentry;
	struct fuse_entry_param output_param;
	unsigned long this_gen;
	struct stat parent_stat;
	MOUNT_T *tmpptr;

	parent_inode = real_ino(req, parent);

	write_log(10, "Debug lookup parent %"FMT_INO_T", name %s\n",
			parent_inode, selfname);

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);

	write_log(10, "Debug lookup parent mode %d\n", parent_stat.st_mode);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 1);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	memset(&output_param, 0, sizeof(struct fuse_entry_param));

	ret_val = lookup_dir(parent_inode, selfname, &temp_dentry);

	write_log(10,
		"Debug lookup %"FMT_INO_T", %s, %d\n", parent_inode, selfname, ret_val);

	if (ret_val < 0) {
		ret_val = -ret_val;
		fuse_reply_err(req, ret_val);
		return;
	}

	this_inode = temp_dentry.d_ino;
	output_param.ino = (fuse_ino_t) this_inode;
	ret_val = fetch_inode_stat(this_inode, &(output_param.attr),
			&this_gen);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &(output_param.attr));
	}
#endif

	output_param.generation = this_gen;
	write_log(10, "Debug lookup inode %"FMT_INO_T", gen %ld\n",
			this_inode, this_gen);

	if (S_ISREG((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_inode,
				1, D_ISREG);
	if (S_ISDIR((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_inode,
				1, D_ISDIR);
	if (S_ISLNK((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_inode,
				1, D_ISLNK);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}
	fuse_reply_entry(req, &output_param);
}

/* Helper function to compare if oldpath is the prefix path of newpath */
int _check_path_prefix(const char *oldpath, const char *newpath)
{
	char *temppath;

	if (strlen(oldpath) < strlen(newpath)) {
		temppath = malloc(strlen(oldpath)+10);
		if (temppath == NULL)
			return -ENOMEM;
		snprintf(temppath, strlen(oldpath)+5, "%s/", oldpath);
		if (strncmp(newpath, oldpath, strlen(temppath)) == 0) {
			free(temppath);
			return -EINVAL;
		}
		free(temppath);
	}
	return 0;
}

/* Helper function for cleaning up after rename operation */
static inline int _cleanup_rename(META_CACHE_ENTRY_STRUCT *body_ptr,
				META_CACHE_ENTRY_STRUCT *old_target_ptr,
				META_CACHE_ENTRY_STRUCT *parent1_ptr,
				META_CACHE_ENTRY_STRUCT *parent2_ptr)
{
	if (parent1_ptr != NULL) {
		meta_cache_close_file(parent1_ptr);
		meta_cache_unlock_entry(parent1_ptr);
	}
	if ((parent2_ptr != NULL) && (parent2_ptr != parent1_ptr)) {
		meta_cache_close_file(parent2_ptr);
		meta_cache_unlock_entry(parent2_ptr);
	}

	if (body_ptr != NULL) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
	}
	if (old_target_ptr != NULL) {
		meta_cache_close_file(old_target_ptr);
		meta_cache_unlock_entry(old_target_ptr);
	}
	return 0;
}

/************************************************************************
*
* Function name: hfuse_ll_rename
*        Inputs: fuse_req_t req, fuse_ino_t parent,
*                const char *selfname1, fuse_ino_t newparent,
*                const char *selfname2
*       Summary: Rename / move the filesystem object pointed by "parent"
*                and "selfname1" to the path pointed by "newparent" and
*                "selfname2".
*
*************************************************************************/
void hfuse_ll_rename(fuse_req_t req, fuse_ino_t parent,
		const char *selfname1, fuse_ino_t newparent,
		const char *selfname2)
{
	/* How to make rename atomic:
		1. Lookup the parents of oldpath and newpath.
		2. Lock both parents of oldpath and newpath.
		3. Lookup inode for oldpath and newpath.
		4. Invalidate pathname cache for oldpath and newpath
			(if needed).
		5. Lock oldpath and newpath (if needed).
		6. Process rename.
	*/
	ino_t parent_inode1, parent_inode2, self_inode, old_target_inode;
	int ret_val;
	struct stat tempstat, old_target_stat;
	mode_t self_mode, old_target_mode;
	DIR_META_TYPE tempmeta;
	META_CACHE_ENTRY_STRUCT *body_ptr = NULL, *old_target_ptr = NULL;
	META_CACHE_ENTRY_STRUCT *parent1_ptr = NULL, *parent2_ptr = NULL;
	DIR_ENTRY_PAGE temp_page;
	int temp_index;
	struct stat parent_stat1, parent_stat2;
	MOUNT_T *tmpptr;

	parent_inode1 = real_ino(req, parent);
	parent_inode2 = real_ino(req, newparent);

	/* Reject if name too long */
	if (strlen(selfname1) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	/* Reject if name too long */
	if (strlen(selfname2) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode1, &parent_stat1, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat1);
	}
#endif

	if (!S_ISDIR(parent_stat1.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat1, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode2, &parent_stat2, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat2);
	}
#endif

	if (!S_ISDIR(parent_stat2.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat2, 3);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


	/*TODO: To add symlink handling for rename*/

	/* Lock parents */
	parent1_ptr = meta_cache_lock_entry(parent_inode1);

	if (parent1_ptr == NULL) {   /* Cannot lock (cannot allocate) */
		fuse_reply_err(req, ENOMEM);
		return;
	}

	if (parent_inode1 != parent_inode2) {
		parent2_ptr = meta_cache_lock_entry(parent_inode2);
		if (parent2_ptr == NULL) { /* Cannot lock (cannot allocate) */
			meta_cache_close_file(parent1_ptr);
			meta_cache_unlock_entry(parent1_ptr);
			fuse_reply_err(req, ENOMEM);
			return;
		}
	} else {
		parent2_ptr = parent1_ptr;
	}

	/* Check if oldpath and newpath exists already */
	ret_val = meta_cache_seek_dir_entry(parent_inode1, &temp_page,
			&temp_index, selfname1, parent1_ptr);

	if ((ret_val != 0) || (temp_index < 0)) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, -ret_val);
		return;
	}
	self_inode = temp_page.dir_entries[temp_index].d_ino;

	if (self_inode < 1) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, ENOENT);
		return;
	}

	ret_val = meta_cache_seek_dir_entry(parent_inode2, &temp_page,
			&temp_index, selfname2, parent2_ptr);

	if ((ret_val != 0) || (temp_index < 0))
		old_target_inode = 0;
	else
		old_target_inode = temp_page.dir_entries[temp_index].d_ino;

	/* If both newpath and oldpath refer to the same file, do nothing */
	if (self_inode == old_target_inode) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, 0);
		return;
	}

	/* Invalidate pathname cache for oldpath and newpath */

	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, ENOMEM);
		return;
	}
	ret_val = meta_cache_lookup_file_data(self_inode, &tempstat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		meta_cache_remove(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Check if need to move to different parent inode, a dir is
		writeable*/
	if ((S_ISDIR(tempstat.st_mode)) && (parent_inode1 != parent_inode2)) {
		ret_val = check_permission(req, &tempstat, 2);

		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	if (old_target_inode > 0) {
		old_target_ptr = meta_cache_lock_entry(old_target_inode);
		if (old_target_ptr == NULL) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);

			fuse_reply_err(req, -ret_val);
			return;
		}

		ret_val = meta_cache_lookup_file_data(old_target_inode,
					&old_target_stat, NULL, NULL,
						0, old_target_ptr);

		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			meta_cache_remove(old_target_inode);

			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	self_mode = tempstat.st_mode;

	/* Start checking if the operation leads to an error */
	if (old_target_inode > 0) {
		old_target_mode = old_target_stat.st_mode;
		if (S_ISDIR(self_mode) && (!S_ISDIR(old_target_mode))) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, ENOTDIR);
			return;
		}
		if ((!S_ISDIR(self_mode)) && (S_ISDIR(old_target_mode))) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, EISDIR);
			return;
		}
		if (S_ISDIR(old_target_mode)) {
			ret_val = meta_cache_lookup_dir_data(old_target_inode,
				NULL, &tempmeta, NULL, old_target_ptr);
			if (tempmeta.total_children > 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				fuse_reply_err(req, ENOTEMPTY);
				return;
			}
		}
	}

	/* If newpath exists, replace the entry and rmdir/unlink
		the old target */
	if (old_target_inode > 0) {
		ret_val = change_dir_entry_inode(parent_inode2, selfname2,
				self_inode, self_mode, parent2_ptr);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
		meta_cache_close_file(old_target_ptr);
		meta_cache_unlock_entry(old_target_ptr);

		if (S_ISDIR(old_target_mode)) {
			/* Deferring actual deletion to forget */
			ret_val = mark_inode_delete(req, old_target_inode);
		} else {
			ret_val = decrease_nlink_inode_file(req,
					old_target_inode);
		}
		old_target_ptr = NULL;
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}
	} else {
		/* If newpath does not exist, add the new entry */
		ret_val = dir_add_entry(parent_inode2, self_inode,
			selfname2, self_mode, parent2_ptr);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	ret_val = dir_remove_entry(parent_inode1, self_inode,
			selfname1, self_mode, parent1_ptr);
	if (ret_val < 0) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		meta_cache_remove(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	if ((S_ISDIR(self_mode)) && (parent_inode1 != parent_inode2)) {
		ret_val = change_parent_inode(self_inode, parent_inode1,
				parent_inode2, body_ptr);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
	}
#ifdef _ANDROID_ENV_
	if (parent_inode1 != parent_inode2) {
		ret_val = pathlookup_write_parent(self_inode, parent_inode2);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}

		if (tmpptr->volume_type == ANDROID_EXTERNAL) {
			ret_val = delete_pathcache_node(tmpptr->vol_path_cache,
							self_inode);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

	}
#endif
	_cleanup_rename(body_ptr, old_target_ptr,
			parent1_ptr, parent2_ptr);

	fuse_reply_err(req, 0);
}

/* Helper function for waiting on full cache in the truncate function */
int truncate_wait_full_cache(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT **body_ptr,
	int entry_index)
{
	int ret_val;

	while (((block_page)->block_entries[entry_index].status == ST_CLOUD) ||
		((block_page)->block_entries[entry_index].status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			/*Sleep if cache already full*/
			write_log(10,
				"debug truncate waiting on full cache\n");
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);
			sleep_on_cache_full();

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			if (*body_ptr == NULL)
				return -ENOMEM;
			ret_val = meta_cache_lookup_file_data(this_inode,
					inode_stat, file_meta_ptr, block_page,
					page_pos, *body_ptr);
			if (ret_val < 0)
				return ret_val;
		} else {
			break;
		}
	}
	return 0;
}

/* Helper function for truncate operation. Will delete all blocks in the page
*  pointed by temppage starting from "start_index". Track the current page
*  using "page_index". "old_last_block" indicates the last block
*  index before the truncate operation (hence we can ignore the blocks after
*  "old_last_block". "inode_index" is the inode number of the file being
*  truncated. */
int truncate_delete_block(BLOCK_ENTRY_PAGE *temppage, int start_index,
			long long page_index, long long old_last_block,
			ino_t inode_index)
{
	int block_count;
	char thisblockpath[1024];
	long long tmp_blk_index;
	off_t cache_block_size;
	off_t total_deleted_cache;
	long long total_deleted_blocks;
	int ret_val, errcode;
	BLOCK_ENTRY *tmpentry;

	total_deleted_cache = 0;
	total_deleted_blocks = 0;

	write_log(10, "Debug truncate_delete_block, start %d, old_last %lld,",
		start_index, old_last_block);
	write_log(10, " idx %lld\n", page_index);
	for (block_count = start_index; block_count
		< MAX_BLOCK_ENTRIES_PER_PAGE; block_count++) {
		tmp_blk_index = block_count
			+ (MAX_BLOCK_ENTRIES_PER_PAGE * page_index);
		if (tmp_blk_index > old_last_block)
			break;

		tmpentry = &(temppage->block_entries[block_count]);

		switch (tmpentry->status) {
		case ST_NONE:
		case ST_TODELETE:
			break;
		case ST_LDISK:
			ret_val = fetch_block_path(thisblockpath, inode_index,
				tmp_blk_index);
			if (ret_val < 0)
				return ret_val;

			cache_block_size =
					check_file_size(thisblockpath);
			ret_val = unlink(thisblockpath);
			if (ret_val < 0) {
				errcode = errno;
				write_log(0, "IO error in truncate. ");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
			}
			if (tmpentry->uploaded == TRUE)
				tmpentry->status = ST_TODELETE;
			else
				tmpentry->status = ST_NONE;

			total_deleted_cache += (long long) cache_block_size;
			total_deleted_blocks += 1;
			break;
		case ST_CLOUD:
			tmpentry->status =
				ST_TODELETE;
			break;
		case ST_BOTH:
		case ST_LtoC:
			ret_val = fetch_block_path(thisblockpath, inode_index,
				tmp_blk_index);
			if (ret_val < 0)
				return ret_val;
			if (access(thisblockpath, F_OK) == 0) {
				cache_block_size =
					check_file_size(thisblockpath);
				unlink(thisblockpath);
				total_deleted_cache +=
					(long long) cache_block_size;
				total_deleted_blocks += 1;
			}
			tmpentry->status = ST_TODELETE;
			break;
		case ST_CtoL:
			ret_val = fetch_block_path(thisblockpath, inode_index,
				tmp_blk_index);
			if (ret_val < 0)
				return ret_val;
			if (access(thisblockpath, F_OK) == 0)
				unlink(thisblockpath);
			tmpentry->status = ST_TODELETE;
			break;
		default:
			break;
		}
	}
	if (total_deleted_blocks > 0)
		change_system_meta(0, -total_deleted_cache,
				-total_deleted_blocks);

	write_log(10, "Debug truncate_delete_block end\n");

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
	int ret, errcode;

	/*Offset not on the boundary of the block. Will need to truncate the
	last block*/
	ret = truncate_wait_full_cache(this_inode, filestat, tempfilemeta,
			temppage, currentfilepos, body_ptr, last_index);
	if (ret < 0)
		return ret;

	ret = fetch_block_path(thisblockpath, filestat->st_ino,
					last_block);
	if (ret < 0)
		return ret;

	if (((temppage->block_entries[last_index]).status == ST_CLOUD) ||
		((temppage->block_entries[last_index]).status == ST_CtoL)) {
		/*Download from backend */
		blockfptr = fopen(thisblockpath, "a+");
		if (blockfptr == NULL) {
			errcode = errno;
			write_log(0, "IO error in truncate. Code %d, %s\n",
				errcode, strerror(errno));
			return -EIO;
		}
		fclose(blockfptr);
		blockfptr = fopen(thisblockpath, "r+");
		if (blockfptr == NULL) {
			errcode = errno;
			write_log(0, "IO error in truncate. Code %d, %s\n",
				errcode, strerror(errno));
			return -EIO;
		}
		setbuf(blockfptr, NULL);
		ret = flock(fileno(blockfptr), LOCK_EX);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "IO error in truncate. Code %d, %s\n",
				errcode, strerror(errno));
			if (blockfptr != NULL) {
				fclose(blockfptr);
				blockfptr = NULL;
			}
			return -EIO;
		}

		ret = meta_cache_lookup_file_data(this_inode, NULL, NULL,
				temppage, currentfilepos, *body_ptr);
		if (ret < 0) {
			if (blockfptr != NULL) {
				fclose(blockfptr);
				blockfptr = NULL;
			}
			return ret;
		}

		if (((temppage->block_entries[last_index]).status ==
			 ST_CLOUD) ||
			((temppage->block_entries[last_index]).status ==
				ST_CtoL)) {
			if ((temppage->block_entries[last_index]).status ==
				ST_CLOUD) {
				(temppage->block_entries[last_index]).status =
					ST_CtoL;
				ret = meta_cache_update_file_data(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					if (blockfptr != NULL) {
						fclose(blockfptr);
						blockfptr = NULL;
					}
					return ret;
				}
			}
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);

#if (DEDUP_ENABLE)
			ret = fetch_from_cloud(blockfptr, temppage->block_entries[last_index].obj_id);
#else
			ret = fetch_from_cloud(blockfptr, filestat->st_ino,
					last_block);

#endif
			if (ret < 0) {
				if (blockfptr != NULL) {
					fclose(blockfptr);
					blockfptr = NULL;
				}
				return ret;
			}

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			if (*body_ptr == NULL) {
				if (blockfptr != NULL) {
					fclose(blockfptr);
					blockfptr = NULL;
				}
				return -ENOMEM;
			}

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, temppage, currentfilepos, *body_ptr);
			if (ret < 0) {
				if (blockfptr != NULL) {
					fclose(blockfptr);
					blockfptr = NULL;
				}
				return ret;
			}

			if (stat(thisblockpath, &tempstat) == 0) {
				(temppage->block_entries[last_index]).status =
					ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					if (blockfptr != NULL) {
						fclose(blockfptr);
						blockfptr = NULL;
					}
					return -EIO;
				}
				ret = meta_cache_update_file_data(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					if (blockfptr != NULL) {
						fclose(blockfptr);
						blockfptr = NULL;
					}
					return ret;
				}

				change_system_meta(0, tempstat.st_size, 1);
			}
		} else {
			if (stat(thisblockpath, &tempstat) == 0) {
				(temppage->block_entries[last_index]).status =
					ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					if (blockfptr != NULL) {
						fclose(blockfptr);
						blockfptr = NULL;
					}
					return -EIO;
				}
				ret = meta_cache_update_file_data(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					if (blockfptr != NULL) {
						fclose(blockfptr);
						blockfptr = NULL;
					}
					return ret;
				}
			}
		}
		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		change_system_meta(0, new_block_size - old_block_size, 0);

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
	} else {
		if ((temppage->block_entries[last_index]).status == ST_NONE)
			return 0;

		/* TODO: Error handling here if block status is not ST_NONE
			but cannot find block on local disk */
		blockfptr = fopen(thisblockpath, "r+");
		if (blockfptr == NULL) {
			errcode = errno;
			write_log(0, "IO error in truncate. Code %d, %s\n",
				errcode, strerror(errno));
			return -EIO;
		}
		setbuf(blockfptr, NULL);
		ret = flock(fileno(blockfptr), LOCK_EX);
		if (ret < 0) {
			errcode = errno;
			write_log(0, "IO error in truncate. Code %d, %s\n",
				errcode, strerror(errno));
			if (blockfptr != NULL) {
				fclose(blockfptr);
				blockfptr = NULL;
			}
			return -EIO;
		}

		/* TODO: check if it is possible for a block to be deleted
			when truncate is conducted (meta is locked) */
		if (stat(thisblockpath, &tempstat) == 0) {
			(temppage->block_entries[last_index]).status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				if (blockfptr != NULL) {
					fclose(blockfptr);
					blockfptr = NULL;
				}
				return -EIO;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, temppage, currentfilepos, *body_ptr);
			if (ret < 0) {
				if (blockfptr != NULL) {
					fclose(blockfptr);
					blockfptr = NULL;
				}
				return ret;
			}
		}

		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		change_system_meta(0, new_block_size - old_block_size, 0);

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
	}
	return 0;
}

/************************************************************************
*
* Function name: hfuse_ll_truncate
*        Inputs: ino_t this_inode, struct stat *filestat,
*                off_t offset, META_CACHE_ENTRY_STRUCT **body_ptr
*                fuse_req_t req
*       Summary: Truncate the regular file pointed by "this_inode"
*                to size "offset".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*          Note: This function is now called by hfuse_ll_setattr.
*
*************************************************************************/
int hfuse_ll_truncate(ino_t this_inode, struct stat *filestat,
	off_t offset, META_CACHE_ENTRY_STRUCT **body_ptr, fuse_req_t req)
{
/* If truncate file smaller, do not truncate metafile, but instead set the
*  affected entries to ST_TODELETE (which will be changed to ST_NONE once
*  object deleted)*/
/* Add ST_TODELETE as a new block status. In truncate, if need to throw away
*  a block, set the status to ST_TODELETE and upload process will handle the
*  actual deletion.*/
/* If need to truncate some block that's ST_CtoL or ST_CLOUD, download it
*  first, mod it, then set to ST_LDISK*/

	FILE_META_TYPE tempfilemeta, truncfilemeta;
	int ret, errcode;
	long long last_block, last_page, old_last_block;
	long long current_page, old_last_page;
	off_t filepos;
	BLOCK_ENTRY_PAGE temppage;
	int last_index;
	long long temp_trunc_size;
	ssize_t ret_ssize;
	MOUNT_T *tmpptr;
	size_t ret_size;
	FILE *truncfptr;
	char truncpath[METAPATHLEN];

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Debug truncate: offset %ld\n", offset);
	/* If the filesystem object is not a regular file, return error */
	if (S_ISREG(filestat->st_mode) == FALSE) {
		if (S_ISDIR(filestat->st_mode))
			return -EISDIR;
		else
			return -EPERM;
	}

	ret = meta_cache_lookup_file_data(this_inode, NULL, &tempfilemeta,
			NULL, 0, *body_ptr);

	if (ret < 0)
		return ret;

	if (filestat->st_size == offset) {
		/*Do nothing if no change needed */
		write_log(10,
			"Debug truncate: no size change. Nothing changed.\n");
		return 0;
	}

	/*If need to extend, only need to change st_size. Do that later.*/
	if (filestat->st_size > offset) {
		if (offset == 0) {
			last_block = -1;
			last_page = -1;
		} else {
			/* Block indexing starts at zero */
			last_block = ((offset-1) / MAX_BLOCK_SIZE);

			/*Page indexing starts at zero*/
			last_page = last_block / MAX_BLOCK_ENTRIES_PER_PAGE;
		}

		old_last_block = ((filestat->st_size - 1) / MAX_BLOCK_SIZE);
		old_last_page = old_last_block / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (last_page >= 0)
			filepos = seek_page(*body_ptr, last_page, 0);
		else
			filepos = 0;
		if (filepos < 0)
			return filepos;

		current_page = last_page;

		/*TODO: put error handling for the read/write ops here*/
		if (filepos != 0) {
			/* Do not need to truncate the block
				the offset byte is in*/
			/* If filepos is zero*/

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, &temppage, filepos,
				*body_ptr);
			if (ret < 0)
				return ret;

			/* Do the actual handling here*/
			last_index = last_block %
				MAX_BLOCK_ENTRIES_PER_PAGE;
			if ((offset % MAX_BLOCK_SIZE) != 0) {
				/* Truncate the last block that remains
				   after the truncate operation */
				ret = truncate_truncate(this_inode, filestat,
					&tempfilemeta,
					&temppage, filepos,
					body_ptr, last_index,
					last_block, offset);
				if (ret < 0)
					return ret;
			}

			/*Delete the rest of blocks in this same page
			as well*/
			ret = truncate_delete_block(&temppage, last_index+1,
				current_page, old_last_block,
				filestat->st_ino);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return ret;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, &temppage, filepos,
				*body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return ret;
			}
		}

		/*Delete the blocks in the rest of the block status pages*/

		/* Note: if filepos = 0, just means this block
			does not exist. */
		/* TODO: Will need to check if the following
			blocks exist or not */
		for (current_page = last_page + 1;
			current_page <= old_last_page; current_page++) {
			filepos = seek_page(*body_ptr, current_page, 0);
			if (filepos < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return filepos;
			}

			/* Skipping pages that do not exist */
			if (filepos < 1)
				continue;

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, &temppage, filepos, *body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return ret;
			}

			ret = truncate_delete_block(&temppage, 0,
				current_page, old_last_block,
				filestat->st_ino);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return ret;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, &temppage, filepos, *body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				return ret;
			}
		}
		write_log(10, "Debug truncate update xattr\n");
		/* Will need to remember the old offset, so that sync to cloud
		process can check the block status and delete them */
		ret = meta_cache_open_file(*body_ptr);
		if (ret < 0) {
			write_log(0, "IO error in truncate. Data may ");
			write_log(0, "not be consistent\n");
			return ret;
		}

		/* First read from meta file, if need
		to update trunc_size, change the value in meta cache and
		write back to meta cache */
#ifdef _ANDROID_ENV_
		ret = fetch_trunc_path(truncpath, this_inode);

		if (access(truncpath, F_OK) != 0) {
			errcode = errno;

			if (errcode != ENOENT) {
				write_log(0, "IO Error\n");
				write_log(10, "Debug %s. %d, %s\n",
					__func__, errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}

			truncfptr = fopen(truncpath, "w");
			if (truncfptr == NULL) {
				errcode = errno;
				write_log(0, "IO Error\n");
				write_log(10, "Debug %s. %d, %s\n",
					__func__, errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
			setbuf(truncfptr, NULL);
			flock(fileno(truncfptr), LOCK_EX);
			temp_trunc_size = filestat->st_size;
			FWRITE(&temp_trunc_size, sizeof(long long), 1,
				truncfptr);
			fclose(truncfptr);
		} else {
			truncfptr = fopen(truncpath, "r+");
			if (truncfptr == NULL) {
				errcode = errno;
				write_log(0, "IO Error\n");
				write_log(10, "Debug %s. %d, %s\n",
					__func__, errcode, strerror(errcode));
				errcode = -errcode;
				goto errcode_handle;
			}
			setbuf(truncfptr, NULL);
			flock(fileno(truncfptr), LOCK_EX);
			FREAD(&temp_trunc_size, sizeof(long long), 1,
				truncfptr);
			if (temp_trunc_size < filestat->st_size) {
				temp_trunc_size = filestat->st_size;
				FSEEK(truncfptr, 0, SEEK_SET);
				FWRITE(&temp_trunc_size, sizeof(long long), 1,
					truncfptr);
			}
			fclose(truncfptr);
		}
#else
		ret_ssize = fgetxattr(fileno((*body_ptr)->fptr),
				"user.trunc_size",
				&temp_trunc_size, sizeof(long long));
		if (((ret_ssize < 0) && (errno == ENOATTR)) ||
			((ret_ssize >= 0) &&
				(temp_trunc_size < filestat->st_size))) {
			temp_trunc_size = filestat->st_size;
			ret = fsetxattr(fileno((*body_ptr)->fptr),
				"user.trunc_size", &(temp_trunc_size),
				sizeof(long long), 0);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent. ");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				return -EIO;
			}
		} else {
			if (ret_ssize < 0) {
				errcode = errno;
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent. ");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				return -EIO;
			}
		}
#endif /* _ANDROID_ENV_ */
	}

	ret = meta_cache_update_file_data(this_inode, filestat,
			&tempfilemeta, NULL, 0, *body_ptr);
	if (ret < 0) {
		write_log(0, "IO error in truncate. Data may ");
		write_log(0, "not be consistent\n");
		return ret;
	}

	/* Update file and system meta here */
	change_system_meta((long long)(offset - filestat->st_size), 0, 0);

	ret = change_mount_stat(tmpptr,
			(long long) (offset - filestat->st_size), 0);
	if (ret < 0)
		return ret;

	filestat->st_size = offset;
	filestat->st_mtime = time(NULL);

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: hfuse_ll_open
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Open the regular file pointed by "ino", and put file
*                handle info to the structure pointed by "file_info".
*
*************************************************************************/
void hfuse_ll_open(fuse_req_t req, fuse_ino_t ino,
			struct fuse_file_info *file_info)
{
	ino_t thisinode;
	long long fh;
	int ret_val;
	struct stat this_stat;
	int file_flags;
	MOUNT_T *tmpptr;

	write_log(10, "Debug open inode %ld\n", ino);

	thisinode = real_ino(req, ino);

	if (thisinode < 1) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	ret_val = fetch_inode_stat(thisinode, &this_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat);
	}
#endif

	file_flags = file_info->flags;

	if (((file_flags & O_ACCMODE) == O_RDONLY) ||
			((file_flags & O_ACCMODE) == O_RDWR)) {
		/* Checking permission */
		ret_val = check_permission(req, &this_stat, 4);

		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	if (((file_flags & O_ACCMODE) == O_WRONLY) ||
			((file_flags & O_ACCMODE) == O_RDWR)) {
		/* Checking permission */
		ret_val = check_permission(req, &this_stat, 2);

		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	fh = open_fh(thisinode, file_flags);
	if (fh < 0) {
		fuse_reply_err(req, ENFILE);
		return;
	}

	file_info->fh = fh;

	fuse_reply_open(req, file_info);
}

/* Helper function for read operation. Will load file object meta from
*  meta cache or meta file. */
int read_lookup_meta(FH_ENTRY *fh_ptr, BLOCK_ENTRY_PAGE *temppage,
		off_t this_page_fpos)
{
	int ret;

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL)
		return -ENOMEM;
	fh_ptr->meta_cache_locked = TRUE;
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL,
			temppage, this_page_fpos, fh_ptr->meta_cache_ptr);
	if (ret < 0)
		return ret;
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	return 0;
}

/* Helper function for read operation. Will wait on cache full and wait
*  until cache is not full. */
int read_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, long long entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos)
{
	int ret;

	while (((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
		((temppage->block_entries[entry_index]).status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			/*Sleep if cache already full*/
			sem_post(&(fh_ptr->block_sem));
			write_log(10, "debug read waiting on full cache\n");
			sleep_on_cache_full();
			sem_wait(&(fh_ptr->block_sem));
			/*Re-read status*/
			ret = read_lookup_meta(fh_ptr, temppage,
					this_page_fpos);
			if (ret < 0)
				return ret;
		} else {
			break;
		}
	}
	return 0;
}

/* Helper function for read operation. Will prefetch a block from backend for
*  reading. */
/* TODO: For prefetching, need to prevent opening multiple threads
to prefetch the same block (waste of resource, but not critical though as
only one thread will actually download the block) */
int read_prefetch_cache(BLOCK_ENTRY_PAGE *tpage, long long eindex,
		ino_t this_inode, long long block_index, off_t this_page_fpos)
{
	int ret;
	PREFETCH_STRUCT_TYPE *temp_prefetch;
	pthread_t prefetch_thread;

	if ((eindex+1) >= MAX_BLOCK_ENTRIES_PER_PAGE)
		return 0;
	if (((tpage->block_entries[eindex+1]).status == ST_CLOUD) ||
		((tpage->block_entries[eindex+1]).status == ST_CtoL)) {
		temp_prefetch = malloc(sizeof(PREFETCH_STRUCT_TYPE));
		if (temp_prefetch == NULL) {
			write_log(0, "Error cannot open prefetch\n");
			return -ENOMEM;
		}
		temp_prefetch->this_inode = this_inode;
		temp_prefetch->block_no = block_index + 1;
		temp_prefetch->page_start_fpos = this_page_fpos;
		temp_prefetch->entry_index = eindex + 1;
		write_log(10, "Prefetching block %lld for inode %"FMT_INO_T"\n",
			block_index + 1, this_inode);
		ret = pthread_create(&(prefetch_thread),
			&prefetch_thread_attr, (void *)&prefetch_block,
			((void *)temp_prefetch));
		if (ret != 0) {
			write_log(0, "Error number %d\n", ret);
			return -EAGAIN;
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
	int ret, errcode;
	META_CACHE_ENTRY_STRUCT *tmpptr;

	ret = fetch_block_path(thisblockpath, this_inode, bindex);
	if (ret < 0)
		return ret;

	fh_ptr->blockfptr = fopen(thisblockpath, "a+");
	if (fh_ptr->blockfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in read. Code %d, %s\n", errcode,
			strerror(errcode));
		return -EIO;
	}

	fclose(fh_ptr->blockfptr);
	fh_ptr->blockfptr = fopen(thisblockpath, "r+");
	if (fh_ptr->blockfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in read. Code %d, %s\n", errcode,
			strerror(errcode));
		return -EIO;
	}
	setbuf(fh_ptr->blockfptr, NULL);
	ret = flock(fileno(fh_ptr->blockfptr), LOCK_EX);
	if (ret < 0) {
		errcode = errno;
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		write_log(0, "IO error in read. Code %d, %s\n", errcode,
			strerror(errcode));
		return -EIO;
	}

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		return -ENOMEM;
	}

	fh_ptr->meta_cache_locked = TRUE;
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, tpage,
			page_fpos, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		return ret;
	}

	if (((tpage->block_entries[eindex]).status == ST_CLOUD) ||
			((tpage->block_entries[eindex]).status == ST_CtoL)) {
		ret = 0;
		if ((tpage->block_entries[eindex]).status == ST_CLOUD) {
			(tpage->block_entries[eindex]).status = ST_CtoL;
			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
		}
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
		}

#if (DEDUP_ENABLE)
		ret = fetch_from_cloud(fh_ptr->blockfptr, tpage->block_entries[eindex].obj_id);
#else
		ret = fetch_from_cloud(fh_ptr->blockfptr, this_inode, bindex);
#endif
		if (ret < 0) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
		}

		/* Do not process cache update and stored_where change if block
		*  is actually deleted by other ops such as truncate*/

		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		if (fh_ptr->meta_cache_ptr == NULL) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return -ENOMEM;
		}

		fh_ptr->meta_cache_locked = TRUE;
		ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
			NULL, tpage, page_fpos, fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
		}

		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_BOTH;
			tmpptr = fh_ptr->meta_cache_ptr;
			ret = set_block_dirty_status(NULL,
				fh_ptr->blockfptr, FALSE);
			if (ret < 0) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(tmpptr);
				meta_cache_unlock_entry(tmpptr);
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return -EIO;
			}
			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(tmpptr);
				meta_cache_unlock_entry(tmpptr);
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return ret;
			}

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

/* Function for reading from a single block for read operation. Will fetch
*  block from backend if needed. */
size_t _read_block(char *buf, size_t size, long long bindex,
		off_t offset, FH_ENTRY *fh_ptr, ino_t this_inode, int *reterr)
{
	long long current_page;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	off_t this_page_fpos;
	size_t this_bytes_read, ret_size;
	long long entry_index;
	char fill_zeros;
	META_CACHE_ENTRY_STRUCT *tmpptr;
	int ret, errnum, errcode;

	/* Decide the page index for block "bindex" */
	/*Page indexing starts at zero*/
	current_page = bindex / MAX_BLOCK_ENTRIES_PER_PAGE;

	/*If may need to change meta, lock*/
	fh_ptr->meta_cache_ptr =
			meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		*reterr = -ENOMEM;
		return 0;
	}
	fh_ptr->meta_cache_locked = TRUE;

	/* Find the offset of the page if it is not cached */
	if (fh_ptr->cached_page_index != current_page) {
		fh_ptr->cached_filepos = seek_page(fh_ptr->meta_cache_ptr,
				current_page, 0);
		if (fh_ptr->cached_filepos < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			*reterr = fh_ptr->cached_filepos;
			return 0;
		}
		if (fh_ptr->cached_filepos == 0) {
			fh_ptr->cached_filepos =
				create_page(fh_ptr->meta_cache_ptr,
				current_page);
			tmpptr = fh_ptr->meta_cache_ptr;
			if (fh_ptr->cached_filepos < 0) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(tmpptr);
				meta_cache_unlock_entry(tmpptr);
				*reterr = fh_ptr->cached_filepos;
				return 0;
			}
		}
		fh_ptr->cached_page_index = current_page;
	}

	this_page_fpos = fh_ptr->cached_filepos;

	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	entry_index = bindex % MAX_BLOCK_ENTRIES_PER_PAGE;

	sem_wait(&(fh_ptr->block_sem));
	fill_zeros = FALSE;
	while (fh_ptr->opened_block != bindex) {
		if (fh_ptr->opened_block != -1) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}

		ret = read_lookup_meta(fh_ptr, &temppage, this_page_fpos);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

		ret = read_wait_full_cache(&temppage, entry_index, fh_ptr,
			this_page_fpos);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

		ret = read_prefetch_cache(&temppage, entry_index,
			this_inode, bindex, this_page_fpos);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

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
			ret = read_fetch_backend(this_inode,
				bindex, fh_ptr, &temppage,
				this_page_fpos, entry_index);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
			}

			fill_zeros = FALSE;
			break;
		default:
			break;
		}

		if ((fill_zeros != TRUE) && (fh_ptr->opened_block !=
				bindex)) {
			ret = fetch_block_path(thisblockpath,
					this_inode, bindex);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
			}

			fh_ptr->blockfptr = fopen(thisblockpath, "r+");
			if (fh_ptr->blockfptr != NULL) {
				setbuf(fh_ptr->blockfptr, NULL);
				fh_ptr->opened_block = bindex;
			} else {
			/* Some exception that block file is deleted in
			*  the middle of the status check*/
				write_log(2,
					"Debug read: cannot open block file.");
				write_log(2, " Perhaps replaced?\n");
				fh_ptr->opened_block = -1;
			}
		} else {
			break;
		}
	}

	if (fill_zeros != TRUE) {
		ret = flock(fileno(fh_ptr->blockfptr), LOCK_SH);
		if (ret < 0) {
			errnum = errno;
			write_log(0, "Error in read. Code %d, %s\n", errnum,
					strerror(errnum));
			sem_post(&(fh_ptr->block_sem));
			*reterr = -EIO;
			return 0;
		}
		FSEEK(fh_ptr->blockfptr, offset, SEEK_SET);

		FREAD(buf, sizeof(char), size, fh_ptr->blockfptr);

		this_bytes_read = ret_size;
		if (this_bytes_read < size) {
			/*Need to pad zeros*/
			write_log(5, "Short reading? %ld %ld\n",
				offset, size + this_bytes_read);
			memset(&buf[this_bytes_read],
					 0, sizeof(char) *
				(size - this_bytes_read));
			this_bytes_read = size;
		}

		flock(fileno(fh_ptr->blockfptr), LOCK_UN);

	} else {
		write_log(5, "Padding zeros? %ld %ld\n", offset, size);
		this_bytes_read = size;
		memset(buf, 0, sizeof(char) * size);
	}

	sem_post(&(fh_ptr->block_sem));

	return this_bytes_read;

errcode_handle:
	sem_post(&(fh_ptr->block_sem));
	*reterr = errcode;
	return 0;
}

/************************************************************************
*
* Function name: hfuse_ll_read
*        Inputs: fuse_req_t req, fuse_ino_t ino, size_t size_org,
*                off_t offset, struct fuse_file_info *file_info
*       Summary: Read "size_org" bytes from the file "ino", starting from
*                "offset". Returned data is sent via fuse_reply_buf.
*                File handle is provided by the structure in "file_info".
*
*************************************************************************/
void hfuse_ll_read(fuse_req_t req, fuse_ino_t ino,
	size_t size_org, off_t offset, struct fuse_file_info *file_info)
{
	FH_ENTRY *fh_ptr;
	long long start_block, end_block;
	long long block_index;
	int total_bytes_read;
	int this_bytes_read;
	off_t current_offset;
	int target_bytes_read;
	struct stat temp_stat;
	size_t size;
	char *buf;
	char noatime;
	int ret, errcode;
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE) {
		fuse_reply_err(req, EBADF);
		return;
	}

	fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

	if (fh_ptr == NULL) {
		fuse_reply_err(req, EBADFD);
		return;
	}

	/* Check if ino passed in is the same as the one stored */

	if (fh_ptr->thisinode != thisinode) {
		fuse_reply_err(req, EBADFD);
		return;
	}

	if ((!((fh_ptr->flags & O_ACCMODE) == O_RDONLY)) &&
			(!((fh_ptr->flags & O_ACCMODE) == O_RDWR))) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (fh_ptr->flags & O_NOATIME)
		noatime = TRUE;
	else
		noatime = FALSE;

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	fh_ptr->meta_cache_locked = TRUE;
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat, NULL,
			NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fuse_reply_err(req, -ret);
		return;
	}

	/* Decide the true maximum bytes to read */
	/* If read request will exceed the size of the file, need to
	*  reduce the size of request */
	if (temp_stat.st_size < (offset+size_org)) {
		if (temp_stat.st_size > offset)
			size = (temp_stat.st_size - offset);
		else
			size = 0;
	} else {
		size = size_org;
	}

	if (size <= 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fuse_reply_buf(req, NULL, 0);
		return;
	}

	total_bytes_read = 0;

	/* Decide the block indices for the first byte and last byte of
	*  the read */
	/* Block indexing starts at zero */
	start_block = (offset / MAX_BLOCK_SIZE);
	end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	buf = malloc(sizeof(char)*size);

	if (buf == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Read data from each block involved */
	for (block_index = start_block; block_index <= end_block;
				block_index++) {
		current_offset = (offset+total_bytes_read) % MAX_BLOCK_SIZE;
		target_bytes_read = MAX_BLOCK_SIZE - current_offset;

		/*Do not need to read that much*/
		if ((size - total_bytes_read) < target_bytes_read)
			target_bytes_read = size - total_bytes_read;

		this_bytes_read = _read_block(&buf[total_bytes_read],
				target_bytes_read, block_index, current_offset,
				fh_ptr, fh_ptr->thisinode, &errcode);
		if ((this_bytes_read == 0) && (errcode < 0)) {
			fuse_reply_err(req, -errcode);
			free(buf);
			return;
		}

		total_bytes_read += this_bytes_read;

		/*Terminate if cannot write as much as we want*/
		if (this_bytes_read < target_bytes_read)
			break;
	}

	if ((total_bytes_read > 0) && (noatime == FALSE)) {
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		if (fh_ptr->meta_cache_ptr == NULL) {
			fuse_reply_err(req, -ENOMEM);
			free(buf);
			return;
		}

		fh_ptr->meta_cache_locked = TRUE;

		/*Update and flush file meta*/

		ret = meta_cache_lookup_file_data(fh_ptr->thisinode,
			&temp_stat, NULL, NULL, 0, fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			fuse_reply_err(req, -ret);
			free(buf);
			return;
		}

		set_timestamp_now(&temp_stat, ATIME);

		ret = meta_cache_update_file_data(fh_ptr->thisinode,
			&temp_stat, NULL, NULL, 0, fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			fuse_reply_err(req, -ret);
			free(buf);
			return;
		}

		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	}
	fuse_reply_buf(req, buf, total_bytes_read);
	free(buf);
}

/* Helper function for write operation. Will wait on cache full and wait
*  until cache is not full. */
int write_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, long long entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos)
{
	int ret;

	/* Adding cache check for new or deleted blocks */
        while ((((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
                ((temppage->block_entries[entry_index]).status == ST_CtoL)) ||
                (((temppage->block_entries[entry_index]).status
                                                        == ST_TODELETE) ||
                ((temppage->block_entries[entry_index]).status == ST_NONE))) {

		write_log(10,
			"Debug write checking if need to wait for cache\n");
		write_log(10, "%lld, %lld\n",
			hcfs_system->systemdata.cache_size,
			CACHE_HARD_LIMIT);
		if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT) {
			if (CURRENT_BACKEND == NONE)
				return -ENOSPC;
			/*Sleep if cache already full*/
			sem_post(&(fh_ptr->block_sem));
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

			write_log(10, "debug write waiting on full cache\n");
			sleep_on_cache_full();
			/*Re-read status*/
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			if (fh_ptr->meta_cache_ptr == NULL) {
				sem_wait(&(fh_ptr->block_sem));
				return -ENOMEM;
			}
			fh_ptr->meta_cache_locked = TRUE;

			sem_wait(&(fh_ptr->block_sem));
			ret = meta_cache_lookup_file_data(fh_ptr->thisinode,
				NULL, NULL, temppage, this_page_fpos,
				fh_ptr->meta_cache_ptr);
			if (ret < 0)
				return ret;
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
	int ret, errcode;

	ret = fetch_block_path(thisblockpath, this_inode, bindex);
	if (ret < 0)
		return ret;

	fh_ptr->blockfptr = fopen(thisblockpath, "a+");
	if (fh_ptr->blockfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in write. Code %d, %s\n", errcode,
				strerror(errcode));
		return -EIO;
	}
	fclose(fh_ptr->blockfptr);
	fh_ptr->blockfptr = fopen(thisblockpath, "r+");
	if (fh_ptr->blockfptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in write. Code %d, %s\n", errcode,
				strerror(errcode));
		return -EIO;
	}
	setbuf(fh_ptr->blockfptr, NULL);
	ret = flock(fileno(fh_ptr->blockfptr), LOCK_EX);
	if (ret < 0) {
		errcode = errno;
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		write_log(0, "IO error in write. Code %d, %s\n", errcode,
				strerror(errcode));
		return -EIO;
	}
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL, NULL, tpage,
					page_fpos, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		return ret;
	}

	if (((tpage->block_entries[eindex]).status == ST_CLOUD) ||
		((tpage->block_entries[eindex]).status == ST_CtoL)) {
		if ((tpage->block_entries[eindex]).status == ST_CLOUD) {
			(tpage->block_entries[eindex]).status = ST_CtoL;
			ret = meta_cache_update_file_data(fh_ptr->thisinode,
				NULL, NULL, tpage, page_fpos,
				fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return ret;
			}
		}
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

#if (DEDUP_ENABLE)
		ret = fetch_from_cloud(fh_ptr->blockfptr, tpage->block_entries[bindex].obj_id);
#else
		ret = fetch_from_cloud(fh_ptr->blockfptr, this_inode, bindex);
#endif
		if (ret < 0) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
		}
		/*Do not process cache update and stored_where change if block
		* is actually deleted by other ops such as truncate*/

		/*Re-read status*/
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		if (fh_ptr->meta_cache_ptr == NULL) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return -ENOMEM;
		}
		fh_ptr->meta_cache_locked = TRUE;
		ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
			NULL, tpage, page_fpos, fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
		}

		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return -EIO;
			}

			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return ret;
			}

			change_system_meta(0, tempstat2.st_size, 1);
		}
	} else {
		if (stat(thisblockpath, &tempstat2) == 0) {
			(tpage->block_entries[eindex]).status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return -EIO;
			}

			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				if (fh_ptr->blockfptr != NULL) {
					fclose(fh_ptr->blockfptr);
					fh_ptr->blockfptr = NULL;
				}
				return ret;
			}
		}
	}
	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
	fclose(fh_ptr->blockfptr);

	return 0;
}

/* Function for writing to a single block for write operation. Will fetch
*  block from backend if needed. */
size_t _write_block(const char *buf, size_t size, long long bindex,
		off_t offset, FH_ENTRY *fh_ptr, ino_t this_inode, int *reterr)
{
	long long current_page;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	off_t this_page_fpos;
	off_t old_cache_size, new_cache_size;
	size_t this_bytes_written, ret_size;
	long long entry_index;
	int ret, errnum, errcode;
	long long tmpcachesize, tmpdiff;

	/* Decide the page index for block "bindex" */
	/*Page indexing starts at zero*/
	current_page = bindex / MAX_BLOCK_ENTRIES_PER_PAGE;

	/* Find the offset of the page if it is not cached */
	if (fh_ptr->cached_page_index != current_page) {
		fh_ptr->cached_filepos = seek_page(fh_ptr->meta_cache_ptr,
				current_page, 0);
		if (fh_ptr->cached_filepos < 0) {
			*reterr = fh_ptr->cached_filepos;
			return 0;
		}
		if (fh_ptr->cached_filepos == 0) {
			fh_ptr->cached_filepos =
				create_page(fh_ptr->meta_cache_ptr,
				current_page);
			if (fh_ptr->cached_filepos < 0) {
				*reterr = fh_ptr->cached_filepos;
				return 0;
			}
		}
		fh_ptr->cached_page_index = current_page;
	}

	this_page_fpos = fh_ptr->cached_filepos;

	entry_index = bindex % MAX_BLOCK_ENTRIES_PER_PAGE;

	ret = fetch_block_path(thisblockpath, this_inode, bindex);
	if (ret < 0) {
		*reterr = ret;
		return 0;
	}
	sem_wait(&(fh_ptr->block_sem));

	/* Check if cache space is full */
	if (access(thisblockpath, F_OK) == 0) {
		old_cache_size = check_file_size(thisblockpath);
		tmpcachesize = hcfs_system->systemdata.cache_size;
		tmpdiff = (offset + size) - old_cache_size;
		if ((tmpdiff > 0) &&
			((tmpdiff + tmpcachesize) > CACHE_HARD_LIMIT)) {
			/* Need to sleep for full here or return ENOSPC */
			if (fh_ptr->opened_block != -1) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->opened_block = -1;
			}
			sem_post(&(fh_ptr->block_sem));
			if (CURRENT_BACKEND == NONE) {
				*reterr = -ENOSPC;
				return 0;
			}
			/*Sleep if cache already full*/
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

			write_log(10, "debug write waiting on full cache\n");
			sleep_on_cache_full();
			/*Re-read status*/
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			if (fh_ptr->meta_cache_ptr == NULL) {
				*reterr = -ENOMEM;
				return 0;
			}
			fh_ptr->meta_cache_locked = TRUE;

			sem_wait(&(fh_ptr->block_sem));
		}
	}

	/* Check if we can reuse cached block */
	if (fh_ptr->opened_block != bindex) {
		/* If the cached block is not the one we are writing to,
		*  close the one already opened. */
		if (fh_ptr->opened_block != -1) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
			NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

		ret = write_wait_full_cache(&temppage, entry_index, fh_ptr,
							this_page_fpos);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

		switch ((temppage).block_entries[entry_index].status) {
		case ST_NONE:
		case ST_TODELETE:
			/*If not stored anywhere, make it on local disk*/
			fh_ptr->blockfptr = fopen(thisblockpath, "a+");
			if (fh_ptr->blockfptr == NULL) {
				errnum = errno;
				sem_post(&(fh_ptr->block_sem));
				*reterr = -EIO;
				write_log(0, "Error in write. Code %d, %s\n",
					errnum, strerror(errnum));
				return 0;
			}
			fclose(fh_ptr->blockfptr);
			(temppage).block_entries[entry_index].status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = -EIO;
				return 0;
			}
			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
			}

			change_system_meta(0, 0, 1);
			break;
		case ST_LDISK:
			break;
		case ST_BOTH:
		case ST_LtoC:
			(temppage).block_entries[entry_index].status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = -EIO;
				return 0;
			}
			ret = meta_cache_update_file_data(fh_ptr->thisinode,
					NULL, NULL, &temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
			}
			break;
		case ST_CLOUD:
		case ST_CtoL:
			/*Download from backend */
			ret = _write_fetch_backend(this_inode, bindex, fh_ptr,
					&temppage, this_page_fpos, entry_index);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
			}
			break;
		default:
			break;
		}

		fh_ptr->blockfptr = fopen(thisblockpath, "r+");
		if (fh_ptr->blockfptr == NULL) {
			errnum = errno;
			sem_post(&(fh_ptr->block_sem));
			*reterr = -EIO;
			write_log(0, "Error in write. Code %d, %s\n",
				errnum, strerror(errnum));
			return 0;
		}
		setbuf(fh_ptr->blockfptr, NULL);
		fh_ptr->opened_block = bindex;
	}
	ret = flock(fileno(fh_ptr->blockfptr), LOCK_EX);
	if (ret < 0) {
		errnum = errno;
		sem_post(&(fh_ptr->block_sem));
		*reterr = -EIO;
		write_log(0, "Error in write. Code %d, %s\n",
			errnum, strerror(errnum));
		return 0;
	}

	old_cache_size = check_file_size(thisblockpath);
	if (offset > old_cache_size) {
		write_log(10, "Debug write: cache block size smaller than ");
		write_log(10, "starting offset. Extending\n");
		ftruncate(fileno(fh_ptr->blockfptr), offset);
	}

	FSEEK(fh_ptr->blockfptr, offset, SEEK_SET);

	FWRITE(buf, sizeof(char), size, fh_ptr->blockfptr);

	this_bytes_written = ret_size;

	new_cache_size = check_file_size(thisblockpath);

	if (old_cache_size != new_cache_size)
		change_system_meta(0, new_cache_size - old_cache_size, 0);

	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
	sem_post(&(fh_ptr->block_sem));

	return this_bytes_written;

errcode_handle:
	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
	sem_post(&(fh_ptr->block_sem));
	*reterr = errcode;
	return 0;
}

/************************************************************************
*
* Function name: hfuse_ll_write
*        Inputs: fuse_req_t req, fuse_ino_t ino, const char *buf,
*                size_t size, off_t offset, struct fuse_file_info *file_info
*       Summary: Write "size" bytes to the file "ino", starting from
*                "offset", from the buffer pointed by "buf". File handle
*                is provided by the structure in "file_info".
*
*************************************************************************/
void hfuse_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		size_t size, off_t offset, struct fuse_file_info *file_info)
{
	FH_ENTRY *fh_ptr;
	long long start_block, end_block;
	long long block_index;
	size_t total_bytes_written;
	size_t this_bytes_written;
	off_t current_offset;
	size_t target_bytes_written;
	struct stat temp_stat;
	int ret, errcode;
	ino_t thisinode;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	thisinode = real_ino(req, ino);

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (size <= 0) {
		fuse_reply_write(req, 0);
		return;
	}

	/*Sleep if cache already full (don't do this now)*/
/*	if (hcfs_system->systemdata.cache_size > CACHE_HARD_LIMIT)
		sleep_on_cache_full();
*/
	total_bytes_written = 0;

	/* Decide the block indices for the first byte and last byte of
	*  the write */
	/* Block indexing starts at zero */
	start_block = (offset / MAX_BLOCK_SIZE);
	end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

	fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

	/* Check if ino passed in is the same as the one stored */

	if (fh_ptr->thisinode != (ino_t) thisinode) {
		fuse_reply_err(req, EBADFD);
		return;
	}

	write_log(10, "flags %d\n", fh_ptr->flags & O_ACCMODE);
	if ((!((fh_ptr->flags & O_ACCMODE) == O_WRONLY)) &&
			(!((fh_ptr->flags & O_ACCMODE) == O_RDWR))) {
		fuse_reply_err(req, EBADF);
		return;
	}

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	fh_ptr->meta_cache_locked = TRUE;

	for (block_index = start_block; block_index <= end_block;
							block_index++) {
		current_offset = (offset+total_bytes_written) % MAX_BLOCK_SIZE;

		target_bytes_written = MAX_BLOCK_SIZE - current_offset;

		/* If do not need to write that much */
		if ((size - total_bytes_written) < target_bytes_written)
			target_bytes_written = size - total_bytes_written;

		this_bytes_written = _write_block(&buf[total_bytes_written],
			target_bytes_written, block_index, current_offset,
				fh_ptr, fh_ptr->thisinode, &errcode);
		if ((this_bytes_written == 0) && (errcode < 0)) {
			if (fh_ptr->meta_cache_ptr != NULL) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(fh_ptr->meta_cache_ptr);
				meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			}
			fuse_reply_err(req, -errcode);
			return;
		}

		total_bytes_written += this_bytes_written;

		/*Terminate if cannot write as much as we want*/
		if (this_bytes_written < target_bytes_written)
			break;
	}

	/*Update and flush file meta*/

	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat, NULL,
					NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fuse_reply_err(req, -ret);
		return;
	}

	if (temp_stat.st_size < (offset + total_bytes_written)) {
		change_system_meta((long long) ((offset + total_bytes_written)
						- temp_stat.st_size), 0, 0);
		ret = change_mount_stat(tmpptr,
			(long long) ((offset + total_bytes_written)
						- temp_stat.st_size), 0);
		if (ret < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			fuse_reply_err(req, -ret);
			return;
		}

		temp_stat.st_size = (offset + total_bytes_written);
		temp_stat.st_blocks = (temp_stat.st_size+511) / 512;
	}

	if (total_bytes_written > 0)
		set_timestamp_now(&temp_stat, MTIME | CTIME);

	ret = meta_cache_update_file_data(fh_ptr->thisinode, &temp_stat, NULL,
					NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fuse_reply_err(req, -ret);
		return;
	}

	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

	fuse_reply_write(req, total_bytes_written);
}

/************************************************************************
*
* Function name: hfuse_ll_statfs
*        Inputs: fuse_req_t req, fuse_ino_t ino
*       Summary: Lookup the filesystem status. "ino" is not used now.
*
*************************************************************************/
void hfuse_ll_statfs(fuse_req_t req, fuse_ino_t ino)
{
	struct statvfs *buf;
	ino_t thisinode;
	MOUNT_T *tmpptr;
	long long system_size, num_inodes;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	/* TODO: Different statistics for different filesystems */
	write_log(10, "Debug statfs\n");

	thisinode = real_ino(req, ino);

	buf = malloc(sizeof(struct statvfs));
	if (buf == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	/*Prototype is linux statvfs call*/
	sem_wait(&(tmpptr->stat_lock));

	system_size = (tmpptr->FS_stat).system_size;
	num_inodes = (tmpptr->FS_stat).num_inodes;

	sem_post(&(tmpptr->stat_lock));

	/* TODO: If no backend, use cache size as total volume size */
	buf->f_bsize = 4096;
	buf->f_frsize = 4096;
	if (system_size > (50*powl(1024, 3)))
		buf->f_blocks = (((system_size - 1)
						/ 4096) + 1) * 2;
	else
		buf->f_blocks = (25*powl(1024, 2));

	if (system_size == 0)
		buf->f_bfree = buf->f_blocks;
	else
		buf->f_bfree = buf->f_blocks -
			(((system_size - 1)
						/ 4096) + 1);
	if (buf->f_bfree < 0)
		buf->f_bfree = 0;
	buf->f_bavail = buf->f_bfree;

	write_log(10, "Debug statfs, checking inodes\n");

	if (num_inodes > 1000000)
		buf->f_files = (num_inodes * 2);
	else
		buf->f_files = 2000000;

	buf->f_ffree = buf->f_files - num_inodes;
	if (buf->f_ffree < 0)
		buf->f_ffree = 0;

#ifdef STAT_VFS_H
        buf->f_namelen = MAX_FILENAME_LEN;
#else
	buf->f_favail = buf->f_ffree;
	buf->f_namemax = MAX_FILENAME_LEN;
#endif

	write_log(10, "Debug statfs, returning info\n");

	fuse_reply_statfs(req, buf);
	free(buf);
	write_log(10, "Debug statfs end\n");
}

/************************************************************************
*
* Function name: hfuse_ll_flush
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Flush the file content. Not used now as cache mode is not
*                write back.
*
*************************************************************************/
void hfuse_ll_flush(fuse_req_t req, fuse_ino_t ino,
				struct fuse_file_info *file_info)
{
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	fuse_reply_err(req, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_release
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Close the file handle pointed by "file_info".
*
*************************************************************************/
void hfuse_ll_release(fuse_req_t req, fuse_ino_t ino,
			struct fuse_file_info *file_info)
{
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	if (file_info->fh < 0) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (file_info->fh >= MAX_OPEN_FILE_ENTRIES) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (system_fh_table.entry_table_flags[file_info->fh] == FALSE) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (system_fh_table.entry_table[file_info->fh].thisinode
					!= thisinode) {
		fuse_reply_err(req, EBADF);
		return;
	}

	close_fh(file_info->fh);
	fuse_reply_err(req, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_fsync
*        Inputs: fuse_req_t req, fuse_ino_t ino, int isdatasync,
*                struct fuse_file_info *file_info
*       Summary: Conduct "fsync". Do nothing now (see hfuse_flush).
*
*************************************************************************/
void hfuse_ll_fsync(fuse_req_t req, fuse_ino_t ino, int isdatasync,
					struct fuse_file_info *file_info)
{
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	fuse_reply_err(req, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_opendir
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Check permission and open directory for access.
*
*************************************************************************/
static void hfuse_ll_opendir(fuse_req_t req, fuse_ino_t ino,
			struct fuse_file_info *file_info)
{
	int ret_val;
	struct stat this_stat;
	ino_t thisinode;
	MOUNT_T *tmpptr;

	thisinode = real_ino(req, ino);

	ret_val = fetch_inode_stat(thisinode, &this_stat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat);
	}
#endif

	if (!S_ISDIR(this_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &this_stat, 4);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	fuse_reply_open(req, file_info);
}

/************************************************************************
*
* Function name: hfuse_ll_readdir
*        Inputs: fuse_req_t req, fuse_ino_t ino, size_t size,
*                off_t offset, struct fuse_file_info *file_info
*       Summary: Read directory content starting from "offset". This
*                implementation can return partial results and continue
*                reading in follow-up calls.
*
*************************************************************************/
void hfuse_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			off_t offset, struct fuse_file_info *file_info)
{
	/* Now will read partial entries and deal with others later */
	ino_t this_inode;
	int count;
	off_t thisfile_pos;
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE temp_page;
	struct stat tempstat, thisstat;
	struct timeval tmp_time1, tmp_time2;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	long countn;
	off_t nextentry_pos;
	int page_start;
	char *buf;
	off_t buf_pos;
	size_t entry_size, ret_size;
	int ret, errcode;

	gettimeofday(&tmp_time1, NULL);

	this_inode = real_ino(req, ino);

	write_log(10, "DEBUG readdir entering readdir, ");
	write_log(10, "size %ld, offset %ld\n", size, offset);

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	ret = meta_cache_lookup_dir_data(this_inode, &thisstat, &tempmeta,
						NULL, body_ptr);
	if (ret < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		fuse_reply_err(req, -ret);
		return;
	}

	buf = malloc(sizeof(char)*size);
	if (buf == NULL) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		fuse_reply_err(req, ENOMEM);
		return;
	}

	buf_pos = 0;

	page_start = 0;
	if (offset >= MAX_DIR_ENTRIES_PER_PAGE) {
		thisfile_pos = offset / (MAX_DIR_ENTRIES_PER_PAGE + 1);
		page_start = offset % (MAX_DIR_ENTRIES_PER_PAGE + 1);
		write_log(10, "readdir starts at offset %ld, entry number %d\n",
						thisfile_pos, page_start);
		if (body_ptr->meta_opened == FALSE) {
			ret = meta_cache_open_file(body_ptr);
			if (ret < 0) {
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, -ret);
				return;
			}
		}
		meta_cache_drop_pages(body_ptr);
	} else {
		thisfile_pos = tempmeta.tree_walk_list_head;

		if (tempmeta.total_children > (MAX_DIR_ENTRIES_PER_PAGE-2)) {
			if (body_ptr->meta_opened == FALSE) {
				ret = meta_cache_open_file(body_ptr);
				if (ret < 0) {
					meta_cache_close_file(body_ptr);
					meta_cache_unlock_entry(body_ptr);
					fuse_reply_err(req, -ret);
					return;
				}
			}
			meta_cache_drop_pages(body_ptr);
		}
	}

	write_log(10, "Debug readdir file pos %ld\n", thisfile_pos);
	countn = 0;
	while (thisfile_pos != 0) {
		write_log(10, "Now %ldth iteration\n", countn);
		countn++;
		memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
		temp_page.this_page_pos = thisfile_pos;
		if ((tempmeta.total_children <= (MAX_DIR_ENTRIES_PER_PAGE-2))
							&& (page_start == 0)) {
			ret = meta_cache_lookup_dir_data(this_inode, NULL,
						NULL, &temp_page, body_ptr);
			if (ret < 0) {
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, -ret);
				return;
			}
		} else {
			FSEEK(body_ptr->fptr, thisfile_pos, SEEK_SET);

			FREAD(&temp_page, sizeof(DIR_ENTRY_PAGE), 1,
						body_ptr->fptr);
		}

		write_log(10, "Debug readdir page start %d %d\n", page_start,
			temp_page.num_entries);
		for (count = page_start; count < temp_page.num_entries;
								count++) {
			memset(&tempstat, 0, sizeof(struct stat));
			tempstat.st_ino = temp_page.dir_entries[count].d_ino;

			if (temp_page.dir_entries[count].d_type == D_ISDIR)
				tempstat.st_mode = S_IFDIR;
			if (temp_page.dir_entries[count].d_type == D_ISREG)
				tempstat.st_mode = S_IFREG;
			if (temp_page.dir_entries[count].d_type == D_ISLNK)
				tempstat.st_mode = S_IFLNK;

			nextentry_pos = temp_page.this_page_pos *
				(MAX_DIR_ENTRIES_PER_PAGE + 1) + (count+1);
			entry_size = fuse_add_direntry(req, &buf[buf_pos],
					(size - buf_pos),
					temp_page.dir_entries[count].d_name,
					&tempstat, nextentry_pos);
			write_log(10, "Debug readdir entry %s, %"FMT_INO_T"\n",
				temp_page.dir_entries[count].d_name,
				tempstat.st_ino);
			write_log(10, "Debug readdir entry size %ld\n",
				entry_size);
			if (entry_size > (size - buf_pos)) {
				meta_cache_unlock_entry(body_ptr);
				write_log(10,
					"Readdir breaks, next offset %ld, ",
					nextentry_pos);
				write_log(10, "file pos %lld, entry %d\n",
					temp_page.this_page_pos, (count+1));
				fuse_reply_buf(req, buf, buf_pos);
				return;
			}
			buf_pos += entry_size;
		}
		page_start = 0;
		thisfile_pos = temp_page.tree_walk_next;
	}

	ret = 0;
	if (buf_pos > 0) {
		set_timestamp_now(&thisstat, ATIME);
		ret = meta_cache_update_dir_data(this_inode, &thisstat, NULL,
					NULL, body_ptr);
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret < 0) {
		fuse_reply_err(req, -ret);
		return;
	}
	gettimeofday(&tmp_time2, NULL);

	write_log(10, "readdir elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	fuse_reply_buf(req, buf, buf_pos);

	return;

errcode_handle:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	fuse_reply_err(req, -errcode);
}

/************************************************************************
*
* Function name: hfuse_ll_releasedir
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Close opened directory. Do nothing now.
*
*************************************************************************/
void hfuse_ll_releasedir(fuse_req_t req, fuse_ino_t ino,
					struct fuse_file_info *file_info)
{
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	fuse_reply_err(req, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_init
*        Inputs: void *userdata, struct fuse_conn_info *conn
*       Summary: Initiate a FUSE mount
*
*************************************************************************/
void hfuse_ll_init(void *userdata, struct fuse_conn_info *conn)
{
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *)userdata;
	write_log(10, "Root inode is %"FMT_INO_T"\n", tmpptr->f_ino);

	lookup_increase(tmpptr->lookup_table, tmpptr->f_ino, 1, D_ISDIR);
}

/************************************************************************
*
* Function name: hfuse_ll_destroy
*        Inputs: void *userdata
*       Summary: Destroy a FUSE mount
*
*************************************************************************/
void hfuse_ll_destroy(void *userdata)
{
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *)userdata;
	write_log(10, "Unmounting FS with root inode %"FMT_INO_T"\n", tmpptr->f_ino);
}

/************************************************************************
*
* Function name: hfuse_ll_setattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, struct stat *attr,
*                int to_set, struct fuse_file_info *fi
*       Summary: Set attribute for a filesystem object. This includes
*                routines such as chmod, chown, truncate, utimens.
*
*************************************************************************/
void hfuse_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
	int to_set, struct fuse_file_info *fi)
{
	int ret_val;
	ino_t this_inode;
	char attr_changed;
	struct timespec timenow;
	struct stat newstat;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	struct fuse_ctx *temp_context;

	write_log(10, "Debug setattr, to_set %d\n", to_set);

	this_inode = real_ino(req, ino);

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	attr_changed = FALSE;

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	ret_val = meta_cache_lookup_file_data(this_inode, &newstat,
			NULL, NULL, 0, body_ptr);

	if (ret_val < 0) {  /* Cannot fetch any meta*/
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		meta_cache_remove(this_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	if ((to_set & FUSE_SET_ATTR_SIZE) &&
			(newstat.st_size != attr->st_size)) {

		/* Checking permission */
		ret_val = check_permission(req, &newstat, 2);

		if (ret_val < 0) {
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}

		ret_val = hfuse_ll_truncate(this_inode, &newstat,
				attr->st_size, &body_ptr, req);
		if (ret_val < 0) {
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_MODE) {
		write_log(10, "Debug setattr context %d, file %d\n",
			temp_context->uid, newstat.st_uid);

		if ((temp_context->uid != 0) &&
			(temp_context->uid != newstat.st_uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.st_mode = attr->st_mode;
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_UID) {
		if (temp_context->uid != 0) {   /* Not privileged */
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.st_uid = attr->st_uid;
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_GID) {
		if (temp_context->uid != 0) {
			ret_val = is_member(req, newstat.st_gid, attr->st_gid);
			if (ret_val < 0) {
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, -ret_val);
				return;
			}
			if ((temp_context->uid != newstat.st_uid) ||
				(ret_val == 0)) {
				/* Not privileged and (not owner or
				not in group) */
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, EPERM);
				return;
			}
		}

		newstat.st_gid = attr->st_gid;
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_ATIME) {
		if ((temp_context->uid != 0) &&
			(temp_context->uid != newstat.st_uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.st_atime = attr->st_atime;
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.st_atim), &(attr->st_atim),
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_MTIME) {
		if ((temp_context->uid != 0) &&
			(temp_context->uid != newstat.st_uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}
		newstat.st_mtime = attr->st_mtime;
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.st_mtim), &(attr->st_mtim),
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	clock_gettime(CLOCK_REALTIME, &timenow);

	if (to_set & FUSE_SET_ATTR_ATIME_NOW) {
		if ((temp_context->uid != 0) &&
			((temp_context->uid != newstat.st_uid) ||
				(check_permission(req, &newstat, 2) < 0))) {
			/* Not privileged and
				(not owner or no write permission)*/

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EACCES);
			return;
		}
		newstat.st_atime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.st_atim), &timenow,
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_MTIME_NOW) {
		if ((temp_context->uid != 0) &&
			((temp_context->uid != newstat.st_uid) ||
				(check_permission(req, &newstat, 2) < 0))) {
			/* Not privileged and
				(not owner or no write permission)*/

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EACCES);
			return;
		}
		newstat.st_mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.st_mtim), &timenow,
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	if (attr_changed == TRUE) {
		newstat.st_ctime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.st_ctim), &timenow,
			sizeof(struct timespec));
#endif
		if (to_set & FUSE_SET_ATTR_SIZE) {
			newstat.st_mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
			memcpy(&(newstat.st_mtim), &timenow,
				sizeof(struct timespec));
#endif
		}
	}

	ret_val = meta_cache_update_file_data(this_inode, &newstat,
			NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}
	fuse_reply_attr(req, &newstat, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_access
*        Inputs: fuse_req_t req, fuse_ino_t ino, int mode
*       Summary: Checks the permission for object "ino" against "mode".
*
*************************************************************************/
static void hfuse_ll_access(fuse_req_t req, fuse_ino_t ino, int mode)
{
	struct stat thisstat;
	int ret_val;
	ino_t thisinode;
	MOUNT_T *tmpptr;

	thisinode = real_ino(req, ino);

	ret_val = fetch_inode_stat(thisinode, &thisstat, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &thisstat);
	}
#endif

	if (mode == F_OK) {
		fuse_reply_err(req, 0);
		return;
	}

	/* Checking permission */
	ret_val = check_permission_access(req, &thisstat, mode);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	fuse_reply_err(req, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_forget
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                unsigned long nlookup
*       Summary: Decrease lookup count for object "ino" by "nlookup", and
*                handle actual filesystem object deletion if count is 0.
*
*************************************************************************/
static void hfuse_ll_forget(fuse_req_t req, fuse_ino_t ino,
	unsigned long nlookup)
{
	int amount;
	int current_val;
	char to_delete;
	char d_type;
	ino_t thisinode;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	thisinode = real_ino(req, ino);

	amount = (int) nlookup;

	current_val = lookup_decrease(tmpptr->lookup_table, thisinode, amount,
					&d_type, &to_delete);

	if (current_val < 0) {
		write_log(0, "Error in lookup count decreasing\n");
		fuse_reply_none(req);
		return;
	}

	if (current_val > 0) {
		write_log(10, "Debug forget: lookup count greater than zero\n");
		fuse_reply_none(req);
		return;
	}

	if ((current_val == 0) && (to_delete == TRUE))
		actual_delete_inode(thisinode, d_type, tmpptr->f_ino, tmpptr);

	fuse_reply_none(req);
}

/************************************************************************
*
* Function name: hfuse_ll_symlink
*        Inputs: fuse_req_t req, const char *link, fuse_ino_t parent,
*                const char *name
*       Summary: Make a symbolic link "name", which links to "link".
*
*************************************************************************/
static void hfuse_ll_symlink(fuse_req_t req, const char *link,
	fuse_ino_t parent, const char *name)
{
	ino_t parent_inode;
	ino_t self_inode;
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry;
	DIR_ENTRY_PAGE dir_page;
	unsigned long this_generation;
	struct fuse_ctx *temp_context;
	struct fuse_entry_param tmp_param;
	struct stat parent_stat;
	struct stat this_stat;
	int ret_val;
	int result_index;
	int errcode;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		fuse_reply_err(req, ENOTSUP);
		return;
	}
#endif

	parent_inode = real_ino(req, parent);

	/* Reject if name too long */
	if (strlen(name) > MAX_FILENAME_LEN) {
		write_log(0, "File name is too long\n");
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}
	if (strlen(name) <= 0) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	/* Reject if link path too long */
	if (strlen(link) >= MAX_LINK_PATH) {
		write_log(0, "Link path is too long\n");
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}
	if (strlen(link) <= 0) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Error if parent is not a dir */
	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Check whether "name" exists or not */
	parent_meta_cache_entry = meta_cache_lock_entry(parent_inode);
	if (!parent_meta_cache_entry) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	ret_val = meta_cache_seek_dir_entry(parent_inode, &dir_page,
		&result_index, name, parent_meta_cache_entry);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}
	if (result_index >= 0) {
		write_log(0, "File %s existed\n", name);
		errcode = -EEXIST;
		goto error_handle;
	}

	/* Prepare stat and request a new inode from superblock */
	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		write_log(0, "Memory allocation error in symlink()\n");
		errcode = -ENOMEM;
		goto error_handle;
	}

	memset(&this_stat, 0, sizeof(struct stat));
	this_stat.st_nlink = 1;
	this_stat.st_size = strlen(link);

        this_stat.st_mode = S_IFLNK | 0777;

	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	set_timestamp_now(&this_stat, ATIME | MTIME | CTIME);

	self_inode = super_block_new_inode(&this_stat, &this_generation);
	if (self_inode < 1) {
		errcode = -ENOSPC;
		goto error_handle;
	}

	this_stat.st_ino = self_inode;

	/* Write symlink meta and add new entry to parent */
	ret_val = symlink_update_meta(parent_meta_cache_entry, &this_stat,
		link, this_generation, name, tmpptr->f_ino);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		errcode = ret_val;
		goto error_handle;
	}

	ret_val = meta_cache_close_file(parent_meta_cache_entry);
	if (ret_val < 0) {
		meta_cache_unlock_entry(parent_meta_cache_entry);
		fuse_reply_err(req, -ret_val);
		return;
	}
	ret_val = meta_cache_unlock_entry(parent_meta_cache_entry);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Reply fuse entry */
	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_generation;
	tmp_param.ino = (fuse_ino_t) self_inode;
	memcpy(&(tmp_param.attr), &this_stat, sizeof(struct stat));

	ret_val = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISLNK);

	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
	}
	ret_val = change_mount_stat(tmpptr, 0, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
	}

	write_log(5, "Debug symlink: symlink operation success\n");
	fuse_reply_entry(req, &(tmp_param));
	return;

error_handle:
	meta_cache_close_file(parent_meta_cache_entry);
	meta_cache_unlock_entry(parent_meta_cache_entry);
	fuse_reply_err(req, -errcode);
}

/************************************************************************
*
* Function name: hfuse_ll_readlink
*        Inputs: fuse_req_t req, fuse_ino_t ino
*       Summary: Read symbolic link target path and reply to fuse.
*
*************************************************************************/
static void hfuse_ll_readlink(fuse_req_t req, fuse_ino_t ino)
{
	ino_t this_inode;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	SYMLINK_META_TYPE symlink_meta;
	struct stat symlink_stat;
	char link_buffer[MAX_LINK_PATH + 1];
	int ret_code;

	this_inode = real_ino(req, ino);

	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "readlink() lock meta cache entry fail\n");
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Get stat and meta */
	ret_code = meta_cache_lookup_symlink_data(this_inode, &symlink_stat,
		&symlink_meta, meta_cache_entry);
	if (ret_code < 0) {
		write_log(0, "readlink() lookup symlink meta fail\n");
		meta_cache_close_file(meta_cache_entry);
		meta_cache_unlock_entry(meta_cache_entry);
		fuse_reply_err(req, -ret_code);
		return;
	}

	/* Update access time */
	set_timestamp_now(&symlink_stat, ATIME);
	ret_code = meta_cache_update_symlink_data(this_inode, &symlink_stat,
		NULL, meta_cache_entry);
	if (ret_code < 0) {
		write_log(0, "readlink() update symlink meta fail\n");
		meta_cache_close_file(meta_cache_entry);
		meta_cache_unlock_entry(meta_cache_entry);
		fuse_reply_err(req, -ret_code);
		return;
	}

	ret_code = meta_cache_close_file(meta_cache_entry);
	if (ret_code < 0) {
		meta_cache_unlock_entry(meta_cache_entry);
		fuse_reply_err(req, -ret_code);
		return;
	}

	ret_code = meta_cache_unlock_entry(meta_cache_entry);
	if (ret_code < 0) {
		fuse_reply_err(req, -ret_code);
		return;
	}

	memcpy(link_buffer, symlink_meta.link_path, sizeof(char) *
		symlink_meta.link_len);
	link_buffer[symlink_meta.link_len] = '\0';

	write_log(5, "Readlink: Lookup symlink success. Link to %s\n",
		link_buffer);
	fuse_reply_readlink(req, link_buffer);
}

/************************************************************************
*
* Function name: hfuse_ll_setxattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, char *name, char *value,
*                size_t size, int flag
*       Summary: Set extended attribute when given (name, value) pair. It
*                creates new attribute when name is not found. On the
*                other hand, if attribute exists, it replaces the old
*                value with new value.
*
*************************************************************************/
static void hfuse_ll_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	const char *value, size_t size, int flag)
{
	/* TODO: Tmp ignore permission of namespace */
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	XATTR_PAGE *xattr_page;
	long long xattr_filepos;
	char key[MAX_KEY_SIZE];
	char name_space;
	int retcode;
	struct stat stat_data;
	ino_t this_inode;

	this_inode = real_ino(req, ino);
	xattr_page = NULL;

	if (size <= 0) {
		write_log(0, "Cannot set key without value.\n");
		fuse_reply_err(req, EINVAL);
		return;
	}

	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	write_log(10, "Debug setxattr: namespace = %d, key = %s, flag = %d\n",
		name_space, key, flag);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "Error: setxattr lock_entry fail\n");
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	if (check_permission(req, &stat_data, 2) < 0) { /* WRITE perm needed */
		write_log(0, "Error: setxattr Permission denied "
			"(WRITE needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos);
	if (retcode < 0)
		goto error_handle;
	write_log(10, "Debug setxattr: fetch xattr_page, xattr_page = %lld\n",
		xattr_filepos);

	/* Begin to Insert xattr */
	retcode = insert_xattr(meta_cache_entry, xattr_page, xattr_filepos,
		name_space, key, value, size, flag);
	if (retcode < 0)
		goto error_handle;

	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	write_log(5, "setxattr operation success\n");
	fuse_reply_err(req, 0);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	fuse_reply_err(req, -retcode);
}

/************************************************************************
*
* Function name: hfuse_ll_getxattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, char *name, size_t size
*
*       Summary: Get the value of corresponding name if name has existed.
*                If name is not found, reply error.
*
*************************************************************************/
static void hfuse_ll_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	size_t size)
{
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	XATTR_PAGE *xattr_page;
	struct stat stat_data;
	long long xattr_filepos;
	char key[MAX_KEY_SIZE];
	char name_space;
	int retcode;
	ino_t this_inode;
	size_t actual_size;
	char *value;

	this_inode = real_ino(req, ino);
	value = NULL;
	xattr_page = NULL;
	actual_size = 0;

	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	write_log(10, "Debug getxattr: namespace = %d, key = %s, size = %d\n",
		name_space, key, size);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "Error: getxattr lock_entry fail\n");
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;
	if (check_permission(req, &stat_data, 4) < 0) { /* READ perm needed */
		write_log(0, "Error: getxattr permission denied "
			"(READ needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos);
	if (retcode < 0)
		goto error_handle;

	/* Get xattr if size is sufficient. If size is zero, return actual
	   needed size. If size is non-zero but too small, return error code
	   ERANGE */
	if (size != 0) {
		value = (char *) malloc(sizeof(char) * size);
		if (!value) {
			write_log(0, "Error: Allocate memory error\n");
			retcode = -ENOMEM;
			goto error_handle;
		}
		memset(value, 0, sizeof(char) * size);
	} else {
		value = NULL;
	}
	actual_size = 0;

	retcode = get_xattr(meta_cache_entry, xattr_page, name_space,
		key, value, size, &actual_size);
	if (retcode < 0) /* Error: ERANGE, ENOENT, or others */
		goto error_handle;

	/* Close & unlock meta */
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);

	if (size <= 0) { /* Reply with needed buffer size */
		write_log(5, "Get xattr needed size of %s\n",
				name);
		fuse_reply_xattr(req, actual_size);

	} else { /* Reply with value of given key */
		write_log(5, "Get xattr value %s success\n",
				value);
		fuse_reply_buf(req, value, actual_size);
	}

	/* Free memory */
	if (xattr_page)
		free(xattr_page);
	if (value)
		free(value);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	if (value)
		free(value);
	fuse_reply_err(req, -retcode);
}

/************************************************************************
*
* Function name: hfuse_ll_listxattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, size_t size
*
*       Summary: List all xattr in USER namespace. Reply needed size if
*                parameter "size" is zero. If size is fitted, reply buffer
*                filled with all names separated by null character.
*
*************************************************************************/
static void hfuse_ll_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	XATTR_PAGE *xattr_page;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	ino_t this_inode;
	long long xattr_filepos;
	int retcode;
	char *key_buf;
	size_t actual_size;

	this_inode = real_ino(req, ino);
	key_buf = NULL;
	xattr_page = NULL;
	actual_size = 0;
	write_log(10, "Debug listxattr: Begin listxattr, "
		"given buffer size = %d\n", size);

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "Error: listxattr lock_entry fail\n");
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos);
	if (retcode < 0)
		goto error_handle;

	/* Allocate sufficient size */
	if (size != 0) {
		key_buf = (char *) malloc(sizeof(char) * size);
		if (!key_buf) {
			write_log(0, "Error: Allocate memory error\n");
			retcode = -ENOMEM;
			goto error_handle;
		}
		memset(key_buf, 0, sizeof(char) * size);
	} else {
		key_buf = NULL;
	}
	actual_size = 0;

	retcode = list_xattr(meta_cache_entry, xattr_page, key_buf, size,
		&actual_size);
	if (retcode < 0) /* Error: ERANGE or others */
		goto error_handle;

	/* Close & unlock meta */
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);

	if (size <= 0) { /* Reply needed size */
		write_log(5, "listxattr: Reply needed size = %d\n",
			actual_size);
		fuse_reply_xattr(req, actual_size);

	} else { /* Reply list */
		write_log(5, "listxattr operation success\n");
		fuse_reply_buf(req, key_buf, actual_size);
	}

	/* Free memory */
	if (xattr_page)
		free(xattr_page);
	if (key_buf)
		free(key_buf);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	if (key_buf)
		free(key_buf);
	fuse_reply_err(req, -retcode);
}

/************************************************************************
*
* Function name: hfuse_ll_removexattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, char *name
*
*       Summary: Remove a xattr and reclaim those resource if needed. Reply
*                error if attribute is not found.
*
*************************************************************************/
static void hfuse_ll_removexattr(fuse_req_t req, fuse_ino_t ino,
	const char *name)
{
	XATTR_PAGE *xattr_page;
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	ino_t this_inode;
	long long xattr_filepos;
	int retcode;
	char name_space;
	char key[MAX_KEY_SIZE];
	struct stat stat_data;

	this_inode = real_ino(req, ino);
	xattr_page = NULL;

	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	write_log(10, "Debug removexattr: namespace = %d, key = %s\n",
		name_space, key);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		write_log(0, "Error: removexattr lock_entry fail\n");
		fuse_reply_err(req, ENOMEM);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	if (check_permission(req, &stat_data, 2) < 0) { /* WRITE perm needed */
		write_log(0, "Error: removexattr Permission denied"
			" (WRITE needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos);
	if (retcode < 0)
		goto error_handle;

	/* Remove xattr */
	retcode = remove_xattr(meta_cache_entry, xattr_page, xattr_filepos,
		name_space, key);
	if (retcode < 0) { /* ENOENT or others */
		write_log(0, "Error: removexattr remove xattr fail\n");
		goto error_handle;
	}

	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	write_log(5, "Remove key success\n");
	fuse_reply_err(req, 0);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	fuse_reply_err(req, -retcode);
}

/************************************************************************
*
* Function name: hfuse_ll_link
*        Inputs: fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
*                const char *newname
*
*       Summary: Make a hard link for inode "ino". The hard link is named
*                as "newname" and is added to parent dir "newparent". Type
*                dir is not allowed to make a hard link. Besides, hard link
*                over FS is also not allowed, which is handled by kernel.
*
*************************************************************************/
static void hfuse_ll_link(fuse_req_t req, fuse_ino_t ino,
	fuse_ino_t newparent, const char *newname)
{
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry;
	DIR_ENTRY_PAGE dir_page;
	struct stat parent_stat, link_stat;
	struct fuse_entry_param tmp_param;
	int result_index;
	int ret_val, errcode;
	unsigned long this_generation;
	ino_t parent_inode, link_inode;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		fuse_reply_err(req, ENOTSUP);
		return;
	}
#endif

	parent_inode = real_ino(req, newparent);
	link_inode = real_ino(req, ino);

	/* Reject if name too long */
	if (strlen(newname) > MAX_FILENAME_LEN) {
		write_log(0, "File name is too long\n");
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}
	if (strlen(newname) <= 0) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Error if parent is not a dir */
	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3); /* W+X */
	if (ret_val < 0) {
		write_log(0, "Dir permission denied. W+X is needed\n");
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Check whether "newname" exists or not */
	parent_meta_cache_entry = meta_cache_lock_entry(parent_inode);
	if (!parent_meta_cache_entry) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	ret_val = meta_cache_seek_dir_entry(parent_inode, &dir_page,
		&result_index, newname, parent_meta_cache_entry);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}
	if (result_index >= 0) {
		write_log(0, "File %s existed\n", newname);
		errcode = -EEXIST;
		goto error_handle;
	}

	/* Increase nlink and add "newname" to parent dir */
	ret_val = link_update_meta(link_inode, newname, &link_stat,
		&this_generation, parent_meta_cache_entry);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}

	/* Unlock parent */
	ret_val = meta_cache_close_file(parent_meta_cache_entry);
	if (ret_val < 0) {
		meta_cache_unlock_entry(parent_meta_cache_entry);
		fuse_reply_err(req, -ret_val);
		return;
	}
	ret_val = meta_cache_unlock_entry(parent_meta_cache_entry);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Reply fuse entry */

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_generation;
	tmp_param.ino = (fuse_ino_t) link_inode;
	memcpy(&(tmp_param.attr), &link_stat, sizeof(struct stat));
	if (S_ISREG(link_stat.st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table,
			link_inode, 1, D_ISREG);
	if (S_ISLNK(link_stat.st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table,
			link_inode, 1, D_ISLNK);
	if (S_ISDIR(link_stat.st_mode))
		ret_val = -EISDIR;
	if (ret_val < 0) {
		write_log(0, "Fail to increase lookup count\n");
		fuse_reply_err(req, -ret_val);
	}

	write_log(10, "Debug: Hard link %s is created successfully\n", newname);
	fuse_reply_entry(req, &(tmp_param));
	return;

error_handle:
	meta_cache_close_file(parent_meta_cache_entry);
	meta_cache_unlock_entry(parent_meta_cache_entry);
	fuse_reply_err(req, -errcode);
}

/************************************************************************
*
* Function name: hfuse_ll_create
*        Inputs: fuse_req_t req, fuse_ino_t parent, const char *name,
*                mode_t mode, struct fuse_file_info *fi
*
*       Summary: Create a regular file named as "name" if it does not
*                exist in dir "parent". If it exists, it will be truncated
*                to size = 0. After creating the file, this function will
*                create a file handle and store it in "fi->fh". Finally
*                reply the fuse entry about the file and fuse file info "fi".
*
*************************************************************************/
static void hfuse_ll_create(fuse_req_t req, fuse_ino_t parent,
	const char *name, mode_t mode, struct fuse_file_info *fi)
{
	int ret_val;
	struct stat parent_stat, this_stat;
	ino_t parent_inode, self_inode;
	mode_t self_mode;
	int file_flags;
	struct fuse_ctx *temp_context;
	struct fuse_entry_param tmp_param;
	unsigned long this_generation;
	long long fh;
	MOUNT_T *tmpptr;

	parent_inode = real_ino(req, parent);

	write_log(10, "DEBUG parent %"FMT_INO_T", name %s mode %d\n",
			parent_inode, name, mode);

	/* Reject if not creating a regular file */
	if (!S_ISREG(mode)) {
		fuse_reply_err(req, EPERM);
		return;
	}

	/* Reject if name too long */
	if (strlen(name) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	/* Check parent type */
	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat);
	}
#endif

	if (!S_ISDIR(parent_stat.st_mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3);
	if (ret_val < 0) {
		write_log(0, "Dir permission denied. W+X is needed\n");
		fuse_reply_err(req, -ret_val);
		return;
	}

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	memset(&this_stat, 0, sizeof(struct stat));
	this_stat.st_size = 0;
	this_stat.st_blksize = MAX_BLOCK_SIZE;
	this_stat.st_blocks = 0;
	this_stat.st_dev = 0;
	this_stat.st_nlink = 1;

        self_mode = mode | S_IFREG;
        this_stat.st_mode = self_mode;

	/*Use the uid and gid of the fuse caller*/
	this_stat.st_uid = temp_context->uid;
	this_stat.st_gid = temp_context->gid;

	/* Use the current time for timestamps */
	set_timestamp_now(&this_stat, ATIME | MTIME | CTIME);
	self_inode = super_block_new_inode(&this_stat, &this_generation);
	/* If cannot get new inode number, error is ENOSPC */
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	this_stat.st_ino = self_inode;
	ret_val = mknod_update_meta(self_inode, parent_inode, name,
			&this_stat, this_generation, tmpptr->f_ino);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Prepare stat data to be replied */

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_generation;
	tmp_param.ino = (fuse_ino_t) self_inode;
	memcpy(&(tmp_param.attr), &this_stat, sizeof(struct stat));

#ifdef _ANDROID_ENV_
	if (tmpptr->volume_type == ANDROID_EXTERNAL) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &(tmp_param.attr));
	}
#endif

	ret_val = lookup_increase(tmpptr->lookup_table, self_inode, 1, D_ISREG);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* In create operation, flag is O_WRONLY when opening */
	file_flags = fi->flags;
	fh = open_fh(self_inode, file_flags);
	if (fh < 0) {
		fuse_reply_err(req, ENFILE);
		return;
	}
	fi->fh = fh;
	fuse_reply_create(req, &tmp_param, fi);
}

/* Specify the functions used for the FUSE operations */
struct fuse_lowlevel_ops hfuse_ops = {
	.getattr = hfuse_ll_getattr,
	.mknod = hfuse_ll_mknod,
	.mkdir = hfuse_ll_mkdir,
	.readdir = hfuse_ll_readdir,
	.opendir = hfuse_ll_opendir,
	.access = hfuse_ll_access,
	.rename = hfuse_ll_rename,
	.open = hfuse_ll_open,
	.release = hfuse_ll_release,
	.write = hfuse_ll_write,
	.read = hfuse_ll_read,
	.init = hfuse_ll_init,
	.destroy = hfuse_ll_destroy,
	.releasedir = hfuse_ll_releasedir,
	.setattr = hfuse_ll_setattr,
	.flush = hfuse_ll_flush,
	.fsync = hfuse_ll_fsync,
	.unlink = hfuse_ll_unlink,
	.rmdir = hfuse_ll_rmdir,
	.statfs = hfuse_ll_statfs,
	.lookup = hfuse_ll_lookup,
	.forget = hfuse_ll_forget,
	.symlink = hfuse_ll_symlink,
	.readlink = hfuse_ll_readlink,
#ifndef _ANDROID_ENV_
	.setxattr = hfuse_ll_setxattr,
	.getxattr = hfuse_ll_getxattr,
	.listxattr = hfuse_ll_listxattr,
	.removexattr = hfuse_ll_removexattr,
#endif
	.link = hfuse_ll_link,
	.create = hfuse_ll_create,
};

/* Initiate FUSE */
void *mount_multi_thread(void *ptr)
{
	struct fuse_session *session_ptr;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) ptr;

	session_ptr = tmpptr->session_ptr;
	fuse_session_loop_mt(session_ptr);
	write_log(10, "Unmounting FS with root inode %"FMT_INO_T", %d\n", tmpptr->f_ino,
			tmpptr->is_unmount);

	if (tmpptr->is_unmount == FALSE)
		unmount_event(tmpptr->f_name);

	lookup_destroy(tmpptr->lookup_table, tmpptr);
}
void *mount_single_thread(void *ptr)
{
	struct fuse_session *session_ptr;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) ptr;

	session_ptr = tmpptr->session_ptr;
	fuse_session_loop(session_ptr);
	write_log(10, "Unmounting FS with root inode %"FMT_INO_T", %d\n", tmpptr->f_ino,
			tmpptr->is_unmount);

	if (tmpptr->is_unmount == FALSE)
		unmount_event(tmpptr->f_name);

	lookup_destroy(tmpptr->lookup_table, tmpptr);
}

int hook_fuse(int argc, char **argv)
{
	int dl_count;

	global_argc = argc;
	global_argv = argv;
	pthread_attr_init(&prefetch_thread_attr);
	pthread_attr_setdetachstate(&prefetch_thread_attr,
						PTHREAD_CREATE_DETACHED);
	init_api_interface();
	init_meta_cache_headers();
	startup_finish_delete();
	/* TODO: Ensure that the above is finished before any operation
		can start */
	while (hcfs_system->system_going_down == FALSE)
		sleep(1);
	destroy_mount_mgr();
	destroy_fs_manager();
	release_meta_cache_headers();
	sync();
	for (dl_count = 0; dl_count < MAX_DOWNLOAD_CURL_HANDLE; dl_count++)
		hcfs_destroy_backend(&(download_curl_handles[dl_count]));

	destroy_api_interface();

	return 0;
}

