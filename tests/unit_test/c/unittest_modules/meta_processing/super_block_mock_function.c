#include "params.h"
#include "fuseop.h"
#include <sys/types.h>

extern SYSTEM_DATA_HEAD *hcfs_system;

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t get_meta_size(ino_t inode, int64_t *metasize)
{
	*metasize = 5566;
	return 0;
}

int32_t change_system_meta(int64_t system_size_delta, int64_t meta_size_delta,
		int64_t cache_size_delta, int64_t cache_blocks_delta,
		int64_t dirty_cache_delta)
{
	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	return 0;
}

int32_t update_sb_size()
{
	return 0;
}

sleep_on_cache_full(void)
{
	return;
}
