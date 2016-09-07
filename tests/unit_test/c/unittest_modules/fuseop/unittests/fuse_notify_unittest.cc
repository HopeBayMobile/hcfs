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
#ifdef PD
#undef PD
#define PD(x) write_log(10, #x " %d\n", ##x)
#endif

MOUNT_T_GLOBAL mount_global = {0};
extern FUSE_NOTIFY_RING_BUF notify_buf;
extern fuse_notify_fn *notify_fn[];

/* 
 * Definition for fake functions
 */

#define FFF_MASK_ON_FAKE
#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, sem_init, sem_t *, int, unsigned int);
typedef int(sem_init_f)(sem_t *, int, unsigned int);
sem_init_f *sem_init_real = (sem_init_f *)dlsym(RTLD_NEXT, "sem_init");
uint32_t sem_init_error_on = -1;
int32_t sem_init_cnt(sem_t *sem, int pshared, unsigned int value)
{
	//PD(sem_init_fake.call_count);
	write_log(10, "sem_init_fake.call_count %d\n", sem_init_fake.call_count);
	if (sem_init_fake.call_count != sem_init_error_on)
		return sem_init_real(sem, pshared, value);
	else
		return -1;
}

FAKE_VALUE_FUNC(void *, realloc, void *, size_t);
typedef void *(realloc_f)(void *, size_t);
realloc_f *realloc_real = (realloc_f *)dlsym(RTLD_NEXT, "realloc");
uint32_t realloc_error_on = -1;
void *realloc_cnt(void *ptr, size_t size)
{
	printf("realloc_fake.call_count %d\n", realloc_fake.call_count);
	if (realloc_fake.call_count != realloc_error_on)
		return realloc_real(ptr, size);
	else
		return NULL;
}

FAKE_VALUE_FUNC(void *, malloc, size_t);
typedef void *(malloc_f)(size_t);
malloc_f *malloc_real = (malloc_f *)dlsym(RTLD_NEXT, "malloc");
uint32_t malloc_error_on = -1;
void *malloc_cnt(size_t size)
{
	if (malloc_fake.call_count != malloc_error_on)
		return malloc_real(size);
	else
		return NULL;
}

FAKE_VALUE_FUNC(int,
		fuse_lowlevel_notify_delete,
		struct fuse_chan *,
		fuse_ino_t,
		fuse_ino_t,
		const char *,
		size_t);

/*
 * Helper functions
 */

FUSE_NOTIFY_DATA data[20];
int32_t ut_enqueue_call = 0;
void ut_enqueue(size_t n)
{
	size_t i, in = 0;
	static size_t seq = __LINE__;

	for (i = 0; i < n; i++) {
		if (in == notify_buf.max_len)
			in = 0;
		printf("Fill with %lu\n", seq);
		memset(&data[ut_enqueue_call % 20], seq,
		       sizeof(FUSE_NOTIFY_DATA));
		((FUSE_NOTIFY_PROTO *)&data[ut_enqueue_call % 20])->func = NOOP;
		notify_buf_enqueue((void *)&data[ut_enqueue_call % 20]);
		in = notify_buf.in + 1;
		ut_enqueue_call++;
		seq++;
	}
}
void ut_dequeue(int32_t n)
{
	FUSE_NOTIFY_DATA *d = NULL;
	int32_t i;
	for (i = 0; i < n; i++) {
		d = notify_buf_dequeue();
		free(d);
	}
}

void reset_fake_functions(void)
{
	reset_ut_helper();
	RESET_FAKE(sem_init);
	sem_init_error_on = -1;
	sem_init_fake.custom_fake = sem_init_cnt;

	RESET_FAKE(realloc);
	realloc_error_on = -1;
	realloc_fake.custom_fake = realloc_cnt;

	RESET_FAKE(malloc);
	malloc_error_on = -1;
	malloc_fake.custom_fake = malloc_cnt;

	RESET_FAKE(fuse_lowlevel_notify_delete);

	FFF_RESET_HISTORY();

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
	free(notify_buf.elems);
	memset(&notify_buf, 0, sizeof(FUSE_NOTIFY_RING_BUF));
	memset(&data, 0, sizeof(data));
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
	init_notify_buf();
	ASSERT_NE(notify_buf.elems, NULL);
	ASSERT_EQ(notify_buf.is_initialized, 1);
	free(notify_buf.elems);
	notify_buf.elems = NULL;
}

TEST_F(NotifyBufferSetUpAndTearDown, InitFail1)
{

	sem_init_fake.custom_fake = sem_init_cnt;
	sem_init_error_on = 1;
	init_notify_buf();
	EXPECT_EQ(notify_buf.elems, NULL);
	EXPECT_EQ(notify_buf.is_initialized, 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, InitFail2)
{

	sem_init_fake.custom_fake = sem_init_cnt;
	sem_init_error_on = 2;
	init_notify_buf();
	EXPECT_EQ(notify_buf.elems, NULL);
	EXPECT_EQ(notify_buf.is_initialized, 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, DestroySkip)
{
	memset(&notify_buf, 0, sizeof(FUSE_NOTIFY_RING_BUF));
	destory_notify_buf();
	EXPECT_EQ(write_log_call_count, 0);
}
TEST_F(NotifyBufferSetUpAndTearDown, DestroySuccess)
{
	FUSE_NOTIFY_DATA d;

	memset(&d, 0, sizeof(FUSE_NOTIFY_DATA));

	init_notify_buf();
	ASSERT_NE(notify_buf.elems, NULL);
	ASSERT_EQ(notify_buf.is_initialized, 1);

	notify_buf_enqueue((void *)&d);
	destory_notify_buf();
	ASSERT_EQ(notify_buf.elems, NULL);
	ASSERT_EQ(notify_buf.is_initialized, 0);
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
	EXPECT_EQ(0, memcmp(data, notify_buf.elems, sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, EnqueueSkiped)
{
	notify_buf.is_initialized = 0;
	ut_enqueue(1);
	EXPECT_EQ(notify_buf.len, 0);
}

TEST_F(NotifyBuffer_Initialized, Dequeue)
{
	FUSE_NOTIFY_DATA *d = NULL;
	ut_enqueue(1);
	d = notify_buf_dequeue();
	ASSERT_NE(d, NULL);
	EXPECT_EQ(0, memcmp(d, notify_buf.elems, sizeof(FUSE_NOTIFY_DATA)));
	free(d);
}

TEST_F(NotifyBuffer_Initialized, DequeueSkiped)
{
	ut_enqueue(1);
	notify_buf.is_initialized = 0;
	notify_buf_dequeue();
	EXPECT_EQ(notify_buf.len, 1);
}

TEST_F(NotifyBuffer_Initialized, DequeueAnEmptyQueue) {
	notify_buf_dequeue();
	EXPECT_STREQ(log_data[write_log_call_count % 5],
		     "Error notify_buf_dequeue: Queue is empty.\n");
	EXPECT_EQ(malloc_fake.call_count, 0);
}

TEST_F(NotifyBuffer_Initialized, DequeueMallocFail) {
	ut_enqueue(1);
	malloc_error_on = 1;
	EXPECT_EQ(notify_buf_dequeue(), NULL);
	EXPECT_STREQ(log_data[write_log_call_count % 5],
		     "Error notify_buf_dequeue: malloc failed.\n");
}

TEST_F(NotifyBuffer_Initialized, ReallocSuccess)
{

	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	EXPECT_NE(notify_buf.elems, NULL);
	EXPECT_EQ(notify_buf.max_len, FUSE_NOTIFY_BUF_DEFAULT_LEN * 8);
}

TEST_F(NotifyBuffer_Initialized, ReallocSkiped)
{
	notify_buf.is_initialized = 0;
	notify_buf_realloc();
	EXPECT_EQ(realloc_fake.call_count, 0);
}

TEST_F(NotifyBuffer_Initialized, ReallocFailed_Limitation)
{
	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	notify_buf_realloc();
	EXPECT_EQ(notify_buf_realloc(), -1);
	EXPECT_EQ(errno, ENOMEM);
	EXPECT_NE(notify_buf.elems, NULL);
	EXPECT_LE(notify_buf.max_len, FUSE_NOTIFY_BUF_MAX_LEN);
}

TEST_F(NotifyBuffer_Initialized, ReallocByEnqueue)
{
	int32_t i;
	notify_buf.max_len = 2;
	notify_buf.in = 1;

	ut_enqueue(4);
	ASSERT_EQ(notify_buf.max_len, 4);
	EXPECT_EQ(0,
		  memcmp(data, notify_buf.elems, 4 * sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, ReallocAndConcateBuffer)
{
	int32_t i;
	notify_buf.max_len = 2;
	notify_buf.in = 1;

	ut_enqueue(2);
	ut_dequeue(1);
	ut_enqueue(2);
	/* After add 2: [2, 1]
	 * After add 3: [ , 1, 2, 3] */
	ASSERT_EQ(notify_buf.max_len, 4);
	EXPECT_EQ(0, memcmp(&data[1], &notify_buf.elems[1],
			    3 * sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, ReallocAndConcateBufferFail)
{
	int32_t i;
	notify_buf.max_len = 2;
	notify_buf.in = 1;

	realloc_error_on = 1;
	ut_enqueue(3);
	ASSERT_EQ(notify_buf.max_len, 2);
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
	init_hfuse_ll_notify_loop();
	EXPECT_EQ(malloc_fake.call_count, 1);
	ASSERT_NE(notify_buf.elems, NULL);
	ASSERT_EQ(notify_buf.is_initialized, 1);
}

TEST_F(NotifyBuffer_Initialized, initLoopFail)
{
	malloc_error_on = 1;
	reset_unittest_env();
	ASSERT_EQ(init_hfuse_ll_notify_loop(), -1);
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
	init_hfuse_ll_notify_loop();
	hcfs_system->system_going_down = TRUE;
	sem_post_error_on = 1;
	destory_hfuse_ll_notify_loop();
	/* Test thread is running */
	ASSERT_EQ(pthread_kill(fuse_nofify_thread, 0), 0);
	sem_post_error_on = -1;
	destory_hfuse_ll_notify_loop();
}

TEST_F(NotifyBuffer_Initialized, destoryLoop_PthreadJoinFail)
{
	init_hfuse_ll_notify_loop();
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify_buf.tasks_sem);
	pthread_join(fuse_nofify_thread, NULL);
	destory_hfuse_ll_notify_loop();
	EXPECT_STREQ(log_data[write_log_call_count % 5],
		     "Error destory_hfuse_ll_notify_loop: Failed to join "
		     "nofify_thread. No such process\n");
}

TEST_F(NotifyBuffer_Initialized, loopCallNotify)
{
	int32_t task;
	struct timespec wait = {0, 100000};
	init_hfuse_ll_notify_loop();
	ut_enqueue(5);
	sem_post(&notify_buf.tasks_sem);
	sem_post(&notify_buf.tasks_sem);
	sem_post(&notify_buf.tasks_sem);
	sem_post(&notify_buf.tasks_sem);
	sem_post(&notify_buf.tasks_sem);
	while (1) {
		sem_wait(&notify_buf.access_sem);
		sem_getvalue(&notify_buf.tasks_sem, &task);
		sem_post(&notify_buf.access_sem);
		if(task == 0)
			break;
		/* waiting loop to finish */
		nanosleep(&wait, NULL);
	}
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify_buf.tasks_sem);
	pthread_join(fuse_nofify_thread, NULL);
	EXPECT_STREQ(log_data[(write_log_call_count + 5 - 1) % 5],
		     "Debug hfuse_ll_notify_loop: Notified\n");
}

TEST_F(NotifyBuffer_Initialized, loopCallNotifyFailed)
{
	int32_t task;
	struct timespec wait = {0, 100000};
	init_hfuse_ll_notify_loop();
	ut_enqueue(1);
	malloc_error_on = 1;
	sem_post(&notify_buf.tasks_sem);
	while (1) {
		sem_getvalue(&notify_buf.tasks_sem, &task);
		if(task == 0)
			break;
		/* waiting loop to finish */
		nanosleep(&wait, NULL);
	}
	hcfs_system->system_going_down = TRUE;
	sem_post(&notify_buf.tasks_sem);
	pthread_join(fuse_nofify_thread, NULL);
	EXPECT_STREQ(log_data[(write_log_call_count + 5 - 1) % 5],
		     "Error hfuse_ll_notify_loop: Dequeue failed\n");
}

TEST_F(NotifyBuffer_Initialized, _do_hfuse_ll_notify_delete_RUN)
{
	FUSE_NOTIFY_DATA *data =
	    (FUSE_NOTIFY_DATA *)calloc(1, sizeof(FUSE_NOTIFY_DATA));
	((FUSE_NOTIFY_DELETE_DATA *)data)->func = DELETE;
	((FUSE_NOTIFY_DELETE_DATA *)data)->name = (char *)malloc(1);
	_do_hfuse_ll_notify_delete(&data, RUN);
	free(data);
	EXPECT_EQ(fuse_lowlevel_notify_delete_fake.call_count, 1);
}


TEST_F(NotifyBuffer_Initialized, notify_delete)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	hfuse_ll_notify_delete(ch, 0, 0, name, 1);
	free(ch);
	free(name);
	free(((FUSE_NOTIFY_DELETE_DATA*)&notify_buf.elems[0])->name);
	EXPECT_EQ(notify_buf.len, 1);
}

TEST_F(NotifyBuffer_Initialized, notify_deleteFail)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t ret;
	notify_buf.is_initialized = 0;
	ret = hfuse_ll_notify_delete(ch, 0, 0, name, 1);
	free(ch);
	free(name);
	EXPECT_EQ(notify_buf.len, 0);
	EXPECT_EQ(ret, -1);
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
	free(ch);
	free(name);
	free(fake_mp);
	for (i = 0; i < MP_TYPE_NUM - 1; i++)
		free(((FUSE_NOTIFY_DELETE_DATA *)&notify_buf.elems[i])->name);
	EXPECT_EQ(notify_buf.len, 2);
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
	free(ch);
	free(name);
	free(fake_mp);
	for (i = 0; i < MP_TYPE_NUM; i++)
		free(((FUSE_NOTIFY_DELETE_DATA *)&notify_buf.elems[i])->name);
	EXPECT_EQ(notify_buf.len, 3);
}

TEST_F(NotifyBuffer_Initialized, notify_delete_mp_BufferOverflow)
{
	struct fuse_chan *ch = (struct fuse_chan *)malloc(1);
	struct fuse_chan *fake_mp = (struct fuse_chan *)malloc(1);
	char *name = strdup("a");
	int32_t i;
	int32_t ret;

	for (i = 1; i <= MP_TYPE_NUM; i++)
		mount_global.ch[i] = fake_mp;
	write_log_hide = 10;
	for (i = 0; i < 1024 / 3; i++) {
		ret = hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name);
		EXPECT_EQ(ret, 0);
	}
	write_log_hide = 11;
	EXPECT_EQ(hfuse_ll_notify_delete_mp(ch, 0, 0, name, 1, name), -1);
	EXPECT_EQ(notify_buf.len, 1024);
	free(ch);
	free(name);
	free(fake_mp);
	for (i = 0; i < 1024; i++)
		free(((FUSE_NOTIFY_DELETE_DATA *)&notify_buf.elems[i])->name);
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
	free(ch);
	free(name);
	free(name2);
	for (i = 0; i < MP_TYPE_NUM; i++)
		free(((FUSE_NOTIFY_DELETE_DATA *)&notify_buf.elems[i])->name);
	EXPECT_EQ(notify_buf.len, 3);
}
