#include "mock_params.h"
#include "super_block.h"
#include "hcfs_cachebuild.h"
#include "fuseop.h"

void init_mock_system_config()
{
	system_config.blockpath = malloc(sizeof(char) * 100);
	strcpy(system_config.blockpath, "/tmp/blockpath");
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	return 0;
}

int build_cache_usage(void)
{
	return ;
}

int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int sync_hcfs_system_data(char need_lock)
{
	return 0;
}

int super_block_mark_dirty(ino_t this_inode)
{
	return 0;	
}

CACHE_USAGE_NODE *return_cache_usage_node(ino_t this_inode)
{
	return NULL;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	long long target_page, long long hint_page)
{
	return 0;
}

int write_log(int level, char *format, ...)
{
	return 0;
}
