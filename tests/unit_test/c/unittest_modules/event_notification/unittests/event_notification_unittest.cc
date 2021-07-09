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

/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
extern "C" {
#include "global.h"
#include "fuseop.h"
#include "time.h"
#include "logger.h"
#include "event_notification.h"
#include "event_filter.h"
}

extern "C" {
void* init_mock_server(void *);
}

extern char *notify_server_path;

#include "../../fff.h"
DEFINE_FFF_GLOBALS;

/* Fake functions of system call*/
FAKE_VALUE_FUNC(time_t, time, time_t *);

/* Fake functions of hcfs */
FAKE_VALUE_FUNC(int32_t, check_event_filter, int32_t);
FAKE_VALUE_FUNC_VARARG(int32_t, write_log, int32_t, const char *, ...);

/* Register event filter here */
REGISTER_EVENTS;

TEST(init_event_queueTEST, InitializedOK)
{
	int32_t ret_code;
	int32_t sem_val;

	ret_code = init_event_queue();
	EXPECT_EQ(ret_code, 0);
	EXPECT_EQ(event_queue->num_events, 0);
	EXPECT_EQ(event_queue->head, -1);
	EXPECT_EQ(event_queue->rear, -1);

	sem_getvalue(&(event_queue->queue_access_sem), &sem_val);
	EXPECT_EQ(sem_val, 1);

	sem_destroy(&(event_queue->queue_access_sem));
	pthread_mutex_destroy(&(event_queue->worker_active_lock));
	pthread_cond_destroy(&(event_queue->worker_active_cond));
	free(event_queue);
}

/* Unittest for set_event_notify_server */
class set_event_notify_serverTest : public ::testing::Test
{
	protected:
	pthread_t server_thread;

	void SetUp()
	{
		init_event_queue();
	}

	void TearDown()
	{
		sem_destroy(&(event_queue->queue_access_sem));
		pthread_mutex_destroy(&(event_queue->worker_active_lock));
		pthread_cond_destroy(&(event_queue->worker_active_cond));
		free(event_queue);
	}
};

TEST_F(set_event_notify_serverTest, SetServerOK)
{
	int32_t ret_code;
	const char *server_path = "event.notify.mock.server";

	ret_code = pthread_create(&server_thread, NULL, &init_mock_server, NULL);
	EXPECT_EQ(ret_code, 0);

	puts("Waiting 1 sec for server up");
	sleep(1);

	ret_code = set_event_notify_server(server_path);
	EXPECT_EQ(ret_code, 0);
}

TEST_F(set_event_notify_serverTest, ServerNotReady)
{
	int32_t ret_code;
	const char *server_path = "test.server.not.ready";

	ret_code = set_event_notify_server(server_path);
	EXPECT_EQ(ret_code, -111);
}
/* End unittest for set_event_notify_serverTest */

/* Unittest for event_enqueueTest */
class event_enqueueTest : public ::testing::Test
{
	protected:

	void SetUp()
	{
		init_event_queue();

		ASSERT_EQ(event_queue->num_events, 0);
		ASSERT_EQ(event_queue->head, -1);
		ASSERT_EQ(event_queue->rear, -1);

	}

	void TearDown()
	{
		sem_destroy(&(event_queue->queue_access_sem));
		pthread_mutex_destroy(&(event_queue->worker_active_lock));
		pthread_cond_destroy(&(event_queue->worker_active_cond));
		free(event_queue);
	}
};

TEST_F(event_enqueueTest, EnqueueOneEl)
{
	int32_t ret_code, event_id = 1;
	json_t *tmp_obj;

	time_fake.return_val = 999;

	ret_code = event_enqueue(event_id, NULL, FALSE);
	EXPECT_EQ(ret_code, 0);
	EXPECT_EQ(event_queue->num_events, 1);
	EXPECT_EQ(event_queue->head, 0);
	EXPECT_EQ(event_queue->rear, 0);

	tmp_obj = json_object_get(event_queue->events[0], "event_id");
	EXPECT_EQ(json_integer_value(tmp_obj), 1);

	EXPECT_EQ(event_filters[event_id].last_send_timestamp, 999);
}

TEST_F(event_enqueueTest, EnqueueWithEventInfo)
{
	int32_t ret_code, event_info;
	json_t *tmp_obj;

	tmp_obj = json_pack("{si}","info", 999);
	ASSERT_NE(tmp_obj, NULL);
	ret_code = event_enqueue(1, tmp_obj, FALSE);
	EXPECT_EQ(ret_code, 0);
	EXPECT_EQ(event_queue->num_events, 1);
	EXPECT_EQ(event_queue->head, 0);
	EXPECT_EQ(event_queue->rear, 0);

	tmp_obj = json_object_get(event_queue->events[0], "info");
	event_info = json_integer_value(tmp_obj);
	EXPECT_EQ(event_info, 999);

	json_decref(tmp_obj);
	json_decref(event_queue->events[0]);
}

TEST_F(event_enqueueTest, EnqueueWithErrorEventInfo)
{
	int32_t ret_code, event_info;
	json_t *tmp_obj;

	tmp_obj = json_pack("s","xxxxxxxxxxxx");
	ret_code = event_enqueue(1, tmp_obj, FALSE);
	EXPECT_EQ(ret_code, -EINVAL);
}

TEST_F(event_enqueueTest, EnqueueWithErrorEventInfo2)
{
	int32_t ret_code, event_info;
	const char *event_info_json_str = "[{}]";
	json_t *tmp_obj;
	tmp_obj = json_pack("[{}]");

	ret_code = event_enqueue(1, json_pack(event_info_json_str), FALSE);
	EXPECT_EQ(ret_code, -EINVAL);
}

TEST_F(event_enqueueTest, EnqueueUtilFull)
{
	int32_t ret_code;
	int32_t head, rear, rear2;
	int32_t count;

	for (count = 0; count < EVENT_QUEUE_SIZE; count++) {
		rear = event_queue->rear;
		rear2 = (rear + 1) % EVENT_QUEUE_SIZE;
		ret_code = event_enqueue(1, NULL, FALSE);
		EXPECT_EQ(ret_code, 0);
		EXPECT_EQ(event_queue->rear, rear2);
		EXPECT_EQ(event_queue->num_events, count + 1);
	}

	ret_code = event_enqueue(1, NULL, FALSE);
	EXPECT_EQ(ret_code, -ENOSPC);
	EXPECT_EQ(event_queue->num_events, EVENT_QUEUE_SIZE);

	ret_code = event_enqueue(1, NULL, FALSE);
	EXPECT_EQ(ret_code, -ENOSPC);
	EXPECT_EQ(event_queue->num_events, EVENT_QUEUE_SIZE);
}
/* End unittest for event_enqueueTest */

/* Unittest for event_dequeueTest */
class event_dequeueTest : public ::testing::Test
{
	protected:

	void SetUp()
	{
		init_event_queue();

		ASSERT_EQ(event_queue->num_events, 0);
		ASSERT_EQ(event_queue->head, -1);
		ASSERT_EQ(event_queue->rear, -1);

	}

	void TearDown()
	{
		sem_destroy(&(event_queue->queue_access_sem));
		pthread_mutex_destroy(&(event_queue->worker_active_lock));
		pthread_cond_destroy(&(event_queue->worker_active_cond));
		free(event_queue);
	}
};

TEST_F(event_dequeueTest, InvalidNumEvents)
{
	int32_t num_events;

	num_events = 0;
	EXPECT_EQ(event_dequeue(num_events), -EINVAL);

	num_events = EVENT_QUEUE_SIZE + 1;
	EXPECT_EQ(event_dequeue(num_events), -EINVAL);
}

TEST_F(event_dequeueTest, QueueEmpty)
{
	int32_t num_events;

	num_events = 1;
	EXPECT_EQ(event_dequeue(num_events), -ENOENT);
}

TEST_F(event_dequeueTest, DequeueOneEl)
{
	/* enqueue first */
	ASSERT_EQ(event_enqueue(1, NULL, FALSE), 0);

	EXPECT_EQ(event_dequeue(1), 1);
	EXPECT_EQ(event_queue->head, -1);
	EXPECT_EQ(event_queue->rear, -1);
	EXPECT_EQ(event_queue->num_events, 0);
}

TEST_F(event_dequeueTest, DequeueUntilEmpty)
{
	int32_t idx, head, head2;
	int32_t total_events = 55;
	int32_t dequeue_size = 2;
	int32_t num_dequeue;

	/* enqueue first */
	for (idx = 0; idx < total_events; idx++) {
		ASSERT_EQ(event_enqueue(1, NULL, FALSE), 0);
	}

	for (;;) {
		head = event_queue->head;
		num_dequeue = event_dequeue(dequeue_size);
		if (num_dequeue != dequeue_size) {
			EXPECT_EQ(total_events % dequeue_size, num_dequeue);
			break;
		} else {
			head2 = (head + dequeue_size) % EVENT_QUEUE_SIZE;
			EXPECT_EQ(event_queue->head, head2);
			EXPECT_EQ(dequeue_size, num_dequeue);
		}
	}

	EXPECT_EQ(event_queue->head, -1);
	EXPECT_EQ(event_queue->rear, -1);
	EXPECT_EQ(event_queue->num_events, 0);
}
/* End unittest for event_dequeueTest */

/* Unittest for send_event_to_serverTest */
class send_event_to_serverTest : public ::testing::Test
{
	protected:
        int32_t ret_code, fd, status, sock_len;
        struct sockaddr_un addr;
	pthread_t server_thread;
	const char *path = "event.notify.mock.server";

	void SetUp()
	{
		ret_code = pthread_create(&server_thread, NULL,
					&init_mock_server, NULL);
		ASSERT_EQ(ret_code, 0);

		puts("Waiting 1 sec for server up");
		sleep(1);

		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		addr.sun_path[0] = 0;
		strcpy(&(addr.sun_path[1]), path);
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		sock_len = 1 + strlen(path) +
			offsetof(struct sockaddr_un, sun_path);
		status = connect(fd, (const struct sockaddr *) &addr, sock_len);

		if (fd < 0 || status < 0)
			ASSERT_EQ(0, 1);
	}

	void TearDown()
	{
		close(fd);
	}
};

TEST_F(send_event_to_serverTest, SendOK)
{
	int32_t ret_code;

	ret_code = send_event_to_server(fd, "goodmsg");
	EXPECT_EQ(ret_code, 0);
}

TEST_F(send_event_to_serverTest, BadEventMsg)
{
	int32_t ret_code;

	ret_code = send_event_to_server(fd, "badmsg");
	EXPECT_EQ(ret_code, -EINVAL);
}

TEST_F(send_event_to_serverTest, BadFileDescriptor)
{
	int32_t ret_code;

	ret_code = send_event_to_server(-1, "goodmsg");
	EXPECT_EQ(ret_code, -EBADF);
}
/* End unittest for send_event_to_serverTest */

/* Unittest for event_worker_loopTest */
class event_worker_loopTest : public ::testing::Test
{
	protected:

	void SetUp()
	{
		init_event_queue();

		ASSERT_EQ(event_queue->num_events, 0);
		ASSERT_EQ(event_queue->head, -1);
		ASSERT_EQ(event_queue->rear, -1);

	}

	void TearDown()
	{
		sem_destroy(&(event_queue->queue_access_sem));
		pthread_mutex_destroy(&(event_queue->worker_active_lock));
		pthread_cond_destroy(&(event_queue->worker_active_cond));
		free(event_queue);
	}
};
/* End unittest for event_worker_loopTest */

/* Unittest for add_notify_eventTest */
class add_notify_eventTest : public ::testing::Test
{
	protected:

	void SetUp()
	{
		init_event_queue();

		ASSERT_EQ(event_queue->num_events, 0);
		ASSERT_EQ(event_queue->head, -1);
		ASSERT_EQ(event_queue->rear, -1);

	}

	void TearDown()
	{
		event_dequeue(EVENT_QUEUE_SIZE);
		sem_destroy(&(event_queue->queue_access_sem));
		pthread_mutex_destroy(&(event_queue->worker_active_lock));
		pthread_cond_destroy(&(event_queue->worker_active_cond));
		free(event_queue);
	}
};
TEST_F(add_notify_eventTest, AddOK)
{
	int32_t ret_code, idx;

	notify_server_path = strdup("fake.server");

	for (idx = 0; idx < NUM_EVENTS; idx++) {
		printf("%d\n", idx);
		ret_code = add_notify_event(idx, NULL, FALSE);
		EXPECT_EQ(ret_code, 0);
	}
	EXPECT_EQ(event_dequeue(NUM_EVENTS), NUM_EVENTS);
}

TEST_F(add_notify_eventTest, NotifyServerNotSet)
{
	int32_t ret_code;

	notify_server_path = NULL;

	ret_code = add_notify_event(1, NULL, FALSE);
	EXPECT_EQ(ret_code, 1);
}

TEST_F(add_notify_eventTest, InvalidEventID)
{
	int32_t ret_code;

	notify_server_path = strdup("fake.server");

	ret_code = add_notify_event(9999, NULL, FALSE);
	EXPECT_EQ(ret_code, -EINVAL);
}

TEST_F(add_notify_eventTest, EventQueueFull)
{
	int32_t ret_code;

	notify_server_path = strdup("fake.server");
	event_queue->num_events = EVENT_QUEUE_SIZE;

	ret_code = add_notify_event(1, NULL, FALSE);
	EXPECT_EQ(ret_code, 2);
	event_queue->num_events = 0;
}

TEST_F(add_notify_eventTest, BlockedByEventFilter)
{
	int32_t ret_code;

	notify_server_path = strdup("fake.server");
	check_event_filter_fake.return_val = -1;

	ret_code = add_notify_event(1, NULL, FALSE);
	EXPECT_EQ(ret_code, 3);
}
/* End unittest for add_notify_eventTest */
