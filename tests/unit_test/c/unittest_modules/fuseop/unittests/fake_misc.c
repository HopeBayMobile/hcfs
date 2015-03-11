#include <sys/types.h>
#include <string.h>
#include <curl/curl.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>

#include "meta_mem_cache.h"
#include "filetables.h"
#include "hcfs_fromcloud.h"
#include "global.h"

#include "fake_misc.h"

ino_t lookup_pathname(const char *path, int *errcode)
{
	*errcode = 0;
	if (strcmp(path, "/") == 0)
		return 1;
	if (strcmp(path, "/does_not_exist") == 0) {
		*errcode = -ENOENT;
		return 0;
	}
	if (strcmp(path, "/testfile") == 0) {
		return 2;
	}
	if (strcmp(path, "/testsamefile") == 0) {
		return 2;
	}
	if (strcmp(path, "/testcreate") == 0) {
		if (before_mknod_created == TRUE) {
			*errcode = -ENOENT;
			return 0;
		}
		return 4;
	}
	if (strcmp(path, "/testmkdir") == 0) {
		if (before_mkdir_created == TRUE) {
			*errcode = -ENOENT;
			return 0;
		}
		return 6;
	}
	if (strcmp(path, "/testmkdir/test") == 0) {
		*errcode = -ENOENT;
		return 0;
	}
	if (strcmp(path, "/testfile1") == 0) {
		return 10;
	}
	if (strcmp(path, "/testfile2") == 0) {
		return 11;
	}
	if (strcmp(path, "/testdir1") == 0) {
		return 12;
	}
	if (strcmp(path, "/testdir2") == 0) {
		return 13;
	}
	if (strcmp(path, "/testtruncate") == 0) {
		return 14;
	}

	*errcode = -EACCES;
	return 0;
}

off_t check_file_size(const char *path)
{
	struct stat tempstat;

	stat(path, &tempstat);
	return tempstat.st_size;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	if (access("/tmp/testblock", F_OK) != 0)
		mkdir("/tmp/testblock", 0700);
	snprintf(pathname, 100, "/tmp/testblock/block_%lld_%lld",
		this_inode, block_num);
	return 0;
}

int change_system_meta(long long system_size_delta,
		long long cache_size_delta, long long cache_blocks_delta)
{
	hcfs_system->systemdata.system_size += system_size_delta;
	hcfs_system->systemdata.cache_size += cache_size_delta;
	hcfs_system->systemdata.cache_blocks += cache_blocks_delta;
	return 0;
}

int parse_parent_self(const char *pathname, char *parentname, char *selfname)
{
	int count;

	if (pathname == NULL)
		return -1;

	if (parentname == NULL)
		return -1;

	if (selfname == NULL)
		return -1;

	if (pathname[0] != '/')	 /* Does not handle relative path */
		return -1;

	if (strlen(pathname) <= 1)  /*This is the root, so no parent*/
	 return -1;

	for (count = strlen(pathname)-1; count >= 0; count--) {
		if ((pathname[count] == '/') && (count < (strlen(pathname)-1)))
			break;
	}

	if (count == 0) {
		strcpy(parentname, "/");
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[1]), strlen(pathname)-2);
			selfname[strlen(pathname)-2] = 0;
		} else {
			strcpy(selfname, &(pathname[1]));
		}
	} else {
		strncpy(parentname, pathname, count);
		parentname[count] = 0;
		if (pathname[strlen(pathname)-1] == '/') {
			strncpy(selfname, &(pathname[count+1]),
						strlen(pathname)-count-2);
			selfname[strlen(pathname)-count-2] = 0;
		} else {
			strcpy(selfname, &(pathname[count+1]));
		}
	}
	return 0;
}

long long open_fh(ino_t thisinode)
{
	long long index;

	index = (long long) thisinode;
	system_fh_table.entry_table_flags[index] = TRUE;
	system_fh_table.entry_table[index].thisinode = thisinode;

	return index;
}

int close_fh(long long index)
{
	system_fh_table.entry_table_flags[index] = FALSE;
	system_fh_table.entry_table[index].thisinode = 0;
	return 0;
}

int seek_page(FH_ENTRY *fh_ptr, long long target_page)
{
	return 0;
}

long long advance_block(META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,
						long long *entry_index)
{
	return 1;
}

void prefetch_block(PREFETCH_STRUCT_TYPE *ptr)
{
	return 0;
}
int fetch_from_cloud(FILE *fptr, ino_t this_inode, long long block_no)
{
	return 0;
}

void sleep_on_cache_full(void)
{
	return;
}

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
int change_parent_inode(ino_t self_inode, ino_t parent_inode1,
			ino_t parent_inode2, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat)
{
	switch (this_inode) {
	case 1:
		inode_stat->st_ino = 1;
		inode_stat->st_mode = S_IFDIR | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 2:
		inode_stat->st_ino = 2;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 4:
		inode_stat->st_ino = 4;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		break;	
	case 6:
		inode_stat->st_ino = 6;
		inode_stat->st_mode = S_IFDIR | 0700;
		inode_stat->st_atime = 100000;
		break;	
	case 10:
		inode_stat->st_ino = 10;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 11:
		inode_stat->st_ino = 11;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		inode_stat->st_size = 1024;
		break;
	case 12:
		inode_stat->st_ino = 12;
		inode_stat->st_mode = S_IFDIR | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 13:
		inode_stat->st_ino = 13;
		inode_stat->st_mode = S_IFDIR | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 14:
		inode_stat->st_ino = 14;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		inode_stat->st_size = 102400;
		break;
	default:
		break;
	}

	if (before_update_file_data == FALSE)
		memcpy(inode_stat, &updated_stat, sizeof(struct stat));

	return 0;
}

int mknod_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
						struct stat *this_stat)
{
	if (fail_mknod_update_meta == TRUE)
		return -1;
        before_mknod_created = FALSE;
	return 0;
}

int mkdir_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname,
						struct stat *this_stat)
{
	if (fail_mkdir_update_meta == TRUE)
		return -1;
        before_mkdir_created = FALSE;
	return 0;
}

int unlink_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname)
{
	if (this_inode == 4)
		before_mknod_created = TRUE;
	return 0;
}

int meta_forget_inode(ino_t self_inode)
{
	return 0;
}

int rmdir_update_meta(ino_t parent_inode, ino_t this_inode, char *selfname)
{
	if (this_inode == 6)
		before_mkdir_created = TRUE;
	return 0;
}

ino_t super_block_new_inode(struct stat *in_stat)
{
	if (fail_super_block_new_inode == TRUE)
		return 0;
	return 4;
}

int super_block_share_locking(void)
{
	return 0;
}

int super_block_share_release(void)
{
	return 0;
}

int invalidate_pathname_cache_entry(const char *path)
{
	return 0;
}

void hcfs_destroy_backend(CURL *curl)
{
	return;
}
int change_dir_entry_inode(ino_t self_inode, char *targetname,
		ino_t new_inode, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}
int decrease_nlink_inode_file(ino_t this_inode)
{
	return 0;
}
int delete_inode_meta(ino_t this_inode)
{
	return 0;
}
