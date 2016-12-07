#include <inttypes.h>
#include <stdlib.h>
#include <semaphore.h>

typedef struct LIST_NODE {
	void *key;
	void *data;
	struct LIST_NODE *next;
} LIST_NODE;

typedef struct LIST_HEAD {
	LIST_NODE *first_entry;
	int32_t num_entries;
	sem_t bucket_sem;
} LIST_HEAD;

typedef struct HASH_LIST {
	uint32_t table_size;
	uint32_t key_size;
	uint32_t data_size;
	int32_t (*hash_ftn)(const void *key);
	int32_t (*key_cmp_ftn)(const void *key1, const void *key2);
	int32_t (*data_update_ftn)(void *data, void *update_data);
	LIST_HEAD *hash_table;
	sem_t table_sem;
} HASH_LIST;

HASH_LIST *create_hash_list(int32_t (*hash_ftn)(const void *key),
		int32_t (*key_cmp_ftn)(const void *key1, const void *key2),
		int32_t (*data_update_ftn)(void *data, void *update_data),
		uint32_t table_size, uint32_t key_size, uint32_t data_size);
int32_t insert_hash_list_entry(HASH_LIST *hash_list, void *key, void *data);
int32_t lookup_hash_list_entry(HASH_LIST *hash_list, void *key, void *data);
int32_t remove_hash_list_entry(HASH_LIST *hash_list, void *key);
int32_t update_hash_list_entry(HASH_LIST *hash_list, void *key, void *update_data);
void destroy_hash_list(HASH_LIST *hash_list);

void hash_list_lock(HASH_LIST *hash_list);
void hash_list_unlock(HASH_LIST *hash_list);

//void hash_list_bucket_lock(HASH_LIST *hash_list, void *key);
//void hash_list_bucket_unlock(HASH_LIST *void *key);
//void *hash_list_get_entry_data(HASH_LIST *hash_listd *key)
