#include "params.h"
#include "global.h"
#include <semaphore.h>

SYSTEM_CONF_STRUCT *system_config;
char hcfs_list_container_success;
char hcfs_init_backend_success;

enum {
	INO__META_CACHE_LOCK_ENTRY_FAIL = 1000000,
	INO__META_CACHE_LOCK_ENTRY_SUCCESS
};

char restore_metapath[METAPATHLEN];
char restore_blockpath[BLOCKPATHLEN];
sem_t restore_sem;

#define RESTORE_METAPATH restore_metapath
#define RESTORE_BLOCKPATH restore_blockpath

