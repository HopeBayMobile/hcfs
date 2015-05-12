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
#include "hfuse_system.h"
#include "params.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

// Tests non-existing file
TEST(check_file_sizeTest, Nonexist) {

  char temp[20];

  strcpy(temp,"nonexist");

  EXPECT_EQ(-1, check_file_size(temp));
}

TEST(check_file_sizeTest, Test_8_bytes) {

  EXPECT_EQ(8,check_file_size("testpatterns/size8bytes"));
}

/* Begin of the test case for the function fetch_meta_path */

class fetch_meta_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    rmdir("/tmp/testmeta");
    free(METAPATH);
   }

 };

TEST_F(fetch_meta_pathTest, NullMetaPath) {
  char *tempptr;
  char pathname[METAPATHLEN];

  tempptr = METAPATH;
  METAPATH = NULL;

  EXPECT_EQ(-1,fetch_meta_path(pathname,0));

  METAPATH = tempptr;
 }
TEST_F(fetch_meta_pathTest, MetaPathNotCreatable) {
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
TEST_F(fetch_meta_pathTest, MetaPathNotAccessible) {
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
TEST_F(fetch_meta_pathTest, MkMetaPathSuccess) {
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
TEST_F(fetch_meta_pathTest, SubDirMod) {
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

class fetch_todelete_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    rmdir("/tmp/testmeta");
    free(METAPATH);
   }

 };

TEST_F(fetch_todelete_pathTest, NullMetaPath) {
  char *tempptr;
  char pathname[METAPATHLEN];

  tempptr = METAPATH;
  METAPATH = NULL;

  EXPECT_EQ(-1,fetch_todelete_path(pathname,0));

  METAPATH = tempptr;
 }
TEST_F(fetch_todelete_pathTest, ToDeletePathNotCreatable) {
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

  EXPECT_EQ(-1,fetch_todelete_path(pathname,0));

  chmod("/tmp/testmeta",0700);

  ret_code = mkdir(METAPATH, 0400);
  ASSERT_EQ(0,ret_code);

  EXPECT_EQ(-1,fetch_todelete_path(pathname,0));

  rmdir(METAPATH);
 }
TEST_F(fetch_todelete_pathTest, ToDeletePathNotAccessible) {
  char pathname[METAPATHLEN];
  char todelete_path[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)<0)
   {
    ret_code = mkdir(METAPATH, 0700);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(METAPATH, 0700);

  ASSERT_EQ(ret_code,0);

  sprintf(todelete_path,"%s/todelete",METAPATH);
  if (access(todelete_path,F_OK)<0)
   {
    ret_code = mkdir(todelete_path, 0400);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(todelete_path, 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-1,fetch_todelete_path(pathname,0));

  rmdir(todelete_path);
  rmdir(METAPATH);
 }
TEST_F(fetch_todelete_pathTest, MkMetaPathSuccess) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = rmdir(METAPATH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,fetch_todelete_path(pathname,0));

  EXPECT_STREQ("/tmp/testmeta/metapath/todelete/sub_0/meta0",pathname);

  rmdir("/tmp/testmeta/metapath/todelete/sub_0");
  rmdir("/tmp/testmeta/metapath/todelete");
  rmdir(METAPATH);
 }
TEST_F(fetch_todelete_pathTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);

  ASSERT_EQ(0,fetch_todelete_path(pathname,NUMSUBDIR));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_0/meta%lld", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_todelete_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%lld/meta%lld", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%lld", NUMSUBDIR-1);
  rmdir(expected_pathname);
  rmdir("/tmp/testmeta/metapath/todelete/sub_0");
  rmdir("/tmp/testmeta/metapath/todelete");
  rmdir(METAPATH);
 }


/* End of the test case for the function fetch_todelete_path */


/* Begin of the test case for the function fetch_block_path */

class fetch_block_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    BLOCKPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(BLOCKPATH, "/tmp/testmeta/blockpath");
   }

  virtual void TearDown() {
    rmdir("/tmp/testmeta");
    free(BLOCKPATH);
   }

 };

TEST_F(fetch_block_pathTest, NullBlockPath) {
  char *tempptr;
  char pathname[METAPATHLEN];

  tempptr = BLOCKPATH;
  BLOCKPATH = NULL;

  EXPECT_EQ(-1,fetch_block_path(pathname,0,0));

  BLOCKPATH = tempptr;
 }
TEST_F(fetch_block_pathTest, BlockPathNotCreatable) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)==0)
   {
    ret_code = rmdir(BLOCKPATH);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod("/tmp/testmeta", 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-1,fetch_block_path(pathname,0,0));

  chmod("/tmp/testmeta",0700);
  rmdir(BLOCKPATH);
 }
TEST_F(fetch_block_pathTest, BlockPathNotAccessible) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)<0)
   {
    ret_code = mkdir(BLOCKPATH, 0400);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(BLOCKPATH, 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-1,fetch_block_path(pathname,0,0));

  rmdir(BLOCKPATH);
 }
TEST_F(fetch_block_pathTest, MkBlockPathSuccess) {
  char pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)==0)
   {
    ret_code = rmdir(BLOCKPATH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,fetch_block_path(pathname,0,0));

  EXPECT_STREQ("/tmp/testmeta/blockpath/sub_0/block0_0",pathname);

  rmdir("/tmp/testmeta/blockpath/sub_0");
  rmdir(BLOCKPATH);
 }
TEST_F(fetch_block_pathTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int ret_code;

  ASSERT_STREQ("/tmp/testmeta/blockpath", BLOCKPATH);

  ASSERT_EQ(0,fetch_block_path(pathname,NUMSUBDIR,0));

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_0/block%lld_0", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_block_path(pathname, 0, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%lld/block0_%lld", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%lld", NUMSUBDIR-1);
  rmdir(expected_pathname);
  rmdir("/tmp/testmeta/blockpath/sub_0");
  rmdir(BLOCKPATH);
 }

/* End of the test case for the function fetch_meta_path */

/* Begin of the test case for the function parse_parent_self */
class parse_parent_selfTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
     pathname=(char *)malloc(400);
     parentname=(char *)malloc(400);
     selfname=(char *)malloc(400);
   }

  virtual void TearDown() {
     free(pathname);
     free(parentname);
     free(selfname);
   }
  char *pathname;
  char *parentname;
  char *selfname;
 };
TEST_F(parse_parent_selfTest, NullPointers) {
  char *temp;

  temp=pathname;
  pathname=NULL;
  ASSERT_EQ(-1,parse_parent_self(pathname,parentname,selfname));
  pathname=temp;

  temp=parentname;
  parentname=NULL;
  ASSERT_EQ(-1,parse_parent_self(pathname,parentname,selfname));
  parentname=temp;

  temp=selfname;
  selfname=NULL;
  ASSERT_EQ(-1,parse_parent_self(pathname,parentname,selfname));
  selfname=temp;

 }
TEST_F(parse_parent_selfTest, InvalidPath) {
  strcpy(pathname,"relativepath");
  ASSERT_EQ(-1,parse_parent_self(pathname,parentname,selfname));

  strcpy(pathname,"/");
  ASSERT_EQ(-1,parse_parent_self(pathname,parentname,selfname));
 }
TEST_F(parse_parent_selfTest, ObjectsUnderRoot) {
  strcpy(pathname,"/testfile");

  ASSERT_EQ(0,parse_parent_self(pathname,parentname,selfname));

  EXPECT_STREQ("/",parentname);
  EXPECT_STREQ("testfile",selfname);

  strcpy(pathname,"/testfolder/");

  ASSERT_EQ(0,parse_parent_self(pathname,parentname,selfname));

  EXPECT_STREQ("/",parentname);
  EXPECT_STREQ("testfolder",selfname);
 }

TEST_F(parse_parent_selfTest, ObjectsNotUnderRoot) {
  strcpy(pathname,"/testdir/testfile");

  ASSERT_EQ(0,parse_parent_self(pathname,parentname,selfname));

  EXPECT_STREQ("/testdir",parentname);
  EXPECT_STREQ("testfile",selfname);
  strcpy(pathname,"/testdir/testsubdir/");

  ASSERT_EQ(0,parse_parent_self(pathname,parentname,selfname));

  EXPECT_STREQ("/testdir",parentname);
  EXPECT_STREQ("testsubdir",selfname);
 }

/* End of the test case for the function parse_parent_self */

/* Start of the test case for the function read_system_config */

TEST(read_system_configTest, PathNotExist) {
  EXPECT_EQ(-1,read_system_config("EmptyFile"));
 }

TEST(read_system_configTest, GoodConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  EXPECT_STREQ(METAPATH, "/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/testHCFS/blockstorage");
  EXPECT_STREQ(SUPERBLOCK, "/testHCFS/metastorage/superblock");
  EXPECT_STREQ(UNCLAIMEDFILE, "/testHCFS/metastorage/unclaimedlist");
  EXPECT_STREQ(HCFSSYSTEM, "/testHCFS/metastorage/hcfssystemfile");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
 }
TEST(read_system_configTest, WrongNumbersConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_soft_limit_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname));

 }

/* End of the test case for the function read_system_config */

