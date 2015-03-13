#include "meta_mem_cache.h"

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return NULL;
}

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}

int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
        int *result_index, char *childname, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
