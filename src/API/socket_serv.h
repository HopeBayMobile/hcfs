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

#ifndef GW20_HCFSAPI_SERV_H_
#define GW20_HCFSAPI_SERV_H_

#include <pthread.h>
#include <inttypes.h>

#define MAX_THREAD 16

typedef struct {
	uint32_t name;
	int32_t (*cmd_fn)(char *largebuf,
			  int32_t arg_len,
			  char *resbuf,
			  int32_t *res_size);
} SOCK_CMDS;

typedef struct {
	uint32_t name;
	int32_t (*cmd_fn)(char *largebuf,
			  int32_t arg_len,
			  char *resbuf,
			  int32_t *res_size,
			  pthread_t *tid);
	pthread_t async_thread;
} SOCK_ASYNC_CMDS;

typedef struct {
	char in_used;
	pthread_attr_t attr;
	pthread_t thread;
	int32_t fd;
} SOCK_THREAD;

#endif  /* GW20_HCFSAPI_SERV_H_ */
