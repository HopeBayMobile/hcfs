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
		errno = 0;                                                     \
		grp_t = getgrnam(OWNER);                                       \
		if (!grp_t) {                                                  \
			write_log(0, "Error on getting gr. Code %d", errno);   \
			break;                                                 \
		}                                                              \
		errno = 0;                                                     \
		passwd_t = getpwnam(GROUP);                                    \
		if (!passwd_t) {                                               \
			write_log(0, "Error on getting pw. Code %d", errno);   \
			break;                                                 \
		}                                                              \
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
