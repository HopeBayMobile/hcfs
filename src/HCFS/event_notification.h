/*************************************************************************
*
* Copyright ?2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: event_notification.c
* Abstract: The header file for event notification functions
*
* Revision History
* 2016/7/7 Yuxun created this file.
*
**************************************************************************/

#ifndef GW20_SRC_EVENT_NOTIFY
#define GW20_SRC_EVENT_NOTIFY

#include <inttypes.h>
#include <semaphore.h>
#include <pthread.h>
#include <jansson.h>

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
int32_t set_event_notify_server(char *path);

void *event_worker_loop(void *ptr);
void destroy_event_worker_loop_thread(void);

int32_t event_enqueue(int32_t event_id, char *event_info_json_str,
		      char blocking);
int32_t event_dequeue(int32_t num_events);

int32_t send_event_to_server(int32_t fd, char *events_in_json);

int32_t add_notify_event(int32_t event_id, char *event_info_json_str,
			 char blocking);

#endif /* GW20_SRC_EVENT_NOTIFY */
