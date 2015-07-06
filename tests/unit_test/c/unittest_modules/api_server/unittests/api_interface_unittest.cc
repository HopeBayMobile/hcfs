#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "api_interface.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"

/* Begin of the test case for the function init_api_interface */

class init_api_interfaceTest : public ::testing::Test {
 protected:
  int count;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
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

  int ret_val;

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

  int ret_val;

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
  int count;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
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

  int ret_val, errcode;

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

class api_moduleTest : public ::testing::Test {
 protected:
  int count;
  int fd, status;
  struct sockaddr_un addr;

  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    if (access(SOCK_PATH, F_OK) == 0)
      unlink(SOCK_PATH);
    fd = 0;
   }

  virtual void TearDown() {

    if (fd != 0)
      close(fd);
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

  int connect_sock() {
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    status = connect(fd, (sockaddr *)&addr, sizeof(addr));
    return status;
   }
 };

/* Test a single API call */ 
TEST_F(api_moduleTest, SingleTest) {

  int ret_val, errcode;
  unsigned int code, cmd_len, size_msg;

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
  size_msg=send(fd, &code, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  ret_val = recv(fd, &errcode, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(0, errcode);
 }

/* Test API call for checking system stat */
TEST_F(api_moduleTest, StatTest) {

  int ret_val, errcode;
  unsigned int code, cmd_len, size_msg;
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
  size_msg=send(fd, &code, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  EXPECT_EQ(strlen(ans_string)+1, size_msg);
  ret_val = recv(fd, tmp_str, size_msg, 0);
  ASSERT_EQ(size_msg, ret_val);
  EXPECT_STREQ(ans_string, tmp_str);
 }

/* Test API call correctness for a call with large arg size */
TEST_F(api_moduleTest, LargeEchoTest) {

  int ret_val, errcode, count;
  unsigned int code, cmd_len, size_msg;
  char teststr[2048], recvstr[2048];
  int bytes_recv;

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
  size_msg = send(fd, &code, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg = send(fd, teststr, 2001, 0);

  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(2001, size_msg);
  bytes_recv = 0;
  while (bytes_recv < size_msg) {
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

  int ret_val, errcode;
  unsigned int code, cmd_len, size_msg;

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
  size_msg=send(fd, &code, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  ret_val = recv(fd, &errcode, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(ENOTSUP, errcode);
 }

void* dummy_HCFS_thread(void *ptr) {
  sigset_t *sigsetptr;
  int ret_sig, ret_val;

  printf("Start of dummy\n");
  sigsetptr = (sigset_t *) ptr;
  ret_val = sigismember(sigsetptr, SIGHUP);
  printf("dummy sig status %d\n", ret_val);
  sigwait(sigsetptr, &ret_sig);
  printf("Finishing this thread\n");
  return NULL;
 }

/* Test system termination call */
TEST_F(api_moduleTest, TerminateTest) {

  int ret_val, errcode;
  unsigned int code, cmd_len, size_msg;
  sigset_t sigset, testset;
  struct timespec waittime;
  struct timeval curtime;
  struct sigaction newact, oldact;

  memset(&newact, 0, sizeof(struct sigaction));
  newact.sa_handler = SIG_IGN;
  sigaction(SIGHUP, NULL, &oldact);
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGHUP);
  ret_val = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
  ret_val = pthread_sigmask(SIG_BLOCK, NULL, &testset);
  ASSERT_EQ(0, ret_val);
  ret_val = sigismember(&testset, SIGHUP);
  ASSERT_EQ(1, ret_val);
  ret_val = sigismember(&sigset, SIGHUP);
  ASSERT_EQ(1, ret_val);

  pthread_create(&HCFS_mount, NULL, dummy_HCFS_thread, (void *)&sigset);

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
  size_msg=send(fd, &code, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  size_msg=send(fd, &cmd_len, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  printf("Start recv\n");
  ret_val = recv(fd, &size_msg, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(sizeof(unsigned int), size_msg);
  ret_val = recv(fd, &errcode, sizeof(unsigned int), 0);
  ASSERT_EQ(sizeof(unsigned int), ret_val);
  ASSERT_EQ(0, errcode);
  ASSERT_EQ(TRUE, hcfs_system->system_going_down);
  gettimeofday(&curtime, NULL);
  waittime.tv_sec = curtime.tv_sec + 10;
  waittime.tv_nsec = 0;
  ret_val = pthread_timedjoin_np(HCFS_mount, NULL, &waittime);
  EXPECT_EQ(0, ret_val);
  if (ret_val < 0)
    pthread_kill(HCFS_mount, SIGKILL);
  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
  sigaction(SIGHUP, &oldact, NULL);

 }

/* End of the test case for the function api_module */

/* Begin of the test case for the function api_server_monitor */

class api_server_monitorTest : public ::testing::Test {

 protected:
  int count;
  int fd[10];
  int status;
  struct sockaddr_un addr;

  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
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

  int connect_sock() {
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

  int ret_val, errcode;
  unsigned int code, cmd_len, size_msg;
  int count1;

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
    size_msg=send(fd[count1], &code, sizeof(unsigned int), 0);
    ASSERT_EQ(sizeof(unsigned int), size_msg);
    size_msg=send(fd[count1], &cmd_len, sizeof(unsigned int), 0);
    ASSERT_EQ(sizeof(unsigned int), size_msg);
   }
  printf("Start recv\n");
  for (count1 = 0; count1 < 10; count1++) {
    ret_val = recv(fd[count1], &size_msg, sizeof(unsigned int), 0);
    ASSERT_EQ(sizeof(unsigned int), ret_val);
    ASSERT_EQ(sizeof(unsigned int), size_msg);
    ret_val = recv(fd[count1], &errcode, sizeof(unsigned int), 0);
    ASSERT_EQ(sizeof(unsigned int), ret_val);
    ASSERT_EQ(0, errcode);
   }
  sleep(5);
  EXPECT_GE(api_server->num_threads, INIT_API_THREADS);
 }

/* End of the test case for the function api_server_monitor */

