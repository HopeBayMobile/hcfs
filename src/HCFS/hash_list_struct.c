#include <inttypes.h>
#include <stdlib.h>

HASH_LIST *create_hash_list(int32_t (*hash_ftn)(void *key),
		int32_t (*key_cmp_ftn)(const void *key1, const void *key2),
		int32_t (*data_update_ftn)(void *data),
		int32_t table_size)
{
	HASH_LIST *hash_list = NULL;

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
		hahs_list = NULL;
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
	hash_list->hash_ftn = hash_ftn;
	hash_list->key_cmp_ftn = key_cmp_ftn;
	hash_list->data_update_ftn = data_update_ftn;
	hash_list->table_size = table_size;

out:
	if (!hash_list)
		write_log(0, "Error: Fail to create hash list. Code %d", errno);
	return hash_list;
}
