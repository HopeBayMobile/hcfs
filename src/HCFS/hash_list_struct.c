
#include "hash_list_struct.h"

#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "global.h"
#include "macro.h"

HASH_LIST *create_hash_list(int32_t (*hash_ftn)(const void *key),
		int32_t (*key_cmp_ftn)(const void *key1, const void *key2),
		int32_t (*data_update_ftn)(void *data, void *update_data),
		uint32_t table_size, uint32_t key_size, uint32_t data_size)
{
	HASH_LIST *hash_list = NULL;
	int32_t ret;
	uint32_t idx;

	if (table_size <= 0 || hash_ftn == NULL || key_cmp_ftn == NULL) {
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

	ret = sem_init(&(hash_list->table_sem), 0, 1);
	if (ret < 0) {
		free(hash_list->hash_table);
		free(hash_list);
		hash_list = NULL;
		goto out;
	}
	for (idx = 0; idx < table_size; idx++) {
		ret = sem_init(&(hash_list->hash_table->bucket_sem), 0, 1);
		if (ret < 0) {
			free(hash_list->hash_table);
			free(hash_list);
			hash_list = NULL;
			goto out;
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

int32_t insert_hash_list_entry(HASH_LIST *hash_list, void *key, void *data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	LIST_NODE *new_entry;
	int32_t hash_idx;
	int32_t ret = 0;
	BOOL hit;

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	sem_wait(&list_head->bucket_sem);
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
		sem_post(&list_head->bucket_sem);
		goto out;
	}

	/* Create node and insert */
	new_entry = calloc(sizeof(LIST_NODE), 1);
	if (!new_entry) {
		sem_post(&list_head->bucket_sem);
		ret = -ENOMEM;
		goto out;
	}
	new_entry->key = calloc(hash_list->key_size, 1);
	if (!(new_entry->key)) {
		sem_post(&list_head->bucket_sem);
		free(new_entry);
		ret = -ENOMEM;
		goto out;
	}
	new_entry->data = calloc(hash_list->data_size, 1);
	if (!(new_entry->data)) {
		sem_post(&list_head->bucket_sem);
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
	sem_post(&list_head->bucket_sem);
out:
	return ret;

}

int32_t lookup_hash_list_entry(HASH_LIST *hash_list, void *key, void *data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	int32_t hash_idx;
	int32_t ret = 0;
	void *ret_data = NULL;

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	sem_wait(&list_head->bucket_sem);
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
		sem_post(&list_head->bucket_sem);
		goto out;
	}
	memcpy(data, ret_data, hash_list->data_size);
	sem_post(&list_head->bucket_sem);
out:
	return ret;
}

int32_t remove_hash_list_entry(HASH_LIST *hash_list, void *key)
{
	LIST_HEAD *list_head;
	LIST_NODE *now, *prev, *hit_node = NULL;
	int32_t hash_idx;
	int32_t ret = 0;

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	sem_wait(&list_head->bucket_sem);
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
		sem_post(&list_head->bucket_sem);
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
	sem_post(&list_head->bucket_sem);
out:
	return ret;
}

int32_t update_hash_list_entry(HASH_LIST *hash_list, void *key,
		void *update_data)
{
	LIST_HEAD *list_head;
	LIST_NODE *now;
	int32_t hash_idx;
	int32_t ret = 0;
	void *hit_data = NULL;

	if (!(hash_list->data_update_ftn))
		return -ENOSYS; /* Not implemented */

	hash_idx = hash_list->hash_ftn(key);
	if (hash_idx < 0 || (uint32_t)hash_idx >= hash_list->table_size) {
		ret = -EINVAL;
		goto out;
	}
	list_head = &(hash_list->hash_table[hash_idx]);

	/* Lock bucket */
	sem_wait(&list_head->bucket_sem);
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
		sem_post(&list_head->bucket_sem);
		goto out;
	}

	/* Update data */
	ret = hash_list->data_update_ftn(hit_data, update_data);
	sem_post(&list_head->bucket_sem);
out:
	return ret;
}

void destroy_hash_list(HASH_LIST *hash_list)
{
	uint32_t idx;
	LIST_NODE *now, *next;

	sem_wait(&hash_list->table_sem);

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

	sem_post(&hash_list->table_sem);
	FREE(hash_list->hash_table);
	FREE(hash_list);

	return;
}

void hash_list_lock(HASH_LIST *hash_list)
{
	sem_wait(&hash_list->table_sem);
}

void hash_list_unlock(HASH_LIST *hash_list)
{
	sem_post(&hash_list->table_sem);
}
