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
* 2015/2/11  Jiahong moved "seek_page" and "advance_block" from filetables
*
**************************************************************************/
#ifndef GW20_HCFS_METAOPS_H_
#define GW20_HCFS_METAOPS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fuseop.h"
#include "meta_mem_cache.h"
#include "filetables.h"

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr);
int change_dir_entry_inode(ino_t self_inode, char *targetname,
		ino_t new_inode, META_CACHE_ENTRY_STRUCT *body_ptr);
int decrease_nlink_inode_file(ino_t this_inode);
int delete_inode_meta(ino_t this_inode);
int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						long long this_page_pos);
int seek_page(FH_ENTRY *fh_ptr, long long target_page);
/*In advance block, need to write back dirty page if change page */
long long advance_block(META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,
						long long *entry_index);

#endif /* GW20_HCFS_METAOPS_H_ */
