#include <sys/types.h>
#include <errno.h>
#include "params.h"
#include "fuseop.h"
#include "super_block.h"
#include "syncpoint_control.h"

extern SYSTEM_DATA_HEAD *hcfs_system;
extern SYSTEM_CONF_STRUCT *system_config;

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t get_meta_size(ino_t inode, int64_t *metasize)
{
	*metasize = 5566;
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
		int64_t meta_size_delta, int64_t cache_data_size_delta,
		int64_t cache_blocks_delta, int64_t dirty_cache_delta,
		int64_t unpin_dirty_data_size, BOOL need_sync)
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

int64_t get_pinned_limit(const char pin_type)
{
	if (pin_type < NUM_PIN_TYPES)
		return PINNED_LIMITS(pin_type);
	else
		return -EINVAL;
}

void move_sync_point(char which_ll, ino_t this_inode,
		struct SUPER_BLOCK_ENTRY *this_entry)
{
	return 0;
}

int32_t init_syncpoint_resource()
{
	sys_super_block->sync_point_is_set = TRUE;
	sys_super_block->sync_point_info = (SYNC_POINT_INFO *)
		malloc(sizeof(SYNC_POINT_INFO));
	memset(sys_super_block->sync_point_info, 0, sizeof(SYNC_POINT_INFO));
	sem_init(&(sys_super_block->sync_point_info->ctl_sem), 0, 1);
}

void free_syncpoint_resource(BOOL remove_file)
{
	return;
}

int32_t write_syncpoint_data()
{
	return 0;
}
