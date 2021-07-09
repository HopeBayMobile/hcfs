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

#ifndef GW20_HCFS_LOGGER_H_
#define GW20_HCFS_LOGGER_H_

#include <inttypes.h>
#include <semaphore.h>
#include <stdio.h>

#include "global.h"
#include "pthread.h"

#define LOG_COMPRESS TRUE

#define MAX_LOG_FILE_SIZE 20971520 /* 20MB */
#define NUM_LOG_FILE 5
#define LOG_MSG_SIZE 128
#define FLUSH_TIME_INTERVAL 3

typedef struct {
	sem_t logsem;
	FILE *fptr;
	int32_t now_log_size;
	char *log_filename;
	char *latest_log_msg;
	char *now_log_msg;
	int32_t repeated_times;
	struct timeval latest_log_time;
	time_t latest_log_start_time;
	pthread_t tid;
	pthread_attr_t flusher_attr;
	BOOL flusher_is_created;
} LOG_STRUCT;

LOG_STRUCT *logptr;   /* Pointer to log structure */

int32_t open_log(char *filename);
int32_t write_log(int32_t level, char *format, ...);
int32_t close_log(void);

#endif  /* GW20_HCFS_LOGGER_H_ */
