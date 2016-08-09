/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pkg_cache.h
* Abstract: The c header file for defining structure of package lookup
*           in memory.
*
* Revision History
* 2016/4/27 Kewei create this file and define data structure.
*
**************************************************************************/

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
