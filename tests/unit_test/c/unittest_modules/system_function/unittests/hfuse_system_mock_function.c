#include "sys/stat.h"
#include "curl/curl.h"
#include "fuseop.h"
#include "hcfscurl.h"
#include "mock_params.h"

int super_block_init(void)
{
	return 0;
}

int init_pathname_cache(void)
{
	return 0;
}

int init_system_fh_table(void)
{
	return 0;
}

int fetch_meta_path(char *pathname, ino_t this_inode)
{
	return 0;
}

ino_t super_block_new_inode(struct stat *in_stat)
{
	return 0;
}

int init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode, 
                                                long long this_page_pos)
{
	return 0;
}

int super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}

int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	return 0;
}

void hcfs_destroy_backend(CURL *curl)
{

}

void ENGINE_load_builtin_engines(void)
{

}

int ENGINE_register_all_complete(void)
{
	return 0;
}

int read_system_config(char *config_path)
{
	return 0;
}

int validate_system_config(void)
{
	return 0;
}

int hcfs_list_container(CURL_HANDLE *curl_handle)
{
	return 0;
}

void *delete_loop(void *arg)
{
	return NULL;
}

void upload_loop(void)
{

}

void run_cache_loop(void)
{
	
}

int hook_fuse(int argc, char **argv)
{
	return 0;
}
