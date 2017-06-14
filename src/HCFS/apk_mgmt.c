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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "fuse_notify.h"
#include "fuseop.h"
#include "global.h"
#include "hash_list_struct.h"
#include "hfuse_system.h"
#include "macro.h"
#include "params.h"
#include "utils.h"
#include "FS_manager.h"

int32_t toggle_use_minimal_apk(bool new_val)
{
	int32_t ret = 0;
	bool old_val = hcfs_system->set_minimal_apk;

	if (old_val == new_val)
		return 0;

	hcfs_system->set_minimal_apk = new_val;
	ret = update_use_minimal_apk();

	return ret;
}

/* Take set_minimal_apk and sync_paused, and consider
whether to use minimal apk */
int32_t update_use_minimal_apk(void)
{
	int32_t ret = 0;
	bool old_val = hcfs_system->use_minimal_apk;
	bool new_val = (hcfs_system->set_minimal_apk ||
	                hcfs_system->sync_paused);

	if (old_val == new_val)
		return 0;

	ret = new_val ? initialize_minimal_apk() : terminate_minimal_apk();
	if (ret != 0)
		write_log(0, "[E] %s: use_minimal_apk failed to %s", __func__,
			  new_val ? "enable" : "disable");
	else
		write_log(4, "[I] %s: use_minimal_apk %s successful", __func__,
			  new_val ? "enable" : "disable");

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
	int32_t ret = 0;

	ret = create_minapk_table();

	/* Enable use_minimal_apk after everything ready */
	if (ret == 0)
		hcfs_system->use_minimal_apk = true;
	return ret;
}

int32_t _invalid_all_minapk(void)
{
	int32_t ret = 0;
	ino_t parent_ino, minapk_ino;
	char apk_name[MAX_FILENAME_LEN] = { 0 };
	MOUNT_T *mount_info = NULL;
	size_t apk_name_len;

	ret = init_iterate_minapk_table();
	if (ret < 0)
		return ret;

	do {
		ret = search_mount(APP_VOL_NAME, NULL, &mount_info);
		if (ret < 0 || mount_info == NULL) {
			write_log(0, "[E] %s: Volume %s not found.\n", __func__,
				  APP_VOL_NAME);
			break;
		}
		while ((ret = iterate_minapk_table(&parent_ino, apk_name,
						   &minapk_ino)) == 0) {
			apk_name_len = strnlen(apk_name, MAX_FILENAME_LEN - 1);
			hfuse_ll_notify_inval_ent(mount_info->chan_ptr,
						  parent_ino, apk_name,
						  apk_name_len);
		}
		if (ret == -ENOENT)
			ret = 0;
	} while (0);

	end_iterate_minapk_table();
	return ret;
}
/**
 * Disable flag `use_minimal_apk`, invalid each minimal apk's dentry on
 * system, remove them from hash list and delete hash table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t terminate_minimal_apk(void)
{
	int32_t ret = 0;

	/* Disable use_minimal_apk first, then destroy everything */
	hcfs_system->use_minimal_apk = false;

	ret = _invalid_all_minapk();
	if (ret < 0) {
		write_log(0, "[E] %s: Fail to invalid minapk dentries, %s\n",
			  __func__, "This can lead to unexpected behavior.");
	}

	destroy_minapk_table();
	return ret;
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

static int32_t _minapk_update(const void *target, const void *update)
{
	MIN_APK_LOOKUP_DATA *t1 = (MIN_APK_LOOKUP_DATA *)target;
	MIN_APK_LOOKUP_DATA *u1 = (MIN_APK_LOOKUP_DATA *)update;

	memcpy(t1, u1, sizeof(MIN_APK_LOOKUP_DATA));
	return 0;
}

/**
 * Create structure of minimal apk lookup table.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t create_minapk_table(void)
{
	int32_t ret = 0;

	minapk_lookup_table = create_hash_list(
	    _minapk_hash, _minapk_cmp, _minapk_update, MINAPK_TABLE_SIZE,
	    sizeof(MIN_APK_LOOKUP_KEY), sizeof(MIN_APK_LOOKUP_DATA));
	if (!minapk_lookup_table) {
		ret = -errno;
		write_log(0, "Error: Fail to create min apk table. Code %d\n",
			  -ret);
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
}

/**
 * Insert triple (parent inode, apk name, miminal apk data) into lookup table.
 *
 * @param parent_ino Parent inode number.
 * @param apk_name Apk file name.
 * @param minapk_data Lookup data of the minimal apk file.
 *
 * @return 0 on success, -EEXIST if key pair (parent inode, apk name) exists.
 */
int32_t insert_minapk_data(ino_t parent_ino,
			   const char *apk_name,
			   MIN_APK_LOOKUP_DATA *minapk_data)
{
	MIN_APK_LOOKUP_KEY temp_key;
	int32_t ret;

	if (!minapk_lookup_table)
		return -EINVAL;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret =
	    insert_hash_list_entry(minapk_lookup_table, &temp_key, minapk_data);
	if (ret < 0 && ret != -EEXIST)
		write_log(2, "Fail to insert min apk data. Code %d\n", -ret);

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
	MIN_APK_LOOKUP_DATA temp_data;
	int32_t ret = 0;

	if (!minapk_lookup_table)
		return -EINVAL;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret =
	    lookup_hash_list_entry(minapk_lookup_table, &temp_key, &temp_data);
	if (ret < 0) {
		if (ret != -ENOENT)
			write_log(0,
				  "Error: Fail to query minimal apk. Code %d\n",
				  -ret);
		goto out;
	}

	/* TODO: might want to move this update to cache replacement if
	we can easily determine whether the inode to be swapped out is an
	apk */
	/* Should check if need to update value of is_complete_apk here */
	if ((temp_data.is_complete_apk == true) &&
	    (temp_data.min_apk_ino > 0)) {
		/* Check if apk is still local */
		ret = check_data_location(temp_data.org_apk_ino);

		if (ret < 0) {
			/* Cannot query location. Remove from table */
			remove_hash_list_entry(minapk_lookup_table, &temp_key);
			write_log(0, "Error checking apk location. Code %d\n",
			          -ret);
			goto out;
		}
		if (ret > 0) {
			/* Apk no longer local. Update table */
			ret = 0;
			temp_data.is_complete_apk = false;
			update_hash_list_entry(minapk_lookup_table, &temp_key,
			                       NULL, &temp_data);
		}
	}

	if (temp_data.is_complete_apk == true)
		*minapk_ino = 0;
	else
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

	if (!minapk_lookup_table)
		return -EINVAL;

	temp_key.parent_ino = parent_ino;
	strncpy(temp_key.apk_name, apk_name, MAX_FILENAME_LEN);
	ret = remove_hash_list_entry(minapk_lookup_table, &temp_key);
	if (ret < 0 && ret != -ENOENT)
		write_log(0, "Error: Fail to remove min apk data. Code %d\n",
			  -ret);

	return ret;
}

/**
 * Initialize an iterator for min apk lookup table. It locks whole hash
 * table before init an iterator.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t init_iterate_minapk_table(void)
{
	HASH_LIST_ITERATOR *iter;
	int32_t ret = 0;

	if (!minapk_lookup_table) {
		ret = -EINVAL;
		goto out;
	}

	hash_list_global_lock(minapk_lookup_table);
	iter = init_hashlist_iter(minapk_lookup_table);
	if (!iter) {
		ret = -errno;
		goto out;
	}
	minapk_lookup_iter = iter;

out:
	return ret;
}

/**
 * Wrap the iterator function and get key/data of min apk lookup table.
 *
 * @param parent_ino Pointer of parent inode number.
 * @param apk_name Pointer of apk name.
 * @param minapk_ino Pointer of min apk inode.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t iterate_minapk_table(ino_t *parent_ino,
			     char *apk_name,
			     ino_t *minapk_ino)
{
	HASH_LIST_ITERATOR *iter;
	MIN_APK_LOOKUP_KEY *key;
	MIN_APK_LOOKUP_DATA *data;
	int32_t ret = 0;

	if (!(minapk_lookup_table && minapk_lookup_iter && parent_ino &&
	      apk_name && minapk_ino)) {
		ret = -EINVAL;
		goto out;
	}

	/* lookup table has been locked. Just iterate all entries. */
	iter = iter_next(minapk_lookup_iter);
	if (!iter) {
		ret = -errno;
		goto out;
	}

	key = (MIN_APK_LOOKUP_KEY *)(iter->now_key);
	data = (MIN_APK_LOOKUP_DATA *)(iter->now_data);

	if (key && data) {
		*parent_ino = key->parent_ino;
		strncpy(apk_name, key->apk_name, MAX_FILENAME_LEN);
		*minapk_ino = data->min_apk_ino;
	} else {
		write_log(0, "Error: Data or key in the entry is null.");
		ret = -ENOMEM;
	}

out:
	return ret;
}

/**
 * Destroy the iterator of min apk lookup table.
 *
 * @return none.
 */
void end_iterate_minapk_table(void)
{
	if (!minapk_lookup_table || !minapk_lookup_iter) {
		write_log(4, "Minimal apk lookup table is invalid");
		return;
	}

	destroy_hashlist_iter(minapk_lookup_iter);
	minapk_lookup_iter = NULL;
	hash_list_global_unlock(minapk_lookup_table);
}
