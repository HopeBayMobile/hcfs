/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: metaops.h
* Abstract: The c header file for meta processing involving regular
*           files and directories in HCFS. Functions are called mainly by
*           other functions in file_present.c.
*
* Revision History
* 2015/2/5 Jiahong created this file by moving some definition from
*          meta_mem_cache.h.
*
**************************************************************************/
#ifndef GW20_HCFS_METAOPS_H_
#define GW20_HCFS_METAOPS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fuseop.h"
#include "meta_mem_cache.h"

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr);
int decrease_nlink_inode_file(ino_t this_inode);
int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						long long this_page_pos);

#endif /* GW20_HCFS_METAOPS_H_ */
