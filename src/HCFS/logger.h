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

#include <semaphore.h>
#include <stdio.h>
#include <sys/time.h>

#include "global.h"
#include "pthread.h"

#define MAX_LOG_FILE_SIZE 20971520 /* 20MB */
#define NUM_LOG_FILE 5
#define LOG_MSG_SIZE 128
#define FLUSH_TIME_INTERVAL 3

struct LOG_internal;
typedef struct LOG_internal LOG_STRUCT;

/* FIXME: introduce dedicated function to retrieve the instance of logger */
LOG_STRUCT *logptr;   /* Pointer to log structure */

int32_t open_log(char *filename);
int32_t write_log(int32_t level, const char *format, ...);
int32_t close_log(void);

#ifdef UNITTEST
FILE *logger_get_fileptr(LOG_STRUCT *);
sem_t *logger_get_semaphore(LOG_STRUCT *);
#endif  /* UNITTEST */

#endif  /* GW20_HCFS_LOGGER_H_ */
