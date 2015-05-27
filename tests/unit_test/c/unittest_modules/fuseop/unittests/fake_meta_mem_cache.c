#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "meta_mem_cache.h"
#include "fake_misc.h"
#include "global.h"

int meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	printf("Debug inode is %lld\n", body_ptr->inode_num);
	switch(body_ptr->inode_num) {
	case 14:
		body_ptr->fptr = fopen("/tmp/hcfs_unittest_truncate", "w");
		unlink("/tmp/hcfs_unittest_truncate");
		break;
	case 16:
		body_ptr->fptr = fopen("/tmp/hcfs_unittest_write", "w");
		unlink("/tmp/hcfs_unittest_write");
		break;
	case 17:
		body_ptr->fptr = fopen(readdir_metapath, "r");
		break;
	default:
		break;
	}
	return 0;
}
int meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (body_ptr->fptr != NULL) {
		fclose(body_ptr->fptr);
		body_ptr->fptr = NULL;
	}
	return 0;
}
int meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	if (target_ptr->fptr != NULL) {
		fclose(target_ptr->fptr);
		target_ptr->fptr = NULL;
	}
	if (target_ptr != NULL)
		free(target_ptr);
	return 0;
}

int meta_cache_update_file_data(ino_t this_inode, const struct stat *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == 1)
		root_updated = TRUE;
	else
		before_update_file_data = FALSE;
	if (inode_stat != NULL) {
		if (this_inode == 1)
			memcpy(&updated_root, inode_stat, sizeof(struct stat));
		else
			memcpy(&updated_stat, inode_stat, sizeof(struct stat));
	}

	if (block_page != NULL) {
		after_update_block_page = TRUE;
		memcpy(&updated_block_page, block_page,
				sizeof(BLOCK_ENTRY_PAGE));
	}
	return 0;
}

int meta_cache_lookup_file_data(ino_t this_inode, struct stat *inode_stat,
	FILE_META_TYPE *file_meta_ptr, BLOCK_ENTRY_PAGE *block_page,
	long long page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (inode_stat != NULL) {
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
		default:
			break;
		}

		inode_stat->st_uid = geteuid();
		inode_stat->st_gid = getegid();

		if (this_inode == 1 && root_updated == TRUE)
			memcpy(inode_stat, &updated_root, sizeof(struct stat));
		if (this_inode != 1 && before_update_file_data == FALSE)
			memcpy(inode_stat, &updated_stat, sizeof(struct stat));
	}


	if (file_meta_ptr != NULL) {
		file_meta_ptr->direct = 0;
		file_meta_ptr->single_indirect = 0;
		file_meta_ptr->double_indirect = 0;
		file_meta_ptr->triple_indirect = 0;
		file_meta_ptr->quadruple_indirect = 0;
		switch (this_inode) {
		case 14:
		case 15:
		case 16:
			file_meta_ptr->direct = sizeof(struct stat)
				+ sizeof(FILE_META_TYPE);
			break;
		default:
			break;
		}
	}

	if (block_page != NULL) {
		memset(block_page, 0, sizeof(BLOCK_ENTRY_PAGE));
		switch (this_inode) {
		case 14:
		case 15:
		case 16:
			if (page_pos == sizeof(struct stat)
				+ sizeof(FILE_META_TYPE)) {
				block_page->num_entries = 1;
				block_page->block_entries[0].status
					= fake_block_status;
			}
			break;
		default:
			break;
		}
		if (after_update_block_page == TRUE) {
			memcpy(block_page, &updated_block_page,
				sizeof(BLOCK_ENTRY_PAGE));
		}
	}
	return 0;
}

int meta_cache_update_dir_data(ino_t this_inode, const struct stat *inode_stat,
	const DIR_META_TYPE *dir_meta_ptr, const DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int meta_cache_lookup_dir_data(ino_t this_inode, struct stat *inode_stat,
	DIR_META_TYPE *dir_meta_ptr, DIR_ENTRY_PAGE *dir_page,
	META_CACHE_ENTRY_STRUCT *body_ptr)
{
	FILE *fptr;
	if (dir_meta_ptr != NULL) {
		switch (this_inode) {
		case 13:
			dir_meta_ptr->total_children = 2;
			break;
		case 17:
			if (readdir_metapath == NULL)
				break;
			fptr = fopen(readdir_metapath, "r");
			fseek(fptr, sizeof(struct stat), SEEK_SET);
			fread(dir_meta_ptr, sizeof(DIR_META_TYPE), 1, fptr);
			fclose(fptr);
			break;
		default:
			dir_meta_ptr->total_children = 0;
			break;
		}
	}
	if (dir_page != NULL) {
		switch (this_inode) {
		case 17:
			if (readdir_metapath == NULL)
				break;
			fptr = fopen(readdir_metapath, "r");
			fseek(fptr, dir_page->this_page_pos, SEEK_SET);
			fread(dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);
			fclose(fptr);
			break;
		default:
			break;
		}
	}
	return 0;
}

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	META_CACHE_ENTRY_STRUCT *ptr;

	ptr = malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	if (ptr != NULL) {
		memset(ptr, 0, sizeof(META_CACHE_ENTRY_STRUCT));
		ptr->inode_num = this_inode;
	}
	return ptr;
}

int meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int init_meta_cache_headers(void)
{
	return 0;
}

int release_meta_cache_headers(void)
{
	return 0;
}

int meta_cache_remove(ino_t this_inode)
{
	return 0;
}

int meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
		int *result_index, const char *childname,
		META_CACHE_ENTRY_STRUCT *body_ptr)
{
	switch (this_inode) {
	case 1:
		if (strcmp(childname,"testfile") == 0) {
			result_page->dir_entries[0].d_ino = 2;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testsamefile") == 0) {
			result_page->dir_entries[0].d_ino = 2;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testfile1") == 0) {
			result_page->dir_entries[0].d_ino = 10;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testfile2") == 0) {
			result_page->dir_entries[0].d_ino = 11;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testdir1") == 0) {
			result_page->dir_entries[0].d_ino = 12;
			*result_index = 0;
			break;
		}
		if (strcmp(childname,"testdir2") == 0) {
			result_page->dir_entries[0].d_ino = 13;
			*result_index = 0;
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}
