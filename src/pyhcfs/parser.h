#include "fuseop.h"
#include "FS_manager.h"

typedef struct {
	uint64_t inode;
	char name[256];
} PORTABLE_DIR_ENTRY;
int32_t list_external_volume(char *meta_path , PORTABLE_DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);
