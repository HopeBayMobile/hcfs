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
#include <sys/types.h>
#include "meta.h"

#define TEST_LISTDIR_INODE 17
#define TEST_APPDIR_INODE 24
#define TEST_APPAPK_INODE 25
#define TEST_APPMIN_INODE 26
#define TEST_APPROOT_INODE 1
int32_t fail_super_block_new_inode;
int32_t fail_mknod_update_meta;
int32_t before_mknod_created;

int32_t fail_mkdir_update_meta;
int32_t before_mkdir_created;

int32_t before_update_file_data;
int32_t root_updated;
int32_t after_update_block_page;
int32_t test_fetch_from_backend;
int32_t num_stat_rebuilt;
uint8_t fake_block_status;
uint32_t fake_paged_out_count;
char readdir_metapath[100];
int32_t fail_open_files;
int32_t tmp_apk_location;
BOOL cached_minapk;
BOOL exists_minapk;

BLOCK_ENTRY_PAGE updated_block_page;
HCFS_STAT updated_stat, updated_root;
mode_t updated_mode;
uid_t updated_uid;
gid_t updated_gid;
time_t updated_atime;
time_t updated_mtime;
struct timespec updated_atim;
struct timespec updated_mtim;

int32_t remove_apk_success;
char verified_apk_name[400];
#define CORRECT_VALUE_SIZE 24269
