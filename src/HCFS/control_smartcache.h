/*************************************************************************
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: control_smartcache.h
* Abstract: The c header file for handling to mount smart cache.
*
* Revision History
* 2016/10/20 Kewei created this header file.
*
**************************************************************************/

#ifndef GW20_HCFS_MOUNT_SMART_CACHE_H_
#define GW20_HCFS_MOUNT_SMART_CACHE_H_

#include <inttypes.h>

#define SMART_CACHE_PATH "/mnt/hcfsblock"

int32_t unmount_smart_cache();
int32_t mount_smart_cache();

#endif
