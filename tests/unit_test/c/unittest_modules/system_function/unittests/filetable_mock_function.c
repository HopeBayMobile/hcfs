#include "meta_mem_cache.h"
#include "mock_params.h"

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	META_CACHE_ENTRY_STRUCT *ret;

	switch (this_inode) {
	case INO__META_CACHE_LOCK_ENTRY_FAIL:
		return NULL;
	case INO__META_CACHE_LOCK_ENTRY_SUCCESS:
		ret = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		ret->inode_num = INO__META_CACHE_LOCK_ENTRY_SUCCESS;
		return ret;
	default:
		ret = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		ret->inode_num = this_inode;
		return ret;
	}
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

