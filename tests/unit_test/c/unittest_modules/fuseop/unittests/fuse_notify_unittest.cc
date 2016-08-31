#include <gtest/gtest.h>

extern "C" {
#include "fuse_notify.h"
#include "mount_manager.h"
#include <dlfcn.h>
#include <errno.h>
#include <fuse/fuse_lowlevel.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
}

MOUNT_T_GLOBAL mount_global = {0};
extern FUSE_NOTIFY_CYCLE_BUF notify_cb;
extern fuse_notify_fn *notify_fn[];

/* Definition for fake functions */
#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, sem_init, sem_t *, int, unsigned int);
typedef int(sem_init_f)(sem_t *, int, unsigned int);
sem_init_f *sem_init_real = (sem_init_f *)dlsym(RTLD_NEXT, "sem_init");
uint32_t sem_init_error_on = -1;
int32_t sem_init_cnt(sem_t *sem, int pshared, unsigned int value)
{
	int32_t ret = -1;

	printf("sem_init_fake.call_count %d\n", sem_init_fake.call_count);
	if (sem_init_fake.call_count != sem_init_error_on)
		ret = sem_init_real(sem, pshared, value);
	return ret;
}

FAKE_VALUE_FUNC(void *, realloc, void *, size_t);
typedef void *(realloc_f)(void *, size_t);
realloc_f *realloc_real = (realloc_f *)dlsym(RTLD_NEXT, "realloc");
uint32_t realloc_error_on = -1;
void *realloc_cnt(void *ptr, size_t size)
{
	void *ret = NULL;

	printf("realloc_fake.call_count %d\n", realloc_fake.call_count);
	if (realloc_fake.call_count != realloc_error_on)
		ret = realloc_real(ptr, size);
	return ret;
}

/* gtests */

int32_t write_log_call = 0;
int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;

	write_log_call++;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

FUSE_NOTIFY_DATA data[20];
int32_t ut_enqueue_call = 0;
void ut_enqueue(size_t n)
{
	size_t i, in = 0;
	static size_t seq = __LINE__;

	for (i = 0; i < n; i++) {
		if (in == notify_cb.max_len)
			in = 0;
		printf("Fill with %lu\n", seq);
		memset(&data[ut_enqueue_call % 20], seq,
		       sizeof(FUSE_NOTIFY_DATA));
		notify_cb_enqueue((void *)&data[ut_enqueue_call % 20]);
		in = notify_cb.in + 1;
		ut_enqueue_call++;
		seq++;
	}
}
void ut_dequeue(int32_t n)
{
	FUSE_NOTIFY_DATA *d = NULL;
	int32_t i;
	for (i = 0; i < n; i++) {
		d = notify_cb_dequeue();
		free(d);
	}
}

void reset_unittest_env(void)
{
	RESET_FAKE(sem_init);
	sem_init_error_on = -1;
	sem_init_fake.custom_fake = sem_init_cnt;

	RESET_FAKE(realloc);
	realloc_error_on = -1;
	realloc_fake.custom_fake = realloc_cnt;

	FFF_RESET_HISTORY();

	write_log_call = 0;
	ut_enqueue_call = 0;
	free(notify_cb.elems);
	memset(&notify_cb, 0, sizeof(FUSE_NOTIFY_CYCLE_BUF));
	memset(&data, 0, sizeof(data));
}

class NotifyBufferSetUpAndTearDown : public ::testing::Test
{
	protected:
	virtual void SetUp() { reset_unittest_env(); }

	virtual void TearDown() { reset_unittest_env(); }
};

TEST_F(NotifyBufferSetUpAndTearDown, InitSuccess)
{
	init_notify_cb();
	ASSERT_NE(notify_cb.elems, NULL);
	ASSERT_EQ(notify_cb.is_initialized, 1);
	free(notify_cb.elems);
	notify_cb.elems = NULL;
}

TEST_F(NotifyBufferSetUpAndTearDown, InitFail)
{

	sem_init_fake.custom_fake = sem_init_cnt;
	sem_init_error_on = 2;
	init_notify_cb();
	EXPECT_EQ(notify_cb.elems, NULL);
	EXPECT_EQ(notify_cb.is_initialized, 0);
}

TEST_F(NotifyBufferSetUpAndTearDown, DestroySkip)
{
	memset(&notify_cb, 0, sizeof(FUSE_NOTIFY_CYCLE_BUF));
	destory_notify_cb();
	EXPECT_EQ(write_log_call, 0);
}
TEST_F(NotifyBufferSetUpAndTearDown, DestroySuccess)
{
	FUSE_NOTIFY_DATA d;

	memset(&d, 0, sizeof(FUSE_NOTIFY_DATA));

	init_notify_cb();
	ASSERT_NE(notify_cb.elems, NULL);
	ASSERT_EQ(notify_cb.is_initialized, 1);

	notify_cb_enqueue((void *)&d);
	destory_notify_cb();
	ASSERT_EQ(notify_cb.elems, NULL);
	ASSERT_EQ(notify_cb.is_initialized, 0);
}

class NotifyBuffer_Initialized : public ::testing::Test
{
	protected:
	virtual void SetUp()
	{
		reset_unittest_env();
		init_notify_cb();
	}

	virtual void TearDown() { reset_unittest_env(); }
};

TEST_F(NotifyBuffer_Initialized, Enqueue)
{
	ut_enqueue(1);
	EXPECT_EQ(0, memcmp(data, notify_cb.elems, sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, EnqueueSkiped)
{
	notify_cb.is_initialized = 0;
	ut_enqueue(1);
	EXPECT_EQ(notify_cb.len, 0);
}

TEST_F(NotifyBuffer_Initialized, Dequeue)
{
	FUSE_NOTIFY_DATA *d = NULL;
	ut_enqueue(1);
	d = notify_cb_dequeue();
	ASSERT_NE(d, NULL);
	EXPECT_EQ(0, memcmp(d, notify_cb.elems, sizeof(FUSE_NOTIFY_DATA)));
	free(d);
}

TEST_F(NotifyBuffer_Initialized, DequeueSkiped)
{
	ut_enqueue(1);
	notify_cb.is_initialized = 0;
	notify_cb_dequeue();
	EXPECT_EQ(notify_cb.len, 1);
}

TEST_F(NotifyBuffer_Initialized, DequeueAnEmptyQueue) { 
	notify_cb_dequeue(); 
}

TEST_F(NotifyBuffer_Initialized, ReallocSuccess)
{

	notify_cb_realloc();
	notify_cb_realloc();
	notify_cb_realloc();
	EXPECT_NE(notify_cb.elems, NULL);
	EXPECT_EQ(notify_cb.max_len, FUSE_NOTIFY_CB_DEFAULT_LEN * 8);
}

TEST_F(NotifyBuffer_Initialized, ReallocSkiped)
{
	notify_cb.is_initialized = 0;
	notify_cb_realloc();
	EXPECT_EQ(realloc_fake.call_count, 0);
}

TEST_F(NotifyBuffer_Initialized, EnqueueToDoubleLength)
{
	int32_t i;
	notify_cb.max_len = 2;
	notify_cb.in = 1;

	ut_enqueue(4);
	ASSERT_EQ(notify_cb.max_len, 4);
	EXPECT_EQ(0,
		  memcmp(data, notify_cb.elems, 4 * sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, ConcateBuffOnRealloc)
{
	int32_t i;
	notify_cb.max_len = 2;
	notify_cb.in = 1;

	ut_enqueue(2);
	ut_dequeue(1);
	ut_enqueue(2);
	/* After add 2: [2, 1]
	 * After add 3: [ , 1, 2, 3] */
	ASSERT_EQ(notify_cb.max_len, 4);
	EXPECT_EQ(0, memcmp(&data[1], &notify_cb.elems[1],
			    3 * sizeof(FUSE_NOTIFY_DATA)));
}

TEST_F(NotifyBuffer_Initialized, ConcateBuffOnReallocFail)
{
	int32_t i;
	notify_cb.max_len = 2;
	notify_cb.in = 1;

	realloc_error_on = 1;
	ut_enqueue(3);
	ASSERT_EQ(notify_cb.max_len, 2);
}
