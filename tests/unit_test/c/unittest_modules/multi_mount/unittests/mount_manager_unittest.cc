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
#include <ftw.h>
extern "C" {
#include "mount_manager.h"
#include "FS_manager.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "mount_manager_unittest.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT *system_config;

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
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

class mount_managerEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		}
		void TearDown()
		{
			free(system_config);
		}
};

::testing::Environment* const metaops_env =
	::testing::AddGlobalTestEnvironment(new mount_managerEnvironment);

MOUNT_NODE_T* prepare_node(char *fname) {
  MOUNT_NODE_T *tmpptr;

  tmpptr = (MOUNT_NODE_T *) malloc(sizeof(MOUNT_NODE_T));
  memset(tmpptr, 0, sizeof(MOUNT_NODE_T));
  tmpptr->mt_entry = (MOUNT_T *) malloc(sizeof(MOUNT_T));
  memset(tmpptr->mt_entry, 0, sizeof(MOUNT_T));
  strcpy((tmpptr->mt_entry)->f_name, fname);
  (tmpptr->mt_entry)->f_mp = NULL;
  tmpptr->mt_entry->volume_type = ANDROID_MULTIEXTERNAL;
  return tmpptr;
 }

void free_node(MOUNT_NODE_T *innode) {
  free(innode->mt_entry);
  free(innode);
 }

void free_tree(MOUNT_NODE_T *node) {
  if (node->lchild != NULL)
    free_tree(node->lchild);

  if (node->rchild != NULL)
    free_tree(node->rchild);

  free_node(node);
 }

MOUNT_NODE_T* build_subtree(MOUNT_NODE_T *parent, int32_t start1, int32_t end1) {
  MOUNT_NODE_T *tmpnode;
  char fsname[10];
  int32_t midval, start, end;

  midval = (start1 + end1) / 2;
  snprintf(fsname, 10, "%4d", midval);

  tmpnode = prepare_node(fsname);
  tmpnode->parent = parent;

  start = start1;
  end = midval - 1;
  if (start <= end)
    tmpnode->lchild = build_subtree(tmpnode, start, end);
  else
    tmpnode->lchild = NULL;

  start = midval + 1;
  end = end1;
  if (start <= end)
    tmpnode->rchild = build_subtree(tmpnode, start, end);
  else
    tmpnode->rchild = NULL;

  return tmpnode;
 }

void build_tree(int32_t num_nodes) {
  mount_mgr.root = build_subtree(NULL, 0, num_nodes - 1);
 }

/* Begin of the test case for the function search_mount */

class search_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(search_mountTest, EmptyDatabase) {
  int32_t ret;
  MOUNT_T *tmpinfo;

  mount_mgr.root = NULL;
  ret = search_mount("anyFS", NULL, &tmpinfo);
  EXPECT_EQ(-ENOENT, ret);

 }

TEST_F(search_mountTest, MultipleFSFound) {
  int32_t ret, count;
  MOUNT_T *tmpinfo;
  char fsname[10];

  build_tree(100);

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, NULL, &tmpinfo);
    EXPECT_EQ(0, ret);
   }

 }
TEST_F(search_mountTest, MultipleFSNotFound) {
  int32_t ret, count;
  MOUNT_T *tmpinfo;
  char fsname[10];

  build_tree(100);

  for (count = 100; count < 200; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, NULL, &tmpinfo);
    EXPECT_EQ(-ENOENT, ret);
   }

 }

/* End of the test case for the function search_mount */

/* Begin of the test case for the function insert_mount */

class insert_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(insert_mountTest, MultipleFS) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1, *tmpinfo2;
  MOUNT_T *tmpptr[100];
  char fsname[10];

  for (count = 0; count < 100; count++) {
    tmpinfo1 = (MOUNT_T *) malloc(sizeof(MOUNT_T));
    snprintf(fsname, 10, "%4d", count);
    strcpy(tmpinfo1->f_name, fsname);
    tmpinfo1->f_mp = "mp";
    ret = insert_mount(fsname, tmpinfo1);
    if (ret != 0)
      printf("%s\n", strerror(-ret));
    ASSERT_EQ(0, ret);
    tmpptr[count] = tmpinfo1;
   }
  ASSERT_EQ(100, mount_mgr.num_mt_FS);
  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, "mp", &tmpinfo2);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(tmpptr[count], tmpinfo2);
   }

 }
TEST_F(insert_mountTest, MultipleFSReverse) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1, *tmpinfo2;
  MOUNT_T *tmpptr[100];
  char fsname[10];

  for (count = 99; count >= 0; count--) {
    tmpinfo1 = (MOUNT_T *) malloc(sizeof(MOUNT_T));
    snprintf(fsname, 10, "%4d", count);
    strcpy(tmpinfo1->f_name, fsname);
    tmpinfo1->f_mp = "mp";
    ret = insert_mount(fsname, tmpinfo1);
    if (ret != 0)
      printf("%s\n", strerror(-ret));
    ASSERT_EQ(0, ret);
    tmpptr[count] = tmpinfo1;
   }
  ASSERT_EQ(100, mount_mgr.num_mt_FS);
  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, "mp", &tmpinfo2);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(tmpptr[count], tmpinfo2);
   }

 }

/* End of the test case for the function insert_mount */

/* Begin of the test case for the function delete_mount */

class delete_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(delete_mountTest, MultipleFS) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(0, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFSReverse) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 99; count >= 0; count--) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(0, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFSReverse1) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 49; count >= 0; count--) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(50, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFSReverse2) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 24; count >= 0; count--) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(75, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFS1) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 49; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(49, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFS2) {
  int32_t ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 74; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, NULL, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, NULL, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(74, mount_mgr.num_mt_FS);

 }

/* End of the test case for the function delete_mount */

/* Begin of the test case for the function init_mount_mgr */

class init_mount_mgrTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(init_mount_mgrTest, InitMgr) {
  int32_t ret;

  ret = init_mount_mgr();
  ASSERT_EQ(0, ret);
  EXPECT_EQ(0, mount_mgr.num_mt_FS);
  if (mount_mgr.root == NULL)
    ret = 1;
  else
    ret = 0;
  EXPECT_EQ(1, ret);
  sem_getvalue(&(mount_mgr.mount_lock), &ret);
  EXPECT_EQ(1, ret);
 }

/* End of the test case for the function init_mount_mgr */

/* Begin of the test case for the function destroy_mount_mgr */

class destroy_mount_mgrTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(destroy_mount_mgrTest, DestroyMgr) {
  int32_t ret;

  ret = destroy_mount_mgr();
  ASSERT_EQ(0, ret);
 }

/* End of the test case for the function destroy_mount_mgr */

/* Begin of the test case for the function FS_is_mounted */

class FS_is_mountedTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(FS_is_mountedTest, EmptyDatabase) {
  int32_t ret;

  mount_mgr.root = NULL;
  ret = FS_is_mounted("anyFS");
  EXPECT_EQ(-ENOENT, ret);

 }

TEST_F(FS_is_mountedTest, MultipleFSFound) {
  int32_t ret, count;
  char fsname[10];

  build_tree(100);

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = FS_is_mounted(fsname);
    EXPECT_EQ(0, ret);
   }

 }
TEST_F(FS_is_mountedTest, MultipleFSNotFound) {
  int32_t ret, count;
  char fsname[10];

  build_tree(100);

  for (count = 100; count < 200; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = FS_is_mounted(fsname);
    EXPECT_EQ(-ENOENT, ret);
   }

 }

/* End of the test case for the function FS_is_mounted */

/* Begin of the test case for the function mount_FS */

class mount_FSTest : public ::testing::Test {
 protected:
  FILE *statfptr;
  FS_STAT_T tmp_stat;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
    FS_CORE_FAILED = 0;
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);

    statfptr = fopen("/tmp/testHCFS/metapath/stat100", "w");
    memset(&tmp_stat, 0, sizeof(FS_STAT_T));
    tmp_stat.num_inodes = 1;

    fwrite(&tmp_stat, sizeof(FS_STAT_T), 1, statfptr);
    fsync(fileno(statfptr));

    fclose(statfptr);

   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    unlink("/tmp/testHCFS/metapath/stat100");
    nftw(METAPATH, do_delete, 20, FTW_DEPTH);(METAPATH);
    nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
    free(METAPATH);

    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(mount_FSTest, AlreadyMounted) {
  int32_t ret;
  char fsname[10];

  build_tree(100);

  snprintf(fsname, 10, "%4d", 99);
  ret = mount_FS(fsname, "/tmp/testmount", MP_DEFAULT);
  EXPECT_EQ(-EPERM, ret);
 }

TEST_F(mount_FSTest, FSNotExist) {
  int32_t ret;
  char fsname[10];

  FS_CORE_FAILED = -ENOENT;
  snprintf(fsname, 10, "%4d", 10);
  ret = mount_FS(fsname, "/tmp/testmount", MP_DEFAULT);
  EXPECT_EQ(-ENOENT, ret);
 }

TEST_F(mount_FSTest, FSCoreError) {
  int32_t ret;
  char fsname[10];

  FS_CORE_FAILED = -EACCES;
  snprintf(fsname, 10, "%4d", 10);
  ret = mount_FS(fsname, "/tmp/testmount", MP_DEFAULT);
  EXPECT_EQ(-EACCES, ret);
 }

TEST_F(mount_FSTest, MountedFS) {
  int32_t ret;
  char fsname[10];

  snprintf(fsname, 10, "%4d", 10);
  ret = mount_FS(fsname, "/tmp/testmount", MP_DEFAULT);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(1, mount_mgr.num_mt_FS);
  EXPECT_EQ(100, mount_mgr.root->mt_entry->f_ino);
  EXPECT_EQ(MP_DEFAULT, mount_mgr.root->mt_entry->mp_mode);
  ASSERT_STREQ("/tmp/testmount", mount_mgr.root->mt_entry->f_mp);

  free(mount_mgr.root->mt_entry->f_mp);
  free(mount_mgr.root->mt_entry->vol_path_cache);
  pthread_join(mount_mgr.root->mt_entry->mt_thread, NULL);
 }

/* End of the test case for the function mount_FS */

/* Begin of the test case for the function unmount_FS */

class unmount_FSTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
    FS_CORE_FAILED = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(unmount_FSTest, NotMounted) {
  int32_t ret;
  char fsname[10];

  snprintf(fsname, 10, "%4d", 99);
  ret = unmount_FS(fsname, "mp");
  EXPECT_EQ(-ENOENT, ret);
 }
TEST_F(unmount_FSTest, UnmountMountedFS) {
  int32_t ret;
  char fsname[10];
  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
  sigset_t sigset, testset;
  struct timespec waittime;
  struct timeval curtime;
  struct sigaction newact, oldact;
  FILE *fptr;

  build_tree(1);

  snprintf(fsname, 10, "%4d", 0);

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

  mount_mgr.root->mt_entry->stat_lock = (sem_t *) malloc(sizeof(sem_t));
  mount_mgr.root->mt_entry->stat_fptr = fopen("/tmp/tmpstatfptr", "w+");
  sem_init((mount_mgr.root->mt_entry->stat_lock), 0, 1);
  mount_mgr.root->mt_entry->f_mp = (char *) malloc(100);
  snprintf(mount_mgr.root->mt_entry->f_mp, 100, "/tmp/testmount");
  pthread_create(&(mount_mgr.root->mt_entry->mt_thread),
         NULL, mount_multi_thread, (void *)mount_mgr.root->mt_entry);

  ret = unmount_FS(fsname, mount_mgr.root->mt_entry->f_mp);
  EXPECT_EQ(0, ret);

  unlink("/tmp/tmpstatfptr");

  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
  sigaction(SIGHUP, &oldact, NULL);
 }

/* End of the test case for the function unmount_FS */

/* Begin of the test case for the function unmount_event */

class unmount_eventTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
    FS_CORE_FAILED = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(unmount_eventTest, UnmountMountedFS) {
  int32_t ret, ret_val;
  char fsname[10];
  sigset_t sigset, testset;
  struct sigaction newact, oldact;
  FILE *fptr;

  build_tree(1);
  mount_mgr.num_mt_FS = 1;

  snprintf(fsname, 10, "%4d", 0);

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

  mount_mgr.root->mt_entry->stat_lock = (sem_t *) malloc(sizeof(sem_t));
  mount_mgr.root->mt_entry->stat_fptr = fopen("/tmp/tmpstatfptr", "w+");
  sem_init((mount_mgr.root->mt_entry->stat_lock), 0, 1);
  mount_mgr.root->mt_entry->f_mp = (char *) malloc(100);
  snprintf(mount_mgr.root->mt_entry->f_mp, 100, "/tmp/testmount");
  pthread_create(&(mount_mgr.root->mt_entry->mt_thread),
         NULL, mount_multi_thread, (void *)mount_mgr.root->mt_entry);

  EXPECT_EQ(1, mount_mgr.num_mt_FS);

  pthread_kill(mount_mgr.root->mt_entry->mt_thread, SIGHUP);
  ret_val = unmount_event(fsname, mount_mgr.root->mt_entry->f_mp);
  EXPECT_EQ(0, ret_val);
  EXPECT_EQ(0, mount_mgr.num_mt_FS);

  if (mount_mgr.root == NULL)
    ret = 0;
  else
    ret = -1;
  EXPECT_EQ(0, ret);
  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
  sigaction(SIGHUP, &oldact, NULL);
 }

/* End of the test case for the function unmount_event */

/* Begin of the test case for the function mount_status */

class mount_statusTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
    FS_CORE_FAILED = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(mount_statusTest, EmptyDatabase) {
  int32_t ret;

  mount_mgr.root = NULL;
  ret = mount_status("anyFS");
  EXPECT_EQ(-ENOENT, ret);

 }

TEST_F(mount_statusTest, MultipleFSFound) {
  int32_t ret, count;
  char fsname[10];

  build_tree(100);

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = mount_status(fsname);
    EXPECT_EQ(0, ret);
   }

 }
TEST_F(mount_statusTest, MultipleFSNotFound) {
  int32_t ret, count;
  char fsname[10];

  build_tree(100);

  for (count = 100; count < 200; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = mount_status(fsname);
    EXPECT_EQ(-ENOENT, ret);
   }

 }

/* End of the test case for the function mount_status */

/* Begin of the test case for the function unmount_all */

class unmount_allTest : public ::testing::Test {
 protected:
  FILE *statfptr;
  FS_STAT_T tmp_stat;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
    sem_init(&(mount_mgr.mount_lock), 0, 1);
    fs_mgr_head = (FS_MANAGER_HEAD_TYPE *) malloc(sizeof(FS_MANAGER_HEAD_TYPE));
    sem_init(&(fs_mgr_head->op_lock), 0, 1);
    FS_CORE_FAILED = 0;
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);

    statfptr = fopen("/tmp/testHCFS/metapath/stat100", "w");
    memset(&tmp_stat, 0, sizeof(FS_STAT_T));
    tmp_stat.num_inodes = 1;

    fwrite(&tmp_stat, sizeof(FS_STAT_T), 1, statfptr);
    fsync(fileno(statfptr));

    fclose(statfptr);
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    unlink("/tmp/testHCFS/metapath/stat100");
    nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
    free(METAPATH);
    free(hcfs_system);
    free(fs_mgr_head);
   }
 };

TEST_F(unmount_allTest, UnmountAll) {
  int32_t ret, count;
  char fsname[10];
  int32_t ret_val, errcode;
  uint32_t code, cmd_len, size_msg;
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

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = mount_FS(fsname, "/tmp/testmount", MP_DEFAULT);
   }
  EXPECT_EQ(100, mount_mgr.num_mt_FS);
  ret = unmount_all();
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, mount_mgr.num_mt_FS);

  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
  sigaction(SIGHUP, &oldact, NULL);
 }

/* End of the test case for the function unmount_all */

/* Begin of the test case for the function change_mount_stat */

class change_mount_statTest : public ::testing::Test {
 protected:
  MOUNT_T tmp_mount;
  virtual void SetUp() {
    memset(&tmp_mount, 0, sizeof(MOUNT_T));
    tmp_mount.FS_stat = (FS_STAT_T *)malloc(sizeof(FS_STAT_T));
    tmp_mount.stat_lock = (sem_t *)malloc(sizeof(sem_t));
    sem_init((tmp_mount.stat_lock), 0, 1);
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);
    tmp_mount.stat_fptr = fopen("/tmp/testHCFS/metapath/tmpstat", "a+");
   }

  virtual void TearDown() {
    fclose(tmp_mount.stat_fptr);
    unlink("/tmp/testHCFS/metapath/tmpstat");
    nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
    free(tmp_mount.FS_stat);
    free(METAPATH);
   }
 };

TEST_F(change_mount_statTest, TestChange) {
  int32_t ret;

  tmp_mount.FS_stat->system_size = 100000;
  tmp_mount.FS_stat->meta_size = 100;
  tmp_mount.FS_stat->num_inodes = 123;

  ret = change_mount_stat(&tmp_mount, 123456, 5566, 789);

  ASSERT_EQ(0, ret);
  EXPECT_EQ(100000 + 123456 + 5566, tmp_mount.FS_stat->system_size);
  EXPECT_EQ(100 + 5566, tmp_mount.FS_stat->meta_size);
  EXPECT_EQ(123 + 789, tmp_mount.FS_stat->num_inodes);

  ret = change_mount_stat(&tmp_mount, -1234560, -123, -7890);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(0, tmp_mount.FS_stat->system_size);
  EXPECT_EQ(100 + 5566 - 123, tmp_mount.FS_stat->meta_size);
  EXPECT_EQ(0, tmp_mount.FS_stat->num_inodes);
 }

/* End of the test case for the function change_mount_stat */

