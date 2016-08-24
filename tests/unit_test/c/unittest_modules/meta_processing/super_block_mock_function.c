#include "super_block_mock_params.h"
#include <sys/types.h>
#include <errno.h>
#include <string.h>
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

int32_t get_meta_size(ino_t inode, int64_t *metasize, int64_t *roundmetasize)
{
	if (metasize)
		*metasize = 5566;
	if (roundmetasize)
		*roundmetasize = 8192;
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

int32_t update_sb_size(void)
{
	return 0;
}

void sleep_on_cache_full(void)
{
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
}
int32_t restore_meta_super_block_entry(ino_t this_inode,
                struct stat *ret_stat)
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
	return 0;
}

void free_syncpoint_resource(BOOL remove_file)
{
	return;
}

int32_t write_syncpoint_data()
{
	return 0;
}

int32_t meta_nospc_log(const char *func_name, int32_t lines)
{
	return 1;
}
int32_t init_rebuild_sb(char rebuild_action)
{
	mknod(SUPERBLOCK, 0600, 0);
	mknod(UNCLAIMEDFILE, 0600, 0);
	return 0;
}

int32_t create_sb_rebuilder()
{
	return 0;
}

int32_t fetch_object_busywait_conn(FILE *fptr, char action_from, char *objname)
{
	if (!strcmp(objname, "FSmgr_backup")) {
		DIR_META_TYPE dirmeta;
		memset(&dirmeta, 0, sizeof(DIR_META_TYPE));
		dirmeta.total_children = NUM_VOL;
		pwrite(fileno(fptr), &dirmeta, sizeof(DIR_META_TYPE), 16);
	}
	printf("fetch object %s\n", objname);

	return 0;
}
