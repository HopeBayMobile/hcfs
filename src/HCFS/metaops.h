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
* 2015/5/11 Jiahong modifying seek_page for new block indexing / searching.
*           Also remove advance_block function.
* 2015/6/2 Jiahong moving lookup_dir to this file
*
**************************************************************************/
#ifndef GW20_HCFS_METAOPS_H_
#define GW20_HCFS_METAOPS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fuse/fuse_lowlevel.h>

#include "fuseop.h"
#include "meta_mem_cache.h"
#include "filetables.h"
#include "mount_manager.h"

int dir_add_entry(ino_t parent_inode, ino_t child_inode, const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int dir_remove_entry(ino_t parent_inode, ino_t child_inode,
			const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr);
int decrease_nlink_inode_file(fuse_req_t req, ino_t this_inode);
int change_dir_entry_inode(ino_t self_inode, const char *targetname,
			ino_t new_inode,
			mode_t new_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int delete_inode_meta(ino_t this_inode);
int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						long long this_page_pos);
long long seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page,
			long long hint_page);
long long create_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page);

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		long long target_page, long long hint_page);

int actual_delete_inode(ino_t this_inode, char d_type, ino_t root_inode,
			MOUNT_T *mptr);
int mark_inode_delete(fuse_req_t req, ino_t this_inode);

int disk_markdelete(ino_t this_inode, ino_t root_inode);
int disk_cleardelete(ino_t this_inode, ino_t root_inode);
int disk_checkdelete(ino_t this_inode, ino_t root_inode);
int startup_finish_delete(void);

int lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry);
#endif /* GW20_HCFS_METAOPS_H_ */
