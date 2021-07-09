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

#ifndef GW20_HCFS_FS_MANAGER_H_
#define GW20_HCFS_FS_MANAGER_H_

#include <semaphore.h>
#include <inttypes.h>
#include "meta.h"

/* We will use DIR_ENTRY_PAGE as the b-tree node */
/* Filesystem name ==> d_name in DIR_ENTRY */
/* Filesystem root inode number ==> d_ino in DIR_ENTRY */
/* For Android, use d_type to identify whether this mount is internal
or external */

#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#define ANDROID_MULTIEXTERNAL 3
/* Android internal volumes */
#define APP_VOL_NAME "hcfs_app"
#define DATA_VOL_NAME "hcfs_data"

#define FSMGR_BACKUP "FSmgr_backup"
#endif

typedef struct {
	int64_t num_FS;
	int32_t FS_list_fh;
	uint8_t sys_uuid[16];
	sem_t op_lock;
} FS_MANAGER_HEAD_TYPE;

/* Data layout of fs_manager file */
typedef struct {
	uint8_t sys_uuid[16];
	DIR_META_TYPE fs_dir_meta;
	CLOUD_RELATED_DATA fs_clouddata; 
} _PACKED FS_MANAGER_HEADER_LAYOUT;

FS_MANAGER_HEAD_TYPE *fs_mgr_head;
char *fs_mgr_path;

int32_t init_fs_manager(void);
void destroy_fs_manager(void);
#ifdef _ANDROID_ENV_
int32_t add_filesystem(char *fsname, char voltype, DIR_ENTRY *ret_entry);
#else
int32_t add_filesystem(char *fsname, DIR_ENTRY *ret_entry);
#endif
int32_t delete_filesystem(char *fsname);
int32_t check_filesystem(char *fsname, DIR_ENTRY *ret_entry);
int32_t check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry);
int32_t list_filesystem(uint64_t buf_num, DIR_ENTRY *ret_entry,
		uint64_t *ret_num);

int32_t backup_FS_database(void);
int32_t restore_FS_database(void);
int32_t prepare_FS_database_backup(void);

#endif  /* GW20_HCFS_FS_MANAGER_H_ */

