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
#include <stdio.h>
#include <sys/types.h>

#define NO_PARENT_INO 5
#define ONE_PARENT_INO 6
#define FAKE_EXIST_PARENT 4
#define FAKE_ROOT 2
#define FAKE_GRAND_PARENT 3
#define MOCK_META_PATH "/tmp/this_meta"
#define MOCK_DIRSTAT_PATH "/tmp/dirstat"
#define MOCK_PATHLOOKUP_PATH "/tmp/pathlookup"

int32_t fake_num_parents;

