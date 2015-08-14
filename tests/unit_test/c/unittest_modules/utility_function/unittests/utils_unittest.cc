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

  EXPECT_EQ(-ENOENT, check_file_size(temp));
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

  EXPECT_EQ(-EPERM,fetch_meta_path(pathname,0));

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

  EXPECT_EQ(-EACCES,fetch_meta_path(pathname,0));

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

  EXPECT_EQ(-EACCES,fetch_meta_path(pathname,0));

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

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_0/meta%d", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_meta_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%d/meta%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%d", NUMSUBDIR-1);
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

  EXPECT_EQ(-EPERM,fetch_todelete_path(pathname,0));

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

  EXPECT_EQ(-EACCES,fetch_todelete_path(pathname,0));

  chmod("/tmp/testmeta",0700);

  ret_code = mkdir(METAPATH, 0400);
  ASSERT_EQ(0,ret_code);

  EXPECT_EQ(-EACCES,fetch_todelete_path(pathname,0));

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

  EXPECT_EQ(-EACCES,fetch_todelete_path(pathname,0));

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

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_0/meta%d", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_todelete_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%d/meta%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%d", NUMSUBDIR-1);
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

  EXPECT_EQ(-EACCES,fetch_block_path(pathname,0,0));

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

  EXPECT_EQ(-EACCES,fetch_block_path(pathname,0,0));

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

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_0/block%d_0", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_block_path(pathname, 0, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%d/block0_%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%d", NUMSUBDIR-1);
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

class read_system_configTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    if (access("/tmp/testHCFS", F_OK) != 0)
      mkdir("/tmp/testHCFS", 0700);
    mkdir("/tmp/testHCFS/metastorage", 0700);
    mkdir("/tmp/testHCFS/blockstorage", 0700);
   }

  virtual void TearDown() {
    rmdir("/tmp/testHCFS/metastorage");
    rmdir("/tmp/testHCFS/blockstorage");
    rmdir("/tmp/testHCFS");
   }
 };

TEST_F(read_system_configTest, PathNotExist) {
  EXPECT_EQ(-1,read_system_config("EmptyFile"));
 }
TEST_F(read_system_configTest, GoodConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
  EXPECT_EQ(CURRENT_BACKEND, SWIFT);
  EXPECT_STREQ(SWIFT_ACCOUNT,"hopebay");
  EXPECT_STREQ(SWIFT_USER, "hopebay");
  EXPECT_STREQ(SWIFT_PASS, "hopebaycloud");
  EXPECT_STREQ(SWIFT_URL, "1.1.1.1:8080");
  EXPECT_STREQ(SWIFT_CONTAINER, "hopebay_private_container");
  EXPECT_STREQ(SWIFT_PROTOCOL, "https");
  EXPECT_STREQ(S3_ACCESS, "aaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_STREQ(S3_SECRET, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  EXPECT_STREQ(S3_URL, "s3.hicloud.net.tw");
  EXPECT_STREQ(S3_BUCKET, "hopebay");
  EXPECT_STREQ(S3_PROTOCOL, "https");
  EXPECT_STREQ(S3_BUCKET_URL, "https://hopebay.s3.hicloud.net.tw");
 }
TEST_F(read_system_configTest, GoodS3Config) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
  EXPECT_EQ(CURRENT_BACKEND, S3);
  EXPECT_STREQ(SWIFT_ACCOUNT,"hopebay");
  EXPECT_STREQ(SWIFT_USER, "hopebay");
  EXPECT_STREQ(SWIFT_PASS, "hopebaycloud");
  EXPECT_STREQ(SWIFT_URL, "1.1.1.1:8080");
  EXPECT_STREQ(SWIFT_CONTAINER, "hopebay_private_container");
  EXPECT_STREQ(SWIFT_PROTOCOL, "https");
  EXPECT_STREQ(S3_ACCESS, "aaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_STREQ(S3_SECRET, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  EXPECT_STREQ(S3_URL, "s3.hicloud.net.tw");
  EXPECT_STREQ(S3_BUCKET, "hopebay");
  EXPECT_STREQ(S3_PROTOCOL, "https");
  EXPECT_STREQ(S3_BUCKET_URL, "https://hopebay.s3.hicloud.net.tw");
 }
TEST_F(read_system_configTest, UnsupportedBackendConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_hcfs_nosupport_backend.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname));
 }
TEST_F(read_system_configTest, UnsupportedProtocol) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_protocol.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname));
 }
TEST_F(read_system_configTest, UnsupportedProtocolS3) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_protocol_s3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname));
 }
TEST_F(read_system_configTest, OptionsTooLong) {
  char pathname[100];

  strcpy(pathname,"testpatterns/options_too_long.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname));
 }
TEST_F(read_system_configTest, WrongNumbersConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_soft_limit_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname));

  strcpy(pathname,"testpatterns/test_wrong_hard_limit_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname));

  strcpy(pathname,"testpatterns/test_wrong_delta_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname));

  strcpy(pathname,"testpatterns/test_wrong_block_size_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname));

 }

/* End of the test case for the function read_system_config*/

/* Start of the test case for the function validate_system_config */

class validate_system_configTest : public ::testing::Test {
 protected:
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
    mkdir("/tmp/testHCFS/metastorage", 0700);
    mkdir("/tmp/testHCFS/blockstorage", 0700);
   }

  virtual void TearDown() {
    rmdir("/tmp/testHCFS/metastorage");
    rmdir("/tmp/testHCFS/blockstorage");
    unlink("/tmp/testHCFS");
    rmdir(tmppath);
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);
   }
 };

TEST_F(validate_system_configTest, PathNotExist) {
  EXPECT_EQ(-1,read_system_config("EmptyFile"));
 }
TEST_F(validate_system_configTest, GoodConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  ASSERT_EQ(0,validate_system_config());

  EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
  EXPECT_EQ(CURRENT_BACKEND, SWIFT);
 }
TEST_F(validate_system_configTest, GoodS3Config) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  ASSERT_EQ(0,validate_system_config());

  EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
  EXPECT_EQ(CURRENT_BACKEND, S3);
 }
TEST_F(validate_system_configTest, NoBackendConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_hcfs_no_backend.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  ASSERT_EQ(-1,validate_system_config());
 }
TEST_F(validate_system_configTest, NoMetaPath) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  rmdir(METAPATH);

  ASSERT_EQ(-1,validate_system_config());
 }
TEST_F(validate_system_configTest, NoBlockPath) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  rmdir(BLOCKPATH);

  ASSERT_EQ(-1,validate_system_config());
 }
TEST_F(validate_system_configTest, InvalidValue) {
  char pathname[100];
  long long tmpval;

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  tmpval = MAX_BLOCK_SIZE;
  MAX_BLOCK_SIZE = 0;
  EXPECT_EQ(-1,validate_system_config());
  MAX_BLOCK_SIZE = tmpval;

  tmpval = CACHE_DELTA;
  CACHE_DELTA = MAX_BLOCK_SIZE - 1;
  EXPECT_EQ(-1,validate_system_config());
  CACHE_DELTA = tmpval;

  tmpval = CACHE_SOFT_LIMIT;
  CACHE_SOFT_LIMIT = MAX_BLOCK_SIZE - 1;
  EXPECT_EQ(-1,validate_system_config());
  CACHE_SOFT_LIMIT = tmpval;

  tmpval = CACHE_HARD_LIMIT;
  CACHE_HARD_LIMIT = CACHE_SOFT_LIMIT - 1;
  EXPECT_EQ(-1,validate_system_config());
  CACHE_HARD_LIMIT = tmpval;

 }
TEST_F(validate_system_configTest, MissingSWIFTConfig) {
  char pathname[100];
  char *tmpptr;

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  tmpptr = SWIFT_ACCOUNT;
  SWIFT_ACCOUNT = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_ACCOUNT = tmpptr;

  tmpptr = SWIFT_USER;
  SWIFT_USER = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_USER = tmpptr;

  tmpptr = SWIFT_PASS;
  SWIFT_PASS = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_PASS = tmpptr;

  tmpptr = SWIFT_URL;
  SWIFT_URL = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_URL = tmpptr;

  tmpptr = SWIFT_CONTAINER;
  SWIFT_CONTAINER = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_CONTAINER = tmpptr;

  tmpptr = SWIFT_PROTOCOL;
  SWIFT_PROTOCOL = NULL;
  EXPECT_EQ(-1,validate_system_config());
  SWIFT_PROTOCOL = tmpptr;

 }

TEST_F(validate_system_configTest, MissingS3Config) {
  char pathname[100];
  char *tmpptr;

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname));

  tmpptr = S3_ACCESS;
  S3_ACCESS = NULL;
  EXPECT_EQ(-1,validate_system_config());
  S3_ACCESS = tmpptr;

  tmpptr = S3_SECRET;
  S3_SECRET = NULL;
  EXPECT_EQ(-1,validate_system_config());
  S3_SECRET = tmpptr;

  tmpptr = S3_URL;
  S3_URL = NULL;
  EXPECT_EQ(-1,validate_system_config());
  S3_URL = tmpptr;

  tmpptr = S3_BUCKET;
  S3_BUCKET = NULL;
  EXPECT_EQ(-1,validate_system_config());
  S3_BUCKET = tmpptr;

  tmpptr = S3_PROTOCOL;
  S3_PROTOCOL = NULL;
  EXPECT_EQ(-1,validate_system_config());
  S3_PROTOCOL = tmpptr;

 }
/* End of the test case for the function validate_system_config*/

