#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "filetables.h"
#include "meta_mem_cache.h"
#include "fake_misc.h"
#include "global.h"

int32_t meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	printf("Debug inode is %lu\n", body_ptr->inode_num);
	switch(body_ptr->inode_num) {
	case 14:
		body_ptr->fptr = fopen("/tmp/testHCFS/hcfs_unittest_truncate",
			"w");
		unlink("/tmp/testHCFS/hcfs_unittest_truncate");
		break;
	case 16:
		body_ptr->fptr = fopen("/tmp/testHCFS/hcfs_unittest_write", "w");
		unlink("/tmp/testHCFS/hcfs_unittest_write");
		break;
	case TEST_LISTDIR_INODE:
		body_ptr->fptr = fopen(readdir_metapath, "r");
		break;
	default:
		break;
	}
	return 0;
}
int32_t meta_cache_close_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (body_ptr->fptr != NULL) {
		fclose(body_ptr->fptr);
		body_ptr->fptr = NULL;
	}
	return 0;
}
int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	int32_t fh;

	fh = target_ptr->inode_num % 100;
	sem_post(&(target_ptr->access_sem));

	if (system_fh_table.entry_table_flags[fh] == NO_FH) {

		if (target_ptr->fptr != NULL) {
			fclose(target_ptr->fptr);
			target_ptr->fptr = NULL;
		}

		if (target_ptr != NULL)
			free(target_ptr);
	}
	return 0;
}

int32_t meta_cache_update_file_data(ino_t this_inode,
				    const HCFS_STAT *inode_stat,
				    const FILE_META_TYPE *file_meta_ptr,
				    const BLOCK_ENTRY_PAGE *block_page,
				    const int64_t page_pos,
				    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == 1)
		root_updated = TRUE;
	else
		before_update_file_data = FALSE;
	if (inode_stat != NULL) {
		if (this_inode == 1)
			memcpy(&updated_root, inode_stat, sizeof(HCFS_STAT));
		else
			memcpy(&updated_stat, inode_stat, sizeof(HCFS_STAT));
	}

	if (block_page != NULL) {
		after_update_block_page = TRUE;
		memcpy(&updated_block_page, block_page,
				sizeof(BLOCK_ENTRY_PAGE));
	}
	return 0;
}

int32_t meta_cache_update_file_nosync(ino_t this_inode,
	const HCFS_STAT *inode_stat,
	const FILE_META_TYPE *file_meta_ptr, const BLOCK_ENTRY_PAGE *block_page,
	const int64_t page_pos, META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == 1)
		root_updated = TRUE;
	else
		before_update_file_data = FALSE;
	if (inode_stat != NULL) {
		if (this_inode == 1)
			memcpy(&updated_root, inode_stat, sizeof(HCFS_STAT));
		else
			memcpy(&updated_stat, inode_stat, sizeof(HCFS_STAT));
	}

	if (block_page != NULL) {
		after_update_block_page = TRUE;
		memcpy(&updated_block_page, block_page,
				sizeof(BLOCK_ENTRY_PAGE));
	}
	return 0;
}

int32_t meta_cache_update_stat_nosync(ino_t this_inode,
				      const HCFS_STAT *inode_stat,
				      META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (this_inode == 1)
		root_updated = TRUE;
	else
		before_update_file_data = FALSE;
	if (inode_stat != NULL) {
		if (this_inode == 1)
			memcpy(&updated_root, inode_stat, sizeof(HCFS_STAT));
		else
			memcpy(&updated_stat, inode_stat, sizeof(HCFS_STAT));
	}

	return 0;
}

int32_t meta_cache_lookup_file_data(ino_t this_inode,
				    HCFS_STAT *inode_stat,
				    FILE_META_TYPE *file_meta_ptr,
				    BLOCK_ENTRY_PAGE *block_page,
				    int64_t page_pos,
				    META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (inode_stat != NULL) {
		switch (this_inode) {
		case 1:
			inode_stat->ino = 1;
			inode_stat->mode = S_IFDIR | 0700;
			inode_stat->atime = 100000;
			break;
		case 2:
			inode_stat->ino = 2;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			break;
		case 4:
			inode_stat->ino = 4;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			break;	
		case 6:
			inode_stat->ino = 6;
			inode_stat->mode = S_IFDIR | 0700;
			inode_stat->atime = 100000;
			break;	
		case 10:
			inode_stat->ino = 10;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			break;
		case 11:
			inode_stat->ino = 11;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			inode_stat->size = 1024;
			break;
		case 12:
			inode_stat->ino = 12;
			inode_stat->mode = S_IFDIR | 0700;
			inode_stat->atime = 100000;
			break;
		case 13:
			inode_stat->ino = 13;
			inode_stat->mode = S_IFDIR | 0700;
			inode_stat->atime = 100000;
			break;
		case 14:
			inode_stat->ino = 14;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			inode_stat->size = 102400;
			break;
		case 15:
			inode_stat->ino = 15;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			inode_stat->size = 204800;
			break;
		case 16:
			inode_stat->ino = 16;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			inode_stat->size = 204800;
			break;
		case TEST_LISTDIR_INODE:
			inode_stat->ino = TEST_LISTDIR_INODE;
			inode_stat->mode = S_IFDIR | 0700;
			inode_stat->atime = 100000;
			break;
		case 18:
			inode_stat->ino = 18;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			break;
		case 19:
			inode_stat->ino = 19;
			inode_stat->mode = S_IFREG | 0000;
			inode_stat->atime = 100000;
			break;
		case 20:
			inode_stat->ino = 20;
			inode_stat->mode = S_IFREG | 0700;
			inode_stat->atime = 100000;
			break;
		default:
			break;
		}

		inode_stat->uid = geteuid();
		inode_stat->gid = getegid();

		if (this_inode == 1 && root_updated == TRUE)
			memcpy(inode_stat, &updated_root, sizeof(HCFS_STAT));
		if (this_inode != 1 && before_update_file_data == FALSE)
			memcpy(inode_stat, &updated_stat, sizeof(HCFS_STAT));
	}


	if (file_meta_ptr != NULL) {
		file_meta_ptr->direct = 0;
		file_meta_ptr->single_indirect = 0;
		file_meta_ptr->double_indirect = 0;
		file_meta_ptr->triple_indirect = 0;
		file_meta_ptr->quadruple_indirect = 0;
		if (this_inode == 14)
			file_meta_ptr->local_pin = TRUE;
		else
			file_meta_ptr->local_pin = FALSE;
		switch (this_inode) {
		case 14:
		case 15:
		case 16:
			file_meta_ptr->direct = sizeof(HCFS_STAT)
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
			if (page_pos == sizeof(HCFS_STAT)
				+ sizeof(FILE_META_TYPE)) {
				block_page->num_entries = 1;
				block_page->block_entries[0].status
					= fake_block_status;
				block_page->block_entries[0].paged_out_count
					= fake_paged_out_count;
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

int32_t meta_cache_update_dir_data(ino_t this_inode,
				   const HCFS_STAT *inode_stat,
				   const DIR_META_TYPE *dir_meta_ptr,
				   const DIR_ENTRY_PAGE *dir_page,
				   META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t meta_cache_lookup_dir_data(ino_t this_inode,
				   HCFS_STAT *inode_stat,
				   DIR_META_TYPE *dir_meta_ptr,
				   DIR_ENTRY_PAGE *dir_page,
				   META_CACHE_ENTRY_STRUCT *body_ptr)
{
	FILE *fptr;
	if (dir_meta_ptr != NULL) {
		switch (this_inode) {
		case 13:
		case TEST_APPDIR_INODE:
			dir_meta_ptr->total_children = 2;
			break;
		case TEST_LISTDIR_INODE:
			fptr = fopen(readdir_metapath, "r");
			fseek(fptr, sizeof(HCFS_STAT), SEEK_SET);
			fread(dir_meta_ptr, sizeof(DIR_META_TYPE), 1, fptr);
			fclose(fptr);
			break;
		default:
			dir_meta_ptr->total_children = 0;
			break;
		}
	}
	int64_t this_page_pos;
	DIR_ENTRY *tmpptr;
	if (dir_page != NULL) {
		switch (this_inode) {
		case TEST_LISTDIR_INODE:
			fptr = fopen(readdir_metapath, "r");
			fseek(fptr, dir_page->this_page_pos, SEEK_SET);
			fread(dir_page, sizeof(DIR_ENTRY_PAGE), 1, fptr);
			fclose(fptr);
			break;
		case TEST_APPDIR_INODE:
			this_page_pos = dir_page->this_page_pos;
			memset(dir_page, 0, sizeof(DIR_ENTRY_PAGE));
			dir_page->this_page_pos = this_page_pos;
			dir_page->num_entries = 4;
			tmpptr = &(dir_page->dir_entries[0]);
			tmpptr->d_ino = TEST_APPDIR_INODE;
			snprintf(tmpptr->d_name, sizeof(tmpptr->d_name),
			         ".");
			tmpptr->d_type = D_ISDIR;
			tmpptr = &(dir_page->dir_entries[1]);
			tmpptr->d_ino = TEST_APPROOT_INODE;
			snprintf(tmpptr->d_name, sizeof(tmpptr->d_name),
			         "..");
			tmpptr->d_type = D_ISDIR;
			tmpptr = &(dir_page->dir_entries[2]);
			tmpptr->d_ino = TEST_APPAPK_INODE;
			snprintf(tmpptr->d_name, sizeof(tmpptr->d_name),
			         "base.apk");
			tmpptr->d_type = D_ISREG;
			tmpptr = &(dir_page->dir_entries[3]);
			tmpptr->d_ino = TEST_APPMIN_INODE;
			snprintf(tmpptr->d_name, sizeof(tmpptr->d_name),
			         ".basemin");
			tmpptr->d_type = D_ISREG;
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
	int32_t fh;

	fh = this_inode % 100;

	if (system_fh_table.entry_table_flags[fh] != NO_FH) {
		ptr = system_fh_table.entry_table[fh].meta_cache_ptr;
		if (ptr != NULL) {
			sem_wait(&(ptr->access_sem));
			return ptr;
		}
	}

	ptr = malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	if (ptr != NULL) {
		memset(ptr, 0, sizeof(META_CACHE_ENTRY_STRUCT));
		ptr->inode_num = this_inode;
		sem_init(&(ptr->access_sem), 0, 1);
		sem_wait(&(ptr->access_sem));
		if (system_fh_table.entry_table_flags[fh] == IS_FH)
			system_fh_table.entry_table[fh].meta_cache_ptr =
				ptr;
	}
	return ptr;
}

int32_t meta_cache_drop_pages(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	return 0;
}

int32_t init_meta_cache_headers(void)
{
	return 0;
}

int32_t release_meta_cache_headers(void)
{
	return 0;
}

int32_t meta_cache_remove(ino_t this_inode)
{
	return 0;
}

int32_t meta_cache_seek_dir_entry(ino_t this_inode, DIR_ENTRY_PAGE *result_page,
		int32_t *result_index, const char *childname,
		META_CACHE_ENTRY_STRUCT *body_ptr,
		BOOL is_external)
{
	*result_index = -1;

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
		if (strcmp(childname,"testsymlink_exist_in_symlink") == 0) {
			result_page->dir_entries[0].d_ino = 14;
			*result_index = 1;
			break;
		}
		if (strcmp(childname,"testsymlink_not_exist_in_symlink") == 0) {
			*result_index = -1;
			break;
		}
		if (strcmp(childname,"base.apk") == 0) {
			result_page->dir_entries[0].d_ino = 15;
			*result_index = 0;
			break;
		}
		if (strcmp(childname, "com.example.test") == 0) {
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

int32_t meta_cache_update_symlink_data(
    ino_t this_inode,
    const HCFS_STAT *inode_stat,
    const SYMLINK_META_TYPE *symlink_meta_ptr,
    META_CACHE_ENTRY_STRUCT *bptr)
{
	return 0;
}

int32_t meta_cache_lookup_symlink_data(ino_t this_inode,
				       HCFS_STAT *inode_stat,
				       SYMLINK_META_TYPE *symlink_meta_ptr,
				       META_CACHE_ENTRY_STRUCT *body_ptr)
{
	char *target = "I_am_target_link";

	strcpy(symlink_meta_ptr->link_path, target);
	symlink_meta_ptr->link_len = strlen(target);

	return 0;
}

