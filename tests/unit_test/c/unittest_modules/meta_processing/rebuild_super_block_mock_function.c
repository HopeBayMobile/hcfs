#include "logger.h"
#include "super_block.h"
#include "fuseop.h"
#include "mount_manager.h"
#include "rebuild_super_block_params.h"
#include "meta_mem_cache.h"

#include <errno.h>
#include <string.h>
#include <stdarg.h>

extern SYSTEM_CONF_STRUCT *system_config;

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	if (level > 8)
		return 0;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	memset(inode_ptr, 0, sizeof(SUPER_BLOCK_ENTRY));
	if (NOW_TEST_RESTORE_META == TRUE)
		inode_ptr->this_index = 0;
	return 0;
}

int32_t super_block_exclusive_locking(void)
{
	return 0;
}

int32_t super_block_exclusive_release(void)
{
	return 0;
}

int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	printf("Write entry\n");
	pwrite(sys_super_block->iofptr, inode_ptr, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + (this_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY));

	return 0;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (NOW_TEST_RESTORE_META == TRUE) {
		inode_ptr->this_index = 0;
		return 0;
	} else {
		inode_ptr->this_index = this_inode;
		inode_ptr->inode_stat.mode =
			this_inode % 2 ? S_IFDIR : S_IFREG;
	}

	sem_wait(&record_inode_sem);
	record_inode[this_inode] = TRUE;
	max_record_inode = max_record_inode < this_inode ?
			this_inode : max_record_inode;
	sem_post(&record_inode_sem);

	return 0;
}

int32_t super_block_init(void)
{
	return 0;
}

int32_t super_block_reclaim_fullscan(void)
{
	return 0;
}

int32_t pin_ll_enqueue(ino_t this_inode, SUPER_BLOCK_ENTRY *this_entry)
{
	return 0;
}

int32_t fetch_object_busywait_conn(FILE *fptr, char action_from, char *objname)
{
	DIR_META_TYPE dirmeta;
	FS_CLOUD_STAT_T fs_stat;
	static int counter = 0;

	memset(&dirmeta, 0, sizeof(DIR_META_TYPE));

	if (!strcmp("FSmgr_backup", objname)) {
		fseek(fptr, 0, SEEK_SET);
		fwrite(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);
	} else if (!strncmp("FSstat", objname, 6)) {
		fseek(fptr, 0, SEEK_SET);
		counter++;
		fs_stat.max_inode = counter;
		fwrite(&fs_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	}
	return 0;
}

int32_t rebuild_parent_stat(ino_t this_inode, ino_t p_inode, int8_t d_type)
{
	return 0;
}

int32_t collect_dirmeta_children(DIR_META_TYPE *dir_meta, FILE *fptr,
	ino_t **dir_node_list, int64_t *num_dir_node,
	ino_t **nondir_node_list, int64_t *num_nondir_node,
	char **nondir_type_list)
{
	if (NOW_NO_ROOTS == TRUE) {
		printf("Now no roots\n");
		*num_dir_node = 0;
		*num_nondir_node = 0;
		*dir_node_list = NULL;
		*nondir_node_list = NULL;
		return 0;
	}

	*dir_node_list = (ino_t *) malloc(sizeof(ino_t) * 3);
	(*dir_node_list)[0] = 234;
	(*dir_node_list)[1] = 345;
	(*dir_node_list)[2] = 456;
	*num_dir_node = 3;
	*nondir_node_list = (ino_t *) malloc(sizeof(ino_t) * 2);
	(*nondir_node_list)[0] = 567;
	(*nondir_node_list)[1] = 678;
	*num_nondir_node = 2;

	return 0;
}

int32_t collect_dir_children(ino_t this_inode,
	ino_t **dir_node_list, int64_t *num_dir_node,
	ino_t **nondir_node_list, int64_t *num_nondir_node,
	char **nondir_type_list)
{
	if (this_inode > 80000) {
		*num_dir_node = 0;
		*num_nondir_node = 0;
		*dir_node_list = NULL;
		*nondir_node_list = NULL;
		*nondir_type_list = NULL;
		return 0;
	}

	*dir_node_list = malloc(sizeof(ino_t) * 1);
	(*dir_node_list)[0] = this_inode + 2;
	*num_dir_node = 1;

	*nondir_node_list = malloc(sizeof(ino_t) * 2);
	(*nondir_node_list)[0] = this_inode + 1;
	(*nondir_node_list)[1] = this_inode + 3;
	*num_nondir_node = 2;

	*nondir_type_list = malloc(sizeof(ino_t) * 2);
	(*nondir_type_list)[0] = S_IFREG;
	(*nondir_type_list)[1] = S_IFREG;

	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "%s/meta_%"PRIu64, METAPATH, (uint64_t)this_inode);
	return 0;
}

int32_t restore_meta_file(ino_t this_inode)
{
	char path[300];
	FILE *fptr;

	if (RESTORED_META_NOT_FOUND == TRUE)
		return -ENOENT;

	fetch_meta_path(path, this_inode);
	fptr = fopen(path, "w+");
	fseek(fptr, 0, SEEK_SET);
	fwrite(&exp_stat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&exp_filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fclose(fptr);	

	return 0;
}

int32_t write_super_block_head(void)
{
	return 0;
}

int32_t fetch_stat_path(char *pathname, ino_t this_inode)
{
	return 0;
}

int32_t update_sb_size()
{
	char path[300];
	struct stat tmpstat;

	sprintf(path, "rebuild_sb_running_folder/superblock");
	stat(path, &tmpstat);
	hcfs_system->systemdata.super_block_size = tmpstat.st_size;
	return 0;
}

int32_t init_rectified_system_meta(char restoration_stage)
{
	return 0;
}

int32_t rectify_space_usage()
{
	return 0;
}

int32_t fetch_all_parents(ino_t self_inode, int32_t *parentnum,
		ino_t **parentlist)
{
	if (NO_PARENTS == TRUE) {
		*parentnum = 0;
		*parentlist = NULL;
	}

	*parentnum = 1;
	*parentlist = (ino_t *)malloc(sizeof(ino_t));
	(*parentlist)[0] = 2;
	return 0;
}

int32_t meta_cache_lookup_dir_data(ino_t this_inode,
				   HCFS_STAT *inode_stat,
				   DIR_META_TYPE *dir_meta_ptr,
				   DIR_ENTRY_PAGE *dir_page,
				   META_CACHE_ENTRY_STRUCT *body_ptr)
{
	char name[200];
	int32_t idx;

	if (dir_meta_ptr) {
		memset(dir_meta_ptr, 0, sizeof(DIR_META_TYPE));
		dir_meta_ptr->tree_walk_list_head = 1234;
	}

	if (dir_page) {
		dir_page->num_entries = 50;
		memset(dir_page, 0, sizeof(DIR_ENTRY_PAGE));
		for (idx = 0; idx < dir_page->num_entries; idx++) {
			if (idx % 2)
				dir_page->dir_entries[idx].d_ino = 10;
			else
				dir_page->dir_entries[idx].d_ino = 20;
			sprintf(name, "test%d", idx);
			strcpy(dir_page->dir_entries[idx].d_name, name);
			dir_page->dir_entries[idx].d_type = D_ISREG;
		}
	}
	return 0;
}

META_CACHE_ENTRY_STRUCT *meta_cache_lock_entry(ino_t this_inode)
{
	return 0;
}

int32_t meta_cache_unlock_entry(META_CACHE_ENTRY_STRUCT *target_ptr)
{
	return 0;
}

int32_t lookup_delete_parent(ino_t self_inode, ino_t parent_inode)
{
	return 0;
}

int32_t dir_remove_entry(ino_t parent_inode, ino_t child_inode,
			const char *childname,
			mode_t child_mode, META_CACHE_ENTRY_STRUCT *body_ptr,
			BOOL is_external)
{
	remove_list[remove_count] = malloc(strlen(childname));
	strcpy(remove_list[remove_count++], childname);

	return 0;
}

int32_t read_system_max_inode(ino_t *ino_num)
{
	*ino_num = 1;
	return 0;
}
