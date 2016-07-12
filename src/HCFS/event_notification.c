/*************************************************************************
*
* Copyright Â2016 Hope Bay Technologies, Inc. All rights reserved.
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
#include <jansson.h>

#include "fuseop.h"
#include "event_filter.h"

char *notify_server_path = NULL;
EVENT_QUEUE *event_queue;

/* TEMP for test, will removed after integrated with hcfs code */
int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
        va_list alist;
        struct timeval tmptime;
        struct tm tmptm;
        char add_newline;
        char timestr[100];

        va_start(alist, format);
        if (format[strlen(format)-1] != '\n')
                add_newline = 1;
        else
                add_newline = 0;

        gettimeofday(&tmptime, NULL);
        localtime_r(&(tmptime.tv_sec), &tmptm);
        strftime(timestr, 90, "%F %T", &tmptm);

        printf("%s.%06d\t", timestr,
                (uint32_t)tmptime.tv_usec);

        vprintf(format, alist);
        if (add_newline == 1)
                printf("\n");

        va_end(alist);
        return 0;

}

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
static int32_t _get_server_conn()
{
	int32_t fd, status;
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 0;
	strcpy(&(addr.sun_path[1]), notify_server_path);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -errno;
	status = connect(fd, &addr, sizeof(struct sockaddr_un));
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
	int32_t ret_code, server_fd;
	char *old_server_path;
	char *msg_to_send = NULL;
	json_t *test_event = NULL;

	old_server_path = notify_server_path;

	notify_server_path = malloc(strlen(server_path) + 1);
	if (notify_server_path == NULL) {
		write_log(4, "Failed to set notify server, errno - %d\n", errno);
		ret_code = -errno;
		goto error_handle;
	}
	strcpy(notify_server_path, server_path);

	/* Construct test event */
	test_event = json_pack("[{s:i}]", "event_id", TESTSERVER);
	if (test_event == NULL) {
		write_log(4, "Failed to construct test event for server setup\n");
		ret_code = -errno;
		goto error_handle;
	}

	msg_to_send = json_dumps(test_event, JSON_COMPACT);
	if (msg_to_send == NULL) {
		write_log(4, "Failed to construct event msg for server setup\n");
		ret_code = -errno;
		goto error_handle;
	}

	/* Send test event */
	server_fd = _get_server_conn();
	if (server_fd < 0) {
		write_log(4,
			"Failed to connect notify server, errno - %d\n", -server_fd);
		ret_code = server_fd;
		goto error_handle;
	}

	printf("%s\n", msg_to_send);
	ret_code = send_event_to_server(server_fd, msg_to_send);
	if (ret_code < 0) {
		write_log(4,
			"Failed to send event msg to server, errno - %d\n", -ret_code);
		goto error_handle;
	}

	free(old_server_path);
	ret_code = 0;
	goto done;

error_handle:
	free(notify_server_path);
	notify_server_path = old_server_path;

done:
	if (test_event != NULL)
		json_decref(test_event);
	if (msg_to_send != NULL)
		free(msg_to_send);

	return ret_code;
}

/************************************************************************
 *
 * Function name: event_enqueue
 *        Inputs: int32_t event_id
 *        Output: Integer
 *       Summary: Add an event to event queue.
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
 ***********************************************************************/
int32_t event_enqueue(int32_t event_id)
{
	int32_t ret_code;
	json_t *event = NULL;

	if (event_queue->num_events >= EVENT_QUEUE_SIZE) {
		write_log(4, "Failed to add to event queue - Event queue full.");
		return -ENOSPC;
	}

	event = json_object();
	if (event == NULL) {
		write_log(4, "Failed to initialize event - Enqueue aborted.");
		return -errno;
	}

	ret_code =
		json_object_set_new(event, "event_id", json_integer(event_id));
	if (ret_code < 0) {
		write_log(4, "Failed to construct event - Enqueue aborted.");
		return -errno;
	}

	/* Wait lock */
	sem_wait(&(event_queue->queue_access_sem));

	if (event_queue->num_events <= 0) {
		event_queue->head = 0;
		event_queue->rear = 0;
	} else {
		event_queue->rear = (event_queue->rear + 1) % EVENT_QUEUE_SIZE;
	}

	event_queue->events[event_queue->rear] = event;
	event_queue->num_events += 1;

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
 *  Return value: 0 if successful. Otherwise returns the negation of the
 *                appropriate error code.
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
		event_queue->num_events -= 1;
		if (event_queue->num_events == 0) {
			event_queue->head = -1;
			event_queue->rear = -1;
		} else {
			event_queue->head =
				(event_queue->head + 1) % EVENT_QUEUE_SIZE;
		}
	}

	/* Unlock */
	sem_post(&(event_queue->queue_access_sem));

	write_log(8, "Event dequeue was successful. Total %d events removed.",
			num_events);

	return 0;
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
	int32_t ret_val;

	r_size = send(fd, events_in_json,
			strlen(events_in_json) + 1, MSG_NOSIGNAL);
	if (r_size < 0)
		return -errno;

	r_size = recv(fd, &ret_val, sizeof(int32_t), MSG_NOSIGNAL);
	if (ret_val == SERVERREPLYOK)
		return 0;
	else
		return -EINVAL;
}

/************************************************************************
 *
 * Function name: run_event_queue_worker
 *        Inputs: NONE
 *        Output: Integer
 *       Summary: Loop to process events in queue. Worker will collect
 *                events and send to notify server. The active/inactive
 *                of this worker will controlled by semaphore.
 *  Return value: NONE
 *
 ***********************************************************************/
void run_event_queue_worker()
{
	int32_t ret_code, count, server_fd;
	int32_t queue_head, num_events_to_send;
	char *msg_to_send;
	json_t *events_to_send;

	/* Loop for sending event notify */
	//while (!hcfs_system->system_going_down)
	while (1)
	{
		printf("Start to wait\n");
		/* Wait for active */
		pthread_mutex_lock(&(event_queue->worker_active_lock));
		pthread_cond_wait(&(event_queue->worker_active_cond),
				    &(event_queue->worker_active_lock));
		pthread_mutex_unlock(&(event_queue->worker_active_lock));
		printf("End wait\n");

		while (event_queue->num_events > 0) {
			server_fd = _get_server_conn();
			if (server_fd < 0) {
				/* TODO - go next round or errno handler? */
				write_log(4,
					"Failed to connect notify server, errno - %d\n", -server_fd);
				continue;
			}

			if (event_queue->num_events < MAX_NUM_EVENT_SEND)
				num_events_to_send = event_queue->num_events;
			else
				num_events_to_send = MAX_NUM_EVENT_SEND;

			events_to_send = json_array();
			if (events_to_send == NULL) {
				write_log(4,
					"Failed to initialize array for events, errno - %d\n", errno);
				close(server_fd);
				continue;
			}

			queue_head = event_queue->head;
			for (count = 0; count < num_events_to_send; count++) {
				/* Collect events */
				ret_code = json_array_append(events_to_send,
						event_queue->events[queue_head + count]);
				if (ret_code < 0) {
					write_log(4, "Failed to append event, errno - %d\n", errno);
					continue;
				}
			}

			msg_to_send = json_dumps(events_to_send, JSON_COMPACT);
			if (msg_to_send == NULL) {
				write_log(4, "Failed to construct events, errno - %d\n", errno);
				continue;
			}

			printf("%s\n", msg_to_send);
			ret_code = send_event_to_server(server_fd, msg_to_send);
			if (ret_code < 0) {
				write_log(4,
					"Failed to send event msg to server, errno - %d\n", -ret_code);
				continue;
			}

			free(msg_to_send);
			json_decref(events_to_send);

			/* These events are send, remove from queue */
			event_dequeue(num_events_to_send);

			close(server_fd);
		}

		/* Go to sleep */
		write_log(8,
			"Event queue is empty, worker will go to sleep.\n");
	}
}

/************************************************************************
 *
 * Function name: add_notify_event
 *        Inputs: int32_t event_id
 *        Output: Integer
 *       Summary: Add a notify event.
 *  Return value: 0 - Operation was successful.
 *                1 - Event is dropped because notify server not set.
 *                2 - Event is dropped due to queue full error.
 *                3 - Event is dropped by event filter.
 *        Otherwise - The negation of the appropriate error code.
 *
 ***********************************************************************/
int32_t add_notify_event(int32_t event_id)
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

	ret_code = event_enqueue(event_id);
	if (ret_code == -ENOSPC) {
		/* Queue is full */
		write_log(4, "Event is dropped due to queue full error.");
		return 2;
	} else if (ret_code < 0) {
		return ret_code;
	}

	/* Wake up queue worker */
	pthread_mutex_lock(&(event_queue->worker_active_lock));
	pthread_cond_signal(&(event_queue->worker_active_cond));
	pthread_mutex_unlock(&(event_queue->worker_active_lock));

	return 0;
}

void main()
{
	int32_t event_id;
	pthread_t t1;

	init_event_queue();
	printf("Set server result %d\n",
			set_event_notify_server("event.notify.mock.server"));

	pthread_create(&t1, NULL, &run_event_queue_worker, NULL);

	printf("Enter event ID\n");
	while (1) {
		scanf("%d", &event_id);
		printf("Add event - result %d\n", add_notify_event(event_id));
	}
}
