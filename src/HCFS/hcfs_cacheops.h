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
#ifndef GW20_HCFS_HCFS_CACHEOPS_H_
#define GW20_HCFS_HCFS_CACHEOPS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

#define SCAN_INT 300

int32_t sleep_on_cache_full(void);
void notify_sleep_on_cache(int32_t cache_replace_status);
#ifdef _ANDROID_ENV_
void *run_cache_loop(void *ptr);
#else
void run_cache_loop(void);
#endif

int64_t get_cache_limit(const char pin_type);
int64_t get_pinned_limit(const char pin_type);

#endif  /* GW20_HCFS_HCFS_CACHEOPS_H_ */
