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
#ifndef SRC_HCFS_MONITOR_H_
#define SRC_HCFS_MONITOR_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "params.h"

#define MONITOR_RETRYING 2

void *monitor_loop(void *ptr);
int32_t check_backend_status(void);
void destroy_monitor_loop_thread(void);
double diff_time(const struct timespec *start, const struct timespec *end);
void update_backend_status(register BOOL status, struct timespec *status_time);
void update_sync_state(void);
void _write_monitor_loop_status_log(double duration);
void force_retry_conn(void);
BOOL now_retry_conn;
BOOL manual_retry_conn;

#endif  /* SRC_HCFS_MONITOR_H_ */
