/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: global.h
* Abstract: This c header file for global settings for HCFS.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_GLOBAL_H_
#define GW20_HCFSAPI_GLOBAL_H_

#include "hcfs_global.h"

#define CONFIG_PATH "/data/hcfs.conf"
#define DB_PATH "/data/data/com.hopebaytech.hcfsmgmt/databases/uid.db"
#ifdef UNITTEST
  #define SOCK_PATH "hcfs_reporter"
#else
  #define SOCK_PATH "/dev/shm/hcfs_reporter"
#endif
#define API_SOCK_PATH "/data/data/com.hopebaytech.hcfsmgmt/hcfsapid_sock"

#define DATA_PREFIX "/data/data"
#define APP_PREFIX "/data/app"
#define EXTERNAL_PREFIX "/storage/emulated"

#define LOG_PATH "/data"
#define LOG_NAME "hcfsapid.log"
#define LOG_LEVEL 4

/* Notify Events */
#define MAX_RETRY_SEND_TIMES 5
#define RETCODE_QUEUE_FULL 2

#define EVENT_BOOST_SUCCESS 8
#define EVENT_BOOST_FAILED 9

/* API codes only used by hcfsapid */
enum { SETCONFIG = 100,
       GETCONFIG,
       GETSTAT,
       SYSREBOOT,
       QUERYPKGUID,
       COLLECTSYSLOGS,
       CHECK_PACKAGE_BOOST_STATUS,
       ENABLE_BOOSTER,
       DISABLE_BOOSTER,
       TRIGGER_BOOST,
       TRIGGER_UNBOOST,
       CLEAR_BOOSTED_PACKAGE,
       MOUNT_SMART_CACHE,
       UMOUNT_SMART_CACHE,
       CREATE_MINI_APK,
       CHECK_MINI_APK };

#endif  /* GW20_HCFSAPI_GLOBAL_H_ */
