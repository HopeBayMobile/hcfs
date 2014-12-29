#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "utils.h"
#include "global.h"
#include "fuseop.h"
#include "params.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

// Tests non-existing file
TEST(check_file_size, Nonexist) {

  char temp[20];

  strcpy(temp,"nonexist");

  EXPECT_EQ(-1, check_file_size(temp));
}

TEST(check_file_size, Test_8_bytes) {

  EXPECT_EQ(8,check_file_size("testpatterns/size8bytes"));
}

/* Begin of the test case for the function fetch_meta_path */

class MetaPathParamTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    free(METAPATH);
   }

 };

TEST_F(MetaPathParamTest, NullMetaPath) {
  char *tempptr;
  char pathname[METAPATHLEN];

  tempptr = METAPATH;
  METAPATH = NULL;

  EXPECT_EQ(-1,fetch_meta_path(pathname,0));

  METAPATH = tempptr;
 }
TEST_F(MetaPathParamTest, MetaPathNotCreatable) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = rmdir(METAPATH);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod("/tmp/testmeta", 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-1,fetch_meta_path(pathname,0));

  chmod("/tmp/testmeta",0700);
  rmdir(METAPATH);
 }
TEST_F(MetaPathParamTest, MetaPathNotAccessible) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)<0)
   {
    ret_code = mkdir(METAPATH, 0400);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(METAPATH, 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-1,fetch_meta_path(pathname,0));

  rmdir(METAPATH);
 }
TEST_F(MetaPathParamTest, MkMetaPathSuccess) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = rmdir(METAPATH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,fetch_meta_path(pathname,0));

  EXPECT_STREQ("/tmp/testmeta/metapath/sub_0/meta0",pathname);

  rmdir("/tmp/testmeta/metapath/sub_0");
  rmdir(METAPATH);
 }
TEST_F(MetaPathParamTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);

  ASSERT_EQ(0,fetch_meta_path(pathname,NUMSUBDIR));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_0/meta%lld", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_meta_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%lld/meta%lld", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%lld", NUMSUBDIR-1);
  rmdir(expected_pathname);
  rmdir("/tmp/testmeta/metapath/sub_0");
  rmdir(METAPATH);
 }

/* End of the test case for the function fetch_meta_path */

/* Begin of the test case for the function fetch_todelete_path */


/* End of the test case for the function fetch_todelete_path */

