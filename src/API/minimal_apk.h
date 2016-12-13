/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: minimal_apk.h
* Abstract: This c header file for minimal apk APIs.
*
* Revision History
* 2016/12/12 Create this file.
*
**************************************************************************/

#ifndef GW20_HCFSAPI_MINI_APK_H_
#define GW20_HCFSAPI_MINI_APK_H_

#define _GNU_SOURCE
#include <inttypes.h>

#define LIB_DIR_PREFIX "lib/"

#define BASE_APK_NAME "base.apk"
#define MINI_APK_NAME ".basemin"

#define F_MANIFEST "AndroidManifest.xml"
#define F_RESOURCE "resources.arsc"

/* status of minimal apk */
#define ST_NOT_EXISTED 0
#define ST_EXISTED 1
#define ST_IS_CREATING 2

#define WRITE_LOG(level, msg, ...)                                             \
	do {                                                                   \
		write_log(level, "In %s. " msg, __func__, ##__VA_ARGS__);      \
	} while (0);


int32_t create_minimal_apk(char *pkg_name);

void *create_minimal_apk_async(void *ptr);

int32_t check_minimal_apk(char *pkg_name);

#endif  /* GW20_HCFSAPI_MINI_APK_H_ */
