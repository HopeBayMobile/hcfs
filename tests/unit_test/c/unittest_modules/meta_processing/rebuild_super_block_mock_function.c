#include "logger.h"
#include "super_block.h"
#include "fuseop.h"
#include "mount_manager.h"
#include "rebuild_super_block_params.h"

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
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
	return 0;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
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

int32_t collect_dir_children(DIR_META_TYPE *dir_meta, FILE *fptr,
	ino_t **dir_node_list, int64_t *num_dir_node,
	ino_t **nondir_node_list, int64_t *num_nondir_node,
	char **nondir_type_list)
{
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	return 0;
}

int32_t restore_meta_file(ino_t this_inode)
{
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
