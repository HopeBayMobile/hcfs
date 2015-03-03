#include <sys/types.h>

int fail_super_block_new_inode;
int fail_mknod_update_meta;
int before_mknod_created;

int fail_mkdir_update_meta;
int before_mkdir_created;

int before_update_file_data;
mode_t updated_mode;
uid_t updated_uid;
gid_t updated_gid;
