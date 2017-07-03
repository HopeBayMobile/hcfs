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
#include <semaphore.h>

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

#define CHOWN(PATH, OWNER, GROUP)                                              \
	do {                                                                   \
		grp_t = getgrnam(OWNER);                                       \
		passwd_t = getpwnam(GROUP);                                    \
		chown(PATH, passwd_t->pw_uid, grp_t->gr_gid);                  \
	} while (0)

#define UTIME(PATH, TIMBUF, STAT)                                              \
	do {                                                                   \
		TIMBUF.actime = STAT.st_atime;                                 \
		TIMBUF.modtime = STAT.st_mtime;                                \
		ret_code = utime(PATH, &TIMBUF);                               \
		if (ret_code < 0) {                                            \
			WRITE_LOG(0, "Failed to change mod time for %s. "      \
				     "Error code - %d",                        \
				  PATH, errno);                                \
			unlink(mini_apk_path);                                 \
			goto error;                                            \
		}                                                              \
	} while (0)

/* List to store packages whose mini apk creation are ongoing */
typedef struct ll_ongoing_mini_apk {
	char *pkg_name;
	struct ll_ongoing_mini_apk *next;
} LL_ONGOING_MINI_APK;

typedef struct mini_apk_needed {
	char *pkg_name;
	char **reserved_icon_names;
	int32_t num_icon;
} MINI_APK_NEEDED;

extern LL_ONGOING_MINI_APK *ongoing_mini_apk_list;
extern sem_t mini_apk_list_sem;

void init_minimal_apk_list();

void destroy_minimal_apk_list();

int32_t create_minimal_apk(MINI_APK_NEEDED *min_apk_needed);

void *create_minimal_apk_async(void *ptr);

int32_t check_minimal_apk(char *pkg_name);

MINI_APK_NEEDED *create_min_apk_needed_data(char *buf);
void destroy_min_apk_needed_data(MINI_APK_NEEDED *min_apk_needed);
#endif  /* GW20_HCFSAPI_MINI_APK_H_ */
