/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <jansson.h>
#include <inttypes.h>
#include "dir_statistics.h"
#include "string.h"
#include "mock_params.h"

int32_t sync_hcfs_system_data(char need_lock)
{
	return 0;
}

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}
int32_t update_dirstat_file(ino_t thisinode, DIR_STATS_TYPE *newstat)
{
	return 0;
}

int32_t prepare_FS_database_backup(void)
{
	return 0;
}

void init_backend_related_module(void)
{
	return;
}

/* For encrypted config */
#define KEY_SIZE 32

FILE *get_decrypt_configfp(uint8_t *config_path)
{
	FILE *configfp = NULL;

	configfp = fopen(config_path, "r");
	return configfp;
}

uint8_t *get_key(char *passphrase)
{
	uint8_t *ret =
	    (uint8_t *)calloc(KEY_SIZE, sizeof(uint8_t));

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

void *monitor_loop(void *ptr)
{
	return NULL;
}

void run_cache_loop(void)
{
	return;
}

void init_download_module(void)
{
	return;
}
int32_t restore_stage1_reduce_cache(void)
{
	return 0;
}
int32_t notify_restoration_result(int8_t stage, int32_t result)
{
	return 0;
}
void start_download_minimal(void)
{
	return;
}
int32_t init_gdrive_token_control(void)
{
	return 0;
}
