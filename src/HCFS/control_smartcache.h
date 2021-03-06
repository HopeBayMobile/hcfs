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

#ifndef GW20_HCFS_CONTROL_SMART_CACHE_H_
#define GW20_HCFS_CONTROL_SMART_CACHE_H_

#include <inttypes.h>
#include <sys/stat.h>

#include "global.h"

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


int32_t write_restored_smartcache_info(void);
int32_t read_restored_smartcache_info(void);
int32_t destroy_restored_smartcacahe_info(void);

int32_t unmount_smart_cache(char *mount_point);
int32_t inject_restored_smartcache(ino_t smartcache_ino);
int32_t mount_and_repair_restored_smartcache(void);
int32_t mount_hcfs_smartcache_vol(void);
void change_stage1_cache_limit(int64_t restored_smartcache_size);
int32_t extract_restored_smartcache(ino_t smartcache_ino,
			BOOL smartcache_already_in_hcfs);
#endif
