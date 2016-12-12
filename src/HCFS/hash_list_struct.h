/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hash_list_struct.h
* Abstract: The header file of a generic linked list in hash table struct.
*
* Revision History
* 2016/12/07 Kewei created this file and define data structure.
*
**************************************************************************/
#ifndef SRC_HCFS_HASH_LIST_STRUCT_H_
#define SRC_HCFS_HASH_LIST_STRUCT_H_

#include <inttypes.h>
#include <stdlib.h>
#include <semaphore.h>

/**
 * Structure of a node in the linked list. "key" and "data" should be
 * defined by user.
 */
typedef struct LIST_NODE {
	void *key;
	void *data;
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
typedef int32_t key_cmp_ftn_t(const void *, const void *);
typedef int32_t data_update_ftn_t(void *data, void *update_data);
typedef int32_t hash_ftn_t(const void *key);

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
			       const void *key,
			       const void *data);
int32_t lookup_hash_list_entry(HASH_LIST *hash_list,
			       const void *key,
			       void *data);
int32_t remove_hash_list_entry(HASH_LIST *hash_list, const void *key);
int32_t update_hash_list_entry(HASH_LIST *hash_list,
			       const void *key,
			       void *data,
			       void *update_data);

/* Global lock will lock whole hash table. */
void hash_list_global_lock(HASH_LIST *hash_list);
void hash_list_global_unlock(HASH_LIST *hash_list);

/*void hash_list_bucket_lock(HASH_LIST *hash_list, void *key);*/
/*void hash_list_bucket_unlock(HASH_LIST *void *key);*/
/*void *hash_list_get_entry_data(HASH_LIST *hash_listd *key)*/

#endif  /* SRC_HCFS_HASH_LIST_STRUCT_H_ */
