#include "api_interface_unittest.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "fuseop.h"
#include "global.h"
#include "mount_manager.h"
#include "meta_mem_cache.h"
#include "dir_statistics.h"

int write_log(int level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int unmount_all(void)
{
	UNMOUNTEDALL = TRUE;
	return 0;
}

int add_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	CREATEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int delete_filesystem(char *fsname)
{
	strcpy(recvFSname, fsname);
	DELETEDFS = TRUE;
	return 0;
}
int check_filesystem(char *fsname, DIR_ENTRY *ret_entry)
{
	strcpy(recvFSname, fsname);
	CHECKEDFS = TRUE;
	return 0;
}
int list_filesystem(unsigned long buf_num, DIR_ENTRY *ret_entry,
		unsigned long *ret_num)
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

int mount_FS(char *fsname, char *mp, char mp_mode)
{
	MOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	strcpy(recvmpname, mp);
	return 0;
}
int unmount_FS(char *fsname, char *mp)
{
	UNMOUNTEDFS = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}
int mount_status(char *fsname)
{
	CHECKEDMOUNT = TRUE;
	strcpy(recvFSname, fsname);
	return 0;
}

int search_mount(char *fsname, char *mp, MOUNT_T **mt_info)
{
	return -ENOENT;
}

int check_filesystem_core(char *fsname, DIR_ENTRY *ret_entry)
{
	return -ENOENT;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	return -EIO;
}

int fetch_stat_path(char *pathname, ino_t this_inode)
{
	return -EIO;
}

int read_dirstat_lookup(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	return -EIO;
}
int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat,
		unsigned long *ret_gen, char *ret_pin_status)
{
	return -EIO;
}

int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return -EIO;
}
int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}

int meta_cache_lookup_symlink_data(ino_t this_inode, struct stat *inode_stat,
	SYMLINK_META_TYPE *symlink_meta_ptr, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return -EIO;
}
META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return NULL;
}

int pin_inode(ino_t this_inode, long long *reserved_pinned_size)
{
	if (PIN_INODE_ROLLBACK == TRUE)
		return -EIO;
	return 0;
}

int unpin_inode(ino_t this_inode, long long *reserved_release_size)
{
	if (UNPIN_INODE_FAIL == TRUE)
		return -EIO;
	return 0;
}

inline void update_sync_state(void)
{
	return;
}
int reload_system_config(const char *config_path)
{
	return 0;
}

int update_quota()
{
	hcfs_system->systemdata.system_quota = 5566;
	return 0;
}
