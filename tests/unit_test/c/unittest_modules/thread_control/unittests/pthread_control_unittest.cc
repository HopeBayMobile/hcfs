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
void* test_create_ftn(void *ptr) {
	sem_t waitsem;

	printf("Running PTHREAD_create\n");
	testval = 0;
	sem_init(&waitsem, 0, 0);
	sem_wait(&waitsem);
	testval = 100;
	return NULL;
}
void* test_normal_ftn(void *ptr) {
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
	int ret = PTHREAD_create(&testthread, NULL, test_create_ftn, NULL);
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
/* End of unittests for function PTHREAD_set_exithandler */

