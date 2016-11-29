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

#ifndef GW20_HCFS_CONTROL_SMART_CACHE_H_
#define GW20_HCFS_CONTROL_SMART_CACHE_H_

#include <inttypes.h>
#include <sys/stat.h>

#define COMMAND_LEN 300

#define SMART_CACHE_ROOT_MP "/data/smartcache"

#define SMART_CACHE_MP "/data/mnt/hcfsblock"
#define SMART_CACHE_VOL_NAME "hcfs_smartcache"
#define SMART_CACHE_FILE "hcfsblock"

#define RESTORED_SMART_CACHE_LODEV "/dev/block/loop5"
#define RESTORED_SMART_CACHE_MP "/data/mnt/hcfsblock_restore"
#define RESTORED_SMARTCACHE_TMP_NAME "hcfsblock_restore"

#define IS_SMARTCACHE_FILE(folder, name) \
	((strcmp(SMART_CACHE_ROOT_MP, folder) == 0) && \
	 (strcmp(SMART_CACHE_FILE, name) == 0))


int32_t write_restored_smartcache_info();
int32_t read_restored_smartcache_info();
int32_t destroy_restored_smartcacahe_info();

int32_t unmount_smart_cache(char *mount_point);
int32_t inject_restored_smartcache(ino_t smartcache_ino);
int32_t extract_restored_smartcache(ino_t smartcache_ino);
int32_t mount_and_repair_restored_smartcache();
int32_t mount_hcfs_smartcache_vol();
void change_stage1_cache_limit(int64_t restored_smartcache_size);
#endif
