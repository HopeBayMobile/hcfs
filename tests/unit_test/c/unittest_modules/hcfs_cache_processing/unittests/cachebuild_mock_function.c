#include "mock_params.h"

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	int sub_dir;
	char tmpname[500];
	sub_dir = (this_inode + block_num) % NUMSUBDIR;
#ifdef ARM_32bit_
	sprintf(tmpname, "%s/sub_%d/block%lld_%lld", BLOCKPATH, sub_dir,
			this_inode, block_num);
#else
	sprintf(tmpname, "%s/sub_%d/block%ld_%lld", BLOCKPATH, sub_dir,
			this_inode, block_num);
#endif
	strcpy(pathname, tmpname);
	return 0;
}

void init_mock_system_config()
{
	system_config.blockpath = malloc(sizeof(char) * 100);
	strcpy(BLOCKPATH, "testpatterns");
}

int write_log(int level, char *format, ...)
{
	return 0;
}
