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

#ifndef GW20_HCFS_FILE_PRESENT_H_
#define GW20_HCFS_FILE_PRESENT_H_

#include <fuse/fuse_lowlevel.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "atomic_tocloud.h"
#include "meta_mem_cache.h"
#include "mount_manager.h"

int32_t fetch_inode_stat(ino_t this_inode,
			 HCFS_STAT *inode_stat,
			 uint64_t *ret_gen,
			 char *ret_pin_status);

int32_t mknod_update_meta(ino_t self_inode,
			  ino_t parent_inode,
			  const char *selfname,
			  HCFS_STAT *this_stat,
			  uint64_t this_gen,
			  MOUNT_T *mountptr,
			  int64_t *delta_meta_size,
			  PIN_t pin,
			  BOOL is_external);

int32_t mkdir_update_meta(ino_t self_inode,
			  ino_t parent_inode,
			  const char *selfname,
			  HCFS_STAT *this_stat,
			  uint64_t this_gen,
			  MOUNT_T *mountptr,
			  int64_t *delta_meta_size,
			  PIN_t pin,
			  BOOL is_external);

int32_t unlink_update_meta(fuse_req_t req,
			   ino_t parent_inode,
			   const DIR_ENTRY *this_entry,
			   BOOL is_external);

int32_t meta_forget_inode(ino_t self_inode);

int32_t rmdir_update_meta(fuse_req_t req,
			  ino_t parent_inode,
			  ino_t this_inode,
			  const char *selfname,
			  BOOL is_external);

int32_t symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
			    const HCFS_STAT *this_stat,
			    const char *link,
			    const uint64_t generation,
			    const char *name,
			    MOUNT_T *mountptr,
			    int64_t *delta_meta_size,
			    PIN_t pin,
			    BOOL is_external);

int32_t fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
			 XATTR_PAGE *xattr_page,
			 int64_t *xattr_pos,
			 BOOL create_page);

int32_t link_update_meta(ino_t link_inode,
			 const char *newname,
			 HCFS_STAT *link_stat,
			 uint64_t *generation,
			 META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry,
			 BOOL is_external);

int32_t increase_pinned_size(int64_t *reserved_pinned_size,
			     int64_t file_size,
			     char local_pin);

int32_t decrease_pinned_size(int64_t *reserved_release_size, int64_t file_size);

int32_t pin_inode(ino_t this_inode,
		  int64_t *reserved_pinned_size,
		  char pin_type);

int32_t unpin_inode(ino_t this_inode, int64_t *reserved_release_size);

int32_t update_upload_seq(META_CACHE_ENTRY_STRUCT *body_ptr);

int32_t fuseproc_set_uploading_info(const UPLOADING_COMMUNICATION_DATA *data);

#endif /* GW20_HCFS_FILE_PRESENT_H_ */
