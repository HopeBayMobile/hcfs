#include <errno.h>
#include <jansson.h>
#include "dir_statistics.h"
#include "string.h"
#include "mock_params.h"

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

char *dec_backup_usermeta(char *path)
{
	if (dec_success)
		return malloc(10 * sizeof(char));
	else
		return NULL;
}

void json_delete(json_t *json)
{
}

json_t *json_loads(const char *input, size_t flags, json_error_t *error)
{
	return 1;
}

json_t *json_object_get(const json_t *object, const char *key)
{
	json_t *ret;

	if (json_file_corrupt)
		return NULL;

	ret = malloc(sizeof(json_t));
	ret->type = JSON_INTEGER; 
	return ret; 
}

json_int_t json_integer_value(const json_t *integer)
{
	free((void *)integer);
	return 5566;
}
