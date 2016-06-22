#include "fuseop.h"
#include "FS_manager.h"
#include "fuseop.h"
#include "hcfs_stat.h"

typedef struct {
	uint64_t inode;
	char name[256];
} PORTABLE_DIR_ENTRY;
int32_t list_external_volume(char *meta_path,
		PORTABLE_DIR_ENTRY **ptr_ret_entry,
		uint64_t *ret_num);
/* size 128 Bytes */
struct stat_aarch64 {
	unsigned long dev;
	unsigned long ino;
	unsigned int mode;
	unsigned int nlink;
	uid_t uid;
	gid_t gid;
	unsigned long rdev;
	unsigned long __pad1;
	long size;
	int blksize;
	int __pad2;
	long blocks;
	long atime;
	unsigned long atime_nsec;
	long mtime;
	unsigned long mtime_nsec;
	long ctime;
	unsigned long ctime_nsec;
	unsigned int __unused4;
	unsigned int __unused5;
};
