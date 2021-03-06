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

#ifndef GW20_RESTORATION_UTILS_H_
#define GW20_RESTORATION_UTILS_H_

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>

#include "global.h"
#include "params.h"
#include "fuseop.h"

#define INCREASE_LIST_SIZE 64

typedef struct INODE_PAIR {
	ino_t src_inode;
	ino_t target_inode;
} INODE_PAIR;

typedef struct INODE_PAIR_LIST {
	INODE_PAIR *inode_pair;
	int32_t list_max_size;
	int32_t num_list_entries;
} INODE_PAIR_LIST;

/* package_uid_list */
typedef struct _PKG_LINKLIST_NODE {
	char name[MAX_FILENAME_LEN + 1];
	int32_t uid;
	struct _PKG_LINKLIST_NODE *next;
} PKG_NODE;

typedef struct {
	PKG_NODE **sarray;
	uint32_t count;
} PKG_INFO;

PKG_INFO restore_pkg_info;
PKG_NODE *pkg_info_list_head;

int32_t stat_device_path(char *path, HCFS_STAT *hcfsstat);
int32_t insert_inode_pair(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t target_inode);
int32_t find_target_inode(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t *target_inode);
INODE_PAIR_LIST *new_inode_pair_list();
void destroy_inode_pair_list(INODE_PAIR_LIST *list);

int32_t init_package_uid_list(char *xml_path);
void destroy_package_uid_list(void);
int32_t lookup_package_uid_list(const char *pkgname);

int32_t create_smartcache_symlink(ino_t this_inode, ino_t root_ino,
		char *pkgname);
#endif
