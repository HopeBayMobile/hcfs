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
