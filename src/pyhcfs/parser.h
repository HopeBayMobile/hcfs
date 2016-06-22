#include <inttypes.h>

#include "fuseop.h"
#include "../HCFS/fuseop.h"
#include "FS_manager.h"
#include "../HCFS/FS_manager.h"
#include "hcfs_stat.h"
#include "../HCFS/hcfs_stat.h"

/* Private declaration */
/* size 128 Bytes */
struct stat_aarch64 {
	uint64_t dev;
	uint64_t ino;
	uint32_t mode;
	uint32_t nlink;
	uint32_t uid;
	uint32_t gid;
	uint64_t rdev;
	uint64_t __pad1;
	int64_t size;
	int32_t blksize;
	int32_t __pad2;
	int64_t blocks;
	int64_t atime;
	uint64_t atime_nsec;
	int64_t mtime;
	uint64_t mtime_nsec;
	int64_t ctime;
	uint64_t ctime_nsec;
	uint32_t __unused4;
	uint32_t __unused5;
};

/* Public declaration */

typedef struct {
	uint64_t inode;
	char name[256];
} PORTABLE_DIR_ENTRY;

typedef struct {
	int32_t result;
	int32_t file_type;
	uint64_t child_number;
	HCFS_STAT hcfs_stat;
} RET_META;

int32_t list_external_volume(char *meta_path,
			     PORTABLE_DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);

int32_t parse_meta(char *meta_path, RET_META *meta);
