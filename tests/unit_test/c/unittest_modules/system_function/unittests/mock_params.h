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

