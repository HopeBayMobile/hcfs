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
#include "fuseop.h"

char NOW_NO_ROOTS;
char NOW_TEST_RESTORE_META;
char RESTORED_META_NOT_FOUND;
char NO_PARENTS; 
char record_inode[100000];
sem_t record_inode_sem;
ino_t max_record_inode;

struct stat exp_stat;
FILE_META_TYPE exp_filemeta;

char *remove_list[100];
int32_t remove_count;
