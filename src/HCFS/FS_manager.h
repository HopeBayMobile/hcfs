/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: FS_manager.h
* Abstract: The header file for filesystem manager
*
* Revision History
* 2015/7/1 Jiahong created this file
*
**************************************************************************/

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
#endif

typedef struct {
	int64_t num_FS;
	int32_t FS_list_fh;
	uint8_t sys_uuid[16];
	sem_t op_lock;
} FS_MANAGER_HEAD_TYPE;

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

