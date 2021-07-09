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

#ifndef GW20_API_INTERFACE_H_
#define GW20_API_INTERFACE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <semaphore.h>
#include <time.h>

#ifdef _ANDROID_ENV_
#include <pthread.h>
#include "pthread_control.h"
#endif

#include "global.h"

#define MAX_API_THREADS 4
#define INIT_API_THREADS 2
#define PROCESS_WINDOW 60
#define INCREASE_RATIO 0.8
#ifdef UNITTEST
  #define SOCK_PATH "hcfs_reporter"
#else
  #define SOCK_PATH "/dev/shm/hcfs_reporter"
#endif
#define API_SERVER_MONITOR_TIME {30,0}

/* Message format for an API request:
	(From the first byte)
	API code, size (uint32_t)
	Total length of arguments, size (uint32_t)
	Arguments (To be stored in a char array and pass to the handler
		for each API)

   Message format for an API response:
	(From the first byte)
	Total length of response, size (uint32_t)
	Response (as a char string)
*/

typedef struct {
	struct sockaddr_un addr;
	int32_t fd;
} SOCKET;

typedef struct {
	SOCKET sock;
	/* API thread (using local socket) */
	PTHREAD_T local_thread[MAX_API_THREADS];
	PTHREAD_T monitor_thread;
	int32_t num_threads;
	int32_t job_count[PROCESS_WINDOW];
	float job_totaltime[PROCESS_WINDOW];
	time_t last_update;
	sem_t job_lock;
	sem_t shutdown_sem;
	BOOL api_shutting_down;
} API_SERVER_TYPE;

API_SERVER_TYPE *api_server;

int32_t init_api_interface(void);
int32_t destroy_api_interface(void);
void api_module(void *index1);
void api_server_monitor(void);

#endif  /* GW20_API_INTERFACE_H_ */
