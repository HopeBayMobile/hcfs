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
#include "FS_manager.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

/* Begin of the test case for the function init_fs_manager */

class init_fs_managerTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);
    snprintf(tmpmgrpath, 100, "%s/fsmgr", METAPATH);
   }

  virtual void TearDown() {
    if (fs_mgr_head != NULL) {
      if (fs_mgr_head->FS_list_fh >= 0)
        close(fs_mgr_head->FS_list_fh);
      free(fs_mgr_head);
     }
    if (fs_mgr_path != NULL)
      free(fs_mgr_path);
    unlink(tmpmgrpath);
    rmdir(METAPATH);
    rmdir("/tmp/testHCFS");
    free(METAPATH);
    free(hcfs_system);
   }

 };

TEST_F(init_fs_managerTest, InitFSManager) {
  int ret, lock_val;
  off_t testlen;

  if (access(tmpmgrpath, F_OK) == 0)
    unlink(tmpmgrpath);

  ret = init_fs_manager();
  ASSERT_EQ(ret, 0);
  EXPECT_STREQ(tmpmgrpath, fs_mgr_path);
  if (fs_mgr_head == NULL)
    ret = 1;
  else
    ret = 0;
  EXPECT_EQ(0, ret);
  sem_getvalue(&(fs_mgr_head->op_lock), &lock_val);
  EXPECT_EQ(1, lock_val);
  ret = access(tmpmgrpath, F_OK);
  EXPECT_EQ(0, ret);
  ASSERT_GE(fs_mgr_head->FS_list_fh, -1);
  testlen = lseek(fs_mgr_head->FS_list_fh, 0, SEEK_END);
  EXPECT_EQ(sizeof(DIR_META_TYPE), testlen);
 }

TEST_F(init_fs_managerTest, OpenFSManager) {
  int ret, lock_val;
  off_t testlen;
  FILE *fptr;
  DIR_META_TYPE tmp_head;

  if (access(tmpmgrpath, F_OK) == 0)
    unlink(tmpmgrpath);
  fptr = fopen(tmpmgrpath, "a+");
  fwrite(&tmp_head, sizeof(DIR_META_TYPE), 1, fptr);
  fclose(fptr);

  ret = init_fs_manager();
  ASSERT_EQ(ret, 0);
  EXPECT_STREQ(tmpmgrpath, fs_mgr_path);
  if (fs_mgr_head == NULL)
    ret = 1;
  else
    ret = 0;
  EXPECT_EQ(0, ret);
  sem_getvalue(&(fs_mgr_head->op_lock), &lock_val);
  EXPECT_EQ(1, lock_val);
  ret = access(tmpmgrpath, F_OK);
  EXPECT_EQ(0, ret);
  ASSERT_GE(fs_mgr_head->FS_list_fh, -1);
  testlen = lseek(fs_mgr_head->FS_list_fh, 0, SEEK_END);
  EXPECT_EQ(sizeof(DIR_META_TYPE), testlen);
 }

/* End of the test case for the function init_fs_manager */

/* Begin of the test case for the function add_filesystem */

class add_filesystemTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  FILE *fptr;
  DIR_META_TYPE tmp_head;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);
    snprintf(tmpmgrpath, 100, "%s/fsmgr", METAPATH);
    if (access(tmpmgrpath, F_OK) == 0)
      unlink(tmpmgrpath);

   }

  virtual void TearDown() {
    if (fs_mgr_head != NULL) {
      if (fs_mgr_head->FS_list_fh >= 0)
        close(fs_mgr_head->FS_list_fh);
      free(fs_mgr_head);
     }
    if (fs_mgr_path != NULL)
      free(fs_mgr_path);
    unlink(tmpmgrpath);
    rmdir(METAPATH);
    rmdir("/tmp/testHCFS");
    free(METAPATH);
    free(hcfs_system);
   }

 };

TEST_F(add_filesystemTest, AddOneFS) {
  int ret, lock_val;
  DIR_ENTRY tmp_entry;

  ret = init_fs_manager();
  ASSERT_EQ(ret, 0);

  ret = add_filesystem("testFS", &tmp_entry);
  ASSERT_EQ(ret, 0);

 }
/* End of the test case for the function add_filesystem */

