#include "params.h"
#include "fuseop.h"
#include <sys/types.h>

extern SYSTEM_DATA_HEAD *hcfs_system;

int write_log(int level, char *format, ...)
{
	return 0;
}

int get_meta_size(ino_t inode, int64_t *metasize)
{
	*metasize = 5566;
	return 0;
}

int change_system_meta(int64_t system_size_delta, int64_t meta_size_delta,
		int64_t cache_size_delta, int64_t cache_blocks_delta,
		int64_t dirty_cache_delta)
{
	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	return 0;
}

int update_sb_size()
{
	return 0;
}
