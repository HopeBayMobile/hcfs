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
#ifndef GW20_HCFS_PATH_RECONSTRUCT_H_
#define GW20_HCFS_PATH_RECONSTRUCT_H_

#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>

#include "params.h"

#define MAX_LOOKUP_NODES 4096
#define NUM_LOOKUP_ENTRY 4096
#define NUM_NODES_DROP 128

struct path_lookup {
	ino_t child;
	ino_t parent;
	char childname[MAX_FILENAME_LEN+1];
	int64_t lookupcount;
	struct path_lookup *prev;
	struct path_lookup *next;
	struct path_lookup *gprev;
	struct path_lookup *gnext;
	struct path_lookup *self;
};
typedef struct path_lookup PATH_LOOKUP;

typedef struct {
	PATH_LOOKUP *first;
	PATH_LOOKUP *last;
} PATH_HEAD_ENTRY;

/* There could be multiple such hash table, one for each mounted volume */
typedef struct {
	PATH_HEAD_ENTRY hashtable[NUM_LOOKUP_ENTRY];
	PATH_LOOKUP *gfirst;
	int32_t num_nodes;
	sem_t pathcache_lock;
	ino_t root_inode;
} PATH_CACHE;

sem_t pathlookup_data_lock;
FILE *pathlookup_data_fptr;

/* API for calling from outside */
PATH_CACHE * init_pathcache(ino_t root_inode);
int32_t destroy_pathcache(PATH_CACHE *cacheptr);

int32_t lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode);
int32_t construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int32_t bufsize);

int32_t construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
                   ino_t rootinode);

int32_t delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete);

int32_t init_pathlookup(void);
void destroy_pathlookup(void);
int32_t pathlookup_write_parent(ino_t self_inode, ino_t parent_inode);
int32_t pathlookup_read_parent(ino_t self_inode, ino_t *parentptr);

#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */

