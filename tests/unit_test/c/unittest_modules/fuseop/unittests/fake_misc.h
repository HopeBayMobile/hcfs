#include <sys/types.h>

int fail_super_block_new_inode;
int fail_mknod_update_meta;
int before_mknod_created;

int fail_mkdir_update_meta;
int before_mkdir_created;

int before_update_file_data;
int after_update_block_page;
int test_fetch_from_backend;
unsigned char fake_block_status;
char readdir_metapath[100];
int fail_open_files;

BLOCK_ENTRY_PAGE updated_block_page;
struct stat updated_stat;
mode_t updated_mode;
uid_t updated_uid;
gid_t updated_gid;
time_t updated_atime;
time_t updated_mtime;
struct timespec updated_atim;
struct timespec updated_mtim;
