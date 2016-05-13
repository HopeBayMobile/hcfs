/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dir_lookup.c
* Abstract: The c header file for conducting pathname to inode number
*           conversion and caching.
*
* Revision History
* 2015/2/10 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
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
