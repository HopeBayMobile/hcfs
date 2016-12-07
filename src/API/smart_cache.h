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

#define DEFAULT_PKG_LIST_SIZE 64

typedef struct boost_job_meta {
	int32_t to_boost;
	int32_t num_pkg;
	int32_t pkg_list_size;
	char **pkg_list;
} BOOST_JOB_META;

#define RUN_CMD_N_CHECK(RAWCMD, ...)                                           \
	do {                                                                   \
		memset(cmd, 0, sizeof(cmd));                                   \
		snprintf(cmd, sizeof(cmd), RAWCMD, ##__VA_ARGS__);             \
		status = system(cmd);                                          \
		if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {      \
			write_log(0, "In %s. Failed to run cmd %s", __func__,  \
				  cmd);                                        \
			ret_code = -EAGAIN;                                    \
			goto rollback;                                         \
		}                                                              \
	} while (0)

#define REMOVE_IF_EXIST(PATH)                                                  \
	do {                                                                   \
		if (access(PATH, F_OK) != -1) {                                \
			write_log(4,                                           \
				  "In %s. Path %s existed. Force remove it.",  \
				  __func__, PATH);                             \
			ret_code = _remove_folder(PATH);                       \
			if (ret_code < 0) {                                    \
				write_log(0,                                   \
					  "In %s. Failed to remove folder %s", \
					  __func__, PATH);                     \
				return -1;                                     \
			}                                                      \
		}                                                              \
	} while (0)

#define CHANGE_PKG_BOOST_STATUS(pkg, status)                                   \
	do {                                                                   \
		ret_code = change_boost_status(pkg, status);                   \
		if (ret_code < 0) {                                            \
			goto rollback;                                         \
		}                                                              \
	} while (0)

int32_t enable_booster(int64_t smart_cache_size);

int32_t trigger_boost(char to_boost, pthread_t *tid);

int32_t check_pkg_boost_status(char *package_name);

int32_t clear_boosted_package(char *package_name);

int32_t toggle_smart_cache_mount(char to_mount);
#endif  /* GW20_HCFSAPI_SMART_CACHE_H_ */
