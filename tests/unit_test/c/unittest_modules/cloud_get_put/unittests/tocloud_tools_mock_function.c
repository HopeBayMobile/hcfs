#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "global.h"
#include "fuseop.h"

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	sprintf(pathname, "tocloud_tools_test_folder/mock_meta_%"PRIu64,
			(uint64_t)this_inode);
	return 0;
}

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	return 0;
}

int32_t set_block_dirty_status(char *path, FILE *fptr, char status)
{
	return 0;
}

off_t check_file_size(const char *path)
{
	return 0;
}

int32_t update_file_stats(FILE *metafptr, int64_t num_blocks_delta,
	int64_t num_cached_blocks_delta, int64_t cached_size_delta,
	int64_t dirty_data_size_delta, ino_t thisinode)
{
	return 0;
}

int32_t change_system_meta(int64_t system_data_size_delta,
	int64_t meta_size_delta, int64_t cache_data_size_delta,
	int64_t cache_blocks_delta, int64_t dirty_cache_delta,
	int64_t unpin_dirty_data_size, BOOL need_sync)
{
	return 0;
}

int32_t change_system_meta_ignore_dirty(ino_t this_inode,
					int64_t system_data_size_delta,
					int64_t meta_size_delta,
					int64_t cache_data_size_delta,
					int64_t cache_blocks_delta,
					int64_t dirty_cache_delta,
					int64_t unpin_dirty_data_size,
					BOOL need_sync)
{
	return 0;
}

int32_t change_action(int32_t fd, char new_action)
{
	return 0;
}

int64_t query_status_page(int32_t fd, int64_t block_index)
{
	return 0;
}

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page)
{
	return 0;
}

int32_t select_upload_thread(char is_block, char is_delete,
#if (DEDUP_ENABLE)
		char is_upload,
		uint8_t old_obj_id[],
#endif
		ino_t this_inode, int64_t block_count,
		int64_t seq, off_t page_pos,
		int64_t e_index, int32_t progress_fd,
		char backend_delete_type)
{
	return 0;
}

void dispatch_delete_block(int32_t which_curl)
{
	return;
}
