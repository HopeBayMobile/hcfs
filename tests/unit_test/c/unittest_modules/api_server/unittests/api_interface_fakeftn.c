#include "api_interface_unittest.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"
#include "mount_manager.h"
#include "meta_mem_cache.h"
#include "dir_statistics.h"

extern SYSTEM_CONF_STRUCT *system_config;

int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t unmount_all(void)
{
	UNMOUNTEDALL = TRUE;
	return 0;
}

int32_t add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	CREATEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int32_t delete_filesystem(char *fsname)
{
	strcpy(recvFSname, fsname);
	DELETEDFS = TRUE;
	return 0;
}
int32_t check_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	strcpy(recvFSname, fsname);
	CHECKEDFS = TRUE;
	return 0;
}
int32_t list_filesystem(uint64_t buf_num, DIR_ENTRY *ret_entry,
		uint64_t *ret_num)
{
	LISTEDFS = TRUE;
	if (numlistedFS == 0) {
		*ret_num = 0;
	} else {
		*ret_num = 1;
		if (buf_num > 0)
			snprintf(ret_entry[0].d_name, 10, "test123");
	}
	return 0;
}

int32_t mount_FS(char *fsname, char *mp, char mp_mode)
{
	MOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	strcpy(recvmpname, mp);
	return 0;
}
int32_t unmount_FS(char *fsname, char *mp)
{
	UNMOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int32_t mount_status(char *fsname)
{
	CHECKEDMOUNT = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}

int32_t search_mount(char *fsname, char *mp, MOUNT_T **mt_info)
{
	return -ENOENT;
}

int32_t check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry)
{
	return -ENOENT;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	return -EIO;
}

int32_t fetch_stat_path(char *pathname, ino_t this_inode)
{
	return -EIO;
}

int32_t read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	return -EIO;
}
int32_t fetch_inode_stat(ino_t this_inode, struct stat *inode_stat,
		uint64_t *ret_gen, char *ret_pin_status)
{
	return -EIO;
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return -EIO;
}
int32_t meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}

int32_t meta_cache_lookup_symlink_data(ino_t this_inode, struct stat *inode_stat,
	SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int32_t meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int32_t meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return NULL;
}

int32_t pin_inode(ino_t this_inode, int64_t *reserved_pinned_size)
{
	if (PIN_INODE_ROLLBACK == TRUE)
		return -EIO;
	return 0;
}

int32_t unpin_inode(ino_t this_inode, int64_t *reserved_release_size)
{
	if (UNPIN_INODE_FAIL == TRUE)
		return -EIO;
	return 0;
}

void update_sync_state(void)
{
	return;
}
int32_t reload_system_config(const char *config_path)
{
	return 0;
}

int32_t update_quota()
{
	hcfs_system->systemdata.system_quota = 5566;
	return 0;
}

int64_t get_pinned_limit(const char pin_type)
{
	if (pin_type < NUM_PIN_TYPES)
		return PINNED_LIMITS(pin_type);
	else
		return -EINVAL;
}

int32_t sync_hcfs_system_data(char need_lock)
{
	return 0;
}

int32_t super_block_set_syncpoint()
{
	return 0;
}

int32_t super_block_cancel_syncpoint()
{
	return 0;
}

int32_t set_event_notify_server(char *path)
{
	if (strcmp(path, "setok") == 0)
		return 0;
	else
		return -1;
}
