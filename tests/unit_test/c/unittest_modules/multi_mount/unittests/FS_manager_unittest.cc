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
#include "FS_manager_unittest.h"
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

/* Begin of the test case for the function destroy_fs_manager */
class destroy_fs_managerTest : public ::testing::Test {
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
    unlink(tmpmgrpath);
    rmdir(METAPATH);
    rmdir("/tmp/testHCFS");
    free(METAPATH);
    free(hcfs_system);
   }

 };

TEST_F(destroy_fs_managerTest, Runtest) {

  int ret;

  if (access(tmpmgrpath, F_OK) == 0)
    unlink(tmpmgrpath);
  init_fs_manager();
  destroy_fs_manager();
  if (fs_mgr_head == NULL)
    ret = 1;
  else
    ret = 0;
  EXPECT_EQ(1, ret);
  if (fs_mgr_path == NULL)
    ret = 1;
  else
    ret = 0;
  EXPECT_EQ(1, ret);
 }

/* End of the test case for the function destroy_fs_manager */

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
    treesplit = FALSE;
    entry_in_database = FALSE;
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
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  DIR_ENTRY_PAGE tmppage;
  ssize_t retsize;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);

  EXPECT_EQ(1, fs_mgr_head->num_FS);
  EXPECT_EQ(2, tmp_entry.d_ino);
  EXPECT_STREQ("testFS", tmp_entry.d_name);
  retsize = pread(fs_mgr_head->FS_list_fh, &tmpmeta, sizeof(DIR_META_TYPE), 0);
  EXPECT_EQ(sizeof(DIR_META_TYPE), retsize);
  EXPECT_EQ(sizeof(DIR_META_TYPE), tmpmeta.root_entry_page);
  EXPECT_EQ(sizeof(DIR_META_TYPE), tmpmeta.tree_walk_list_head);
  EXPECT_EQ(1, tmpmeta.total_children);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE), sizeof(DIR_META_TYPE));
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(2, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS", tmppage.dir_entries[0].d_name);

  unlink(tmppath);
 }

TEST_F(add_filesystemTest, AddThreeFSSplit) {
  int ret, lock_val;
  DIR_ENTRY tmp_entry;
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  DIR_ENTRY_PAGE tmppage;
  ssize_t retsize;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(2, tmp_entry.d_ino);
  EXPECT_STREQ("testFS", tmp_entry.d_name);
  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);
  unlink(tmppath);

  fakeino = 3;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);
  ret = add_filesystem("testFS1", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(3, tmp_entry.d_ino);
  EXPECT_STREQ("testFS1", tmp_entry.d_name);
  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);
  unlink(tmppath);

  /* Split the nodes */
  treesplit = TRUE;
  fakeino = 4;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);
  ret = add_filesystem("testFS2", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(4, tmp_entry.d_ino);
  EXPECT_STREQ("testFS2", tmp_entry.d_name);
  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);
  unlink(tmppath);


  EXPECT_EQ(3, fs_mgr_head->num_FS);
  retsize = pread(fs_mgr_head->FS_list_fh, &tmpmeta, sizeof(DIR_META_TYPE), 0);
  EXPECT_EQ(sizeof(DIR_META_TYPE), retsize);
  EXPECT_EQ(sizeof(DIR_META_TYPE)+2*sizeof(DIR_ENTRY_PAGE),
      tmpmeta.root_entry_page);
  EXPECT_EQ(sizeof(DIR_META_TYPE)+2*sizeof(DIR_ENTRY_PAGE),
      tmpmeta.tree_walk_list_head);
  EXPECT_EQ(3, tmpmeta.total_children);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE), sizeof(DIR_META_TYPE));
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(2, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS", tmppage.dir_entries[0].d_name);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE),
			sizeof(DIR_META_TYPE)+sizeof(DIR_ENTRY_PAGE));
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(4, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS2", tmppage.dir_entries[0].d_name);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE),
			sizeof(DIR_META_TYPE)+ 2*sizeof(DIR_ENTRY_PAGE));
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(3, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS1", tmppage.dir_entries[0].d_name);

 }
TEST_F(add_filesystemTest, NameTooLong) {
  int ret;
  char verylongname[300];
  int count;
  DIR_ENTRY tmp_entry;

  for (count = 0; count < 290; count++)
    verylongname[count] = '0' + (count % 10);
  verylongname[290] = 0;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = add_filesystem(verylongname, &tmp_entry);
  EXPECT_EQ(-ENAMETOOLONG, ret);
 }

/* End of the test case for the function add_filesystem */

/* Begin of the test case for the function delete_filesystem */

class delete_filesystemTest : public ::testing::Test {
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
    treesplit = FALSE;

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

TEST_F(delete_filesystemTest, NoFSDelete) {
  int ret;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = delete_filesystem("testFS");
  EXPECT_EQ(-ENOENT, ret);
 }
TEST_F(delete_filesystemTest, NameTooLong) {
  int ret;
  char verylongname[300];
  int count;

  for (count = 0; count < 290; count++)
    verylongname[count] = '0' + (count % 10);
  verylongname[290] = 0;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = delete_filesystem(verylongname);
  EXPECT_EQ(-ENAMETOOLONG, ret);
 }
TEST_F(delete_filesystemTest, NoRootMeta) {
  int ret, lock_val;
  DIR_ENTRY tmp_entry;
  ssize_t retsize;
  char tmppath[100];

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);
  unlink(tmppath);

  entry_in_database = TRUE;
  ret = delete_filesystem("testFS");
  EXPECT_EQ(-ENOENT, ret);
 }
TEST_F(delete_filesystemTest, RootNotEmpty) {
  int ret, lock_val;
  DIR_ENTRY tmp_entry;
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  ssize_t retsize;
  FILE *fptr;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  fptr = fopen(tmppath, "r+");
  if (fptr == NULL)
    ret = -1;
  else
    ret = 0;
  ASSERT_EQ(0, ret);
  fseek(fptr, sizeof(struct stat), SEEK_SET);
  fread(&tmpmeta, sizeof(DIR_META_TYPE), 1, fptr);
  tmpmeta.total_children = 1;
  fseek(fptr, sizeof(struct stat), SEEK_SET);
  fwrite(&tmpmeta, sizeof(DIR_META_TYPE), 1, fptr);
  fclose(fptr);

  entry_in_database = TRUE;
  ret = delete_filesystem("testFS");
  EXPECT_EQ(-ENOTEMPTY, ret);
  unlink(tmppath);
 }

TEST_F(delete_filesystemTest, DeleteOneFS) {
  int ret, lock_val, errcode;
  DIR_ENTRY tmp_entry;
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  ssize_t retsize;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  entry_in_database = TRUE;
  ret = delete_filesystem("testFS");
  EXPECT_EQ(0, ret);
  errcode = 0;
  ret = access(tmppath, F_OK);
  errcode = errno;
  EXPECT_EQ(ENOENT, errcode);
  EXPECT_EQ(0, fs_mgr_head->num_FS);
 }

/* End of the test case for the function delete_filesystem */

/* Begin of the test case for the function check_filesystem */

class check_filesystemTest : public ::testing::Test {
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
    treesplit = FALSE;

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

TEST_F(check_filesystemTest, NameTooLong) {
  int ret;
  char verylongname[300];
  int count;
  DIR_ENTRY tmp_entry;

  for (count = 0; count < 290; count++)
    verylongname[count] = '0' + (count % 10);
  verylongname[290] = 0;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = check_filesystem(verylongname, &tmp_entry);
  EXPECT_EQ(-ENAMETOOLONG, ret);
 }
TEST_F(check_filesystemTest, NoFSCheck) {
  int ret;
  DIR_ENTRY tmp_entry;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = check_filesystem("testFS", &tmp_entry);
  EXPECT_EQ(-ENOENT, ret);
 }

TEST_F(check_filesystemTest, FSFound) {
  int ret, lock_val, errcode;
  DIR_ENTRY tmp_entry;
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  ssize_t retsize;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  entry_in_database = TRUE;
  ret = check_filesystem("testFS", &tmp_entry);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(fakeino, tmp_entry.d_ino);

  unlink(tmppath);
 }

TEST_F(check_filesystemTest, FSNotFound) {
  int ret, lock_val, errcode;
  DIR_ENTRY tmp_entry;
  char tmppath[100];
  DIR_META_TYPE tmpmeta;
  ssize_t retsize;

  fakeino = 2;
  snprintf(tmppath, 100, "%s/meta%ld", METAPATH, fakeino);

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = add_filesystem("testFS", &tmp_entry);
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  entry_in_database = FALSE;
  ret = check_filesystem("testFS2", &tmp_entry);
  EXPECT_EQ(-ENOENT, ret);

  unlink(tmppath);
 }

/* End of the test case for the function check_filesystem */

/* Begin of the test case for the function list_filesystem */

class list_filesystemTest : public ::testing::Test {
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
    treesplit = FALSE;

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

TEST_F(list_filesystemTest, NoFS) {
  int ret;
  DIR_ENTRY tmp_entry[10];
  unsigned long retval;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  ret = list_filesystem(10, tmp_entry, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(0, retval);
 }  

/* End of the test case for the function check_filesystem */

