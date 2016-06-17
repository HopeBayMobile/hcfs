#include "../HCFS/fuseop.h"
#include "../HCFS/FS_manager.h"

int32_t list_external_volume(char *meta_path , DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);
