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

#include "file_meta_defs.h"

void sleep_on_cache_full(void);
void notify_sleep_on_cache(void);
void run_cache_loop(void);

#endif  /* GW20_HCFS_HCFS_CACHEOPS_H_ */
