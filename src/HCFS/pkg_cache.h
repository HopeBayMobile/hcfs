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

#ifndef GW20_HCFS_PKG_LOOKUP_H_
#define GW20_HCFS_PKG_LOOKUP_H_
#include <semaphore.h>
#include <sys/types.h>
#include <stdint.h>

#include "params.h"

#define PKG_HASH_SIZE 8 /* Size of hash bucket */
#define MAX_PKG_ENTRIES 8 /* Max size of MRU linked list */
/* Moved pkg lookup here */
typedef struct PKG_CACHE_ENTRY {
	char pkgname[MAX_FILENAME_LEN+1];
	uid_t pkguid;
	struct PKG_CACHE_ENTRY *next;
} PKG_CACHE_ENTRY;

typedef struct {
	PKG_CACHE_ENTRY *first_pkg_entry;
	int32_t num_pkgs;
} PKG_ENTRY_HEAD;

typedef struct {
	PKG_ENTRY_HEAD pkg_hash[PKG_HASH_SIZE];
	int32_t num_cache_pkgs;
	int64_t hit_count;
	int64_t query_count;
	sem_t pkg_cache_lock; /* Lock for package to uid lookup cache */
} PKG_CACHE;

PKG_CACHE pkg_cache;

int32_t init_pkg_cache(void);
int32_t destroy_pkg_cache(void);
int32_t lookup_cache_pkg(const char *pkgname, uid_t *uid);
int32_t insert_cache_pkg(const char *pkgname, uid_t uid);
int32_t remove_cache_pkg(const char *pkgname);

#endif
