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
#ifndef SRC_HCFS_HASH_LIST_STRUCT_H_
#define SRC_HCFS_HASH_LIST_STRUCT_H_

#include <inttypes.h>
#include <stdlib.h>
#include <semaphore.h>

/**
 * Structure of a node in the linked list. "key" and "data" should be
 * defined by user.
 */
typedef void hash_key_t;
typedef void hash_data_t;
typedef void data_update_t;

typedef struct LIST_NODE {
	hash_key_t *key;
	hash_data_t *data;
	struct LIST_NODE *next;
} LIST_NODE;

/**
 * Structure of the header of linked list.
 */
typedef struct LIST_HEAD {
	LIST_NODE *first_entry;
	int32_t num_entries;
	sem_t bucket_sem;
} LIST_HEAD;

/**
 * Structure of the hash table. For each hash bucket, it points to a
 * linked list header. User should at least define both "hash_ftn" and
 * "key_cmp_ftn", which are utilized to hash some key to a bucket and
 * compare between keys when querying, respectively. "data_update_ftn"
 * can be null if entry will not be modified after insertion.
 *
 * When a entry is insertion, lookup, update, or removal, the bucket of
 * that entry belonging to will be locked instead of locking all hash table.
 * When using "global lock", it means all buckets in the hash table can not
 * be accessed.
 */
typedef int32_t key_cmp_ftn_t(const hash_key_t *, const hash_key_t *);
typedef int32_t data_update_ftn_t(hash_data_t *data,
				  data_update_t *update_data);
typedef int32_t hash_ftn_t(const hash_key_t *key);

typedef struct HASH_LIST {
	uint32_t table_size;
	uint32_t key_size;
	uint32_t data_size;
	hash_ftn_t *hash_ftn;
	key_cmp_ftn_t *key_cmp_ftn;
	data_update_ftn_t *data_update_ftn;
	LIST_HEAD *hash_table;
	sem_t table_sem;
	uint32_t num_lock_bucket;
	sem_t shared_var_sem;
	sem_t can_lock_table_sem;
} HASH_LIST;

HASH_LIST *create_hash_list(hash_ftn_t *hash_ftn,
			    key_cmp_ftn_t *key_cmp_ftn,
			    data_update_ftn_t *data_update_ftn,
			    uint32_t table_size,
			    uint32_t key_size,
			    uint32_t data_size);

void destroy_hash_list(HASH_LIST *hash_list);

/* Following operations will lock the bucket "key" belonging to. */
int32_t insert_hash_list_entry(HASH_LIST *hash_list,
			       const hash_key_t *key,
			       const hash_data_t *data);
int32_t lookup_hash_list_entry(HASH_LIST *hash_list,
			       const hash_key_t *key,
			       hash_data_t *data);
int32_t remove_hash_list_entry(HASH_LIST *hash_list, const hash_key_t *key);
int32_t update_hash_list_entry(HASH_LIST *hash_list,
			       const hash_key_t *key,
			       hash_data_t *data,
			       data_update_t *update_data);

/* Global lock will lock whole hash table. */
void hash_list_global_lock(HASH_LIST *hash_list);
void hash_list_global_unlock(HASH_LIST *hash_list);

/*void hash_list_bucket_lock(HASH_LIST *hash_list, void *key);*/
/*void hash_list_bucket_unlock(HASH_LIST *void *key);*/
/*void *hash_list_get_entry_data(HASH_LIST *hash_listd *key)*/

#endif  /* SRC_HCFS_HASH_LIST_STRUCT_H_ */
