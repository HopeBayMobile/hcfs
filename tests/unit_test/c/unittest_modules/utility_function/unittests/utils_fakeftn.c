#include "dir_statistics.h"
#include "string.h"
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

/* For encrypted config */
#define KEY_SIZE 32

FILE *get_decrypt_configfp(unsigned char *config_path)
{
	FILE *configfp = NULL;

	configfp = fopen(config_path, "r");
	return configfp;
}

unsigned char *get_key(char *passphrase)
{
	unsigned char *ret =
	    (unsigned char *)calloc(KEY_SIZE, sizeof(unsigned char));

	sprintf(ret, "mock encrypt key for test");
	return ret;
}
