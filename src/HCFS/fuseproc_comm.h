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

#ifndef GW20_FUSEPROC_COMM_H_
#define GW20_FUSEPROC_COMM_H_

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>

/* Define socket path for process communicating */
#define MAX_FUSE_COMMUNICATION_THREAD 4

int32_t init_fuse_proc_communication(pthread_t *communicate_tid, int32_t *socket_fd);
int32_t destroy_fuse_proc_communication(pthread_t *communicate_tid, int32_t socket_fd);
#endif
