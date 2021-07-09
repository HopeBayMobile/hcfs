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
#include <sys/stat.h>

#define TMP_META_DIR "/tmp/tmp_meta_dir"
#define TMP_META_FILE_PATH "/tmp/tmp_meta_dir/tmp_meta"
/*
	INO__FETCH_META_PATH_FAIL causes mock fetch_meta_path() fail and return -1
	INO__FETCH_META_PATH_SUCCESS makes mock fetch_meta_path() success and return 0
	INO__FETCH_META_PATH_ERR makes mock fetch_meta_path() return 0 but fill char *path with nothing
 */
enum { INO__FETCH_META_PATH_FAIL, 
	INO__FETCH_META_PATH_SUCCESS, 
	INO__FETCH_META_PATH_ERR };

HCFS_STAT *generate_mock_stat(ino_t inode_num);

char MOCK_FINISH_UPLOADING;
char MOCK_RETURN_VAL;
ino_t num_stat_rebuilt;
