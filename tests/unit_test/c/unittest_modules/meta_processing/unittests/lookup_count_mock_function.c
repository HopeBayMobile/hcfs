#include <sys/stat.h>
#include "mock_param.h"
#include "global.h"

int actual_delete_inode(ino_t this_inode, char d_type)
{
	check_actual_delete_table[this_inode] = TRUE;
	return 0;
}

int disk_checkdelete(ino_t this_inode)
{
	return 1;
}

int write_log(int level, char *format, ...)
{
	return 0;
}

