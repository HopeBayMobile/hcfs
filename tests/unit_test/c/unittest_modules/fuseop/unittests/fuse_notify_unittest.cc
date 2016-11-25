/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuse_notify_unittest.cc
* Abstract: The unittest code for fuse notification related FUSE
*           operation.
*
* Revision History
* 2016/9/1 Jethro Add unittest for delete notification.
*
**************************************************************************/
#include <gtest/gtest.h>

extern "C" {
#include <dlfcn.h>
#include <errno.h>
#include <fuse/fuse_lowlevel.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fuse_notify.h"
#include "hfuse_system.h"
#include "mount_manager.h"
#include "ut_helper.h"
}

MOUNT_T_GLOBAL mount_global;
extern FUES_NOTIFY_SHARED_DATA notify;
extern fuse_notify_fn *notify_fn[];

/*
 * Helper functions
 */

FUSE_NOTIFY_PROTO saved_notify[20];
int32_t ut_enqueue_call;
int32_t ut_enqueue(size_t n)
{
	size_t i, in = 0;
	uint8_t fake_data = 0;
	int32_t ret, write_log_hide_orgin = write_log_hide;
	FUSE_NOTIFY_PROTO *data;

	if (n > 50)
		write_log_hide = 10;
	for (i = 0; i < n; i++) {
		write_log(11, "notify.len %lu\n", notify.len);
		data = &saved_notify[ut_enqueue_call % 20];
		memset(data, fake_data, FUSE_NOTIFY_ENTRY_SIZE);
		data->func = NOOP;
		ret = notify_buf_enqueue(data);
		in = notify.in + 1;
		if (in == FUSE_NOTIFY_RINGBUF_MAXLEN)
			in = 0;
		ut_enqueue_call++;
		fake_data++;
	}

	write_log_hide = write_log_hide_orgin;
	return ret;
}
void ut_dequeue(int32_t n)
{
	FUSE_NOTIFY_PROTO *d = NULL;
	int32_t i;
	for (i = 0; i < n; i++) {
		d = notify_buf_dequeue();
		FREE(d);
	}
}

void reset_fake_functions(void)
{
	reset_ut_helper();
	ut_enqueue_call = 0;
}

void reset_unittest_env(void)
{
	if (fuse_nofify_thread != 0 &&
	    pthread_kill(fuse_nofify_thread, 0) == 0) {
		hcfs_system->system_going_down = TRUE;
		destory_hfuse_ll_notify_loop();
		fuse_nofify_thread = 0;
	}
	hcfs_system->system_going_down = FALSE;
	memset(&notify, 0, sizeof(FUES_NOTIFY_SHARED_DATA));
	memset(&saved_notify, 0, sizeof(saved_notify));
}

/*
 * Google Tests
 */

class fuseNotifyEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp()
	{
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)calloc(1, sizeof(SYSTEM_DATA_HEAD));
		reset_fake_functions();
	}

	virtual void TearDown() {}
};

::testing::Environment *const fuseop_env =
    ::testing::AddGlobalTestEnvironment(new fuseNotifyEnvironment);

class NotifyBufferSetUpAndTearDown : public ::testing::Test
{
	protected:
	virtual void SetUp() {
	}

	virtual void TearDown()
	{
		reset_unittest_env();
		reset_fake_functions();
	}
};

TEST_F(NotifyBufferSetUpAndTearDown, InitSuccess)
{
	ASSERT_EQ(init_notify_buf(), 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, InitFail_InitSemFail_1)
{

	sem_init_error_on = 1;
	EXPECT_LT(init_notify_buf(), 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, InitFail_InitSemFail_2)
{

	sem_init_error_on = 2;
	EXPECT_LT(init_notify_buf(), 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, DestroyBufSuccess)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	char *name2 = strdup("B");
	char *name3;
	int32_t i;

	ASSERT_EQ(init_notify_buf(), 0);

	mount_global.ch[1] = ch;
	for (i = 2; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name2);

	destory_notify_buf();

	for (i = 0; i < MP_TYPE_NUM - 1; i++) {
		name3 = ((FUSE_NOTIFY_DELETE_DATA *)&notify.ring_buf[i])->name;
		EXPECT_NE(0, (name3 == NULL));
	}
	FREE(ch);
	FREE(name);
	FREE(name2);
	FREE(fake_mp);
}

class NotifyBuffer_Initialized : public ::testing::Test
{
	protected:
	virtual void SetUp()
	{
		init_notify_buf();
		reset_fake_functions(); // clean up SetUp
	}

	virtual void TearDown()
	{
		reset_unittest_env();
		reset_fake_functions(); // clean up Tests
	}
};

TEST_F(NotifyBuffer_Initialized, Enqueue)
{
	ut_enqueue(1);
	EXPECT_EQ(0,
		  memcmp(&saved_notify[0], &notify.ring_buf[0], sizeof(FUSE_NOTIFY_PROTO)));
}

TEST_F(NotifyBuffer_Initialized, EnqueueToLinkedListFailToAllocateNode)
{
	int32_t i;
	uint8_t fake_data = 0;
	uint8_t *d = NULL;

	malloc_error_on = 1;
	EXPECT_EQ(ut_enqueue(FUSE_NOTIFY_RINGBUF_MAXLEN+1), -1);
	write_log_hide = 2;
	for (i = 0; i < FUSE_NOTIFY_RINGBUF_MAXLEN; i++) {
		d = (uint8_t*)notify_buf_dequeue();
		ASSERT_EQ(1, (d != NULL));
		/* use d[offset] to skip func field */
		EXPECT_EQ(0, memcmp(&fake_data, &(d[5]), sizeof(fake_data)));
		FREE(d);
		fake_data++;
	}
}
TEST_F(NotifyBuffer_Initialized, EnqueueToLinkedListFailToAllocateData)
{
	int32_t i;
	uint8_t fake_data = 0;
	uint8_t *d = NULL;

	malloc_error_on = 2;
	EXPECT_EQ(ut_enqueue(FUSE_NOTIFY_RINGBUF_MAXLEN+1), -1);
	write_log_hide = 2;
	for (i = 0; i < FUSE_NOTIFY_RINGBUF_MAXLEN; i++) {
		d = (uint8_t*)notify_buf_dequeue();
		ASSERT_EQ(1, (d != NULL));
		/* use d[offset] to skip func field */
		EXPECT_EQ(0, memcmp(&fake_data, &(d[5]), sizeof(fake_data)));
		FREE(d);
		fake_data++;
	}
}

TEST_F(NotifyBuffer_Initialized, Dequeue)
{
	FUSE_NOTIFY_PROTO *d = NULL;

	ut_enqueue(1);
	d = notify_buf_dequeue();
	ASSERT_NE(0, (d != NULL));
	EXPECT_EQ(0, memcmp(d, notify.ring_buf, FUSE_NOTIFY_ENTRY_SIZE));
	FREE(d);
}

TEST_F(NotifyBuffer_Initialized, DequeueLinkedList)
{
	int32_t i;
	uint8_t fake_data = 0;
	uint8_t *d = NULL;

	EXPECT_EQ(ut_enqueue(FUSE_NOTIFY_RINGBUF_MAXLEN * 2), 0);
	write_log_hide = 2;
	for (i = 0; i < FUSE_NOTIFY_RINGBUF_MAXLEN * 2; i++) {
		d = (uint8_t*)notify_buf_dequeue();
		ASSERT_EQ(1, (d != NULL));
		/* use d[offset] to skip func field */
		EXPECT_EQ(0, memcmp(&fake_data, &(d[5]), sizeof(fake_data)));
		FREE(d);
		fake_data++;
	}
}

TEST_F(NotifyBuffer_Initialized, DequeueMallocFail) {
	ut_enqueue(1);
	malloc_error_on = 1;
	EXPECT_NE(0, (notify_buf_dequeue() == NULL));
	EXPECT_EQ(ENOMEM, errno);
}

TEST_F(NotifyBuffer_Initialized, initLoop)
{
	init_hfuse_ll_notify_loop();
	/* Test thread is running */
	ASSERT_EQ(pthread_kill(fuse_nofify_thread, 0), 0);
}

TEST_F(NotifyBuffer_Initialized, initLoopWithInitBuffer)
{
	reset_unittest_env();
	ASSERT_EQ(init_hfuse_ll_notify_loop(), 0);
}

TEST_F(NotifyBuffer_Initialized, initLoopFail_pthread_create)
{
	reset_unittest_env();
	pthread_create_error_on = 1;
	ASSERT_LT(init_hfuse_ll_notify_loop(), 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, initLoopFail_init_notify_buf)
{
	reset_unittest_env();
	sem_init_error_on = 1;
	ASSERT_LT(init_hfuse_ll_notify_loop(), 0);
}

TEST_F(NotifyBuffer_Initialized, destoryLoop)
{
	init_hfuse_ll_notify_loop();
	hcfs_system->system_going_down = TRUE;
	destory_hfuse_ll_notify_loop();
	/* Test thread is running */
	ASSERT_EQ(pthread_kill(fuse_nofify_thread, 0), 3);
}

TEST_F(NotifyBuffer_Initialized, destoryLoop_SemPostFail)
{
	struct timespec wait = {0, 1000000};
	init_hfuse_ll_notify_loop();
	hcfs_system->system_going_down = TRUE;
	nanosleep(&wait, NULL);

	sem_post_call_count = 0;
	sem_post_errno = 123;
	sem_post_error_on = 1;
	ASSERT_EQ(destory_hfuse_ll_notify_loop(), -123);
	sem_post_error_on = -1;
	destory_hfuse_ll_notify_loop();
}

TEST_F(NotifyBuffer_Initialized, destoryLoop_PthreadJoinFail)
{
	init_hfuse_ll_notify_loop();
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify.not_empty);
	pthread_join(fuse_nofify_thread, NULL);
	destory_hfuse_ll_notify_loop();
	EXPECT_STREQ(log_data[write_log_call_count % LOG_RECORD_SIZE],
		     "Error destory_hfuse_ll_notify_loop: Failed to join "
		     "nofify_thread. No such process\n");
}

TEST_F(NotifyBuffer_Initialized, loopCallNotify)
{
	int32_t i;
	struct timespec wait = {0, 100000};
	init_hfuse_ll_notify_loop();
	nanosleep(&wait, NULL);
	ut_enqueue(5);
	for (i = 0; i < 100; i++) {
		if (notify.len != 0)
			nanosleep(&wait, NULL);
	}
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify.not_empty);
	pthread_join(fuse_nofify_thread, NULL);
	EXPECT_EQ(notify.len, 0);
}

TEST_F(NotifyBuffer_Initialized, loopCallNotifyFailed)
{
	int32_t task;
	struct timespec wait = {0, 100000};
	init_hfuse_ll_notify_loop();
	write_log_hide = 2;
	malloc_error_on = 1;
	ut_enqueue(1);
	while (1) {
		sem_getvalue(&notify.not_empty, &task);
		if(task == 0)
			break;
		/* waiting loop to finish */
		nanosleep(&wait, NULL);
	}
	puts("UT: system_going_down");
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify.not_empty);
	pthread_join(fuse_nofify_thread, NULL);
#define msg \
	"Error hfuse_ll_notify_loop: Dequeue failed. Cannot allocate memory\n"
	EXPECT_STREQ(log_data[write_log_call_count % LOG_RECORD_SIZE], msg);
#undef msg
}

TEST_F(NotifyBuffer_Initialized, _do_hfuse_ll_notify_delete_RUN)
{
	FUSE_NOTIFY_PROTO *this_data =
	    (FUSE_NOTIFY_PROTO *)calloc(1, FUSE_NOTIFY_ENTRY_SIZE);
	((FUSE_NOTIFY_DELETE_DATA *)this_data)->func = DELETE;
	((FUSE_NOTIFY_DELETE_DATA *)this_data)->name = (char *)malloc(1);
	_do_hfuse_ll_notify_delete(this_data, RUN);
	FREE(this_data);
	EXPECT_EQ(fuse_lowlevel_notify_delete_call_count, 1);
}

TEST_F(NotifyBuffer_Initialized, _do_hfuse_ll_notify_delete_RUN_Fail)
{
	FUSE_NOTIFY_PROTO *this_data =
	    (FUSE_NOTIFY_PROTO *)calloc(1, FUSE_NOTIFY_ENTRY_SIZE);
	((FUSE_NOTIFY_DELETE_DATA *)this_data)->func = DELETE;
	((FUSE_NOTIFY_DELETE_DATA *)this_data)->name = (char *)malloc(1);
	fuse_lowlevel_notify_delete_error_on = 1;
	EXPECT_EQ(_do_hfuse_ll_notify_delete(this_data, RUN), 0);
	FREE(this_data);
}

TEST_F(NotifyBuffer_Initialized, notify_delete)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	hfuse_ll_notify_delete(ch, 0, 0, name, 1);
	FREE(ch);
	FREE(name);
	FREE(((FUSE_NOTIFY_DELETE_DATA*)&notify.ring_buf[0])->name);
	EXPECT_EQ(notify.len, 1);
}

TEST_F(NotifyBuffer_Initialized, notify_deleteFail_SystemShutdownFail)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t ret;
	hcfs_system->system_going_down = TRUE;
	ret = hfuse_ll_notify_delete(ch, 0, 0, name, 1);
	FREE(ch);
	FREE(name);
	EXPECT_EQ(notify.len, 0);
	EXPECT_EQ(ret, -1);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_StrndupFail)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	strndup_error_on = 1;
	EXPECT_LT(hfuse_ll_notify_delete(ch, 0, 0, name, 1), 0);
	FREE(ch);
	FREE(name);
	EXPECT_EQ(notify.len, 0);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_Normal)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;

	mount_global.ch[1] = ch;
	for (i = 2; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name);
	FREE(ch);
	FREE(name);
	FREE(fake_mp);
	for (i = 0; i < MP_TYPE_NUM - 1; i++)
		FREE(((FUSE_NOTIFY_DELETE_DATA *)&notify.ring_buf[i])->name);
	EXPECT_EQ(notify.len, 2);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_FakeAll)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;

	for (i = 1; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name);
	FREE(ch);
	FREE(name);
	FREE(fake_mp);
	for (i = 0; i < MP_TYPE_NUM; i++)
		FREE(((FUSE_NOTIFY_DELETE_DATA *)&notify.ring_buf[i])->name);
	EXPECT_EQ(notify.len, 3);
}
TEST_F(NotifyBuffer_Initialized, notify_delete_mp_SkipAll)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;

	for (i = 1; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = NULL;
	hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name);
	FREE(ch);
	FREE(name);
	EXPECT_EQ(notify.len, 0);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_OverflowRingBuffer)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;
	int32_t ret;
	FUSE_NOTIFY_LINKED_NODE *node, *next_node;

	for (i = 1; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	write_log_hide = 10;
	for (i = 0; i < 10000; i++) {
		ret = hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name);
		EXPECT_EQ(ret, 0);
	}
	write_log(5, "notify.len %lu\n", notify.len);
	EXPECT_EQ(notify.len, 10000 * MP_TYPE_NUM);
	FREE(ch);
	FREE(name);
	FREE(fake_mp);
	for (i = 0; i < FUSE_NOTIFY_RINGBUF_MAXLEN; i++)
		FREE(((FUSE_NOTIFY_DELETE_DATA *)&notify.ring_buf[i])->name);
	node = notify.linked_list_head;
	while (node != NULL) {
		next_node = node->next;
		FREE(((FUSE_NOTIFY_DELETE_DATA *)node->data)->name);
		FREE(node->data);
		FREE(node);
		node = next_node;
	}
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_DiffCaseNameTriggerAllNotify)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	char *name2 = strdup("B");
	int32_t i;

	mount_global.ch[1] = ch;
	for (i = 2; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name2);
	FREE(ch);
	FREE(name);
	FREE(name2);
	FREE(fake_mp);
	for (i = 0; i < MP_TYPE_NUM; i++)
		FREE(((FUSE_NOTIFY_DELETE_DATA *)&notify.ring_buf[i])->name);
	EXPECT_EQ(notify.len, 3);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_Fail)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;

	mount_global.ch[1] = ch;
	for (i = 2; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	strndup_error_on = 1;
	EXPECT_EQ(-ENOMEM, hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name));
	FREE(ch);
	FREE(name);
	FREE(fake_mp);
}
