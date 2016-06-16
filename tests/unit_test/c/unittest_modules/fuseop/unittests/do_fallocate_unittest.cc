#include <errno.h>

extern "C" {
#include "do_fallocate.h"
#include "global.h"
#include "params.h"
#include "utils.h"
#include "meta_mem_cache.h"
}
#include "gtest/gtest.h"

#include "fake_misc.h"

/* Begin of the test case for the function do_fallocate */

class do_fallocateTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    system_config->max_cache_limit =
        (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
    system_config->max_pinned_limit =
        (int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
    before_update_file_data = TRUE;
    root_updated = FALSE;
    fake_block_status = ST_NONE;
    after_update_block_page = FALSE;
    hcfs_system->systemdata.system_quota = 102400000;
    hcfs_system->systemdata.system_size = 12800000;
    hcfs_system->systemdata.cache_size = 1200000;
    hcfs_system->systemdata.cache_blocks = 13;
    hcfs_system->systemdata.pinned_size = 10000;
    system_config->max_cache_limit[1] = 1024768000;
    system_config->max_pinned_limit[1] = 1024768000 * 0.8;
    sem_init(&(hcfs_system->access_sem), 0, 1);
  }

  virtual void TearDown() {
    free(hcfs_system);
  }
};

TEST_F(do_fallocateTest, WrongMode) {
  int32_t ret;
  fuse_req_t req1;

  ret = do_fallocate(10, NULL, 4, 0, 0, NULL, req1);
  ASSERT_EQ(-ENOTSUP, ret);
}
TEST_F(do_fallocateTest, NoExtend) {
  int32_t ret;
  fuse_req_t req1;
  struct stat tempstat;
  META_CACHE_ENTRY_STRUCT *tmpptr;

  tempstat.st_size = 1024;
  tempstat.st_mode = S_IFREG;
  ret = do_fallocate(14, &tempstat, 0, 0, 10, &tmpptr, req1);
  ASSERT_EQ(0, ret);

  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}

TEST_F(do_fallocateTest, Extend) {
  int32_t ret;
  fuse_req_t req1;
  struct stat tempstat;
  META_CACHE_ENTRY_STRUCT *tmpptr;

  tempstat.st_size = 1024;
  tempstat.st_mode = S_IFREG;
  ret = do_fallocate(14, &tempstat, 0, 0, 1024768, &tmpptr, req1);
  ASSERT_EQ(0, ret);

  EXPECT_EQ(tempstat.st_size, 1024768);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000 + 1024768 - 1024);
}

TEST_F(do_fallocateTest, ExceedPinSize) {
  int32_t ret;
  fuse_req_t req1;
  struct stat tempstat;
  META_CACHE_ENTRY_STRUCT *tmpptr;

  hcfs_system->systemdata.pinned_size = 10000;
  system_config->max_cache_limit[1] = 1024768;
  system_config->max_pinned_limit[1] = 1024768 * 0.8;
  tempstat.st_size = 1024;
  tempstat.st_mode = S_IFREG;
  ret = do_fallocate(14, &tempstat, 0, 0, 1024768, &tmpptr, req1);
  ASSERT_EQ(-ENOSPC, ret);

  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}

TEST_F(do_fallocateTest, ExceedSystemQuota) {
  int32_t ret;
  fuse_req_t req1;
  struct stat tempstat;
  META_CACHE_ENTRY_STRUCT *tmpptr;

  hcfs_system->systemdata.system_quota = 12800000 - 1;
  tempstat.st_size = 1024;
  tempstat.st_mode = S_IFREG;
  ret = do_fallocate(14, &tempstat, 0, 0, 1024700000, &tmpptr, req1);
  ASSERT_EQ(-ENOSPC, ret);

  EXPECT_EQ(tempstat.st_size, 1024);
  EXPECT_EQ(hcfs_system->systemdata.system_size, 12800000);
}

/* End of the test case for the function do_fallocate */
