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

#ifndef GW20_SRC_GLOBAL_H_
#define GW20_SRC_GLOBAL_H_

#include <stdint.h>
#include <limits.h>
#include <stdbool.h>

#define BOOL bool
#define TRUE true
#define FALSE false
#define ON 1
#define OFF 0

/* Defines the version of the current meta defs */
#define CURRENT_META_VER 1
#define BACKWARD_COMPATIBILITY 1

/* ENABLE() - turn on a specific feature of HCFS */
#define ENABLE(HCFS_FEATURE) \
	(defined ENABLE_##HCFS_FEATURE && ENABLE_##HCFS_FEATURE)

#define X64 1
#define ARM_32BIT 2
#define ANDROID_32BIT 3
#define ANDROID_64BIT 4

#define ARCH_CODE ANDROID_64BIT

/* Code for restoration stages */
#define NOT_RESTORING 0
#define RESTORING_STAGE1 1
#define RESTORING_STAGE2 2

/* List of API codes */
#define TERMINATE 0
#define VOLSTAT 1
#define TESTAPI 2
#define ECHOTEST 3
#define CREATEVOL 4
#define MOUNTVOL 5
#define DELETEVOL 6
#define CHECKVOL 7
#define LISTVOL 8
#define UNMOUNTVOL 9
#define CHECKMOUNT 10
#define UNMOUNTALL 11
#define PIN 12
#define UNPIN 13
#define CHECKDIRSTAT 14
#define GETVOLSIZE 15
#define GETCLOUDSIZE 16
#define GETPINSIZE 17
#define GETCACHESIZE 18
#define CHECKLOC 19
#define CHECKPIN 20
#define GETMAXPINSIZE 21
#define GETMAXCACHESIZE 22
#define CLOUDSTAT 23
#define GETDIRTYCACHESIZE 24
#define GETXFERSTAT 25
#define RESETXFERSTAT 26
#define SETSYNCSWITCH 27
#define GETSYNCSWITCH 28
#define GETSYNCSTAT 29
#define RELOADCONFIG 30
#define GETQUOTA 31
#define TRIGGERUPDATEQUOTA 32
#define CHANGELOG 33
#define UNPINDIRTYSIZE 34
#define OCCUPIEDSIZE 35
#define GETXFERSTATUS 36
#define SETNOTIFYSERVER 37
#define SETSWIFTTOKEN 38
#define SETSYNCPOINT 39
#define CANCELSYNCPOINT 40
#define GETMETASIZE 41
#define INITIATE_RESTORATION 42
#define CHECK_RESTORATION_STATUS 43
#define NOTIFY_APPLIST_CHANGE 44
#define SEND_NOTIFY_EVENT 45
#define TOGGLE_USE_MINIMAL_APK 46
#define GET_MINIMAL_APK_STATUS 47
#define SET_GOOGLEDRIVE_TOKEN 48
#define RETRY_CONN 49
#define ISSKIPDEX 50
#define SET_UPLOAD_INTERVAL 51
#define GETMAXMETASIZE 52

#define DEFAULT_PIN FALSE

#ifdef _ANDROID_ENV_
#define GID_SDCARD_RW 1015
#define GID_EVERYBODY 9997
#endif

#define SYSTEM_UID 1000
#define SYSTEM_GID 1000

#endif  /* GW20_SRC_GLOBAL_H_ */
