#include "dir_statistics.h"
int sync_hcfs_system_data(char need_lock)
{
	return 0;
}

int write_log(int level, char *format, ...)
{
	return 0;
}
int update_dirstat_file(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	return 0;
}

int prepare_FS_database_backup(void)
{
	return 0;
}

void init_backend_related_module()
{
	return;
}
