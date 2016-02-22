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
* 2015/7/24 Kewei Modified function unlink_update_meta().
*
**************************************************************************/

#ifndef GW20_HCFS_FILE_PRESENT_H_
#define GW20_HCFS_FILE_PRESENT_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fuse/fuse_lowlevel.h>

#include "meta_mem_cache.h"
#include "xattr_ops.h"

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat,
		unsigned long *ret_gen, char *ret_pin_status);

int mknod_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen,
			ino_t root_ino, long long *delta_meta_size, char ispin);

int mkdir_update_meta(ino_t self_inode, ino_t parent_inode,
			const char *selfname,
			struct stat *this_stat, unsigned long this_gen,
			ino_t root_ino, long long *delta_meta_size, char ispin);

int unlink_update_meta(fuse_req_t req, ino_t parent_inode,
			const DIR_ENTRY *this_entry);

int meta_forget_inode(ino_t self_inode);

int rmdir_update_meta(fuse_req_t req, ino_t parent_inode, ino_t this_inode,
			const char *selfname);

int symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
	const struct stat *this_stat, const char *link,
	const unsigned long generation, const char *name,
	ino_t root_ino, long long *delta_meta_size, char ispin);

int fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, long long *xattr_pos, BOOL create_page);

int link_update_meta(ino_t link_inode, const char *newname,
	struct stat *link_stat, unsigned long *generation,
	META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry);

int increase_pinned_size(long long *reserved_pinned_size,
		long long file_size);

int decrease_pinned_size(long long *reserved_release_size,
		long long file_size);

int pin_inode(ino_t this_inode, long long *reserved_pinned_size);
int unpin_inode(ino_t this_inode, long long *reserved_release_size);

#endif /* GW20_HCFS_FILE_PRESENT_H_ */
