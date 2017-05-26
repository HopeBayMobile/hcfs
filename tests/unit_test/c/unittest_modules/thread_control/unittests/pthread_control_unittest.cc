/* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved. */
#include <pthread.h>
#include <signal.h>
extern "C" {
#include "pthread_control.h"
}
#include "gtest/gtest.h"

#define UNUSED(x) ((void)x)

int checkval;
int32_t testval;
sigset_t signalset;

void mock_ftn(int sig)
{
	checkval = 100;
}

/* Begin unittests for function PTHREAD_sighandler_init */
class PTHREAD_sighandler_initTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		checkval = 0;
	}

	virtual void TearDown() {
	}
};

TEST_F(PTHREAD_sighandler_initTest, InitNone)
{
	PTHREAD_sighandler_init(NULL);
	pthread_kill(pthread_self(), SIGUSR2);
	EXPECT_EQ(0, checkval);
}

TEST_F(PTHREAD_sighandler_initTest, InitTest)
{
	PTHREAD_sighandler_init(&mock_ftn);
	pthread_kill(pthread_self(), SIGUSR2);
	EXPECT_EQ(100, checkval);
}

/* End of unittests for function PTHREAD_sighandler_init */

/* Begin unittests for function PTHREAD_set_exithandler */
void* test_thread_ftn(void *ptr) {
	sem_t waitsem;

	PTHREAD_set_exithandler();
	pthread_setspecific(PTHREAD_status_key, ptr);
	testval = 0;
	sem_init(&waitsem, 0, 0);
	pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	sem_wait(&waitsem);
	testval = 100;
	return NULL;
}

class PTHREAD_set_exithandlerTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		checkval = 0;
		testval = 0;
		sigemptyset(&signalset);
		sigaddset(&signalset, SIGUSR2);
		pthread_sigmask(SIG_BLOCK, &signalset, NULL);
	}

	virtual void TearDown() {
		pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	}
};

TEST_F(PTHREAD_set_exithandlerTest, NoSetup)
{
	pthread_t testthread;
	pthread_create(&testthread, NULL, test_thread_ftn, NULL);
	sleep(1);
	pthread_kill(testthread, SIGUSR2);
	pthread_join(testthread, NULL);
	EXPECT_EQ(100, testval);
}

TEST_F(PTHREAD_set_exithandlerTest, PTHREADNotCancelable)
{
	PTHREAD_T testthread;

	memset(&testthread, 0, sizeof(PTHREAD_T));
	testthread.cancelable = 0;
	testthread.terminating = 0;
	pthread_create(&(testthread.self), NULL, test_thread_ftn, &testthread);
	sleep(1);
	pthread_kill(testthread.self, SIGUSR2);
	pthread_join(testthread.self, NULL);
	EXPECT_EQ(100, testval);
	EXPECT_NE(0, testthread.terminating);
}

TEST_F(PTHREAD_set_exithandlerTest, PTHREADCancelable)
{
	PTHREAD_T testthread;

	memset(&testthread, 0, sizeof(PTHREAD_T));
	testthread.cancelable = 1;
	testthread.terminating = 0;
	pthread_create(&(testthread.self), NULL, test_thread_ftn, &testthread);
	sleep(1);
	pthread_kill(testthread.self, SIGUSR2);
	pthread_join(testthread.self, NULL);
	EXPECT_EQ(0, testval);
}

/* End of unittests for function PTHREAD_set_exithandler */

/* Begin unittests for function PTHREAD_REUSE_set_exithandler */
void* test_reuse_thread_ftn(void *ptr) {
	sem_t waitsem;

	PTHREAD_REUSE_set_exithandler();
	pthread_setspecific(PTHREAD_status_key, ptr);
	testval = 0;
	sem_init(&waitsem, 0, 0);
	pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	sem_wait(&waitsem);
	testval = 100;
	return NULL;
}

class PTHREAD_REUSE_set_exithandlerTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		checkval = 0;
		testval = 0;
		sigemptyset(&signalset);
		sigaddset(&signalset, SIGUSR2);
		pthread_sigmask(SIG_BLOCK, &signalset, NULL);
	}

	virtual void TearDown() {
		pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	}
};

TEST_F(PTHREAD_REUSE_set_exithandlerTest, NoSetup)
{
	pthread_t testthread;
	pthread_create(&testthread, NULL, test_reuse_thread_ftn, NULL);
	sleep(1);
	pthread_kill(testthread, SIGUSR2);
	pthread_join(testthread, NULL);
	EXPECT_EQ(100, testval);
}

TEST_F(PTHREAD_REUSE_set_exithandlerTest, PTHREADNotCancelable)
{
	PTHREAD_REUSE_T testthread;

	memset(&testthread, 0, sizeof(PTHREAD_T));
	testthread.cancelable = 0;
	testthread.terminating = 0;
	pthread_create(&(testthread.self), NULL, test_reuse_thread_ftn, &testthread);
	sleep(1);
	pthread_kill(testthread.self, SIGUSR2);
	pthread_join(testthread.self, NULL);
	EXPECT_EQ(100, testval);
	EXPECT_NE(0, testthread.terminating);
}

TEST_F(PTHREAD_REUSE_set_exithandlerTest, PTHREADCancelable)
{
	PTHREAD_T testthread;

	memset(&testthread, 0, sizeof(PTHREAD_T));
	testthread.cancelable = 1;
	testthread.terminating = 0;
	pthread_create(&(testthread.self), NULL, test_reuse_thread_ftn, &testthread);
	sleep(1);
	pthread_kill(testthread.self, SIGUSR2);
	pthread_join(testthread.self, NULL);
	EXPECT_EQ(0, testval);
}

/* End of unittests for function PTHREAD_REUSE_set_exithandler */

/* Begin unittests for function PTHREAD_create */
void* test_block_ftn(void *ptr)
{
	sem_t waitsem;

	printf("Running PTHREAD_create\n");
	testval = 0;
	sem_init(&waitsem, 0, 0);
	sem_wait(&waitsem);
	testval = 100;
	return NULL;
}
void* test_normal_ftn(void *ptr)
{
	printf("Running PTHREAD_create\n");
	testval = 100;
	return NULL;
}
class PTHREAD_createTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		checkval = 0;
		testval = 0;
		sigemptyset(&signalset);
		sigaddset(&signalset, SIGUSR2);
		pthread_sigmask(SIG_BLOCK, &signalset, NULL);
	}

	virtual void TearDown() {
		pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	}
};

TEST_F(PTHREAD_createTest, NormalCreateAndKill)
{
	PTHREAD_T testthread;

	PTHREAD_set_exithandler();
	int ret = PTHREAD_create(&testthread, NULL, test_block_ftn, NULL);
	EXPECT_EQ(0, ret);
	sleep(1);
	PTHREAD_kill(&testthread, SIGUSR2);
	PTHREAD_join(&testthread, NULL);
	EXPECT_EQ(0, testval);
}

TEST_F(PTHREAD_createTest, NormalCreateAndJoin)
{
	PTHREAD_T testthread;

	PTHREAD_set_exithandler();
	int ret = PTHREAD_create(&testthread, NULL, test_normal_ftn, NULL);
	EXPECT_EQ(0, ret);
	PTHREAD_join(&testthread, NULL);
	EXPECT_EQ(100, testval);
}

TEST_F(PTHREAD_createTest, NormalCreateAndDetach)
{
	PTHREAD_T testthread;

	pthread_attr_t thread_attr;
	pthread_attr_init(&(thread_attr));
	pthread_attr_setdetachstate(&(thread_attr),
			PTHREAD_CREATE_DETACHED);

	PTHREAD_set_exithandler();
	int ret = PTHREAD_create(&testthread, &thread_attr, test_normal_ftn, NULL);
	EXPECT_EQ(0, ret);
	sleep(1);
	ret = PTHREAD_join(&testthread, NULL);
	EXPECT_EQ(EINVAL, ret);
	EXPECT_EQ(100, testval);
}
/* End of unittests for function PTHREAD_create */

/* Begin unittests for function PTHREAD_REUSE_create */
void* test_reuse_block_ftn(void *ptr)
{
	sem_t waitsem;

	printf("Running PTHREAD_REUSE_create\n");
	testval = 0;
	sem_init(&waitsem, 0, 0);
	sem_wait(&waitsem);
	testval = 100;
	return NULL;
}
void* test_reuse_normal_ftn(void *ptr)
{
	printf("Running PTHREAD_REUSE_create\n");
	testval = *((int *) ptr);
	return NULL;
}
void* test_reuse_sum_ftn(void *ptr)
{
	printf("Running PTHREAD_REUSE_create\n");
	printf("%d\n", *((int *) ptr));
	testval += *((int *) ptr);
	checkval++;
	return NULL;
}
class PTHREAD_REUSE_createTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		checkval = 0;
		testval = 0;
		sigemptyset(&signalset);
		sigaddset(&signalset, SIGUSR2);
		pthread_sigmask(SIG_BLOCK, &signalset, NULL);
	}

	virtual void TearDown() {
		pthread_sigmask(SIG_UNBLOCK, &signalset, NULL);
	}
};

TEST_F(PTHREAD_REUSE_createTest, CreateAndTerminate)
{
	PTHREAD_REUSE_T testthread;

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, NULL);
	EXPECT_EQ(0, ret);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(0, testval);
}

TEST_F(PTHREAD_REUSE_createTest, RunAndTerminateBlocked)
{
	PTHREAD_REUSE_T testthread;

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, NULL);
	EXPECT_EQ(0, ret);
	PTHREAD_REUSE_run(&testthread, test_reuse_block_ftn, NULL);
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(0, testval);
}
TEST_F(PTHREAD_REUSE_createTest, RunAndTerminateBlockedNotCancelable)
{
	PTHREAD_REUSE_T testthread;

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, NULL);
	EXPECT_EQ(0, ret);
	sleep(1);
	testthread.cancelable = 0;
	PTHREAD_REUSE_run(&testthread, test_reuse_block_ftn, NULL);
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(100, testval);
}
TEST_F(PTHREAD_REUSE_createTest, DetachedAndTerminateBlocked)
{
	PTHREAD_REUSE_T testthread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&(thread_attr));
	pthread_attr_setdetachstate(&(thread_attr),
			PTHREAD_CREATE_DETACHED);

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, &thread_attr);
	EXPECT_EQ(0, ret);
	PTHREAD_REUSE_run(&testthread, test_reuse_block_ftn, NULL);
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(0, testval);
}

TEST_F(PTHREAD_REUSE_createTest, RunAndTerminateNonblocked)
{
	PTHREAD_REUSE_T testthread;
	int count = 100;

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, NULL);
	EXPECT_EQ(0, ret);
	PTHREAD_REUSE_run(&testthread, test_reuse_normal_ftn,
	                  (void *) &count);
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(100, testval);
}
TEST_F(PTHREAD_REUSE_createTest, ReuseMultipleTimes)
{
	PTHREAD_REUSE_T testthread;
	int count;

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, NULL);
	EXPECT_EQ(0, ret);
	for (count = 0; count < 100; count++) {
		PTHREAD_REUSE_run(&testthread, test_reuse_normal_ftn,
		                  (void *) &count);
		PTHREAD_REUSE_join(&testthread);
		EXPECT_EQ(count, testval);
	}
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
}
TEST_F(PTHREAD_REUSE_createTest, DetachedReuseMultipleTimes)
{
	PTHREAD_REUSE_T testthread;
	int count, verify_sum;
	pthread_attr_t thread_attr;
	pthread_attr_init(&(thread_attr));
	pthread_attr_setdetachstate(&(thread_attr),
			PTHREAD_CREATE_DETACHED);

	PTHREAD_REUSE_set_exithandler();
	int ret = PTHREAD_REUSE_create(&testthread, &thread_attr);
	testval = 0;
	verify_sum = 0;
	checkval = 0;
	EXPECT_EQ(0, ret);
	for (count = 0; count < 100; count++) {
		ret = PTHREAD_REUSE_run(&testthread, test_reuse_sum_ftn,
		                  (void *) &checkval);
		if (ret < 0) {
			int32_t errcode = errno;
			EXPECT_EQ(EAGAIN, errcode);
			struct timespec sleeptime;
			sleeptime.tv_sec = 0;
			sleeptime.tv_nsec = 1000;
			nanosleep(&sleeptime, NULL);
			count--;
			continue;
		}
		verify_sum += count;
	}
	sleep(1);
	PTHREAD_REUSE_terminate(&testthread);
	EXPECT_EQ(verify_sum, testval);
}
/* End of unittests for function PTHREAD_create */

