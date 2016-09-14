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
#include <ftw.h>

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

SYSTEM_CONF_STRUCT *system_config;
extern struct fuse_lowlevel_ops hfuse_ops;
MOUNT_T unittest_mount;
MOUNT_T_GLOBAL mount_global;

static int do_delete(const char *fpath,
		     const struct stat *sb,
		     int32_t tflag,
		     struct FTW *ftwbuf)
{
	switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir (fpath);
			break;
		default:
			unlink (fpath);
			break;
	}
	return (0);
}

static void _mount_test_fuse(MOUNT_T *tmpmount) {
  char **argv;
  int32_t ret_val;
  struct fuse_chan *tmp_channel;
  struct fuse_session *tmp_session;
  int32_t mt, fg;
  char *mount;

  argv = (char **) malloc(sizeof(char *)*3);
  argv[0] = (char *) malloc(sizeof(char)*100);
  argv[1] = (char *) malloc(sizeof(char)*100);
  argv[2] = (char *) malloc(sizeof(char)*100);

  snprintf(argv[0],90,"test_fuse");
  snprintf(argv[1],90,"/tmp/test_fuse");
  snprintf(argv[2],90,"-d");
  ret_val = mkdir("/tmp/test_fuse",0777);
  printf("create /tmp/test_fuse return %d\n", ret_val);
  //  hook_fuse(3, argv);
  struct fuse_args tmp_args = FUSE_ARGS_INIT(3, argv);

  memset(tmpmount, 0, sizeof(MOUNT_T));
  tmpmount->stat_lock = (sem_t *)malloc(sizeof(sem_t));
  tmpmount->FS_stat = (FS_STAT_T *) malloc(sizeof(FS_STAT_T));
  tmpmount->f_ino = 1;
  sem_init((tmpmount->stat_lock), 0, 1);
  fuse_parse_cmdline(&tmp_args, &mount, &mt, &fg);
  tmp_channel = fuse_mount(mount, &tmp_args);
  ASSERT_NE(tmp_channel, NULL);
  tmp_session = fuse_lowlevel_new(&tmp_args,
			&hfuse_ops, sizeof(hfuse_ops), (void *) tmpmount);
  fuse_set_signal_handlers(tmp_session);
  fuse_session_add_chan(tmp_session, tmp_channel);
  tmpmount->session_ptr = tmp_session;
  tmpmount->chan_ptr = tmp_channel;
  tmpmount->is_unmount = FALSE;
  pthread_create(&(tmpmount->mt_thread), NULL,
			mount_multi_thread, (void *) tmpmount);

  free(argv[0]); free(argv[1]); free(argv[2]);
  free(argv);
  return;
}

class fuseopEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {
    workpath = NULL;
    tmppath = NULL;
    system("for i in `find /sys/fs/fuse/connections -name abort`; do echo 1 > $i;done");
    system("fusermount -u /tmp/test_fuse");
    if (access("/tmp/testHCFS", F_OK) != 0) {
      workpath = get_current_dir_name();
      tmppath = (char *)malloc(strlen(workpath)+20);
      snprintf(tmppath, strlen(workpath)+20, "%s/tmpdir", workpath);
      if (access(tmppath, F_OK) != 0)
        mkdir(tmppath, 0700);
      symlink(tmppath, "/tmp/testHCFS");
     }

    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
    system_config->max_cache_limit =
        (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
    system_config->max_pinned_limit =
        (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    sem_init(&(hcfs_system->access_sem), 0, 1);
    sys_super_block = (SUPER_BLOCK_CONTROL *)
				malloc(sizeof(SUPER_BLOCK_CONTROL));
    memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
    system_config->max_block_size = 2097152;
    system_config->cache_hard_limit = 3200000;
    system_config->cache_soft_limit = 3200000;
    system_config->max_cache_limit[0] = 3200000; /* cache thresholds */
    system_config->max_pinned_limit[0] = 3200000 * 0.8;
    system_config->max_cache_limit[1] = 3200000;
    system_config->max_pinned_limit[1] = 3200000 * 0.8;
    system_config->max_cache_limit[2] = 3200000;
    system_config->max_pinned_limit[2] = 3200000 * 0.8;
    system_config->current_backend = 1;
    system_config->meta_space_limit = 1000000;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.system_meta_size = 0;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.pinned_size = 0;
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;
    fail_open_files = FALSE;

    system_fh_table.entry_table_flags = (uint8_t *) malloc(sizeof(char) * 100);
    memset(system_fh_table.entry_table_flags, 0, sizeof(char) * 100);
    system_fh_table.entry_table = (FH_ENTRY *) malloc(sizeof(FH_ENTRY) * 100);
    memset(system_fh_table.entry_table, 0, sizeof(FH_ENTRY) * 100);
    system_fh_table.direntry_table = (DIRH_ENTRY *) malloc(sizeof(DIRH_ENTRY) * 100);
    memset(system_fh_table.direntry_table, 0, sizeof(DIRH_ENTRY) * 100);
    sem_init(&(pathlookup_data_lock), 0, 1);

    _mount_test_fuse(&unittest_mount);
  }

  virtual void TearDown() {
    int32_t ret_val;
    int32_t exit_status;
    int32_t i;

    for (i = 0; i < 10; i++) {
	    puts("unmount fuse...");
	    exit_status = system("fusermount -u /tmp/test_fuse");
	    if (exit_status == 0)
		    break;
	    system("for i in `\\ls /sys/fs/fuse/connections/`; do echo 1 > /sys/fs/fuse/connections/$i/abort; done");
    }
    puts("unmount fuse... done");
    ret_val = nftw("/tmp/test_fuse", do_delete, 20, FTW_DEPTH);
    printf("delete return %d\n", ret_val);
    ASSERT_EQ(exit_status, 0);
    pthread_join(unittest_mount.mt_thread, NULL);
    fuse_session_remove_chan(unittest_mount.chan_ptr);
    fuse_remove_signal_handlers(unittest_mount.session_ptr);
    fuse_session_destroy(unittest_mount.session_ptr);
    fuse_unmount(unittest_mount.f_mp, unittest_mount.chan_ptr);
    fuse_opt_free_args(&(unittest_mount.mount_args));

    free(system_fh_table.entry_table_flags);
    free(system_fh_table.entry_table);
    free(system_fh_table.direntry_table);
    free(sys_super_block);
    free(hcfs_system);
    free(system_config->max_cache_limit);
    free(system_config->max_pinned_limit);
    free(system_config);

    unlink("/tmp/testHCFS");
    if (tmppath != NULL)
      nftw (tmppath, do_delete, 20, FTW_DEPTH);
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);

    free(unittest_mount.FS_stat);
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = access("/tmp/test_fuse/does_not_exist",F_OK);
  tmp_err = errno;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_getattrTest, TestRoot) {
  int32_t ret_val;
  struct stat tempstat; /* raw file ops */

  ret_val = stat("/tmp/test_fuse", &tempstat);
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = mknod("/tmp/test_fuse/does_not_exist/test", 0700, tmp_dev);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_mknodTest, ParentNotDir) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = mknod("/tmp/test_fuse/testfile/test", 0700, tmp_dev);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_mknodTest, SuperBlockError) {
  int32_t ret_val;
  int32_t tmp_err;

  fail_super_block_new_inode = TRUE;
  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
}
TEST_F(hfuse_mknodTest, MknodUpdateError) {
  int32_t ret_val;
  int32_t tmp_err;

  fail_mknod_update_meta = TRUE;
  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, 1);
}
TEST_F(hfuse_mknodTest, MknodOK) {
  int32_t ret_val;

  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);

  EXPECT_EQ(ret_val, 0);
}

TEST_F(hfuse_mknodTest, Mknod_NoSpace) {
  int32_t ret_val;
  int32_t tmp_err;

  hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;

  ret_val = mknod("/tmp/test_fuse/testcreate", 0700, tmp_dev);
  tmp_err = errno;

  EXPECT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
  hcfs_system->systemdata.system_meta_size = 0;
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = mkdir("/tmp/test_fuse/does_not_exist/test", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_mkdirTest, ParentNotDir) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = mkdir("/tmp/test_fuse/testfile/test", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_mkdirTest, SuperBlockError) {
  int32_t ret_val;
  int32_t tmp_err;

  fail_super_block_new_inode = TRUE;
  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
}
TEST_F(hfuse_mkdirTest, MkdirUpdateError) {
  int32_t ret_val;
  int32_t tmp_err;

  fail_mkdir_update_meta = TRUE;
  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  if (ret_val < 0)
    printf("%s\n",strerror(tmp_err));
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, 1);
}
TEST_F(hfuse_mkdirTest, MkdirOK) {
  int32_t ret_val;

  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);

  EXPECT_EQ(ret_val, 0);
}

TEST_F(hfuse_mkdirTest, Mkdir_NoSpace) {
  int32_t ret_val;
  int32_t tmp_err;

  hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;
  ret_val = mkdir("/tmp/test_fuse/testmkdir", 0700);
  tmp_err = errno;

  EXPECT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOSPC);
  hcfs_system->systemdata.system_meta_size = 0;
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = unlink("/tmp/test_fuse/does_not_exist");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_unlinkTest, ParentNotExist) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = unlink("/tmp/test_fuse/does_not_exist/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_unlinkTest, NotRegFile) {
  int32_t ret_val;
  int32_t tmp_err;

  before_mkdir_created = FALSE;
  ret_val = unlink("/tmp/test_fuse/testmkdir");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}

TEST_F(hfuse_unlinkTest, PathNotDir) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = unlink("/tmp/test_fuse/testfile/afile");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_unlinkTest, DeleteSuccess) {
  int32_t ret_val;
  int32_t tmp_err;

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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rmdir("/tmp/test_fuse/does_not_exist");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_rmdirTest, ParentNotExist) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rmdir("/tmp/test_fuse/does_not_exist/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_rmdirTest, NotDir) {
  int32_t ret_val;
  int32_t tmp_err;

  before_mknod_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testcreate");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_rmdirTest, PathNotDir) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rmdir("/tmp/test_fuse/testfile/adir");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}

TEST_F(hfuse_rmdirTest, DeleteSelf) {
  int32_t ret_val;
  int32_t tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testmkdir/.");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EINVAL);
}

TEST_F(hfuse_rmdirTest, DeleteParent) {
  int32_t ret_val;
  int32_t tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rmdir("/tmp/test_fuse/testmkdir/..");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTEMPTY);
}

TEST_F(hfuse_rmdirTest, DeleteSuccess) {
  int32_t ret_val;
  int32_t tmp_err;

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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/does_not_exist", "/tmp/test_fuse/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_renameTest, PrefixCheck) {
  int32_t ret_val;
  int32_t tmp_err;

  before_mkdir_created = FALSE;
  ret_val = rename("/tmp/test_fuse/testmkdir", "/tmp/test_fuse/testmkdir/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EINVAL);
}

TEST_F(hfuse_renameTest, Parent1NotExist) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/does_not_exist/test2", "/tmp/test_fuse/test");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_renameTest, Parent2NotExist) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile",
			"/tmp/test_fuse/does_not_exist/test2");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_renameTest, SameFile) {
  int32_t ret_val;

  ret_val = rename("/tmp/test_fuse/testfile",
			"/tmp/test_fuse/testsamefile");

  EXPECT_EQ(ret_val, 0);
}
TEST_F(hfuse_renameTest, SelfDirTargetFile) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/testdir1",
			"/tmp/test_fuse/testfile1");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTDIR);
}
TEST_F(hfuse_renameTest, SelfFileTargetDir) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/testfile2",
			"/tmp/test_fuse/testdir2");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}
TEST_F(hfuse_renameTest, TargetDirNotEmpty) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = rename("/tmp/test_fuse/testdir1/",
			"/tmp/test_fuse/testdir2/");
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOTEMPTY);
}
TEST_F(hfuse_renameTest, RenameFile) {
  int32_t ret_val;

  ret_val = rename("/tmp/test_fuse/testfile1",
			"/tmp/test_fuse/testfile2");
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = chmod("/tmp/test_fuse/does_not_exist", 0700);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_chmodTest, ChmodFile) {
  int32_t ret_val;
  struct stat tempstat; /* raw ops*/

  ret_val = chmod("/tmp/test_fuse/testfile1", 0444);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile1", &tempstat);
  EXPECT_EQ(tempstat.st_mode, S_IFREG | 0444);
}
TEST_F(hfuse_chmodTest, ChmodDir) {
  int32_t ret_val;
  struct stat tempstat; /* raw ops */

  ret_val = chmod("/tmp/test_fuse/testdir1", 0550);

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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = chown("/tmp/test_fuse/does_not_exist", 1002, 1003);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_chownTest, ChownNotRoot) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = chown("/tmp/test_fuse/testfile1", 1, 1);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EPERM);
}
/* Cannot test this if not root */
/*
TEST_F(hfuse_chownTest, ChownDir) {
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;

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
  int32_t ret_val;
  int32_t tmp_err;
  struct utimbuf target_time;

  ret_val = utime("/tmp/test_fuse/does_not_exist", &target_time);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_utimensTest, UtimeTest) {
  int32_t ret_val;
  struct utimbuf target_time;
  struct stat tempstat; /* raw file ops */

  target_time.actime = 123456;
  target_time.modtime = 456789;
  ret_val = utime("/tmp/test_fuse/testfile1", &target_time);

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
  int32_t ret_val;
  struct timespec target_time[2];
  struct stat tempstat;

  target_time[0].tv_sec = 123456;
  target_time[1].tv_sec = 456789;
  target_time[0].tv_nsec = 2222;
  target_time[1].tv_nsec = 12345678;
  ret_val = utimensat(1, "/tmp/test_fuse/testfile1", target_time, 0);

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
  int32_t ret_val;
  int32_t fd;
  struct timespec target_time[2];
  struct stat tempstat;

  target_time[0].tv_sec = 123456;
  target_time[1].tv_sec = 456789;
  target_time[0].tv_nsec = 2222;
  target_time[1].tv_nsec = 12345678;

  fd = open("/tmp/test_fuse/testfile1", O_RDWR);
  ret_val = futimens(fd, target_time);

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
    hcfs_system->systemdata.system_quota = 25600000; /* Set quota */
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.pinned_size = 10000;
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
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = truncate("/tmp/test_fuse/does_not_exist", 100);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}
TEST_F(hfuse_truncateTest, IsNotFile) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = truncate("/tmp/test_fuse/testdir1", 100);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EISDIR);
}
TEST_F(hfuse_truncateTest, NegativeSize) {
  int32_t ret_val;
  int32_t tmp_err;

  ret_val = truncate("/tmp/test_fuse/testfile2", -1);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, EINVAL);
}
TEST_F(hfuse_truncateTest, NoSizeChange) {
  int32_t ret_val;
  struct stat tempstat;

  ret_val = truncate("/tmp/test_fuse/testfile2", 1024);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}
TEST_F(hfuse_truncateTest, ExtendSize) {
  int32_t ret_val;
  struct stat tempstat;

  ret_val = truncate("/tmp/test_fuse/testfile2", 102400);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 102400);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 + (102400 - 1024));
}
TEST_F(hfuse_truncateTest, ExtendExceedQuota) {
  int32_t ret_val;
  int32_t tmp_err;
  struct stat tempstat;

  ret_val = truncate("/tmp/test_fuse/testfile2", 102400000);
  tmp_err = errno;

  ASSERT_EQ(ret_val, -1);
  ASSERT_EQ(tmp_err, ENOSPC);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}
TEST_F(hfuse_truncateTest, TruncateZeroNoBlock) {
  int32_t ret_val;
  struct stat tempstat;

  fake_block_status = ST_NONE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
}
TEST_F(hfuse_truncateTest, TruncateZeroBlockTodelete) {
  int32_t ret_val;
  struct stat tempstat;

  fake_block_status = ST_TODELETE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
}
TEST_F(hfuse_truncateTest, TruncateZeroBlockLdisk) {
  int32_t ret_val;
  struct stat tempstat;
  char temppath[1024];

  fetch_block_path(temppath, 14, 0);
  mknod(temppath, S_IFREG | 0700, makedev(0,0));
  ASSERT_EQ(access(temppath, F_OK), 0);
  fake_block_status = ST_LDISK;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 0);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 0);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 102400);
  EXPECT_NE(access(temppath, F_OK), 0);
}

TEST_F(hfuse_truncateTest, TruncateHalfLdisk) {
  int32_t ret_val;
  char temppath[1024];
  int32_t fd;
  struct stat tempstat;
  char buffer[102400] = {0};

  fetch_block_path(temppath, 14, 0);
  fd = creat(temppath, 0700);
  pwrite(fd, buffer, 102400, 0);
  //ftruncate(fd, 102400);
  close(fd);
  stat(temppath, &tempstat);
  ASSERT_EQ(tempstat.st_size, 102400);
  ASSERT_EQ(access(temppath, F_OK), 0);
  fake_block_status = ST_LDISK;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);

  ASSERT_EQ(ret_val, 0);
  ASSERT_EQ(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  stat(temppath, &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000 - (round_size(102400) - round_size(51200)));
  EXPECT_EQ(hcfs_system->systemdata.cache_blocks, 13);
}
TEST_F(hfuse_truncateTest, TruncateHalfNoblock) {
  int32_t ret_val;
  struct stat tempstat;
  char temppath[1024];
  int32_t fd;

  fetch_block_path(temppath, 14, 0);
  if (access(temppath, F_OK) == 0)
    unlink(temppath);
  fake_block_status = ST_NONE;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);

  ASSERT_EQ(ret_val, 0);
  EXPECT_NE(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000);
  EXPECT_EQ(hcfs_system->systemdata.cache_blocks, 13);
}
TEST_F(hfuse_truncateTest, TruncateHalfCloud) {
  int32_t ret_val;
  struct stat tempstat;
  char temppath[1024];
  int32_t fd;

  fetch_block_path(temppath, 14, 0);

  fake_block_status = ST_CLOUD;
  ret_val = truncate("/tmp/test_fuse/testtruncate", 51200);

  ASSERT_EQ(ret_val, 0);
  ASSERT_EQ(access(temppath, F_OK), 0);
  stat("/tmp/test_fuse/testtruncate", &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  stat(temppath, &tempstat);
  EXPECT_EQ(tempstat.st_size, 51200);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 - 51200);
  EXPECT_EQ(hcfs_system->systemdata.cache_size, 1200000 + round_size(51200));
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
  int32_t tmp_err;
  int32_t ret_val;

  fptr = fopen("/tmp/test_fuse/does_not_exist", "r");
  tmp_err = errno;
  
  ret_val = 0;
  if (fptr == NULL)
    ret_val = -1;
  ASSERT_EQ(ret_val, -1);
  EXPECT_EQ(tmp_err, ENOENT);
}

TEST_F(hfuse_openTest, FailOpenFh) {
  int32_t tmp_err;
  int32_t ret_val;

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
  int32_t tmp_err;
  int32_t ret_val;

  errno = 0;
  fptr = fopen("/tmp/test_fuse/testfile1", "r");
  tmp_err = errno;
  
  EXPECT_NE(fptr, NULL);
  EXPECT_EQ(tmp_err, 0);
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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int32_t fd;
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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int32_t fd;
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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;

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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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

TEST_F(hfuse_readTest, ReadPagedOutRead) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  struct stat tmpstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;
  ino_t tmpino;
  FILE *tmpfptr;
  int fd;

  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  stat("/tmp/test_fuse/testread", &tmpstat);
  fd = open("/tmp/test_fuse/testread", O_RDONLY | O_DIRECT);
  fptr = fdopen(fd, "r");
  setbuf(fptr, NULL);
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);

  tmpino = (int64_t) tmpstat.st_ino;
  tmpfptr = system_fh_table.entry_table[tmpino].blockfptr;
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  /* Delete the block file */
  unlink(temppath);
  /* Then write something bogus */
  fseek(tmpfptr, 0, SEEK_SET);
  fprintf(tmpfptr, "This is not correct");

  fseek(fptr, 0, SEEK_SET);
  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is not correct", 
            strlen("This is not correct")), 0);

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_CLOUD;
  tempbuf[0] = 0;
  fflush(fptr);

  fseek(fptr, 0, SEEK_SET);
  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadPagedOutReRead) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;
  struct stat tmpstat;
  ino_t tmpino;
  FILE *tmpfptr, *fptr1;
  int fd;

  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  stat("/tmp/test_fuse/testread", &tmpstat);
  fd = open("/tmp/test_fuse/testread", O_RDONLY | O_DIRECT);
  fptr = fdopen(fd, "r");
  setbuf(fptr, NULL);
  ASSERT_NE(fptr != NULL, 0);

  fake_paged_out_count = 10;

  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);

  tmpino = (int64_t) tmpstat.st_ino;
  tmpfptr = system_fh_table.entry_table[tmpino].blockfptr;
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  /* Delete the block file */
  unlink(temppath);
  /* Then write something bogus */
  fseek(tmpfptr, 0, SEEK_SET);
  fprintf(tmpfptr, "This is not correct");

  fseek(fptr, 0, SEEK_SET);
  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is not correct",
            strlen("This is not correct")), 0);

  /* Recreate the block */
  fetch_block_path(temppath, 15, 0);
  fake_block_status = ST_LDISK;
  fptr1 = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr1);
  fclose(fptr1);

  fake_paged_out_count = 11;

  fseek(fptr, 0, SEEK_SET);
  ret_items = fread(tempbuf, 100, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(strncmp(tempbuf, "This is a test data", tmp_len), 0);
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_readTest, ReadCloudWaitCache) {
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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
    hcfs_system->systemdata.system_quota = 25600000;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.pinned_size = 0;
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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int32_t fd;
  size_t ret_items;

  fetch_block_path(temppath, 16, 0);

  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr != NULL, 0);

  ret_items = fwrite(tempbuf, 0, 0, fptr);
  EXPECT_EQ(ret_items, 0);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_ll_writeTest, WriteWhenExceedingSystemQuota) {
  int32_t ret_val;
  int32_t tmp_err, tmp_len;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int32_t fd;
  size_t ret_items;

  hcfs_system->systemdata.system_quota =
    hcfs_system->systemdata.system_size - 1;

  fetch_block_path(temppath, 16, 0);

  EXPECT_EQ(0, access("/tmp/test_fuse/testwrite", F_OK));
  fptr = fopen("/tmp/test_fuse/testwrite", "r+");
  ASSERT_NE(fptr, NULL);
  setbuf(fptr, NULL);

  snprintf(tempbuf, 10, "test");
  fseek(fptr, 0, SEEK_SET);
  tmp_len = strlen(tempbuf)+1;
  errno = 0;
  ret_items = fwrite(tempbuf, tmp_len, 1, fptr);
  tmp_err = errno;
  EXPECT_EQ(ret_items, 0);
  EXPECT_EQ(tmp_err, ENOSPC);
  fclose(fptr);
  fptr = NULL;
}

TEST_F(hfuse_ll_writeTest, WritePastEnd) {
  int32_t ret_val;
  int32_t tmp_err, tmp_len;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  int32_t fd;
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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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

TEST_F(hfuse_ll_writeTest, ReWritePagedOutLocal) {
  int ret_val;
  int tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;
  ino_t tmpino;
  FILE *tmpfptr, *fptr1;
  int fd;
  struct stat tmpstat;

  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  stat("/tmp/test_fuse/testwrite", &tmpstat);
  fd = open("/tmp/test_fuse/testwrite", O_RDWR | O_DIRECT);
  fptr = fdopen(fd, "r+");
  setbuf(fptr, NULL);
  ASSERT_NE(fptr != NULL, 0);

  fake_paged_out_count = 10;

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);

  tmpino = (int64_t) tmpstat.st_ino;
  tmpfptr = system_fh_table.entry_table[tmpino].blockfptr;
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  /* Delete the block file */
  unlink(temppath);
  /* Then write something bogus */
  fseek(tmpfptr, 0, SEEK_SET);
  fprintf(tmpfptr, "This is not correct");

  /* Recreate the block file */
  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_LDISK;
  fptr1 = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr1);
  fclose(fptr1);

  fake_paged_out_count = 11;

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);

  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0) << tempbuf;
}

TEST_F(hfuse_ll_writeTest, ReWritePagedOut) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;
  ino_t tmpino;
  FILE *tmpfptr;
  int fd;
  struct stat tmpstat;

  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_LDISK;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  stat("/tmp/test_fuse/testwrite", &tmpstat);
  fd = open("/tmp/test_fuse/testwrite", O_RDWR | O_DIRECT);
  fptr = fdopen(fd, "r+");
  setbuf(fptr, NULL);
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);

  tmpino = (int64_t) tmpstat.st_ino;
  tmpfptr = system_fh_table.entry_table[tmpino].blockfptr;
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  /* Delete the block file */
  unlink(temppath);
  /* Then write something bogus */
  fseek(tmpfptr, 0, SEEK_SET);
  fprintf(tmpfptr, "This is not correct");

  test_fetch_from_backend = TRUE;
  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_CLOUD;

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);

  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0);
}

TEST_F(hfuse_ll_writeTest, ReWriteSynced) {
  int ret_val;
  int tmp_err;
  struct stat tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int count;
  int tmp_len;
  ino_t tmpino;
  int fd;
  struct stat tmpstat;

  fetch_block_path(temppath, 16, 0);
  fake_block_status = ST_BOTH;
  fptr = fopen(temppath,"a+");
  snprintf(tempbuf, 100, "This is a test data");
  tmp_len = strlen(tempbuf);
  fwrite(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);

  stat("/tmp/test_fuse/testwrite", &tmpstat);
  fd = open("/tmp/test_fuse/testwrite", O_RDWR | O_DIRECT);
  fptr = fdopen(fd, "r+");
  setbuf(fptr, NULL);
  ASSERT_NE(fptr != NULL, 0);

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);

  tmpino = (int64_t) tmpstat.st_ino;
  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);
  EXPECT_EQ(ST_LDISK, updated_block_page.block_entries[0].status);

  /* Fake synced block */
  fake_block_status = ST_BOTH;

  fseek(fptr, 10, SEEK_SET);
  snprintf(tempbuf, 10, "temp");
  ret_items = fwrite(tempbuf, 4, 1, fptr);
  EXPECT_EQ(ret_items, 1);
  EXPECT_EQ(ST_LDISK, updated_block_page.block_entries[0].status);

  EXPECT_EQ(0, system_fh_table.entry_table[tmpino].opened_block);

  fclose(fptr);
  fptr = NULL;

  fptr = fopen(temppath,"r");
  fread(tempbuf, tmp_len, 1, fptr);
  fclose(fptr);
  fptr = NULL;
  EXPECT_EQ(strncmp(tempbuf, "This is a temp data", tmp_len), 0);
}

TEST_F(hfuse_ll_writeTest, ReWriteCloudWaitCache) {
  int32_t ret_val;
  int32_t tmp_err;
  HCFS_STAT tempstat;
  char temppath[1024];
  char tempbuf[1024];
  size_t ret_items;
  int32_t count;
  int32_t tmp_len;

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
    unittest_mount.FS_stat->system_size = 12800000;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.system_quota = 25600000; /* set quota */
    sys_super_block->head.num_active_inodes = 10000;
    unittest_mount.FS_stat->num_inodes = 10000;
    before_update_file_data = TRUE;
    root_updated = FALSE;
  }

  virtual void TearDown() {
  }
};

TEST_F(hfuse_ll_statfsTest, SmallSysStat) {

  struct statfs tmpstat;
  int32_t ret_val;
  int32_t ret;
  int64_t total_blocks;

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

  total_blocks = (25600000 - 1) / 4096 + 1;
  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(total_blocks, tmpstat.f_blocks);
  EXPECT_EQ(total_blocks - (((12800000 - 1) / 4096) + 1), tmpstat.f_bfree);
  EXPECT_EQ(total_blocks - (((12800000 - 1) / 4096) + 1), tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000 - 10000, tmpstat.f_ffree);
}
TEST_F(hfuse_ll_statfsTest, EmptySysStat) {

  struct statfs tmpstat;
  int32_t ret_val;
  int64_t total_blocks;

  hcfs_system->systemdata.system_size = 0;
  hcfs_system->systemdata.cache_size = 0;
  hcfs_system->systemdata.cache_blocks = 0;
  hcfs_system->systemdata.system_quota = 25600000; /* set quota */
  sys_super_block->head.num_active_inodes = 0;
  unittest_mount.FS_stat->system_size = 0;
  unittest_mount.FS_stat->num_inodes = 0;

  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  total_blocks = (25600000 - 1) / 4096 + 1;
  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(total_blocks, tmpstat.f_blocks);
  EXPECT_EQ(total_blocks, tmpstat.f_bfree);
  EXPECT_EQ(total_blocks, tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000, tmpstat.f_ffree);
}

TEST_F(hfuse_ll_statfsTest, BorderStat) {

  struct statfs tmpstat;
  int32_t ret_val;
  int64_t total_blocks;

  hcfs_system->systemdata.system_size = 4096;
  hcfs_system->systemdata.cache_size = 0;
  hcfs_system->systemdata.cache_blocks = 0;
  hcfs_system->systemdata.system_quota = 25600000; /* set quota */
  unittest_mount.FS_stat->system_size = 4096;

  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  total_blocks = (25600000 - 1) / 4096 + 1;
  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(total_blocks, tmpstat.f_blocks);
  EXPECT_EQ(total_blocks - 1, tmpstat.f_bfree);
  EXPECT_EQ(total_blocks - 1, tmpstat.f_bavail);
  EXPECT_EQ(2000000, tmpstat.f_files);
  EXPECT_EQ(2000000 - 10000, tmpstat.f_ffree);
}

TEST_F(hfuse_ll_statfsTest, LargeSysStat) {

  struct statfs tmpstat;
  int32_t ret_val;
  int64_t sys_blocks;

  hcfs_system->systemdata.system_size = 512*powl(1024,3) + 1;
  hcfs_system->systemdata.system_quota = 512*powl(1024,3) + 1; /* set quota */
  sys_super_block->head.num_active_inodes = 2000000;
  unittest_mount.FS_stat->system_size = 512*powl(1024,3) + 1;
  unittest_mount.FS_stat->num_inodes = 2000000;

  sys_blocks = ((512*powl(1024,3) + 1 - 1) / 4096) + 1;
  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(sys_blocks, tmpstat.f_blocks);
  EXPECT_EQ(0, tmpstat.f_bfree);
  EXPECT_EQ(0, tmpstat.f_bavail);
  EXPECT_EQ(4000000, tmpstat.f_files);
  EXPECT_EQ(2000000, tmpstat.f_ffree);
}

TEST_F(hfuse_ll_statfsTest, ExceedSysStat) {

  struct statfs tmpstat;
  int32_t ret_val;
  int64_t sys_blocks;

  hcfs_system->systemdata.system_size = 512*powl(1024,3) + 1;
  hcfs_system->systemdata.system_quota = 256*powl(1024,3) + 1; /* set quota */
  sys_super_block->head.num_active_inodes = 2000000;
  unittest_mount.FS_stat->system_size = 512*powl(1024,3) + 1;
  unittest_mount.FS_stat->num_inodes = 2000000;

  sys_blocks = ((256*powl(1024,3) + 1 - 1) / 4096) + 1; /* The same as system quota */
  ret_val = statfs("/tmp/test_fuse/testfile", &tmpstat);

  ASSERT_EQ(0, ret_val);

  EXPECT_EQ(4096, tmpstat.f_bsize);
  EXPECT_EQ(4096, tmpstat.f_frsize);
  EXPECT_EQ(sys_blocks, tmpstat.f_blocks);
  EXPECT_EQ(0, tmpstat.f_bfree);
  EXPECT_EQ(0, tmpstat.f_bavail);
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
  int32_t ret_val;
  HCFS_STAT tempstat;

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = 0;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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
  int32_t ret_val;
  HCFS_STAT tempstat;

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = 1;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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
  int32_t ret_val, count;
  HCFS_STAT tempstat;
  char filename[100];

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = MAX_DIR_ENTRIES_PER_PAGE - 2;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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
  int32_t ret_val, count;
  HCFS_STAT tempstat;
  char filename[100];

  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = (2 * MAX_DIR_ENTRIES_PER_PAGE) - 2;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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

TEST_F(hfuse_ll_readdirTest, TwoMaxPageEntriesWithSnapshot) {
/* Note: this won't happen in actual b-tree */
  DIR_META_TYPE temphead;
  DIR_ENTRY_PAGE temppage;
  DIR *dptr;
  struct dirent tmp_dirent, *tmp_dirptr;
  int ret_val, count;
  HCFS_STAT tempstat;
  char filename[100];
  DIRH_ENTRY *dirh_ptr;
  char readdirsnap_metapath[100];

  /* Create the mock dir meta that is modified */
  fptr = fopen(readdir_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = 1;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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

  /* Create the snapshotted version */
  snprintf(readdirsnap_metapath, 100, "%s_snap", readdir_metapath);
  fptr = fopen(readdirsnap_metapath, "w");
  setbuf(fptr, NULL);
  fwrite(&tempstat, sizeof(HCFS_STAT), 1, fptr);
  temphead.total_children = (2 * MAX_DIR_ENTRIES_PER_PAGE) - 2;
  temphead.root_entry_page = sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE);
  temphead.next_xattr_page = 0;
  temphead.entry_page_gc_list = 0;
  temphead.tree_walk_list_head = temphead.root_entry_page;
  fwrite(&temphead, sizeof(DIR_META_TYPE), 1, fptr);

  ASSERT_EQ(sizeof(HCFS_STAT) + sizeof(DIR_META_TYPE), ftell(fptr));
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

  /* Change to snapshot from here */
  dirh_ptr = &(system_fh_table.direntry_table[TEST_LISTDIR_INODE]);
  dirh_ptr->snapshot_ptr = fopen(readdirsnap_metapath, "r");
  
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
  unlink(readdirsnap_metapath);
}

/* End of the test case for the function hfuse_ll_readdir */

//#ifndef _ANDROID_ENV_
/* 
	Unittest of hfuse_ll_setxattr()
 */
class hfuse_ll_setxattrTest : public ::testing::Test {
protected:
	void SetUp()
	{
		root_updated = FALSE;
		before_update_file_data = TRUE;
  		hcfs_system->systemdata.system_meta_size = 0;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_setxattrTest, SetKeyWithoutValue)
{
	int32_t ret;

	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", "", 0, 0);

	EXPECT_EQ(0, ret);
}

TEST_F(hfuse_ll_setxattrTest, NamespaceInvalid)
{
	int32_t ret;
	int32_t errcode;

	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_setxattrTest, PermissionDenied)
{
	int32_t ret;
	int32_t errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_setxattrTest, SecurityAlwaysAllow)
{
	int32_t ret;
	int32_t errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"security.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EPERM, errcode);
}

TEST_F(hfuse_ll_setxattrTest, InsertXattrReturnFail)
{
	int32_t ret;
	int32_t errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa", "123", 3, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_setxattrTest, InsertXattrSuccess)
{
	int32_t ret;
	int32_t errcode;
	
	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", "123", 3, 0);

	EXPECT_EQ(0, ret);
}

TEST_F(hfuse_ll_setxattrTest, InsertXattr_NoSpace)
{
	int32_t ret;
	int32_t errcode;

  	hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;
	ret = setxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", "123", 3, 0);
  	hcfs_system->systemdata.system_meta_size = 0;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOSPC, errno);
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
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_getxattrTest, NamespaceInvalid)
{
	int32_t ret;
	int32_t errcode;
	char buf[10];

	ret = getxattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa", buf, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_getxattrTest, PermissionDenied)
{
	int32_t ret;
	int32_t errcode;
	char buf[10];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa", buf, 0);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_getxattrTest, SecurityAlwaysAllow)
{
	int32_t ret;
	int32_t errcode;
	char buf[10];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"security.aaa", buf, 0);

	EXPECT_EQ(CORRECT_VALUE_SIZE, ret);
}

TEST_F(hfuse_ll_getxattrTest, GetCorrectValueSizeSuccess)
{
	int32_t ret;
	int32_t errcode;
	char buf[10];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa", buf, 0);

	EXPECT_EQ(CORRECT_VALUE_SIZE, ret);
}

TEST_F(hfuse_ll_getxattrTest, GetValueFail)
{
	int32_t ret;
	int32_t errcode;
	char buf[100];
	
	ret = getxattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa", buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_getxattrTest, GetValueSuccess)
{
	int32_t ret;
	int32_t errcode;
	char buf[100];
	char const *ans = "hello!getxattr:)";

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
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_listxattrTest, GetCorrectValueSizeSuccess)
{
	int32_t ret;
	int32_t errcode;
	char buf[10];
	
	ret = listxattr("/tmp/test_fuse/testsetxattr", 
		buf, 0);

	EXPECT_EQ(CORRECT_VALUE_SIZE, ret);
}

TEST_F(hfuse_ll_listxattrTest, GetValueFail)
{
	int32_t ret;
	int32_t errcode;
	char buf[100];
	
	ret = listxattr("/tmp/test_fuse/testsetxattr_fail", 
		buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_listxattrTest, GetValueSuccess)
{
	int32_t ret;
	int32_t errcode;
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
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_removexattrTest, NamespaceInvalid)
{
	int32_t ret;
	int32_t errcode;

	ret = removexattr("/tmp/test_fuse/testsetxattr", 
		"aloha.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EOPNOTSUPP, errcode);
}

TEST_F(hfuse_ll_removexattrTest, PermissionDenied)
{
	int32_t ret;
	int32_t errcode;
	
	ret = removexattr("/tmp/test_fuse/testsetxattr_permissiondeny", 
		"user.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_removexattrTest, SecurityAlwaysAllow)
{
	int32_t ret;
	int32_t errcode;
	
	ret = removexattr("/tmp/test_fuse/testsetxattr", 
		"security.aaa");

	errcode = errno;
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EPERM, errcode);
}

TEST_F(hfuse_ll_removexattrTest, RemoveXattrReturnFail)
{
	int32_t ret;
	int32_t errcode;
	
	ret = removexattr("/tmp/test_fuse/testsetxattr_fail", 
		"user.aaa");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_removexattrTest, RemoveXattrSuccess)
{
	int32_t ret;

	ret = removexattr("/tmp/test_fuse/testsetxattr", 
		"user.aaa");

	EXPECT_EQ(0, ret);
}
/*
	End of unittest of hfuse_ll_removexattr()
 */

//#endif
/*
	Unittest of hfuse_ll_symlink()
 */
class hfuse_ll_symlinkTest : public ::testing::Test {
protected:
	void SetUp()
	{
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_symlinkTest, FileExists)
{
	int32_t ret;
	int32_t errcode;

	errcode = 0;
	ret = symlink("name_not_used", "/tmp/test_fuse/testsymlink");
	errcode = errno;	

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_symlinkTest, SelfNameTooLong)
{
	int32_t ret;
	int32_t errcode;
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
	int32_t ret;
	int32_t errcode;
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
	int32_t ret;
	int32_t errcode;

	errcode = 0;

	ret = symlink("not_used", 
		"/tmp/test_fuse/testsymlink_exist_in_symlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_symlinkTest, UpdateMetaFail)
{
	int32_t ret;
	int32_t errcode;

	errcode = 0;

	ret = symlink("update_meta_fail", 
		"/tmp/test_fuse/testsymlink_not_exist_in_symlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(1, errcode);
}

TEST_F(hfuse_ll_symlinkTest, SymlinkSuccess)
{
	int32_t ret;

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
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_readlinkTest, FileNotExist)
{
	char buf[100];
	int32_t ret;
	int32_t errcode;

	ret = readlink("/tmp/test_fuse/test_readlink_not_exist", buf, 100);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOENT, errcode);
}

TEST_F(hfuse_ll_readlinkTest, ReadLinkSuccess)
{
	char buf[100];
	char const *ans = "I_am_target_link";
	int32_t ret;
	int32_t errcode;

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
		root_updated = FALSE;
		before_update_file_data = TRUE;
	}

	void TearDown()
	{
	}
};

TEST_F(hfuse_ll_linkTest, OldlinkNotExists)
{
	int32_t ret;
	int32_t errcode;

	ret = link("/tmp/test_fuse/old_link_not_exists",
		"/tmp/test_fuse/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOENT, errcode);

}

TEST_F(hfuse_ll_linkTest, NewlinkExists)
{
	int32_t ret;
	int32_t errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testlink");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EEXIST, errcode);
}

TEST_F(hfuse_ll_linkTest, NewlinkNameTooLong)
{
	int32_t ret;
	int32_t errcode;
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
	int32_t ret;
	int32_t errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testlink_dir_perm_denied/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EACCES, errcode);

}

TEST_F(hfuse_ll_linkTest, ParentIsNotDir)
{
	int32_t ret;
	int32_t errcode;

	ret = link("/tmp/test_fuse/testlink",
		"/tmp/test_fuse/testfile/new_link");
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(ENOTDIR, errcode);

}

TEST_F(hfuse_ll_linkTest, link_update_metaFail)
{
	int32_t ret;
	int32_t errcode;
	char hardlink[500] = "/tmp/test_fuse/new_link_update_meta_fail";

	ret = link("/tmp/test_fuse/testlink", hardlink);
	errcode = errno;

	EXPECT_EQ(-1, ret);
	EXPECT_EQ(123, errcode);
}

TEST_F(hfuse_ll_linkTest, LinkSuccess)
{
	int32_t ret;
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
	int32_t fd;

	void SetUp()
	{
		root_updated = FALSE;
		before_update_file_data = TRUE;
  		hcfs_system->systemdata.system_meta_size = 0;
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
	int32_t errcode;

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
	const char *name = "/tmp/test_fuse/testfile/creat_test";
	int32_t errcode;


	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENOTDIR, errcode);
}

TEST_F(hfuse_ll_createTest, ParentPermissionDenied)
{
	const char *name = "/tmp/test_fuse/testlink_dir_perm_denied/creat_test";
	int32_t errcode;

	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(EACCES, errcode);
}

TEST_F(hfuse_ll_createTest, super_block_new_inodeFail)
{
	const char *name = "/tmp/test_fuse/creat_test";
	int32_t errcode;

	fail_super_block_new_inode = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENOSPC, errcode);
	
	fail_super_block_new_inode = FALSE;
}

TEST_F(hfuse_ll_createTest, mknod_update_metaFail)
{
	const char *name = "/tmp/test_fuse/creat_test";
	int32_t errcode;

	fail_mknod_update_meta = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(1, errcode);
	fail_mknod_update_meta = FALSE;
}

TEST_F(hfuse_ll_createTest, open_fhFail)
{
	const char *name = "/tmp/test_fuse/creat_test";
	int32_t errcode;

	fail_open_files = TRUE;
	fd = creat(name, 0777);
	errcode = errno;

	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENFILE, errcode);
	fail_open_files = FALSE;
}

TEST_F(hfuse_ll_createTest, CreateSuccess)
{
	const char *name = "/tmp/test_fuse/creat_test";

	fd = creat(name, 0777);

	EXPECT_GT(fd, 0);
}

TEST_F(hfuse_ll_createTest, Create_NoSpace)
{
	const char *name = "/tmp/test_fuse/creat_test";
	int32_t errcode;

  	hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;
	fd = creat(name, 0777);
	errcode = errno;
  	hcfs_system->systemdata.system_meta_size = 0;

	EXPECT_EQ(fd, -1);
	EXPECT_EQ(ENOSPC, errcode);
}

/*
	End of unittest of hfuse_ll_create()
 */

/* Begin of the test case for the function hfuse_ll_fallocate */
class hfuse_ll_fallocateTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    before_update_file_data = TRUE;
    root_updated = FALSE;
    fake_block_status = ST_NONE;
    after_update_block_page = FALSE;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.pinned_size = 10000;
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
TEST_F(hfuse_ll_fallocateTest, ExtendSize) {
  int32_t ret_val;
  struct stat tempstat;
  FILE *fptr;

  fptr = fopen("/tmp/test_fuse/testfile2", "r+");
  ASSERT_NE(0, (fptr != NULL));
  ret_val = fallocate(fileno(fptr), 0, 0, 102400);
  fclose(fptr);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 102400);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 + (102400 - 1024));
}
TEST_F(hfuse_ll_fallocateTest, ExtendSize2) {
  int32_t ret_val;
  struct stat tempstat;
  FILE *fptr;

  fptr = fopen("/tmp/test_fuse/testfile2", "r+");
  ASSERT_NE(0, (fptr != NULL));
  ret_val = fallocate(fileno(fptr), 0, 1024, 102400);
  fclose(fptr);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 102400 + 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 + 102400);
}
TEST_F(hfuse_ll_fallocateTest, ModeNotSupported) {
  int32_t ret_val;
  int32_t tmp_err;
  FILE *fptr;

  fptr = fopen("/tmp/test_fuse/testfile2", "r+");
  ASSERT_NE(0, (fptr != NULL));
  ret_val = fallocate(fileno(fptr), 4, 1024, 102400);
  tmp_err = errno;
  fclose(fptr);

  ASSERT_EQ(-1, ret_val);
  ASSERT_EQ(ENOTSUP, tmp_err);
}
TEST_F(hfuse_ll_fallocateTest, NoExtend) {
  int32_t ret_val;
  struct stat tempstat;
  FILE *fptr;

  fptr = fopen("/tmp/test_fuse/testfile2", "r+");
  ASSERT_NE(0, (fptr != NULL));
  ret_val = fallocate(fileno(fptr), 0, 0, 10);
  fclose(fptr);

  ASSERT_EQ(ret_val, 0);
  stat("/tmp/test_fuse/testfile2", &tempstat);
  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}

/* End of the test case for the function hfuse_ll_fallocate */

