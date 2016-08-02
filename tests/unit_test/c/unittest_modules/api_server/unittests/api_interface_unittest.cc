/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include "api_interface_unittest.h"

#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <ftw.h>

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
}
#include "gtest/gtest.h"

#define UNUSED(x) ((void)x)

extern int32_t api_server_monitor_time;
SYSTEM_CONF_STRUCT *system_config;

/* Begin of the test case for the function init_api_interface */

class init_api_interfaceTest : public ::testing::Test {
 protected:
  int32_t count;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;
    sem_init(&(hcfs_system->fuse_sem), 0, 0);
    sem_init(&(hcfs_system->something_to_replace), 0, 0);
    if (access(SOCK_PATH, F_OK) == 0)
      unlink(SOCK_PATH);
  }

  virtual void TearDown() {

    hcfs_system->system_going_down = TRUE;

    if (api_server != NULL) {
      for (count = 0; count < api_server->num_threads; count++)
        pthread_join(api_server->local_thread[count], NULL);
      pthread_join(api_server->monitor_thread, NULL);
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
TEST_F(init_api_interfaceTest, TestIntegrity) {

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
TEST_F(init_api_interfaceTest, TestPreCleanup) {

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
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;
    sem_init(&(hcfs_system->fuse_sem), 0, 0);
    sem_init(&(hcfs_system->something_to_replace), 0, 0);
    if (access(SOCK_PATH, F_OK) == 0)
      unlink(SOCK_PATH);
   }

  virtual void TearDown() {

    hcfs_system->system_going_down = TRUE;

    if (api_server != NULL) {
      for (count = 0; count < api_server->num_threads; count++)
        pthread_join(api_server->local_thread[count], NULL);
      pthread_join(api_server->monitor_thread, NULL);
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
TEST_F(destroy_api_interfaceTest, TestIntegrity) {

  int32_t ret_val, errcode;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);

  hcfs_system->system_going_down = TRUE;

  ret_val = destroy_api_interface();
  ASSERT_EQ(0, ret_val);
  errcode = 0;
  ret_val = access(SOCK_PATH, F_OK);
  errcode = errno;
  EXPECT_NE(0, ret_val);
  EXPECT_EQ(ENOENT, errcode);
  if (api_server == NULL)
    ret_val = -1;
  else
    ret_val = 0;
  EXPECT_EQ(-1, ret_val);
}

/* End of the test case for the function destroy_api_interface */

/* Begin of the test case for the function api_module */

class api_moduleTest : public ::testing::Test
{
	protected:
	int32_t count;
	int32_t fd, status;
	struct sockaddr_un addr;

	virtual void SetUp()
	{
    		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_cache_limit =
			(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
		system_config->max_pinned_limit =
			(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
		api_server_monitor_time = 1;
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
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
		nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
		mkdir("/tmp/testHCFS", 0700);
		if (access(METAPATH, F_OK) != 0)
			mkdir(METAPATH, 0700);
		HCFSPAUSESYNC = (char *)malloc(strlen(METAPATH) + 20);
		ASSERT_EQ(TRUE, HCFSPAUSESYNC != NULL);
		snprintf(HCFSPAUSESYNC, strlen(METAPATH) + 20,
			 "%s/hcfspausesync", METAPATH);
	}

	virtual void TearDown()
	{

		if (fd != 0)
			close(fd);
		hcfs_system->system_going_down = TRUE;

		if (api_server != NULL) {
			for (count = 0; count < api_server->num_threads;
			     count++)
				pthread_join(api_server->local_thread[count],
					     NULL);
			pthread_join(api_server->monitor_thread, NULL);
			sem_destroy(&(api_server->job_lock));
			free(api_server);
			api_server = NULL;
		}
		if (access(SOCK_PATH, F_OK) == 0)
			unlink(SOCK_PATH);
		nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
		free(HCFSPAUSESYNC);
		free(METAPATH);
		free(hcfs_system);
    		free(system_config);
	}

	int32_t connect_sock()
	{
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, SOCK_PATH);
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		status = connect(fd, (sockaddr *)&addr, sizeof(addr));
		return status;
	}

	static int32_t do_delete(const char *fpath, const struct stat *sb, int32_t tflag,
		      struct FTW *ftwbuf)
	{
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
TEST_F(api_moduleTest, SingleTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = TESTAPI;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
 }

/* Test API call for checking system stat */
TEST_F(api_moduleTest, StatTest) {

  int32_t ret_val;
  uint32_t code, cmd_len, size_msg;
  char ans_string[50], tmp_str[50];

  hcfs_system->systemdata.system_size = 100000;
  hcfs_system->systemdata.cache_size = 2000;
  hcfs_system->systemdata.cache_blocks = 13;
  snprintf(ans_string, 50, "%lld %lld %lld",
    hcfs_system->systemdata.system_size,
    hcfs_system->systemdata.cache_size,
    hcfs_system->systemdata.cache_blocks);

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = VOLSTAT;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  EXPECT_EQ(strlen(ans_string)+1, size_msg);
  ret_val = recv(fd, tmp_str, size_msg, 0);
  ASSERT_EQ(size_msg, ret_val);
  EXPECT_STREQ(ans_string, tmp_str);
 }

/* Test API call correctness for a call with large arg size */
TEST_F(api_moduleTest, LargeEchoTest) {

  int32_t ret_val, count;
  uint32_t code, cmd_len, size_msg;
  char teststr[2048], recvstr[2048];
  int32_t bytes_recv;

  /* Fill up test string */
  for (count = 0; count < 2000; count++)
    teststr[count] = (count % 10) + '0';
  teststr[2000] = 0;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = ECHOTEST;
  cmd_len = 2001;
  printf("Start sending\n");
  size_msg = send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg = send(fd, teststr, 2001, 0);

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
TEST_F(api_moduleTest, InvalidCode) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = 99999999;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(ENOTSUP, errcode);
 }

/* Test system termination call */
TEST_F(api_moduleTest, TerminateTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;

  UNMOUNTEDALL = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = TERMINATE;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);

  sem_post(&(api_server->shutdown_sem));
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, hcfs_system->system_going_down);
  ASSERT_EQ(TRUE, UNMOUNTEDALL);
  /* Check if terminate will indeed signal threads sleeping on
  something_to_replace */
  sem_getvalue(&(hcfs_system->something_to_replace), &ret_val);
  ASSERT_EQ(1, ret_val);
 }

/* Test CREATEVOL API call */ 
TEST_F(api_moduleTest, CreateFSTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char tmpstr[10];

  CREATEDFS = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = CREATEVOL;
  cmd_len = 10;
  snprintf(tmpstr, 10, "123456789");
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, CREATEDFS);
  EXPECT_STREQ("123456789", recvFSname);
 }
/* Test DELETEVOL API call */ 
TEST_F(api_moduleTest, DeleteFSTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char tmpstr[10];

  DELETEDFS = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = DELETEVOL;
  cmd_len = 10;
  snprintf(tmpstr, 10, "123456789");
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, DELETEDFS);
  EXPECT_STREQ("123456789", recvFSname);
 }
/* Test CHECKVOL API call */ 
TEST_F(api_moduleTest, CheckFSTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char tmpstr[10];

  CHECKEDFS = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = CHECKVOL;
  cmd_len = 10;
  snprintf(tmpstr, 10, "123456789");
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, CHECKEDFS);
  EXPECT_STREQ("123456789", recvFSname);
 }
/* Test LISTVOL API call */ 
TEST_F(api_moduleTest, ListFSTestNoFS) {

  int32_t ret_val;
  uint32_t code, cmd_len, size_msg;

  LISTEDFS = FALSE;
  numlistedFS = 0;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = LISTVOL;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, size_msg);
  ASSERT_EQ(TRUE, LISTEDFS);
 }

/* Test LISTVOL API call */ 
TEST_F(api_moduleTest, ListFSTestOneFS) {

  int32_t ret_val;
  uint32_t code, cmd_len, size_msg;
  DIR_ENTRY tmp_entry;

  LISTEDFS = FALSE;
  numlistedFS = 1;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);

  code = LISTVOL;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(DIR_ENTRY), size_msg);
  ret_val = recv(fd, &tmp_entry, sizeof(DIR_ENTRY), 0);
  ASSERT_EQ(sizeof(DIR_ENTRY), ret_val);
  ASSERT_STREQ("test123", tmp_entry.d_name);
  ASSERT_EQ(TRUE, LISTEDFS);
 }

/* Test MOUNTVOL API call */ 
TEST_F(api_moduleTest, MountFSTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char tmpstr[10];
  char mpstr[10];
  int32_t fsname_len;
  char mp_mode;

  MOUNTEDFS = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = MOUNTVOL;
  cmd_len = 20 + sizeof(int32_t) + sizeof(char);
  fsname_len = 10;
  snprintf(tmpstr, 10, "123456789");
  snprintf(mpstr, 10, "123456789");
  mp_mode = MP_DEFAULT;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &mp_mode, sizeof(char), 0);
  ASSERT_EQ(sizeof(char), size_msg);
  size_msg=send(fd, &fsname_len, sizeof(int32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);

  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  size_msg=send(fd, mpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, MOUNTEDFS);
  EXPECT_STREQ("123456789", recvFSname);
  EXPECT_STREQ("123456789", recvmpname);
 }

/* Test UNMOUNTVOL API call */ 
TEST_F(api_moduleTest, UnmountFSTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg, fsname_len;
  char tmpstr[10];

  UNMOUNTEDFS = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = UNMOUNTVOL;
  cmd_len = 20 + sizeof(int32_t);
  snprintf(tmpstr, 10, "123456789");
  fsname_len = 10;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);

  size_msg=send(fd, &fsname_len, sizeof(int32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, UNMOUNTEDFS);
  EXPECT_STREQ("123456789", recvFSname);
 }

/* Test CHECKMOUNT API call */ 
TEST_F(api_moduleTest, CheckMountTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char tmpstr[10];

  CHECKEDMOUNT = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = CHECKMOUNT;
  cmd_len = 10;
  snprintf(tmpstr, 10, "123456789");
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);

  size_msg=send(fd, tmpstr, 10, 0);
  ASSERT_EQ(10, size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, CHECKEDMOUNT);
  EXPECT_STREQ("123456789", recvFSname);
 }

/* Test UNMOUNTALL API call */
TEST_F(api_moduleTest, UnmountAllTest) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;

  UNMOUNTEDALL = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = UNMOUNTALL;
  cmd_len = 0;
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, UNMOUNTEDALL);
 }

TEST_F(api_moduleTest, pin_inodeTest_InvalidPinType) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  int64_t reserved_size;
  char pin_type;
  uint32_t num_inode;

  PIN_INODE_ROLLBACK = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = PIN;
  reserved_size = 1000;
  pin_type = 3; /* This pin type is not supported */
  num_inode = 0;
  cmd_len = sizeof(int64_t) + sizeof(uint32_t);
  memcpy(buf, &reserved_size, sizeof(int64_t));
  memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
  memcpy(buf + sizeof(int64_t) + sizeof(char),
		  &num_inode, sizeof(uint32_t));
  /* Space not available */
  hcfs_system->systemdata.pinned_size = 0;

  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(-EINVAL, errcode);
 }

TEST_F(api_moduleTest, pin_inodeTest_NoSpace) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  int64_t reserved_size;
  char pin_type;
  uint32_t num_inode;

  PIN_INODE_ROLLBACK = FALSE;
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = PIN;
  reserved_size = 1000;
  pin_type = 1;
  num_inode = 0;
  cmd_len = sizeof(int64_t) + sizeof(uint32_t);
  memcpy(buf, &reserved_size, sizeof(int64_t));
  memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
  memcpy(buf + sizeof(int64_t) + sizeof(char),
		  &num_inode, sizeof(uint32_t));
  /* Space not available */
  hcfs_system->systemdata.pinned_size = 0;
  system_config->max_cache_limit[pin_type] = 300;
  system_config->max_pinned_limit[pin_type] = 300 * 0.8;

  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(-ENOSPC, errcode);
 }

TEST_F(api_moduleTest, pin_inodeTest_Success) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  int64_t reserved_size;
  char pin_type;
  uint32_t num_inode;
  ino_t inode_list[1];

  PIN_INODE_ROLLBACK = FALSE; /* pin_inode() success */
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = PIN;
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

  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
 }

TEST_F(api_moduleTest, pin_inodeTest_RollBack) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  int64_t reserved_size;
  char pin_type;
  uint32_t num_inode;
  ino_t inode_list[1];

  PIN_INODE_ROLLBACK = TRUE; /* pin_inode() fail */
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);
  code = PIN;
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

  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(-EIO, errcode);
 }

TEST_F(api_moduleTest, unpin_inodeTest_Success) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  uint32_t num_inode;
  ino_t inode_list[1];

  UNPIN_INODE_FAIL = FALSE; /* unpin_inode() success */
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);

  code = UNPIN;
  num_inode = 1;
  inode_list[0] = 5;
  cmd_len = sizeof(uint32_t) + sizeof(ino_t);
  memcpy(buf, &num_inode, sizeof(uint32_t));
  memcpy(buf + sizeof(uint32_t),
  		&inode_list, sizeof(ino_t));
  CACHE_HARD_LIMIT = 500;
  hcfs_system->systemdata.pinned_size = 0;

  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(0, errcode);
 }

TEST_F(api_moduleTest, unpin_inodeTest_Fail) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  char buf[300];
  uint32_t num_inode;
  ino_t inode_list[1];

  UNPIN_INODE_FAIL = TRUE; /* unpin_inode() success */
  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, fd);

  code = UNPIN;
  num_inode = 1;
  inode_list[0] = 5;
  cmd_len = sizeof(uint32_t) + sizeof(ino_t);
  memcpy(buf, &num_inode, sizeof(uint32_t));
  memcpy(buf + sizeof(uint32_t),
  		&inode_list, sizeof(ino_t));
  CACHE_HARD_LIMIT = 500; 
  hcfs_system->systemdata.pinned_size = 0;
  
  printf("Start sending\n");
  size_msg=send(fd, &code, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  size_msg=send(fd, &buf, cmd_len, 0);
  ASSERT_EQ(cmd_len, size_msg);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(sizeof(uint32_t), size_msg);
  ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
  ASSERT_EQ(sizeof(uint32_t), ret_val);
  ASSERT_EQ(-EIO, errcode);
}

/* Test CLOUDSTAT API call */
TEST_F(api_moduleTest, CloudState)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = CLOUDSTAT;
	cmd_len = 0;

	hcfs_system->backend_is_online = TRUE;
	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	ret_val = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), ret_val);
	ASSERT_EQ(TRUE, retcode);
	ASSERT_EQ(TRUE, hcfs_system->backend_is_online);
}

/* Test SETSYNCSWITCH API call */
TEST_F(api_moduleTest, SetSyncSwitchOff)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;
	int32_t status;

	hcfs_system->backend_is_online = TRUE;
	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = SETSYNCSWITCH;

	/* Disable sync */
	hcfs_system->sync_manual_switch = TRUE;
	status = FALSE;
	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	cmd_len = sizeof(status);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	size_msg = send(fd, &status, sizeof(status), 0);
	ASSERT_EQ(sizeof(status), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	size_msg = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), size_msg);

	ASSERT_EQ(0, retcode);
	ASSERT_EQ(FALSE, hcfs_system->sync_manual_switch);
}

TEST_F(api_moduleTest, SetSyncSwitchOn)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;
	int32_t status;

	hcfs_system->backend_is_online = TRUE;
	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = SETSYNCSWITCH;

	/* Enable sync */
	hcfs_system->sync_manual_switch = FALSE;
	mknod(HCFSPAUSESYNC, S_IFREG | 0600, 0);
	status = TRUE;
	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	cmd_len = sizeof(status);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	size_msg = send(fd, &status, sizeof(status), 0);
	ASSERT_EQ(sizeof(status), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	size_msg = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), size_msg);

	ASSERT_EQ(0, retcode);
	ASSERT_EQ(TRUE, hcfs_system->sync_manual_switch);
}
TEST_F(api_moduleTest, SetSyncSwitchOnFail)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;
	int32_t status;

	hcfs_system->backend_is_online = TRUE;
	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = SETSYNCSWITCH;

	/* Enable sync */
	hcfs_system->sync_manual_switch = FALSE;
	mkdir(HCFSPAUSESYNC, 0700);
	status = TRUE;
	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	cmd_len = sizeof(status);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	size_msg = send(fd, &status, sizeof(status), 0);
	ASSERT_EQ(sizeof(status), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	size_msg = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), size_msg);

	ASSERT_EQ(-21, retcode);
	ASSERT_EQ(TRUE, hcfs_system->sync_manual_switch);
}

/* Test GETSYNCSWITCH API call */
TEST_F(api_moduleTest, GetSyncSwitch)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = GETSYNCSWITCH;
	cmd_len = 0;

	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	ret_val = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), ret_val);
	ASSERT_EQ(TRUE, retcode);
	ASSERT_EQ(TRUE, hcfs_system->sync_manual_switch);
}

/* Test GETSYNCSTAT API call */
TEST_F(api_moduleTest, GetSyncStat)
{
	int32_t ret_val, retcode;
	uint32_t code, cmd_len, size_msg;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);
	code = GETSYNCSTAT;
	cmd_len = 0;

	printf("Start sending\n");
	size_msg = send(fd, &code, sizeof(code), 0);
	ASSERT_EQ(sizeof(code), size_msg);
	size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
	ASSERT_EQ(sizeof(cmd_len), size_msg);
	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(size_msg), 0);
	ASSERT_EQ(sizeof(size_msg), ret_val);
	ASSERT_EQ(sizeof(retcode), size_msg);
	ret_val = recv(fd, &retcode, sizeof(retcode), 0);
	ASSERT_EQ(sizeof(retcode), ret_val);
	ASSERT_EQ(TRUE, retcode);
	ASSERT_EQ(FALSE, hcfs_system->sync_paused);
}

TEST_F(api_moduleTest, ReloadConfigSuccess) {

	int32_t ret_val, errcode;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = RELOADCONFIG;
	cmd_len = 0;
	memset(buf, 0, 300);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(0, errcode);
}

TEST_F(api_moduleTest, GetQuotaSuccess) {

	int32_t ret_val;
	uint32_t code, cmd_len, size_msg;
	int64_t quota;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	hcfs_system->systemdata.system_quota = 55667788;
	code = GETQUOTA;
	cmd_len = 0;
	memset(buf, 0, 300);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int64_t), size_msg);
	ret_val = recv(fd, &quota, sizeof(int64_t), 0);
	ASSERT_EQ(sizeof(int64_t), ret_val);
	ASSERT_EQ(55667788, quota);
}

TEST_F(api_moduleTest, UpdateQuotaSuccess) {

	int32_t ret_val, errcode;
	uint32_t code, cmd_len, size_msg;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = TRIGGERUPDATEQUOTA;
	cmd_len = 0;
	hcfs_system->systemdata.system_quota = 0; /* It will be modified */

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(0, errcode);

	EXPECT_EQ(5566, hcfs_system->systemdata.system_quota);
}

TEST_F(api_moduleTest, ChangeLogLevelSuccess) {

	int32_t ret_val, errcode;
	uint32_t code, cmd_len, size_msg;
	char buf[300];
	int32_t loglevel;

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = CHANGELOG;
	system_config->log_level = 10; /* Original level */
	loglevel = 6; /* New level */
	cmd_len = sizeof(int32_t);
	memset(buf, 0, 300);
	memcpy(buf, &loglevel, sizeof(int32_t));

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	ret_val = recv(fd, &errcode, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(0, errcode);

	EXPECT_EQ(6, system_config->log_level);
}

TEST_F(api_moduleTest, GetTotalCloudSizeSuccess) {

	int32_t ret_val;
	int64_t cloudsize;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = GETCLOUDSIZE;
	cmd_len = 0;
	memset(buf, 0, 300);
	hcfs_system->systemdata.backend_size = 12345566;

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int64_t), size_msg);
	ret_val = recv(fd, &cloudsize, sizeof(int64_t), 0);
	ASSERT_EQ(sizeof(int64_t), ret_val);
	ASSERT_EQ(12345566, cloudsize);
}

TEST_F(api_moduleTest, GetOccupiedSizeSuccess) {

	int32_t ret_val;
	int64_t occupiedsize;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = OCCUPIEDSIZE;
	cmd_len = 0;
	memset(buf, 0, 300);
	hcfs_system->systemdata.unpin_dirty_data_size = 556677;
	hcfs_system->systemdata.pinned_size = 655405;

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int64_t), size_msg);
	ret_val = recv(fd, &occupiedsize, sizeof(int64_t), 0);
	ASSERT_EQ(sizeof(int64_t), ret_val);
	ASSERT_EQ(655405 + 556677, occupiedsize);
}

TEST_F(api_moduleTest, UnpinDirtySizeSuccess) {

	int32_t ret_val;
	int64_t unpindirtysize;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = UNPINDIRTYSIZE;
	cmd_len = 0;
	memset(buf, 0, 300);
	hcfs_system->systemdata.unpin_dirty_data_size = 556677;

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int64_t), size_msg);
	ret_val = recv(fd, &unpindirtysize, sizeof(int64_t), 0);
	ASSERT_EQ(sizeof(int64_t), ret_val);
	ASSERT_EQ(556677, unpindirtysize);
}

TEST_F(api_moduleTest, XferStatusNoTransit) {

	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = GETXFERSTATUS;
	cmd_len = 0;
	memset(buf, 0, 300);
	hcfs_system->systemdata.xfer_now_window = 0;
	hcfs_system->xfer_upload_in_progress = FALSE;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, XferStatusNormalTransit) {

	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = GETXFERSTATUS;
	cmd_len = 0;
	memset(buf, 0, 300);
	memset(hcfs_system->systemdata.xfer_throughput,
			0, sizeof(int64_t) * 6);
	memset(hcfs_system->systemdata.xfer_total_obj,
			0, sizeof(int64_t) * 6);
	hcfs_system->systemdata.xfer_now_window = 1;
	hcfs_system->systemdata.xfer_throughput[1] = 1000;
	hcfs_system->systemdata.xfer_total_obj[1] = 1;
	hcfs_system->xfer_upload_in_progress = TRUE;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
	ASSERT_EQ(1, status);
}

TEST_F(api_moduleTest, XferStatusSlowTransit) {

	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = GETXFERSTATUS;
	cmd_len = 0;
	memset(buf, 0, 300);
	memset(hcfs_system->systemdata.xfer_throughput,
			0, sizeof(int64_t) * 6);
	memset(hcfs_system->systemdata.xfer_total_obj,
			0, sizeof(int64_t) * 6);
	hcfs_system->systemdata.xfer_now_window = 0;
	hcfs_system->systemdata.xfer_throughput[0] = 10;
	hcfs_system->systemdata.xfer_total_obj[0] = 1;
	hcfs_system->xfer_upload_in_progress = FALSE;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);
	sem_post(&(hcfs_system->xfer_download_in_progress_sem));

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
	ASSERT_EQ(2, status);
}

TEST_F(api_moduleTest, SetSyncPointReturnSuccess) {
	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = SETSYNCPOINT;
	cmd_len = 0;
	memset(buf, 0, 300);
	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, SetNotifyServerOK) {

	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];
	char *server_path = "setok";

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = SETNOTIFYSERVER;
	cmd_len = strlen(server_path) + 1;
	memcpy(buf, server_path, cmd_len);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, CancelSyncPointSuccess) {
	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = SETSYNCPOINT;
	cmd_len = 0;
	memset(buf, 0, 300);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);

	ASSERT_EQ(0, status);
}

TEST_F(api_moduleTest, SetNotifyServerFailed) {

	int32_t ret_val, status;
	uint32_t code, cmd_len, size_msg;
	char buf[300];
	char *server_path = "setfailed";

	ret_val = init_api_interface();
	ASSERT_EQ(0, ret_val);
	ret_val = access(SOCK_PATH, F_OK);
	ASSERT_EQ(0, ret_val);
	ret_val = connect_sock();
	ASSERT_EQ(0, ret_val);
	ASSERT_NE(0, fd);

	code = SETNOTIFYSERVER;
	cmd_len = strlen(server_path) + 1;
	memcpy(buf, server_path, cmd_len);

	printf("Start sending\n");
	size_msg=send(fd, &code, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &cmd_len, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), size_msg);
	size_msg=send(fd, &buf, cmd_len, 0);
	ASSERT_EQ(cmd_len, size_msg);

	printf("Start recv\n");
	ret_val = recv(fd, &size_msg, sizeof(uint32_t), 0);
	ASSERT_EQ(sizeof(uint32_t), ret_val);
	ASSERT_EQ(sizeof(int32_t), size_msg);
	ret_val = recv(fd, &status, sizeof(int32_t), 0);
	ASSERT_EQ(sizeof(int32_t), ret_val);
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
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;
    sem_init(&(hcfs_system->fuse_sem), 0, 0);
    sem_init(&(hcfs_system->something_to_replace), 0, 0);
    if (access(SOCK_PATH, F_OK) == 0)
      unlink(SOCK_PATH);
    for (count = 0; count < 10; count++)
      fd[count] = 0;
   }

  virtual void TearDown() {

    for (count = 0; count < 10; count++) {
      if (fd[count] != 0)
        close(fd[count]);
     }
    hcfs_system->system_going_down = TRUE;

    if (api_server != NULL) {
      /* Adding lock wait before terminating to prevent last sec
         thread changes */
      sem_wait(&(api_server->job_lock));
      for (count = 0; count < api_server->num_threads; count++)
        pthread_join(api_server->local_thread[count], NULL);
      pthread_join(api_server->monitor_thread, NULL);
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
TEST_F(api_server_monitorTest, TestThreadIncrease) {

  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  int32_t count1;

  ret_val = init_api_interface();
  ASSERT_EQ(0, ret_val);
  ret_val = access(SOCK_PATH, F_OK);
  ASSERT_EQ(0, ret_val);
  ret_val = connect_sock();
  ASSERT_EQ(0, ret_val);

  code = TESTAPI;
  cmd_len = 0;
  printf("Start sending\n");
  for (count1 = 0; count1 < 10; count1++) {
    size_msg=send(fd[count1], &code, sizeof(uint32_t), 0);
    ASSERT_EQ(sizeof(uint32_t), size_msg);
    size_msg=send(fd[count1], &cmd_len, sizeof(uint32_t), 0);
    ASSERT_EQ(sizeof(uint32_t), size_msg);
   }
  printf("Start recv\n");
  for (count1 = 0; count1 < 10; count1++) {
    ret_val = recv(fd[count1], &size_msg, sizeof(uint32_t), 0);
    ASSERT_EQ(sizeof(uint32_t), ret_val);
    ASSERT_EQ(sizeof(uint32_t), size_msg);
    ret_val = recv(fd[count1], &errcode, sizeof(uint32_t), 0);
    ASSERT_EQ(sizeof(uint32_t), ret_val);
    ASSERT_EQ(0, errcode);
   }
  sleep(5);
  EXPECT_GE(api_server->num_threads, INIT_API_THREADS);
 }

/* End of the test case for the function api_server_monitor */

