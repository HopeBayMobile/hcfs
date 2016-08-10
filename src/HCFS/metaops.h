/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
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

int32_t dir_add_entry(ino_t parent_inode, ino_t child_inode, const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int32_t dir_remove_entry(ino_t parent_inode, ino_t child_inode,
			const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int32_t change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr);
int32_t decrease_nlink_inode_file(fuse_req_t req, ino_t this_inode);
int32_t change_dir_entry_inode(ino_t self_inode, const char *targetname,
			ino_t new_inode,
			mode_t new_mode, META_CACHE_ENTRY_STRUCT *body_ptr);
int32_t delete_inode_meta(ino_t this_inode);
int32_t init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						int64_t this_page_pos);
int64_t seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page,
			int64_t hint_page);
int64_t create_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page);

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page);

int32_t actual_delete_inode(ino_t this_inode, char d_type, ino_t root_inode,
			MOUNT_T *mptr);
int32_t mark_inode_delete(fuse_req_t req, ino_t this_inode);

int32_t disk_markdelete(ino_t this_inode, MOUNT_T *mptr);
int32_t disk_cleardelete(ino_t this_inode, ino_t root_inode);
int32_t disk_checkdelete(ino_t this_inode, ino_t root_inode);
int32_t startup_finish_delete(void);

int32_t lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry);

int32_t check_page_level(int64_t page_index);

int32_t change_pin_flag(ino_t this_inode, mode_t this_mode, char new_pin_status);

int32_t collect_dir_children(ino_t this_inode, ino_t **dir_node_list,
	int64_t *num_dir_node, ino_t **nondir_node_list,
	int64_t *num_nondir_node, char **nondir_type_list);

int32_t update_meta_seq(META_CACHE_ENTRY_STRUCT *bptr);

int32_t update_block_seq(META_CACHE_ENTRY_STRUCT *bptr, off_t page_fpos,
		int64_t eindex, int64_t bindex, int64_t now_seq,
		BLOCK_ENTRY_PAGE *bpage_ptr);
int32_t inherit_xattr(ino_t parent_inode, ino_t this_inode,
		META_CACHE_ENTRY_STRUCT *selbody_ptr);

int32_t change_unpin_dirty_size(ino_t this_inode, char ispin);
int32_t collect_dirmeta_children(DIR_META_TYPE *dir_meta, FILE *fptr,
		ino_t **dir_node_list, int64_t *num_dir_node,
		ino_t **nondir_node_list, int64_t *num_nondir_node,
		char **nondir_type_list);

int32_t restore_meta_file(ino_t this_inode);
int32_t restore_meta_structure(FILE *fptr);

#endif /* GW20_HCFS_METAOPS_H_ */
