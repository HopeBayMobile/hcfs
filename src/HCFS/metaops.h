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
#include "meta.h"

#define BLOCKS_OF_SIZE(size, block) ((size == 0) ? 0 : ((size - 1) / block + 1))
int32_t dir_add_entry(ino_t parent_inode, ino_t child_inode, const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external);
int32_t dir_remove_entry(ino_t parent_inode, ino_t child_inode,
			const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external);
/* change_entry_name should only be called from a rename situation where
the volume is "external" and if the old and the new name are the same if
case insensitive */
int32_t change_entry_name(ino_t parent_inode, const char *targetname,
			META_CACHE_ENTRY_STRUCT *body_ptr);
int32_t change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external);
int32_t decrease_nlink_inode_file(fuse_req_t req, ino_t this_inode);
int32_t change_dir_entry_inode(ino_t self_inode, const char *targetname,
			ino_t new_inode,
			mode_t new_mode, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external);
int32_t delete_inode_meta(ino_t this_inode);
int32_t init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode,
						int64_t this_page_pos);
int64_t seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page,
			int64_t hint_page);
int64_t create_page(META_CACHE_ENTRY_STRUCT *body_ptr, int64_t target_page);

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page);

int32_t check_meta_on_cloud(ino_t this_inode,
		char d_type, BOOL *meta_on_cloud,
		int64_t *metasize, int64_t *metalocalsize);
int32_t actual_delete_inode(ino_t this_inode, char d_type, ino_t root_inode,
			MOUNT_T *mptr);
int32_t mark_inode_delete(fuse_req_t req, ino_t this_inode);

int32_t disk_markdelete(ino_t this_inode, MOUNT_T *mptr);
int32_t disk_cleardelete(ino_t this_inode, ino_t root_inode);
int32_t disk_checkdelete(ino_t this_inode, ino_t root_inode);
int32_t startup_finish_delete(void);

int32_t lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry,
		   BOOL is_external);

int32_t check_page_level(int64_t page_index);

int32_t change_pin_flag(ino_t this_inode, mode_t this_mode, char new_pin_status);

int32_t collect_dir_children(ino_t this_inode, ino_t **dir_node_list,
	int64_t *num_dir_node, ino_t **nondir_node_list,
	int64_t *num_nondir_node, char **nondir_type_list, BOOL ignore_minapk);

int32_t update_meta_seq(META_CACHE_ENTRY_STRUCT *bptr);

int32_t update_block_seq(META_CACHE_ENTRY_STRUCT *bptr, off_t page_fpos,
		int64_t eindex, int64_t bindex, int64_t now_seq,
		BLOCK_ENTRY_PAGE *bpage_ptr);
int32_t inherit_xattr(ino_t parent_inode, ino_t this_inode,
		META_CACHE_ENTRY_STRUCT *selbody_ptr);

int32_t change_unpin_dirty_size(ino_t this_inode, PIN_t pin);
int32_t collect_dirmeta_children(DIR_META_TYPE *dir_meta, FILE *fptr,
		ino_t **dir_node_list, int64_t *num_dir_node,
		ino_t **nondir_node_list, int64_t *num_nondir_node,
		char **nondir_type_list, BOOL ignore_minapk);

int32_t restore_meta_file(ino_t this_inode);
int32_t restore_meta_structure(FILE *fptr);
int32_t restore_borrowed_meta_structure(FILE *fptr, int32_t uid, ino_t src_ino,
			ino_t target_ino);

/* Returns 0 if a file is "local", 1 if "cloud", or 2 if "hybrid" */
int32_t check_data_location(ino_t this_inode);

#endif /* GW20_HCFS_METAOPS_H_ */
