/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
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

#include "fuseop.h"

/* We will use DIR_ENTRY_PAGE as the b-tree node */
/* Filesystem name ==> d_name in DIR_ENTRY */
/* Filesystem root inode number ==> d_ino in DIR_ENTRY */
/* For Android, use d_type to identify whether this mount is internal
or external */

#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#define ANDROID_3EXTERNAL 3
#endif

typedef struct {
	unsigned long num_FS;
	int FS_list_fh;
	unsigned char sys_uuid[16];
	sem_t op_lock;
} FS_MANAGER_HEAD_TYPE;

FS_MANAGER_HEAD_TYPE *fs_mgr_head;
char *fs_mgr_path;

int init_fs_manager(void);
void destroy_fs_manager(void);
#ifdef _ANDROID_ENV_
int add_filesystem(char *fsname, char voltype, DIR_ENTRY *ret_entry);
#else
int add_filesystem(char *fsname, DIR_ENTRY *ret_entry);
#endif
int delete_filesystem(char *fsname);
int check_filesystem(char *fsname, DIR_ENTRY *ret_entry);
int check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry);
int list_filesystem(unsigned long buf_num, DIR_ENTRY *ret_entry,
		unsigned long *ret_num);

int backup_FS_database(void);
int restore_FS_database(void);
int prepare_FS_database_backup(void);

#endif  /* GW20_HCFS_FS_MANAGER_H_ */

