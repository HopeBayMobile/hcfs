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
#ifndef GW20_HCFS_DIR_LOOKUP_H_
#define GW20_HCFS_DIR_LOOKUP_H_

#include <semaphore.h>

#include "fuseop.h"

#define MAX_PATHNAME 256
#define PATHNAME_CACHE_ENTRY_NUM 65536

typedef struct {
	char pathname[MAX_PATHNAME+10];
	ino_t inode_number;
	sem_t cache_entry_sem;
} PATHNAME_CACHE_ENTRY;

ino_t lookup_pathname(const char *path, int32_t *errcode);
ino_t lookup_pathname_recursive(ino_t subroot, int32_t prefix_len,
		const char *partialpath, const char *fullpath, int32_t *errcode);
uint64_t compute_hash(const char *path);
int32_t init_pathname_cache(void);
int32_t replace_pathname_cache(int64_t index, char *path, ino_t inode_number);

/* If a dir or file is removed or changed, by e.g. rename, move, rm, rmdir,
	this function has to be called */
int32_t invalidate_pathname_cache_entry(const char *path);
ino_t check_cached_path(const char *path);
int32_t lookup_dir(ino_t parent, const char *childname, DIR_ENTRY *dentry);

#endif  /* GW20_HCFS_DIR_LOOKUP_H_ */
