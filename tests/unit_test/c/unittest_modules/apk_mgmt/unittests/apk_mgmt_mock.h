#include "hfuse_system.h"
#include "fuseop.h"
#include "apk_mgmt.h"
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
    int32_t, lookup_hash_list_entry, HASH_LIST *, const void *, void *);
FAKE_VALUE_FUNC(int32_t, remove_hash_list_entry, HASH_LIST *, const void *);
FAKE_VOID_FUNC(hash_list_global_lock, HASH_LIST *);
FAKE_VALUE_FUNC(HASH_LIST_ITERATOR *, init_hashlist_iter, HASH_LIST *);
FAKE_VOID_FUNC(destroy_hashlist_iter, HASH_LIST_ITERATOR *);
FAKE_VOID_FUNC(hash_list_global_unlock, HASH_LIST *);

/*
 * Helper functions
 */

