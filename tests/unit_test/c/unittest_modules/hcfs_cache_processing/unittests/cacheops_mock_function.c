#include <stdarg.h>
#include <inttypes.h>
#include "mock_params.h"
#include "super_block.h"
#include "hcfs_cachebuild.h"
#include "fuseop.h"
#include <stdarg.h>

void init_mock_system_config()
{
	system_config->blockpath = malloc(sizeof(char) * 100);
	strcpy(system_config->blockpath, "/tmp/testHCFS/blockpath");
}

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	char block_name[200];

	sprintf(block_name, "/tmp/testHCFS/run_cache_loop_block%" PRIu64 "_%lld",
			(uint64_t)this_inode, block_num);
	strcpy(pathname, block_name);
	
	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	char meta_name[200];

	sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%" PRIu64 "",
			(uint64_t)this_inode);
	strcpy(pathname, meta_name);

	return 0;
}

int build_cache_usage(void)
{
	return ;
}

int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (inode_ptr == NULL)
		inode_ptr = (SUPER_BLOCK_ENTRY *) malloc(sizeof(SUPER_BLOCK_ENTRY));
	
	inode_ptr->inode_stat.st_ino = 1;
	inode_ptr->inode_stat.st_mode = S_IFREG;

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
	if (this_inode > 0)
		nonempty_cache_hash_entries--;
	
	return NULL;
}

long long seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr, 
	long long target_page, long long hint_page)
{	
	long long ret = sizeof(struct stat) + sizeof(FILE_META_TYPE) + 
		target_page * sizeof(BLOCK_ENTRY_PAGE);
	
	return ret;
}

int write_log(int level, char *format, ...)
{
	va_list ap;

	/*va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);*/

	return 0;
}
int update_file_stats(FILE *metafptr, long long num_blocks_delta,
			long long num_cached_blocks_delta,
			long long cached_size_delta, ino_t thisinode)
{
	return 0;
}

