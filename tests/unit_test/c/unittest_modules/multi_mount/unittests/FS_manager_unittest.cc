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
#include "mount_manager.h"
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
  char tmpbackuppath[100];
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);
    snprintf(tmpmgrpath, 100, "%s/fsmgr", METAPATH);
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);
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
    unlink(tmpbackuppath);
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
  EXPECT_EQ(sizeof(DIR_META_TYPE) + 16, testlen);
 }

TEST_F(init_fs_managerTest, OpenFSManager) {
  int ret, lock_val;
  off_t testlen;
  FILE *fptr;
  DIR_META_TYPE tmp_head;
  unsigned char testuuid[20], verifyuuid[16];

  snprintf((char *)testuuid, 20, "1234567890abcdef");
  if (access(tmpmgrpath, F_OK) == 0)
    unlink(tmpmgrpath);
  fptr = fopen(tmpmgrpath, "a+");
  fwrite(testuuid, 1, 16, fptr);
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
  EXPECT_EQ(sizeof(DIR_META_TYPE) + 16, testlen);
  lseek(fs_mgr_head->FS_list_fh, 0, SEEK_SET);
  read(fs_mgr_head->FS_list_fh, verifyuuid, 16);
  ret = strncmp((char *) verifyuuid, (char *) testuuid, 16);
  EXPECT_EQ(0, ret);
 }

/* End of the test case for the function init_fs_manager */

/* Begin of the test case for the function destroy_fs_manager */
class destroy_fs_managerTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  char tmpbackuppath[100];
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    METAPATH = (char *)malloc(sizeof(char)*100);
    snprintf(METAPATH, 100, "/tmp/testHCFS/metapath");
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    if (access(METAPATH, F_OK) != 0)
      mkdir(METAPATH, 0700);
    snprintf(tmpmgrpath, 100, "%s/fsmgr", METAPATH);
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);
   }

  virtual void TearDown() {
    unlink(tmpmgrpath);
    unlink(tmpbackuppath);
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
  char tmpsyncpath[100];
  char tmpbackuppath[100];

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
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

    treesplit = FALSE;
    entry_in_database = FALSE;
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    mkdir(tmpsyncpath, 0700);
   }

  virtual void TearDown() {
    if (fs_mgr_head != NULL) {
      if (fs_mgr_head->FS_list_fh >= 0)
        close(fs_mgr_head->FS_list_fh);
      free(fs_mgr_head);
     }
    if (fs_mgr_path != NULL)
      free(fs_mgr_path);
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    rmdir(tmpsyncpath);
    unlink(tmpmgrpath);
    unlink(tmpbackuppath);
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

#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);

  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);

  EXPECT_EQ(1, fs_mgr_head->num_FS);
  EXPECT_EQ(2, tmp_entry.d_ino);
  EXPECT_STREQ("testFS", tmp_entry.d_name);
  retsize = pread(fs_mgr_head->FS_list_fh, &tmpmeta, sizeof(DIR_META_TYPE), 16);
  EXPECT_EQ(sizeof(DIR_META_TYPE), retsize);
  EXPECT_EQ(sizeof(DIR_META_TYPE) + 16, tmpmeta.root_entry_page);
  EXPECT_EQ(sizeof(DIR_META_TYPE) + 16, tmpmeta.tree_walk_list_head);
  EXPECT_EQ(1, tmpmeta.total_children);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE), sizeof(DIR_META_TYPE) + 16);
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

#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS1", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS1", &tmp_entry);
#endif
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS2", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS2", &tmp_entry);
#endif
  if (ret != 0)
    unlink(tmppath);
  ASSERT_EQ(0, ret);
  EXPECT_EQ(4, tmp_entry.d_ino);
  EXPECT_STREQ("testFS2", tmp_entry.d_name);
  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);
  unlink(tmppath);


  EXPECT_EQ(3, fs_mgr_head->num_FS);
  retsize = pread(fs_mgr_head->FS_list_fh, &tmpmeta, sizeof(DIR_META_TYPE), 16);
  EXPECT_EQ(sizeof(DIR_META_TYPE), retsize);
  EXPECT_EQ(sizeof(DIR_META_TYPE)+2*sizeof(DIR_ENTRY_PAGE) + 16,
      tmpmeta.root_entry_page);
  EXPECT_EQ(sizeof(DIR_META_TYPE)+2*sizeof(DIR_ENTRY_PAGE) + 16,
      tmpmeta.tree_walk_list_head);
  EXPECT_EQ(3, tmpmeta.total_children);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE), sizeof(DIR_META_TYPE) + 16);
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(2, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS", tmppage.dir_entries[0].d_name);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE),
			sizeof(DIR_META_TYPE)+sizeof(DIR_ENTRY_PAGE) + 16);
  EXPECT_EQ(sizeof(DIR_ENTRY_PAGE), retsize);
  EXPECT_EQ(1, tmppage.num_entries);
  EXPECT_EQ(4, tmppage.dir_entries[0].d_ino);
  EXPECT_STREQ("testFS2", tmppage.dir_entries[0].d_name);

  retsize = pread(fs_mgr_head->FS_list_fh, &tmppage,
			sizeof(DIR_ENTRY_PAGE),
			sizeof(DIR_META_TYPE)+ 2*sizeof(DIR_ENTRY_PAGE) + 16);
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

#ifdef _ANDROID_ENV_
  ret = add_filesystem(verylongname, ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem(verylongname, &tmp_entry);
#endif
  EXPECT_EQ(-ENAMETOOLONG, ret);
 }

/* End of the test case for the function add_filesystem */

/* Begin of the test case for the function delete_filesystem */

class delete_filesystemTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  char tmpsyncpath[100];
  char tmpbackuppath[100];
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
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    mkdir(tmpsyncpath, 0700);
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

    treesplit = FALSE;
    sem_init(&(mount_mgr.mount_lock), 0, 1);

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
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    rmdir(tmpsyncpath);
    unlink(tmpbackuppath);
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
  char tmpsyncpath[100];
  char tmpbackuppath[100];
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
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

    treesplit = FALSE;
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    mkdir(tmpsyncpath, 0700);

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
    snprintf(tmpsyncpath, 100, "%s/FS_sync", METAPATH);
    rmdir(tmpsyncpath);
    unlink(tmpbackuppath);
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
#ifdef _ANDROID_ENV_
  ret = add_filesystem((char*)"testFS", ANDROID_INTERNAL, &tmp_entry);
#else
  ret = add_filesystem((char*)"testFS", &tmp_entry);
#endif
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
  char tmpbackuppath[100];
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
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

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
    unlink(tmpbackuppath);
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

TEST_F(list_filesystemTest, MoreFSthanBuf) {
  int ret;
  DIR_ENTRY tmp_entry[10];
  unsigned long retval;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  fs_mgr_head->num_FS = 20;

  ret = list_filesystem(10, tmp_entry, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(20, retval);
 }  

TEST_F(list_filesystemTest, ReturnFSnumOnly) {
  int ret;
  DIR_ENTRY tmp_entry[10];
  unsigned long retval;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 10;

  ret = list_filesystem(0, NULL, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(10, retval);
 }  

TEST_F(list_filesystemTest, ListFSOneNode) {
  int ret;
  DIR_ENTRY tmp_entry[10];
  unsigned long retval;
  DIR_META_TYPE tmphead;
  DIR_ENTRY_PAGE tmppage;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 10;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 10;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE) + 16;

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 16);
  memset(&tmppage, 0, sizeof(DIR_ENTRY_PAGE));
  tmppage.num_entries = 10;
  for (count = 0; count < 10; count++) {
    tmppage.dir_entries[count].d_ino = 5 + count;
    snprintf(tmppage.dir_entries[count].d_name, MAX_FILENAME_LEN,
		"FS_%d", count);
   }
  pwrite(fs_mgr_head->FS_list_fh, &tmppage, sizeof(DIR_ENTRY_PAGE),
		sizeof(DIR_META_TYPE) + 16);

  ret = list_filesystem(10, tmp_entry, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(10, retval);
  for (count = 0; count < 10; count++) {
    EXPECT_EQ(5+count, tmp_entry[count].d_ino);
    EXPECT_STREQ(tmppage.dir_entries[count].d_name, tmp_entry[count].d_name);
   }
 }  

TEST_F(list_filesystemTest, ListFSTwoNodes) {
  int ret;
  DIR_ENTRY tmp_entry[20];
  unsigned long retval;
  DIR_META_TYPE tmphead;
  DIR_ENTRY_PAGE tmppage, tmppage2;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 20;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 20;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE) + 16;

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 16);

  memset(&tmppage, 0, sizeof(DIR_ENTRY_PAGE));
  tmppage.num_entries = 10;
  for (count = 0; count < 10; count++) {
    tmppage.dir_entries[count].d_ino = 5 + count;
    snprintf(tmppage.dir_entries[count].d_name, MAX_FILENAME_LEN,
		"FS_%d", count);
   }
  tmppage.tree_walk_next = sizeof(DIR_META_TYPE) + sizeof(DIR_ENTRY_PAGE) + 16;
  pwrite(fs_mgr_head->FS_list_fh, &tmppage, sizeof(DIR_ENTRY_PAGE),
		sizeof(DIR_META_TYPE) + 16);

  memset(&tmppage2, 0, sizeof(DIR_ENTRY_PAGE));
  tmppage2.num_entries = 10;
  for (count = 0; count < 10; count++) {
    tmppage2.dir_entries[count].d_ino = 100 + count;
    snprintf(tmppage2.dir_entries[count].d_name, MAX_FILENAME_LEN,
		"FS_%d", count);
   }
  tmppage2.tree_walk_prev = sizeof(DIR_META_TYPE) + 16;
  pwrite(fs_mgr_head->FS_list_fh, &tmppage2, sizeof(DIR_ENTRY_PAGE),
		sizeof(DIR_META_TYPE)+sizeof(DIR_ENTRY_PAGE) + 16);

  ret = list_filesystem(20, tmp_entry, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(20, retval);
  for (count = 0; count < 10; count++) {
    EXPECT_EQ(5+count, tmp_entry[count].d_ino);
    EXPECT_STREQ(tmppage.dir_entries[count].d_name, tmp_entry[count].d_name);
   }
  for (count = 0; count < 0; count++) {
    EXPECT_EQ(100+count, tmp_entry[count+10].d_ino);
    EXPECT_STREQ(tmppage2.dir_entries[count].d_name,
			tmp_entry[count+10].d_name);
   }
 }  

TEST_F(list_filesystemTest, RecomputeNum) {
  int ret;
  DIR_ENTRY tmp_entry[20];
  unsigned long retval;
  DIR_META_TYPE tmphead;
  DIR_ENTRY_PAGE tmppage, tmppage2;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 10;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 7;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE) + 16;

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 16);

  memset(&tmppage, 0, sizeof(DIR_ENTRY_PAGE));
  tmppage.num_entries = 10;
  for (count = 0; count < 10; count++) {
    tmppage.dir_entries[count].d_ino = 5 + count;
    snprintf(tmppage.dir_entries[count].d_name, MAX_FILENAME_LEN,
		"FS_%d", count);
   }
  tmppage.tree_walk_next = sizeof(DIR_META_TYPE) + sizeof(DIR_ENTRY_PAGE) + 16;
  pwrite(fs_mgr_head->FS_list_fh, &tmppage, sizeof(DIR_ENTRY_PAGE),
		sizeof(DIR_META_TYPE) + 16);

  memset(&tmppage2, 0, sizeof(DIR_ENTRY_PAGE));
  tmppage2.num_entries = 10;
  for (count = 0; count < 10; count++) {
    tmppage2.dir_entries[count].d_ino = 100 + count;
    snprintf(tmppage2.dir_entries[count].d_name, MAX_FILENAME_LEN,
		"FS_%d", count);
   }
  tmppage2.tree_walk_prev = sizeof(DIR_META_TYPE) + 16;
  pwrite(fs_mgr_head->FS_list_fh, &tmppage2, sizeof(DIR_ENTRY_PAGE),
		sizeof(DIR_META_TYPE)+sizeof(DIR_ENTRY_PAGE) + 16);

  ret = list_filesystem(20, tmp_entry, &retval);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(20, retval);
  for (count = 0; count < 10; count++) {
    EXPECT_EQ(5+count, tmp_entry[count].d_ino);
    EXPECT_STREQ(tmppage.dir_entries[count].d_name, tmp_entry[count].d_name);
   }
  for (count = 0; count < 0; count++) {
    EXPECT_EQ(100+count, tmp_entry[count+10].d_ino);
    EXPECT_STREQ(tmppage2.dir_entries[count].d_name,
			tmp_entry[count+10].d_name);
   }

  EXPECT_EQ(20, fs_mgr_head->num_FS);
  pread(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 16);
  EXPECT_EQ(20, tmphead.total_children);
 }  

/* End of the test case for the function list_filesystem */

/* Begin of the test case for the function backup_FS_database */

class backup_FS_databaseTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  char tmpbackuppath[100];
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
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

    treesplit = FALSE;
    failedcurlinit = FALSE;
    failedput = FALSE;
    CURRENT_BACKEND = SWIFT;
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
    unlink(tmpbackuppath);
    rmdir(METAPATH);
    rmdir("/tmp/testHCFS");
    free(METAPATH);
    free(hcfs_system);
   }
 };

TEST_F(backup_FS_databaseTest, CannotCreate) {
  int ret;
  FILE *tmpfptr;

  mknod("/tmp/FSmgr_upload", S_IFREG | 0400, 0);

  ret = init_fs_manager();
  EXPECT_EQ(0, ret);
  tmpfptr = fopen(tmpbackuppath, "w");
  fprintf(tmpfptr, "test data\n");
  fclose(tmpfptr);
  ret = backup_FS_database();
  EXPECT_EQ(-EACCES, ret);
  unlink("/tmp/FSmgr_upload");
 }

TEST_F(backup_FS_databaseTest, UploadDone) {
  int ret, errcode;
  DIR_META_TYPE tmphead;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 20;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 20;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE);

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 0);
  ret = prepare_FS_database_backup();
  EXPECT_EQ(0, ret);

  ret = backup_FS_database();
  EXPECT_EQ(0, ret);
  errcode = 0;
  ret = access("/tmp/FSmgr_upload", F_OK);
  if (ret < 0)
    errcode = errno;
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errcode);
  if (ret == 0)
    unlink("/tmp/FSmgr_upload");
  EXPECT_EQ(tmphead.total_children, headbuf.total_children);
  EXPECT_EQ(tmphead.tree_walk_list_head, headbuf.tree_walk_list_head);

 }

/* Init backend is done in get object op now */
/*
TEST_F(backup_FS_databaseTest, FailedInit) {
  int ret, errcode;
  DIR_META_TYPE tmphead;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 20;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 20;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE);

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 0);
  failedcurlinit = TRUE;

  ret = prepare_FS_database_backup();
  EXPECT_EQ(0, ret);

  ret = backup_FS_database();
  EXPECT_EQ(-EIO, ret);

  errcode = 0;
  ret = access("/tmp/FSmgr_upload", F_OK);
  if (ret < 0)
    errcode = errno;
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errcode);
  if (ret == 0)
    unlink("/tmp/FSmgr_upload");

 }
*/
TEST_F(backup_FS_databaseTest, FailedPut) {
  int ret, errcode;
  DIR_META_TYPE tmphead;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);
  fs_mgr_head->num_FS = 20;

  memset(&tmphead, 0, sizeof(DIR_META_TYPE));
  tmphead.total_children = 20;
  tmphead.tree_walk_list_head = sizeof(DIR_META_TYPE);

  pwrite(fs_mgr_head->FS_list_fh, &tmphead, sizeof(DIR_META_TYPE), 0);
  failedput = TRUE;
  ret = prepare_FS_database_backup();
  EXPECT_EQ(0, ret);

  ret = backup_FS_database();
  EXPECT_EQ(-EIO, ret);

  errcode = 0;
  ret = access("/tmp/FSmgr_upload", F_OK);
  if (ret < 0)
    errcode = errno;
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errcode);
  if (ret == 0)
    unlink("/tmp/FSmgr_upload");

 }

/* End of the test case for the function backup_FS_database */

/* Begin of the test case for the function restore_FS_database */

class restore_FS_databaseTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  char tmpbackuppath[100];
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
    snprintf(tmpbackuppath, sizeof(tmpbackuppath), "%s/fsmgr_upload",
             METAPATH);

    treesplit = FALSE;
    failedcurlinit = FALSE;
    failedget = FALSE;
    failedput = FALSE;

   }

  virtual void TearDown() {
    unlink(tmpmgrpath);
    unlink(tmpbackuppath);
    rmdir(METAPATH);
    rmdir("/tmp/testHCFS");
    free(METAPATH);
    free(hcfs_system);
   }
 };

TEST_F(restore_FS_databaseTest, CannotCreate) {
  int ret;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  if (fs_mgr_head != NULL) {
    if (fs_mgr_head->FS_list_fh >= 0)
      close(fs_mgr_head->FS_list_fh);
    free(fs_mgr_head);
   }

  chmod(tmpmgrpath, 0400);

  ret = restore_FS_database();
  EXPECT_EQ(-EACCES, ret);
  chmod(fs_mgr_path, 0700);
  if (fs_mgr_path != NULL)
    free(fs_mgr_path);
 }
/* Init backend is done in get object op now */
/*
TEST_F(restore_FS_databaseTest, CurlInitFailed) {
  int ret;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  if (fs_mgr_head != NULL) {
    if (fs_mgr_head->FS_list_fh >= 0)
      close(fs_mgr_head->FS_list_fh);
    free(fs_mgr_head);
   }

  failedcurlinit = TRUE;
  ret = restore_FS_database();
  EXPECT_EQ(-EIO, ret);

  if (fs_mgr_path != NULL)
    free(fs_mgr_path);
 }
*/

TEST_F(restore_FS_databaseTest, CurlGetFailed) {
  int ret;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  if (fs_mgr_head != NULL) {
    if (fs_mgr_head->FS_list_fh >= 0)
      close(fs_mgr_head->FS_list_fh);
    free(fs_mgr_head);
   }

  failedget = TRUE;
  ret = restore_FS_database();
  EXPECT_EQ(-EIO, ret);

  if (fs_mgr_path != NULL)
    free(fs_mgr_path);
 }

TEST_F(restore_FS_databaseTest, DownloadDone) {
  int ret;

  ret = init_fs_manager();
  ASSERT_EQ(0, ret);

  if (fs_mgr_head != NULL) {
    if (fs_mgr_head->FS_list_fh >= 0)
      close(fs_mgr_head->FS_list_fh);
    free(fs_mgr_head);
   }

  ret = restore_FS_database();
  EXPECT_EQ(0, ret);

  if (fs_mgr_path != NULL)
    free(fs_mgr_path);
 }

/* End of the test case for the function restore_FS_database */

