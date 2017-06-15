#include "apk_mgmt.h"
#include "fuseop.h"
#include "hfuse_system.h"
#include "mount_manager.h"
#include <errno.h>

#include "../../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int32_t, search_mount, char *, char *, MOUNT_T **);
FAKE_VALUE_FUNC(int32_t,
		hfuse_ll_notify_inval_ent,
		struct fuse_chan *,
		fuse_ino_t,
		const char *,
		size_t);
FAKE_VALUE_FUNC(HASH_LIST *,
		create_hash_list,
		hash_ftn_t *,
		key_cmp_ftn_t *,
		data_update_ftn_t *,
		uint32_t,
		uint32_t,
		uint32_t);

int32_t search_mount_success = 0;
MOUNT_T search_mount_ret;
int32_t custom_search_mount(char *a, char *b, MOUNT_T **c)
{
	if (search_mount_success) {
		*c = &search_mount_ret;
		return 0;
	}
	return -1;
}

int32_t create_hash_list_success = 0;
HASH_LIST *custom_create_hash_list(hash_ftn_t *hash_ftn,
				   key_cmp_ftn_t *key_cmp_ftn,
				   data_update_ftn_t *data_update_ftn,
				   uint32_t table_size,
				   uint32_t key_size,
				   uint32_t data_size)
{
	if (create_hash_list_success) {
		return (HASH_LIST *)1;
	}

	errno = ENOMEM;
	return NULL;
}

FAKE_VOID_FUNC(destroy_hash_list, HASH_LIST *);

FAKE_VALUE_FUNC(
    int32_t, insert_hash_list_entry, HASH_LIST *, const void *, const void *)
int32_t insert_minapk_data_success = 0;
int32_t custom_insert_hash_list_entry(HASH_LIST *hash_list,
				      const void *key,
				      const void *data)
{
	if (insert_minapk_data_success)
		return 0;
	else
		return -EEXIST;
}

FAKE_VALUE_FUNC(
    int32_t, update_hash_list_entry, HASH_LIST *, const hash_key_t *,
    hash_data_t *, data_update_t *);

int32_t custom_update_hash_list_entry(HASH_LIST *hash_list,
    const hash_key_t *key, hash_data_t *data, data_update_t *update_data)
{
	return 0;
}

FAKE_VALUE_FUNC(
    int32_t, lookup_hash_list_entry, HASH_LIST *, const void *, void *);
int32_t query_minapk_data_success = 0;
int32_t custom_lookup_hash_list_entry(HASH_LIST *hash_list,
				      const void *key,
				      void *data)
{
	((MIN_APK_LOOKUP_DATA *)data)->is_complete_apk = false;
	if (query_minapk_data_success) {
		((MIN_APK_LOOKUP_DATA *)data)->min_apk_ino = 5566;
		return 0;
	}

	return -ENOENT;
}

FAKE_VALUE_FUNC(int32_t, remove_hash_list_entry, HASH_LIST *, const void *);

int32_t remove_minapk_data_success = 0;
int32_t custom_remove_hash_list_entry(HASH_LIST *hash_list, const void *key)
{
	if (remove_minapk_data_success) {
		return 0;
	}

	return -ENOENT;
}

FAKE_VOID_FUNC(hash_list_global_lock, HASH_LIST *);

FAKE_VALUE_FUNC(HASH_LIST_ITERATOR *, init_hashlist_iter, HASH_LIST *);

int32_t init_hashlist_iter_success = 0;
HASH_LIST_ITERATOR *init_hashlist_iter_ret;
HASH_LIST_ITERATOR *custom_init_hashlist_iter(HASH_LIST *hash_list)
{
	if (init_hashlist_iter_success) {
		if (init_hashlist_iter_ret == NULL)
			return (HASH_LIST_ITERATOR *)1;
		return init_hashlist_iter_ret;
	}

	errno = ENOMEM;
	return NULL;
}

FAKE_VOID_FUNC(destroy_hashlist_iter, HASH_LIST_ITERATOR *);
FAKE_VOID_FUNC(hash_list_global_unlock, HASH_LIST *);

FAKE_VALUE_FUNC(int32_t, check_data_location, ino_t);

int32_t custom_check_data_location(ino_t thisinode)
{
	return 0;
}

/*
 * Helper functions
 */

