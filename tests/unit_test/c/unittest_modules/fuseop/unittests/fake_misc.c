#include <sys/types.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>

#include "meta_mem_cache.h"
#include "filetables.h"
#include "hcfs_fromcloud.h"
#include "xattr_ops.h"
#include "global.h"

#include "fake_misc.h"

int init_api_interface(void)
{
	return 0;
}

int destroy_api_interface(void)
{
	return 0;
}

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
	if (strcmp(path, "/testread") == 0) {
		return 15;
	}
	if (strcmp(path, "/testwrite") == 0) {
		return 16;
	}
	if (strcmp(path, "/testlistdir") == 0) {
		return 17;
	}
	if (strcmp(path, "/testsetxattr") == 0) {
		return 18;
	}
	if (strcmp(path, "/testsetxattr_permissiondeny") == 0) {
		return 19;
	}
	if (strcmp(path, "/testsetxattr_fail") == 0) {
		return 20;
	}

	*errcode = -EACCES;
	return 0;
}

int lookup_dir(ino_t parent, char *childname, DIR_ENTRY *dentry)
{
	ino_t this_inode;
	char this_type;

	this_inode = 0;

	if (parent == 1) {   /* If parent is root */
		if (strcmp(childname, "does_not_exist") == 0)
			return -ENOENT;

		if (strcmp(childname, "testfile") == 0) {
			this_inode = 2;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testsamefile") == 0) {
			this_inode = 2;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testcreate") == 0) {
			if (before_mknod_created == TRUE)
				return -ENOENT;
			this_inode = 4;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testmkdir") == 0) {
			if (before_mkdir_created == TRUE)
				return -ENOENT;
			this_inode = 6;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testfile1") == 0) {
			this_inode = 10;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testfile2") == 0) {
			this_inode = 11;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testdir1") == 0) {
			this_inode = 12;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testdir2") == 0) {
			this_inode = 13;
			this_type = D_ISDIR;
		}

		if (strcmp(childname, "testtruncate") == 0) {
			this_inode = 14;
			this_type = D_ISREG;
		}

		if (strcmp(childname, "testread") == 0) {
			this_inode = 15;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testwrite") == 0) {
			this_inode = 16;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testlistdir") == 0) {
			this_inode = 17;
			this_type = D_ISDIR;
		}
		if (strcmp(childname, "testsetxattr") == 0) {
			this_inode = 18;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsetxattr_permissiondeny") == 0) {
			this_inode = 19;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsetxattr_fail") == 0) {
			this_inode = 20;
			this_type = D_ISREG;
		}
		if (strcmp(childname, "testsymlink") == 0) {
			this_inode = 21;
			this_type = D_ISLNK;
		}
	}

	if (parent == 6) {
		if (strcmp(childname, "test") == 0) {
			return -ENOENT;
		}
	}

	if (this_inode > 0) {
		dentry->d_ino = this_inode;
		strcpy(dentry->d_name, childname);
		dentry->d_type = this_type;
		return 0;
	}

	return -ENOENT;
}

off_t check_file_size(const char *path)
{
	struct stat tempstat;

	stat(path, &tempstat);
	return tempstat.st_size;
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	if (access("/tmp/testHCFS/testblock", F_OK) != 0)
		mkdir("/tmp/testHCFS/testblock", 0700);
	snprintf(pathname, 100, "/tmp/testHCFS/testblock/block_%lld_%lld",
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

long long open_fh(ino_t thisinode, int flags)
{
	long long index;

	if (fail_open_files)
		return -1;

	index = (long long) thisinode;
	system_fh_table.entry_table_flags[index] = TRUE;
	system_fh_table.entry_table[index].thisinode = thisinode;
	system_fh_table.entry_table[index].meta_cache_ptr = NULL;
	system_fh_table.entry_table[index].meta_cache_locked = FALSE;
	system_fh_table.entry_table[index].flags = flags;

	system_fh_table.entry_table[index].blockfptr = NULL;
	system_fh_table.entry_table[index].opened_block = -1;
	system_fh_table.entry_table[index].cached_page_index = -1;
	system_fh_table.entry_table[index].cached_filepos = -1;
	sem_init(&(system_fh_table.entry_table[index].block_sem), 0, 1);

	return index;
}

int close_fh(long long index)
{
	FH_ENTRY *tmp_entry;

	tmp_entry = &(system_fh_table.entry_table[index]);
	tmp_entry->meta_cache_locked = FALSE;
	system_fh_table.entry_table_flags[index] = FALSE;
	tmp_entry->thisinode = 0;

	tmp_entry->meta_cache_ptr = NULL;
	tmp_entry->blockfptr = NULL;
	tmp_entry->opened_block = -1;
	sem_destroy(&(tmp_entry->block_sem));
	return 0;
}

long long seek_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page,
			long long hint_page)
{
	switch (target_page) {
	case 0:
		return sizeof(struct stat) + sizeof(FILE_META_TYPE);
	default:
		return 0;
	}
	return 0;
}

long long create_page(META_CACHE_ENTRY_STRUCT *body_ptr, long long target_page)
{
	switch (target_page) {
	case 0:
		return sizeof(struct stat) + sizeof(FILE_META_TYPE);
	default:
		return 0;
	}
	return 0;
}

void prefetch_block(PREFETCH_STRUCT_TYPE *ptr)
{
	return 0;
}
int fetch_from_cloud(FILE *fptr, ino_t this_inode, long long block_no)
{
	char tempbuf[1024];
	int tmp_len;

	switch (this_inode) {
	case 14:
		ftruncate(fileno(fptr), 102400);
		break;
	case 15:
	case 16:
		if (test_fetch_from_backend == TRUE) {
			fseek(fptr, 0, SEEK_SET);
			snprintf(tempbuf, 100, "This is a test data");
			tmp_len = strlen(tempbuf);
			fwrite(tempbuf, tmp_len, 1, fptr);
			fflush(fptr);
		} else {
			ftruncate(fileno(fptr), 204800);
		}
		break;
	default:
		break;
	}
	return 0;
}

void sleep_on_cache_full(void)
{
	printf("Debug passed sleep on cache full\n");
	hcfs_system->systemdata.cache_size = 1200000;
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

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat, unsigned long *gen)
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
	case 15:
		inode_stat->st_ino = 15;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		inode_stat->st_size = 204800;
		break;
	case 16:
		inode_stat->st_ino = 16;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		inode_stat->st_size = 204800;
		break;
	case 17:
		inode_stat->st_ino = 17;
		inode_stat->st_mode = S_IFDIR | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 18:
		inode_stat->st_ino = 18;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 19:
		inode_stat->st_ino = 19;
		inode_stat->st_mode = S_IFREG | 0500;
		inode_stat->st_atime = 100000;
		break;
	case 20:
		inode_stat->st_ino = 20;
		inode_stat->st_mode = S_IFREG | 0700;
		inode_stat->st_atime = 100000;
		break;
	case 21:
		inode_stat->st_ino = 21;
		inode_stat->st_mode = S_IFLNK | 0700;
		inode_stat->st_atime = 100000;
		break;
	default:
		break;
	}

	inode_stat->st_uid = geteuid();
	inode_stat->st_gid = getegid();

	if (this_inode == 1 && root_updated == TRUE)
		memcpy(inode_stat, &updated_root, sizeof(struct stat));
	if (this_inode != 1 && before_update_file_data == FALSE)
		memcpy(inode_stat, &updated_stat, sizeof(struct stat));

	if (gen)
		*gen = 10;

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

int lookup_init()
{
	return 0;
}
int lookup_increase(ino_t this_inode, int amount, char d_type)
{
	return 0;
}
int lookup_decrease(ino_t this_inode, int amount, char *d_type,
				char *need_delete)
{
	return 0;
}
int lookup_markdelete(ino_t this_inode)
{
	return 0;
}

int actual_delete_inode(ino_t this_inode, char d_type)
{
	return 0;
}
int mark_inode_delete(ino_t this_inode)
{
	return 0;
}

int disk_markdelete(ino_t this_inode)
{
	return 0;
}
int disk_cleardelete(ino_t this_inode)
{
	return 0;
}
int disk_checkdelete(ino_t this_inode)
{
	return 0;
}
int startup_finish_delete()
{
	return 0;
}
int lookup_destroy()
{
	return 0;
}

int write_log(int level, char *format, ...)
{
	return 0;
}

int parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	if (!strcmp(name, "user.aaa"))
		return 0;
	else
		return -EOPNOTSUPP;
}

int insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
        const long long xattr_filepos, const char name_space, const char *key, 
	const char *value, const size_t size, const int flag)
{
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	return 0;
}

int get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const char name_space, const char *key, char *value, const size_t size, 
	size_t *actual_size)
{
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;	

	if (size == 0) {
		*actual_size = CORRECT_VALUE_SIZE;
	} else {
		char *ans = "hello!getxattr:)";
		strcpy(value, ans);
		*actual_size = strlen(ans);
	}
	return 0;
}

int list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	char *key_buf, const size_t size, size_t *actual_size)
{
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;	

	if (size == 0) {
		*actual_size = CORRECT_VALUE_SIZE;
	} else {
		char *ans = "hello!listxattr:)";
		strcpy(key_buf, ans);
		*actual_size = strlen(ans);
	}
	return 0;
}

int remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const long long xattr_filepos, const char name_space, const char *key)
{
	if (meta_cache_entry->inode_num == 20)
		return -EEXIST;

	return 0;
}

int fetch_xattr_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *xattr_pos)
{
	return 0;
}

int symlink_update_meta(META_CACHE_ENTRY_STRUCT *parent_meta_cache_entry, 
	const struct stat *this_stat, const char *link, 
	const unsigned long generation, const char *name)
{
	if (!strcmp("update_meta_fail", link))
		return -1;

	return 0;
}
