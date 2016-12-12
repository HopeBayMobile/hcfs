/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hash_list_struct.h
* Abstract: The source file of a generic linked list in hash table struct.
*
* Revision History
* 2016/12/07 Kewei created this file and define data structure.
*
**************************************************************************/

#include "hash_list_struct.h"

#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "global.h"
#include "macro.h"

/**
 * Globally lock the hash table
 *
 * @param hash_list hash list table structure.
 *
 * @return none.
 */
void hash_list_global_lock(HASH_LIST *hash_list)
{
	sem_wait(&hash_list->table_sem);
	sem_wait(&hash_list->can_lock_table_sem);
}

/**
 * Unlock the global lock of hash table
 *
 * @param hash_list hash list table structure.
 *
 * @return none.
 */
void hash_list_global_unlock(HASH_LIST *hash_list)
{
	sem_post(&hash_list->can_lock_table_sem);
	sem_post(&hash_list->table_sem);
}

/**
 * Partially lock the bucket using "bucket_sem".
 *
 * @param hash_list hash list table structure.
 * @param bucket_sem Semaphore of the bucket.
 *
 * @return none.
 */
static void _bucket_lock(HASH_LIST *hash_list, sem_t *bucket_sem)
{
	/* Lock bucket */
	sem_wait(bucket_sem);

	sem_wait(&hash_list->table_sem);

	/* update num_lock_bucket */
	sem_wait(&hash_list->shared_var_sem);
	if (hash_list->num_lock_bucket == 0)
		/* Confiscate the global lock */
		sem_wait(&hash_list->can_lock_table_sem);
	hash_list->num_lock_bucket += 1;
	sem_post(&hash_list->shared_var_sem);

	sem_post(&hash_list->table_sem);
}

/**
 * Unlock the partial lock "bucket_sem".
 *
 * @param hash_list hash list table structure.
 * @param bucket_sem Semaphore of the bucket.
 *
 * @return none.
 */
static void _bucket_unlock(HASH_LIST *hash_list, sem_t *bucket_sem)
{
	/* update num_lock_bucket */
	sem_wait(&hash_list->shared_var_sem);
	hash_list->num_lock_bucket -= 1;
	if (hash_list->num_lock_bucket == 0)
		/* Release the global lock */
		sem_post(&hash_list->can_lock_table_sem);
	sem_post(&hash_list->shared_var_sem);

	/* Unlock bucket */
	sem_post(bucket_sem);
}

/**
 * Create a hash list data structure.
 *
 * @param hash_ftn Hash function. It cannot be null.
 * @param key_cmp_ftn Key compare function. Return 0 when hitting.
 *        It cannot be null.
 * @param data_update_ftn Function used to update data in entry. It can be
 *        skipped if hash table entry will not be updated.
 * @param table_size Number of buckets in the hash table.
 * @param key_size Size of user's "key" structure.
 * @param data_size Size of user's "data" structure.
 *
 * @return hash_list structure on success, otherwise null. errno is set to
 *         indicate the error.
 */
HASH_LIST *create_hash_list(hash_ftn_t *hash_ftn,
			    key_cmp_ftn_t *key_cmp_ftn,
			    data_update_ftn_t *data_update_ftn,
			    uint32_t table_size,
			    uint32_t key_size,
			    uint32_t data_size)
{
	HASH_LIST *hash_list = NULL;
	int32_t ret, ret1, ret2, ret3;
	uint32_t idx;

	if (table_size <= 0 || key_size <= 0 || data_size <= 0 ||
	    hash_ftn == NULL || key_cmp_ftn == NULL) {
		errno = EINVAL;
		goto out;
	}

	hash_list = (HASH_LIST *) calloc(sizeof(HASH_LIST), 1);
	if (!hash_list) {
		errno = ENOMEM;
		goto out;
	}

	hash_list->hash_table = (LIST_HEAD *)
			calloc(sizeof(LIST_HEAD), table_size);
	if (!(hash_list->hash_table)) {
		free(hash_list);
		hash_list = NULL;
		errno = ENOMEM;
		goto out;
	}

	ret1 = sem_init(&(hash_list->table_sem), 0, 1);
	ret2 = sem_init(&hash_list->shared_var_sem, 0, 1);
	ret3 = sem_init(&hash_list->can_lock_table_sem, 0, 1);
	if (ret1 || ret2 || ret3) {
		free(hash_list->hash_table);
		free(hash_list);
		hash_list = NULL;
		goto out; /* errno is set */
	}

	for (idx = 0; idx < table_size; idx++) {
		ret = sem_init(&(hash_list->hash_table[idx].bucket_sem), 0, 1);
		if (ret < 0) {
			free(hash_list->hash_table);
			free(hash_list);
			hash_list = NULL;
			goto out; /* errno is set */
		}
	}
	hash_list->table_size = table_size;
	hash_list->key_size = key_size;
	hash_list->data_size = data_size;
	hash_list->hash_ftn = hash_ftn;
	hash_list->key_cmp_ftn = key_cmp_ftn;
	hash_list->data_update_ftn = data_update_ftn;

out:
	if (!hash_list)
		write_log(0, "Error: Fail to create hash list. Code %d", errno);
	return hash_list;
}

/**
 * Insert an entry with data pair(key, data) into the hash list structure.
 *
 * @param hash_list Pointer of hash list structure.
 * @param key Pointer of key put in the new entry.
 * @param data Pointer of data put in the new entry.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t insert_hash_list_entry(HASH_LIST *hash_list, void *key, void *data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	LIST_NODE *new_entry;
	int32_t hash_idx;
	int32_t ret = 0;
	BOOL hit;

	if (!(hash_list && key && data)) {
		ret = -EINVAL;
		goto out;
	}

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	_bucket_lock(hash_list, &list_head->bucket_sem);
	now = list_head->first_entry;
	hit = FALSE;
	while (now) {
		if (hash_list->key_cmp_ftn(key, now->key) == 0) {
			/* Hit entry */
			hit = TRUE;
			break;
		}
		now = now->next;
	}
	if (hit) { /* Entry found. Do not insert */
		ret = -EEXIST;
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		goto out;
	}

	/* Create node and insert */
	new_entry = calloc(sizeof(LIST_NODE), 1);
	if (!new_entry) {
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		ret = -ENOMEM;
		goto out;
	}
	new_entry->key = calloc(hash_list->key_size, 1);
	if (!(new_entry->key)) {
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		free(new_entry);
		ret = -ENOMEM;
		goto out;
	}
	new_entry->data = calloc(hash_list->data_size, 1);
	if (!(new_entry->data)) {
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		free(new_entry->key);
		free(new_entry);
		ret = -ENOMEM;
		goto out;
	}
	memcpy(new_entry->key, key, hash_list->key_size);
	memcpy(new_entry->data, data, hash_list->data_size);

	new_entry->next = list_head->first_entry;
	list_head->first_entry = new_entry;
	list_head->num_entries += 1;
	_bucket_unlock(hash_list, &list_head->bucket_sem);


out:
	if (ret < 0)
		write_log(2, "Error in %s. Code %d", __func__, -ret);
	return ret;

}

/**
 * Query the hash list and get data using parameter "key".
 *
 * @param hash_list Pointer of hash list structure.
 * @param key Pointer of key.
 * @param data Pointer of data.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t lookup_hash_list_entry(HASH_LIST *hash_list, void *key, void *data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	int32_t hash_idx;
	int32_t ret = 0;
	void *ret_data = NULL;

	if (!(hash_list && key && data)) {
		ret = -EINVAL;
		goto out;
	}

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	_bucket_lock(hash_list, &list_head->bucket_sem);
	now = list_head->first_entry;
	while (now) {
		if (hash_list->key_cmp_ftn(key, now->key) == 0) {
			/* Hit entry */
			ret_data = now->data;
			break;
		}
		now = now->next;
	}
	if (!ret_data) {
		ret = -ENOENT;
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		goto out;
	}
	memcpy(data, ret_data, hash_list->data_size);
	_bucket_unlock(hash_list, &list_head->bucket_sem);

out:
	if (ret < 0)
		write_log(2, "Error in %s. Code %d", __func__, -ret);
	return ret;
}

/**
 * Remove an entry in hash list.
 *
 * @param hash_list Pointer of hash list structure.
 * @param key Pointer of key.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t remove_hash_list_entry(HASH_LIST *hash_list, void *key)
{
	LIST_HEAD *list_head;
	LIST_NODE *now, *prev, *hit_node = NULL;
	int32_t hash_idx;
	int32_t ret = 0;

	if (!(hash_list && key)) {
		ret = -EINVAL;
		goto out;
	}

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	_bucket_lock(hash_list, &list_head->bucket_sem);
	hit_node = NULL;
	prev = NULL;
	now = list_head->first_entry;
	while (now) {
		if (hash_list->key_cmp_ftn(key, now->key) == 0) {
			/* Hit entry */
			hit_node = now;
			break;
		}
		prev = now;
		now = now->next;
	}
	if (!hit_node) {
		ret = -ENOENT;
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		goto out;
	}

	/* Remove entry */
	if (prev)
		prev->next = hit_node->next;
	else
		list_head->first_entry = hit_node->next;
	list_head->num_entries -= 1;

	free(hit_node->key);
	free(hit_node->data);
	free(hit_node);
	_bucket_unlock(hash_list, &list_head->bucket_sem);

out:
	if (ret < 0)
		write_log(2, "Error in %s. Code %d", __func__, -ret);
	return ret;
}

/**
 * Update an entry in this hash list using "update_data".
 *
 * @param hash_list Pointer of hash list structure.
 * @param key Pointer of key.
 * @param update_data Pointer of the needed data used to update entry.
 *
 * @return 0 on success, otherwise negation of error code.
 */
int32_t update_hash_list_entry(HASH_LIST *hash_list, void *key,
		void *update_data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	int32_t hash_idx;
	int32_t ret = 0;
	void *hit_data = NULL;

	if (!(hash_list && key && update_data)) {
		ret = -EINVAL;
		goto out;
	}

	if (!(hash_list->data_update_ftn)) {
		ret = -ENOSYS; /* Not implemented */
		goto out;
	}

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	_bucket_lock(hash_list, &list_head->bucket_sem);
	now = list_head->first_entry;
	while (now) {
		if (hash_list->key_cmp_ftn(key, now->key) == 0) {
			/* Hit entry */
			hit_data = now->data;
			break;
		}
		now = now->next;
	}
	if (!hit_data) {
		ret = -ENOENT;
		_bucket_unlock(hash_list, &list_head->bucket_sem);
		goto out;
	}

	/* Update data */
	ret = hash_list->data_update_ftn(hit_data, update_data);
	_bucket_unlock(hash_list, &list_head->bucket_sem);

out:
	if (ret < 0)
		write_log(2, "Error in %s. Code %d", __func__, -ret);
	return ret;
}

/**
 * Destroy the hash list and free all entries.
 *
 * @param hash_list Hash list to be destroyed.
 *
 * @return none.
 */
void destroy_hash_list(HASH_LIST *hash_list)
{
	uint32_t idx;
	LIST_NODE *now, *next;

	if (!hash_list) {
		write_log(4, "Destroy null pointer in %s", __func__);
		goto out;
	}

	hash_list_global_lock(hash_list);

	for (idx = 0; idx < hash_list->table_size; idx++) {
		now = hash_list->hash_table[idx].first_entry;
		while (now) {
			next = now->next;
			free(now->key);
			free(now->data);
			free(now);
			now = next;
		}
		sem_destroy(&(hash_list->hash_table[idx].bucket_sem));
		hash_list->hash_table[idx].first_entry = NULL;
	}

	hash_list_global_unlock(hash_list);
	sem_destroy(&hash_list->table_sem);
	sem_destroy(&hash_list->shared_var_sem);
	sem_destroy(&hash_list->can_lock_table_sem);
	FREE(hash_list->hash_table);
	FREE(hash_list);

out:
	return;
}

