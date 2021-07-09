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
#include "api_interface_unittest.h"

#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <ftw.h>
#include <pthread.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "api_interface.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "mount_manager.h"
#include "hcfscurl.h"
#include "pthread_control.h"
}
#include "gtest/gtest.h"

#define UNUSED(x) ((void)x)

BACKEND_TOKEN_CONTROL swifttoken_control = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER
};

extern struct timespec api_server_monitor_time;
SYSTEM_CONF_STRUCT *system_config;

char swift_auth_string[1024] = {0};
char swift_url_string[1024] = {0};

/* Begin of the test case for the function init_api_interface */

#define SEND(x) ASSERT_EQ(sizeof(x), send(fd, &(x), sizeof(x), 0))
#define SENDBUF(buf, size) ASSERT_EQ(size, send(fd, &buf, size, 0));
#define API_SEND(in_code)                                                      \
	do {                                                                   \
		uint32_t code = in_code;                                       \
		uint32_t size = 0;                                             \
		printf("Start sending\n");                                     \
		SEND(code);                                                    \
		SEND(size);                                                    \
	} while (0)
#define API_SEND1(in_code, data, data_len)                                     \
	do {                                                                   \
		uint32_t code = in_code;                                       \
		uint32_t size = data_len;                                      \
		printf("Start sending\n");                                     \
		SEND(code);                                                    \
		SEND(size);                                                    \
		SENDBUF(data, data_len);                                       \
	} while (0)

#define RECV(x) ASSERT_EQ(sizeof(x), recv(fd, &(x), sizeof(x), 0))
#define API_RECV()                                                             \
	do {                                                                   \
		uint32_t size = 0;                                             \
		printf("Start recv\n");                                        \
		RECV(size);                                                    \
		ASSERT_EQ(0, size);                                            \
	} while (0)
#define API_RECV1(data)                                                        \
	do {                                                                   \
		uint32_t size;                                                 \
		printf("Start recv\n");                                        \
		RECV(size);                                                    \
		ASSERT_EQ(sizeof(data), size);                                 \
		RECV(data);                                                    \
	} while (0)

class UnittestEnv : public ::testing::Environment
{
	public:
	virtual void SetUp()
	{
		api_server_monitor_time = { 0, 1000 * 1000 * 10 };
	}

	virtual void TearDown() {}
};

::testing::Environment *const fuseop_env =
    ::testing::AddGlobalTestEnvironment(new UnittestEnv);

class init_api_interfaceTest : public ::testing::Test {
	protected:
	int32_t count;

	virtual void SetUp() {
		hcfs_system = (SYSTEM_DATA_HEAD *)
		              malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = false;
		hcfs_system->backend_is_online = true;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->fuse_sem), 0, 0);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
	}

	virtual void TearDown() {
		hcfs_system->system_going_down = true;

		if (api_server != NULL) {
		 	for (count = 0; count < api_server->num_threads; count++)
		 		PTHREAD_kill(&(api_server->local_thread[count]), SIGUSR2);
		 	PTHREAD_kill(&(api_server->monitor_thread), SIGUSR2);
			for (count = 0; count < api_server->num_threads; count++)
				PTHREAD_join(&(api_server->local_thread[count]), NULL);
			PTHREAD_join(&(api_server->monitor_thread), NULL);
			sem_destroy(&(api_server->job_lock));
			free(api_server);
			api_server = NULL;
		}
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		free(hcfs_system);
	}
};

/* Testing whether init API server is correct */
TEST_F(init_api_interfaceTest, TestIntegrity)
{
	int32_t ret_val;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	if (api_server == NULL)
		ret_val = -1;
	else
		ret_val = 0;
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	EXPECT_EQ(INIT_API_THREADS, api_server->num_threads);
	sem_getvalue(&(api_server->job_lock), &ret_val);
	EXPECT_EQ(1, ret_val);
}

/* Testing if can correctly cleanup the environment before init */
TEST_F(init_api_interfaceTest, TestPreCleanup)
{
	int32_t ret_val;

	if (access(SOCK_PATH, F_OK) != 0)
		mknod(SOCK_PATH, S_IFREG | O_RDWR, 0);
	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	if (api_server == NULL)
		ret_val = -1;
	else
		ret_val = 0;
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
}

/* End of the test case for the function init_api_interface */

/* Begin of the test case for the function destroy_api_interface */

class destroy_api_interfaceTest : public ::testing::Test {
	protected:
	int32_t count;

	virtual void SetUp() {
		hcfs_system = (SYSTEM_DATA_HEAD *)
		              malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = false;
		hcfs_system->backend_is_online = true;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->fuse_sem), 0, 0);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
	}

	virtual void TearDown() {
		hcfs_system->system_going_down = true;

		if (api_server != NULL) {
		 	for (count = 0; count < api_server->num_threads; count++)
		 		PTHREAD_kill(&(api_server->local_thread[count]), SIGUSR2);
		 	PTHREAD_kill(&(api_server->monitor_thread), SIGUSR2);
			for (count = 0; count < api_server->num_threads; count++)
				PTHREAD_join(&(api_server->local_thread[count]), NULL);
			PTHREAD_join(&(api_server->monitor_thread), NULL);
			sem_destroy(&(api_server->job_lock));
			free(api_server);
			api_server = NULL;
		}
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		free(hcfs_system);
	}
};

/* Testing if destory process of API server runs correctly */
TEST_F(destroy_api_interfaceTest, TestIntegrity)
{
	int32_t ret_val, retcode;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);

	hcfs_system->system_going_down = true;

	ret_val = destroy_api_interface();
	ASSERT_EQ(0, ret_val);
	retcode = 0;
	ret_val = access(SOCK_PATH, F_OK);
	retcode = errno;
	EXPECT_NE(0, ret_val);
	EXPECT_EQ(ENOENT, retcode);
	if (api_server == NULL)
		ret_val = -1;
	else
		ret_val = 0;
	EXPECT_EQ(-1, ret_val);
}

/* End of the test case for the function destroy_api_interface */

/* Begin of the test case for the function api_module */

class api_moduleTest : public ::testing::TestWithParam<int32_t>
{
	protected:
	int32_t count;
	int32_t fd, status;
	struct sockaddr_un addr;

	virtual void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
		                malloc(sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_cache_limit =
		    (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
		system_config->max_pinned_limit =
		    (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = false;
		hcfs_system->backend_is_online = true;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->access_sem), 0, 1);
		sem_init(&(hcfs_system->fuse_sem), 0, 0);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		fd = 0;
		METAPATH = (char *)malloc(sizeof(char) * 100);
		snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
		nftw("/tmp/testHCFS", &do_delete, 20, FTW_DEPTH);
		mkdir("/tmp/testHCFS", 0700);
		if (access(METAPATH, F_OK) != 0)
			mkdir(METAPATH, 0700);
		HCFSPAUSESYNC = (char *)malloc(strlen(METAPATH) + 20);
		ASSERT_EQ(true, HCFSPAUSESYNC != NULL);
		snprintf(HCFSPAUSESYNC, strlen(METAPATH) + 20,
		         "%s/hcfspausesync", METAPATH);
		ASSERT_EQ(0, init_api_interface());
		ASSERT_EQ(0, access(SOCK_PATH, F_OK));
		ASSERT_EQ(0, connect_sock());
		ASSERT_NE(0, fd);
	}

	virtual void TearDown() {
		if (fd != 0)
			close(fd);
		hcfs_system->system_going_down = true;

		if (api_server != NULL) {
		 	for (count = 0; count < api_server->num_threads; count++)
		 		PTHREAD_kill(&(api_server->local_thread[count]), SIGUSR2);
		 	PTHREAD_kill(&(api_server->monitor_thread), SIGUSR2);
			for (count = 0; count < api_server->num_threads;
			     count++) {
				PTHREAD_kill(&(api_server->local_thread[count]), SIGUSR2);
				PTHREAD_join(&(api_server->local_thread[count]),
				             NULL);
			}
			PTHREAD_kill(&(api_server->monitor_thread), SIGUSR2);
			PTHREAD_join(&(api_server->monitor_thread), NULL);
			sem_destroy(&(api_server->job_lock));
			free(api_server);
			api_server = NULL;
		}
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
		free(HCFSPAUSESYNC);
		free(METAPATH);
		free(system_config->max_cache_limit);
		free(system_config->max_pinned_limit);
		free(hcfs_system);
		free(system_config);
	}

	int32_t connect_sock() {
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, SOCK_PATH);
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		status = connect(fd, (sockaddr *)&addr, sizeof(addr));
		return status;
	}

	static int32_t do_delete(const char *fpath,
	                         const struct stat *sb,
	                         int32_t tflag,
	                         struct FTW *ftwbuf) {
		UNUSED(sb);
		UNUSED(ftwbuf);
		switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir(fpath);
			break;
		default:
			unlink(fpath);
			break;
		}
		return (0);
	}
};

/* Test a single API call */
TEST_F(api_moduleTest, SingleTest)
{
	int32_t retcode;

	API_SEND(TESTAPI);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
}

/* Test API call for checking system stat */
TEST_F(api_moduleTest, StatTest)
{
	int32_t ret_val;
	uint32_t size_msg;
	char ans_string[50], tmp_str[50];

	hcfs_system->systemdata.system_size = 100000;
	hcfs_system->systemdata.cache_size = 2000;
	hcfs_system->systemdata.cache_blocks = 13;
	snprintf(ans_string, 50, "%ld %ld %ld",
		 hcfs_system->systemdata.system_size,
		 hcfs_system->systemdata.cache_size,
		 hcfs_system->systemdata.cache_blocks);

	API_SEND(VOLSTAT);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	EXPECT_EQ(strlen(ans_string) + 1, size_msg);
	ret_val = recv(fd, tmp_str, size_msg, 0);
	ASSERT_EQ(size_msg, ret_val);
	EXPECT_STREQ(ans_string, tmp_str);
}

/* Test API call correctness for a call with large arg size */
TEST_F(api_moduleTest, LargeEchoTest)
{
	int32_t ret_val, count;
	uint32_t cmd_len, size_msg;
	char teststr[2048], recvstr[2048];
	int32_t bytes_recv;

	/* Fill up test string */
	for (count = 0; count < 2000; count++)
		teststr[count] = (count % 10) + '0';
	teststr[2000] = 0;

	cmd_len = 2001;
	API_SEND1(ECHOTEST, teststr, cmd_len);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(2001, size_msg);
	bytes_recv = 0;
	while ((uint32_t)bytes_recv < size_msg) {
		ret_val = recv(fd, &(recvstr[bytes_recv]), 2001, 0);
		bytes_recv += ret_val;
		if (ret_val == 0)
			break;
	}
	ASSERT_EQ(2001, bytes_recv);
	ret_val = strncmp(teststr, recvstr, 1000);
	EXPECT_EQ(0, ret_val);
	ret_val = strncmp(&(teststr[1000]), &(recvstr[1000]), 1001);
	EXPECT_EQ(0, ret_val);
}

/* Test handling for unsupported API calls */
TEST_F(api_moduleTest, InvalidCode)
{
	int32_t retcode;

	API_SEND(99999999);
	API_RECV1(retcode);
	ASSERT_EQ(ENOTSUP, retcode);
}

/* Test system termination call */
TEST_F(api_moduleTest, TerminateTest)
{
	int32_t ret_val, retcode;

	UNMOUNTEDALL = false;
	API_SEND(TERMINATE);

	sem_post(&(api_server->shutdown_sem));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, hcfs_system->system_going_down);
	ASSERT_EQ(true, UNMOUNTEDALL);
	/* Check if terminate will indeed signal threads sleeping on
	something_to_replace */
	sem_getvalue(&(hcfs_system->something_to_replace), &ret_val);
	ASSERT_EQ(1, ret_val);
}

/* Test CREATEVOL API call */
TEST_F(api_moduleTest, CreateFSTest)
{
	int32_t retcode;
	char tmpstr[10];

	CREATEDFS = false;
	snprintf(tmpstr, 10, "123456789");
	API_SEND1(CREATEVOL, tmpstr, sizeof(tmpstr));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, CREATEDFS);
	EXPECT_STREQ("123456789", recvFSname);
}

/* Test DELETEVOL API call */
TEST_F(api_moduleTest, DeleteFSTest)
{
	int32_t retcode;
	char tmpstr[10];

	DELETEDFS = false;
	snprintf(tmpstr, 10, "123456789");
	API_SEND1(DELETEVOL, tmpstr, sizeof(tmpstr));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, DELETEDFS);
	EXPECT_STREQ("123456789", recvFSname);
}

/* Test CHECKVOL API call */
TEST_F(api_moduleTest, CheckFSTest)
{
	int32_t retcode;
	char tmpstr[10];

	CHECKEDFS = false;
	snprintf(tmpstr, 10, "123456789");
	API_SEND1(CHECKVOL, tmpstr, sizeof(tmpstr));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, CHECKEDFS);
	EXPECT_STREQ("123456789", recvFSname);
}

/* Test LISTVOL API call */
TEST_F(api_moduleTest, ListFSTestNoFS)
{
	LISTEDFS = false;
	numlistedFS = 0;
	API_SEND(LISTVOL);
	API_RECV();
	ASSERT_EQ(true, LISTEDFS);
}

/* Test LISTVOL API call */
TEST_F(api_moduleTest, ListFSTestOneFS)
{
	DIR_ENTRY tmp_entry;

	LISTEDFS = false;
	numlistedFS = 1;

	API_SEND(LISTVOL);
	API_RECV1(tmp_entry);
	ASSERT_STREQ("test123", tmp_entry.d_name);
	ASSERT_EQ(true, LISTEDFS);
}

/* Test MOUNTVOL API call */
TEST_F(api_moduleTest, MountFSTest)
{
	int32_t retcode;
	uint32_t code, cmd_len;
	char tmpstr[10];
	char mpstr[10];
	int32_t fsname_len;
	char mp_mode;

	MOUNTEDFS = false;
	code = MOUNTVOL;
	cmd_len =
	    sizeof(int32_t) + sizeof(char) + sizeof(tmpstr) + sizeof(mpstr);
	fsname_len = 10;
	snprintf(tmpstr, 10, "123456789");
	snprintf(mpstr, 10, "123456789");
	mp_mode = MP_DEFAULT;
	printf("Start sending\n");
	SEND(code);
	SEND(cmd_len);
	SEND(mp_mode);
	SEND(fsname_len);
	SEND(tmpstr);
	SEND(mpstr);

	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, MOUNTEDFS);
	EXPECT_STREQ("123456789", recvFSname);
	EXPECT_STREQ("123456789", recvmpname);
}

/* Test UNMOUNTVOL API call */
TEST_F(api_moduleTest, UnmountFSTest)
{
	int32_t retcode;
	uint32_t code, cmd_len, fsname_len;
	char tmpstr[10];

	UNMOUNTEDFS = false;
	code = UNMOUNTVOL;
	cmd_len = 20 + sizeof(int32_t);
	snprintf(tmpstr, 10, "123456789");
	fsname_len = 10;
	printf("Start sending\n");
	SEND(code);
	SEND(cmd_len);
	SEND(fsname_len);
	SEND(tmpstr);
	SEND(tmpstr);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, UNMOUNTEDFS);
	EXPECT_STREQ("123456789", recvFSname);
}

/* Test CHECKMOUNT API call */
TEST_F(api_moduleTest, CheckMountTest)
{
	int32_t retcode;
	char tmpstr[10];

	CHECKEDMOUNT = false;
	snprintf(tmpstr, 10, "123456789");
	API_SEND1(CHECKMOUNT, tmpstr, sizeof(tmpstr));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, CHECKEDMOUNT);
	EXPECT_STREQ("123456789", recvFSname);
}

/* Test UNMOUNTALL API call */
TEST_F(api_moduleTest, UnmountAllTest)
{
	int32_t retcode;

	UNMOUNTEDALL = false;
	API_SEND(UNMOUNTALL);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, UNMOUNTEDALL);
}

TEST_F(api_moduleTest, pin_inodeTest_InvalidPinType)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	int64_t reserved_size;
	char pin_type;
	uint32_t num_inode;

	PIN_INODE_ROLLBACK = false;
	reserved_size = 1000;
	pin_type = 3; /* This pin type is not supported */
	num_inode = 0;
	memcpy(buf, &reserved_size, sizeof(int64_t));
	memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
	memcpy(buf + sizeof(int64_t) + sizeof(char), &num_inode, sizeof(uint32_t));
	/* Space not available */
	hcfs_system->systemdata.pinned_size = 0;

	cmd_len = sizeof(int64_t) + sizeof(char);
	API_SEND1(PIN, buf, cmd_len);
	API_RECV1(retcode);
	ASSERT_EQ(-EINVAL, retcode);
}

TEST_F(api_moduleTest, pin_inodeTest_NoSpace)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	int64_t reserved_size;
	uint8_t pin_type;
	uint32_t num_inode;

	PIN_INODE_ROLLBACK = false;
	reserved_size = 1000;
	pin_type = 1;
	num_inode = 0;
	memcpy(buf, &reserved_size, sizeof(int64_t));
	memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
	memcpy(buf + sizeof(int64_t) + sizeof(char),
	       &num_inode, sizeof(uint32_t));
	/* Space not available */
	hcfs_system->systemdata.pinned_size = 0;
	system_config->max_cache_limit[pin_type] = 300;
	system_config->max_pinned_limit[pin_type] = 300 * 0.8;

	cmd_len = sizeof(int64_t) + sizeof(uint32_t);
	API_SEND1(PIN, buf, cmd_len);
	API_RECV1(retcode);
	ASSERT_EQ(-ENOSPC, retcode);
}

TEST_F(api_moduleTest, pin_inodeTest_Success)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	int64_t reserved_size;
	uint8_t pin_type;
	uint32_t num_inode;
	ino_t inode_list[1];

	PIN_INODE_ROLLBACK = false; /* pin_inode() success */
	reserved_size = 10;
	pin_type = 1;
	num_inode = 1;
	inode_list[0] = 5;
	cmd_len = sizeof(int64_t) + +sizeof(char) +
	          sizeof(uint32_t) + sizeof(ino_t);
	memcpy(buf, &reserved_size, sizeof(int64_t));
	memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
	memcpy(buf + sizeof(int64_t) + sizeof(char), &num_inode, sizeof(uint32_t));
	memcpy(buf + sizeof(int64_t) + sizeof(char) + sizeof(uint32_t),
	       &inode_list, sizeof(ino_t));
	hcfs_system->systemdata.pinned_size = 0;
	system_config->max_cache_limit[pin_type] = 500;
	system_config->max_pinned_limit[pin_type] = 500 * 0.8;

	API_SEND1(PIN, buf, cmd_len);

	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
}

TEST_F(api_moduleTest, pin_inodeTest_RollBack)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	int64_t reserved_size;
	uint8_t pin_type;
	uint32_t num_inode;
	ino_t inode_list[1];

	PIN_INODE_ROLLBACK = true; /* pin_inode() fail */
	reserved_size = 10;
	pin_type = 1;
	num_inode = 1;
	inode_list[0] = 5;
	cmd_len = sizeof(int64_t) + sizeof(char) +
	          sizeof(uint32_t) + sizeof(ino_t);
	memcpy(buf, &reserved_size, sizeof(int64_t));
	memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
	memcpy(buf + sizeof(int64_t) + sizeof(char), &num_inode, sizeof(uint32_t));
	memcpy(buf + sizeof(int64_t) + sizeof(char) + sizeof(uint32_t),
	       &inode_list, sizeof(ino_t));
	hcfs_system->systemdata.pinned_size = 0;
	system_config->max_cache_limit[pin_type] = 500;
	system_config->max_pinned_limit[pin_type] = 500 * 0.8;

	API_SEND1(PIN, buf, cmd_len);
	API_RECV1(retcode);
	ASSERT_EQ(-EIO, retcode);
}

TEST_F(api_moduleTest, unpin_inodeTest_Success)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	uint32_t num_inode;
	ino_t inode_list[1];

	UNPIN_INODE_FAIL = false; /* unpin_inode() success */
	num_inode = 1;
	inode_list[0] = 5;
	cmd_len = sizeof(uint32_t) + sizeof(ino_t);
	memcpy(buf, &num_inode, sizeof(uint32_t));
	memcpy(buf + sizeof(uint32_t),
	       &inode_list, sizeof(ino_t));
	CACHE_HARD_LIMIT = 500;
	hcfs_system->systemdata.pinned_size = 0;

	API_SEND1(UNPIN, buf, cmd_len);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
}

TEST_F(api_moduleTest, unpin_inodeTest_Fail)
{
	int32_t retcode;
	uint32_t cmd_len;
	char buf[300];
	uint32_t num_inode;
	ino_t inode_list[1];

	UNPIN_INODE_FAIL = true; /* unpin_inode() success */
	num_inode = 1;
	inode_list[0] = 5;
	cmd_len = sizeof(uint32_t) + sizeof(ino_t);
	memcpy(buf, &num_inode, sizeof(uint32_t));
	memcpy(buf + sizeof(uint32_t),
	       &inode_list, sizeof(ino_t));
	CACHE_HARD_LIMIT = 500;
	hcfs_system->systemdata.pinned_size = 0;

	API_SEND1(UNPIN, buf, cmd_len);

	API_RECV1(retcode);
	ASSERT_EQ(-EIO, retcode);
}

/* Test CLOUDSTAT API call */
TEST_F(api_moduleTest, CloudState)
{
	int32_t retcode;

	API_SEND(CLOUDSTAT);
	API_RECV1(retcode);
	ASSERT_EQ(true, retcode);
	ASSERT_EQ(true, hcfs_system->backend_is_online);
}

/* Test SETSYNCSWITCH API call */
TEST_F(api_moduleTest, SetSyncSwitch)
{
	int32_t retcode;
	uint32_t sw;

	/* Disable sync */
	hcfs_system->sync_manual_switch = true;
	sw = false;
	API_SEND1(SETSYNCSWITCH, sw, sizeof(uint32_t));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(false, hcfs_system->sync_manual_switch);

	ASSERT_EQ(0, connect_sock());

	/* Enable sync */
	hcfs_system->sync_manual_switch = false;
	sw = true;
	mknod(HCFSPAUSESYNC, S_IFREG | 0600, 0);
	API_SEND1(SETSYNCSWITCH, sw, sizeof(uint32_t));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, hcfs_system->sync_manual_switch);
}

TEST_F(api_moduleTest, SetSyncSwitchOnFail)
{
	int32_t retcode;
	uint32_t sw = true;

	/* Enable sync */
	hcfs_system->sync_manual_switch = false;
	mkdir(HCFSPAUSESYNC, 0700);

	API_SEND1(SETSYNCSWITCH, sw, sizeof(uint32_t));

	API_RECV1(retcode);

	ASSERT_EQ(-21, retcode);
	ASSERT_EQ(true, hcfs_system->sync_manual_switch);
}

/* Test GETSYNCSWITCH API call */
TEST_F(api_moduleTest, GetSyncSwitch)
{
	int32_t retcode;

	hcfs_system->sync_manual_switch = ON;
	API_SEND(GETSYNCSWITCH);
	API_RECV1(retcode);
	ASSERT_EQ(retcode, ON);

	ASSERT_EQ(0, connect_sock());

	hcfs_system->sync_manual_switch = OFF;
	API_SEND(GETSYNCSWITCH);
	API_RECV1(retcode);
	ASSERT_EQ(retcode, OFF);
}

/* Test GETSYNCSTAT API call */
TEST_F(api_moduleTest, GetSyncStat)
{
	int32_t retcode;

	API_SEND(GETSYNCSTAT);
	API_RECV1(retcode);
	ASSERT_EQ(retcode, !hcfs_system->sync_paused);
}

TEST_F(api_moduleTest, ReloadConfigSuccess)
{
	int32_t retcode;

	API_SEND(RELOADCONFIG);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
}

TEST_F(api_moduleTest, GetQuotaSuccess)
{
	int64_t quota;

	hcfs_system->systemdata.system_quota = 55667788;

	API_SEND(GETQUOTA);
	API_RECV1(quota);
	ASSERT_EQ(55667788, quota);
}

TEST_F(api_moduleTest, GetMetaSizeSuccess)
{
	int64_t metasize;

	hcfs_system->systemdata.system_meta_size = 55667788;

	API_SEND(GETMETASIZE);
	API_RECV1(metasize);
	ASSERT_EQ(55667788, metasize);
}

TEST_F(api_moduleTest, UpdateQuotaSuccess)
{
	int32_t retcode;

	hcfs_system->systemdata.system_quota = 0; /* It will be modified */

	API_SEND(TRIGGERUPDATEQUOTA);
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	EXPECT_EQ(5566, hcfs_system->systemdata.system_quota);
}

TEST_F(api_moduleTest, ChangeLogLevelSuccess)
{
	int32_t retcode;
	int32_t loglevel;

	system_config->log_level = 10; /* Original level */
	loglevel = 6; /* New level */

	API_SEND1(CHANGELOG, loglevel, sizeof(loglevel));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);

	EXPECT_EQ(6, system_config->log_level);
}

TEST_F(api_moduleTest, GetTotalCloudSizeSuccess)
{
	int64_t cloudsize;

	hcfs_system->systemdata.backend_size = 12345566;

	API_SEND(GETCLOUDSIZE);
	API_RECV1(cloudsize);
	ASSERT_EQ(12345566, cloudsize);
}

TEST_F(api_moduleTest, GetOccupiedSizeSuccess)
{
	int64_t occupiedsize;

	hcfs_system->systemdata.unpin_dirty_data_size = 556677;
	hcfs_system->systemdata.pinned_size = 655405;

	API_SEND(OCCUPIEDSIZE);
	API_RECV1(occupiedsize);
	ASSERT_EQ(655405 + 556677, occupiedsize);
}

TEST_F(api_moduleTest, UnpinDirtySizeSuccess)
{
	int64_t unpindirtysize;

	hcfs_system->systemdata.unpin_dirty_data_size = 556677;

	API_SEND(UNPINDIRTYSIZE);
	API_RECV1(unpindirtysize);
	ASSERT_EQ(556677, unpindirtysize);
}

TEST_F(api_moduleTest, XferStatusNoTransit)
{
	int32_t status;

	hcfs_system->systemdata.xfer_now_window = 0;
	hcfs_system->xfer_upload_in_progress = false;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);

	API_SEND(GETXFERSTATUS);
	API_RECV1(status);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, XferStatusNormalTransit)
{
	int32_t status;

	memset(hcfs_system->systemdata.xfer_throughput,
	       0, sizeof(int64_t) * 6);
	memset(hcfs_system->systemdata.xfer_total_obj,
	       0, sizeof(int64_t) * 6);
	hcfs_system->systemdata.xfer_now_window = 1;
	hcfs_system->systemdata.xfer_throughput[1] = 1000;
	hcfs_system->systemdata.xfer_total_obj[1] = 1;
	hcfs_system->xfer_upload_in_progress = true;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);

	API_SEND(GETXFERSTATUS);
	API_RECV1(status);
	ASSERT_EQ(1, status);
}

TEST_F(api_moduleTest, XferStatusSlowTransit)
{
	int32_t status;

	memset(hcfs_system->systemdata.xfer_throughput,
	       0, sizeof(int64_t) * 6);
	memset(hcfs_system->systemdata.xfer_total_obj,
	       0, sizeof(int64_t) * 6);
	hcfs_system->systemdata.xfer_now_window = 0;
	hcfs_system->systemdata.xfer_throughput[0] = 10;
	hcfs_system->systemdata.xfer_total_obj[0] = 1;
	hcfs_system->xfer_upload_in_progress = false;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);
	sem_post(&(hcfs_system->xfer_download_in_progress_sem));

	API_SEND(GETXFERSTATUS);
	API_RECV1(status);
	ASSERT_EQ(2, status);
}

TEST_F(api_moduleTest, SetSyncPointReturnSuccess)
{
	int32_t status;

	API_SEND(SETSYNCPOINT);
	API_RECV1(status);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, SetNotifyServerOK)
{
	int32_t status;
	uint32_t cmd_len;
	char buf[300];
	const char *server_path = "setok";

	cmd_len = strlen(server_path) + 1;
	memcpy(buf, server_path, cmd_len);

	API_SEND1(SETNOTIFYSERVER, buf, cmd_len);

	API_RECV1(status);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, CancelSyncPointSuccess)
{
	int32_t status;

	API_SEND(SETSYNCPOINT);
	API_RECV1(status);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, SetNotifyServerFailed)
{
	int32_t status;
	uint32_t cmd_len;
	char buf[300];
	const char *server_path = "setfailed";

	cmd_len = strlen(server_path) + 1;
	memcpy(buf, server_path, cmd_len);

	API_SEND1(SETNOTIFYSERVER, buf, cmd_len);
	API_RECV1(status);
	ASSERT_EQ(-1, status);
}
/* End of the test case for the function api_module */

/* Begin of the test case for the function api_server_monitor */

class api_server_monitorTest : public ::testing::Test {
	protected:
	int32_t count;
	int32_t fd[10];
	int32_t status;
	struct sockaddr_un addr;

	virtual void SetUp() {
		hcfs_system = (SYSTEM_DATA_HEAD *)
		              malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = false;
		hcfs_system->backend_is_online = true;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->fuse_sem), 0, 0);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		for (count = 0; count < 10; count++)
			fd[count] = 0;
		ASSERT_EQ(0, init_api_interface());
		ASSERT_EQ(0, access(SOCK_PATH, F_OK));
		ASSERT_EQ(0, connect_sock());
		ASSERT_NE(0, fd);
	}

	virtual void TearDown() {
		for (count = 0; count < 10; count++) {
			if (fd[count] != 0)
				close(fd[count]);
		}
		hcfs_system->system_going_down = true;

		if (api_server != NULL) {
			/* Adding lock wait before terminating to prevent last sec
			   thread changes */
			sem_wait(&(api_server->job_lock));
		 	for (count = 0; count < api_server->num_threads; count++)
		 		PTHREAD_kill(&(api_server->local_thread[count]), SIGUSR2);
		 	PTHREAD_kill(&(api_server->monitor_thread), SIGUSR2);
			for (count = 0; count < api_server->num_threads; count++)
				PTHREAD_join(&(api_server->local_thread[count]), NULL);
			PTHREAD_join(&(api_server->monitor_thread), NULL);
			sem_post(&(api_server->job_lock));
			sem_destroy(&(api_server->job_lock));
			free(api_server);
			api_server = NULL;
		}
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		free(hcfs_system);
	}

	int32_t connect_sock() {
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, SOCK_PATH);
		for (count = 0; count < 10; count++) {
			fd[count] = socket(AF_UNIX, SOCK_STREAM, 0);
			status = connect(fd[count], (sockaddr *)&addr, sizeof(addr));
			if (status != 0)
				break;
		}
		return status;
	}
};

/* Test if number of threads will increase under heavy loading */
TEST_F(api_server_monitorTest, TestThreadIncrease)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;
	int32_t count1;


	code = TESTAPI;
	cmd_len = 0;
	printf("Start sending\n");
	for (count1 = 0; count1 < 10; count1++) {
		size_msg = send(fd[count1], &code, sizeof(uint32_t), 0);
		ASSERT_EQ(sizeof(uint32_t), size_msg);
		size_msg = send(fd[count1], &cmd_len, sizeof(uint32_t), 0);
		ASSERT_EQ(sizeof(uint32_t), size_msg);
	}
	printf("Start recv\n");
	for (count1 = 0; count1 < 10; count1++) {
		ret_val = recv(fd[count1], &size_msg, sizeof(uint32_t), 0);
		ASSERT_EQ(sizeof(uint32_t), ret_val);
		ASSERT_EQ(sizeof(uint32_t), size_msg);
		ret_val = recv(fd[count1], &retcode, sizeof(uint32_t), 0);
		ASSERT_EQ(sizeof(uint32_t), ret_val);
		ASSERT_EQ(0, retcode);
	}
	sleep(1);
	EXPECT_GE(api_server->num_threads, INIT_API_THREADS);
}

TEST_F(api_moduleTest, GetMinimalApkStatus)
{
	int32_t retcode;

	hcfs_system->use_minimal_apk = true;
	API_SEND(GET_MINIMAL_APK_STATUS);
	API_RECV1(retcode);
	ASSERT_EQ(retcode, true);

	ASSERT_EQ(0, connect_sock());

	hcfs_system->use_minimal_apk = false;
	API_SEND(GET_MINIMAL_APK_STATUS);
	API_RECV1(retcode);
	ASSERT_EQ(retcode, false);
}

TEST_F(api_moduleTest, SetMinimalApkStatus)
{
	int32_t retcode;
	uint32_t sw = true;

	/* Disable sync */
	hcfs_system->use_minimal_apk = true;
	sw = false;
	API_SEND1(TOGGLE_USE_MINIMAL_APK, sw, sizeof(uint32_t));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(false, hcfs_system->use_minimal_apk);

	ASSERT_EQ(0, connect_sock());

	/* Enable sync */
	hcfs_system->use_minimal_apk = false;
	sw = true;
	API_SEND1(TOGGLE_USE_MINIMAL_APK, sw, sizeof(uint32_t));
	API_RECV1(retcode);
	ASSERT_EQ(0, retcode);
	ASSERT_EQ(true, hcfs_system->use_minimal_apk);
}

/* End of the test case for the function api_server_monitor */
