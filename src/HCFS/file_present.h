/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: file_present.h
* Abstract: The c header file for meta processing involving regular
*           files and directories in HCFS. "file_present" means 
*           "file-level presentation".
*
* Revision History
* 2015/2/5 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#ifndef GW20_HCFS_FILE_PRESENT_H_
#define GW20_HCFS_FILE_PRESENT_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "meta_mem_cache.h"
#include "xattr_ops.h"

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat,
		unsigned long *ret_gen);

int mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen);
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen);
int unlink_update_meta(ino_t parent_inode, ino_t this_inode,
			const char *selfname);

int meta_forget_inode(ino_t self_inode);

int rmdir_update_meta(ino_t parent_inode, ino_t this_inode,
			const char *selfname);

int symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry, 
	const struct stat *this_stat, const char *link, 
	const unsigned long generation, const char *name);

int fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *xattr_pos);

#endif /* GW20_HCFS_FILE_PRESENT_H_ */
