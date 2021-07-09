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

#ifndef GW20_SRC_HCFSVOL_H_
#define GW20_SRC_HCFSVOL_H_

#include "global.h"

#define MAX_FILENAME_LEN 255
#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#define ANDROID_MULTIEXTERNAL 3

#define MP_DEFAULT 1
#define MP_READ 2
#define MP_WRITE 3
#endif

typedef struct {
	const char *name;
	int32_t code;
} CMD;
CMD cmd_list[] = { { "create", CREATEVOL },
		   { "delete", DELETEVOL },
		   { "check", CHECKVOL },
		   { "list", LISTVOL },
		   { "terminate", TERMINATE },
		   { "mount", MOUNTVOL },
		   { "unmount", UNMOUNTVOL },
		   { "checkmount", CHECKMOUNT },
		   { "unmountall", UNMOUNTALL },
		   { "checknode", CHECKDIRSTAT },
		   { "volsize", GETVOLSIZE },
		   { "metasize", GETMETASIZE },
		   { "cloudsize", GETCLOUDSIZE },
		   { "pinsize", GETPINSIZE },
		   { "cachesize", GETCACHESIZE },
		   { "location", CHECKLOC },
		   { "ispin", CHECKPIN },
		   { "maxpinsize", GETMAXPINSIZE },
		   { "maxcachesize", GETMAXCACHESIZE },
		   { "dirtysize", GETDIRTYCACHESIZE },
		   { "getxfer", GETXFERSTAT },
		   { "resetxfer", RESETXFERSTAT },
		   { "cloudstat", CLOUDSTAT },
		   { "setsyncswitch", SETSYNCSWITCH },
		   { "getsyncswitch", GETSYNCSWITCH },
		   { "getsyncstat", GETSYNCSTAT },
		   { "reloadconfig", RELOADCONFIG },
		   { "getquota", GETQUOTA },
		   { "updatequota", TRIGGERUPDATEQUOTA },
		   { "changelog", CHANGELOG },
		   { "unpindirtysize", UNPINDIRTYSIZE },
		   { "occupiedsize", OCCUPIEDSIZE },
		   { "xferstatus", GETXFERSTATUS },
		   { "setnotifyserver", SETNOTIFYSERVER },
		   { "setswifttoken", SETSWIFTTOKEN },
		   { "setsyncpoint", SETSYNCPOINT },
		   { "cancelsyncpoint", CANCELSYNCPOINT },
		   { "initiate_restoration", INITIATE_RESTORATION },
		   { "check_restoration_status", CHECK_RESTORATION_STATUS },
		   { "notify_applist_change", NOTIFY_APPLIST_CHANGE },
		   { "toggle_use_minimal_apk", TOGGLE_USE_MINIMAL_APK },
		   { "get_minimal_apk_status", GET_MINIMAL_APK_STATUS },
		   { "high-pin", PIN},
		   { "pin", PIN},
		   { "unpin", UNPIN},
		   { "set_googledrive_token", SET_GOOGLEDRIVE_TOKEN},
		   { "set_upload_interval", SET_UPLOAD_INTERVAL},
		   { "isskipdex", ISSKIPDEX} };

enum { CMD_SIZE = sizeof(cmd_list) / sizeof(cmd_list[0]) };

#endif /* GW20_SRC_HCFSVOL_H_ */
