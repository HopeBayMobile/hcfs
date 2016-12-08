/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt.h
* Abstract: The header file to control Android app pin/unpin status.
*
* Revision History
* 2016/12/07 Jethro created this file.
*
**************************************************************************/

#ifndef SRC_HCFS_APK_MGMT_H_
#define SRC_HCFS_APK_MGMT_H_

#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>

#include "params.h"
//#include "hash_list_struct.h"
#include "meta_iterator.h"

int32_t toggle_use_minimal_apk(bool new_val);
int32_t initialize_minimal_apk(void);
int32_t terminate_minimal_apk(void);

#define MINAPK_TABLE_SIZE 128
typedef struct MIN_APK_LOOKUP_KEY {
	ino_t parent_ino;
	char apk_name[MAX_FILENAME_LEN];
} MIN_APK_LOOKUP_KEY;

typedef struct MIN_APK_LOOKUP_DATA {
	ino_t min_apk_ino;
} MIN_APK_LOOKUP_DATA;

HASH_LIST *minapk_lookup_table;
HASH_LIST_ITERATOR *minapk_lookup_iter;

int32_t create_minapk_table(void);
void destroy_minapk_table(void);
int32_t insert_minapk_data(ino_t parent_ino, const char *apk_name,
		ino_t minapk_ino);
int32_t query_minapk_data(ino_t parent_ino, const char *apk_name,
		ino_t *minapk_ino);
int32_t remove_minapk_data(ino_t parent_ino, const char *apk_name);

/**
 * Traverse all entries of the minapk table
 *
 * ino_t parent_ino, minapk_ino;
 * char apk_name[MAX_FILENAME_LEN];
 *
 * ret = init_iterate_minapk_table();
 * if (ret < 0)
 * 	// Init failed :(
 * while (iterate_minapk_table(&parent_ino, apk_name, &minapk_ino) != -ENOENT) {
 * 	//do something...
 * }
 * ret = end_iterate_minapk_table();
 *
 */
int32_t init_iterate_minapk_table(void);
int32_t iterate_minapk_table(ino_t *parent_ino, char *apk_name,
		ino_t *minapk_ino);
void end_iterate_minapk_table(void);

#endif  /* SRC_HCFS_APK_MGMT_H_ */
