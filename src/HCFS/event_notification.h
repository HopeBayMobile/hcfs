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

#ifndef GW20_SRC_EVENT_NOTIFY
#define GW20_SRC_EVENT_NOTIFY

#include <inttypes.h>
#include <jansson.h>
#include <pthread.h>
#include <semaphore.h>

#include "global.h"

#define SERVERREPLYOK 1 /* Server should reply after event msg received */
#define MAX_NOTIFY_SERVER_LENGTH 256
#define EVENT_QUEUE_SIZE 64
#define MAX_NUM_EVENT_SEND 3

typedef struct {
	/* To control active/inactive queue worker */
	pthread_mutex_t worker_active_lock;
	pthread_cond_t worker_active_cond;
	/* Lock for queue operations */
	sem_t queue_access_sem;
	/* Wait when queue is full */
	sem_t queue_full_sem;
	int32_t num_events;
	int32_t head;
	int32_t rear;
	json_t *events[EVENT_QUEUE_SIZE];
} EVENT_QUEUE;

extern EVENT_QUEUE *event_queue;


int32_t init_event_queue(void);
int32_t set_event_notify_server(const char *path);

void *event_worker_loop(void *ptr);
void destroy_event_worker_loop_thread(void);

int32_t event_enqueue(int32_t event_id, json_t *event, BOOL blocking);
int32_t event_dequeue(int32_t num_events);

int32_t send_event_to_server(int32_t fd, const char *events_in_json);

int32_t add_notify_event(int32_t event_id,
			 const char *event_info_json_str,
			 char blocking);
int32_t add_notify_event_obj(int32_t event_id, json_t *event, char blocking);

#endif /* GW20_SRC_EVENT_NOTIFY */
