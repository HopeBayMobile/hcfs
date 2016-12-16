/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: HCFSvol.h
* Abstract: The header file for HCFSvol
*
* Revision History
* 2015/11/17 Jethro split header file to clean style warning
*
**************************************************************************/

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
		   { "unpin", UNPIN} };
enum { CMD_SIZE = sizeof(cmd_list) / sizeof(cmd_list[0]) };

#endif /* GW20_SRC_HCFSVOL_H_ */
