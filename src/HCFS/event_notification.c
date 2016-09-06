/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: event_notification.c
* Abstract: The c source file for event notification functions
*
* Revision History
* 2016/7/7 Yuxun created this file.
*
**************************************************************************/

#include "event_notification.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <math.h>
#include <jansson.h>

#include "global.h"
#include "fuseop.h"
#include "event_filter.h"
#include "macro.h"
#include "logger.h"

#define DEFAULT_NOTIFY_SERVER "mgmt.api.sock"

char *notify_server_path = NULL;
EVENT_QUEUE *event_queue;


/************************************************************************
 *
 *  Function name: init_event_queue
 *         Inputs: NONE
 *         Output: Integer
 *        Summary: Initialize event queue and event filter.
 *   Return value: 0 if successful. Otherwise returns the negation of the
 *                 appropriate error code.
 *
 ***********************************************************************/
int32_t init_event_queue()
{
	int32_t ret_code;

	event_queue = (EVENT_QUEUE *)calloc(1, sizeof(EVENT_QUEUE));
	if (event_queue == NULL)
		return -errno;

	event_queue->num_events = 0;
	event_queue->head = -1;
	event_queue->rear = -1;

	ret_code = sem_init(&(event_queue->queue_access_sem), 0, 1);
	if (ret_code < 0) {
		free(event_queue);
		return -errno;
	}

	ret_code = sem_init(&(event_queue->queue_full_sem), 0, EVENT_QUEUE_SIZE);
	if (ret_code < 0) {
		free(event_queue);
		return -errno;
	}

	ret_code = pthread_mutex_init(&(event_queue->worker_active_lock), NULL);
	if (ret_code != 0) {
		free(event_queue);
		return -ret_code;
	}

	ret_code = pthread_cond_init(&(event_queue->worker_active_cond), NULL);
	if (ret_code != 0) {
		free(event_queue);
		return -ret_code;
	}

	/* Set default notify server loc */
	notify_server_path = malloc(strlen(DEFAULT_NOTIFY_SERVER) + 1);
	if (notify_server_path == NULL) {
		write_log(4, "%s, errno - %d\n"
			"Failed to set default notify server", errno);
		return -errno;
	}
	strcpy(notify_server_path, DEFAULT_NOTIFY_SERVER);

	return 0;
}

/************************************************************************
 *
 * Function name: _get_server_conn
 *        Inputs: NONE
 *        Output: Integer
 *       Summary: Returns socket fd of notify server.
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
 *
 ***********************************************************************/
static int32_t _get_server_conn(char *path)
{
	int32_t fd, status, sock_len;
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 0;
	strcpy(&(addr.sun_path[1]), path);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -errno;
	sock_len = 1 + strlen(path) + offsetof(struct sockaddr_un, sun_path);
	status = connect(fd, (const struct sockaddr *) &addr, sock_len);
	if (status < 0) {
		close(fd);
		return -errno;
	}

	return fd;
}

/************************************************************************
 *
 * Function name: set_event_notify_server
 *        Inputs: char *server_path
 *        Output: Integer
 *       Summary: To Set the location of notify server. Will send a test
 *                event to make sure server is running.
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
 ***********************************************************************/
int32_t set_event_notify_server(char *server_path)
{
	int32_t ret_code;
	int32_t server_fd = 0;
	char *new_server_path, *temp_server_path;
	char msg_to_send[32];
	json_t *test_event = NULL;

	new_server_path = malloc(strlen(server_path) + 1);
	if (new_server_path == NULL) {
		write_log(4, "Failed to set notify server, errno - %d\n", errno);
		ret_code = -errno;
		goto error_handle;
	}
	strcpy(new_server_path, server_path);

	/* Construct test event */
	sprintf(msg_to_send, "[{\"event_id\":%d}]", TESTSERVER);

	/* Send test event */
	server_fd = _get_server_conn(new_server_path);
	if (server_fd < 0) {
		write_log(4,
			"Failed to connect notify server, errno - %d\n", -server_fd);
		ret_code = server_fd;
		goto error_handle;
	}

	ret_code = send_event_to_server(server_fd, msg_to_send);
	if (ret_code < 0) {
		write_log(4,
			"Failed to send event msg to server, errno - %d\n", -ret_code);
		goto error_handle;
	}

	temp_server_path = notify_server_path;
	notify_server_path = new_server_path;
	free(temp_server_path);

	ret_code = 0;
	goto done;

error_handle:
	free(new_server_path);

done:
	if (server_fd > 0)
		close(server_fd);

	if (test_event != NULL)
		json_decref(test_event);

	return ret_code;
}

/************************************************************************
 *
 * Function name: event_enqueue
 *        Inputs: int32_t event_id, char *event_info_json_str
 *                char blocking
 *        Output: Integer
 *       Summary: Add an (event_id) event with (event_info_json_str) info
 *                to event queue. The rule of (event_info_json_str) and
 *                (blocking) are described in add_notify_event function.
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
 ***********************************************************************/
int32_t event_enqueue(int32_t event_id, char *event_info_json_str,
		      char blocking)
{
	int32_t ret_code;
	json_t *event = NULL;
	json_error_t json_err;

	if (event_info_json_str == NULL) {
		event = json_object();
		if (event == NULL) {
			write_log(4, "Failed to initialize event - Enqueue aborted.");
			return -errno;
		}

	} else {
		event = json_loads(event_info_json_str, JSON_DECODE_ANY, &json_err);
		if (event == NULL) {
			write_log(4, "%s, error - %s\n",
					"Failed to parse event info", json_err.text);
			return -EINVAL;
		} else if (!json_is_object(event)) {
			write_log(4, "Event info must be a json object.\n");
			return -EINVAL;
		}
	}

	ret_code =
		json_object_set_new(event, "event_id", json_integer(event_id));
	if (ret_code < 0) {
		write_log(4, "Failed to construct event - Enqueue aborted.");
		return -errno;
	}

	if (blocking == FALSE &&
			event_queue->num_events >= EVENT_QUEUE_SIZE) {
		write_log(4, "%s - %s",
				"Failed to add to event queue",
				"Event queue full.");
		return -ENOSPC;
	}

	/* Reserve one space of queue */
	sem_wait(&(event_queue->queue_full_sem));
	/* Wait lock */
	sem_wait(&(event_queue->queue_access_sem));

	if (event_queue->num_events <= 0) {
		event_queue->head = 0;
		event_queue->rear = 0;
	} else {
		event_queue->rear =
			(event_queue->rear + 1) % EVENT_QUEUE_SIZE;
	}

	event_queue->events[event_queue->rear] = event;
	event_queue->num_events += 1;

	/* Update event timestamp */
	event_filters[event_id].last_send_timestamp =
			(int64_t)time(NULL);

	/* Unlock */
	sem_post(&(event_queue->queue_access_sem));

	write_log(8, "Event enqueue was successful.");

	return 0;
}

/************************************************************************
 *
 * Function name: event_dequeue
 *        Inputs: int32_t num_events
 *        Output: Integer
 *       Summary: Remove (num_events) events from event queue. The range
 *                of (num_events) should be 0 < num_events <= EVENT_QUEUE_SIZE.
 *  Return value: Return total number of events dequeued if successful.
 *                Otherwise returns the negation of the appropriate error code.
 *
 ***********************************************************************/
int32_t event_dequeue(int32_t num_events)
{
	int32_t idx;

	if (num_events <= 0 || num_events > EVENT_QUEUE_SIZE)
		return -EINVAL;

	if (num_events > event_queue->num_events)
		num_events = event_queue->num_events;

	if (event_queue->num_events <= 0) {
		write_log(4, "Failed to dequeue - Event queue is empty.");
		return -ENOENT;
	}

	/* Wait lock */
	sem_wait(&(event_queue->queue_access_sem));

	for (idx = num_events; idx > 0; idx--) {
		json_decref(event_queue->events[event_queue->head]);
		event_queue->num_events--;
		if (event_queue->num_events == 0) {
			event_queue->head = -1;
			event_queue->rear = -1;
		} else {
			event_queue->head =
				(event_queue->head + 1) % EVENT_QUEUE_SIZE;
		}
		/* Release one space of queue */
		sem_post(&(event_queue->queue_full_sem));
	}

	/* Unlock */
	sem_post(&(event_queue->queue_access_sem));

	write_log(8, "Event dequeue was successful. Total %d events removed.",
			num_events);

	return num_events;
}

/************************************************************************
 *
 * Function name: send_event_to_server
 *        Inputs: int32_t fd, char *events_in_json
 *        Output: Integer
 *       Summary: Send the given json format string to notify server.
 *                Input (events_in_json) should be a valid json format
 *                str and follow the rule that an array contains multiple
 *                event_str likes "[event_1_json, event_2_json, ...]".
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
 *
 ***********************************************************************/
int32_t send_event_to_server(int32_t fd, char *events_in_json)
{
	int32_t r_size;
	int32_t ret_val = 0;
	char event_msg[strlen(events_in_json) + 2];

	/*
	 * Android API BufferedReader.readline() hangs until
	 * end-of-line is reached. So we put '\n' at the end
	 * of events_in_json string.
	 */
	sprintf(event_msg, "%s\n", events_in_json);

	r_size = send(fd, event_msg,
			strlen(event_msg), MSG_NOSIGNAL);
	if (r_size <= 0)
		return -errno;

	r_size = recv(fd, &ret_val, sizeof(int32_t), MSG_NOSIGNAL);
	UNUSED(r_size);
	if (ret_val == SERVERREPLYOK)
		return 0;
	else
		return -EINVAL;
}

/************************************************************************
 *
 * Function name: get_backoff_exponent_second
 *        Inputs: int32_t backoff_exponent
 *        Output: Integer
 *       Summary: return a integer in seconds generated by
 *       	  exponential backoff algorithm.
 *  Return value: seconds
 *
 ***********************************************************************/
static int32_t get_backoff_exponent_second(int32_t backoff_exponent)
{
	int32_t min, max;
	int32_t wait_sec = 0;
	int32_t max_backoff_exponent = 9;
	int32_t backoff_slot  = 1;

	if (backoff_exponent > max_backoff_exponent)
		backoff_exponent = max_backoff_exponent;
	max = (int32_t)pow(2, backoff_exponent);
	min = (max >= 8) ? max / 8 : 1;
	wait_sec = backoff_slot;
	wait_sec *= (min + (rand() % (max - min + 1)));
	return wait_sec;
}

/************************************************************************
 *
 * Function name: event_worker_loop
 *        Inputs: NONE
 *        Output: NONE
 *       Summary: Loop to process events in queue. Worker will collect
 *                events and send to notify server. The active/inactive
 *                of this worker will controlled by semaphore.
 *  Return value: NONE
 *
 ***********************************************************************/
void *event_worker_loop(void *ptr)
{
	int32_t ret_code, count, server_fd;
	int32_t need_retry, retry_times, wait_time;
	int32_t queue_head, num_events_to_send, num_events_dequeue;
	char *msg_str_to_send;
	json_t *events_to_send;
	struct timespec timeout;

	UNUSED(ptr);

	/* Loop for sending event notify */
	while (hcfs_system->system_going_down == FALSE)
	{
		/* Wait for active */
		pthread_mutex_lock(&(event_queue->worker_active_lock));
		pthread_cond_wait(&(event_queue->worker_active_cond),
				    &(event_queue->worker_active_lock));
		pthread_mutex_unlock(&(event_queue->worker_active_lock));

		retry_times = 0;
		while (event_queue->num_events > 0 &&
				hcfs_system->system_going_down == FALSE) {
			/* Init vars for this round */
			server_fd = -1;
			need_retry = FALSE;
			events_to_send = NULL;
			msg_str_to_send = NULL;

			if (event_queue->num_events < MAX_NUM_EVENT_SEND)
				num_events_to_send = event_queue->num_events;
			else
				num_events_to_send = MAX_NUM_EVENT_SEND;

			events_to_send = json_array();
			if (events_to_send == NULL) {
				write_log(4, "Failed to init array for events, errno - %d",
						errno);
				goto error_handler;
			}

			queue_head = event_queue->head;
			for (count = 0; count < num_events_to_send; count++) {
				/* Collect events */
				ret_code = json_array_append(events_to_send,
						event_queue->events[queue_head + count]);
				if (ret_code < 0) {
					write_log(4, "Failed to append event, errno - %d",
							errno);
					goto error_handler;
				}
			}

			msg_str_to_send = json_dumps(events_to_send, JSON_COMPACT);
			if (msg_str_to_send == NULL) {
				write_log(4, "Failed to construct events, errno - %d",
						errno);
				goto error_handler;
			}

			server_fd = _get_server_conn(notify_server_path);
			if (server_fd < 0) {
				write_log(4, "Failed to connect notify server, errno - %d",
						-server_fd);
				goto error_handler;
			}

			ret_code = send_event_to_server(server_fd, msg_str_to_send);
			if (ret_code < 0) {
				write_log(4, "Failed to send event msg to server, errno - %d",
						-ret_code);
				goto error_handler;
			}

			/* These events are send, remove from queue */
			num_events_dequeue = event_dequeue(num_events_to_send);
			if (num_events_dequeue < 0)
				write_log(4, "Failed to remove events from queue");
			goto cleanup;

error_handler:
			need_retry = TRUE;
			retry_times += 1;
cleanup:
			if (events_to_send != NULL)
				json_decref(events_to_send);
			if (msg_str_to_send != NULL)
				free(msg_str_to_send);
			if (server_fd >= 0)
				close(server_fd);
			if (need_retry) {
				wait_time = get_backoff_exponent_second(retry_times);
				write_log(8, "Will retry to send notify events after %d sec later",
						wait_time);
				clock_gettime(CLOCK_REALTIME, &timeout);
		                timeout.tv_sec += wait_time;
				pthread_mutex_lock(&(event_queue->worker_active_lock));
				pthread_cond_timedwait(&(event_queue->worker_active_cond),
					    &(event_queue->worker_active_lock),
					    &timeout);
				pthread_mutex_unlock(&(event_queue->worker_active_lock));
			} else {
				retry_times = 0;
			}
		}
		/* Go to sleep */
		write_log(8, "Empty event queue, worker will go to sleep");
	}
	return NULL;
}


/************************************************************************
 *
 * Function name: destroy_event_worker_loop_thread
 *        Inputs: NONE
 *        Output: Integer
 *       Summary: Destroy event_worker_loop thread.
 *  Return value: NONE
 *
 ***********************************************************************/
void destroy_event_worker_loop_thread()
{
	int32_t queue_full_sem_val;

	/* Wake up event worker */
	pthread_mutex_lock(&(event_queue->worker_active_lock));
	pthread_cond_signal(&(event_queue->worker_active_cond));
	pthread_mutex_unlock(&(event_queue->worker_active_lock));

	/* Avoid some thread blocked in enqueue function */
	do {
		sem_getvalue(&(event_queue->queue_full_sem),
				&queue_full_sem_val);
		sem_post(&(event_queue->queue_full_sem));
	} while (queue_full_sem_val == 0);
}

/************************************************************************
 *
 * Function name: add_notify_event
 *        Inputs: int32_t event_id, char *event_info_json_str
 *        	  char blocking
 *        Output: Integer
 *       Summary: Add a notify event. If there is only event_id required
 *                to passed to notify server, then set (event_info_json_str)
 *                to NULL. If there are addtional key/value pairs should
 *                be passed with this event, the arg (event_info_json_str)
 *                should contain these information. The format must be a
 *                string followed the json specification.
 *                E.g. "{\"key1\":\"val1\", \"key2\":\"val2\"}"
 *                Passing TRUE|FALSE to arg (blocking) to control whether
 *                this function should be blocked when queue is full or not.
 *  Return value: 0 - Operation was successful.
 *                1 - Event is dropped because notify server is not set.
 *                2 - Event is dropped due to queue full error.
 *                3 - Event is dropped by event filter.
 *        Otherwise - The negation of the appropriate error code.
 *
 ***********************************************************************/
int32_t add_notify_event(int32_t event_id, char *event_info_json_str,
			 char blocking)
{
	int32_t ret_code;

	/* Server not set? */
	if (notify_server_path == NULL) {
		write_log(4, "Event is dropped because notify server not set.");
		return 1;
	}

	/* Event ID validator */
	if (!IS_EVENT_VALID(event_id))
		return -EINVAL;

	/* Event filter */
	ret_code = check_event_filter(event_id);
	if (ret_code < 0) {
		write_log(8, "Event is dropped by event filter.");
		return 3;
	}

	ret_code = event_enqueue(event_id, event_info_json_str, blocking);
	if (ret_code == -ENOSPC) {
		/* Event queue is full */
		write_log(4, "Event is dropped due to queue full error.");
		ret_code = 2;
		goto done;
	} else if (ret_code < 0) {
		return ret_code;
	}

	ret_code = 0;
done:
	/* Wake up queue worker */
	pthread_mutex_lock(&(event_queue->worker_active_lock));
	pthread_cond_signal(&(event_queue->worker_active_cond));
	pthread_mutex_unlock(&(event_queue->worker_active_lock));

	return ret_code;
}

