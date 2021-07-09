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
#ifndef GW20_HCFS_PARENT_LOOKUP_H_
#define GW20_HCFS_PARENT_LOOKUP_H_

#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdint.h>

#include "params.h"

#define MAX_PARENTS_PER_PAGE 32
#define PLOOKUP_HASH_NUM_ENTRIES 1024
typedef struct {
	ino_t parentinode;
	uint8_t haveothers;
} PRIMARY_PARENT_T;

typedef struct {
	ino_t thisinode;
	int8_t num_parents;
	ino_t parents[MAX_PARENTS_PER_PAGE];
	int64_t nextlookup;  /* File pos of the next inode in the same hash */
	int64_t nextpage; /* File pos of the next page for this inode */
	int64_t gc_next;
} PLOOKUP_PAGE_T;

typedef struct {
	int64_t hash_head[PLOOKUP_HASH_NUM_ENTRIES];
	int64_t gc_head;
} PLOOKUP_HEAD_T;

PLOOKUP_HEAD_T parent_lookup_head;	

FILE *plookup2_fptr;
/* API for calling from outside */

int32_t fetch_all_parents(ino_t self_inode, int32_t *parentnum, ino_t **parentlist);
int32_t lookup_add_parent(ino_t self_inode, ino_t parent_inode);
int32_t lookup_delete_parent(ino_t self_inode, ino_t parent_inode);
int32_t lookup_replace_parent(ino_t self_inode, ino_t parent_inode1,
			  ino_t parent_inode2);

#endif  /* GW20_HCFS_PARENT_LOOKUP_H_ */

