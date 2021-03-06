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

#define FUSE_USE_VERSION 29
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fuseop.h"

#include <ctype.h>
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
#include <linux/xattr.h>
#include <inttypes.h>
#include <sqlite3.h>
#include <linux/capability.h>

/* Headers from the other libraries */
#include <fuse/fuse_lowlevel.h>
#include <fuse/fuse_common.h>
#include <fuse/fuse_opt.h>

#include "FS_manager.h"
#include "alias.h"
#include "api_interface.h"
#include "atomic_tocloud.h"
#include "dir_statistics.h"
#include "do_fallocate.h"
#include "file_present.h"
#include "filetables.h"
#include "fuse_notify.h"
#include "global.h"
#include "hcfs_cacheops.h"
#include "hcfs_fromcloud.h"
#include "hcfs_tocloud.h"
#include "hcfscurl.h"
#include "logger.h"
#include "lookup_count.h"
#include "macro.h"
#include "meta.h"
#include "meta_mem_cache.h"
#include "metaops.h"
#include "mount_manager.h"
#include "params.h"
#include "parent_lookup.h"
#include "path_reconstruct.h"
#include "pin_scheduling.h"
#include "pkg_cache.h"
#include "super_block.h"
#include "utils.h"
#include "xattr_ops.h"
#ifndef _ANDROID_ENV_
#include "fuseproc_comm.h"
#include <attr/xattr.h>
#endif
#include "rebuild_parent_dirstat.h"
#include "rebuild_super_block.h"
#include "hfuse_system.h"
#include "do_restoration.h"
#include "control_smartcache.h"
#include "apk_mgmt.h"
#include "meta_iterator.h"
/* Steps for allowing opened files / dirs to be accessed after deletion
 *
 *  1. in lookup_count, add a field "to_delete". rmdir, unlink will first
 *     mark this as true and if in forget() the count is dropped to zero,
 *     the inode is deleted.
 *
 *  2. to allow inode deletion fixes due to system crashing, a subfolder
 *     will be created so that the inode number of inodes to be deleted
 *     can be touched here, and removed when actually deleted.
 *
 *  3. in lookup_decrease, should delete nodes when lookup drops to zero
 *     (to save space in the int64_t run).
 *
 *  4. in unmount, can pick either scanning lookup table for inodes to
 *     delete or list the folder.
 */
/* TODO: for mechanisms that needs timer, use per-process or per-thread
 * cpu clock instead of using current time to avoid time changes due to
 * ntpdate / ntpd or manual changes
 */

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

__thread struct timeval last_forget_time;

BOOL _check_capability(pid_t thispid, int32_t cap_to_check);
static int32_t symlink_internal(fuse_req_t req, const char *link,
	fuse_ino_t parent, const char *name, struct fuse_entry_param *tmp_param);

/*!
 * Helper function for checking permissions.
 * @param req           request of FUSE operations
 * @param thisstat      file status
 * @param mode          bitwise ORs of read, write, exec (4, 2, 1)
 * @param root_check    perform root check or not
 */
int32_t check_permission(fuse_req_t req, const HCFS_STAT *thisstat,
                         int32_t mode, BOOL root_check)
{
	struct fuse_ctx *temp_context;
	gid_t *tmp_list = NULL, tmp1_list[10];
	int32_t num_groups, count;
	BOOL is_in_group;

	// root-tera: dirty code
	return 0;

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL)
		return -ENOMEM;

	if (!root_check) {
		/* Check the capabilities of the process */
		if (_check_capability(temp_context->pid, CAP_DAC_OVERRIDE) == TRUE)
			return 0;
	} else {
		/* root check: if exec is set for any for reg files */
		if ((temp_context->uid == 0) ||
		    (_check_capability(temp_context->pid, CAP_DAC_OVERRIDE))) {
			if ((S_ISREG(thisstat->mode)) && (mode & X_OK) &&
			    (!(thisstat->mode & (S_IXUSR | S_IXGRP | S_IXOTH))))
				return -EACCES;
			return 0;
		}
	}

	/* First check owner permission */
	if (temp_context->uid == thisstat->uid) {
		if (mode & 4)
			if (!(thisstat->mode & S_IRUSR))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->mode & S_IWUSR))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->mode & S_IXUSR))
				return -EACCES;
		return 0;
	}

	/* Check group permission */
	if (temp_context->gid == thisstat->gid) {
		if (mode & 4)
			if (!(thisstat->mode & S_IRGRP))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->mode & S_IWGRP))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->mode & S_IXGRP))
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
					thisstat->gid);
		if (tmp_list[count] == thisstat->gid) {
			is_in_group = TRUE;
			break;
		}
	}

	if (tmp_list != tmp1_list)
		free(tmp_list);

	if (is_in_group == TRUE) {
		if (mode & 4)
			if (!(thisstat->mode & S_IRGRP))
				return -EACCES;
		if (mode & 2)
			if (!(thisstat->mode & S_IWGRP))
				return -EACCES;
		if (mode & 1)
			if (!(thisstat->mode & S_IXGRP))
				return -EACCES;
		return 0;
	}

	/* Check others */

	if (mode & 4)
		if (!(thisstat->mode & S_IROTH))
			return -EACCES;
	if (mode & 2)
		if (!(thisstat->mode & S_IWOTH))
			return -EACCES;
	if (mode & 1)
		if (!(thisstat->mode & S_IXOTH))
			return -EACCES;
	return 0;
}

int32_t is_member(fuse_req_t req, gid_t this_gid, gid_t target_gid)
{
	gid_t *tmp_list, tmp1_list[10];
	int32_t num_groups, count;
	BOOL is_in_group;

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

/* Copy from real_ino(). Get the original inode number. */
ino_t org_real_ino(fuse_req_t req, fuse_ino_t ino)
{
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Root inode is %" PRIu64 "\n", (uint64_t)tmpptr->f_ino);

	if (ino == 1)
		return tmpptr->f_ino;
	else
		return (ino_t) ino;
}

/* Helper function for translating from FUSE inode number to real one */
ino_t real_ino(fuse_req_t req, fuse_ino_t ino)
{
	ino_t org_ino = org_real_ino(req, ino);

#ifdef _ANDROID_ENV_
	/* Translating from alias inode number to real inode number. */
	if (IS_ALIAS_INODE(org_ino))
		org_ino = get_real_ino_in_alias_group(org_ino);
#endif

	return org_ino;
}

void to_lowercase(const char *org_name, char *lower_case)
{
	int i;
	if (!org_name || !lower_case)
		return;

	for (i = 0; org_name[i]; i++) {
		lower_case[i] = tolower(org_name[i]);
	}
	lower_case[i] = '\0';
}

#ifdef _ANDROID_ENV_
/* Internal function for generating the ownership / permission for
Android external storage */
#define PKG_DB_PATH "/data/data/com.hopebaytech.hcfsmgmt/databases/uid.db"

static int32_t _lookup_pkg_cb(void *data, int32_t argc, char **argv, char **azColName)
{

	size_t uid_len;
	char **uid = (char **)data;

	UNUSED(argc);
	UNUSED(azColName);
	if (!argv[0])
		return -1;

	uid_len = strlen(argv[0]);
	*uid = malloc(sizeof(char) * (uid_len + 1));
	snprintf(*uid, uid_len + 1, "%s", argv[0]);
	return 0;
}

static int32_t _lookup_pkg_status_cb(void *data, int32_t argc, char **argv, char **azColName)
{

	int32_t *res = (int32_t *)data;

	UNUSED(argc);
	UNUSED(azColName);
	if (!argv[0] || !argv[1])
		return -1;

	res[0] = atoi(argv[0]);
	res[1] = atoi(argv[1]);
	return 0;
}

static int32_t _lookup_pkg_whitelist_cb(void *data, int32_t argc, char **argv, char **azColName)
{

	UNUSED(argc);
	UNUSED(azColName);
	if (!argv[0])
		return -1;

	*(int32_t *)data = atoi(argv[0]);
	return 0;
}

/*
 * Helper function for querying uid of input (pkgname),
 * result uid will be stored in (uid).
 *
 * @return - 0 for success, otherwise -1.
 */
int32_t lookup_pkg(char *pkgname, uid_t *uid)
{

	sqlite3 *db;
	int32_t ret_code, ret;
	char *data;
	char *sql_err = 0;
	char sql[500];

	write_log(8, "Looking up pkg %s\n", pkgname);
	/* Return uid 0 if error occurred */
	data = NULL;
	*uid = 0;

	ret = lookup_cache_pkg(pkgname, uid);
	if (ret == 0) {
		return 0;
	} else {
		if (ret != -ENOENT)
			return -1;
	}

	/* Find pkg from db */
	snprintf(sql, sizeof(sql),
		 "SELECT uid from uid WHERE package_name='%s'",
		 pkgname);

	if (access(PKG_DB_PATH, F_OK) != 0) {
		write_log(4, "Query pkg uid err (open db) - db file not existed\n");
		return -1;
	}

	ret_code = sqlite3_open(PKG_DB_PATH, &db);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg uid err (open db) - %s\n", sqlite3_errmsg(db));
		return -1;
	}

	/* Sql statement */
	ret_code =
	    sqlite3_exec(db, sql, _lookup_pkg_cb, (void *)&data, &sql_err);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg uid err (sql statement) - %s\n", sql_err);
		sqlite3_free(sql_err);
		sqlite3_close(db);
		return -1;
	}

	sqlite3_close(db);

	if (data == NULL) {
		write_log(8, "Query pkg uid err (sql statement) - pkg not found\n");
		*uid = 0;
	} else {
		*uid = (uid_t)atoi(data);
		write_log(8, "Fetch pkg uid %d, %d\n", *uid, data);
	}

	ret = insert_cache_pkg(pkgname, *uid);
	if (ret < 0) {
		free(data);
		return -1;
	}

	free(data);
	return 0;
}

/*
 * Helper function for querying status of input (pkgname),
 * result pin will be one of: 0 = unpin, 1 = pin, 2 = high priority pin.
 * result issys will be TRUE if pkg is system app, otherwise FALSE.
 *
 * @return - 0 for success, otherwise -1.
 */
int32_t lookup_pkg_status(const char *pkgname, PIN_t *pin_status, BOOL *issys)
{

	sqlite3 *db;
	int32_t ret_code;
	int32_t status_code[2] = {-1, -1};
	char *sql_err = 0;
	char sql[500];

	write_log(8, "Looking up pkg status %s\n", pkgname);

	snprintf(sql, sizeof(sql),
		 "SELECT system_app,pin_status from uid WHERE package_name='%s'",
		 pkgname);

	if (access(PKG_DB_PATH, F_OK) != 0) {
		write_log(4, "Query pkg status err (open db) - db file not existed\n");
		return -1;
	}

	ret_code = sqlite3_open(PKG_DB_PATH, &db);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg status err (open db) - %s\n", sqlite3_errmsg(db));
		return -1;
	}

	/* Sql statement */
	ret_code = sqlite3_exec(db, sql, _lookup_pkg_status_cb,
				(void *)status_code, &sql_err);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg status err (sql statement) - %s\n", sql_err);
		sqlite3_free(sql_err);
		sqlite3_close(db);
		return -1;
	}

	sqlite3_close(db);

	if (status_code[0] < 0 || status_code[1] < 0) {
		write_log(4, "Query pkg status err (sql statement) - pkg not found\n");
		return -1;
	}

	write_log(8, "Fetch pkg status issys = %d, pin_status = %d\n",
		  status_code[0], status_code[1]);

	*issys = status_code[0];
	*pin_status = status_code[1];
	return 0;
}

static inline void _android6_permission(HCFS_STAT *thisstat,
					char mp_mode,
					mode_t *newpermission)
{
	if (mp_mode == MP_DEFAULT) {
		if (!S_ISDIR(thisstat->mode))
			*newpermission = 0660;
		else
			*newpermission = 0771;
	} else if (mp_mode == MP_READ) {
		if (!S_ISDIR(thisstat->mode))
			*newpermission = 0640;
		else
			*newpermission = 0750;
	} else {
		if (!S_ISDIR(thisstat->mode))
			*newpermission = 0660;
		else
			*newpermission = 0770;
	}
}


static inline void _try_get_pin_st(const char *tmptok_prev, PIN_t *pin_status)
{
	PIN_t pin;
	BOOL issys;
	int32_t ret;

	ret = lookup_pkg_status(tmptok_prev, &pin, &issys);
	/* Get pin st if success */
	if (ret == 0) {
		if (issys == TRUE) {
			*pin_status = P_HIGH_PRI_PIN;
		} else {
			if (P_IS_VALID_PIN(pin))
				*pin_status = (PIN_t) pin;
			else
				write_log(0, "Error: Lookup pin status "
						"is not valid value\n");
		}
	} else {
		write_log(4, "Pin status of pkg %s is not found\n",
				tmptok_prev);
	}
}

int32_t _rewrite_stat(MOUNT_T *tmpptr,
		      HCFS_STAT *thisstat,
		      const char *selfname,
		      PIN_t *pin_status)
{
	int32_t ret, errcode;
	char *tmppath;
	mode_t tmpmask, newpermission;
	char *tmptok, *tmptoksave, *tmptok_prev;
	int32_t count;
	uid_t tmpuid;
	char volume_type, mp_mode;
	BOOL keep_check;

	tmppath = NULL;
	tmptok_prev = NULL;
	tmptok = NULL;
	tmptoksave = NULL;
	write_log(10, "Debug rewrite stat inode %" PRIu64,
		  (uint64_t)thisstat->ino);
	ret = construct_path(tmpptr->vol_path_cache, thisstat->ino,
				&tmppath, tmpptr->f_ino);
	if (ret < 0) {
		if (tmppath != NULL)
			free(tmppath);
		errcode = ret;
		goto errcode_handle;
	}
	write_log(10, "Debug path lookup %s\n", tmppath);

	volume_type = tmpptr->volume_type;
	mp_mode = tmpptr->mp_mode;
	write_log(10, "Debug: volume type %d, mp_mode %d\n",
			(int32_t)volume_type, (int32_t)mp_mode);

	/* For android 6.0, first check folders/files under root. */
	if (volume_type == ANDROID_MULTIEXTERNAL) {
		tmptok = strtok_r(tmppath, "/", &tmptoksave);
		/* Root in android 6.0 */
		if (tmptok == NULL) {
			newpermission = 0711;
			keep_check = FALSE;
		} else {
			if (is_natural_number(tmptok) == TRUE) {
				keep_check = TRUE;

			} else {
				/* Not multi-user folder */
				_android6_permission(thisstat, mp_mode,
						&newpermission);
				keep_check = FALSE;

			}
		}

		if (keep_check == FALSE) {
			thisstat->uid = 0;
			if (tmpptr->mp_mode == MP_DEFAULT) /* default */
				thisstat->gid = GID_SDCARD_RW;
			else /* read/write */
				thisstat->gid = GID_EVERYBODY;
			tmpmask = 0777;
			tmpmask = ~tmpmask;
			tmpmask = tmpmask & thisstat->mode;
			thisstat->mode = tmpmask | newpermission;
			free(tmppath);
			return 0;
		}
	}

	/* For android 6.0, begin to check /x/<file/folder>, for 5.0,
	 * check /<file/folder> */
	for (count = 0; count < 4; count++) {
		if (count == 0 && volume_type != ANDROID_MULTIEXTERNAL)
			tmptok = strtok_r(tmppath, "/", &tmptoksave);
		else
			tmptok = strtok_r(NULL, "/", &tmptoksave);

		write_log(10, "%s, %s\n", tmptok, tmptok_prev);
		if (tmptok == NULL) {
			switch (count) {
			case 0:
				/* It is root for android 5.0 */
				thisstat->uid = 0;
				thisstat->gid = 1028;
				newpermission = 0770;
				break;
			case 1:
				/* Is /Android for 5.0, /x/Android for 6.0 */
				if (!S_ISDIR(thisstat->mode)) {
					/* If not a directory */
					thisstat->uid = 0;
					thisstat->gid = 1028;
					newpermission = 0770;
				} else {
					thisstat->uid = 0;
					thisstat->gid = 1028;
					newpermission = 0771;
				}
				break;
			case 2:
				if (!S_ISDIR(thisstat->mode)) {
					/* If not a directory */
					thisstat->uid = 0;
					thisstat->gid = 1028;
					newpermission = 0770;
				} else {
					/* If a subfolder under /Android */
					thisstat->uid = 0;
					thisstat->gid = 1028;
					newpermission = 0771;
					/* When parent is /0/Android/data/,
					 * need to check self pkg name */
					if (selfname) {
						if (pin_status != P_UNPIN)
						/* Create external pkg
						 * folder */
							_try_get_pin_st(
								selfname,
								pin_status);
						else
						/* Remove external pkg
						 * folder */
							remove_cache_pkg(
								selfname);
					}
				}
				break;
			case 3:
				if (!S_ISDIR(thisstat->mode)) {
					/* If not a directory */
					thisstat->uid = 0;
					thisstat->gid = 1028;
					newpermission = 0770;
				} else {
					/* If this is a package folder */
					/* Need to lookup package uid */
					lookup_pkg(tmptok_prev, &tmpuid);
					/* If lookup failed in the previous step,
					no need to lookup pin status */
					if ((pin_status != P_UNPIN) && (tmpuid != 0))
						_try_get_pin_st(tmptok_prev,
								pin_status);
					thisstat->uid = tmpuid;
					thisstat->gid = 1028;
					newpermission = 0770;
				}
				break;
			default:
				thisstat->uid = 0;
				thisstat->gid = 1028;
				newpermission = 0770;
				break;
			}
			break;
		}
		if (count == 3) {
			lookup_pkg(tmptok_prev, &tmpuid);
			/* If lookup failed in the previous step,
			no need to lookup pin status */
			if ((pin_status != P_UNPIN) && (tmpuid != 0))
				_try_get_pin_st(tmptok_prev,
						pin_status);
			thisstat->uid = tmpuid;
			thisstat->gid = 1028;
			newpermission = 0770;
			break;
		}
		tmptok_prev = tmptok;
		/* Make path comparison in emulated case-insensitive */
		if ((count == 0) && (strcasecmp(tmptok, "Android") != 0)) {
			/* Not under /Android for android 5.0 */
			thisstat->uid = 0;
			thisstat->gid = 1028;
			newpermission = 0770;
			break;
		}
	}

	/* Modify permission and gid for android 6.0 */
	if (volume_type == ANDROID_MULTIEXTERNAL) {
		_android6_permission(thisstat, mp_mode,
				&newpermission);
		if (tmpptr->mp_mode == MP_DEFAULT) /* default */
			thisstat->gid = GID_SDCARD_RW;
		else /* read/write */
			thisstat->gid = GID_EVERYBODY;
	}

	tmpmask = 0777;
	tmpmask = ~tmpmask;
	tmpmask = tmpmask & thisstat->mode;
	thisstat->mode = tmpmask | newpermission;
	free(tmppath);
	return 0;
errcode_handle:
	return errcode;
}
#endif /* _ANDROID_ENV_ */

int notify_avail_space(long avail_space)
{
	static int fd = -1;

	if (fd == -1) {
		fd = open("/dev/fuse", O_RDONLY);
		if (fd == -1)
			return errno;
	}

	if (avail_space < 0)
		avail_space = 0;

	if (ioctl(fd, FUSE_HCFS_AVAIL_SPACE_NOTIFY, avail_space) == -1)
		return errno;

	return 0;
}

int test_and_notify_no_space(int error)
{
	if (error != ENOSPC)
		return 0;

	return notify_avail_space(0);
}

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
	ino_t hit_inode, org_ino;
	int32_t ret_code;
	struct timeval tmp_time1, tmp_time2;
	HCFS_STAT tmp_stat;
	struct stat ret_stat; /* fuse reply sys stat */
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	UNUSED(fi);

	write_log(8, "Debug getattr inode %ld\n", ino);
	gettimeofday(&tmp_time1, NULL);
	org_ino = org_real_ino(req, ino);
	hit_inode = real_ino(req, ino);

	write_log(10, "Debug getattr hit inode %" PRIu64 "\n",
		  (uint64_t)hit_inode);

	if (hit_inode > 0) {
#ifdef _ANDROID_ENV_
		tmpptr = (MOUNT_T *) fuse_req_userdata(req);
#endif

		ret_code = fetch_inode_stat(hit_inode, &tmp_stat, NULL, NULL);
		if (ret_code < 0) {
			fuse_reply_err(req, -ret_code);
			return;
		}

#ifdef _ANDROID_ENV_
		if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
			if (tmpptr->vol_path_cache == NULL) {
				fuse_reply_err(req, EIO);
				return;
			}
			_rewrite_stat(tmpptr, &tmp_stat, NULL, NULL);
			tmp_stat.ino = org_ino;
		}
#endif

		write_log(10, "Debug getattr return inode %" PRIu64 "\n",
				(uint64_t)tmp_stat.ino);
		gettimeofday(&tmp_time2, NULL);

		write_log(10, "getattr elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));
		convert_hcfsstat_to_sysstat(&ret_stat, &tmp_stat);

		// root-tera: dirty code
		{
			static int get_sdcard_attr_ok = 0;
			static struct stat s;
			if (!get_sdcard_attr_ok) {
				get_sdcard_attr_ok = !stat("/sdcard/", &s);
			}
			if (get_sdcard_attr_ok) {
				ret_stat.st_uid = s.st_uid;
				ret_stat.st_gid = s.st_gid;
				if (ret_stat.st_mode & S_IFDIR)
					ret_stat.st_mode |= 0777;
				else
					ret_stat.st_mode = (ret_stat.st_mode & ~0777) | 0666;
			}
		}

		fuse_reply_attr(req, &ret_stat, REPLY_ATTR_TIMEOUT);
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
	HCFS_STAT this_stat;
	mode_t self_mode;
	int32_t ret_val;
	struct fuse_ctx *temp_context;
	int32_t ret_code;
	struct timeval tmp_time1, tmp_time2;
	struct fuse_entry_param tmp_param;
	HCFS_STAT parent_stat;
	uint64_t this_generation;
	MOUNT_T *tmpptr;
	char local_pin;
	PIN_t pin;
	int64_t delta_meta_size;
	BOOL is_external = FALSE;

	write_log(10,
		"DEBUG parent %ld, name %s mode %d\n", parent, selfname, mode);

	if (NO_META_SPACE()) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	gettimeofday(&tmp_time1, NULL);

	/* Reject if not creating a regular file or fifo */
	if (!S_ISFILE(mode)) {
		fuse_reply_err(req, EPERM);
		return;
	}

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	parent_inode = real_ino(req, parent);

	write_log(8, "Debug mknod: name %s, parent %" PRIu64 "\n", selfname,
		  (uint64_t)parent_inode);

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	ret_val = fetch_inode_stat(parent_inode, &parent_stat,
			NULL, &local_pin);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	pin = P_INVALID;
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, NULL, &pin);
		is_external = TRUE;
	}
	/* Inherit parent pin status if "pin" is not modified */
	if (pin == P_INVALID)
		pin = local_pin;
#else
	/* Default inherit parent's pin status */
	pin = local_pin;
#endif

	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	init_hcfs_stat(&this_stat);

	this_stat.dev = dev;
	this_stat.size = 0;
	this_stat.blksize = ST_BLKSIZE;
	this_stat.blocks = 0;
	this_stat.nlink = 1;

	self_mode = mode;
	this_stat.mode = self_mode;

	/*Use the uid and gid of the fuse caller*/

	this_stat.uid = temp_context->uid;
	this_stat.gid = temp_context->gid;

	/* Use the current time for timestamps */
	set_timestamp_now(&this_stat, A_TIME | M_TIME | C_TIME);

	self_inode = super_block_new_inode(&this_stat, &this_generation,
			pin);
	/* If cannot get new inode number, error is ENOSPC */
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	this_stat.ino = self_inode;

	write_log(6, "Mknod: parent %" PRIu64 ", self %" PRIu64
	          ", name %s\n", (uint64_t) parent_inode, (uint64_t) self_inode,
	          selfname);

	ret_code = mknod_update_meta(self_inode, parent_inode, selfname,
			&this_stat, this_generation, tmpptr,
			&delta_meta_size, pin, is_external);

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
	convert_hcfsstat_to_sysstat(&(tmp_param.attr), &this_stat);

	ret_code = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISREG);
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}

	if (delta_meta_size != 0)
		change_system_meta(delta_meta_size, 0, 0, 0, 0, 0, TRUE);
	ret_val = change_mount_stat(tmpptr, 0, delta_meta_size, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	fuse_reply_entry(req, &(tmp_param));
}

static inline BOOL pkg_in_whitelist(const char *pkgname)
{

	sqlite3 *db;
	int32_t ret_code;
	int32_t res = -1;
	char *sql_err = 0;
	char sql[MAX_FILENAME_LEN + 200];

	if (mgmt_app_is_created == FALSE)
		return FALSE;

	/* Reject if smartcache not found */
	write_log(8, "Debug: check hcfsblock");
	if (access("/data/smartcache/hcfsblock", F_OK) < 0) {
		write_log(6, "Lookup %s but smartcache not found. Code %d",
				pkgname, errno);
		return FALSE;
	}

	write_log(6, "Looking up pkg %s\n", pkgname);
	/* Reject if db not found */
	if (access(PKG_DB_PATH, F_OK) < 0) {
		write_log(4, "Query pkg whitelist err (access db). Code %d",
				errno);
		return FALSE;
	}

	/* Find pkg from db */
	snprintf(sql, sizeof(sql),
		 "SELECT EXISTS"
		 "(SELECT * FROM booster_white_list "
		 "WHERE package_name='%s')", pkgname);

	ret_code = sqlite3_open(PKG_DB_PATH, &db);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg whitelist err (open db) - %s\n",
				sqlite3_errmsg(db));
		return FALSE;
	}

	/* Sql statement */
	ret_code = sqlite3_exec(db, sql, _lookup_pkg_whitelist_cb,
				(void *)&res, &sql_err);
	if (ret_code != SQLITE_OK) {
		write_log(4, "Query pkg whitelist err (sql statement) - %s\n",
				sql_err);
		sqlite3_free(sql_err);
		sqlite3_close(db);
		return FALSE;
	}
	sqlite3_close(db);

	return res == 1 ? TRUE : FALSE;
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
	HCFS_STAT this_stat;
	mode_t self_mode;
	int32_t ret_val;
	struct fuse_ctx *temp_context;
	int32_t ret_code, ret;
	struct timeval tmp_time1, tmp_time2;
	struct fuse_entry_param tmp_param;
	HCFS_STAT parent_stat;
	uint64_t this_gen;
	MOUNT_T *tmpptr;
	char local_pin;
	PIN_t pin;
	int64_t delta_meta_size;
	BOOL is_external = FALSE;

	if (NO_META_SPACE()) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	parent_inode = real_ino(req, parent);

	write_log(8, "Debug mkdir: name %s, parent %" PRIu64 "\n", selfname,
		  (uint64_t)parent_inode);

	gettimeofday(&tmp_time1, NULL);
#ifdef _ANDROID_ENV_
	if ((parent_inode == data_data_root) && pkg_in_whitelist(selfname)) {
		char link_target[MAX_FILENAME_LEN + 100];

		sprintf(link_target, "/data/mnt/hcfsblock/%s", selfname);
		ret = mkdir(link_target, mode);
		if (ret == 0 || (ret < 0 && errno == EEXIST)) {
			ret = symlink_internal(req, link_target,
					parent, selfname, &tmp_param);
			if (ret == 0) {
				tmp_param.attr.st_mode = mode | S_IFDIR;
				fuse_reply_entry(req, &(tmp_param));
				return;
			} else {
				write_log(4, "Failed to create symlink."
						" Code %d", -ret);
				rmdir(link_target);
				errno = -ret;
			}
		}
		write_log(4, "Error: Failed to create symlink and smartcache "
				"folder. Just create folder %s under "
				"/data/data. Code %d", selfname, errno);
	}
#endif

	ret_val = fetch_inode_stat(parent_inode, &parent_stat,
			NULL, &local_pin);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	pin = P_INVALID;
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, selfname, &pin);
		is_external = TRUE;
	}
	/* Inherit parent pin status if "pin" is not modified */
	if (pin == P_INVALID)
		pin = local_pin;
#else
	/* Default inherit parent's pin status */
	pin = local_pin;
#endif
	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	init_hcfs_stat(&this_stat);
	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	this_stat.nlink = 2; /*One pointed by the parent, another by self*/

	self_mode = mode | S_IFDIR;
	this_stat.mode = self_mode;

	/*Use the uid and gid of the fuse caller*/
	this_stat.uid = temp_context->uid;
	this_stat.gid = temp_context->gid;

	set_timestamp_now(&this_stat, A_TIME | M_TIME | C_TIME);

	this_stat.size = 0;
	this_stat.blksize = ST_BLKSIZE;
	this_stat.blocks = 0;

	self_inode = super_block_new_inode(&this_stat, &this_gen, pin);
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	this_stat.ino = self_inode;

	write_log(6, "Mkdir: parent %" PRIu64 ", self %" PRIu64
	          ", name %s\n", (uint64_t) parent_inode, (uint64_t) self_inode,
	          selfname);

	delta_meta_size = 0;
	ret_code = mkdir_update_meta(self_inode, parent_inode,
			selfname, &this_stat, this_gen,
			tmpptr, &delta_meta_size, pin,
			is_external);
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_gen;
	tmp_param.ino = (fuse_ino_t) self_inode;
	convert_hcfsstat_to_sysstat(&(tmp_param.attr), &this_stat);

	ret_code = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISDIR);
	if (ret_code < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_code);
		return;
	}

	if (delta_meta_size != 0)
		change_system_meta(delta_meta_size, 0, 0, 0, 0, 0, TRUE);
	ret_val = change_mount_stat(tmpptr, 0, delta_meta_size, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	if (parent_inode == data_data_root) {
		/*Check if need to cleanup package lookup cache */
		remove_cache_pkg(selfname);
	}

	/* Check if mgmt pkg folder is created. */
	if (parent_inode == data_data_root && mgmt_app_is_created == FALSE &&
	    !strcmp(selfname, "com.hopebaytech.hcfsmgmt"))
		mgmt_app_is_created = TRUE;
#endif

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
#ifdef _ANDROID_ENV_
	ino_t this_inode;
	MOUNT_T *tmpptr = (MOUNT_T *)fuse_req_userdata(req);
#endif
	ino_t parent_inode, alias_ino;
	int32_t ret_val;
	DIR_ENTRY temp_dentry;
	HCFS_STAT parent_stat;
	BOOL is_external = FALSE;
	char lowercase[MAX_FILENAME_LEN+1], *alias_name;

	parent_inode = real_ino(req, parent);
	write_log(8, "Debug unlink: name %s, parent %" PRIu64 "\n", selfname,
		  (uint64_t)parent_inode);

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, NULL, NULL);
		is_external = TRUE;
	}
#endif

	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = lookup_dir(parent_inode, selfname, &temp_dentry, is_external);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	this_inode = temp_dentry.d_ino;
#endif

	ret_val =
	    unlink_update_meta(req, parent_inode, &temp_dentry, is_external);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_

	/* Remove minapk if delete apk */
	if ((tmpptr->f_ino == hcfs_system->data_app_root) &&
	    (is_apk(selfname) == TRUE)) {
		char minapk_name[MAX_FILENAME_LEN] = {0};
		DIR_ENTRY minapk_entry;
		ret_val = convert_minapk(selfname, minapk_name);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
		ret_val = lookup_dir(parent_inode, minapk_name,
		                     &minapk_entry, is_external);
		if (ret_val < 0 && ret_val != -ENOENT) {
			fuse_reply_err(req, -ret_val);
			return;
		}

		if (ret_val == 0) {
			ret_val = unlink_update_meta(req, parent_inode,
			                             &minapk_entry,
			                             is_external);
			if (ret_val < 0 && ret_val != -ENOENT) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		if (ret_val == 0) {
			ret_val = hfuse_ll_notify_delete(tmpptr->chan_ptr,
			                                 parent_inode,
			                                 minapk_entry.d_ino,
			                                 minapk_name,
			                                 strlen(minapk_name));
			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		}
	}

	/* Handle removal of minimal apk */
	if ((hcfs_system->use_minimal_apk == TRUE) &&
	    (tmpptr->f_ino == hcfs_system->data_app_root)) {
		if (is_apk(selfname) == TRUE) {
			ret_val = remove_minapk_data(parent_inode, selfname);
			if (ret_val < 0 && ret_val != -ENOENT) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		} else if (is_minapk(selfname) == TRUE) {
			char origin_apk[MAX_FILENAME_LEN] = {0};

			ret_val = convert_origin_apk(origin_apk, selfname);
			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
			ret_val = remove_minapk_data(parent_inode, origin_apk);
			if (ret_val < 0 && ret_val != -ENOENT) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		}
	}

	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		ret_val =
		    delete_pathcache_node(tmpptr->vol_path_cache, this_inode);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}

		/* Transfer the name to lower case. */
		to_lowercase(temp_dentry.d_name, lowercase);

		/* Notify to delete all alias inodes. */
		while (1) {
			alias_name = get_name_in_alias_group(
							temp_dentry.d_ino, lowercase, &alias_ino);
			if (alias_name == NULL)
				break;

			/* Tell all views that the alias inode is gone. This
			 * can be done by passing selfname as a fake name. */
			ret_val = hfuse_ll_notify_delete_mp(
				tmpptr->chan_ptr, parent_inode, alias_ino,
				alias_name, strlen(alias_name), selfname);

			free(alias_name);

			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		/* Tell all other views that node is gone; if filename
		 * has different case then tell all views */
		ret_val = hfuse_ll_notify_delete_mp(
		    tmpptr->chan_ptr, parent_inode, temp_dentry.d_ino,
		    temp_dentry.d_name, strlen(selfname), selfname);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}
#endif
	fuse_reply_err(req, -ret_val);
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
	ino_t this_inode, parent_inode, alias_ino;
	int32_t ret_val;
	DIR_ENTRY temp_dentry;
	HCFS_STAT parent_stat;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif
	BOOL is_external = FALSE;
	char lowercase[MAX_FILENAME_LEN+1], *alias_name;

	parent_inode = real_ino(req, parent);
	write_log(8, "Debug rmdir: name %s, parent %" PRIu64 "\n", selfname,
			(uint64_t)parent_inode);
	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, selfname, NULL);
		is_external = TRUE;
	}
#endif

	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);

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

	ret_val = lookup_dir(parent_inode, selfname, &temp_dentry,
			     is_external);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	this_inode = temp_dentry.d_ino;
	write_log(10, "Debug rmdir: name %s, %" PRIu64 "\n",
			temp_dentry.d_name, (uint64_t)this_inode);
	ret_val = rmdir_update_meta(req, parent_inode, this_inode, selfname,
				    is_external);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

#ifdef _ANDROID_ENV_
	/* Check mgmt pkg folder. */
	if (parent_inode == data_data_root && mgmt_app_is_created == TRUE &&
	    !strcmp(selfname, "com.hopebaytech.hcfsmgmt"))
		mgmt_app_is_created = FALSE;

	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		ret_val =
		    delete_pathcache_node(tmpptr->vol_path_cache, this_inode);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}

		/* Transfer the name to lower case. */
		to_lowercase(temp_dentry.d_name, lowercase);

		/* Notify to delete all alias inodes. */
		while (1) {
			alias_name = get_name_in_alias_group(
							temp_dentry.d_ino, lowercase, &alias_ino);
			if (alias_name == NULL)
				break;

			/* Tell all views that the alias inode is gone. This
			 * can be done by passing selfname as a fake name. */
			ret_val = hfuse_ll_notify_delete_mp(
				tmpptr->chan_ptr, parent_inode, alias_ino,
				alias_name, strlen(alias_name), selfname);

			free(alias_name);

			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		/* Tell all other views that node is gone; if filename
		 * has different case then tell all views */
		ret_val = hfuse_ll_notify_delete_mp(
		    tmpptr->chan_ptr, parent_inode, temp_dentry.d_ino,
		    temp_dentry.d_name, strlen(selfname), selfname);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}
	/* TODO: Check if this still works for app to sdcard */
	if (parent_inode == data_data_root) {
		/*Check if need to cleanup package lookup cache */
		remove_cache_pkg(selfname);
	}
#endif

	fuse_reply_err(req, -ret_val);
}

/* Helper function on checking whether to use minimal apk */
/* Returns 0 if check completed normally, and negative of error if check
terminated abnormally. Value of "*minapk_ino" is non-zero if minimal apk
is found and we will use it, otherwise the value is zero. */
static inline int32_t _check_use_minapk(ino_t parent_ino, const char *selfname,
                                        MIN_APK_LOOKUP_DATA *minapk_data,
                                        ino_t apk_ino)
{
	/* TODO: 4. Check if minimal apk exists */
	/* TODO: 5. Check if apk is local */
	char minapk_name[MAX_FILENAME_LEN+1];
	int32_t errcode, ret;
	ino_t *parentlist;
	int32_t numparents, count;
	DIR_ENTRY temp_dentry;

	memset(minapk_data, 0, sizeof(MIN_APK_LOOKUP_DATA));

	/* First check if the parent is an app folder under the root
	of /data/app */
	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
		          strerror(errcode));
		write_log(6, "Error location at %s\n", __func__);
		errcode = -errcode;
		return errcode;
	}
	parentlist = NULL;
	ret = fetch_all_parents(parent_ino, &numparents, &parentlist);

	/* Check parent only if fetch parent is successful */
	if (ret >= 0) {
		for (count = 0; count < numparents; count++)
			if (parentlist[count] == hcfs_system->data_app_root)
				break;
	}
	free(parentlist);
	parentlist = NULL;
	sem_post(&(pathlookup_data_lock));

	/* Return if encountered an error */
	if (ret < 0)
		return ret;

	/* If parent folder is not an app folder under /data/app, do not
	use minimal apk */
	/* For kitkat: If parent folder is /data/app, use min apk */
	if ((count == numparents) && (parent_ino != hcfs_system->data_app_root))
		return 0;

	ret = convert_minapk(selfname, minapk_name);
	if (ret < 0)
		return ret;

	memset(&temp_dentry, 0, sizeof(DIR_ENTRY));

	/* Check if the minimal apk exists in the same folder */
	ret = lookup_dir(parent_ino, minapk_name, &temp_dentry, FALSE);

	write_log(6, "[App unpin] Min apk %s found? %s\n", minapk_name,
	          ret == 0 ? "True" : "False");
	/* Still return 0 if cannot find minimal apk */
	if (ret == -ENOENT)
		return 0;
	else if (ret < 0) 
		return ret;

	/* Check if the original apk is "local" */
	ret = check_data_location(apk_ino);
	/* Don't use the minimal apk if the check failed or the apk is
	"local" */
	if (ret < 0)
		return ret;

	/* Found the minimal apk */

	if (ret == 0)
		minapk_data->is_complete_apk = true;
	else
		minapk_data->is_complete_apk = false;
	minapk_data->min_apk_ino = temp_dentry.d_ino;
	minapk_data->org_apk_ino = apk_ino;
	return 0;
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

	ino_t this_ino, parent_ino;
	int32_t ret_val;
	DIR_ENTRY temp_dentry;
	struct fuse_entry_param output_param;
	uint64_t this_gen;
	HCFS_STAT parent_stat, this_stat;
	MOUNT_T *tmpptr;
	BOOL is_external = FALSE;

	parent_ino = real_ino(req, parent);

	write_log(8, "Debug lookup parent %" PRIu64 ", name %s\n",
			(uint64_t)parent_ino, selfname);

	/* Reject if name too long */
	if (strlen(selfname) > MAX_FILENAME_LEN) {
		fuse_reply_err(req, ENAMETOOLONG);
		return;
	}

	ret_val = fetch_inode_stat(parent_ino, &parent_stat, NULL, NULL);

	write_log(10, "Debug lookup parent mode %d\n", parent_stat.mode);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, NULL, NULL);
		is_external = TRUE;
	}
#endif

	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 1, FALSE);

	if (ret_val < 0) {
		write_log(6, "[lookup] Permission not granted for parent %"
		          PRIu64 ", name %s\n", (uint64_t)parent_ino, selfname);
		fuse_reply_err(req, -ret_val);
		return;
	}

	memset(&output_param, 0, sizeof(struct fuse_entry_param));
	memset(&temp_dentry, 0, sizeof(DIR_ENTRY));

	/* Proceed on checking whether to use minimal apk here */
	if (((hcfs_system->use_minimal_apk == TRUE) &&
	    (tmpptr->f_ino == hcfs_system->data_app_root)) &&
	    (is_apk(selfname) == TRUE)) {
		ino_t minapk_ino;

		/* Query hash table for cached result */
		ret_val = query_minapk_data(parent_ino, selfname, &minapk_ino);
		if ((ret_val == 0) && (minapk_ino > 0)) {
			/* Reuse info for stored minimal apk */
			temp_dentry.d_ino = minapk_ino;
			snprintf(temp_dentry.d_name, sizeof(temp_dentry.d_name),
			         "%s", selfname);
		} else if ((ret_val == 0) && (minapk_ino == 0)) {
			/* There is no minimal apk. Use the original one */
			ret_val = lookup_dir(parent_ino, selfname, &temp_dentry,
					     is_external);
			write_log(10, "Debug lookup %" PRIu64 ", %s, %d\n",
				  (uint64_t)parent_ino, selfname, ret_val);

			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
		} else {
			/* Nothing is cached in hash table. Check the result */
			/* First check if the apk exists */
			ret_val = lookup_dir(parent_ino, selfname, &temp_dentry,
					     is_external);
			write_log(10, "Debug lookup %" PRIu64 ", %s, %d\n",
				  (uint64_t)parent_ino, selfname, ret_val);

			/* Don't insert anything to hash table if
			apk not found */
			if (ret_val < 0) {
				fuse_reply_err(req, -ret_val);
				return;
			}
			/* temp_dentry now points to the apk entry */
			/* Check whether to use minimal apk */

			MIN_APK_LOOKUP_DATA minapk_data;
			ret_val = _check_use_minapk(parent_ino, selfname,
			                        &minapk_data, temp_dentry.d_ino);
			minapk_ino = minapk_data.min_apk_ino;
			if (minapk_data.is_complete_apk == TRUE)
				minapk_ino = 0;
			write_log(6, "[App unpin] check_use_minapk: %d, %"
			          PRIu64 "\n", ret_val, (uint64_t) minapk_ino);

			/* Decide whether to use minimal apk */
			if ((ret_val == 0) && (minapk_ino > 0)) {
				/* Use minimal apk */
				write_log(10, "[App unpin] Use minapk\n");
				temp_dentry.d_ino = minapk_ino;
			} else {
				minapk_ino = 0;
			}

			/* Insert the result to hash table */
			insert_minapk_data(parent_ino, selfname, &minapk_data);
		}
	} else {
		ret_val = lookup_dir(parent_ino, selfname, &temp_dentry,
				     is_external);
		write_log(10, "Debug lookup %" PRIu64 ", %s, %d\n",
			  (uint64_t)parent_ino, selfname, ret_val);

		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	this_ino = temp_dentry.d_ino;

	ret_val = fetch_inode_stat(this_ino, &this_stat, &this_gen, NULL);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}
	convert_hcfsstat_to_sysstat(&output_param.attr, &this_stat);

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat, NULL, NULL);
	}
#endif

	if (S_ISFILE((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_ino,
				1, D_ISREG);
	if (S_ISDIR((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_ino,
				1, D_ISDIR);
	if (S_ISLNK((output_param.attr).st_mode))
		ret_val = lookup_increase(tmpptr->lookup_table, this_ino,
				1, D_ISLNK);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* (Only for external volume) If the name "selfname" is case-insensitive,
	 * let the non-existed inode be a duplicated of the original inode but just
	 * the inode number is different. */
	if (is_external && strcmp(selfname, temp_dentry.d_name)) {
		/* Find if the alias inode is existed, and if not, create it. */
		ret_val = seek_and_add_in_alias_group(temp_dentry.d_ino, &this_ino,
		                                      temp_dentry.d_name, selfname);
		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	output_param.ino = (fuse_ino_t) this_ino;
	output_param.generation = this_gen;
	write_log(10, "Debug lookup inode %" PRIu64 ", gen %ld\n",
			(uint64_t)this_ino, this_gen);

	fuse_reply_entry(req, &output_param);
}

/* Helper function to compare if oldpath is the prefix path of newpath */
int32_t _check_path_prefix(const char *oldpath, const char *newpath)
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
static inline int32_t _cleanup_rename(META_CACHE_ENTRY_STRUCT *body_ptr,
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
	int32_t ret_val, ret;
	HCFS_STAT tempstat, old_target_stat;
	mode_t self_mode = 0, old_target_mode = 0;
	DIR_META_TYPE tempmeta;
	META_CACHE_ENTRY_STRUCT *body_ptr = NULL, *old_target_ptr = NULL;
	META_CACHE_ENTRY_STRUCT *parent1_ptr = NULL, *parent2_ptr = NULL;
	DIR_ENTRY_PAGE temp_page;
	int32_t temp_index;
	HCFS_STAT parent_stat1, parent_stat2;
	MOUNT_T *tmpptr;
	DIR_STATS_TYPE tmpstat;
	int64_t old_metasize1, old_metasize2, new_metasize1, new_metasize2;
	int64_t old_metasize1_blk, old_metasize2_blk = 0;
	int64_t new_metasize1_blk, new_metasize2_blk;
	int64_t delta_meta_size1, delta_meta_size2;
	int64_t delta_meta_size1_blk, delta_meta_size2_blk;
	BOOL is_external = FALSE;
	BOOL rename_self_external = FALSE;
	char lowercase[MAX_FILENAME_LEN+1], *alias_name;
	ino_t alias_ino;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	parent_inode1 = real_ino(req, parent);
	parent_inode2 = real_ino(req, newparent);
#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		is_external = TRUE;
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
	}
#endif

	write_log(6, "Debug rename: name %s, parent %" PRIu64 "\n", selfname1,
		  (uint64_t)parent_inode1);
	write_log(6, "Rename target: name %s, parent %" PRIu64 "\n", selfname2,
		  (uint64_t)parent_inode2);

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

	ret_val = fetch_inode_stat(parent_inode1, &parent_stat1, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	if (is_external)
		_rewrite_stat(tmpptr, &parent_stat1, NULL, NULL);

	if (!S_ISDIR(parent_stat1.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat1, 3, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = fetch_inode_stat(parent_inode2, &parent_stat2, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	if (is_external)
		_rewrite_stat(tmpptr, &parent_stat2, NULL, NULL);

	if (!S_ISDIR(parent_stat2.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat2, 3, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


	/*TODO: To add symlink handling for rename*/

	/* Lock parents */
	parent1_ptr = meta_cache_lock_entry(parent_inode1);

	if (parent1_ptr == NULL) {   /* Cannot lock (cannot allocate) */
		ret_val = errno;
		fuse_reply_err(req, ret_val);
		return;
	}

	ret_val = update_meta_seq(parent1_ptr);
	if (ret_val < 0) {
		meta_cache_close_file(parent1_ptr);
		meta_cache_unlock_entry(parent1_ptr);
		fuse_reply_err(req, -ret_val);
		return;
	}
	meta_cache_get_meta_size(parent1_ptr, &old_metasize1,
			&old_metasize1_blk);

	if (parent_inode1 != parent_inode2) {
		parent2_ptr = meta_cache_lock_entry(parent_inode2);
		if (parent2_ptr == NULL) { /* Cannot lock (cannot allocate) */
			ret_val = errno;
			meta_cache_close_file(parent1_ptr);
			meta_cache_unlock_entry(parent1_ptr);
			fuse_reply_err(req, ret_val);
			return;
		}

		ret_val = update_meta_seq(parent2_ptr);
		if (ret_val < 0) {
			meta_cache_close_file(parent2_ptr);
			meta_cache_unlock_entry(parent2_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}
		meta_cache_get_meta_size(parent2_ptr, &old_metasize2,
				&old_metasize2_blk);
	} else {
		parent2_ptr = parent1_ptr;
		old_metasize2 = old_metasize1;
		old_metasize2_blk = old_metasize1_blk;
	}

	/* Check if oldpath and newpath exists already */
	ret_val = meta_cache_seek_dir_entry(parent_inode1, &temp_page,
			&temp_index, selfname1, parent1_ptr,
			is_external);

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

	/* It's not permitted to do 'mv TESTDIR testDIR' under the same directory
	 * in the external volume. In this case, they share the same real inode
	 * when calling ll_lookup(). So it becomes to move TESTDIR under testDIR.
	 * That leads the ll_rename() to pass (parent1, name1, parent2, name2) with
	 * values (TESTDIR's parent-ino, 'TESTDIR', TESTDIR-ino, 'TESTDIR'), which
	 * are illegal parameters. */
	if ((is_external == TRUE) && (parent_inode2 == self_inode)) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, EINVAL);
		return;
	}

	ret_val = meta_cache_seek_dir_entry(parent_inode2, &temp_page,
			&temp_index, selfname2, parent2_ptr,
			is_external);

	if ((ret_val != 0) || (temp_index < 0))
		old_target_inode = 0;
	else
		old_target_inode = temp_page.dir_entries[temp_index].d_ino;

	/* If both newpath and oldpath refer to the same file, do nothing */
	/* But if in external volume, will rename if name is different */
	if (self_inode == old_target_inode) {
		if ((is_external == TRUE) && (strcmp(selfname1, selfname2) != 0)
			/* Skip the case of the rename target is the original name. */
			&& strcmp(temp_page.dir_entries[temp_index].d_name, selfname2)) {
			old_target_inode = 0;
			rename_self_external = TRUE;
		} else {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, 0);
			return;
		}
	}

	/* Invalidate pathname cache for oldpath and newpath */

	body_ptr = meta_cache_lock_entry(self_inode);
	if (body_ptr == NULL) {
		ret_val = errno;
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		fuse_reply_err(req, ret_val);
		return;
	}

	ret_val = update_meta_seq(body_ptr);
	if (ret_val < 0) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		meta_cache_remove(self_inode);
		fuse_reply_err(req, -ret_val);
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
	if ((S_ISDIR(tempstat.mode)) && (parent_inode1 != parent_inode2)) {
		ret_val = check_permission(req, &tempstat, 2, FALSE);

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
			ret_val = errno;
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);

			fuse_reply_err(req, ret_val);
			return;
		}

		ret_val = update_meta_seq(old_target_ptr);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			meta_cache_remove(old_target_inode);

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

	self_mode = tempstat.mode;

	/* Start checking if the operation leads to an error */
	if (old_target_inode > 0) {
		old_target_mode = old_target_stat.mode;
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
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				fuse_reply_err(req, -ret_val);
				return;
			}
			if (tempmeta.total_children > 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				fuse_reply_err(req, ENOTEMPTY);
				return;
			}
		}
	}

	write_log(10, "Debug: Start updating dir entries\n");

	/* If newpath exists, replace the entry and rmdir/unlink
		the old target */
	if (old_target_inode > 0) {
		ret_val = change_dir_entry_inode(parent_inode2, selfname2,
				self_inode, self_mode, parent2_ptr,
				is_external);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
		/* If reg file being unlinked, need to update dir statistics */
		if (S_ISREG(old_target_mode)) {
			/* Check location for deleted file*/
			ret_val = meta_cache_open_file(old_target_ptr);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}

			ret_val = check_file_storage_location(
					old_target_ptr->fptr, &tmpstat);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		meta_cache_close_file(old_target_ptr);
		meta_cache_unlock_entry(old_target_ptr);

		/* Delete the old target */
		if (S_ISDIR(old_target_mode)) {
			/* Deferring actual deletion to forget */
			ret_val = mark_inode_delete(req, old_target_inode);
		} else {
			ret_val = decrease_nlink_inode_file(req,
					old_target_inode);
		}
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}

		/* Delete parent lookup entry for the old target*/
		ret_val = sem_wait(&(pathlookup_data_lock));
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}
		ret_val = lookup_delete_parent(old_target_inode, parent_inode2);
		if (ret_val < 0) {
			sem_post(&(pathlookup_data_lock));
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}

		/* If this is a regular file, undo the dir statistics */
		if (S_ISREG(old_target_mode)) {
			tmpstat.num_local = -tmpstat.num_local;
			tmpstat.num_cloud = -tmpstat.num_cloud;
			tmpstat.num_hybrid = -tmpstat.num_hybrid;
			ret_val =
			    update_dirstat_parent(parent_inode2, &tmpstat);
			if (ret_val < 0) {
				sem_post(&(pathlookup_data_lock));
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		sem_post(&(pathlookup_data_lock));

		/* If this is a directory, reset the dir statistics */
		if (S_ISDIR(old_target_mode)) {
			ret_val = reset_dirstat_lookup(old_target_inode);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}

		old_target_ptr = NULL;
	} else {
		/* If newpath does not exist, add the new entry */
		/* But will not add new entry if rename self */
		if (rename_self_external != TRUE) {
			ret_val = dir_add_entry(parent_inode2, self_inode,
				selfname2, self_mode, parent2_ptr, is_external);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}
	}

	if (rename_self_external == TRUE) {
		ret_val = change_entry_name(parent_inode1, selfname2, parent1_ptr);
	} else {
		ret_val = dir_remove_entry(parent_inode1, self_inode,
				selfname1, self_mode, parent1_ptr, is_external);
	}
	if (ret_val < 0) {
		_cleanup_rename(body_ptr, old_target_ptr,
				parent1_ptr, parent2_ptr);
		meta_cache_remove(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Update alias list. */
	if (is_external == TRUE) {
		update_in_alias_group(self_inode, selfname2);

		/* Delete all alias inodes if not case-insensitive condition. */
		if (rename_self_external == FALSE) {
			to_lowercase(selfname1, lowercase);

			while (1) {
				alias_name = get_name_in_alias_group(
				    self_inode, lowercase, &alias_ino);
				if (alias_name == NULL)
					break;

				/* Tell all views that the alias inode is gone. This
				 * can be done by passing selfname2 as a fake name. */
				ret_val = hfuse_ll_notify_delete_mp(
					tmpptr->chan_ptr, parent_inode1, alias_ino,
					alias_name, strlen(alias_name), selfname2);

				free(alias_name);

				if (ret_val < 0) {
					_cleanup_rename(body_ptr, old_target_ptr,
							parent1_ptr, parent2_ptr);
					meta_cache_remove(self_inode);
					fuse_reply_err(req, -ret_val);
					return;
				}
			}
		}
	}

	write_log(10, "Debug: Finished updating dir entries\n");

	if ((S_ISDIR(self_mode)) && (parent_inode1 != parent_inode2)) {
		ret_val = change_parent_inode(self_inode, parent_inode1,
				parent_inode2, body_ptr, is_external);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	/* If a file or directory is moved to another parent, statistics
	 * for the path from the old parent to the root and
	 * for the path from the new parent to the root are changed.
	 */
	if (parent_inode1 != parent_inode2) {
		if (S_ISDIR(self_mode)) {
			/* If this inode is a directory, find the dir stat */
			ret_val = read_dirstat_lookup(self_inode, &tmpstat);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}
		if (S_ISREG(self_mode)) {
			/* Check location for this file*/
			ret_val = meta_cache_open_file(body_ptr);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}

			ret_val = check_file_storage_location(body_ptr->fptr,
							      &tmpstat);
			if (ret_val < 0) {
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}


		ret_val = sem_wait(&(pathlookup_data_lock));
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}

		/* Change the parent lookup for source inode */
		ret_val = lookup_replace_parent(self_inode, parent_inode1,
						parent_inode2);
		if (ret_val < 0) {
			sem_post(&(pathlookup_data_lock));
			_cleanup_rename(body_ptr, old_target_ptr,
					parent1_ptr, parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
		/* Process changes to dir statistics */
		if ((S_ISDIR(self_mode)) || (S_ISREG(self_mode))) {
			ret_val = update_dirstat_parent(parent_inode2,
							&tmpstat);
			if (ret_val < 0) {
				sem_post(&(pathlookup_data_lock));
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}

			tmpstat.num_local = -tmpstat.num_local;
			tmpstat.num_cloud = -tmpstat.num_cloud;
			tmpstat.num_hybrid = -tmpstat.num_hybrid;
			ret_val = update_dirstat_parent(parent_inode1,
							&tmpstat);
			if (ret_val < 0) {
				sem_post(&(pathlookup_data_lock));
				_cleanup_rename(body_ptr, old_target_ptr,
						parent1_ptr, parent2_ptr);
				meta_cache_remove(self_inode);
				fuse_reply_err(req, -ret_val);
				return;
			}
		}
		sem_post(&(pathlookup_data_lock));

	} /* (parent_inode1 != parent_inode2) */

	/* Clear path cache entry after rename */
	if (is_external) {
		ret_val = delete_pathcache_node(tmpptr->vol_path_cache,
						old_target_inode);
		if (ret_val < 0) {
			_cleanup_rename(body_ptr, old_target_ptr, parent1_ptr,
					parent2_ptr);
			meta_cache_remove(self_inode);
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	meta_cache_get_meta_size(parent1_ptr, &new_metasize1,
			&new_metasize1_blk);
	meta_cache_get_meta_size(parent2_ptr, &new_metasize2,
			&new_metasize2_blk);

	_cleanup_rename(body_ptr, old_target_ptr,
			parent1_ptr, parent2_ptr);

	if (new_metasize1 > 0 && old_metasize1 > 0) {
		delta_meta_size1 = new_metasize1 - old_metasize1;
		delta_meta_size1_blk = new_metasize1_blk - old_metasize1_blk;
	} else {
		delta_meta_size1 = 0;
		delta_meta_size1_blk = 0;
	}
	if (new_metasize2 > 0 && old_metasize2 > 0) {
		delta_meta_size2 = new_metasize2 - old_metasize2;
		delta_meta_size2_blk = new_metasize2_blk - old_metasize2_blk;
	} else {
		delta_meta_size2 = 0;
		delta_meta_size2_blk = 0;
	}

	if (delta_meta_size1 + delta_meta_size2 != 0) {
		change_system_meta(delta_meta_size1 + delta_meta_size2,
				delta_meta_size1_blk + delta_meta_size2_blk,
				0, 0, 0, 0, TRUE);
		change_mount_stat(tmpptr, 0,
				  delta_meta_size1 + delta_meta_size2, 0);
	}

#ifdef _ANDROID_ENV_
	/* Update minimal apk table */
	/* Rename an app folder under /data/app to another place
	 * where is not under /data/app */
	if ((hcfs_system->use_minimal_apk == TRUE) && (
	    parent_inode1 == hcfs_system->data_app_root &&
	    S_ISDIR(self_mode) &&
	    parent_inode2 != hcfs_system->data_app_root)) {
		DIR_ENTRY_ITERATOR *iter;
		/* remove all entries with key (parent_inode1, <x>.apk)
		 * in minapk_lookup_table */
		body_ptr = meta_cache_lock_entry(self_inode);
		ret = meta_cache_open_file(body_ptr);
		if (ret < 0) {
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, -ret);
			return;
		}
		iter = init_dir_iter(body_ptr->fptr);
		if (!iter) {
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, -errno);
			return;
		}
		while (iter_next(iter)) {
			if (!strcmp(iter->now_entry->d_name, ".") ||
			    !strcmp(iter->now_entry->d_name, ".."))
				continue;
			/* Check if it is apk file */
			if (iter->now_entry->d_type == D_ISREG &&
			    is_apk(iter->now_entry->d_name)) {
				ret = remove_minapk_data(self_inode,
						   iter->now_entry->d_name);
				if (ret < 0 && ret != -ENOENT)
					break;
			}
		}
		destroy_dir_iter(iter);
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
	}

	/* Remove entry when rename from (to) the .apk file or
	 * minimal apk file. */
	if ((hcfs_system->use_minimal_apk == TRUE) &&
	    (tmpptr->f_ino == hcfs_system->data_app_root)) {
		const char *selfname[2] = {selfname1, selfname2};
		mode_t mode[2] = {self_mode, old_target_mode};
		ino_t parent_inode[2] = {parent_inode1, parent_inode2};
		int32_t i;

		/* Both check old name and new name. */
		for (i = 0; i < 2; i++) {
			if (mode[i] != 0 && !S_ISREG(mode[i]))
				continue;
			if (is_apk(selfname[i])) {
				ret = remove_minapk_data(parent_inode[i],
						selfname[i]);
				if (ret < 0 && ret != -ENOENT) {
					fuse_reply_err(req, -ret);
					return;
				}
			} else if (is_minapk(selfname[i])) {
				char origin_apk[MAX_FILENAME_LEN] = {0};

				ret = convert_origin_apk(origin_apk,
						selfname[i]);
				if (ret < 0)
					continue; /* Skip this name */

				ret = remove_minapk_data(parent_inode[i],
						origin_apk);
				if (ret < 0 && ret != -ENOENT) {
					fuse_reply_err(req, -ret);
					return;
				}
			}
		}
	}
#endif

	fuse_reply_err(req, 0);
}

/* Helper function for waiting on full cache in the truncate function */
int32_t truncate_wait_full_cache(ino_t this_inode,
				 HCFS_STAT *inode_stat,
				 FILE_META_TYPE *file_meta_ptr,
				 BLOCK_ENTRY_PAGE *block_page,
				 int64_t page_pos,
				 META_CACHE_ENTRY_STRUCT **body_ptr,
				 int32_t entry_index)
{
	int32_t ret_val;
	int64_t max_cache_size;

	max_cache_size = get_cache_limit(file_meta_ptr->local_pin);
	if (max_cache_size < 0)
		return -EIO;

	while (((block_page)->block_entries[entry_index].status == ST_CLOUD) ||
		((block_page)->block_entries[entry_index].status == ST_CtoL)) {
		if (hcfs_system->systemdata.cache_size > max_cache_size) {
			if (hcfs_system->system_going_down == TRUE)
				return -EBUSY;

			/*Sleep if cache already full*/
			write_log(10,
				"debug truncate waiting on full cache\n");
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);
			ret_val = sleep_on_cache_full();
			if (ret_val < 0)
				return ret_val;

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			if (*body_ptr == NULL)
				return -errno;
			ret_val = meta_cache_lookup_file_data(
			    this_inode, inode_stat, file_meta_ptr, block_page,
			    page_pos, *body_ptr);
			if (ret_val < 0)
				return ret_val;
		} else {
			break;
		}
	}
	return 0;
}

/**
 * When write or truncate a block with status ST_LDISK/ST_LtoC, check if
 * this file is now syncing and try to copy the block. If cache is full,
 * unlock meta entry and sleep. Then wait for cache space and copy block again.
 *
 * @return 0 on success. Positive integer means it had ever slept.
 *         Otherwise return negative error code.
 */
int32_t _check_sync_wait_full_cache(META_CACHE_ENTRY_STRUCT **body_ptr,
		ino_t this_inode, int64_t blockno, int64_t seq,
		BLOCK_ENTRY_PAGE *temppage, int64_t pagepos)
{
	int32_t ret, sleep_times;

	sleep_times = 0;
	while (hcfs_system->system_going_down == FALSE) {
		/* Check if this file is uploading. If so, try to copy block
		 * if this block is not being uploaded. Otherwise do nothing. */
		ret = meta_cache_check_uploading(*body_ptr,
				this_inode, blockno, seq);
		if (ret == -ENOSPC) {
			meta_cache_update_file_data(this_inode, NULL,
					NULL, temppage, pagepos, *body_ptr);
			ret = meta_cache_unlock_entry(*body_ptr);
			if (ret < 0)
				break;

			/* Wait for free cache space */
			sleep_times++;
			write_log(10, "Debug: Sleep and wait free cache."
				" Now check block_%"PRIu64"_%"PRId64,
				(uint64_t)this_inode, blockno);
			ret = sleep_on_cache_full();
			write_log(10, "Debug: Wake up and keep check sync."
				" Now check block_%"PRIu64"_%"PRId64,
				(uint64_t)this_inode, blockno);
			if (ret < 0) {
				*body_ptr = meta_cache_lock_entry(
						this_inode);
				if ((*body_ptr) != NULL) {
					meta_cache_lookup_file_data(this_inode,
						NULL, NULL, temppage, pagepos,
						*body_ptr);
				}
				break;
			}

			*body_ptr = meta_cache_lock_entry(this_inode);
			if (*body_ptr == NULL) {
				ret = -errno;
				break;
			}

			ret = meta_cache_open_file(*body_ptr);
			if (ret < 0)
				break;
			/* Lookup again. Page may be modified by others. */
			meta_cache_lookup_file_data(this_inode, NULL,
					NULL, temppage, pagepos, *body_ptr);
		} else {
			break;
		}
	}

	/* If it had ever slept and succeed in check block sync status,
	 * then return a positive integer. */
	if (sleep_times > 0 && ret == 0)
		return sleep_times;

	return ret;
}

/* Helper function for truncate operation. Will delete all blocks in the page
*  pointed by temppage starting from "start_index". Track the current page
*  using "page_index". "old_last_block" indicates the last block
*  index before the truncate operation (hence we can ignore the blocks after
*  "old_last_block". "inode_index" is the inode number of the file being
*  truncated. */
int32_t truncate_delete_block(BLOCK_ENTRY_PAGE *temppage, int32_t start_index,
			int64_t page_index, int64_t page_pos,
			int64_t old_last_block, ino_t inode_index,
			META_CACHE_ENTRY_STRUCT *body_ptr,
			FILE_META_TYPE *filemeta)
{
	int32_t block_count;
	char thisblockpath[1024];
	int64_t tmp_blk_index;
	off_t cache_block_size;
	off_t total_deleted_cache;
	int64_t total_deleted_blocks;
	int64_t total_deleted_fileblocks;
	int64_t unpin_dirty_size;
	off_t total_deleted_dirty_cache;
	int32_t ret_val, errcode, ret;
	BLOCK_ENTRY *tmpentry;
	PIN_t pin;

	total_deleted_cache = 0;
	total_deleted_dirty_cache = 0;
	total_deleted_blocks = 0;
	total_deleted_fileblocks = 0;
	pin = filemeta->local_pin;

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

		while (hcfs_system->system_going_down == FALSE) {
			switch (tmpentry->status) {
			case ST_NONE:
			case ST_TODELETE:
				break;
			case ST_LDISK:
				/* Now will just delete block if not uploaded.
				If the file is being uploaded, the upload
				processs will be canceled if some block is not
				uploaded but deleted */

				ret_val = fetch_block_path(thisblockpath,
					inode_index, tmp_blk_index);
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

				total_deleted_cache += (int64_t)
					cache_block_size;
				total_deleted_dirty_cache +=
					(int64_t) cache_block_size;
				total_deleted_blocks += 1;
				total_deleted_fileblocks++;
				break;
			case ST_CLOUD:
				tmpentry->status =
						ST_TODELETE;
				total_deleted_fileblocks++;
				break;
			case ST_BOTH:
				ret_val = fetch_block_path(thisblockpath,
					inode_index, tmp_blk_index);
				if (ret_val < 0)
					return ret_val;
				if (access(thisblockpath, F_OK) == 0) {
					cache_block_size =
						check_file_size(thisblockpath);
					unlink(thisblockpath);
					total_deleted_cache +=
						(int64_t) cache_block_size;
					total_deleted_blocks += 1;
				}
				tmpentry->status = ST_TODELETE;
				total_deleted_fileblocks++;
				break;
			case ST_LtoC:
				ret_val = _check_sync_wait_full_cache(&body_ptr,
					inode_index, block_count,
					tmpentry->seqnum,
					temppage, page_pos);
				if (ret_val < 0)
					return ret_val;
				else if (ret_val > 0) /* Check status again */
					continue;
				ret_val = fetch_block_path(thisblockpath,
					inode_index, tmp_blk_index);
				if (ret_val < 0)
					return ret_val;
				if (access(thisblockpath, F_OK) == 0) {
					cache_block_size =
						check_file_size(thisblockpath);
					unlink(thisblockpath);
					total_deleted_cache +=
						(int64_t) cache_block_size;
					total_deleted_dirty_cache +=
						(int64_t) cache_block_size;
					total_deleted_blocks += 1;
				}
				tmpentry->status = ST_TODELETE;
				total_deleted_fileblocks++;
				break;
			case ST_CtoL:
				ret_val = fetch_block_path(thisblockpath,
						inode_index, tmp_blk_index);
				if (ret_val < 0)
					return ret_val;
				if (access(thisblockpath, F_OK) == 0)
					unlink(thisblockpath);
				tmpentry->status = ST_TODELETE;
				total_deleted_fileblocks++;
				break;
			default:
				break;
			}

			/* Leave loop directly */
			break;
		}

		/* Update block seq */
		tmpentry->seqnum = filemeta->finished_seq;
	}
	if ((total_deleted_fileblocks > 0) || (total_deleted_blocks > 0)) {
		unpin_dirty_size = (P_IS_UNPIN(pin) ? -total_deleted_dirty_cache : 0);
		change_system_meta_ignore_dirty(
		    inode_index, 0, 0, -total_deleted_cache,
		    -total_deleted_blocks, -total_deleted_dirty_cache,
		    unpin_dirty_size, TRUE);
		ret = update_file_stats(
		    body_ptr->fptr, -total_deleted_fileblocks,
		    -total_deleted_blocks, -total_deleted_cache,
		    -total_deleted_dirty_cache, inode_index);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}
	}

	write_log(10, "Debug truncate_delete_block end\n");

	return 0;
errcode_handle:
	return errcode;
}

/* Helper function for hfuse_truncate. This will truncate the last block
*  that remains after the truncate operation so that the size of this block
*  fits (offset % MAX_BLOCK_SIZE) */
int32_t truncate_truncate(ino_t this_inode,
			  HCFS_STAT *filestat,
			  FILE_META_TYPE *tempfilemeta,
			  BLOCK_ENTRY_PAGE *temppage,
			  int64_t currentfilepos,
			  META_CACHE_ENTRY_STRUCT **body_ptr,
			  int32_t last_index,
			  int64_t last_block,
			  off_t offset)

{
	char thisblockpath[1024];
	char objname[1000];
	FILE *blockfptr;
	struct stat tempstat; /* block ops */
	off_t old_block_size, new_block_size;
	int32_t ret, errcode;
	int64_t cache_delta;
	int64_t cache_block_delta;
	int64_t block_dirty_size, delta_dirty_size;
	int64_t unpin_dirty_size;
	char tmpstatus;
	BLOCK_ENTRY *last_block_entry;

	cache_delta = 0;
	cache_block_delta = 0;
	/*Offset not on the boundary of the block. Will need to truncate the
	last block*/
	ret = truncate_wait_full_cache(this_inode, filestat, tempfilemeta,
				       temppage, currentfilepos, body_ptr,
				       last_index);
	if (ret < 0)
		return ret;
	if (!*body_ptr)
		return -EINVAL;

	ret = fetch_block_path(thisblockpath, filestat->ino, last_block);
	if (ret < 0)
		return ret;

	last_block_entry = &(temppage->block_entries[last_index]);
	if ((last_block_entry->status == ST_CLOUD) ||
		(last_block_entry->status == ST_CtoL)) {
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
			fclose(blockfptr);
			return -EIO;
		}

		ret = meta_cache_lookup_file_data(this_inode, NULL, NULL,
				temppage, currentfilepos, *body_ptr);
		if (ret < 0) {
			fclose(blockfptr);
			return ret;
		}

		tmpstatus = (temppage->block_entries[last_index]).status;
		if ((last_block_entry->status == ST_CLOUD) ||
				(last_block_entry->status == ST_CtoL)) {
			if (last_block_entry->status == ST_CLOUD) {
				last_block_entry->status = ST_CtoL;
				ret = meta_cache_update_file_nosync(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					fclose(blockfptr);
					return ret;
				}
			}
			meta_cache_close_file(*body_ptr);
			meta_cache_unlock_entry(*body_ptr);


#if ENABLE(DEDUP)
			fetch_backend_block_objname(objname,
					last_block_entry->obj_id);
#else
			fetch_backend_block_objname(objname, filestat->ino,
					last_block, last_block_entry->seqnum);
#endif
			ret = fetch_from_cloud(blockfptr, READ_BLOCK, objname,
					       last_block_entry->blockID);

			if (ret < 0) {
				fclose(blockfptr);
				return -EIO;
			}

			/*Re-read status*/
			*body_ptr = meta_cache_lock_entry(this_inode);
			if (*body_ptr == NULL) {
				ret = -errno;
				fclose(blockfptr);
				return ret;
			}

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, temppage, currentfilepos, *body_ptr);
			if (ret < 0) {
				fclose(blockfptr);
				return ret;
			}

			if (stat(thisblockpath, &tempstat) == 0) {
				int64_t block_size_blk;

				last_block_entry->status = ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					fclose(blockfptr);
					return -EIO;
				}
				ret = meta_cache_update_file_data(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					fclose(blockfptr);
					return ret;
				}

				block_size_blk = tempstat.st_blocks * 512;
				change_system_meta_ignore_dirty(
				    this_inode, 0, 0, block_size_blk, 1,
				    block_size_blk, 0, TRUE);
				cache_delta += block_size_blk;
				cache_block_delta += 1;
			}
		} else {
			if (stat(thisblockpath, &tempstat) == 0) {
				last_block_entry->status = ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					fclose(blockfptr);
					return -EIO;
				}
				ret = meta_cache_update_file_data(this_inode,
					NULL, NULL, temppage, currentfilepos,
					*body_ptr);
				if (ret < 0) {
					fclose(blockfptr);
					return ret;
				}
			}
		}
		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		if (tmpstatus == ST_BOTH)
			delta_dirty_size = new_block_size;
		else
			delta_dirty_size = new_block_size - old_block_size;

		unpin_dirty_size = (P_IS_UNPIN(tempfilemeta->local_pin) ?
				delta_dirty_size : 0);
		change_system_meta_ignore_dirty(
		    this_inode, 0, 0, new_block_size - old_block_size, 0,
		    delta_dirty_size, unpin_dirty_size, TRUE);

		cache_delta += new_block_size - old_block_size;

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
		ret = meta_cache_open_file(*body_ptr);
		if (ret < 0)
			return ret;
		ret = update_file_stats((*body_ptr)->fptr, 0,
				cache_block_delta, cache_delta,
				delta_dirty_size, this_inode);
		if (ret < 0) {
			meta_cache_close_file(*body_ptr);
			return ret;
		}

	} else {
		if (last_block_entry->status == ST_NONE)
			return 0;

		ret = _check_sync_wait_full_cache(body_ptr, this_inode,
				last_block,
				(temppage->block_entries[last_index]).seqnum,
				temppage, currentfilepos);
		if (ret < 0)
			return ret;

		tmpstatus = (temppage->block_entries[last_index]).status;

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
			fclose(blockfptr);
			return -EIO;
		}

		/* TODO: check if it is possible for a block to be deleted
			when truncate is conducted (meta is locked) */
		if (stat(thisblockpath, &tempstat) == 0) {
			last_block_entry->status = ST_LDISK;
			ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
			if (ret < 0) {
				fclose(blockfptr);
				return -EIO;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, temppage, currentfilepos, *body_ptr);
			if (ret < 0) {
				fclose(blockfptr);
				return ret;
			}
		}

		old_block_size = check_file_size(thisblockpath);
		ftruncate(fileno(blockfptr), (offset % MAX_BLOCK_SIZE));
		new_block_size = check_file_size(thisblockpath);

		/* When status is BOTH, this truncated block is a new dirty
		 * block. Otherwise it is an old dirty block */
		block_dirty_size = (tmpstatus == ST_BOTH ? new_block_size :
				new_block_size - old_block_size);
		unpin_dirty_size =
		    (P_IS_UNPIN(tempfilemeta->local_pin) ? block_dirty_size
							 : 0);
		change_system_meta_ignore_dirty(
		    this_inode, 0, 0, new_block_size - old_block_size, 0,
		    block_dirty_size, unpin_dirty_size, TRUE);

		cache_delta += new_block_size - old_block_size;

		flock(fileno(blockfptr), LOCK_UN);
		fclose(blockfptr);
		ret = meta_cache_open_file(*body_ptr);
		if (ret < 0)
			return ret;
		ret = update_file_stats((*body_ptr)->fptr, 0,
				cache_block_delta, cache_delta,
				block_dirty_size, this_inode);
		if (ret < 0) {
			meta_cache_close_file(*body_ptr);
			return ret;
		}
	}
	return 0;
}

int32_t try_cancel_undone_sync(ino_t this_inode,
			       META_CACHE_ENTRY_STRUCT *body_ptr)
{
	char path[METAPATHLEN];
	char toupload_metapath[METAPATHLEN];
	char block_path[400];
	FILE *fptr;
	FILE_BLOCK_ITERATOR *block_iter;
	int32_t ret = 0;

	/* Do nothing if it is uploading */
	if (body_ptr->uploading_info.is_uploading == TRUE)
		goto out;

	/* Do nothing if no progress file */
	fetch_progress_file_path(path, this_inode);
	if (access(path, F_OK) < 0)
		goto out;

	/* Try to access toupload meta */
	fetch_toupload_meta_path(toupload_metapath, this_inode);
	if (access(toupload_metapath, F_OK) < 0)
		goto out;

	fptr = fopen(toupload_metapath, "r");
	if (!fptr) {
		ret = -errno;
		goto out;
	}
	flock(fileno(fptr), LOCK_EX);

	/* Init iterator and then traverse all blocks */
	block_iter = init_block_iter(fptr);
	if (!block_iter) {
		flock(fileno(fptr), LOCK_UN);
		fclose(fptr);
		ret = -errno;
		goto out;
	}

	while (iter_next(block_iter)) {
		fetch_toupload_block_path(block_path, this_inode,
					  block_iter->now_block_no);
		if (access(block_path, F_OK) == 0)
			unlink_upload_file(block_path);
	}
	if (errno != ENOENT) {
		ret = -errno;
		goto release_resource;
	}

	/* Abandon the sync this time . Do not remove progress
	 * file because it will be used to remove all garbage on cloud */
	unlink(toupload_metapath);
	write_log(
	    6, "Remove to-upload blocks when truncating inode %" PRIu64,
	    (uint64_t)this_inode);

release_resource:
	destroy_block_iter(block_iter);
	flock(fileno(fptr), LOCK_UN);
	fclose(fptr);
out:
	return ret;
}

/************************************************************************
*
* Function name: hfuse_ll_truncate
*        Inputs: ino_t this_inode, HCFS_STAT *filestat,
*                off_t offset, META_CACHE_ENTRY_STRUCT **body_ptr
*                fuse_req_t req
*       Summary: Truncate the regular file pointed by "this_inode"
*                to size "offset".
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*          Note: This function is now called by hfuse_ll_setattr.
*
*************************************************************************/
int32_t hfuse_ll_truncate(ino_t this_inode,
			  HCFS_STAT *filestat,
			  off_t offset,
			  META_CACHE_ENTRY_STRUCT **body_ptr,
			  fuse_req_t req)
{
/* If truncate file smaller, do not truncate metafile, but instead set the
*  affected entries to ST_TODELETE (which will be changed to ST_NONE once
*  object deleted)*/
/* Add ST_TODELETE as a new block status. In truncate, if need to throw away
*  a block, set the status to ST_TODELETE and upload process will handle the
*  actual deletion.*/
/* If need to truncate some block that's ST_CtoL or ST_CLOUD, download it
*  first, mod it, then set to ST_LDISK*/

	FILE_META_TYPE tempfilemeta;
	int32_t ret, errcode;
	int64_t last_block, last_page, old_last_block;
	int64_t current_page, old_last_page;
	off_t filepos;
	BLOCK_ENTRY_PAGE temppage;
	int32_t last_index;
	int64_t temp_trunc_size, sizediff;
	MOUNT_T *tmpptr;
	int64_t now_seq;
	int64_t max_pinned_size;
#ifdef _ANDROID_ENV_
	FILE *truncfptr;
	char truncpath[METAPATHLEN];
#else
	ssize_t ret_ssize;
#endif

	/* Return error -EFBIG if offset is greater than expected */
	if (offset > (off_t) MAX_FILE_SIZE)
		return -EFBIG;

	if (offset < 0)
		return -EINVAL;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	write_log(10, "Debug truncate: offset %ld\n", offset);
	/* If the filesystem object is not a regular file, return error */
	if (S_ISREG(filestat->mode) == FALSE) {
		if (S_ISDIR(filestat->mode))
			return -EISDIR;
		else
			return -EPERM;
	}

	ret = meta_cache_lookup_file_data(this_inode, NULL, &tempfilemeta,
			NULL, 0, *body_ptr);

	if (ret < 0)
		return ret;

	now_seq = tempfilemeta.finished_seq;

	if (filestat->size == offset) {
		/*Do nothing if no change needed */
		write_log(10,
			"Debug truncate: no size change. Nothing changed.\n");
		return 0;
	}

	/* TODO TODO: Check if progress file and to-upload meta exist but not
	 * syncing. If so, remove to-upload meta and all to-upload blocks */
	try_cancel_undone_sync(this_inode, *body_ptr);

	if (filestat->size < offset) {
		int64_t pin_sizediff;

		sizediff = (int64_t) offset - filestat->size;
		pin_sizediff = round_size((int64_t)offset) -
					round_size(filestat->size);
		sem_wait(&(hcfs_system->access_sem));
		/* Check system size and reject if exceeding quota */
		if (hcfs_system->systemdata.system_size + sizediff >
				hcfs_system->systemdata.system_quota) {
			sem_post(&(hcfs_system->access_sem));
			return -ENOSPC;
		}
		/* If this is a pinned file and we want to extend the file,
		need to find out if pinned space is still available for this
		extension */
		/* If pinned space is available, add the amount of changes
		to the total usage first */
		if (P_IS_PIN(tempfilemeta.local_pin)) {
			max_pinned_size =
				get_pinned_limit(tempfilemeta.local_pin);
			if (max_pinned_size < 0) {
				sem_post(&(hcfs_system->access_sem));
				return -EIO;
			}
			if ((hcfs_system->systemdata.pinned_size + pin_sizediff)
					> max_pinned_size) {
				sem_post(&(hcfs_system->access_sem));
				return -ENOSPC;
			}
			hcfs_system->systemdata.pinned_size += pin_sizediff;
		}
		sem_post(&(hcfs_system->access_sem));
	}

	/*If need to extend, only need to change size. Do that later.*/
	if (filestat->size > offset) {
		if (offset == 0) {
			last_block = -1;
			last_page = -1;
		} else {
			/* Block indexing starts at zero */
			last_block = ((offset-1) / MAX_BLOCK_SIZE);

			/*Page indexing starts at zero*/
			last_page = last_block / MAX_BLOCK_ENTRIES_PER_PAGE;
		}

		old_last_block = ((filestat->size - 1) / MAX_BLOCK_SIZE);
		old_last_page = old_last_block / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (last_page >= 0)
			filepos = seek_page(*body_ptr, last_page, 0);
		else
			filepos = 0;
		if (filepos < 0) {
			errcode = filepos;
			goto errcode_handle;
		}

		current_page = last_page;

		/*TODO: put error handling for the read/write ops here*/
		if (filepos != 0) {
			/* Do not need to truncate the block
				the offset byte is in*/
			/* If filepos is zero*/

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, &temppage, filepos,
				*body_ptr);
			if (ret < 0) {
				errcode = ret;
				goto errcode_handle;
			}

			/* Do the actual handling here*/
			last_index = last_block %
				MAX_BLOCK_ENTRIES_PER_PAGE;
			if ((offset % MAX_BLOCK_SIZE) != 0) {
				/* Truncate the last block that remains
				   after the truncate operation */
				ret = truncate_truncate(
				    this_inode, filestat, &tempfilemeta,
				    &temppage, filepos, body_ptr, last_index,
				    last_block, offset);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}

				/* Update block seq number */
				ret = update_block_seq(*body_ptr, filepos,
						last_index, last_block,
						now_seq, &temppage);
				if (ret < 0) {
					errcode = ret;
					goto errcode_handle;
				}
			}

			/*Delete the rest of blocks in this same page
			as well*/
			ret = meta_cache_open_file(*body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}

			ret = truncate_delete_block(&temppage, last_index+1,
				current_page, filepos, old_last_block,
				filestat->ino, (*body_ptr), &tempfilemeta);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, &temppage, filepos,
				*body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
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
				errcode = filepos;
				goto errcode_handle;
			}

			/* Skipping pages that do not exist */
			if (filepos < 1)
				continue;

			ret = meta_cache_lookup_file_data(this_inode, NULL,
				NULL, &temppage, filepos, *body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}

			ret = meta_cache_open_file(*body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}
			ret = truncate_delete_block(&temppage, 0,
				current_page, filepos, old_last_block,
				filestat->ino, (*body_ptr), &tempfilemeta);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}

			ret = meta_cache_update_file_data(this_inode, NULL,
				NULL, &temppage, filepos, *body_ptr);
			if (ret < 0) {
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent\n");
				errcode = ret;
				goto errcode_handle;
			}
		}
		write_log(10, "Debug truncate update xattr\n");
		/* Will need to remember the old offset, so that sync to cloud
		process can check the block status and delete them */
		ret = meta_cache_open_file(*body_ptr);
		if (ret < 0) {
			write_log(0, "IO error in truncate. Data may ");
			write_log(0, "not be consistent\n");
			errcode = ret;
			goto errcode_handle;
		}

		/* First read from meta file, if need
		to update trunc_size, change the value in meta cache and
		write back to meta cache */
#ifdef _ANDROID_ENV_
		ret = fetch_trunc_path(truncpath, this_inode);
		if (ret != 0) {
			errcode = ret;
			goto errcode_handle;
		}

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
			temp_trunc_size = filestat->size;
			FWRITE(&temp_trunc_size, sizeof(int64_t), 1,
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
			FREAD(&temp_trunc_size, sizeof(int64_t), 1,
				truncfptr);
			if (temp_trunc_size < filestat->size) {
				temp_trunc_size = filestat->size;
				FSEEK(truncfptr, 0, SEEK_SET);
				FWRITE(&temp_trunc_size, sizeof(int64_t), 1,
					truncfptr);
			}
			fclose(truncfptr);
		}
#else
		ret_ssize = fgetxattr(fileno((*body_ptr)->fptr),
				"user.trunc_size",
				&temp_trunc_size, sizeof(int64_t));
		if (((ret_ssize < 0) && (errno == ENOATTR)) ||
			((ret_ssize >= 0) &&
				(temp_trunc_size < filestat->size))) {
			temp_trunc_size = filestat->size;
			ret = fsetxattr(fileno((*body_ptr)->fptr),
				"user.trunc_size", &(temp_trunc_size),
				sizeof(int64_t), 0);
			if (ret < 0) {
				errcode = errno;
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent. ");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				errcode = -EIO;
				goto errcode_handle;
			}
		} else {
			if (ret_ssize < 0) {
				errcode = errno;
				write_log(0, "IO error in truncate. Data may ");
				write_log(0, "not be consistent. ");
				write_log(0, "Code %d, %s\n", errcode,
						strerror(errcode));
				errcode = -EIO;
				goto errcode_handle;
			}
		}
#endif /* _ANDROID_ENV_ */
	}

	ret = meta_cache_update_file_data(this_inode, filestat,
			&tempfilemeta, NULL, 0, *body_ptr);
	if (ret < 0) {
		write_log(0, "IO error in truncate. Data may ");
		write_log(0, "not be consistent\n");
		errcode = ret;
		goto errcode_handle;
	}

	if (P_IS_PIN(tempfilemeta.local_pin) &&
	    (offset < filestat->size)) {
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size +=
			(round_size(offset) - round_size(filestat->size));
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	/* Update file and system meta here */
	change_system_meta((int64_t)(offset - filestat->size),
			0, 0, 0, 0, 0, TRUE);

	ret = change_mount_stat(tmpptr,
			(int64_t) (offset - filestat->size), 0, 0);
	if (ret < 0)
		return ret;

	filestat->size = offset;
	filestat->mtime = time(NULL);

	return 0;
errcode_handle:
	/* If an error occurs, need to revert the changes to pinned size */
	if (P_IS_PIN(tempfilemeta.local_pin) &&
	    (offset > filestat->size)) {
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size -=
			(round_size(offset) - round_size(filestat->size));
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

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
	int64_t fh;
	int32_t ret_val;
	HCFS_STAT this_stat;
	int32_t file_flags;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	write_log(8, "Debug open inode %ld\n", ino);

	thisinode = real_ino(req, ino);

	if (thisinode < 1) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	ret_val = fetch_inode_stat(thisinode, &this_stat, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat, NULL, NULL);
	}
#endif

	file_flags = file_info->flags;

	if (((file_flags & O_ACCMODE) == O_RDONLY) ||
			((file_flags & O_ACCMODE) == O_RDWR)) {
		/* Checking permission */
		ret_val = check_permission(req, &this_stat, 4, FALSE);

		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	if (((file_flags & O_ACCMODE) == O_WRONLY) ||
			((file_flags & O_ACCMODE) == O_RDWR)) {
		/* Checking permission */
		ret_val = check_permission(req, &this_stat, 2, FALSE);

		if (ret_val < 0) {
			fuse_reply_err(req, -ret_val);
			return;
		}
	}

	fh = open_fh(thisinode, file_flags, FALSE);
	if (fh < 0) {
		fuse_reply_err(req, ENFILE);
		return;
	}

	file_info->fh = fh;

	fuse_reply_open(req, file_info);
}

/* Helper function for read operation. Will load file object meta from
*  meta cache or meta file. */
int32_t read_lookup_meta(FH_ENTRY *fh_ptr, BLOCK_ENTRY_PAGE *temppage,
		off_t this_page_fpos)
{
	int32_t ret;

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL)
		return -errno;
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
int32_t read_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, int64_t entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos, uint8_t pin_s)
{
	int32_t ret;

	while (((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
		((temppage->block_entries[entry_index]).status == ST_CtoL)) {

		if (hcfs_system->sync_paused)
			return -EIO;

		if (hcfs_system->systemdata.cache_size > CACHE_LIMITS(pin_s)) {
			if (hcfs_system->system_going_down == TRUE)
				return -EBUSY;
			/*Sleep if cache already full*/
			sem_post(&(fh_ptr->block_sem));
			write_log(10, "debug read waiting on full cache\n");
			ret = sleep_on_cache_full();
			sem_wait(&(fh_ptr->block_sem));
			if (ret < 0)
				return ret;

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
int32_t read_prefetch_cache(BLOCK_ENTRY_PAGE *tpage, int64_t eindex,
		ino_t this_inode, int64_t block_index, off_t this_page_fpos)
{
	int32_t ret;
	PREFETCH_STRUCT_TYPE *temp_prefetch;
	pthread_t prefetch_thread;

	if ((eindex+1) >= MAX_BLOCK_ENTRIES_PER_PAGE)
		return 0;
	if (((tpage->block_entries[eindex+1]).status == ST_CLOUD) ||
		((tpage->block_entries[eindex+1]).status == ST_CtoL)) {

		if (hcfs_system->sync_paused)
			return -EIO;

		temp_prefetch = malloc(sizeof(PREFETCH_STRUCT_TYPE));
		if (temp_prefetch == NULL) {
			write_log(0, "Error cannot open prefetch\n");
			return -ENOMEM;
		}
		temp_prefetch->this_inode = this_inode;
		temp_prefetch->block_no = block_index + 1;
		temp_prefetch->seqnum = tpage->block_entries[eindex+1].seqnum;
		temp_prefetch->page_start_fpos = this_page_fpos;
		temp_prefetch->entry_index = eindex + 1;
		write_log(10, "Prefetching block %lld for inode %" PRIu64 "\n",
			block_index + 1, (uint64_t)this_inode);
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
int32_t read_fetch_backend(ino_t this_inode, int64_t bindex, FH_ENTRY *fh_ptr,
		BLOCK_ENTRY_PAGE *tpage, off_t page_fpos, int64_t eindex)
{
	char thisblockpath[400];
	char objname[1000];
	struct stat tempstat2; /* block ops */
	int32_t ret, errcode, semval;
	META_CACHE_ENTRY_STRUCT *tmpptr = NULL;

	/* Check network status */
	if (hcfs_system->sync_paused)
		return -EIO;

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
		ret = -errno;
		if (fh_ptr->blockfptr != NULL) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->blockfptr = NULL;
		}
		return ret;
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
			/* Do not sync block status if fetch from backend */
			ret = meta_cache_update_file_nosync(fh_ptr->thisinode,
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


#if ENABLE(DEDUP)
		fetch_backend_block_objname(objname,
				tpage->block_entries[eindex].obj_id);
#else
		fetch_backend_block_objname(objname, this_inode, bindex,
				tpage->block_entries[eindex].seqnum);
#endif
		ret = fetch_from_cloud(fh_ptr->blockfptr, READ_BLOCK, objname,
				       tpage->block_entries[eindex].blockID);

		if (ret < 0) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}

			/* Recover status */
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			if (fh_ptr->meta_cache_ptr == NULL)
				return -EIO;
			meta_cache_lookup_file_data(fh_ptr->thisinode,
				NULL, NULL, tpage, page_fpos,
				fh_ptr->meta_cache_ptr);
			if ((tpage->block_entries[eindex]).status == ST_CtoL) {
				(tpage->block_entries[eindex]).status =
								ST_CLOUD;
				/* Do not sync if fetch failed */
				meta_cache_update_file_nosync(fh_ptr->thisinode,
					NULL, NULL, tpage, page_fpos,
					fh_ptr->meta_cache_ptr);
				/* Unlink this block */
				if (access(thisblockpath, F_OK) == 0)
					unlink(thisblockpath);
			}
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);

			return -EIO;
		}

		/* Do not process cache update and stored_where change if block
		*  is actually deleted by other ops such as truncate*/

		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		if (fh_ptr->meta_cache_ptr == NULL) {
			ret = -errno;
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
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
			int64_t block_size_blk;

			if ((tpage->block_entries[eindex]).status == ST_CtoL) {
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
				/* Do not sync if only fetch from backend */
				ret = meta_cache_update_file_nosync(
						fh_ptr->thisinode,
						NULL, NULL, tpage, page_fpos,
						fh_ptr->meta_cache_ptr);
				if (ret < 0)
					goto error_handling;
			}
			/* Update system meta to reflect correct cache size */
			block_size_blk = tempstat2.st_blocks * 512;
			change_system_meta(0, 0, block_size_blk,
					1, 0, 0, TRUE);

			/* Signal cache management that something can be paged
			out */
			semval = 0;
			ret = sem_check_and_release(&(hcfs_system->something_to_replace),
					            &semval);

			tmpptr = fh_ptr->meta_cache_ptr;
			ret = meta_cache_open_file(tmpptr);
			if (ret < 0)
				goto error_handling;
			ret = update_file_stats(tmpptr->fptr, 0,
						1, block_size_blk,
						0, fh_ptr->thisinode);
			if (ret < 0)
				goto error_handling;

			/* Do not sync block status changes due to download */
			/*
			ret = super_block_mark_dirty(fh_ptr->thisinode);
			if (ret < 0)
				goto error_handling;
			*/
		}
	}
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	setbuf(fh_ptr->blockfptr, NULL);
	fh_ptr->opened_block = bindex;

	return 0;
error_handling:
	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_close_file(fh_ptr->meta_cache_ptr);
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	if (fh_ptr->blockfptr != NULL) {
		fclose(fh_ptr->blockfptr);
		fh_ptr->blockfptr = NULL;
	}
	return ret;
}

/* Function for reading from a single block for read operation. Will fetch
*  block from backend if needed. */
size_t _read_block(char *buf, size_t size, int64_t bindex, off_t offset,
                   FH_ENTRY *fh_ptr, ino_t this_inode, int32_t *reterr,
                   uint8_t pin_s)
{
	int64_t current_page;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	off_t this_page_fpos;
	size_t this_bytes_read;
	int64_t entry_index;
	char fill_zeros;
	META_CACHE_ENTRY_STRUCT *tmpptr;
	int32_t ret, errnum;
	ssize_t ret_ssize;

	/* Decide the page index for block "bindex" */
	/*Page indexing starts at zero*/
	current_page = bindex / MAX_BLOCK_ENTRIES_PER_PAGE;

	/*If may need to change meta, lock*/
	fh_ptr->meta_cache_ptr =
			meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		*reterr = -errno;
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

	ret = read_lookup_meta(fh_ptr, &temppage, this_page_fpos);
	if (ret < 0) {
		sem_post(&(fh_ptr->block_sem));
		*reterr = ret;
		return 0;
	}
	write_log(10, "Debug read status %d\n",
		  (temppage).block_entries[entry_index].status);

	/* If status of the current block indicates the block file
	may not exist locally, close the opened block file */
	/* Close opened block file if not sure there is a valid local copy */
	switch ((temppage).block_entries[entry_index].status) {
	case ST_NONE:
	case ST_TODELETE:
	case ST_CLOUD:
	case ST_CtoL:
		write_log(10, "Debug opened block %" PRIu64 ", bindex %" PRIu64
			      "\n",
			  fh_ptr->opened_block, bindex);
		if (fh_ptr->opened_block == bindex) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		break;
	default:
		/* Close the cached file if paged out previously */
		if (((temppage).block_entries[entry_index].paged_out_count !=
		     fh_ptr->cached_paged_out_count) &&
		    (fh_ptr->opened_block != -1)) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		break;
	}

	while (fh_ptr->opened_block != bindex) {
		if (fh_ptr->opened_block != -1) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}

		ret = read_wait_full_cache(&temppage, entry_index, fh_ptr,
			this_page_fpos, pin_s);
		if (ret < 0) {
			sem_post(&(fh_ptr->block_sem));
			*reterr = ret;
			return 0;
		}

		/* return -EIO when failing to fetching from cloud */
		ret = read_prefetch_cache(&temppage, entry_index,
			this_inode, bindex, this_page_fpos);
		if (ret < 0)
			write_log(5, "Fail to prefetch block. Code %d\n", -ret);

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
			/* Download from backend */
			/* return -EIO when failing to fetching from cloud */
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

				BLOCK_ENTRY *tptr;
				tptr = &(temppage.block_entries[entry_index]);
				fh_ptr->cached_paged_out_count =
					tptr->paged_out_count;
			} else {
			/* Some exception that block file is deleted in
			*  the middle of the status check*/
				write_log(2,
					"Debug read: cannot open block file.");
				write_log(2, " Perhaps replaced?\n");
				fh_ptr->opened_block = -1;
			}
			ret = read_lookup_meta(fh_ptr, &temppage, this_page_fpos);
			if (ret < 0) {
				sem_post(&(fh_ptr->block_sem));
				*reterr = ret;
				return 0;
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
		ret_ssize = PREAD(fileno(fh_ptr->blockfptr), buf, sizeof(char) * size,
		      offset);

		this_bytes_read = (size_t) ret_ssize;
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
	int64_t start_block, end_block;
	int64_t block_index;
	int32_t total_bytes_read;
	int32_t this_bytes_read;
	off_t current_offset;
	int32_t target_bytes_read;
	HCFS_STAT temp_stat;
	size_t size;
	char *buf;
	char noatime;
	int32_t ret, errcode;
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	if (system_fh_table.entry_table_flags[file_info->fh] != IS_FH) {
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

	/* There was a section that checks for read permission, but
	in async mode of fuse write, read op will be called to read
	in a page, so this check could cause errors */

	if (fh_ptr->flags & O_NOATIME)
		noatime = TRUE;
	else
		noatime = FALSE;

	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		fuse_reply_err(req, errno);
		return;
	}

	FILE_META_TYPE tempmeta;

	fh_ptr->meta_cache_locked = TRUE;
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat,
			&tempmeta, NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		fuse_reply_err(req, -ret);
		return;
	}
	uint8_t pin_s = tempmeta.local_pin;

	/* Decide the true maximum bytes to read */
	/* If read request will exceed the size of the file, need to
	*  reduce the size of request */
	if (temp_stat.size < (offset + (off_t)size_org)) {
		if (temp_stat.size > offset)
			size = (size_t) (temp_stat.size - offset);
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
	end_block = ((offset + ((off_t)size) - 1) / MAX_BLOCK_SIZE);

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
		if (((off_t)size - total_bytes_read) < target_bytes_read)
			target_bytes_read = (off_t)size - total_bytes_read;

		this_bytes_read = _read_block(&buf[total_bytes_read],
				target_bytes_read, block_index, current_offset,
				fh_ptr, fh_ptr->thisinode, &errcode, pin_s);
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
			fuse_reply_err(req, errno);
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

		set_timestamp_now(&temp_stat, A_TIME);

		/* Write changes to disk but do not sync to backend */
		ret = meta_cache_update_stat_nosync(
		    fh_ptr->thisinode, &temp_stat, fh_ptr->meta_cache_ptr);
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
int32_t write_wait_full_cache(BLOCK_ENTRY_PAGE *temppage, int64_t entry_index,
		FH_ENTRY *fh_ptr, off_t this_page_fpos, PIN_t pin)
{
	int32_t ret;
	int64_t max_cache_size;

	max_cache_size = get_cache_limit(pin);
	if (max_cache_size < 0)
		return -EIO;

	/* Adding cache check for new or deleted blocks */
	while (
	    (((temppage->block_entries[entry_index]).status == ST_CLOUD) ||
	     ((temppage->block_entries[entry_index]).status == ST_CtoL)) ||
	    (((temppage->block_entries[entry_index]).status == ST_TODELETE) ||
	     ((temppage->block_entries[entry_index]).status == ST_NONE))) {

		write_log(10,
			"Debug write checking if need to wait for cache\n");
		write_log(10, "%lld, %lld\n",
			hcfs_system->systemdata.cache_size,
			max_cache_size);
		if (hcfs_system->systemdata.cache_size > max_cache_size) {
			if (CURRENT_BACKEND == NONE ||
			    hcfs_system->system_restoring == RESTORING_STAGE1)
				return -ENOSPC;
			if (hcfs_system->system_going_down == TRUE)
				return -EBUSY;
			/*Sleep if cache already full*/
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			sem_post(&(fh_ptr->block_sem));

			write_log(10, "debug write waiting on full cache\n");
			ret = sleep_on_cache_full();
			sem_wait(&(fh_ptr->block_sem));
			if (ret < 0)
				return ret;

			/*Re-read status*/
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			if (fh_ptr->meta_cache_ptr == NULL) {
				return -errno;
			}
			fh_ptr->meta_cache_locked = TRUE;

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
int32_t _write_fetch_backend(ino_t this_inode, int64_t bindex, FH_ENTRY *fh_ptr,
		BLOCK_ENTRY_PAGE *tpage, off_t page_fpos, int64_t eindex,
		PIN_t pin)
{
	char thisblockpath[400];
	char objname[1000];
	struct stat tempstat2; /* block ops */
	int32_t ret, errcode;
	int64_t unpin_dirty_size;
	META_CACHE_ENTRY_STRUCT *tmpptr;

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
			ret = meta_cache_update_file_nosync(fh_ptr->thisinode,
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


#if ENABLE(DEDUP)
		fetch_backend_block_objname(objname,
				tpage->block_entries[eindex].obj_id);
#else
		fetch_backend_block_objname(objname, this_inode, bindex,
				tpage->block_entries[eindex].seqnum);
#endif
		ret = fetch_from_cloud(fh_ptr->blockfptr, READ_BLOCK, objname,
				       tpage->block_entries[eindex].blockID);

		if (ret < 0) {
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return -EIO;
		}
		/*Do not process cache update and stored_where change if block
		* is actually deleted by other ops such as truncate*/

		/*Re-read status*/
		fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
		if (fh_ptr->meta_cache_ptr == NULL) {
			ret = -errno;
			if (fh_ptr->blockfptr != NULL) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->blockfptr = NULL;
			}
			return ret;
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
			int64_t block_size_blk;

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
			block_size_blk = tempstat2.st_blocks * 512;
			unpin_dirty_size = (P_IS_UNPIN(pin) ?
					block_size_blk : 0);
			change_system_meta_ignore_dirty(
			    this_inode, 0, 0, block_size_blk, 1, block_size_blk,
			    unpin_dirty_size, TRUE);
			tmpptr = fh_ptr->meta_cache_ptr;
			ret = meta_cache_open_file(tmpptr);
			if (ret < 0)
				goto error_handling;
			ret = update_file_stats(tmpptr->fptr, 0,
						1, block_size_blk,
						block_size_blk,
						fh_ptr->thisinode);
			if (ret < 0)
				goto error_handling;
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
error_handling:
	if (fh_ptr->blockfptr != NULL) {
		fclose(fh_ptr->blockfptr);
		fh_ptr->blockfptr = NULL;
	}
	return ret;
}

/* Function for writing to a single block for write operation. Will fetch
*  block from backend if needed. */
size_t _write_block(const char *buf, size_t size, int64_t bindex,
		off_t offset, FH_ENTRY *fh_ptr, ino_t this_inode,
		int32_t *reterr, PIN_t pin, int64_t now_seq)
{
	int64_t current_page;
	char thisblockpath[400];
	BLOCK_ENTRY_PAGE temppage;
	off_t this_page_fpos;
	off_t old_cache_size, new_cache_size;
	size_t this_bytes_written;
	int64_t entry_index;
	int32_t ret, errnum, errcode;
	int64_t tmpcachesize, tmpdiff;
	int64_t unpin_dirty_size;
	int64_t max_cache_size;
	META_CACHE_ENTRY_STRUCT *tmpptr;
	ssize_t ret_ssize;
	BOOL block_dirty;

	/* Check system size before writing */
	if (hcfs_system->systemdata.system_size >
			hcfs_system->systemdata.system_quota) {
		*reterr = -ENOSPC;
		write_log(4, "Warn: system no space?\n");
		return 0;
	}

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

	/* Check if cache space is full */
	if (access(thisblockpath, F_OK) == 0) {
		old_cache_size = check_file_size(thisblockpath);
		tmpcachesize = hcfs_system->systemdata.cache_size;
		tmpdiff = (offset + (off_t) size) - old_cache_size;
		write_log(10, "%zu, %lld, %lld, %lld, %lld\n", size,
			(off_t) size, tmpdiff, offset, old_cache_size);
		max_cache_size = get_cache_limit(pin);
		if (max_cache_size < 0) {
			*reterr = -EIO;
			return 0;
		}
		if ((tmpdiff > 0) &&
			((tmpdiff + tmpcachesize) > max_cache_size)) {
			/* Need to sleep for full here or return ENOSPC */
			if (fh_ptr->opened_block != -1) {
				fclose(fh_ptr->blockfptr);
				fh_ptr->opened_block = -1;
			}
			if (CURRENT_BACKEND == NONE ||
			    hcfs_system->system_restoring == RESTORING_STAGE1) {
				*reterr = -ENOSPC;
				return 0;
			}
			/*Sleep if cache already full*/
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			sem_post(&(fh_ptr->block_sem));

			write_log(10, "debug write waiting on full cache\n");
			ret = sleep_on_cache_full();
			sem_wait(&(fh_ptr->block_sem));
			if (ret < 0) {
				*reterr = ret;
				return 0;
			}

			/*Re-read status*/
			fh_ptr->meta_cache_ptr =
				meta_cache_lock_entry(fh_ptr->thisinode);
			if (fh_ptr->meta_cache_ptr == NULL) {
				*reterr = -errno;
				return 0;
			}
			fh_ptr->meta_cache_locked = TRUE;
		}
	}

	/* Check latest block status */
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, NULL,
			NULL, &temppage, this_page_fpos,
			fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		*reterr = ret;
		return 0;
	}

	/* If status of the current block indicates the block file
	may not exist locally, close the opened block file */
	/* Close opened block file if not sure there is a valid local copy */
	switch ((temppage).block_entries[entry_index].status) {
	case ST_NONE:
	case ST_TODELETE:
	case ST_CLOUD:
	case ST_CtoL:
		if (fh_ptr->opened_block == bindex) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		break;
	default:
		/* Close the cached file if paged out previously */
		if (((temppage).block_entries[entry_index].paged_out_count !=
		     fh_ptr->cached_paged_out_count) &&
		    (fh_ptr->opened_block != -1)) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}
		break;
	}

	block_dirty = FALSE;
	/* Check if we can reuse cached block */
	if (fh_ptr->opened_block != bindex) {
		int64_t seq;
		/* If the cached block is not the one we are writing to,
		*  close the one already opened. */
		if (fh_ptr->opened_block != -1) {
			fclose(fh_ptr->blockfptr);
			fh_ptr->opened_block = -1;
		}

		ret = write_wait_full_cache(&temppage, entry_index, fh_ptr,
					    this_page_fpos, pin);
		if (ret < 0) {
			*reterr = ret;
			return 0;
		}

		while (hcfs_system->system_going_down == FALSE) {
			seq = temppage.block_entries[entry_index].seqnum;
			switch ((temppage).block_entries[entry_index].status) {
			case ST_NONE:
			case ST_TODELETE:
				/*If not stored anywhere, make it on local disk*/
				fh_ptr->blockfptr = fopen(thisblockpath, "a+");
				if (fh_ptr->blockfptr == NULL) {
					errnum = errno;
					*reterr = -EIO;
					write_log(0, "Error in write. Code %d, %s\n",
						errnum, strerror(errnum));
					return 0;
				}
				fclose(fh_ptr->blockfptr);
				(temppage).block_entries[entry_index].status =
						ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
							NULL, TRUE);
				if (ret < 0) {
					*reterr = -EIO;
					return 0;
				}
				ret = meta_cache_update_file_data(
					fh_ptr->thisinode, NULL, NULL,
					&temppage, this_page_fpos,
					fh_ptr->meta_cache_ptr);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}

				tmpptr = fh_ptr->meta_cache_ptr;
				change_system_meta(0, 0, 0, 1, 0, 0, TRUE);
				ret = meta_cache_open_file(tmpptr);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}
				ret = update_file_stats(tmpptr->fptr, 1,
						1, 0, 0, fh_ptr->thisinode);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}
				break;
			case ST_LDISK:
				block_dirty = TRUE;
				ret = _check_sync_wait_full_cache(
						&(fh_ptr->meta_cache_ptr),
						this_inode, bindex, seq,
						&temppage, this_page_fpos);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				} else if (ret > 0) { /* Check status again */
					continue;
				}
				break;
			case ST_BOTH:
			case ST_LtoC:
				if ((temppage).block_entries[entry_index].status ==
						ST_LtoC) {
					block_dirty = TRUE;
					ret = _check_sync_wait_full_cache(
						&(fh_ptr->meta_cache_ptr),
						this_inode, bindex, seq,
						&temppage, this_page_fpos);
					if (ret < 0) {
						*reterr = ret;
						return 0;
					/* Check status again */
					} else if (ret > 0) {
						continue;
					}
				}
				(temppage).block_entries[entry_index].status =
					ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					*reterr = -EIO;
					return 0;
				}
				ret = meta_cache_update_file_data(
						fh_ptr->thisinode,
						NULL, NULL, &temppage,
						this_page_fpos,
						fh_ptr->meta_cache_ptr);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}
				break;
			case ST_CLOUD:
			case ST_CtoL:
				/*Download from backend */
				block_dirty = TRUE;
				ret = _write_fetch_backend(this_inode, bindex,
					fh_ptr, &temppage, this_page_fpos,
					entry_index, pin);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}
				break;
			default:
				break;
			}

			/* Leave loop */
			break;
		}

		fh_ptr->blockfptr = fopen(thisblockpath, "r+");
		if (fh_ptr->blockfptr == NULL) {
			errnum = errno;
			*reterr = -EIO;
			write_log(0, "Error in write. Code %d, %s\n",
				errnum, strerror(errnum));
			return 0;
		}
		setbuf(fh_ptr->blockfptr, NULL);
		fh_ptr->opened_block = bindex;
		fh_ptr->cached_paged_out_count =
			(temppage).block_entries[entry_index].paged_out_count;
	} else {
		int64_t seq;

		while (hcfs_system->system_going_down == FALSE) {
			seq = temppage.block_entries[entry_index].seqnum;
			/* Check if there is a need for status change */
			switch ((temppage).block_entries[entry_index].status) {
			case ST_LDISK:
				block_dirty = TRUE;
				ret = _check_sync_wait_full_cache(
					&(fh_ptr->meta_cache_ptr),
					this_inode, bindex, seq,
					&temppage, this_page_fpos);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				} else if (ret > 0) { /* Check status again */
					continue;
				}
				break;
			case ST_BOTH:
			case ST_LtoC:
				if ((temppage).block_entries[entry_index].status
						== ST_LtoC) {
					block_dirty = TRUE;
					ret = _check_sync_wait_full_cache(
						&(fh_ptr->meta_cache_ptr),
						this_inode, bindex, seq,
						&temppage, this_page_fpos);
					if (ret < 0) {
						*reterr = ret;
						return 0;
					/* Check status again */
					} else if (ret > 0) {
						continue;
					}
				}
				(temppage).block_entries[entry_index].status =
						ST_LDISK;
				ret = set_block_dirty_status(thisblockpath,
						NULL, TRUE);
				if (ret < 0) {
					*reterr = -EIO;
					return 0;
				}
				ret = meta_cache_update_file_data(
						fh_ptr->thisinode, NULL, NULL,
						&temppage, this_page_fpos,
						fh_ptr->meta_cache_ptr);
				if (ret < 0) {
					*reterr = ret;
					return 0;
				}
				break;
			default:
				break;
			}

			/* Leave loop */
			break;
		}
	}

	ret = flock(fileno(fh_ptr->blockfptr), LOCK_EX);
	if (ret < 0) {
		errnum = errno;
		*reterr = -EIO;
		write_log(0, "Error in write. Code %d, %s\n",
			errnum, strerror(errnum));
		return 0;
	}

	old_cache_size = check_file_size(thisblockpath);

	/* There was a ftruncate line here, which is not needed as
	pwrite can extend beyond the current size. This also seems
	to cause data corruption when writeback cache is enabled */

	ret_ssize = PWRITE(fileno(fh_ptr->blockfptr), buf, sizeof(char) * size, offset);

	this_bytes_written = (size_t) ret_ssize;
	new_cache_size = check_file_size(thisblockpath);

	if (this_bytes_written != 0) {
		int64_t dirty_delta;

		if (block_dirty == TRUE)
			dirty_delta = new_cache_size - old_cache_size;
		else
			dirty_delta = new_cache_size;

		unpin_dirty_size = (P_IS_UNPIN(pin) ? dirty_delta : 0);
		change_system_meta_ignore_dirty(
		    this_inode, 0, 0, new_cache_size - old_cache_size, 0,
		    dirty_delta, unpin_dirty_size, TRUE);

		tmpptr = fh_ptr->meta_cache_ptr;
		ret = meta_cache_open_file(tmpptr);
		if (ret < 0) {
			*reterr = ret;
			return 0;
		}
		ret = update_file_stats(tmpptr->fptr, 0,
				0, new_cache_size - old_cache_size,
				dirty_delta, fh_ptr->thisinode);
		if (ret < 0) {
			*reterr = ret;
			return 0;
		}
	}

	/* Update block seq num */
	ret = update_block_seq(fh_ptr->meta_cache_ptr, this_page_fpos,
			entry_index, bindex, now_seq, &temppage);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	flock(fileno(fh_ptr->blockfptr), LOCK_UN);

	return this_bytes_written;

errcode_handle:
	flock(fileno(fh_ptr->blockfptr), LOCK_UN);
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
	int64_t start_block, end_block;
	int64_t block_index;
	off_t total_bytes_written;
	size_t this_bytes_written;
	off_t current_offset;
	size_t target_bytes_written;
	HCFS_STAT temp_stat;
	int32_t ret, errcode;
	ino_t thisinode;
	MOUNT_T *tmpptr;
	FILE_META_TYPE thisfilemeta;
	int64_t pin_sizediff, amount_preallocated;
	int64_t old_metasize, new_metasize, delta_meta_size;
	int64_t old_metasize_blk, new_metasize_blk, delta_meta_size_blk;
	int64_t now_seq;
	int64_t max_pinned_size;
	long avail_space, avail_space1, avail_space2;

	write_log(10, "Debug write: size %zu, offset %lld\n", size, offset);

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	thisinode = real_ino(req, ino);

	/* If size after writing is greater than max file size,
	return -EFBIG */
	if (size > (size_t) MAX_FILE_SIZE) {
		fuse_reply_err(req, EFBIG);
		return;
	}

	if (offset > (off_t) (MAX_FILE_SIZE - size)) {
		fuse_reply_err(req, EFBIG);
		return;
	}

	if (offset < 0) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	if (system_fh_table.entry_table_flags[file_info->fh] != IS_FH) {
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
	end_block = ((offset + ((off_t)size) - 1) / MAX_BLOCK_SIZE);

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

	sem_wait(&(fh_ptr->block_sem));
	fh_ptr->meta_cache_ptr = meta_cache_lock_entry(fh_ptr->thisinode);
	if (fh_ptr->meta_cache_ptr == NULL) {
		test_and_notify_no_space(errno);
		fuse_reply_err(req, errno);
		return;
	}
	fh_ptr->meta_cache_locked = TRUE;

	meta_cache_get_meta_size(fh_ptr->meta_cache_ptr, &old_metasize,
			&old_metasize_blk);

	/* Move update_meta_seq() here so that avoid race condition
	 * caused from write by multi-threads */
	ret = update_meta_seq(fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		sem_post(&(fh_ptr->block_sem));
		test_and_notify_no_space(-ret);
		fuse_reply_err(req, -ret);
		return;
	}

	/* If the file is pinned, need to change the pinned_size
	first if necessary */
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat,
			&thisfilemeta, NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		sem_post(&(fh_ptr->block_sem));
		test_and_notify_no_space(-ret);
		fuse_reply_err(req, -ret);
		return;
	}

	/* Remember now sequence number */
	now_seq = thisfilemeta.finished_seq;

	/* Get cache threshold */
	max_pinned_size = get_pinned_limit(thisfilemeta.local_pin);
	if (max_pinned_size < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		sem_post(&(fh_ptr->block_sem));
		fuse_reply_err(req, EIO);
		return;
	}

	write_log(10, "Debug: system size: %lld, system quota %lld",
		hcfs_system->systemdata.system_size,
		hcfs_system->systemdata.system_quota);
	write_log(10, "Write details: seq %lld\n", thisfilemeta.finished_seq);
	write_log(10, "Write details: %d, %lld, %zu\n", thisfilemeta.local_pin,
		  offset, size);
	write_log(10, "Write details: %lld, %lld, %f\n", temp_stat.size,
		  hcfs_system->systemdata.pinned_size, max_pinned_size);
	amount_preallocated = 0;
	if (P_IS_PIN(thisfilemeta.local_pin) &&
	    ((offset + (off_t)size) > temp_stat.size)) {
		sem_wait(&(hcfs_system->access_sem));
		pin_sizediff = round_size(offset + (off_t)size) -
					round_size(temp_stat.size);
		write_log(10, "Write details: %lld, %lld\n", pin_sizediff,
			  (off_t)size);
		if ((hcfs_system->systemdata.pinned_size + pin_sizediff)
			> max_pinned_size) {
			sem_post(&(hcfs_system->access_sem));
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			sem_post(&(fh_ptr->block_sem));
			notify_avail_space(0);
			fuse_reply_err(req, ENOSPC);
			return;
		}

		hcfs_system->systemdata.pinned_size += pin_sizediff;
		amount_preallocated = pin_sizediff;
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	for (block_index = start_block; block_index <= end_block;
							block_index++) {
		current_offset = (offset+total_bytes_written) % MAX_BLOCK_SIZE;

		target_bytes_written = MAX_BLOCK_SIZE - current_offset;

		/* If do not need to write that much */
		if ((size - total_bytes_written) < target_bytes_written)
			target_bytes_written = size - total_bytes_written;

		this_bytes_written = _write_block(&buf[total_bytes_written],
				target_bytes_written, block_index,
				current_offset, fh_ptr, fh_ptr->thisinode,
				&errcode, thisfilemeta.local_pin, now_seq);
		if ((this_bytes_written == 0) && (errcode < 0)) {
			if (fh_ptr->meta_cache_ptr != NULL) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(fh_ptr->meta_cache_ptr);
				meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			}
			goto errcode_handle;
		}

		total_bytes_written += (off_t) this_bytes_written;

		/*Terminate if cannot write as much as we want*/
		if (this_bytes_written < target_bytes_written)
			break;
	}

	meta_cache_get_meta_size(fh_ptr->meta_cache_ptr, &new_metasize,
			&new_metasize_blk);
	if (old_metasize > 0 && new_metasize > 0) {
		delta_meta_size = (new_metasize - old_metasize);
		delta_meta_size_blk = (new_metasize_blk - old_metasize_blk);
	} else {
		delta_meta_size = 0;
		delta_meta_size_blk = 0;
	}

	/*Update and flush file meta*/
	ret = meta_cache_lookup_file_data(fh_ptr->thisinode, &temp_stat,
			&thisfilemeta, NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		errcode = ret;
		goto errcode_handle;
	}

	if (P_IS_PIN(thisfilemeta.local_pin) &&
	    ((temp_stat.size < (offset + (off_t)size)) &&
	     (((off_t) size) > total_bytes_written))) {
		/* If size written not equal to as planned, need
		to change pinned_size */
		/* If offset + total_bytes_written < st_size, substract
		the difference between size and st_size */
		if (offset + total_bytes_written < temp_stat.size)
			pin_sizediff = round_size(offset + (off_t) size) -
				round_size(temp_stat.size);
		else
			pin_sizediff = round_size(offset + (off_t)size) -
				round_size(offset + total_bytes_written);
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size -= pin_sizediff;
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	if (temp_stat.size < (offset + total_bytes_written)) {
		change_system_meta((int64_t) ((offset + total_bytes_written)
				- temp_stat.size + delta_meta_size),
				delta_meta_size_blk,
				0, 0, 0, 0, TRUE);
		ret = change_mount_stat(tmpptr,
			(int64_t) ((offset + total_bytes_written)
			- temp_stat.size), delta_meta_size, 0);
		if (ret < 0) {
			fh_ptr->meta_cache_locked = FALSE;
			meta_cache_close_file(fh_ptr->meta_cache_ptr);
			meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
			sem_post(&(fh_ptr->block_sem));
			test_and_notify_no_space(-ret);
			fuse_reply_err(req, -ret);
			return;
		}

		temp_stat.size = (offset + total_bytes_written);
		temp_stat.blocks = (temp_stat.size+511) / 512;
	} else {
		if (delta_meta_size != 0) {
			change_system_meta(delta_meta_size, delta_meta_size_blk,
					0, 0, 0, 0, TRUE);
			ret = change_mount_stat(tmpptr, 0, delta_meta_size, 0);
			if (ret < 0) {
				fh_ptr->meta_cache_locked = FALSE;
				meta_cache_close_file(fh_ptr->meta_cache_ptr);
				meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
				sem_post(&(fh_ptr->block_sem));
				test_and_notify_no_space(-ret);
				fuse_reply_err(req, -ret);
				return;
			}
		}
	}

	if (total_bytes_written > 0)
		set_timestamp_now(&temp_stat, M_TIME | C_TIME);

	ret = meta_cache_update_file_data(fh_ptr->thisinode, &temp_stat, NULL,
					NULL, 0, fh_ptr->meta_cache_ptr);
	if (ret < 0) {
		fh_ptr->meta_cache_locked = FALSE;
		meta_cache_close_file(fh_ptr->meta_cache_ptr);
		meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
		sem_post(&(fh_ptr->block_sem));
		test_and_notify_no_space(-ret);
		fuse_reply_err(req, -ret);
		return;
	}

	fh_ptr->meta_cache_locked = FALSE;
	meta_cache_unlock_entry(fh_ptr->meta_cache_ptr);
	sem_post(&(fh_ptr->block_sem));

	avail_space1 = max_pinned_size - hcfs_system->systemdata.pinned_size;
	avail_space2 =
	    hcfs_system->systemdata.system_quota -
	    hcfs_system->systemdata.system_size;
	avail_space = avail_space1 < avail_space2 ? avail_space1 : avail_space2;
	notify_avail_space(avail_space - WRITEBACK_CACHE_RESERVE_SPACE);

	fuse_reply_write(req, total_bytes_written);
	return;
errcode_handle:
	/* If op failed and size won't be extended, revert the preallocated
	pinned space */
	if (amount_preallocated > 0) {
		sem_wait(&(hcfs_system->access_sem));
		hcfs_system->systemdata.pinned_size -= amount_preallocated;
		if (hcfs_system->systemdata.pinned_size < 0)
			hcfs_system->systemdata.pinned_size = 0;
		sem_post(&(hcfs_system->access_sem));
	}

	sem_post(&(fh_ptr->block_sem));
	test_and_notify_no_space(-errcode);
	fuse_reply_err(req, -errcode);
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
	MOUNT_T *tmpptr;
	int64_t system_size, num_inodes, free_block;
	int64_t quota;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	/* TODO: Different statistics for different filesystems */
	write_log(10, "Debug statfs\n");

	/* thisinode = */real_ino(req, ino);

	buf = malloc(sizeof(struct statvfs));
	if (buf == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	/*Prototype is linux statvfs call*/
	sem_wait((tmpptr->stat_lock));

	system_size = hcfs_system->systemdata.system_size;
	write_log(10, "Debug: system_size is %lld\n", system_size);
	write_log(10, "Debug: volume size is %lld\n",
			(tmpptr->FS_stat)->system_size);
	num_inodes = (tmpptr->FS_stat)->num_inodes;

	sem_post((tmpptr->stat_lock));

	/* TODO: If no backend, use cache size as total volume size */
	buf->f_bsize = 4096;
	buf->f_frsize = 4096;
	/*if (system_size > (512 * powl(1024, 3)))
		buf->f_blocks = (((system_size - 1) / 4096) + 1) * 2;
	else*/
	quota = hcfs_system->systemdata.system_quota;
	buf->f_blocks = quota ? (quota - 1) / buf->f_bsize + 1 : 0;

	if (system_size == 0) {
		buf->f_bfree = buf->f_blocks;
	} else {
		/* we have assigned double size of system blocks, so it
		 * will keep being postive after subtracting */
		free_block = buf->f_blocks - (((system_size - 1) / 4096) + 1);
		buf->f_bfree = free_block < 0 ? 0 : free_block;
	}

	buf->f_bavail = buf->f_bfree;

	write_log(10, "Debug statfs, checking inodes\n");

	/* mekes f_files larger than num_inodes, if num_inodes >
	 * 9223372036854775807, num_inodes*2 overflows */
	if (num_inodes > 1000000)
		buf->f_files = (num_inodes * 2);
	else
		buf->f_files = 2000000;

	/* TODO: BUG here: buf->f_ffree is unsugned and may become larger
	 * if runs into negative value */
	if (num_inodes < 0) {
		/* when FS_stat.num_inodes >= 9223372036854775808,
		 * num_inodes will become negative here and system free
		 * inode will gets reset */
		buf->f_ffree = buf->f_files;
	} else if (buf->f_files < (uint64_t)num_inodes) {
		/* over used inodes */
		buf->f_ffree = 0;
	} else {
		buf->f_ffree = buf->f_files - num_inodes;
	}

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
	/* ino_t thisinode; */

	UNUSED(ino);
	UNUSED(file_info);
	/* thisinode = */real_ino(req, ino);

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

	/* file_info->fh is uint64_t */

	if (file_info->fh >= MAX_OPEN_FILE_ENTRIES) {
		fuse_reply_err(req, EBADF);
		return;
	}

	if (system_fh_table.entry_table_flags[file_info->fh] != IS_FH) {
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
*        Inputs: fuse_req_t req, fuse_ino_t ino, int32_t isdatasync,
*                struct fuse_file_info *file_info
*       Summary: Conduct "fsync". Do nothing now (see hfuse_flush).
*
*************************************************************************/
void hfuse_ll_fsync(fuse_req_t req, fuse_ino_t ino, int32_t isdatasync,
					struct fuse_file_info *file_info)
{
	/* ino_t thisinode; */

	UNUSED(ino);
	UNUSED(isdatasync);
	UNUSED(file_info);
	/* thisinode = */real_ino(req, ino);

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
	int32_t ret_val;
	HCFS_STAT this_stat;
	ino_t thisinode;
	long long dirh;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	thisinode = real_ino(req, ino);

	ret_val = fetch_inode_stat(thisinode, &this_stat, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat, NULL, NULL);
	}
#endif

	if (!S_ISDIR(this_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &this_stat, 4, FALSE);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	dirh = open_fh(thisinode, 0, TRUE);
	if (dirh < 0) {
		fuse_reply_err(req, ENFILE);
		return;
	}

	file_info->fh = dirh;

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
	int32_t count;
	off_t thisfile_pos;
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE _temp_page, *temp_page;
	struct stat tempstat; /* fuse reply sys stat */
	HCFS_STAT thisstat;
	struct timeval tmp_time1, tmp_time2;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	int64_t countn;
	off_t nextentry_pos;
	int32_t page_start;
	char *buf, *tmpstrptr;;
	off_t buf_pos;
	size_t entry_size;
	int32_t ret;
	char this_type;
	DIRH_ENTRY *dirh_ptr;
	int32_t snap_access_val;
	struct timespec snap_sleep;
	BOOL use_snap;

	snap_sleep.tv_sec = 0;
	snap_sleep.tv_nsec = 100000000;
	buf = NULL;
	use_snap = FALSE;

	gettimeofday(&tmp_time1, NULL);

	this_inode = real_ino(req, ino);

	write_log(10, "DEBUG readdir entering readdir, ");
	write_log(10, "size %ld, offset %ld\n", size, offset);

	if (system_fh_table.entry_table_flags[file_info->fh] != IS_DIRH) {
		fuse_reply_err(req, EBADF);
		return;
	}

	dirh_ptr = &(system_fh_table.direntry_table[file_info->fh]);

	if (dirh_ptr == NULL) {
		fuse_reply_err(req, EBADF);
		return;
	}

	/* Check if ino passed in is the same as the one stored */

	if (dirh_ptr->thisinode != this_inode) {
		fuse_reply_err(req, EBADF);
		return;
	}

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL) {
		fuse_reply_err(req, errno);
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

	/* If snapshot of dir is created, and offset is 0, need to wait
	for other snapshot references to finish and then close the snapshot */
	if (dirh_ptr->snapshot_ptr != NULL) {
		sem_wait(&(dirh_ptr->wait_ref_sem));
		if (offset == 0) {
			/* Need to wait for threads using this snapshot
			to finish */
			sem_getvalue(&(dirh_ptr->snap_ref_sem),
				     &snap_access_val);
			while (snap_access_val > 0) {
				nanosleep(&snap_sleep, NULL);
				sem_getvalue(&(dirh_ptr->snap_ref_sem),
					     &snap_access_val);
			}

			sem_wait(&(system_fh_table.fh_table_sem));
			fclose(dirh_ptr->snapshot_ptr);
			dirh_ptr->snapshot_ptr = NULL;
			system_fh_table.have_nonsnap_dir = TRUE;
			sem_post(&(system_fh_table.fh_table_sem));
		}
		sem_post(&(dirh_ptr->wait_ref_sem));
		if ((dirh_ptr->snapshot_ptr != NULL) && (offset != 0)) {
			sem_post(&(dirh_ptr->snap_ref_sem));
			use_snap = TRUE;
			PREAD(fileno(dirh_ptr->snapshot_ptr), &tempmeta,
			      sizeof(DIR_META_TYPE), sizeof(HCFS_STAT));
		}
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
				free(buf);
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
					free(buf);
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
		temp_page = &_temp_page;
		memset(temp_page, 0, sizeof(DIR_ENTRY_PAGE));
		temp_page->this_page_pos = thisfile_pos;
		if (use_snap == TRUE) {
			FSEEK(dirh_ptr->snapshot_ptr, thisfile_pos, SEEK_SET);
			FREAD(temp_page, sizeof(DIR_ENTRY_PAGE), 1,
			      dirh_ptr->snapshot_ptr);
		} else if ((tempmeta.total_children <=
			    (MAX_DIR_ENTRIES_PER_PAGE - 2)) &&
			   (page_start == 0)) {
			ret = meta_cache_lookup_dir_data(this_inode, NULL,
						NULL, temp_page, body_ptr);
			if (ret < 0) {
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, -ret);
				free(buf);
				return;
			}
		} else {
			temp_page = MREAD(body_ptr, NULL, sizeof(DIR_ENTRY_PAGE),
			      thisfile_pos);
		}

		write_log(10, "Debug readdir page start %d %d\n", page_start,
			temp_page->num_entries);
		for (count = page_start; count < temp_page->num_entries;
								count++) {
			memset(&tempstat, 0, sizeof(tempstat));
			tempstat.st_ino = temp_page->dir_entries[count].d_ino;
			this_type = temp_page->dir_entries[count].d_type;
			if (this_type == D_ISREG)
				tempstat.st_mode = S_IFREG;
			else if (this_type == D_ISDIR)
				tempstat.st_mode = S_IFDIR;
			else if (this_type == D_ISLNK)
				tempstat.st_mode = S_IFLNK;
			else if (this_type == D_ISFIFO)
				tempstat.st_mode = S_IFIFO;
			else if (this_type == D_ISSOCK)
				tempstat.st_mode = S_IFSOCK;

			/* Rebuild parent lookup / dir statistics here
			(excluding . and ..), for every pair of (parent / child)
			discovered here */
			if (hcfs_system->system_restoring
			    == RESTORING_STAGE2) {
				tmpstrptr = temp_page->dir_entries[count].d_name;
				if ((strcmp(tmpstrptr, ".") != 0) &&
				    (strcmp(tmpstrptr, "..") != 0))
					rebuild_parent_stat(tempstat.st_ino,
						this_inode, this_type);
			}

			nextentry_pos = temp_page->this_page_pos *
				(MAX_DIR_ENTRIES_PER_PAGE + 1) + (count+1);
			entry_size = fuse_add_direntry(
			    req, &buf[buf_pos], (size - buf_pos),
			    temp_page->dir_entries[count].d_name, &tempstat,
			    nextentry_pos);
			write_log(10, "Debug readdir entry %s, %" PRIu64 "\n",
				temp_page->dir_entries[count].d_name,
				(uint64_t)tempstat.st_ino);
			write_log(10, "Debug readdir entry size %ld\n",
				entry_size);
			if (entry_size > (size - buf_pos)) {
				meta_cache_unlock_entry(body_ptr);
				write_log(10,
					"Readdir breaks, next offset %ld, ",
					nextentry_pos);
				write_log(10, "file pos %lld, entry %d\n",
					temp_page->this_page_pos, (count+1));
				fuse_reply_buf(req, buf, buf_pos);
				free(buf);
				return;
			}
			buf_pos += entry_size;
		}
		page_start = 0;
		thisfile_pos = temp_page->tree_walk_next;
	}

	ret = 0;
	if (buf_pos > 0) {
		/* Do not sync A_TIME change to backend */
		set_timestamp_now(&thisstat, A_TIME);
		ret = meta_cache_update_stat_nosync(this_inode, &thisstat,
						    body_ptr);
	}
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret < 0) {
		fuse_reply_err(req, -ret);
		free(buf);
		return;
	}
	gettimeofday(&tmp_time2, NULL);

	write_log(10, "readdir elapse %f\n",
			(tmp_time2.tv_sec - tmp_time1.tv_sec)
			+ 0.000001 * (tmp_time2.tv_usec - tmp_time1.tv_usec));

	fuse_reply_buf(req, buf, buf_pos);

	free(buf);

	if (use_snap == TRUE)
		sem_trywait(&(dirh_ptr->snap_ref_sem));

	return;

errcode_handle:
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	fuse_reply_err(req, -errcode);
	free(buf);
	if (use_snap == TRUE)
		sem_trywait(&(dirh_ptr->snap_ref_sem));
}

/************************************************************************
*
* Function name: hfuse_ll_releasedir
*        Inputs: fuse_req_t req, fuse_ino_t ino,
*                struct fuse_file_info *file_info
*       Summary: Close opened directory.
*
*************************************************************************/
void hfuse_ll_releasedir(fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *file_info)
{
	ino_t thisinode;

	thisinode = real_ino(req, ino);

	if (file_info->fh >= MAX_OPEN_FILE_ENTRIES) {
		write_log(10, "FH too large\n");
		fuse_reply_err(req, EBADF);
		return;
	}

	if (system_fh_table.entry_table_flags[file_info->fh] != IS_DIRH) {
		write_log(10, "Handle is not a DIRH\n");
		fuse_reply_err(req, EBADF);
		return;
	}

	if (system_fh_table.direntry_table[file_info->fh].thisinode
					!= thisinode) {
		write_log(10, "Handle is not itself\n");
		fuse_reply_err(req, EBADF);
		return;
	}

	close_fh(file_info->fh);

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

	UNUSED(conn);
	tmpptr = (MOUNT_T *)userdata;
	write_log(10, "Root inode is %" PRIu64 "\n", (uint64_t)tmpptr->f_ino);

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
	write_log(10, "Unmounting FS with root inode %" PRIu64 "\n",
		  (uint64_t)tmpptr->f_ino);
}

/* Helper for converting string to 64-bit mask */
uint64_t str_to_mask(const char *input)
{
	int32_t count;
	uint64_t tempout;

	tempout = 0;

        /* First compute the length of the mask */
	if (strlen(input) < 16)
		return 0;
        for (count = 0; count < 16; count++) {
                if ((input[count] <= 'f') && (input[count] >= 'a')) {
                        tempout = tempout << 4;
                        tempout += (10 + (input[count] - 'a'));
                        continue;
                }
                if ((input[count] <= '9') && (input[count] >= '0')) {
                        tempout = tempout << 4;
                        tempout += (input[count] - '0');
                        continue;
                }
		tempout = 0;
                break;
        }

	return tempout;
}

/* Helper for checking chown capability */
BOOL _check_capability(pid_t thispid, int32_t cap_to_check)
{
	char proc_status_path[100];
        char tmpstr[100], outstr[20] = {0};
        char *saveptr, *outptr;
        FILE *fptr;
	uint64_t cap_mask, op_mask, result_mask;
	int32_t errcode;

	snprintf(proc_status_path, sizeof(proc_status_path), "/proc/%d/status",
		 thispid);
	fptr = fopen(proc_status_path, "r");
	if (!fptr) {
		errcode = errno;
		write_log(4, "Cannot open %s. Code %d\n", proc_status_path,
			  errcode);
		return FALSE;
	}

	do {
		fgets(tmpstr, 80, fptr);
		outptr = strtok_r(tmpstr, "\t", &saveptr);
		if (strcmp(outptr, "CapEff:") == 0) {
			outptr = strtok_r(NULL, "\t", &saveptr);
			snprintf(outstr, sizeof(outstr), "%s", outptr);
			break;
		} else {
			continue;
		}
	} while (!feof(fptr));
	fclose(fptr);
	/* Convert string to 64bit bitmask */
	if (strlen(outstr) < 16) {
		write_log(4, "Length of bitmask is shorter than expected\n");
		return FALSE;
	}

	cap_mask = str_to_mask(outstr);
	write_log(10, "Cap mask is %" PRIu64 "\n", cap_mask);

	/* Check the chown bit in the bitmask */

	op_mask = 1;
	op_mask = op_mask << cap_to_check;
	result_mask = cap_mask & op_mask;

	/* Return whether the chown is set */
	if (result_mask != 0)
		return TRUE;
	return FALSE;
}

/************************************************************************
*
* Function name: hfuse_ll_setattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, struct stat *attr,
*                int32_t to_set, struct fuse_file_info *fi
*       Summary: Set attribute for a filesystem object. This includes
*                routines such as chmod, chown, truncate, utimens.
*
*************************************************************************/
void hfuse_ll_setattr(fuse_req_t req,
		      fuse_ino_t ino,
		      struct stat *attr, /* Keep for Fuse low level API */
		      int32_t to_set,
		      struct fuse_file_info *file_info)
{
	int32_t ret_val;
	ino_t this_inode, org_ino;
	BOOL attr_changed, only_atime_changed;
	struct timespec timenow;
	HCFS_STAT newstat;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	struct fuse_ctx *temp_context;
	struct stat retstat; /* fuse reply */
	FH_ENTRY *fh_ptr;

	write_log(10, "Debug setattr, to_set %d\n", to_set);

	org_ino = org_real_ino(req, ino);
	this_inode = real_ino(req, ino);

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	attr_changed = FALSE;
	only_atime_changed = TRUE;

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL) {
		fuse_reply_err(req, errno);
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
			(newstat.size != attr->st_size)) {
		/* If opened file, check file table first */
		if (file_info != NULL) {
			if (system_fh_table.entry_table_flags[file_info->fh]
			    != IS_FH)
				goto continue_check;

			fh_ptr = &(system_fh_table.entry_table[file_info->fh]);
			if (fh_ptr->thisinode != (ino_t) this_inode)
				goto continue_check;
			if ((!((fh_ptr->flags & O_ACCMODE) == O_WRONLY)) &&
			    (!((fh_ptr->flags & O_ACCMODE) == O_RDWR)))
				goto continue_check;
			goto allow_truncate;
		}

continue_check:
		/* Checking permission */
		ret_val = check_permission(req, &newstat, 2, FALSE);

		if (ret_val < 0) {
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, -ret_val);
			return;
		}

allow_truncate:
		/* Now will update meta seq only if truncated */
		ret_val = update_meta_seq(body_ptr);
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
		only_atime_changed = FALSE;
		write_log(10, "Truncated\n");
	}

	if ((to_set & FUSE_SET_ATTR_MODE) &&
	    (newstat.mode != attr->st_mode)) {
		write_log(10, "Debug setattr context %d, file %d\n",
			temp_context->uid, newstat.uid);

		if ((_check_capability(temp_context->pid, CAP_FOWNER) != TRUE) &&
			(temp_context->uid != newstat.uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.mode = attr->st_mode;
		attr_changed = TRUE;
		only_atime_changed = FALSE;
		write_log(10, "Mode changed\n");
	}

	if ((to_set & FUSE_SET_ATTR_UID) &&
	    (newstat.uid != attr->st_uid)) {
		/* Checks if process has CHOWN capabilities here */
		if (_check_capability(temp_context->pid,
			CAP_CHOWN) != TRUE) {
			/* Not privileged */
			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.uid = attr->st_uid;
		attr_changed = TRUE;
		only_atime_changed = FALSE;
		write_log(10, "uid changed\n");
	}

	if ((to_set & FUSE_SET_ATTR_GID) &&
	    (newstat.gid != attr->st_gid)) {
		/* Checks if process has CHOWN capabilities here */
		/* Or if is owner or in group */
		if (_check_capability(temp_context->pid,
			CAP_CHOWN) != TRUE) {
			ret_val = is_member(req, newstat.gid, attr->st_gid);
			if (ret_val < 0) {
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, -ret_val);
				return;
			}
			if ((temp_context->uid != newstat.uid) ||
				(ret_val == 0)) {
				/* Not privileged and (not owner or
				not in group) */
				meta_cache_close_file(body_ptr);
				meta_cache_unlock_entry(body_ptr);
				fuse_reply_err(req, EPERM);
				return;
			}
		}

		newstat.gid = attr->st_gid;
		attr_changed = TRUE;
		only_atime_changed = FALSE;
		write_log(10, "gid changed\n");
	}

	if ((to_set & FUSE_SET_ATTR_ATIME) &&
	    (newstat.atime != attr->st_atime)) {
		if ((_check_capability(temp_context->pid, CAP_FOWNER) != TRUE) &&
			(temp_context->uid != newstat.uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}

		newstat.atime = attr->st_atime;
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.atime), &(attr->st_atime),
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	if ((to_set & FUSE_SET_ATTR_MTIME) &&
	    (newstat.mtime != attr->st_mtime)) {
		if ((_check_capability(temp_context->pid, CAP_FOWNER) != TRUE) &&
			(temp_context->uid != newstat.uid)) {
			/* Not privileged and not owner */

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EPERM);
			return;
		}
		newstat.mtime = attr->st_mtime;
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.mtime), &(attr->st_mtime),
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
		only_atime_changed = FALSE;
		write_log(10, "mtime changed\n");
	}

	clock_gettime(CLOCK_REALTIME, &timenow);

	if (to_set & FUSE_SET_ATTR_ATIME_NOW) {
		if ((_check_capability(temp_context->pid, CAP_FOWNER) != TRUE) &&
			((temp_context->uid != newstat.uid) ||
				(check_permission(req, &newstat, 2, FALSE) < 0))) {
			/* Not privileged and
				(not owner or no write permission)*/

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EACCES);
			return;
		}
		newstat.atime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.atime), &timenow,
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
	}

	if (to_set & FUSE_SET_ATTR_MTIME_NOW) {
		if ((_check_capability(temp_context->pid, CAP_FOWNER) != TRUE) &&
			((temp_context->uid != newstat.uid) ||
				(check_permission(req, &newstat, 2, FALSE) < 0))) {
			/* Not privileged and
				(not owner or no write permission)*/

			meta_cache_close_file(body_ptr);
			meta_cache_unlock_entry(body_ptr);
			fuse_reply_err(req, EACCES);
			return;
		}
		newstat.mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.mtime), &timenow,
			sizeof(struct timespec));
#endif
		attr_changed = TRUE;
		only_atime_changed = FALSE;
		write_log(10, "mtime changed to now\n");
	}

	if (attr_changed == TRUE) {
		newstat.ctime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.ctime), &timenow,
			sizeof(struct timespec));
#endif
		if (to_set & FUSE_SET_ATTR_SIZE) {
			newstat.mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
			memcpy(&(newstat.mtime), &timenow,
				sizeof(struct timespec));
#endif
		}
	}

	if (only_atime_changed == TRUE)
		write_log(10, "Only access time changed\n");
	else
		write_log(10, "Some other stat changed also\n");

	/* If only access time is changed (other than ctime), do not sync */
	if (only_atime_changed == TRUE)
		ret_val = meta_cache_update_stat_nosync(this_inode, &newstat,
				body_ptr);
	else
		ret_val = meta_cache_update_file_data(this_inode, &newstat,
				NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	newstat.ino = org_ino;
	convert_hcfsstat_to_sysstat(&retstat, &newstat);
	fuse_reply_attr(req, &retstat, 0);
}

/************************************************************************
*
* Function name: hfuse_ll_access
*        Inputs: fuse_req_t req, fuse_ino_t ino, int32_t mode
*       Summary: Checks the permission for object "ino" against "mode".
*
*************************************************************************/
static void hfuse_ll_access(fuse_req_t req, fuse_ino_t ino, int32_t mode)
{
	HCFS_STAT thisstat;
	int32_t ret_val;
	ino_t thisinode;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	thisinode = real_ino(req, ino);

	ret_val = fetch_inode_stat(thisinode, &thisstat, NULL, NULL);

	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}


#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &thisstat, NULL, NULL);
	}
#endif

	if (mode == F_OK) {
		fuse_reply_err(req, 0);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &thisstat, mode, TRUE);

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
	int32_t amount;
	int32_t current_val;
	char to_delete;
	char d_type;
	ino_t thisinode, org_ino;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

	org_ino = org_real_ino(req, ino);
	thisinode = real_ino(req, ino);

	gettimeofday(&last_forget_time, NULL);

#ifdef _ANDROID_ENV_
	if (IS_ALIAS_INODE(org_ino)) {
		delete_in_alias_group(org_ino);
	}
#endif

	amount = (int32_t) nlookup;

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

	if ((current_val == 0) && (to_delete == TRUE)) {
#ifdef _ANDROID_ENV_
		delete_in_alias_group(thisinode);
#endif
		actual_delete_inode(thisinode, d_type, tmpptr->f_ino, tmpptr);
	}

	// umount
	if (ino == 1) {
		hcfs_system->last_umount_time = last_forget_time;
		pthread_mutex_lock(&(hcfs_system->immediate_sync_meta_mutex));
		pthread_cond_broadcast(&(hcfs_system->immediate_sync_meta_cond));
		pthread_mutex_unlock(&(hcfs_system->immediate_sync_meta_mutex));
	}

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
static int32_t symlink_internal(fuse_req_t req, const char *link,
	fuse_ino_t parent, const char *name, struct fuse_entry_param *tmp_param)
{
	ino_t parent_inode;
	ino_t self_inode;
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry;
	DIR_ENTRY_PAGE dir_page;
	uint64_t this_generation;
	struct fuse_ctx *temp_context;
	HCFS_STAT parent_stat;
	HCFS_STAT this_stat;
	int32_t ret_val;
	int32_t result_index;
	int32_t errcode;
	MOUNT_T *tmpptr;
	char local_pin;
	int64_t delta_meta_size;
	BOOL is_external = FALSE;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		return ENOTSUP;
	}
#endif

	/* Reject if no more meta space */
	if (NO_META_SPACE()) {
		return ENOSPC;
	}

	parent_inode = real_ino(req, parent);

	/* Reject if name too long */
	if (strlen(name) > MAX_FILENAME_LEN) {
		write_log(0, "File name is too long\n");
		return ENAMETOOLONG;
	}
	if (strlen(name) <= 0) {
		return EINVAL;
	}

	/* Reject if link path too long */
	if (strlen(link) >= MAX_LINK_PATH) {
		write_log(0, "Link path is too long\n");
		return ENAMETOOLONG;
	}
	if (strlen(link) <= 0) {
		return EINVAL;
	}

	ret_val = fetch_inode_stat(parent_inode, &parent_stat,
			NULL, &local_pin);
	if (ret_val < 0) {
		return -ret_val;
	}

	/* Error if parent is not a dir */
	if (!S_ISDIR(parent_stat.mode)) {
		return ENOTDIR;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);
	if (ret_val < 0) {
		return -ret_val;
	}

	/* Check whether "name" exists or not */
	parent_meta_cache_entry = meta_cache_lock_entry(parent_inode);
	if (!parent_meta_cache_entry) {
		return errno;
	}
	ret_val = meta_cache_seek_dir_entry(parent_inode, &dir_page,
		&result_index, name, parent_meta_cache_entry,
		is_external);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}
	if (result_index >= 0) {
		write_log(0, "File %s existed\n", name);
		errcode = -EEXIST;
		goto error_handle;
	}

#ifdef _ANDROID_ENV_
	/* Android will create a symlink "/data/data/<pkg>/lib" linked
	 * to /data/app/lib when system rebooted. This should be marked
	 * as high-priority-pin.
	 */
	if (strcmp(tmpptr->f_name, DATA_VOL_NAME) == 0 &&
	    strcmp(name, "lib") == 0)
		local_pin = P_HIGH_PRI_PIN;
#endif

	/* Prepare stat and request a new inode from superblock */
	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		write_log(0, "Memory allocation error in symlink()\n");
		errcode = -ENOMEM;
		goto error_handle;
	}

	init_hcfs_stat(&this_stat);

	this_stat.nlink = 1;
	this_stat.size = strlen(link);

	this_stat.mode = S_IFLNK | 0777;

	this_stat.uid = temp_context->uid;
	this_stat.gid = temp_context->gid;
	set_timestamp_now(&this_stat, A_TIME | M_TIME | C_TIME);

	self_inode = super_block_new_inode(&this_stat, &this_generation,
			local_pin);
	if (self_inode < 1) {
		errcode = -ENOSPC;
		goto error_handle;
	}

	this_stat.ino = self_inode;

	/* Write symlink meta and add new entry to parent */
	ret_val = symlink_update_meta(parent_meta_cache_entry, &this_stat,
			link, this_generation, name, tmpptr,
			&delta_meta_size, local_pin, is_external);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		errcode = ret_val;
		goto error_handle;
	}

	ret_val = update_meta_seq(parent_meta_cache_entry);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}

	ret_val = meta_cache_close_file(parent_meta_cache_entry);
	if (ret_val < 0) {
		meta_cache_unlock_entry(parent_meta_cache_entry);
		return -ret_val;
	}
	ret_val = meta_cache_unlock_entry(parent_meta_cache_entry);
	if (ret_val < 0) {
		return -ret_val;
	}

	/* Reply fuse entry */
	memset(tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param->generation = this_generation;
	tmp_param->ino = (fuse_ino_t) self_inode;
	convert_hcfsstat_to_sysstat(&(tmp_param->attr), &this_stat);

	ret_val = lookup_increase(tmpptr->lookup_table, self_inode,
				1, D_ISLNK);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		return -ret_val;
	}

	if (delta_meta_size != 0)
		change_system_meta(delta_meta_size, 0, 0, 0, 0, 0, TRUE);
	ret_val = change_mount_stat(tmpptr, 0, delta_meta_size, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		return -ret_val;
	}

	write_log(5, "Debug symlink: symlink operation success\n");
	return 0;

error_handle:
	meta_cache_close_file(parent_meta_cache_entry);
	meta_cache_unlock_entry(parent_meta_cache_entry);
	return -errcode;
}

static void hfuse_ll_symlink(fuse_req_t req, const char *link,
	fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param tmp_param;
	int32_t ret_val;

	ret_val = symlink_internal(req, link, parent, name,
	                           &tmp_param);

	if (ret_val != 0)
		fuse_reply_err(req, ret_val);
	else
		fuse_reply_entry(req, &(tmp_param));
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
	HCFS_STAT symlink_stat;
	char link_buffer[MAX_LINK_PATH + 1];
	int32_t ret_code;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	this_inode = real_ino(req, ino);

#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		fuse_reply_err(req, ENOTSUP);
		return;
	}
#endif

	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		ret_code = errno;
		write_log(0, "readlink() lock meta cache entry fail\n");
		fuse_reply_err(req, ret_code);
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

	/* Update access time but do not sync to backend */
	set_timestamp_now(&symlink_stat, A_TIME);
	ret_code = meta_cache_update_stat_nosync(this_inode, &symlink_stat,
		meta_cache_entry);
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

#ifndef FUSE_NOXATTR
/**
 * Check xattr R/W permission.
 *
 * @return 0 on allowing this action. Reject action when return
 *           negative error code.
 */
static int32_t _xattr_permission(char name_space,
				 pid_t thispid,
				 fuse_req_t req,
				 const HCFS_STAT *thisstat,
				 char ops)
{
	switch (name_space) {
	case SYSTEM:
	case USER:
		if (ops == WRITE_XATTR)
			return check_permission(req, thisstat, 2, FALSE);
		if (ops == READ_XATTR)
			return check_permission(req, thisstat, 4, FALSE);
		break;
	case SECURITY:
		return 0;
		break;
	case TRUSTED:
		if (_check_capability(thispid,
				CAP_SYS_ADMIN) == TRUE)
			return 0;
		else
			return -EACCES;
		break;
	default:
		break;
	}

	return -EINVAL;
}
#endif /* FUSE_NOXATTR */

#ifndef FUSE_NOXATTR
/************************************************************************
*
* Function name: hfuse_ll_setxattr
*        Inputs: fuse_req_t req, fuse_ino_t ino, char *name, char *value,
*                size_t size, int32_t flag
*       Summary: Set extended attribute when given (name, value) pair. It
*                creates new attribute when name is not found. On the
*                other hand, if attribute exists, it replaces the old
*                value with new value.
*
*************************************************************************/
static void hfuse_ll_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	const char *value, size_t size, int32_t flag)
{
	META_CACHE_ENTRY_STRUCT *meta_cache_entry;
	XATTR_PAGE *xattr_page;
	int64_t xattr_filepos;
	char key[MAX_KEY_SIZE];
	char name_space;
	int32_t retcode;
	HCFS_STAT stat_data;
	ino_t this_inode;
	MOUNT_T *tmpptr;
	struct fuse_ctx *temp_context;
	int64_t old_metasize, new_metasize, delta_meta_size;
	int64_t old_metasize_blk, new_metasize_blk, delta_meta_size_blk;
	FILE_META_TYPE this_filemeta;

	tmpptr = (MOUNT_T *)fuse_req_userdata(req);

	this_inode = real_ino(req, ino);
	xattr_page = NULL;
	old_metasize = 0;
	new_metasize = 0;

	if ((flag != XATTR_REPLACE) && NO_META_SPACE()) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}
	write_log(10, "Debug setxattr: namespace = %d, key = %s, flag = %d\n",
		name_space, key, flag);

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type) &&
	    name_space == SECURITY &&
	    strncmp(key, SELINUX_XATTR_KEY, sizeof(SELINUX_XATTR_KEY)) == 0) {
		fuse_reply_err(req, 0);
		return;
	}
#endif

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		retcode = errno;
		write_log(0, "Error: setxattr lock_entry fail\n");
		fuse_reply_err(req, retcode);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	if (name_space == SECURITY)
		goto fetch_xattr; /* Skip perm check if SECURITY domain */

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		&this_filemeta, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &stat_data, NULL, NULL);
	}
#endif

	temp_context = (struct fuse_ctx *)fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	if (_xattr_permission(name_space, temp_context->pid, req, &stat_data,
			WRITE_XATTR) < 0) {
		write_log(5, "Error: setxattr Permission denied ");
		write_log(5, "(WRITE needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

fetch_xattr:
	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}

	meta_cache_get_meta_size(meta_cache_entry, &old_metasize,
			&old_metasize_blk);

	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos, TRUE);
	if (retcode < 0)
		goto error_handle;
	write_log(10, "Debug setxattr: fetch xattr_page, xattr_page = %lld\n",
		xattr_filepos);

	/* Begin to Insert xattr */
	retcode = insert_xattr(meta_cache_entry, xattr_page, xattr_filepos,
		name_space, key, value, size, flag);
	if (retcode < 0)
		goto error_handle;

	meta_cache_get_meta_size(meta_cache_entry, &new_metasize,
			&new_metasize_blk);

	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);

	if (new_metasize > 0 && old_metasize > 0) {
		delta_meta_size = new_metasize - old_metasize;
		delta_meta_size_blk = new_metasize_blk - old_metasize_blk;
	} else {
		delta_meta_size = 0;
		delta_meta_size_blk = 0;
	}

	if (delta_meta_size != 0) {
		change_system_meta(delta_meta_size, delta_meta_size_blk,
				0, 0, 0, 0, TRUE);
		change_mount_stat(tmpptr, 0, delta_meta_size, 0);
	}

	fuse_reply_err(req, 0);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	fuse_reply_err(req, -retcode);
}
#endif  /* FUSE_NOXATTR */

#ifndef FUSE_NOXATTR
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
	HCFS_STAT stat_data;
	int64_t xattr_filepos;
	char key[MAX_KEY_SIZE];
	char name_space;
	int32_t retcode;
	ino_t this_inode;
	size_t actual_size;
	char *value;
	struct fuse_ctx *temp_context;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
#endif

	write_log(10, "hfuse_ll_getxattr %d\n", __LINE__);
	this_inode = real_ino(req, ino);
	value = NULL;
	xattr_page = NULL;
	actual_size = 0;

	write_log(10, "hfuse_ll_getxattr %d\n", __LINE__);
	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}
	write_log(10, "Debug getxattr: namespace = %d, key = %s, size = %d\n",
		name_space, key, size);

#ifdef _ANDROID_ENV_
	/* If get xattr of security.selinux in external volume, return the
	   adhoc value. */
	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type) &&
	    name_space == SECURITY &&
	    strncmp(key, SELINUX_XATTR_KEY, strlen(SELINUX_XATTR_KEY)) == 0) {
		actual_size = strlen(SELINUX_EXTERNAL_XATTR_VAL);

		if (size <= 0)
			fuse_reply_xattr(req, actual_size);

		else if (size < actual_size)
			fuse_reply_err(req, ERANGE);

		else
			fuse_reply_buf(req, SELINUX_EXTERNAL_XATTR_VAL,
				actual_size);
		return;
	}
#endif

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		retcode = errno;
		write_log(0, "Error: getxattr lock_entry fail\n");
		fuse_reply_err(req, retcode);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	if (name_space == SECURITY)
		goto fetch_xattr; /* Skip perm check if SECURITY domain */

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

#ifdef _ANDROID_ENV_
	tmpptr = (MOUNT_T *)fuse_req_userdata(req);
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &stat_data, NULL, NULL);
	}
#endif

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	if (_xattr_permission(name_space, temp_context->pid, req, &stat_data,
			READ_XATTR) < 0) {
		write_log(5, "Error: getxattr permission denied ");
		write_log(5, "(READ needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

fetch_xattr:
	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos, FALSE);
	if (retcode < 0) {
		if (retcode == -ENOENT) {
			write_log(10, "Debug: xattr page was not created\n");
			meta_cache_close_file(meta_cache_entry);
			meta_cache_unlock_entry(meta_cache_entry);
			free(xattr_page);
			fuse_reply_err(req, ENODATA);
			return;
		} else {
			goto error_handle;
		}
	}

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
#endif /* FUSE_NOXATTR */

#ifndef FUSE_NOXATTR
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
	int64_t xattr_filepos;
	int32_t retcode;
	char *key_buf;
	size_t actual_size;
	int32_t sel_keysize;
	char sel_key[200];
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);
	this_inode = real_ino(req, ino);
	key_buf = NULL;
	xattr_page = NULL;
	actual_size = 0;
	write_log(10,
		  "Debug listxattr: Begin listxattr, given buffer size = %d\n",
		  size);

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		retcode = errno;
		write_log(0, "Error: listxattr lock_entry fail\n");
		fuse_reply_err(req, retcode);
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
		&xattr_filepos, FALSE);
	if (retcode < 0) {
		if (retcode == -ENOENT) {
			meta_cache_close_file(meta_cache_entry);
			meta_cache_unlock_entry(meta_cache_entry);
			free(xattr_page);
#ifdef _ANDROID_ENV_
			if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
				sprintf(sel_key, "security.%s",
						SELINUX_XATTR_KEY);
				actual_size = strlen(sel_key) + 1;
				if (size <= 0)
					fuse_reply_xattr(req, actual_size);
				else if (size < actual_size)
					fuse_reply_err(req, ERANGE);
				else
					fuse_reply_buf(req, sel_key,
						actual_size);
				return;
			}
#endif
			fuse_reply_xattr(req, 0);
			return;
		} else {
			goto error_handle;
		}
	}

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
#ifdef _ANDROID_ENV_
		if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
			sprintf(sel_key, "security.%s", SELINUX_XATTR_KEY);
			actual_size += (strlen(sel_key) + 1);
		}
#endif
		write_log(5, "listxattr: Reply needed size = %d\n",
			actual_size);
		fuse_reply_xattr(req, actual_size);

	} else { /* Reply list */
#ifdef _ANDROID_ENV_
		if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
			sprintf(sel_key, "security.%s", SELINUX_XATTR_KEY);
			sel_keysize = strlen(sel_key) + 1; /* null char */
			if (size < actual_size + sel_keysize) {
				fuse_reply_err(req, ERANGE);
				if (xattr_page)
					free(xattr_page);
				if (key_buf)
					free(key_buf);
				return;
			} else {
				memcpy(key_buf + actual_size, sel_key,
					sel_keysize);
				actual_size += sel_keysize;
			}
		}
#endif
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
#endif /* FUSE_NOXATTR */

#ifndef FUSE_NOXATTR
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
	int64_t xattr_filepos;
	int32_t retcode;
	char name_space;
	char key[MAX_KEY_SIZE];
	HCFS_STAT stat_data;
	struct fuse_ctx *temp_context;
#ifdef _ANDROID_ENV_
	MOUNT_T *tmpptr;
	tmpptr = (MOUNT_T *)fuse_req_userdata(req);
#endif

	this_inode = real_ino(req, ino);
	xattr_page = NULL;

	/* Parse input name and separate it into namespace and key */
	retcode = parse_xattr_namespace(name, &name_space, key);
	if (retcode < 0) {
		fuse_reply_err(req, -retcode);
		return;
	}
	write_log(10, "Debug removexattr: namespace = %d, key = %s\n",
		name_space, key);

#ifdef _ANDROID_ENV_
	/* If remove security.selinux under sd_card, just return */
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type) &&
	    name_space == SECURITY &&
	    strncmp(key, SELINUX_XATTR_KEY, strlen(SELINUX_XATTR_KEY)) == 0) {
		fuse_reply_err(req, 0);
		return;
	}
#endif

	/* Lock the meta cache entry and use it to find pos of xattr page */
	meta_cache_entry = meta_cache_lock_entry(this_inode);
	if (meta_cache_entry == NULL) {
		retcode = errno;
		write_log(0, "Error: removexattr lock_entry fail\n");
		fuse_reply_err(req, retcode);
		return;
	}

	/* Open the meta file and set exclusive lock to it */
	retcode = meta_cache_open_file(meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

	if (name_space == SECURITY)
		goto fetch_xattr;  /* Skip perm check if SECURITY domain */

	/* Check permission */
	retcode = meta_cache_lookup_file_data(this_inode, &stat_data,
		NULL, NULL, 0, meta_cache_entry);
	if (retcode < 0)
		goto error_handle;

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &stat_data, NULL, NULL);
	}
#endif

	temp_context = (struct fuse_ctx *)fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	if (_xattr_permission(name_space, temp_context->pid, req, &stat_data,
			WRITE_XATTR) < 0) {
		write_log(
		    5, "Error: removexattr Permission denied (WRITE needed)\n");
		retcode = -EACCES;
		goto error_handle;
	}

fetch_xattr:
	/* Fetch xattr page. Allocate new page if need. */
	xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
	if (!xattr_page) {
		write_log(0, "Error: Allocate memory error\n");
		retcode = -ENOMEM;
		goto error_handle;
	}
	retcode = fetch_xattr_page(meta_cache_entry, xattr_page,
		&xattr_filepos, FALSE);
	if (retcode < 0) {
		if (retcode == -ENOENT) {
			meta_cache_close_file(meta_cache_entry);
			meta_cache_unlock_entry(meta_cache_entry);
			free(xattr_page);
			fuse_reply_err(req, ENODATA);
			return;
		} else {
			goto error_handle;
		}
	}

	/* Remove xattr */
	retcode = remove_xattr(meta_cache_entry, xattr_page, xattr_filepos,
		name_space, key);
	if (retcode < 0 && retcode != -ENODATA) { /* ENOENT or others */
		write_log(5, "Error: removexattr remove xattr fail. Code %d\n",
				-retcode);
		goto error_handle;
	}

	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	fuse_reply_err(req, 0);
	return;

error_handle:
	meta_cache_close_file(meta_cache_entry);
	meta_cache_unlock_entry(meta_cache_entry);
	if (xattr_page)
		free(xattr_page);
	fuse_reply_err(req, -retcode);
}
#endif /* FUSE_NOXATTR */

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
	HCFS_STAT parent_stat, link_stat;
	struct fuse_entry_param tmp_param;
	int32_t result_index;
	int32_t ret_val, errcode;
	uint64_t this_generation;
	ino_t parent_inode, link_inode;
	MOUNT_T *tmpptr;
	int64_t old_metasize, new_metasize, delta_meta_size;
	int64_t old_metasize_blk, new_metasize_blk, delta_meta_size_blk;
	char local_pin;
	BOOL is_external = FALSE;

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
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

	ret_val = fetch_inode_stat(parent_inode, &parent_stat, NULL,
				   &local_pin);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Error if parent is not a dir */
	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE); /* W+X */
	if (ret_val < 0) {
		write_log(0, "Dir permission denied. W+X is needed\n");
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Check whether "newname" exists or not */
	parent_meta_cache_entry = meta_cache_lock_entry(parent_inode);
	if (!parent_meta_cache_entry) {
		fuse_reply_err(req, errno);
		return;
	}

	meta_cache_get_meta_size(parent_meta_cache_entry, &old_metasize,
			&old_metasize_blk);

	ret_val = meta_cache_seek_dir_entry(parent_inode, &dir_page,
		&result_index, newname, parent_meta_cache_entry,
		is_external);
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
		&this_generation, parent_meta_cache_entry,
		is_external);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}

	ret_val = update_meta_seq(parent_meta_cache_entry);
	if (ret_val < 0) {
		errcode = ret_val;
		goto error_handle;
	}
	meta_cache_get_meta_size(parent_meta_cache_entry, &new_metasize,
			&new_metasize_blk);
	delta_meta_size = new_metasize - old_metasize;
	delta_meta_size_blk = new_metasize_blk - old_metasize_blk;
	if (new_metasize > 0 && old_metasize > 0 && delta_meta_size != 0) {
		change_system_meta(delta_meta_size, delta_meta_size_blk,
				0, 0, 0, 0, TRUE);
		change_mount_stat(tmpptr, 0, delta_meta_size, 0);
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
	convert_hcfsstat_to_sysstat(&(tmp_param.attr), &link_stat);
	if (S_ISFILE(link_stat.mode))
		ret_val = lookup_increase(tmpptr->lookup_table,
			link_inode, 1, D_ISREG);
	else if (S_ISLNK(link_stat.mode))
		ret_val = lookup_increase(tmpptr->lookup_table,
			link_inode, 1, D_ISLNK);
	else if (S_ISDIR(link_stat.mode))
		ret_val = -EISDIR;
	else
		ret_val = -EPERM;

	if (ret_val < 0) {
		write_log(0, "Fail to increase lookup count\n");
		fuse_reply_err(req, -ret_val);
		return;
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
	int32_t ret_val;
	HCFS_STAT parent_stat, this_stat;
	ino_t parent_inode, self_inode;
	mode_t self_mode;
	int32_t file_flags;
	struct fuse_ctx *temp_context;
	struct fuse_entry_param tmp_param;
	uint64_t this_generation;
	int64_t fh;
	MOUNT_T *tmpptr;
	char local_pin;
	PIN_t pin;
	int64_t delta_meta_size;
	BOOL is_external = FALSE;

	parent_inode = real_ino(req, parent);

	write_log(10, "DEBUG parent %" PRIu64 ", name %s mode %d\n",
			(uint64_t)parent_inode, name, mode);

	if (NO_META_SPACE()) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

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
	ret_val = fetch_inode_stat(parent_inode, &parent_stat,
			NULL, &local_pin);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}

	tmpptr = (MOUNT_T *) fuse_req_userdata(req);

#ifdef _ANDROID_ENV_
	pin = P_INVALID;
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &parent_stat, NULL, &pin);
		is_external = TRUE;
	}
	/* Inherit parent pin status if "pin" is not modified */
	if (pin == P_INVALID)
		pin = local_pin;
#else
	/* Default inherit parent's pin status */
	pin = local_pin;
#endif

	if (!S_ISDIR(parent_stat.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	/* Checking permission */
	ret_val = check_permission(req, &parent_stat, 3, FALSE);
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

	init_hcfs_stat(&this_stat);
	this_stat.dev = 0;
	this_stat.size = 0;
	this_stat.blksize = ST_BLKSIZE;
	this_stat.blocks = 0;
	this_stat.nlink = 1;
	self_mode = mode | S_IFREG;
	this_stat.mode = self_mode;

	/*Use the uid and gid of the fuse caller*/
	this_stat.uid = temp_context->uid;
	this_stat.gid = temp_context->gid;

	/* Use the current time for timestamps */
	set_timestamp_now(&this_stat, A_TIME | M_TIME | C_TIME);
	self_inode = super_block_new_inode(&this_stat, &this_generation,
			pin);
	/* If cannot get new inode number, error is ENOSPC */
	if (self_inode < 1) {
		fuse_reply_err(req, ENOSPC);
		return;
	}

	this_stat.ino = self_inode;
	ret_val = mknod_update_meta(self_inode, parent_inode, name,
			&this_stat, this_generation, tmpptr,
			&delta_meta_size, pin, is_external);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* Prepare stat data to be replied */

	memset(&tmp_param, 0, sizeof(struct fuse_entry_param));
	tmp_param.generation = this_generation;
	tmp_param.ino = (fuse_ino_t) self_inode;

#ifdef _ANDROID_ENV_
	if (IS_ANDROID_EXTERNAL(tmpptr->volume_type)) {
		if (tmpptr->vol_path_cache == NULL) {
			fuse_reply_err(req, EIO);
			return;
		}
		_rewrite_stat(tmpptr, &this_stat, NULL, NULL);
	}
#endif

	convert_hcfsstat_to_sysstat(&(tmp_param.attr), &this_stat);

	if (delta_meta_size != 0)
		change_system_meta(delta_meta_size, 0, 0, 0, 0, 0, TRUE);
	ret_val = change_mount_stat(tmpptr, 0, delta_meta_size, 1);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	ret_val = lookup_increase(tmpptr->lookup_table, self_inode, 1, D_ISREG);
	if (ret_val < 0) {
		meta_forget_inode(self_inode);
		fuse_reply_err(req, -ret_val);
		return;
	}

	/* In create operation, flag is O_WRONLY when opening */
	file_flags = fi->flags;
	fh = open_fh(self_inode, file_flags, FALSE);
	if (fh < 0) {
		fuse_reply_err(req, ENFILE);
		return;
	}
	fi->fh = fh;

	fuse_reply_create(req, &tmp_param, fi);
}


/* TODO:
	if mode = 0, default op.
	if mode = 1, keep filesize.
	if mode = 3, punch hole and keep size.
	otherwise return ENOTSUPP
	If mode 0 or 1, don't need to zero regions already containing data
	If mode = 3, need to zero regions.
*/
static void hfuse_ll_fallocate(fuse_req_t req, fuse_ino_t ino, int32_t mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int32_t ret_val;
	ino_t this_inode;
	struct timespec timenow;
	HCFS_STAT newstat;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	struct fuse_ctx *temp_context;
	off_t old_file_size;

	UNUSED(fi);

	write_log(10, "Debug fallocate, mode %d, off %lld, len %lld\n",
	          mode, offset, length);

	if ((offset < 0) || (length <= 0)) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	this_inode = real_ino(req, ino);

	temp_context = (struct fuse_ctx *) fuse_req_ctx(req);
	if (temp_context == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	body_ptr = meta_cache_lock_entry(this_inode);
	if (body_ptr == NULL) {
		fuse_reply_err(req, errno);
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

	/* Checking permission */
	ret_val = check_permission(req, &newstat, 2, FALSE);

	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		fuse_reply_err(req, -ret_val);
		return;
	}

	old_file_size = newstat.size;
	ret_val = do_fallocate(this_inode, &newstat, mode, offset, length,
			       &body_ptr, req);
	if (ret_val < 0) {
		meta_cache_close_file(body_ptr);
		meta_cache_unlock_entry(body_ptr);
		fuse_reply_err(req, -ret_val);
		return;
	}

	clock_gettime(CLOCK_REALTIME, &timenow);

	if ((mode == 3) || (old_file_size != newstat.size)) {
		newstat.ctime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.ctime), &timenow,
			sizeof(struct timespec));
#endif
		newstat.mtime = (time_t)(timenow.tv_sec);
#ifndef _ANDROID_ENV_
		memcpy(&(newstat.mtime), &timenow,
			sizeof(struct timespec));
#endif
	}

	ret_val = meta_cache_update_file_data(this_inode, &newstat,
			NULL, NULL, 0, body_ptr);
	meta_cache_close_file(body_ptr);
	meta_cache_unlock_entry(body_ptr);
	if (ret_val < 0) {
		fuse_reply_err(req, -ret_val);
		return;
	}
	fuse_reply_err(req, 0);
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
#ifndef FUSE_NOXATTR
	.setxattr = hfuse_ll_setxattr,
	.getxattr = hfuse_ll_getxattr,
	.listxattr = hfuse_ll_listxattr,
	.removexattr = hfuse_ll_removexattr,
#endif
	.link = hfuse_ll_link,
	.create = hfuse_ll_create,
	.fallocate = hfuse_ll_fallocate,
};

/* Initiate FUSE */
void *mount_multi_thread(void *ptr)
{
	struct fuse_session *session_ptr;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) ptr;

	session_ptr = tmpptr->session_ptr;
	fuse_session_loop_mt(session_ptr);
	write_log(10, "Unmounting FS with root inode %" PRIu64 ", %d\n",
			(uint64_t)tmpptr->f_ino, tmpptr->is_unmount);

	if (tmpptr->is_unmount == FALSE)
		unmount_event(tmpptr->f_name, tmpptr->f_mp);

	return 0;
}
void *mount_single_thread(void *ptr)
{
	struct fuse_session *session_ptr;
	MOUNT_T *tmpptr;

	tmpptr = (MOUNT_T *) ptr;

	session_ptr = tmpptr->session_ptr;
	fuse_session_loop(session_ptr);
	write_log(10, "Unmounting FS with root inode %" PRIu64 ", %d\n",
			(uint64_t)tmpptr->f_ino, tmpptr->is_unmount);

	if (tmpptr->is_unmount == FALSE)
		unmount_event(tmpptr->f_name, tmpptr->f_mp);

	return 0;
}

void _unlink_restore_stat(void)
{
	char restore_stat_path[METAPATHLEN];

	sem_wait(&(restore_sem));
	fetch_restore_stat_path(restore_stat_path);
	unlink(restore_stat_path);
	sem_post(&(restore_sem));
}

int32_t hook_fuse(int32_t argc, char **argv)
{
	int32_t dl_count;
#ifndef _ANDROID_ENV_
	pthread_t communicate_tid[MAX_FUSE_COMMUNICATION_THREAD];
	int32_t socket_fd;
#endif

#ifdef _FORCE_FUSE_DEBUG_  /* If want to dump debug log from fuse lib */
	int32_t count;

	global_argc = argc + 1;
	global_argv = (char **) malloc(sizeof(char *) * global_argc);
	global_argv[argc] = (char *) malloc(10);
	snprintf(global_argv[argc], 10, "-d");
	for (count = 0; count < argc; count++)
		global_argv[count] = argv[count];
#else
	global_argc = argc;
	global_argv = argv;
#endif
	data_data_root = (ino_t) 0;
	data_smart_root = (ino_t) 0;
	mgmt_app_is_created = FALSE;

#ifndef _ANDROID_ENV_ /* Not in Android */
	init_fuse_proc_communication(communicate_tid, &socket_fd);
#endif
	/* Init meta cache before api interface being setup */
	init_meta_cache_headers();
	init_api_interface();
	init_alias_group();

	startup_finish_delete();
	init_download_control();
	init_pin_scheduler();

	/* Check and cleanup meta / data no longer in use from content
	before restoration */
	cleanup_stage1_data();

	/* Wait on the fuse semaphore, until waked up by api_interface */
	while (!hcfs_system->system_going_down) {
		/* Wait */
		sem_wait(&(hcfs_system->fuse_sem));

		/* Check if restoring completed. */
		sem_wait(&(hcfs_system->access_sem));
		if (hcfs_system->system_restoring == RESTORING_STAGE2) {
			if (rebuild_sb_jobs->job_finish) {
				hcfs_system->system_restoring = NOT_RESTORING;
				destroy_rebuild_sb(TRUE);
				/* Remove tag for rebuilding */
				_unlink_restore_stat();
				/* Notify user that the restoration is done */
				notify_restoration_result(2, 0);

				/* Enable backend related services */
				init_backend_related_module();
				write_log(10, "Debug: Finish rebuilding."
					" Now Enable sync/upload/cache mgmt");
				/* Backup package list */
				backup_package_list();
			}
		}
		sem_post(&(hcfs_system->access_sem));

		/* Backup if have_new_pkgbackup == TRUE */
		if (hcfs_system->system_going_down == FALSE)
			force_backup_package();
	}

	/* Join thread if still restoring */
	if (hcfs_system->system_restoring == RESTORING_STAGE2)
		destroy_rebuild_sb(FALSE);

	destroy_mount_mgr();
	destroy_fs_manager();
	release_meta_cache_headers();
	destroy_download_control();
	destroy_pin_scheduler();
#ifndef _ANDROID_ENV_ /* Not in Android */
	destroy_fuse_proc_communication(communicate_tid, socket_fd);
#endif
	sync();
	for (dl_count = 0; dl_count < MAX_DOWNLOAD_CURL_HANDLE; dl_count++)
		hcfs_destroy_backend(&(download_curl_handles[dl_count]));

	return 0;
}
