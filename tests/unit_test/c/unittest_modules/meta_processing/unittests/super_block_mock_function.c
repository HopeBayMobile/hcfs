#include "params.h"
#include "fuseop.h"
#include <sys/types.h>

extern SYSTEM_DATA_HEAD *hcfs_system;

int write_log(int level, char *format, ...)
{
	return 0;
}

int get_meta_size(ino_t inode, long long *metasize)
{
	*metasize = 5566;
	return 0;
}

int change_system_meta(long long system_size_delta, long long meta_size_delta,
		long long cache_size_delta, long long cache_blocks_delta,
		long long dirty_cache_delta)
{
	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	return 0;
}

int update_sb_size()
{
	return 0;
}
