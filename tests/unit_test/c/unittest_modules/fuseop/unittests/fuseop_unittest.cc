#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
extern "C" {
#include "fuseop.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"

#include "fake_misc.h"

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
    before_update_file_data = TRUE;
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

/* Begin of the test case for the function hfuse_mknod */

class hfuse_mknodTest : public ::testing::Test {
 protected:
  dev_t tmp_dev;
  virtual void SetUp() {
    fail_super_block_new_inode = FALSE;
    fail_mknod_update_meta = FALSE;
    before_mknod_created = TRUE;
    before_update_file_data = TRUE;
    tmp_dev = makedev(0, 0);
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_mknodTest, ParentNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = mknod("/tmp/test_fuse/does_not_exist/test", 0700, tmp_dev);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_mknodTest, ParentNotDir) {
  int ret_val;
  int tmp_err;

  ret_val = mknod("/tmp/test_fuse/testfile/test", 0700, tmp_dev);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_mknodTest, SuperBlockError) {
  int ret_val;
  int tmp_err;

  fail_super_block_new_inode = TRUE;
  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
}
TEST_F(hfuse_mknodTest, MknodUpdateError) {
  int ret_val;
  int tmp_err;

  fail_mknod_update_meta = TRUE;
  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, 1);
}
TEST_F(hfuse_mknodTest, MknodOK) {
  int ret_val;
  int tmp_err;

  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  EXPECT_EQ(ret_val, 0);
}

/* End of the test case for the function hfuse_mknod */

/* Begin of the test case for the function hfuse_mkdir */

class hfuse_mkdirTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fail_super_block_new_inode = FALSE;
    fail_mkdir_update_meta = FALSE;
    before_mkdir_created = TRUE;
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_mkdirTest, ParentNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = mkdir("/tmp/test_fuse/does_not_exist/test", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_mkdirTest, ParentNotDir) {
  int ret_val;
  int tmp_err;

  ret_val = mkdir("/tmp/test_fuse/testfile/test", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_mkdirTest, SuperBlockError) {
  int ret_val;
  int tmp_err;

  fail_super_block_new_inode = TRUE;
  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
}
TEST_F(hfuse_mkdirTest, MkdirUpdateError) {
  int ret_val;
  int tmp_err;

  fail_mkdir_update_meta = TRUE;
  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, 1);
}
TEST_F(hfuse_mkdirTest, MkdirOK) {
  int ret_val;
  int tmp_err;

  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  EXPECT_EQ(ret_val, 0);
}

/* End of the test case for the function hfuse_mkdir */

/* Begin of the test case for the function hfuse_unlink */

class hfuse_unlinkTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_mkdir_created = TRUE;
    before_mknod_created = TRUE;
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_unlinkTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = unlink("/tmp/test_fuse/does_not_exist");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_unlinkTest, ParentNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = unlink("/tmp/test_fuse/does_not_exist/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_unlinkTest, NotRegFile) {
  int ret_val;
  int tmp_err;

  before_mkdir_created = FALSE;
  ret_val = unlink("/tmp/test_fuse/testmkdir");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}

TEST_F(hfuse_unlinkTest, PathNotDir) {
  int ret_val;
  int tmp_err;

  ret_val = unlink("/tmp/test_fuse/testfile/afile");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_unlinkTest, DeleteSuccess) {
  int ret_val;
  int tmp_err;

  before_mknod_created = FALSE;
  ret_val = unlink("/tmp/test_fuse/testcreate");
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  ret_val = access("/tmp/test_fuse/testcreate", F_OK);
  tmp_err = errno;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}


/* End of the test case for the function hfuse_unlink */

/* Begin of the test case for the function hfuse_rmdir */

class hfuse_rmdirTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_mkdir_created = TRUE;
    before_mknod_created = TRUE;
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_rmdirTest, DirNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = rmdir("/tmp/test_fuse/does_not_exist");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_rmdirTest, ParentNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = rmdir("/tmp/test_fuse/does_not_exist/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_rmdirTest, NotDir) {
  int ret_val;
  int tmp_err;

  before_mknod_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testcreate");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_rmdirTest, PathNotDir) {
  int ret_val;
  int tmp_err;

  ret_val = rmdir("/tmp/test_fuse/testfile/adir");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_rmdirTest, DeleteSelf) {
  int ret_val;
  int tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testmkdir/.");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EINVAL);
}

TEST_F(hfuse_rmdirTest, DeleteParent) {
  int ret_val;
  int tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testmkdir/..");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTEMPTY);
}

TEST_F(hfuse_rmdirTest, DeleteSuccess) {
  int ret_val;
  int tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testmkdir");
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  ret_val = access("/tmp/test_fuse/testmkdir", F_OK);
  tmp_err = errno;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

/* End of the test case for the function hfuse_rmdir */

/* Begin of the test case for the function hfuse_rename */

class hfuse_renameTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_mkdir_created = TRUE;
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_renameTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/does_not_exist", "/tmp/test_fuse/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_renameTest, PrefixCheck) {
  int ret_val;
  int tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rename("/tmp/test_fuse/testmkdir", "/tmp/test_fuse/testmkdir/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EINVAL);
}

TEST_F(hfuse_renameTest, Parent1NotExist) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/does_not_exist/test2", "/tmp/test_fuse/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_renameTest, Parent2NotExist) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile",
			"/tmp/test_fuse/does_not_exist/test2");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_renameTest, SameFile) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile",
			"/tmp/test_fuse/testsamefile");
  tmp_err = errno;

  EXPECT_EQ(ret_val, 0);
}
TEST_F(hfuse_renameTest, SelfDirTargetFile) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testdir1",
			"/tmp/test_fuse/testfile1");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_renameTest, SelfFileTargetDir) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile2",
			"/tmp/test_fuse/testdir2");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}
TEST_F(hfuse_renameTest, TargetDirNotEmpty) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testdir1/",
			"/tmp/test_fuse/testdir2/");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTEMPTY);
}
TEST_F(hfuse_renameTest, RenameFile) {
  int ret_val;
  int tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile1",
			"/tmp/test_fuse/testfile2");
  tmp_err = errno;

  EXPECT_EQ(ret_val, 0);
}

/* End of the test case for the function hfuse_rename */

/* Begin of the test case for the function hfuse_chmod */
class hfuse_chmodTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};
TEST_F(hfuse_chmodTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = chmod("/tmp/test_fuse/does_not_exist", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_chmodTest, ChmodFile) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chmod("/tmp/test_fuse/testfile1", 0444);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_mode, S_IFREG | 0444);
}
TEST_F(hfuse_chmodTest, ChmodDir) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chmod("/tmp/test_fuse/testdir1", 0550);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testdir1", &tempstat);
  EXPECT_EQ(tempstat.st_mode, S_IFDIR | 0550);
}

/* End of the test case for the function hfuse_chmod */

/* Begin of the test case for the function hfuse_chown */
class hfuse_chownTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
  }

  virtual void TearDown() {
  }
};
TEST_F(hfuse_chownTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = chown("/tmp/test_fuse/does_not_exist", 1002, 1003);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_chownTest, ChmodFile) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chown("/tmp/test_fuse/testfile1", 1002, 1003);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_uid, 1002);
  EXPECT_EQ(tempstat.st_gid, 1003);
}
TEST_F(hfuse_chownTest, ChmodDir) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chown("/tmp/test_fuse/testdir1", 1, 1);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testdir1", &tempstat);
  EXPECT_EQ(tempstat.st_uid, 1);
  EXPECT_EQ(tempstat.st_gid, 1);
}

/* End of the test case for the function hfuse_chown */
