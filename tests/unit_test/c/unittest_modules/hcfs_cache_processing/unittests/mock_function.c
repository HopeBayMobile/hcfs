#include "mock_tool.h"

int fetch_block_path(char *pathname, ino_t this_inode, long long block_num)
{
	return 0;
}

void init_mock_system_config()
{
	system_config.blockpath = malloc(sizeof(char) * 100);
	strcpy(system_config.blockpath, "/tmp/blockpath");
}
