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


typedef struct {
	long long root_entry_page;
	long long entry_page_gc_list;
	long long tree_walk_list_head;
	unsigned long num_FS;
	int FS_list_fh;
	sem_t op_lock;
} FS_MANAGER_HEAD_TYPE;

FS_MANAGER_HEAD_TYPE *fs_manager_head;

int init_fs_manager(void);
int destroy_fs_manager(void);
int add_filesystem(char *fsname, DIR_ENTRY *ret_entry);
int delete_filesystem(char *fsname);
int check_filesystem(char *fsname, DIR_ENTRY *ret_entry);
int list_filesystem(DIR_ENTRY *ret_entry, int *ret_num);

int backup_FS_database(void);
int restore_FS_database(void);

#endif  /* GW20_HCFS_FS_MANAGER_H_ */

