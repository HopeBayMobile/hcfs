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

#ifndef SRC_HCFS_APK_MGMT_H_
#define SRC_HCFS_APK_MGMT_H_

#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>

#include "meta_iterator.h"
#include "params.h"

int32_t toggle_use_minimal_apk(bool new_val);

int32_t update_use_minimal_apk(void);

int32_t initialize_minimal_apk(void);
int32_t terminate_minimal_apk(void);

#define MINAPK_TABLE_SIZE 128
typedef struct MIN_APK_LOOKUP_KEY {
	ino_t parent_ino;
	char apk_name[MAX_FILENAME_LEN];
} MIN_APK_LOOKUP_KEY;

typedef struct MIN_APK_LOOKUP_DATA {
	ino_t min_apk_ino;
	ino_t org_apk_ino;
	bool is_complete_apk;
} MIN_APK_LOOKUP_DATA;

HASH_LIST *minapk_lookup_table;
HASH_LIST_ITERATOR *minapk_lookup_iter;

int32_t create_minapk_table(void);
void destroy_minapk_table(void);
int32_t insert_minapk_data(ino_t parent_ino,
			   const char *apk_name,
			   MIN_APK_LOOKUP_DATA *minapk_data);
int32_t query_minapk_data(ino_t parent_ino,
			  const char *apk_name,
			  ino_t *minapk_ino);
int32_t remove_minapk_data(ino_t parent_ino, const char *apk_name);

/**
 * // Traverse all entries of the minapk table
 *
 * ino_t parent_ino, minapk_ino;
 * char apk_name[MAX_FILENAME_LEN];
 *
 * ret = init_iterate_minapk_table();
 * if (ret < 0)
 * 	// Init failed :(
 * while ((ret =
 *	   iterate_minapk_table(&parent_ino, apk_name, &minapk_ino)) == 0) {
 * 	//do something...
 * }
 * end_iterate_minapk_table();
 * // Check ret
 */
int32_t init_iterate_minapk_table(void);
int32_t iterate_minapk_table(ino_t *parent_ino,
			     char *apk_name,
			     ino_t *minapk_ino);
void end_iterate_minapk_table(void);

#endif /* SRC_HCFS_APK_MGMT_H_ */
