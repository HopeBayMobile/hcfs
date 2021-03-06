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

#include <sys/types.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <fuse/fuse_lowlevel.h>
#include <attr/xattr.h>

#include "atomic_tocloud.h"
#include "dir_statistics.h"
#include "filetables.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "meta_iterator.h"
#include "meta_mem_cache.h"
#include "mount_manager.h"
#include "params.h"
#include "xattr_ops.h"
#include "meta_iterator.h"

#include "fake_misc.h"

#define MOCK() printf("[MOCK] fake_misc.c:%4d %s\n",  __LINE__, __func__);
extern SYSTEM_CONF_STRUCT *system_config;

int32_t init_api_interface(void)
{
	MOCK();
	return 0;
}

int32_t destroy_api_interface(void)
{
	MOCK();
	return 0;
}

ino_t lookup_pathname(const char *path, int32_t *errcode)
{
	MOCK();
	*errcode = 0;
	uint32_t i;
	struct {
		const char *name;
		ino_t inod;
		int32_t errcode;
	} files[] = { { "/does_not_exist",              0,                  -ENOENT},
		      { "/testmkdir/test",              0,                  -ENOENT},
		      { "/",                            1,                  0},
		      { "/testfile",                    2,                  0},
		      { "/testsamefile",                2,                  0},
		      { "/testfile1",                   10,                 0},
		      { "/testfile2",                   11,                 0},
		      { "/testdir1",                    12,                 0},
		      { "/testdir2",                    13,                 0},
		      { "/testtruncate",                14,                 0},
		      { "/testread",                    15,                 0},
		      { "/testwrite",                   16,                 0},
		      { "/testlistdir",                 TEST_LISTDIR_INODE, 0},
		      { "/testsetxattr",                18,                 0},
		      { "/testsetxattr_permissiondeny", 19,                 0},
		      { "/testsetxattr_fail",           20,                 0} };
	for (i = 0; i < sizeof(files); i++) {
		if (strcmp(path, files[i].name) == 0) {
			*errcode = files[i].errcode;
			return files[i].inod;
		}
	}
	if (strcmp(path, "/testcreate") == 0) {
		if (before_mknod_created == TRUE) {
			*errcode = -ENOENT;
			return 0;
		}
		return 4;
	}
	if (strcmp(path, "/testmkdir") == 0) {
		if (before_mkdir_created == TRUE) {
			*errcode = -ENOENT;
			return 0;
		}
		return 6;
	}

	*errcode = -EACCES;
	return 0;
}

int32_t lookup_dir(ino_t parent, char *childname, DIR_ENTRY *dentry)
{
	MOCK();
	ino_t this_inode;
	char this_type;

	this_inode = 0;

	if (parent == 1) {   /* If parent is root */
		if (strcmp(childname, "does_not_exist") == 0)
			return -ENOENT;

		if (strcmp(childname, "testfile") == 0) {
			this_inode = 2;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testsamefile") == 0) {
			this_inode = 2;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testcreate") == 0) {
			if (before_mknod_created == TRUE)
				return -ENOENT;
			this_inode = 4;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testmkdir") == 0) {
			if (before_mkdir_created == TRUE)
				return -ENOENT;
			this_inode = 6;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testfile1") == 0) {
			this_inode = 10;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testfile2") == 0) {
			this_inode = 11;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testdir1") == 0) {
			this_inode = 12;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testdir2") == 0) {
			this_inode = 13;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testtruncate") == 0) {
			this_inode = 14;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testread") == 0) {
			this_inode = 15;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testwrite") == 0) {
			this_inode = 16;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testlistdir") == 0) {
			this_inode = TEST_LISTDIR_INODE;
			this_type = D_ISDIR;
		}
		if (strcmp(childname, "testsetxattr") == 0) {
			this_inode = 18;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsetxattr_permissiondeny") == 0) {
			this_inode = 19;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsetxattr_fail") == 0) {
			this_inode = 20;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsymlink") == 0) {
			this_inode = 21;
			this_type = D_ISLNK;
		}
		if (strcmp(childname, "testlink") == 0) {
			this_inode = 22;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testlink_dir_perm_denied") == 0) {
			this_inode = 23;
			this_type = D_ISDIR;
		}
		if (strcmp(childname, "com.example.test") == 0) {
			this_inode = TEST_APPDIR_INODE;
			this_type = D_ISDIR;
		}
		if (strcmp(childname, "base.apk") == 0) {
			this_inode = 15;
			this_type = D_ISREG;
		}
	}

	if (parent == 6) {
		if (strcmp(childname, "test") == 0) {
			return -ENOENT;
		}
	}

	if (parent == TEST_APPDIR_INODE) {
		if (strcmp(childname, "base.apk") == 0) {
			this_inode = TEST_APPAPK_INODE;
			this_type = D_ISREG;
		}
		if (strcmp(childname, ".basemin") == 0) {
			if (exists_minapk == FALSE)
				return -ENOENT;
			this_inode = TEST_APPMIN_INODE;
			this_type = D_ISREG;
		}
	}

	if (this_inode > 0) {
		dentry->d_ino = this_inode;
		strcpy(dentry->d_name, childname);
		dentry->d_type = this_type;
		return 0;
	}

	return -ENOENT;
}

off_t check_file_size(const char *path)
{
	MOCK();
	struct stat tempstat; /* file ops */

	stat(path, &tempstat);
	return tempstat.st_blocks * 512;
}

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	MOCK();
	if (access("/tmp/testHCFS/testblock", F_OK) != 0)
		mkdir("/tmp/testHCFS/testblock", 0700);
	snprintf(pathname, 100, "/tmp/testHCFS/testblock/block_%lu_%ld",
		 this_inode, block_num);
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
			   int64_t meta_size_delta,
			   int64_t cache_data_size_delta,
			   int64_t cache_blocks_delta,
			   int64_t dirty_cache_delta,
			   int64_t unpin_dirty_data_size,
			   BOOL need_sync)
{
	MOCK();
	hcfs_system->systemdata.system_meta_size += meta_size_delta;
	hcfs_system->systemdata.system_size += system_data_size_delta;
	hcfs_system->systemdata.cache_size += cache_data_size_delta;
	hcfs_system->systemdata.cache_blocks += cache_blocks_delta;
	return 0;
}

int32_t change_system_meta_ignore_dirty(ino_t this_inode,
					int64_t system_data_size_delta,
					int64_t meta_size_delta,
					int64_t cache_data_size_delta,
					int64_t cache_blocks_delta,
					int64_t dirty_cache_delta,
					int64_t unpin_dirty_data_size,
					BOOL need_sync)
{
	return change_system_meta(system_data_size_delta, meta_size_delta,
				  cache_data_size_delta, cache_blocks_delta,
				  dirty_cache_delta, unpin_dirty_data_size,
				  need_sync);
}

int32_t parse_parent_self(const char *pathname, char *parentname, char *selfname)
{
	MOCK();
	int32_t count;

	if (pathname == NULL)
		return -1;

	if (parentname == NULL)
		return -1;

	if (selfname == NULL)
		return -1;

	if (pathname[0] != '/')	 /* Does not handle relative path */
		return -1;

	if (strlen(pathname) <= 1)  /*This is the root, so no parent*/
	 return -1;

	for (count = strlen(pathname)-1; count >= 0; count--) {
		if ((pathname[count] == '/') &&
		    (count < ((int32_t)strlen(pathname) - 1)))
			break;
	}

	if (count == 0) {
		strcpy(parentname, "/");
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[1]), strlen(pathname)-2);
			selfname[strlen(pathname)-2] = 0;
		} else {
			strcpy(selfname, &(pathname[1]));
		}
	} else {
		strncpy(parentname, pathname, count);
		parentname[count] = 0;
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[count+1]),
						strlen(pathname)-count-2);
			selfname[strlen(pathname)-count-2] = 0;
		} else {
			strcpy(selfname, &(pathname[count+1]));
		}
	}
	return 0;
}

int64_t open_fh(ino_t thisinode, int32_t flags, BOOL isdir)
{
	MOCK();
	int64_t index;
	DIRH_ENTRY *dirh_ptr;

	if (fail_open_files)
		return -1;

	index = (int64_t) thisinode;
	/* To avoid file table conflicts in between tests */
	if (system_fh_table.entry_table_flags[index] != NO_FH)
		sleep(2);

	if (isdir == TRUE) {
		system_fh_table.entry_table_flags[index] = IS_DIRH;
		dirh_ptr = &(system_fh_table.direntry_table[index]);
		dirh_ptr->thisinode = thisinode;
		dirh_ptr->flags = flags;
		dirh_ptr->snapshot_ptr = NULL;
		sem_init(&(dirh_ptr->snap_ref_sem), 0, 0);
		sem_init(&(dirh_ptr->wait_ref_sem), 0, 1);
		system_fh_table.have_nonsnap_dir = TRUE;
	} else {
		system_fh_table.entry_table_flags[index] = IS_FH;
		system_fh_table.entry_table[index].thisinode = thisinode;
		system_fh_table.entry_table[index].meta_cache_ptr = NULL;
		system_fh_table.entry_table[index].meta_cache_locked = FALSE;
		system_fh_table.entry_table[index].flags = flags;

		system_fh_table.entry_table[index].blockfptr = NULL;
		system_fh_table.entry_table[index].opened_block = -1;
		system_fh_table.entry_table[index].cached_page_index = -1;
		system_fh_table.entry_table[index].cached_filepos = -1;
		sem_init(&(system_fh_table.entry_table[index].block_sem), 0, 1);
	}

	return index;
}

int32_t close_fh(int64_t index)
{
	MOCK();
	FH_ENTRY *tmp_entry;
	DIRH_ENTRY *tmp_DIRH_entry;

	if (system_fh_table.entry_table_flags[index] == IS_FH) {
		tmp_entry = &(system_fh_table.entry_table[index]);
		tmp_entry->meta_cache_locked = FALSE;
		system_fh_table.entry_table_flags[index] = NO_FH;
		tmp_entry->thisinode = 0;

		if (tmp_entry->meta_cache_ptr != NULL) {
			if (tmp_entry->meta_cache_ptr->fptr != NULL) {
				fclose(tmp_entry->meta_cache_ptr->fptr);
				tmp_entry->meta_cache_ptr->fptr = NULL;
			}

			free(tmp_entry->meta_cache_ptr);
		}
		tmp_entry->meta_cache_ptr = NULL;
		tmp_entry->blockfptr = NULL;
		tmp_entry->opened_block = -1;
		sem_destroy(&(tmp_entry->block_sem));
	} else {
		tmp_DIRH_entry = &(system_fh_table.direntry_table[index]);

		if (tmp_DIRH_entry->snapshot_ptr != NULL) {
			fclose(tmp_DIRH_entry->snapshot_ptr);
			tmp_DIRH_entry->snapshot_ptr = NULL;
		}
		system_fh_table.entry_table_flags[index] = NO_FH;
		tmp_DIRH_entry->thisinode = 0;
		sem_destroy(&(tmp_DIRH_entry->snap_ref_sem));
		sem_destroy(&(tmp_DIRH_entry->wait_ref_sem));
	}

	return 0;
}

int64_t seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page,
			int64_t hint_page)
{
	MOCK();
	switch (target_page) {
	case 0:
		return sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
	default:
		return 0;
	}
	return 0;
}

int64_t create_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page)
{
	MOCK();
	switch (target_page) {
	case 0:
		return sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
	default:
		return 0;
	}
	return 0;
}

void prefetch_block(PREFETCH_STRUCT_TYPE *ptr)
{
	MOCK();
}
int32_t fetch_from_cloud(FILE *fptr,
			 char action_from,
			 char *objname,
			 char *fileID)
{
	MOCK();
	char tempbuf[1024];
	int tmp_len;
	ino_t this_inode;
	long long block_no, seqnum;
	char buffer[102400] = {0};

	sscanf(objname, "data_%"PRIu64"_%lld_%lld",
			(uint64_t *)&this_inode, &block_no, &seqnum);

	switch (this_inode) {
	case 14:
		setbuf(fptr, NULL);
		pwrite(fileno(fptr), buffer, 102400, 0);
		//ftruncate(fileno(fptr), 102400);
		break;
	case 15:
	case 16:
		if (test_fetch_from_backend == TRUE) {
			fseek(fptr, 0, SEEK_SET);
			snprintf(tempbuf, 100, "This is a test data");
			tmp_len = strlen(tempbuf);
			fwrite(tempbuf, tmp_len, 1, fptr);
			fflush(fptr);
		} else {
			ftruncate(fileno(fptr), 204800);
		}
		break;
	default:
		break;
	}
	return 0;
}

void sleep_on_cache_full(void)
{
	MOCK();
	printf("Debug passed sleep on cache full\n");
	hcfs_system->systemdata.cache_size = 1200000;
	return;
}

int32_t dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
	mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}

int32_t dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}
int32_t change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}
int32_t change_entry_name(ino_t parent_inode, const char *targetname,
		          META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}

int32_t fetch_inode_stat(ino_t this_inode,
			 HCFS_STAT *inode_stat,
			 uint64_t *gen,
			 char *ret_pin_status)
{
	MOCK();
	if (ret_pin_status)
		*ret_pin_status = P_UNPIN;
	switch (this_inode) {
	case 1:
		inode_stat->ino = 1;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 2:
		inode_stat->ino = 2;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		break;
	case 4:
		inode_stat->ino = 4;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		break;
	case 6:
		inode_stat->ino = 6;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 10:
		inode_stat->ino = 10;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		break;
	case 11:
		inode_stat->ino = 11;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		inode_stat->size = 1024;
		break;
	case 12:
		inode_stat->ino = 12;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 13:
		inode_stat->ino = 13;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 14:
		inode_stat->ino = 14;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		inode_stat->size = 102400;
		break;
	case 15:
		inode_stat->ino = 15;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		inode_stat->size = 204800;
		break;
	case 16:
		inode_stat->ino = 16;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		inode_stat->size = 204800;
		break;
	case 17:
		inode_stat->ino = 17;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 18:
		inode_stat->ino = 18;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		break;
	case 19:
		inode_stat->ino = 19;
		inode_stat->mode = S_IFREG | 0500;
		inode_stat->atime = 100000;
		break;
	case 20:
		inode_stat->ino = 20;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		break;
	case 21:
		inode_stat->ino = 21;
		inode_stat->mode = S_IFLNK | 0700;
		inode_stat->atime = 100000;
		break;
	case 22:
		inode_stat->ino = 22;
		inode_stat->mode = S_IFREG | 0700;
		inode_stat->atime = 100000;
		inode_stat->nlink = 1;
		break;
	case 23:
		inode_stat->ino = 23;
		inode_stat->mode = S_IFDIR | 0600;
		inode_stat->atime = 100000;
		break;
	case 24:
		inode_stat->ino = 24;
		inode_stat->mode = S_IFDIR | 0700;
		inode_stat->atime = 100000;
		break;
	case 25:
		inode_stat->ino = 25;
		inode_stat->mode = S_IFREG | 0600;
		inode_stat->atime = 100000;
		inode_stat->size = 20480000;
		break;
	case 26:
		inode_stat->ino = 26;
		inode_stat->mode = S_IFREG | 0600;
		inode_stat->atime = 100000;
		inode_stat->size = 204800;
		break;
	default:
		break;
	}

	inode_stat->uid = geteuid();
	inode_stat->gid = getegid();

	if (this_inode == 1 && root_updated == TRUE)
		memcpy(inode_stat, &updated_root, sizeof(HCFS_STAT));
	if (this_inode != 1 && before_update_file_data == FALSE)
		memcpy(inode_stat, &updated_stat, sizeof(HCFS_STAT));

	if (gen)
		*gen = 10;

	return 0;
}

int32_t mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			HCFS_STAT *this_stat, uint64_t this_gen,
			MOUNT_T *mountptr, int64_t *delta_metasize, char ispin,
			BOOL is_external)
{
	MOCK();
	if (fail_mknod_update_meta == TRUE)
		return -1;
        before_mknod_created = FALSE;
	return 0;
}

int32_t mkdir_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			HCFS_STAT *this_stat, uint64_t this_gen,
			MOUNT_T *mountptr, int64_t *delta_metasize, char ispin,
			BOOL is_external)
{
	MOCK();
	if (fail_mkdir_update_meta == TRUE)
		return -1;
        before_mkdir_created = FALSE;
	return 0;
}

int32_t unlink_update_meta(fuse_req_t req, ino_t parent_inode,
			const DIR_ENTRY *this_entry,
			BOOL is_external)
{
	MOCK();
	if (this_entry->d_ino == 4)
		before_mknod_created = TRUE;
	return 0;
}

int32_t meta_forget_inode(ino_t self_inode)
{
	MOCK();
	return 0;
}

int32_t rmdir_update_meta(fuse_req_t req, ino_t parent_inode, ino_t this_inode,
			char *selfname, BOOL is_external)
{
	MOCK();
	if (this_inode == 6)
		before_mkdir_created = TRUE;
	return 0;
}

ino_t super_block_new_inode(HCFS_STAT *in_stat)
{
	MOCK();
	if (fail_super_block_new_inode == TRUE)
		return 0;
	return 4;
}

int32_t super_block_share_locking(void)
{
	MOCK();
	return 0;
}

int32_t super_block_share_release(void)
{
	MOCK();
	return 0;
}

int32_t invalidate_pathname_cache_entry(const char *path)
{
	MOCK();
	return 0;
}

void hcfs_destroy_backend(CURL_HANDLE *curl_handle)
{
	MOCK();
	return;
}
int32_t change_dir_entry_inode(ino_t self_inode, char *targetname,
	ino_t new_inode, mode_t new_mode, META_CACHE_ENTRY_STRUCT *body_ptr,
	BOOL is_external)
{
	MOCK();
	return 0;
}
int32_t decrease_nlink_inode_file(ino_t this_inode)
{
	MOCK();
	return 0;
}
int32_t delete_inode_meta(ino_t this_inode)
{
	MOCK();
	return 0;
}

int32_t lookup_init(void)
{
	MOCK();
	return 0;
}
int32_t lookup_increase(ino_t this_inode, int32_t amount, char d_type)
{
	MOCK();
	return 0;
}
int32_t lookup_decrease(ino_t this_inode, int32_t amount, char *d_type,
				char *need_delete)
{
	MOCK();
	return 0;
}
int32_t lookup_markdelete(ino_t this_inode)
{
	MOCK();
	return 0;
}

int32_t actual_delete_inode(ino_t this_inode, char d_type)
{
	MOCK();
	return 0;
}
int32_t mark_inode_delete(ino_t this_inode)
{
	MOCK();
	return 0;
}

int32_t disk_markdelete(ino_t this_inode)
{
	MOCK();
	return 0;
}
int32_t disk_cleardelete(ino_t this_inode)
{
	MOCK();
	return 0;
}
int32_t disk_checkdelete(ino_t this_inode)
{
	MOCK();
	return 0;
}
int32_t startup_finish_delete(void)
{
	MOCK();
	return 0;
}
int32_t lookup_destroy(void)
{
	MOCK();
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	MOCK();
	printf("Now parsing namespace\n");

	if (!strcmp(name, "user.aaa")) {
		*name_space = USER;
		strcpy(key, "aaa");
		return 0;
	} else if (!strcmp(name, "security.aaa")) {
		*name_space = SECURITY;
		strcpy(key, "aaa");
		return 0;
	} else {
		return -EOPNOTSUPP;
	}
}

int32_t insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, const int64_t xattr_filepos, 
	const char name_space, const char *key, 
	const char *value, const size_t size, const int32_t flag)
{
	MOCK();
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	return 0;
}

int32_t get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const char name_space, const char *key, char *value, const size_t size, 
	size_t *actual_size)
{
	MOCK();
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	if (size == 0) {
		*actual_size = CORRECT_VALUE_SIZE;
	} else {
		char *ans = "hello!getxattr:)";
		strcpy(value, ans);
		*actual_size = strlen(ans);
	}
	return 0;
}

int32_t list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, char *key_buf, 
	const size_t size, size_t *actual_size)
{
	MOCK();
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	if (size == 0) {
		*actual_size = CORRECT_VALUE_SIZE;
	} else {
		char ans[] = "hello!listxattr:)";
		strcpy(key_buf, ans);
		*actual_size = strlen(ans) + 1;
	}
	return 0;
}

int32_t remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, const int64_t xattr_filepos, 
	const char name_space, const char *key)
{
	MOCK();
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	return 0;
}

int32_t fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, int64_t *xattr_pos)
{
	MOCK();
	return 0;
}

int32_t unmount_event(char *fsname, char *mp)
{
	MOCK();
	return 0;
}

int32_t destroy_mount_mgr(void)
{
	MOCK();
	return 0;
}

void destroy_fs_manager(void)
{
	MOCK();
}

int32_t symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry, 
	const HCFS_STAT *this_stat, const char *link, 
	const uint64_t generation, const char *name, MOUNT_T *mountptr,
	int64_t *delta_metasize, char ispin, BOOL is_external)
{
	MOCK();
	if (!strcmp("update_meta_fail", link))
		return -1;

	return 0;
}

int32_t change_mount_stat(MOUNT_T *mptr, int64_t system_size_delta,
		int64_t meta_size_delta, int64_t num_inodes_delta)
{
	MOCK();
	return 0;
}

int32_t link_update_meta(ino_t link_inode, const char *newname,
	HCFS_STAT *link_stat, uint64_t *generation, 
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
	BOOL is_external)
{
	MOCK();
	memset(link_stat, 0, sizeof(HCFS_STAT));
	*generation = 5;
	link_stat->ino = link_inode;
	link_stat->mode = S_IFREG;

	if (!strcmp(newname, "new_link_update_meta_fail"))
		return -123;
	else
		return 0;
}

int32_t set_block_dirty_status(char *path, FILE *fptr, char status)
{
	MOCK();
	if (path != NULL) {
		if (status == TRUE)
			setxattr(path, "user.dirty", "T", 1, 0);
		else
			setxattr(path, "user.dirty", "F", 1, 0);
	} else {
		if (status == TRUE)
			fsetxattr(fileno(fptr), "user.dirty", "T", 1, 0);
		else
			fsetxattr(fileno(fptr), "user.dirty", "F", 1, 0);
	}
	return 0;
}

int32_t fetch_trunc_path(char *pathname, ino_t this_inode)
{
	MOCK();
	strcpy(pathname, "/tmp/testHCFS/mock_trunc");
	return 0;
}

int32_t construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		ino_t rootinode)
{
	MOCK();
	*result = malloc(10);
	snprintf(*result, 10, "/test");
	return 0;
}

int32_t delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete)
{
	MOCK();
	return 0;
}

int32_t pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	MOCK();
	return 0;
}
int32_t super_block_mark_dirty(ino_t this_inode)
{
	MOCK();
	return 0;
}
int32_t update_file_stats(FILE *metafptr,
			  int64_t num_blocks_delta,
			  int64_t num_cached_blocks_delta,
			  int64_t cached_size_delta,
			  int64_t dirty_data_size_delta,
			  ino_t thisinode)
{
	MOCK();
	return 0;
}
int32_t init_pin_scheduler(void)
{
	MOCK();
	return 0;
}
int32_t destroy_pin_scheduler(void)
{
	MOCK();
	return 0;
}
int32_t init_download_control(void)
{
	MOCK();
	return 0;
}
int32_t destroy_download_control(void)
{
	MOCK();
	return 0;
}

int32_t reset_dirstat_lookup(ino_t thisinode)
{
	MOCK();
	return 0;
}
int32_t update_dirstat_parent(ino_t baseinode, DIR_STATS_TYPE *newstat)
{
	MOCK();
	return 0;
}
int32_t read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	MOCK();
	return 0;
}
int32_t lookup_delete_parent(ino_t self_inode, ino_t parent_inode)
{
	MOCK();
	return 0;
}
int32_t lookup_replace_parent(ino_t self_inode, ino_t parent_inode1,
			  ino_t parent_inode2)
{
	MOCK();
	return 0;
}
int32_t check_file_storage_location(FILE *fptr,  DIR_STATS_TYPE *newstat)
{
	MOCK();
	return 0;
}

int update_meta_seq(META_CACHE_ENTRY_STRUCT *bptr)
{
	MOCK();
	return 0;
}

int update_block_seq(META_CACHE_ENTRY_STRUCT *bptr,
		off_t page_fpos, long long eindex, long long bindex,
		BLOCK_ENTRY_PAGE *bpage_ptr)
{
	MOCK();
	return 0;
}

BOOL is_natural_number(char const *str)
{
	MOCK();
	return TRUE;
}

void fetch_backend_block_objname(char *objname,
#if ENABLE(DEDUP)
		unsigned char *obj_id)
#else
	ino_t inode, long long block_no, long long seqnum)
#endif
{
	MOCK();
#if ENABLE(DEDUP)
	char obj_id_str[OBJID_STRING_LENGTH];

	obj_id_to_string(obj_id, obj_id_str);
	sprintf(objname, "data_%s", obj_id_str);
#else
	sprintf(objname, "data_%"PRIu64"_%lld_%lld",
			(uint64_t)inode, block_no, seqnum);
#endif

	return;
}

int32_t meta_cache_check_uploading(META_CACHE_ENTRY_STRUCT *body_ptr,
		ino_t inode, int64_t bindex, int64_t seq)
{
	MOCK();
	return 0;
}

int32_t meta_cache_set_uploading_info(META_CACHE_ENTRY_STRUCT *body_ptr,
		BOOL is_now_uploading, int32_t new_fd, int64_t toupload_blocks)
{
	MOCK();
	return 0;
}

int update_upload_seq(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	MOCK();
	return 0;
}

int fuseproc_set_uploading_info(const UPLOADING_COMMUNICATION_DATA *data)
{
	MOCK();
	return 0;
}

int32_t do_fallocate(ino_t this_inode, HCFS_STAT *newstat, int32_t mode,
		off_t offset, off_t length,
		META_CACHE_ENTRY_STRUCT **body_ptr, fuse_req_t req)
{
	MOCK();
	off_t newlen;
	off_t oldsize;

	if (mode != 0)
		return -ENOTSUP;
	newlen = offset + length;
	if (newlen <= newstat->size)
		return 0;
	oldsize = newstat->size;

	newstat->size = newlen;
	hcfs_system->systemdata.system_size += (newlen - oldsize);
	return 0;
}

int32_t meta_cache_get_meta_size(META_CACHE_ENTRY_STRUCT *ptr,
		int64_t *metasize, int64_t *metalocalsize)
{
	struct stat tmpstat;
	int64_t size, roundsize;

	MOCK();
	if (ptr->fptr) {
		fstat(fileno(ptr->fptr), &tmpstat);
		size = tmpstat.st_size;
		roundsize = tmpstat.st_blocks * 512;
	}

	if (metasize)
		*metasize = size;
	if (metalocalsize)
		*metalocalsize = roundsize;

	return 0;
}

int32_t lookup_cache_pkg(const char *pkgname, uid_t *uid)
{
	MOCK();
	return 0;
}

int32_t insert_cache_pkg(const char *pkgname, uid_t uid)
{
	MOCK();
	return 0;
}

int32_t remove_cache_pkg(const char *pkgname)
{
	MOCK();
	return 0;
}

int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type)
{
	num_stat_rebuilt++;
	return 0;
}
void destroy_rebuild_sb(BOOL destroy_queue_file)
{
	return;
}
void init_backend_related_module()
{
	return;
}
int64_t get_cache_limit(const char pin_type)
{
	MOCK();
	if (pin_type < NUM_PIN_TYPES)
		return CACHE_LIMITS(pin_type);
	else
		return -EINVAL;
}

int64_t get_pinned_limit(const char pin_type)
{
	MOCK();
	if (pin_type < NUM_PIN_TYPES)
		return PINNED_LIMITS(pin_type);
	else
		return -EINVAL;
}
int32_t meta_nospc_log(const char *func_name, int32_t lines)
{
	MOCK();
	return 1;
}

int32_t notify_restoration_result(int8_t stage, int32_t result)
{
	MOCK();
	return 0;
}
int32_t fetch_restore_stat_path(char *pathname)
{
	snprintf(pathname, METAPATHLEN, "%s/system_restoring_status",
	         METAPATH);
	return 0;
}

int64_t round_size(int64_t size)
{
	int64_t blksize = 4096;
	int64_t ret_size;

	if (size >= 0) {
		/* round up to filesystem block size */
		ret_size = (size + blksize - 1) & (~(blksize - 1));
	} else {
		size = -size;
		ret_size = -((size + blksize - 1) & (~(blksize - 1)));
	}

	return ret_size;
}
void cleanup_stage1_data(void)
{
	return;
}
void force_backup_package(void)
{
	return;
}

int32_t backup_package_list(void)
{
	return 0;
}

int32_t restore_stage1_reduce_cache(void)
{
	return 0;
}

void start_download_minimal(void)
{
	return;
}
int32_t fetch_all_parents(ino_t self_inode, int32_t *parentnum, ino_t **parentlist)
{
	if (self_inode == TEST_APPDIR_INODE) {
		*parentnum = 1;
		*parentlist = malloc(sizeof(ino_t));
		*parentlist[0] = TEST_APPROOT_INODE;
	}
	return 0;
}
int32_t check_data_location(ino_t this_inode)
{
	if (this_inode == TEST_APPAPK_INODE)
		return tmp_apk_location;
	return 0;
}

DIR_ENTRY_ITERATOR *mock_next(DIR_ENTRY_ITERATOR *iter)
{
	if (iter->now_entry) {
		free(iter->now_entry);
		iter->now_entry = NULL;
		errno = ENOENT;
		return NULL;
	}

	iter->now_entry = calloc(sizeof(DIR_ENTRY), 1);
	iter->now_entry->d_type = D_ISREG;
	strcpy(iter->now_entry->d_name, "kewei.apk");
	return iter;
}

DIR_ENTRY_ITERATOR *init_dir_iter(FILE *fptr)
{
	DIR_ENTRY_ITERATOR *iter =
	    (DIR_ENTRY_ITERATOR *)calloc(sizeof(DIR_ENTRY_ITERATOR), 1);
	iter->base.next = (void *)&mock_next;
	return iter;
}

void destroy_dir_iter(DIR_ENTRY_ITERATOR *iter)
{
	if (iter) {
		if (iter->now_entry) {
			free(iter->now_entry);
			iter->now_entry = NULL;
		}
		free(iter);
	}
}
int64_t init_lastsync_time(void)
{
	return 0;
}
/* Helper function for checking if the file extension is .apk */
BOOL is_apk(const char *filename)
{
	int32_t name_len;

	name_len = strlen(filename);

	/* If filename is too short to be an apk*/
	if (name_len < 5)
		return FALSE;

	if (!strncmp(&(filename[name_len - 4]), ".apk", 4))
		return TRUE;

	return FALSE;
}

BOOL is_minapk(const char *filename)
{
	int32_t name_len;

	name_len = strlen(filename);

	/* If filename is too short to be an apk*/
	if (name_len < 5)
		return FALSE;

	/* minapk name is ".<x>min" */
	if (*filename == '.' &&
		!strncmp(filename + name_len - 3, "min", 3))
		return TRUE;
	else
		return FALSE;
}
/* Helper function for converting apk name to minimal apk name */
int32_t convert_minapk(const char *apkname, char *minapk_name)
{
	size_t name_len;

	name_len = strlen(apkname);

	/* The length to copy before ".apk" */
	name_len -= 4;
	snprintf(minapk_name, (name_len + 2), ".%s", apkname);
	snprintf(&(minapk_name[1 + name_len]), 4, "min");
	write_log(10, "[App unpin] Name of minapk: %s\n", minapk_name);
	return 0;
}

int32_t convert_origin_apk(char *apkname, const char *minapk_name)
{
	size_t name_len;

	name_len = strlen(minapk_name);

	/* Could not be an apk */
	if (name_len < 5)
		return -EINVAL;

	/* From .<x>min to <x>.apk */
	memcpy(apkname, minapk_name + 1, name_len - 4);
	memcpy(apkname + name_len - 4, ".apk", 4);
	apkname[name_len] = '\0';
	return 0;
}
void fetch_progress_file_path(char *pathname, ino_t inode)
{
	sprintf(pathname, "%s/upload_bullpen/upload_progress_inode_%"PRIu64,
		METAPATH, (uint64_t)inode);

	return;
}
int32_t fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	int32_t errcode, ret;
	char path[200];

	sprintf(path, "%s/upload_bullpen", METAPATH);

	if (access(path, F_OK) == -1)
		MKDIR(path, 0700);

	sprintf(pathname, "%s/hcfs_local_meta_%"PRIu64".tmp",
			path, (uint64_t)inode);

	return 0;

errcode_handle:
	if (errcode == -EEXIST) {
		sprintf(pathname, "%s/hcfs_local_meta_%"PRIu64".tmp",
				path, (uint64_t)inode);
		return 0;
	}
	return errcode;
}

int32_t fetch_toupload_block_path(char *pathname,
				  ino_t inode,
				  int64_t block_no)
{
	char path[200];
	int32_t errcode;
	int32_t ret;

	sprintf(path, "%s/upload_temp_block", BLOCKPATH);

	if (access(path, F_OK) == -1)
		MKDIR(path, 0700);

	sprintf(pathname, "%s/hcfs_sync_block_%"PRIu64"_%"PRId64".tmp",
		path, (uint64_t)inode, block_no);

	return 0;

errcode_handle:
	if (errcode == -EEXIST) {
		sprintf(pathname, "%s/hcfs_sync_block_%"PRIu64"_%"PRId64".tmp",
				path, (uint64_t)inode, block_no);
		return 0;
	}
	return errcode;

}
FILE_BLOCK_ITERATOR *init_block_iter(FILE *fptr)
{
	FILE_BLOCK_ITERATOR *iter;
	int64_t ret_size;
	int32_t ret, errcode;

	iter = (FILE_BLOCK_ITERATOR *) calloc(sizeof(FILE_BLOCK_ITERATOR), 1);
	return iter;

errcode_handle:
	errno = -errcode;
	destroy_block_iter(iter);
	return NULL;
}
void destroy_block_iter(FILE_BLOCK_ITERATOR *iter)
{
	free(iter);
}
int32_t unlink_upload_file(char *filename)
{
	unlink(filename);
	return 0;
}
int32_t hfuse_ll_notify_delete(struct fuse_chan *ch,
                            fuse_ino_t parent,
                            fuse_ino_t child,
                            const char *name,
                            size_t namelen)
{
	return 0;
}
