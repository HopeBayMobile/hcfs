#include <sys/types.h>

#define TEST_LISTDIR_INODE 17
int32_t fail_super_block_new_inode;
int32_t fail_mknod_update_meta;
int32_t before_mknod_created;

int32_t fail_mkdir_update_meta;
int32_t before_mkdir_created;

int32_t before_update_file_data;
int32_t root_updated;
int32_t after_update_block_page;
int32_t test_fetch_from_backend;
int32_t num_stat_rebuilt;
uint8_t fake_block_status;
uint32_t fake_paged_out_count;
char readdir_metapath[100];
int32_t fail_open_files;

BLOCK_ENTRY_PAGE updated_block_page;
HCFS_STAT updated_stat, updated_root;
mode_t updated_mode;
uid_t updated_uid;
gid_t updated_gid;
time_t updated_atime;
time_t updated_mtime;
struct timespec updated_atim;
struct timespec updated_mtim;

#define CORRECT_VALUE_SIZE 24269
