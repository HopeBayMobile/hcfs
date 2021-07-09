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

#ifndef GW20_SRC_HFUSE_SYSTEM_H_
#define GW20_SRC_HFUSE_SYSTEM_H_

#include "params.h"
#include "pthread.h"
#include "inttypes.h"
#include "pthread_control.h"
#include "global.h"

int32_t init_hfuse(int8_t is_restoring);
int32_t init_hcfs_system_data(int8_t is_restoring);
int32_t sync_hcfs_system_data(char need_lock);
void init_backend_related_module(void);
int32_t init_event_notify_module(void);
void init_download_module(void);

#define CHILD_NUM 2
#define BATTERY_LOW_LEVEL 3
#define WRITE_SYS_INTERVAL 10

pthread_t delete_loop_thread;
pthread_t monitor_loop_thread;
pthread_t event_loop_thread;
pthread_t fuse_nofify_thread;
PTHREAD_REUSE_T write_sys_thread;
#ifdef _ANDROID_ENV_
pthread_t upload_loop_thread;
pthread_t cache_loop_thread;
#else
pid_t child_pids[CHILD_NUM];
pid_t this_pid;
int32_t proc_idx;
#endif /* _ANDROID_ENV_ */

#endif  /* GW20_SRC_HFUSE_SYSTEM_H_ */
