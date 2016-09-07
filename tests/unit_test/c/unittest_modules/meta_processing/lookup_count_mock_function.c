#include <sys/stat.h>
#include "mock_param.h"
#include "global.h"

int32_t actual_delete_inode(ino_t this_inode, char d_type)
{
	check_actual_delete_table[this_inode] = TRUE;
	return 0;
}

int32_t disk_checkdelete(ino_t this_inode)
{
	return 1;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

