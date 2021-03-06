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
#include "params.h"
#include "global.h"
#include "utils.h"
#include <semaphore.h>

#define HTTP_OK 200
#define HTTP_FAIL 500

/* Define for hcfs_fromcloud */
#define BLOCK_NUM__FETCH_SUCCESS 1
#define BLOCK_NUM__FETCH_FAIL 2
#define TEST_DOWNLOAD_OBJ "data_test_download"

#define EXTEND_FILE_SIZE 1234 // Used to truncate file size

/* Define for hcfs_clouddelete */
#define INODE__FETCH_TODELETE_PATH_SUCCESS 1
#define INODE__FETCH_TODELETE_PATH_FAIL 2

#define TODELETE_PATH "tmpdir/todelete_meta_path"
#define MOCK_META_PATH "tmpdir/mock_file_meta"

char **objname_list;
int32_t objname_counter;
int32_t mock_total_page;
sem_t objname_counter_sem;

char no_backend_stat;

char upload_ctl_todelete_blockno[100];
SYSTEM_CONF_STRUCT *system_config;
char CACHE_FULL;
char OPEN_BLOCK_PATH_FAIL;
char OPEN_META_PATH_FAIL;
char NOW_STATUS;
char FETCH_BACKEND_BLOCK_TESTING;

typedef struct {
	int32_t *to_handle_inode; // inode_t to be deleted by delete_loop()
	int32_t tohandle_counter; // counter used by super_block
	int32_t num_inode; // total number of to_delete_inode
} LoopTestData;

typedef struct{
	int32_t *record_handle_inode; // Recorded in inode array when inode is called
	int32_t record_inode_counter; 
	sem_t record_inode_sem;
} LoopToVerifiedData;

LoopTestData test_data;
LoopToVerifiedData to_verified_data;
LoopTestData *shm_test_data;  // Used in upload_loop_unittest as expected value
LoopToVerifiedData *shm_verified_data; // Used in upload_loop_unittest as actual value

/* For atomic tocloud */
typedef struct {
	sem_t record_sem;
	ino_t record_uploading_inode[500];
	int total_inode;
} TEST_REVERT_STRUCT;

TEST_REVERT_STRUCT test_sync_struct;
TEST_REVERT_STRUCT test_delete_struct;

char is_first_upload;
char fetch_from_cloud_fail;
char usermeta_notfound;
int64_t fake_access_time;
BOOL test_upload_delay;
