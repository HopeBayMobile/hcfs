#define FUSE_USE_VERSION 29

#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <utime.h>
#include <math.h>
#include <dirent.h>

#include <fuse/fuse_lowlevel.h>

extern "C" {
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "filetables.h"
#include "utils.h"
#include "super_block.h"
#include "mount_manager.h"
}
#include "gtest/gtest.h"

#include "fake_misc.h"

SYSTEM_CONF_STRUCT system_config;
extern struct fuse_lowlevel_ops hfuse_ops;
MOUNT_T unittest_mount;

static void _mount_test_fuse(MOUNT_T *tmpmount) {
  char **argv;
  int ret_val;
  struct fuse_chan *tmp_channel;
  struct fuse_session *tmp_session;
  int mt, fg;
  char *mount;

  argv = (char **) malloc(sizeof(char *)*3);
  argv[0] = (char *) malloc(sizeof(char)*100);
  argv[1] = (char *) malloc(sizeof(char)*100);
  argv[2] = (char *) malloc(sizeof(char)*100);

  snprintf(argv[0],90,"test_fuse");
  snprintf(argv[1],90,"/tmp/test_fuse");
  snprintf(argv[2],90,"-d");
  ret_val = mkdir("/tmp/test_fuse",0777);
  printf("created return %d\n",ret_val);
//  hook_fuse(3, argv);
  struct fuse_args tmp_args = FUSE_ARGS_INIT(3, argv);

  memset(tmpmount, 0, sizeof(MOUNT_T));
  tmpmount->f_ino = 1;
  sem_init(&(tmpmount->stat_lock), 0, 1);
  fuse_parse_cmdline(&tmp_args, &mount, &mt, &fg);
  tmp_channel = fuse_mount(mount, &tmp_args);
  tmp_session = fuse_lowlevel_new(&tmp_args,
			&hfuse_ops, sizeof(hfuse_ops), (void *) tmpmount);
  fuse_set_signal_handlers(tmp_session);
  fuse_session_add_chan(tmp_session, tmp_channel);
  tmpmount->session_ptr = tmp_session;
  tmpmount->chan_ptr = tmp_channel;
  tmpmount->is_unmount = FALSE;
  pthread_create(&(tmpmount->mt_thread), NULL,
			mount_multi_thread, (void *) tmpmount);
  sleep(5);
  return;
}

class fuseopEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {
    workpath = NULL;
    tmppath = NULL;
    if (access("/tmp/testHCFS", F_OK) != 0) {
      workpath = get_current_dir_name();
      tmppath = (char *)malloc(strlen(workpath)+20);
      snprintf(tmppath, strlen(workpath)+20, "%s/tmpdir", workpath);
      if (access(tmppath, F_OK) != 0)
        mkdir(tmppath, 0700);
      symlink(tmppath, "/tmp/testHCFS");
     }

    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    sem_init(&(hcfs_system->access_sem), 0, 1);
    sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));
    memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
    system_config.max_block_size = 2097152;
    system_config.cache_hard_limit = 3200000;
    system_config.cache_soft_limit = 3200000;
    system_config.current_backend = 1;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->system_going_down = FALSE;
    fail_open_files = FALSE;

    system_fh_table.entry_table_flags = (char *) malloc(sizeof(char) * 100);
    memset(system_fh_table.entry_table_flags, 0, sizeof(char) * 100);
    system_fh_table.entry_table = (FH_ENTRY *) malloc(sizeof(FH_ENTRY) * 100);
    memset(system_fh_table.entry_table, 0, sizeof(FH_ENTRY) * 100);

    _mount_test_fuse(&unittest_mount);
  }

  virtual void TearDown() {
    int ret_val, tmp_err;

    sleep(3);
    if (fork() == 0)
     execlp("fusermount","fusermount","-u","/tmp/test_fuse",(char *) NULL);
    else
     wait(NULL);
    sleep(1);
    pthread_join(unittest_mount.mt_thread, NULL);
    fuse_session_remove_chan(unittest_mount.chan_ptr);
    fuse_remove_signal_handlers(unittest_mount.session_ptr);
    fuse_session_destroy(unittest_mount.session_ptr);
    fuse_unmount(unittest_mount.f_mp, unittest_mount.chan_ptr);
    fuse_opt_free_args(&(unittest_mount.mount_args));

    ret_val = rmdir("/tmp/test_fuse");
    tmp_err = errno;
    printf("delete return %d\n",ret_val);
    free(system_fh_table.entry_table_flags);
    free(system_fh_table.entry_table);
    free(sys_super_block);
    free(hcfs_system);

    unlink("/tmp/testHCFS");
    rmdir(tmppath);
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);
  }
};

::testing::Environment* const fuseop_env = ::testing::AddGlobalTestEnvironment(new fuseopEnvironment);


/* Begin of the test case for the function hfuse_getattr */

class hfuse_getattrTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    after_update_block_page = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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
    root_updated = FALSE;
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

TEST_F(hfuse_chownTest, ChownNotRoot) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chown("/tmp/test_fuse/testfile1", 1, 1);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EPERM);
}
/* Cannot test this if not root */
/*
TEST_F(hfuse_chownTest, ChownDir) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = chown("/tmp/test_fuse/testdir1", 1, 1);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testdir1", &tempstat);
  EXPECT_EQ(tempstat.st_uid, 1);
  EXPECT_EQ(tempstat.st_gid, 1);
}*/

/* End of the test case for the function hfuse_chown */

/* Begin of the test case for the function hfuse_utimens */
class hfuse_utimensTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
  }

  virtual void TearDown() {
  }
};
TEST_F(hfuse_utimensTest, FileNotExist) {
  int ret_val;
  int tmp_err;
  struct utimbuf target_time;

  ret_val = utime("/tmp/test_fuse/does_not_exist", &target_time);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_utimensTest, UtimeTest) {
  int ret_val;
  int tmp_err;
  struct utimbuf target_time;
  struct stat tempstat;

  target_time.actime = 123456;
  target_time.modtime = 456789;
  ret_val = utime("/tmp/test_fuse/testfile1", &target_time);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_atime, 123456);
  EXPECT_EQ(tempstat.st_mtime, 456789);
#ifndef _ANDROID_ENV_
  EXPECT_EQ(tempstat.st_atim.tv_sec, 123456);
  EXPECT_EQ(tempstat.st_mtim.tv_sec, 456789);
  EXPECT_EQ(tempstat.st_atim.tv_nsec, 0);
  EXPECT_EQ(tempstat.st_mtim.tv_nsec, 0);
#endif
}
TEST_F(hfuse_utimensTest, UtimensatTest) {
  int ret_val;
  int tmp_err;
  struct timespec target_time[2];
  struct stat tempstat;

  target_time[0].tv_sec = 123456;
  target_time[1].tv_sec = 456789;
  target_time[0].tv_nsec = 2222;
  target_time[1].tv_nsec = 12345678;
  ret_val = utimensat(1, "/tmp/test_fuse/testfile1", target_time, 0);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_atime, 123456);
  EXPECT_EQ(tempstat.st_mtime, 456789);
#ifndef _ANDROID_ENV_
  EXPECT_EQ(tempstat.st_atim.tv_sec, 123456);
  EXPECT_EQ(tempstat.st_mtim.tv_sec, 456789);
  EXPECT_EQ(tempstat.st_atim.tv_nsec, 2222);
  EXPECT_EQ(tempstat.st_mtim.tv_nsec, 12345678);
#endif
}
TEST_F(hfuse_utimensTest, FutimensTest) {
  int ret_val;
  int tmp_err;
  int fd;
  struct timespec target_time[2];
  struct stat tempstat;

  target_time[0].tv_sec = 123456;
  target_time[1].tv_sec = 456789;
  target_time[0].tv_nsec = 2222;
  target_time[1].tv_nsec = 12345678;

  fd = open("/tmp/test_fuse/testfile1", O_RDWR);
  ret_val = futimens(fd, target_time);
  tmp_err = errno;

  close(fd);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_atime, 123456);
  EXPECT_EQ(tempstat.st_mtime, 456789);
#ifndef _ANDROID_ENV_
  EXPECT_EQ(tempstat.st_atim.tv_sec, 123456);
  EXPECT_EQ(tempstat.st_mtim.tv_sec, 456789);
  EXPECT_EQ(tempstat.st_atim.tv_nsec, 2222);
  EXPECT_EQ(tempstat.st_mtim.tv_nsec, 12345678);
#endif
}

/* End of the test case for the function hfuse_utimens */

/* Begin of the test case for the function hfuse_truncate */
class hfuse_truncateTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    fake_block_status = ST_NONE;
    after_update_block_page = FALSE;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
  }

  virtual void TearDown() {
    char temppath[1024];

    fetch_block_path(temppath, 14, 0);
    if (access(temppath, F_OK) == 0)
      unlink(temppath);
    fetch_trunc_path(temppath, 14);
    if (access(temppath, F_OK) == 0)
      unlink(temppath);
  }
};
TEST_F(hfuse_truncateTest, FileNotExist) {
  int ret_val;
  int tmp_err;

  ret_val = truncate("/tmp/test_fuse/does_not_exist", 100);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_truncateTest, IsNotFile) {
  int ret_val;
  int tmp_err;

  ret_val = truncate("/tmp/test_fuse/testdir1", 100);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}
TEST_F(hfuse_truncateTest, NoSizeChange) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = truncate("/tmp/test_fuse/testfile2", 1024);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}
TEST_F(hfuse_truncateTest, ExtendSize) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  ret_val = truncate("/tmp/test_fuse/testfile2", 102400);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 102400);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 + (102400 - 1024));
}
TEST_F(hfuse_truncateTest, TruncateZeroNoBlock) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  fake_block_status = ST_NONE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
}
TEST_F(hfuse_truncateTest, TruncateZeroBlockTodelete) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;

  fake_block_status = ST_TODELETE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
}
TEST_F(hfuse_truncateTest, TruncateZeroBlockLdisk) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];

  fetch_block_path(temppath, 14, 0);
  mknod(temppath, S_IFREG | 0700, makedev(0,0));
  ASSERT_EQ(access(temppath, F_OK), 0);
  fake_block_status = ST_LDISK;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
  EXPECT_NE(access(temppath, F_OK), 0);
}

TEST_F(hfuse_truncateTest, TruncateHalfLdisk) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  int fd;

  fetch_block_path(temppath, 14, 0);
  fd = creat(temppath, 0700);
  ftruncate(fd, 102400);
  close(fd);
  stat(temppath, &tempstat);
  ASSERT_EQ(tempstat.st_size, 102400);
  ASSERT_EQ(access(temppath, F_OK), 0);
  fake_block_status = ST_LDISK;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  ASSERT_EQ(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  stat(temppath, &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_blocks, 13);
}
TEST_F(hfuse_truncateTest, TruncateHalfNoblock) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  int fd;

  fetch_block_path(temppath, 14, 0);
  if (access(temppath, F_OK) == 0)
    unlink(temppath);
  fake_block_status = ST_NONE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  EXPECT_NE(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000);
  EXPECT_EQ(hcfs_system->systemdata.cache_blocks, 13);
}
TEST_F(hfuse_truncateTest, TruncateHalfCloud) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  int fd;

  fetch_block_path(temppath, 14, 0);

  fake_block_status = ST_CLOUD;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);
  tmp_err = errno;

  ASSERT_EQ(ret_val, 0);
  ASSERT_EQ(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  stat(temppath, &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000 + 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_blocks, 14);
}

/* End of the test case for the function hfuse_truncate */

/* Begin of the test case for the function hfuse_open */
class hfuse_openTest : public ::testing::Test {
 protected:
  FILE *fptr;

  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    fail_open_files = FALSE;
    fptr = NULL;
  }

  virtual void TearDown() {
    if (fptr != NULL)
      fclose(fptr);
    fail_open_files = FALSE;
  }
};
TEST_F(hfuse_openTest, FileNotExist) {
  int tmp_err;
  int ret_val;

  fptr = fopen("/tmp/test_fuse/does_not_exist", "r");
  tmp_err = errno;
  
  ret_val = 0;
  if (fptr == NULL)
    ret_val = -1;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_openTest, FailOpenFh) {
  int tmp_err;
  int ret_val;

  fail_open_files = TRUE;
  fptr = fopen("/tmp/test_fuse/testfile1", "r");
  tmp_err = errno;
  
  ret_val = 0;
  if (fptr == NULL)
    ret_val = -1;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENFILE);
}

TEST_F(hfuse_openTest, OpenFileOK) {
  int tmp_err;
  int ret_val;

  fptr = fopen("/tmp/test_fuse/testfile1", "r");
  tmp_err = errno;
  
  ret_val = 0;
  if (fptr == NULL)
    ret_val = -1;
  ASSERT_EQ(ret_val, 0);
  EXPECT_EQ(tmp_err, ENFILE);
  fclose(fptr);
  fptr = NULL;
}

/* End of the test case for the function hfuse_open */

/* Begin of the test case for the function hfuse_ll_read */
class hfuse_readTest : public ::testing::Test {
 protected:
  FILE *fptr;

  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    after_update_block_page = FALSE;
    test_fetch_from_backend = FALSE;
    fake_block_status = ST_NONE;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    fptr = NULL;
  }

  virtual void TearDown() {
    char temppath[1024];

    if (fptr != NULL)
      fclose(fptr);

    fetch_block_path(temppath, 15, 0);
    if (access(temppath, F_OK) == 0)
      unlink(temppath);
  }
};

TEST_F(hfuse_readTest, ReadZeroByte) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int fd;
  size_t ret_items;

  fetch_block_path(temppath, 15, 0);

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 0, 0, fptr);
  EXPECT_EQ(ret_items,0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadPastEnd) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int fd;
  size_t ret_items;

  fetch_block_path(temppath, 15, 0);

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 204900, SEEK_SET);
  ret_items = fread(tempbuf, 1, 1, fptr);
  EXPECT_EQ(ret_items,0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadEmptyContent) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;

  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_NONE;

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  ret_val = 0;
  for (count = 0; count < 100; count++) {
    if (tempbuf[count] != 0) {
      ret_val = 1;
      break;
    }
  }
  EXPECT_EQ(ret_val, 0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadLocalContent) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadCloudContent) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_CLOUD;
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadCloudWaitCache) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  hcfs_system->systemdata.cache_size = 5000000;

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_CLOUD;
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);

  fptr = fopen("/tmp/test_fuse/testread", "r");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);
  fclose(fptr);
  fptr = NULL;
}

/* End of the test case for the function hfuse_read */

/* Begin of the test case for the function hfuse_ll_write */
class hfuse_ll_writeTest : public ::testing::Test {
 protected:
  FILE *fptr;

  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    after_update_block_page = FALSE;
    test_fetch_from_backend = FALSE;
    fake_block_status = ST_NONE;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    fptr = NULL;
  }

  virtual void TearDown() {
    char temppath[1024];

    if (fptr != NULL)
      fclose(fptr);

    fetch_block_path(temppath, 16, 0);
    if (access(temppath, F_OK) == 0)
      unlink(temppath);
  }
};

TEST_F(hfuse_ll_writeTest, WriteZeroByte) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int fd;
  size_t ret_items;

  fetch_block_path(temppath, 16, 0);

  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fwrite(tempbuf, 0, 0, fptr);
  EXPECT_EQ(ret_items, 0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_ll_writeTest, WritePastEnd) {
  int ret_val;
  int tmp_err, tmp_len;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int fd;
  size_t ret_items;

  fetch_block_path(temppath, 16, 0);

  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  snprintf(tempbuf, 10, "test");
  fseek(fptr, 204900, SEEK_SET);
  tmp_len = strlen(tempbuf)+1;
  ret_items = fwrite(tempbuf, tmp_len, 1, fptr);
  EXPECT_EQ(ret_items,1);
  fclose(fptr);
  fptr = NULL;
  fptr = fopen(temppath, "r");
  fseek(fptr, 204900, SEEK_SET);
  ret_items = fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(0, strcmp(tempbuf, "test"));
}

TEST_F(hfuse_ll_writeTest, ReWriteLocalContent) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0);
}

TEST_F(hfuse_ll_writeTest, ReWriteCloudContent) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_CLOUD;
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);

  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0);
}

TEST_F(hfuse_ll_writeTest, ReWriteCloudWaitCache) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;

  hcfs_system->systemdata.cache_size = 5000000;

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_CLOUD;
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);

  ASSERT_EQ(hcfs_system->systemdata.cache_size, 5000000);
  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  ASSERT_NE(fptr != NULL, 0);
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0);
}

/* End of the test case for the function hfuse_ll_write */

/* Begin of the test case for the function hfuse_ll_statfs */
class hfuse_ll_statfsTest : public ::testing::Test {
 protected:

  virtual void SetUp() {
    unittest_mount.FS_stat.system_size = 12800000;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    sys_super_block->head.num_active_inodes = 10000;
    unittest_mount.FS_stat.num_inodes = 10000;
    before_update_file_data = TRUE;
    root_updated = FALSE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_ll_statfsTest, SmallSysStat) {

  struct statfs tmpstat;
  int ret_val;
  int ret;

  if (sys_super_block == NULL)
    ret = 1;
  else
    ret = 0;
  ASSERT_EQ(0, ret);
  if (hcfs_system == NULL)
    ret = 1;
  else
    ret = 0;
  ASSERT_EQ(0, ret);

  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(25*powl(1024,2), tmpstat.f_blocks);
  EXPECT_EQ(25*powl(1024,2) - (((12800000 - 1) / 4096) + 1), tmpstat.f_bfree);
  EXPECT_EQ(25*powl(1024,2) - (((12800000 - 1) / 4096) + 1), tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000 - 10000, tmpstat.f_ffree);
}
TEST_F(hfuse_ll_statfsTest, EmptySysStat) {

  struct statfs tmpstat;
  int ret_val;

  hcfs_system->systemdata.system_size = 0;
  hcfs_system->systemdata.cache_size = 0;
  hcfs_system->systemdata.cache_blocks = 0;
  sys_super_block->head.num_active_inodes = 0;
  unittest_mount.FS_stat.system_size = 0;
  unittest_mount.FS_stat.num_inodes = 0;

  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(25*powl(1024,2), tmpstat.f_blocks);
  EXPECT_EQ(25*powl(1024,2), tmpstat.f_bfree);
  EXPECT_EQ(25*powl(1024,2), tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000, tmpstat.f_ffree);
}

TEST_F(hfuse_ll_statfsTest, BorderStat) {

  struct statfs tmpstat;
  int ret_val;

  hcfs_system->systemdata.system_size = 4096;
  hcfs_system->systemdata.cache_size = 0;
  hcfs_system->systemdata.cache_blocks = 0;
  unittest_mount.FS_stat.system_size = 4096;

  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(25*powl(1024,2), tmpstat.f_blocks);
  EXPECT_EQ(25*powl(1024,2) - 1, tmpstat.f_bfree);
  EXPECT_EQ(25*powl(1024,2) - 1, tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000 - 10000, tmpstat.f_ffree);
}

TEST_F(hfuse_ll_statfsTest, LargeSysStat) {

  struct statfs tmpstat;
  int ret_val;
  long long sys_blocks;

  hcfs_system->systemdata.system_size = 50*powl(1024,3) + 1;
  sys_super_block->head.num_active_inodes = 2000000;
  unittest_mount.FS_stat.system_size = 50*powl(1024,3) + 1;
  unittest_mount.FS_stat.num_inodes = 2000000;

  sys_blocks = ((50*powl(1024,3) + 1 - 1) / 4096) + 1;
  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(2 * sys_blocks, tmpstat.f_blocks);
  EXPECT_EQ(sys_blocks, tmpstat.f_bfree);
  EXPECT_EQ(sys_blocks, tmpstat.f_bavail);
  EXPECT_EQ(4000000, tmpstat.f_files);
  EXPECT_EQ(2000000, tmpstat.f_ffree);
}

/* End of the test case for the function hfuse_ll_statfs */

/* Begin of the test case for the function hfuse_ll_readdir */
class hfuse_ll_readdirTest : public ::testing::Test {
 protected:
  FILE *fptr;

  virtual void SetUp() {
    snprintf(readdir_metapath, 100, "/tmp/readdir_meta");
    before_update_file_data = TRUE;
    root_updated = FALSE;
  }

  virtual void TearDown() {
    if (access(readdir_metapath, F_OK) == 0)
      unlink(readdir_metapath);
  }
};

TEST_F(hfuse_ll_readdirTest, NoEntry) {

  DIR_META_TYPE temphead;
  DIR_ENTRY_PAGE temppage;
  DIR *dptr;
  struct dirent tmp_dirent, *tmp_dirptr;
  int ret_val;
  struct stat tempstat;

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(struct stat), 1, fptr);
  temphead.total_children = 0;
  temphead.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(struct stat) + sizeof(DIR_META_TYPE), ftell(fptr));
  memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries = 2;
  temppage.this_page_pos = temphead.root_entry_page;
  temppage.dir_entries[0].d_ino = 17;
  snprintf(temppage.dir_entries[0].d_name, 200, ".");
  temppage.dir_entries[0].d_type = D_ISDIR;
  temppage.dir_entries[1].d_ino = 1;
  snprintf(temppage.dir_entries[1].d_name, 200, "..");
  temppage.dir_entries[1].d_type = D_ISDIR;
  fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
  fclose(fptr);

  dptr = opendir("/tmp/test_fuse/testlistdir");
  ASSERT_NE(0, dptr != NULL);
  ret_val = readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 17);  
  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 1);  
  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, tmp_dirptr != NULL);
  closedir(dptr);
}

TEST_F(hfuse_ll_readdirTest, SingleEntry) {

  DIR_META_TYPE temphead;
  DIR_ENTRY_PAGE temppage;
  DIR *dptr;
  struct dirent tmp_dirent, *tmp_dirptr;
  int ret_val;
  struct stat tempstat;

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(struct stat), 1, fptr);
  temphead.total_children = 1;
  temphead.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(struct stat) + sizeof(DIR_META_TYPE), ftell(fptr));
  memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries = 3;
  temppage.this_page_pos = temphead.root_entry_page;
  temppage.dir_entries[0].d_ino = 17;
  snprintf(temppage.dir_entries[0].d_name, 200, ".");
  temppage.dir_entries[0].d_type = D_ISDIR;
  temppage.dir_entries[1].d_ino = 1;
  snprintf(temppage.dir_entries[1].d_name, 200, "..");
  temppage.dir_entries[1].d_type = D_ISDIR;

  temppage.dir_entries[2].d_ino = 18;
  snprintf(temppage.dir_entries[2].d_name, 200, "test1");
  temppage.dir_entries[2].d_type = D_ISREG;

  fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
  fclose(fptr);

  dptr = opendir("/tmp/test_fuse/testlistdir");
  ASSERT_NE(0, dptr != NULL);
  ret_val = readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 17);  
  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 1);  

  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 18);
  EXPECT_EQ(0, strcmp(tmp_dirent.d_name, "test1"));

  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, tmp_dirptr != NULL);
  closedir(dptr);
}

TEST_F(hfuse_ll_readdirTest, OneMaxPageEntries) {

  DIR_META_TYPE temphead;
  DIR_ENTRY_PAGE temppage;
  DIR *dptr;
  struct dirent tmp_dirent, *tmp_dirptr;
  int ret_val, count;
  struct stat tempstat;
  char filename[100];

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(struct stat), 1, fptr);
  temphead.total_children = MAX_DIR_ENTRIES_PER_PAGE - 2;
  temphead.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(struct stat) + sizeof(DIR_META_TYPE), ftell(fptr));
  memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries = MAX_DIR_ENTRIES_PER_PAGE;
  temppage.this_page_pos = temphead.root_entry_page;
  temppage.dir_entries[0].d_ino = 17;
  snprintf(temppage.dir_entries[0].d_name, 200, ".");
  temppage.dir_entries[0].d_type = D_ISDIR;
  temppage.dir_entries[1].d_ino = 1;
  snprintf(temppage.dir_entries[1].d_name, 200, "..");
  temppage.dir_entries[1].d_type = D_ISDIR;

  for (count = 2; count < MAX_DIR_ENTRIES_PER_PAGE; count++) {
    temppage.dir_entries[count].d_ino = 16 + count;
    snprintf(temppage.dir_entries[count].d_name, 200, "test%d", count - 1);
    temppage.dir_entries[count].d_type = D_ISREG;
  }

  fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
  fclose(fptr);

  dptr = opendir("/tmp/test_fuse/testlistdir");
  ASSERT_NE(0, dptr != NULL);
  ret_val = readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 17);  
  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 1);  

  for (count = 2; count < MAX_DIR_ENTRIES_PER_PAGE; count++) {
    readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
    ASSERT_NE(0, tmp_dirptr != NULL);
    EXPECT_EQ(tmp_dirent.d_ino, 16 + count);
    snprintf(filename, 100, "test%d", count - 1);
    EXPECT_EQ(0, strcmp(tmp_dirent.d_name, filename));
  }

  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, tmp_dirptr != NULL);
  closedir(dptr);
}

TEST_F(hfuse_ll_readdirTest, TwoMaxPageEntries) {
/* Note: this won't happen in actual b-tree */
  DIR_META_TYPE temphead;
  DIR_ENTRY_PAGE temppage;
  DIR *dptr;
  struct dirent tmp_dirent, *tmp_dirptr;
  int ret_val, count;
  struct stat tempstat;
  char filename[100];

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(struct stat), 1, fptr);
  temphead.total_children = (2 * MAX_DIR_ENTRIES_PER_PAGE) - 2;
  temphead.root_entry_page = sizeof(struct stat) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(struct stat) + sizeof(DIR_META_TYPE), ftell(fptr));
  memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries = MAX_DIR_ENTRIES_PER_PAGE;
  temppage.this_page_pos = temphead.root_entry_page;
  temppage.child_page_pos[0] = temphead.root_entry_page
				+ sizeof(DIR_ENTRY_PAGE);
  temppage.tree_walk_next = temppage.child_page_pos[0];
  temppage.dir_entries[0].d_ino = 17;
  snprintf(temppage.dir_entries[0].d_name, 200, ".");
  temppage.dir_entries[0].d_type = D_ISDIR;
  temppage.dir_entries[1].d_ino = 1;
  snprintf(temppage.dir_entries[1].d_name, 200, "..");
  temppage.dir_entries[1].d_type = D_ISDIR;

  for (count = 2; count < MAX_DIR_ENTRIES_PER_PAGE; count++) {
    temppage.dir_entries[count].d_ino = 16 + count;
    snprintf(temppage.dir_entries[count].d_name, 200, "test%d", count - 1);
    temppage.dir_entries[count].d_type = D_ISREG;
  }

  fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);

// Second page
  memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));
  temppage.num_entries = MAX_DIR_ENTRIES_PER_PAGE;
  temppage.this_page_pos = temphead.root_entry_page
				+ sizeof(DIR_ENTRY_PAGE);
  temppage.tree_walk_prev = temphead.root_entry_page;
  for (count = 0; count < MAX_DIR_ENTRIES_PER_PAGE; count++) {
    temppage.dir_entries[count].d_ino = 16 + count + MAX_DIR_ENTRIES_PER_PAGE;
    snprintf(temppage.dir_entries[count].d_name, 200, "test%d",
		MAX_DIR_ENTRIES_PER_PAGE + count - 1);
    temppage.dir_entries[count].d_type = D_ISREG;
  }

  fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1, fptr);
  fclose(fptr);

  dptr = opendir("/tmp/test_fuse/testlistdir");
  ASSERT_NE(0, dptr != NULL);
  ret_val = readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, ret_val);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 17);  
  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_NE(0, tmp_dirptr != NULL);
  EXPECT_EQ(tmp_dirent.d_ino, 1);  

  for (count = 2; count < (2 * MAX_DIR_ENTRIES_PER_PAGE); count++) {
    readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
    ASSERT_NE(0, tmp_dirptr != NULL);
    EXPECT_EQ(tmp_dirent.d_ino, 16 + count);
    snprintf(filename, 100, "test%d", count - 1);
    EXPECT_EQ(0, strcmp(tmp_dirent.d_name, filename));
  }

  readdir_r(dptr, &tmp_dirent, &tmp_dirptr);
  ASSERT_EQ(0, tmp_dirptr != NULL);
  closedir(dptr);
}

/* End of the test case for the function hfuse_ll_readdir */

#ifndef _ANDROID_ENV_
/* 
	Unittest of hfuse_ll_setxattr()
 */
class hfuse_ll_setxattrTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_setxattrTest, SetKeyWithoutValue)
{
	int ret;
	int errcode;

	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", "", 0, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errcode);
}

TEST_F(hfuse_ll_setxattrTest, NamespaceInvalid)
{
	int ret;
	int errcode;

	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_setxattrTest, PermissionDenied)
{
	int ret;
	int errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_setxattrTest, InsertXattrReturnFail)
{
	int ret;
	int errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_setxattrTest, InsertXattrSuccess)
{
	int ret;
	int errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", "123", 3, 0);

	EXPECT_EQ(0, ret);
}
/*
	End of unittest of hfuse_ll_setxattr()
 */

/* 
	Unittest of hfuse_ll_getxattr()
 */
class hfuse_ll_getxattrTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_getxattrTest, NamespaceInvalid)
{
	int ret;
	int errcode;
	char buf[10];

	ret = getxattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa", buf, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_getxattrTest, PermissionDenied)
{
	int ret;
	int errcode;
	char buf[10];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa", buf, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_getxattrTest, GetCorrectValueSizeSuccess)
{
	int ret;
	int errcode;
	char buf[10];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", buf, 0);

	EXPECT_EQ(CORRECT_VALUE_SIZE, ret);
}

TEST_F(hfuse_ll_getxattrTest, GetValueFail)
{
	int ret;
	int errcode;
	char buf[100];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa", buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_getxattrTest, GetValueSuccess)
{
	int ret;
	int errcode;
	char buf[100];
	const char *ans = "hello!getxattr:)";
	
	ret = getxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", buf, 100);
	buf[ret] = '\0';

	EXPECT_EQ(strlen(ans), ret);
	EXPECT_STREQ(ans, buf);
}
/* 
	End of unittest of hfuse_ll_getxattr()
 */

/* 
	Unittest of hfuse_ll_listxattr()
 */
class hfuse_ll_listxattrTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_listxattrTest, GetCorrectValueSizeSuccess)
{
	int ret;
	int errcode;
	char buf[10];
	
	ret = listxattr("/tmp/test_fuse/testsetxattr", 
		buf, 0);

	EXPECT_EQ(CORRECT_VALUE_SIZE, ret);
}

TEST_F(hfuse_ll_listxattrTest, GetValueFail)
{
	int ret;
	int errcode;
	char buf[100];
	
	ret = listxattr("/tmp/test_fuse/testsetxattr_fail", 
		buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_listxattrTest, GetValueSuccess)
{
	int ret;
	int errcode;
	char buf[100];
	const char *ans = "hello!listxattr:)";
	
	ret = listxattr("/tmp/test_fuse/testsetxattr", 
		buf, 100);
	buf[ret] = '\0';

	EXPECT_EQ(strlen(ans), ret);
	EXPECT_STREQ(ans, buf);
}

/* 
	End of unittest of hfuse_ll_listxattr()
 */

/* 
	Unittest of hfuse_ll_removexattr()
 */
class hfuse_ll_removexattrTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_removexattrTest, NamespaceInvalid)
{
	int ret;
	int errcode;

	ret = removexattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_removexattrTest, PermissionDenied)
{
	int ret;
	int errcode;
	
	ret = removexattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_removexattrTest, RemoveXattrReturnFail)
{
	int ret;
	int errcode;
	
	ret = removexattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_removexattrTest, RemoveXattrSuccess)
{
	int ret;

	ret = removexattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa");

	EXPECT_EQ(0, ret);
}
/*
	End of unittest of hfuse_ll_removexattr()
 */

#endif
/*
	Unittest of hfuse_ll_symlink()
 */
class hfuse_ll_symlinkTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_symlinkTest, FileExists)
{
	int ret;
	int errcode;

	errcode = 0;
	ret = symlink("name_not_used", "/tmp/test_fuse/testsymlink");
	errcode = errno;	

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_symlinkTest, SelfNameTooLong)
{
	int ret;
	int errcode;
	char selfname[MAX_FILENAME_LEN + 50];

	/* Mock path "/tmp/test_fuse/aaaaaaaaaaa...." */
	errcode = 0;
	memset(selfname, 0, MAX_FILENAME_LEN + 50);
	memset(selfname, 'a', MAX_FILENAME_LEN + 40);
	memcpy(selfname, "/tmp/test_fuse/", 15);

	ret = symlink("haha", selfname);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENAMETOOLONG, errcode);
}

TEST_F(hfuse_ll_symlinkTest, LinkPathTooLong)
{
	int ret;
	int errcode;
	char link_path[MAX_LINK_PATH + 1];

	/* Mock path "/tmp/test_fuse/aaaaaaaaaaa...." */
	errcode = 0;
	memset(link_path, 0, MAX_LINK_PATH + 1);
	memset(link_path, 'a', MAX_LINK_PATH);

	ret = symlink(link_path, "/tmp/test_fuse/selfname_not_exists");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENAMETOOLONG, errcode);
}

TEST_F(hfuse_ll_symlinkTest, FileExistInSymlink)
{
	int ret;
	int errcode;

	errcode = 0;

	ret = symlink("not_used", 
		"/tmp/test_fuse/testsymlink_exist_in_symlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_symlinkTest, UpdateMetaFail)
{
	int ret;
	int errcode;

	errcode = 0;

	ret = symlink("update_meta_fail", 
		"/tmp/test_fuse/testsymlink_not_exist_in_symlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(1, errcode);
}

TEST_F(hfuse_ll_symlinkTest, SymlinkSuccess)
{
	int ret;

	ret = symlink("update_meta_success", 
		"/tmp/test_fuse/testsymlink_not_exist_in_symlink");

	EXPECT_EQ(0, ret);
}
/*
	End of unittest of hfuse_ll_symlink()
 */

/*
	Unittest of hfuse_ll_readlink()
 */
class hfuse_ll_readlinkTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_readlinkTest, FileNotExist)
{
	char buf[100];
	int ret;
	int errcode;

	ret = readlink("/tmp/test_fuse/test_readlink_not_exist", buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOENT, errcode);
}

TEST_F(hfuse_ll_readlinkTest, ReadLinkSuccess)
{
	char buf[100];
	char *ans = "I_am_target_link";
	int ret;
	int errcode;

	ret = readlink("/tmp/test_fuse/testsymlink", buf, 100);
	buf[ret] = '\0';

	EXPECT_EQ(strlen(ans), ret);
	EXPECT_STREQ(ans, buf);
}
/*
	End of unittest of hfuse_ll_readlink()
 */

/*
	Unittest of hfuse_ll_link()
 */
class hfuse_ll_linkTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_linkTest, OldlinkNotExists)
{
	int ret;
	int errcode;

	ret = link("/tmp/test_fuse/old_link_not_exists",
		"/tmp/test_fuse/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOENT, errcode);

}

TEST_F(hfuse_ll_linkTest, NewlinkExists)
{
	int ret;
	int errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_linkTest, NewlinkNameTooLong)
{
	int ret;
	int errcode;
	char linkname[MAX_FILENAME_LEN + 50];

	/* Mock path "/tmp/test_fuse/aaaaaaaaaaa...." */
	errcode = 0;
	memset(linkname, 0, MAX_FILENAME_LEN + 50);
	memset(linkname, 'a', MAX_FILENAME_LEN + 40);
	memcpy(linkname, "/tmp/test_fuse/", 15);

	ret = link("/tmp/test_fuse/testlink", linkname);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENAMETOOLONG, errcode);
}

TEST_F(hfuse_ll_linkTest, ParentDirPermissionDenied)
{
	int ret;
	int errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testlink_dir_perm_denied/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);

}

TEST_F(hfuse_ll_linkTest, ParentIsNotDir)
{
	int ret;
	int errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testfile/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOTDIR, errcode);

}

TEST_F(hfuse_ll_linkTest, link_update_metaFail)
{
	int ret;
	int errcode;
	char hardlink[500] = "/tmp/test_fuse/new_link_update_meta_fail";

	ret = link("/tmp/test_fuse/testlink", hardlink);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(123, errcode);
}

TEST_F(hfuse_ll_linkTest, LinkSuccess)
{
	int ret;
	char hardlink[500] = "/tmp/test_fuse/xxxxxxx";

	ret = link("/tmp/test_fuse/testlink", hardlink);

	EXPECT_EQ(0, ret);
}

/*
	End of unittest of hfuse_ll_link()
 */

/*
	Unittest of hfuse_ll_create()
 */
class hfuse_ll_createTest : public ::testing::Test {
protected:
	int fd;

	void SetUp()
	{
	}

	void TearDown()
	{
		if (fd > 0)
			close(fd);
	}
};

TEST_F(hfuse_ll_createTest, NameTooLong)
{
	char name[MAX_FILENAME_LEN + 100];
	int errcode;

	memset(name, 0, MAX_FILENAME_LEN + 100);
	memset(name, 'a', MAX_FILENAME_LEN + 90);
	memcpy(name, "/tmp/test_fuse/", 15);

	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENAMETOOLONG, errcode);
}

TEST_F(hfuse_ll_createTest, ParentIsNotDir)
{
	char *name = "/tmp/test_fuse/testfile/creat_test";
	int errcode;


	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENOTDIR, errcode);
}

TEST_F(hfuse_ll_createTest, ParentPermissionDenied)
{
	char *name = "/tmp/test_fuse/testlink_dir_perm_denied/creat_test";
	int errcode;

	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_createTest, super_block_new_inodeFail)
{
	char *name = "/tmp/test_fuse/creat_test";
	int errcode;

	fail_super_block_new_inode = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENOSPC, errcode);
	
	fail_super_block_new_inode = FALSE;
}

TEST_F(hfuse_ll_createTest, mknod_update_metaFail)
{
	char *name = "/tmp/test_fuse/creat_test";
	int errcode;

	fail_mknod_update_meta = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(1, errcode);
	fail_mknod_update_meta = FALSE;
}

TEST_F(hfuse_ll_createTest, open_fhFail)
{
	char *name = "/tmp/test_fuse/creat_test";
	int errcode;

	fail_open_files = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENFILE, errcode);
	fail_open_files = FALSE;
}

TEST_F(hfuse_ll_createTest, CreateSuccess)
{
	char *name = "/tmp/test_fuse/creat_test";
	int errcode;

	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_GT(fd, 0);
}
/*
	End of unittest of hfuse_ll_create()
 */
