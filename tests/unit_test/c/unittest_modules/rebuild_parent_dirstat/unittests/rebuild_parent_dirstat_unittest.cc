/* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved. */
#include "rebuild_parent_dirstat_unittest.h"

#include <errno.h>

extern "C" {
#include "rebuild_parent_dirstat.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "parent_lookup.h"
#include "dir_statistics.h"
#include "path_reconstruct.h"
}
#include "gtest/gtest.h"

#define UNUSED(x) ((void)x)

SYSTEM_CONF_STRUCT *system_config;

/* Begin of the test case for the function rebuild_parent_stat */

class rebuild_parent_statTest : public ::testing::Test {
 protected:
  int32_t count;
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;
    hcfs_system->system_restoring = TRUE;
    sem_init(&(hcfs_system->fuse_sem), 0, 0);
    sem_init(&(hcfs_system->something_to_replace), 0, 0);
    sem_init(&(pathlookup_data_lock), 0, 1);

    fake_num_parents = 0;
    dirstat_lookup_data_fptr = fopen(MOCK_DIRSTAT_PATH, "w+");
    pathlookup_data_fptr = fopen(MOCK_PATHLOOKUP_PATH, "w+");
  }

  virtual void TearDown() {
    free(hcfs_system);
    fclose(dirstat_lookup_data_fptr);
    fclose(pathlookup_data_fptr);
    unlink(MOCK_DIRSTAT_PATH);
    unlink(MOCK_PATHLOOKUP_PATH);
   }
  void BuildMockPathlookup() {
    PRIMARY_PARENT_T tmpparent;
    off_t filepos;

    tmpparent.haveothers = FALSE;
    tmpparent.parentinode = FAKE_ROOT;
    filepos = (off_t) ((FAKE_GRAND_PARENT - 1) *
               sizeof(PRIMARY_PARENT_T));
    pwrite(fileno(pathlookup_data_fptr), &tmpparent,
          sizeof(PRIMARY_PARENT_T), filepos);

    tmpparent.parentinode = FAKE_GRAND_PARENT;
    filepos = (off_t) ((FAKE_EXIST_PARENT - 1) *
               sizeof(PRIMARY_PARENT_T));
    pwrite(fileno(pathlookup_data_fptr), &tmpparent,
          sizeof(PRIMARY_PARENT_T), filepos);
   }
  void fetch_meta_path(char *pathname, ino_t this_inode) {
    sprintf(pathname, "%s_%" PRIu64 "", MOCK_META_PATH,
            (uint64_t)this_inode);
   }

 };

/* Test the case when system not restoring */
TEST_F(rebuild_parent_statTest, NotRestoring) {
  int32_t ret;
  hcfs_system->system_restoring = FALSE;
  ret = rebuild_parent_stat(3, 2, D_ISDIR);
  ASSERT_EQ(0, ret);
  ASSERT_EQ(FALSE, hcfs_system->system_restoring);
 }

/* Test the case when inode number is zero */
TEST_F(rebuild_parent_statTest, InodeNumZero) {
  ASSERT_EQ(-EINVAL, rebuild_parent_stat(0, 2, D_ISDIR));
  ASSERT_EQ(-EINVAL, rebuild_parent_stat(2, 0, D_ISDIR));
 }

/* Test the case when parent is already added */
TEST_F(rebuild_parent_statTest, ParentAddedAlready) {
  fake_num_parents = 1;
  ASSERT_EQ(0, rebuild_parent_stat(ONE_PARENT_INO, FAKE_EXIST_PARENT, D_ISDIR));
  EXPECT_EQ(1, fake_num_parents);
 }

/* Rebuild non-file / non-dir objects */
TEST_F(rebuild_parent_statTest, NonFileDirObjects) {
  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, 2, D_ISLNK));
  EXPECT_EQ(1, fake_num_parents);
  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, 2, D_ISFIFO));
  EXPECT_EQ(1, fake_num_parents);
  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, 2, D_ISSOCK));
  EXPECT_EQ(1, fake_num_parents);
 }

/* Rebuild dir objects */
TEST_F(rebuild_parent_statTest, DirObjects) {
  DIR_STATS_TYPE tmpstat;
  off_t filepos;

  memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
  tmpstat.num_local = 10;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;

  /* Write wrong data */
  filepos = (off_t) ((NO_PARENT_INO - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, 2, D_ISDIR));
  EXPECT_EQ(1, fake_num_parents);
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(0, tmpstat.num_local);
  EXPECT_EQ(0, tmpstat.num_cloud);
  EXPECT_EQ(0, tmpstat.num_hybrid);
 }

/* Rebuild file object, no local meta */
TEST_F(rebuild_parent_statTest, FileObjectNoMeta) {
  DIR_STATS_TYPE tmpstat;
  off_t filepos;

  memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
  /* Write old data */
  tmpstat.num_local = 10;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 15;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 20;
  tmpstat.num_cloud = 15;
  tmpstat.num_hybrid = 13;
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  BuildMockPathlookup();

  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, FAKE_EXIST_PARENT, D_ISREG));
  EXPECT_EQ(1, fake_num_parents);
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(10, tmpstat.num_local);
  EXPECT_EQ(11, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(15, tmpstat.num_local);
  EXPECT_EQ(11, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(20, tmpstat.num_local);
  EXPECT_EQ(16, tmpstat.num_cloud);
  EXPECT_EQ(13, tmpstat.num_hybrid);
 }

/* Rebuild file object, have local meta, file in cloud */
TEST_F(rebuild_parent_statTest, FileObjectHaveMetaCloud) {
  DIR_STATS_TYPE tmpstat;
  off_t filepos;
  FILE *fptr;
  FILE_STATS_TYPE tmpmetastat;
  char tmpmetapath[50];

  fetch_meta_path(tmpmetapath, NO_PARENT_INO);
  /* Write mock meta file */
  fptr = fopen(tmpmetapath, "w+");
  tmpmetastat.num_blocks = 10;
  tmpmetastat.num_cached_blocks = 0;
  pwrite(fileno(fptr), &tmpmetastat, sizeof(FILE_STATS_TYPE),
         sizeof(struct stat) + sizeof(FILE_META_TYPE));
  fclose(fptr);

  memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
  /* Write old data */
  tmpstat.num_local = 10;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 15;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 20;
  tmpstat.num_cloud = 15;
  tmpstat.num_hybrid = 13;
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  BuildMockPathlookup();

  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, FAKE_EXIST_PARENT, D_ISREG));
  EXPECT_EQ(1, fake_num_parents);
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(10, tmpstat.num_local);
  EXPECT_EQ(11, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(15, tmpstat.num_local);
  EXPECT_EQ(11, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(20, tmpstat.num_local);
  EXPECT_EQ(16, tmpstat.num_cloud);
  EXPECT_EQ(13, tmpstat.num_hybrid);
  unlink(tmpmetapath);
 }

/* Rebuild file object, have local meta, file in local */
TEST_F(rebuild_parent_statTest, FileObjectHaveMetaLocal) {
  DIR_STATS_TYPE tmpstat;
  off_t filepos;
  FILE *fptr;
  FILE_STATS_TYPE tmpmetastat;
  char tmpmetapath[50];

  fetch_meta_path(tmpmetapath, NO_PARENT_INO);
  /* Write mock meta file */
  fptr = fopen(tmpmetapath, "w+");
  tmpmetastat.num_blocks = 10;
  tmpmetastat.num_cached_blocks = 10;
  pwrite(fileno(fptr), &tmpmetastat, sizeof(FILE_STATS_TYPE),
         sizeof(struct stat) + sizeof(FILE_META_TYPE));
  fclose(fptr);

  memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
  /* Write old data */
  tmpstat.num_local = 10;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 15;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 20;
  tmpstat.num_cloud = 15;
  tmpstat.num_hybrid = 13;
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  BuildMockPathlookup();

  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, FAKE_EXIST_PARENT, D_ISREG));
  EXPECT_EQ(1, fake_num_parents);
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(11, tmpstat.num_local);
  EXPECT_EQ(10, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(16, tmpstat.num_local);
  EXPECT_EQ(10, tmpstat.num_cloud);
  EXPECT_EQ(10, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(21, tmpstat.num_local);
  EXPECT_EQ(15, tmpstat.num_cloud);
  EXPECT_EQ(13, tmpstat.num_hybrid);
  unlink(tmpmetapath);
 }

/* Rebuild file object, have local meta, file is hybrid */
TEST_F(rebuild_parent_statTest, FileObjectHaveMetaHybrid) {
  DIR_STATS_TYPE tmpstat;
  off_t filepos;
  FILE *fptr;
  FILE_STATS_TYPE tmpmetastat;
  char tmpmetapath[50];

  fetch_meta_path(tmpmetapath, NO_PARENT_INO);
  /* Write mock meta file */
  fptr = fopen(tmpmetapath, "w+");
  tmpmetastat.num_blocks = 10;
  tmpmetastat.num_cached_blocks = 5;
  pwrite(fileno(fptr), &tmpmetastat, sizeof(FILE_STATS_TYPE),
         sizeof(struct stat) + sizeof(FILE_META_TYPE));
  fclose(fptr);

  memset(&tmpstat, 0, sizeof(DIR_STATS_TYPE));
  /* Write old data */
  tmpstat.num_local = 10;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 15;
  tmpstat.num_cloud = 10;
  tmpstat.num_hybrid = 10;
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  tmpstat.num_local = 20;
  tmpstat.num_cloud = 15;
  tmpstat.num_hybrid = 13;
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pwrite(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);

  BuildMockPathlookup();

  fake_num_parents = 0;
  ASSERT_EQ(0, rebuild_parent_stat(NO_PARENT_INO, FAKE_EXIST_PARENT, D_ISREG));
  EXPECT_EQ(1, fake_num_parents);
  filepos = (off_t) ((FAKE_EXIST_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(10, tmpstat.num_local);
  EXPECT_EQ(10, tmpstat.num_cloud);
  EXPECT_EQ(11, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_GRAND_PARENT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(15, tmpstat.num_local);
  EXPECT_EQ(10, tmpstat.num_cloud);
  EXPECT_EQ(11, tmpstat.num_hybrid);
  filepos = (off_t) ((FAKE_ROOT - 1) * sizeof(DIR_STATS_TYPE));
  pread(fileno(dirstat_lookup_data_fptr), &tmpstat,
         sizeof(DIR_STATS_TYPE), filepos);
  EXPECT_EQ(20, tmpstat.num_local);
  EXPECT_EQ(15, tmpstat.num_cloud);
  EXPECT_EQ(14, tmpstat.num_hybrid);
  unlink(tmpmetapath);
 }

/* End of the test case for the function rebuild_parent_stat */


