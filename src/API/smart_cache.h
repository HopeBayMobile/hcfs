/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: smart_cache.h
* Abstract: This c header file for smart cache APIs.
*
* Revision History
* 2016/10/15 Create this file.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_SMART_CACHE_H_
#define GW20_HCFSAPI_SMART_CACHE_H_

#include <inttypes.h>
#include <pthread.h>
#include <sqlite3.h>

#define DATA_PREFIX "/data/data"
#define ANDROID_INTERNAL 1
#define MP_DEFAULT 1

#define SMARTCACHE_DB_PATH "/data/data/com.hopebaytech.hcfsmgmt/databases/uid.db"
#define SMARTCACHE_TABLE_NAME "uid"

#define TOUNBOOST 0
#define TOBOOST 1

/* boost status */
#define ST_NON_BOOSTABLE 0
#define ST_INIT_UNBOOST 1
#define ST_UNBOOSTED 2
#define ST_UNBOOSTING 3
#define ST_UNBOOST_FAILED 4
#define ST_INIT_BOOST 5
#define ST_BOOSTED 6
#define ST_BOOSTING 7
#define ST_BOOST_FAILED 8

#define SMARTCACHEVOL "hcfs_smartcache"
#define SMARTCACHE "/data/smartcache" /* hcfs mountpoint */
#define SMARTCACHEAMNT "/data/mnt" /* parent of ext4 mountpoint */
#define SMARTCACHEMTP "/data/mnt/hcfsblock" /* ext4 mountpoint */
#define LOOPDEV "/dev/block/loop6"
#define HCFSBLOCK "hcfsblock"
#define HCFSBLOCKSIZE 104857600 /* default 100MB */

typedef struct boost_job_meta {
	int32_t to_boost;
	sqlite3 *db;
} BOOST_JOB_META;

#define RUN_CMD_N_CHECK()                                                      \
	do {                                                                   \
		status = system(cmd);                                          \
		if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {      \
			write_log(0, "In %s. Failed to run cmd %s", __func__,  \
				  cmd);                                        \
			return -EAGAIN;                                        \
		}                                                              \
		memset(cmd, 0, sizeof(cmd));                                   \
	} while (0)

int32_t enable_booster(int64_t smart_cache_size);

int32_t trigger_boost(char to_boost, pthread_t *tid);

int32_t check_pkg_boost_status(char *package_name);

int32_t clear_boosted_package(char *package_name);

int32_t toggle_smart_cache_mount(char to_mount);
#endif  /* GW20_HCFSAPI_SMART_CACHE_H_ */
