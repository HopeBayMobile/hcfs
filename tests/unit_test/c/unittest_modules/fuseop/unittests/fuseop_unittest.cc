#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
extern "C" {
#include "fuseop.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

static void * _mount_test_fuse(void *ptr) {
  char **argv;
  int ret_val;

  argv = (char **) malloc(sizeof(char *)*3);
  argv[0] = (char *) malloc(sizeof(char)*100);
  argv[1] = (char *) malloc(sizeof(char)*100);
  argv[2] = (char *) malloc(sizeof(char)*100);

  snprintf(argv[0],90,"test_fuse");
  snprintf(argv[1],90,"/tmp/test_fuse");
  snprintf(argv[2],90,"-f");
  ret_val = mkdir("/tmp/test_fuse",0777);
  printf("created return %d\n",ret_val);
  hook_fuse(3,argv);
  return NULL;
}

class fuseopEnvironment : public ::testing::Environment {
 public:
  pthread_t new_thread;

  virtual void SetUp() {

    logfptr = fopen("/tmp/test_fuse_log","a+");
    setbuf(logfptr, NULL);

    pthread_create(&new_thread, NULL, &_mount_test_fuse, NULL);
    sleep(5);
  }

  virtual void TearDown() {
    int ret_val, tmp_err;

    if (fork() == 0)
     execlp("fusermount","fusermount","-u","/tmp/test_fuse",(char *) NULL);
    else
     wait(NULL);
    sleep(1);
    ret_val = rmdir("/tmp/test_fuse");
    tmp_err = errno;
    printf("delete return %d\n",ret_val);
    unlink("/tmp/test_fuse_log");
  }
};

::testing::Environment* const fuseop_env = ::testing::AddGlobalTestEnvironment(new fuseopEnvironment);

/* Begin of the test case for the function hfuse_getattr */

class hfuse_getattrTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
   }

  virtual void TearDown() {
   }
};

TEST_F(hfuse_getattrTest, EmptyTest) {
  ASSERT_EQ(access("/tmp/test_fuse",F_OK),0);
}
TEST_F(hfuse_getattrTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = access("/tmp/test_fuse/does_not_exist",F_OK);
  tmp_err = errno;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_getattrTest, TestRoot) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = stat("/tmp/test_fuse", &tempstat);
  tmp_err = errno;
  ASSERT_EQ(ret_val, 0);
  EXPECT_EQ(tempstat.st_atime, 100000);
}

/* End of the test case for the function hfuse_getattr */

