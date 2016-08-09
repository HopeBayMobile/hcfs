/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_cacheops.h
* Abstract: The c header file for cache management operations.
*
* Revision History
* 2015/2/11 Jiahong created this file from part of hcfs_cache.h.
* 2015/2/12 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_HCFS_CACHEOPS_H_
#define GW20_HCFS_HCFS_CACHEOPS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

#define SCAN_INT 300

int32_t sleep_on_cache_full(void);
void notify_sleep_on_cache(int32_t cache_replace_status);
#ifdef _ANDROID_ENV_
void *run_cache_loop(void *ptr);
#else
void run_cache_loop(void);
#endif

int64_t get_cache_limit(const char pin_type);
int64_t get_pinned_limit(const char pin_type);

#endif  /* GW20_HCFS_HCFS_CACHEOPS_H_ */
