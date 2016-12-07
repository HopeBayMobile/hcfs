/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt.c
* Abstract: The c file to control Android app pin/unpin status.
*
* Revision History
* 2016/12/07 Jethro created this file.
*
**************************************************************************/

#include "apk_mgmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>

#include "fuseop.h"
#include "global.h"
#include "hfuse_system.h"
#include "macro.h"
#include "params.h"
#include "utils.h"

int32_t toggle_use_minimal_apk(bool new_val)
{
	int32_t ret = 0;
	bool old_val = hcfs_system->use_minimal_apk;

	if (old_val == false && new_val == true)
		ret = initialize_minimal_apk();
	else if (old_val == true && new_val == false)
		ret = terminate_minimal_apk();

	return ret;
}

/**
 * Create hash table to store minimal apk, then enable system flag
 * `use_minimal_apk`
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t initialize_minimal_apk(void)
{
	create_minapk_table();

	/* Enable use_minimal_apk after everything ready */
	hcfs_system->use_minimal_apk = true;
	return 0;
}

/**
 * Disable flag `use_minimal_apk`, invalid each minimal apk's dentry on
 * system, remove them from hash list and delete hash table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t terminate_minimal_apk(void)
{
	/* Disable use_minimal_apk first, then destroy everything */
	hcfs_system->use_minimal_apk = false;

	/*
	 * TODO : iterate over hash table, invalid all minapk
	 * destroy_minapk_table();
	 */
	return 0;
}

static int32_t _minapk_hash(const void *key)
{
	return (((MIN_APK_LOOKUP_KEY *)key)->parent_ino % MINAPK_TABLE_SIZE);
}

static int32_t _minapk_cmp(const void *key1, const void *key2)
{
	MIN_APK_LOOKUP_KEY *k1 = (MIN_APK_LOOKUP_KEY *)key1;
	MIN_APK_LOOKUP_KEY *k2 = (MIN_APK_LOOKUP_KEY *)key2;

	if (k1->parent_ino == k2->parent_ino &&
	    !strncmp(k1->apk_name, k2->apk_name, MAX_FILENAME_LEN))
		return 0;
	else
		return -1;
}

/**
 * Create structure of minimal apk lookup table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t create_minapk_table(void)
{
	int32_t ret = 0;

	minapk_lookup_table = create_hash_list(_minapk_hash, _minapk_cmp, NULL,
		MINAPK_TABLE_SIZE, sizeof(MIN_APK_LOOKUP_KEY),
		sizeof(MINAPK_LOOKUP_DATA));
	if (!minapk_lookup_table) {
		ret = -errno;
		write_log(0, "Error: Fail to create min apk table."
				" Code %d", -ret);
	}

	return ret;
}

/**
 * Destroy structure of minimal apk lookup table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
void destroy_minapk_table(void)
{
	if (minapk_lookup_table)
		destroy_hash_list(minapk_lookup_table);
	minapk_lookup_table = NULL;
	return;
}

/**
 * Insert triple (parent inode, apk name, miminal apk inode) into lookup table.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 * @param minapk_ino Inode number of the minimal apk file.
 *
 * @return 0 on success, -EEXIST if key pair (parent inode, apk name) exists.
 */
int32_t insert_minapk_data(ino_t parent_ino,
			   const char *apk_name,
			   ino_t minapk_ino)
{
	MIN_APK_LOOKUP_KEY temp_key;
	MINAPK_LOOKUP_DATA temp_data = {.min_apk_ino = minapk_ino};
	int32_t ret;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret = insert_hash_list_entry(minapk_lookup_table,
			&temp_key, &temp_data);
	if (ret < 0 && ret != -EEXIST)
		write_log(2, "Fail to insert min apk data."
				" Code %d", -ret);

	return ret;
}

/**
 * Query the mini apk lookup table by pair (parent inode, apk name) and get the
 * inode # of minimal apk file.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 * @param minapk_ino Inode number of the minimal apk file.
 *
 * @return 0 on success, -ENOENT if key pair (parent inode, apk name)
 *         does not exist.
 */
int32_t query_minapk_data(ino_t parent_ino,
			  const char *apk_name,
			  ino_t *minapk_ino)
{
	MIN_APK_LOOKUP_KEY temp_key;
	MINAPK_LOOKUP_DATA temp_data;
	int32_t ret = 0;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret = lookup_hash_list_entry(minapk_lookup_table,
			&temp_key, &temp_data);
	if (ret < 0) {
		if (ret != -ENOENT)
			write_log(0, "Error: Fail to query minimal apk."
					" Code %d", -ret);
		goto out;
	}

	*minapk_ino = temp_data.min_apk_ino;
out:
	return ret;
}

/**
 * Query the mini apk lookup table by pair (parent inode, apk name) and remove
 * the entry from this lookup table.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 *
 * @return 0 on success, -ENOENT if key pair (parent inode, apk name)
 *         does not exist.
 */
int32_t remove_minapk_data(ino_t parent_ino, const char *apk_name)
{
	MIN_APK_LOOKUP_KEY temp_key;
	int32_t ret = 0;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret = remove_hash_list_entry(minapk_lookup_table, &temp_key);
	if (ret < 0 && ret != -ENOENT)
		write_log(0, "Error: Fail to remove min apk data."
				" Code %d", -ret);

	return ret;
}

