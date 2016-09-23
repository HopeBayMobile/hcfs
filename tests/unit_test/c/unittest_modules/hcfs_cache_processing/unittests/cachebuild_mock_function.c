#include <inttypes.h>
#include "mock_params.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "global.h"
#include "meta.h"

#define TRUE 1
#define FALSE 0

int32_t fetch_block_path(char *pathname, ino_t this_inode, int64_t block_num)
{
	int32_t sub_dir;
	char tmpname[500];
	sub_dir = (this_inode + block_num) % NUMSUBDIR;
	sprintf(tmpname, "%s/sub_%d/block%" PRIu64 "_%lld", BLOCKPATH, sub_dir,
			(uint64_t)this_inode, block_num);
	strcpy(pathname, tmpname);
	return 0;
}

void init_mock_system_config()
{
	system_config->blockpath = malloc(sizeof(char) * 100);
	strcpy(BLOCKPATH, "testpatterns");
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int32_t get_block_dirty_status(char *path, FILE *fptr, char *status)
{
#ifdef _ANDROID_ENV_

	struct stat tmpstat;
	stat(path, &tmpstat);
		/* Use sticky bit to store dirty status */

	if ((tmpstat.st_mode & S_ISVTX) == 0)
		*status = FALSE;
	else
		*status = TRUE;
#else
	char tmpstr[5];
	getxattr(path, "user.dirty", (void *) tmpstr, 1);
	if (strncmp(tmpstr, "T", 1) == 0)
		*status = TRUE;
	else
		*status = FALSE;
#endif
	return 0;
}
