#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>
extern "C" {
#include "utils.h"
#include "global.h"
#include "hfuse_system.h"
#include "params.h"
#include "fuseop.h"
#include "mount_manager.h"
#include "super_block.h"
}
#include "gtest/gtest.h"
#include "mock_params.h"

extern SYSTEM_DATA_HEAD *hcfs_system;
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

// Tests non-existing file
TEST(check_file_sizeTest, Nonexist) {

  char temp[20];

  strcpy(temp,"nonexist");

  EXPECT_EQ(-ENOENT, check_file_size(temp));
}

TEST(check_file_sizeTest, Test_8_bytes) {

  EXPECT_EQ(4096, check_file_size("testpatterns/size8bytes"));
}

/* Begin of the test case for the function check_and_create_metapaths */

class check_and_create_metapathsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    nftw("/tmp/testmeta", do_delete, 20, FTW_DEPTH);
    free(METAPATH);
    free(system_config);
   }

 };

TEST_F(check_and_create_metapathsTest, MetaPathNotCreatable) {
  char pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod("/tmp/testmeta", 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-EACCES,check_and_create_metapaths());

  chmod("/tmp/testmeta",0700);
  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(check_and_create_metapathsTest, MetaPathNotAccessible) {
  char pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)<0)
   {
    ret_code = mkdir(METAPATH, 0400);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(METAPATH, 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-EACCES,check_and_create_metapaths());

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(check_and_create_metapathsTest, MkMetaPathSuccess) {
  char pathname[METAPATHLEN];
  int32_t ret_code, i;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,check_and_create_metapaths());

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }

/* End of the test case for the function check_and_create_metapaths */

/* Begin of the test case for the function check_and_create_blockpaths */

class check_and_create_blockpathsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    BLOCKPATH = (char *)malloc(BLOCKPATHLEN);
    mkdir("/tmp/testblock",0700);
    strcpy(BLOCKPATH, "/tmp/testblock/blockpath");
   }

  virtual void TearDown() {
    nftw("/tmp/testblock", do_delete, 20, FTW_DEPTH);
    free(BLOCKPATH);
    free(system_config);
   }

 };

TEST_F(check_and_create_blockpathsTest, BlockPathNotCreatable) {
  char pathname[BLOCKPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testblock/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)==0)
   {
    ret_code = nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod("/tmp/testblock", 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-EACCES,check_and_create_blockpaths());

  chmod("/tmp/testblock",0700);
  nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(check_and_create_blockpathsTest, BlockPathNotAccessible) {
  char pathname[BLOCKPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testblock/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)<0)
   {
    ret_code = mkdir(BLOCKPATH, 0400);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod(BLOCKPATH, 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-EACCES,check_and_create_blockpaths());

  nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(check_and_create_blockpathsTest, MkBlockPathSuccess) {
  char pathname[BLOCKPATHLEN];
  int32_t ret_code, i;

  ASSERT_STREQ("/tmp/testblock/blockpath", BLOCKPATH);
  ret_code = 0;
  if (access(BLOCKPATH,F_OK)==0)
   {
    ret_code = nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,check_and_create_blockpaths());

  nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
 }

/* End of the test case for the function check_and_create_blockpaths */

/* Begin of the test case for the function fetch_meta_path */

class fetch_meta_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    nftw("/tmp/testmeta", do_delete, 20, FTW_DEPTH);
    free(METAPATH);
    free(system_config);
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
TEST_F(fetch_meta_pathTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);

  ASSERT_EQ(0,fetch_meta_path(pathname,NUMSUBDIR));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_0/meta%d", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_meta_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%d/meta%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/sub_%d", NUMSUBDIR-1);
  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }

/* End of the test case for the function fetch_meta_path */

/* Begin of the test case for the function fetch_todelete_path */

class fetch_todelete_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    METAPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(METAPATH, "/tmp/testmeta/metapath");
   }

  virtual void TearDown() {
    nftw("/tmp/testmeta", do_delete, 20, FTW_DEPTH);
    free(METAPATH);
    free(system_config);
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
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }
  ret_code = chmod("/tmp/testmeta", 0400);

  ASSERT_EQ(ret_code,0);

  EXPECT_EQ(-EACCES,fetch_todelete_path(pathname,0));

  chmod("/tmp/testmeta",0700);

  ret_code = mkdir(METAPATH, 0400);
  ASSERT_EQ(0,ret_code);

  EXPECT_EQ(-EACCES,fetch_todelete_path(pathname,0));

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(fetch_todelete_pathTest, ToDeletePathNotAccessible) {
  char pathname[METAPATHLEN];
  char todelete_path[METAPATHLEN];
  int32_t ret_code;

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

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(fetch_todelete_pathTest, MkMetaPathSuccess) {
  char pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);
  ret_code = 0;
  if (access(METAPATH,F_OK)==0)
   {
    ret_code = nftw(METAPATH, do_delete, 20, FTW_DEPTH);
    ASSERT_EQ(0,ret_code);
   }

  ASSERT_EQ(0,fetch_todelete_path(pathname,0));

  EXPECT_STREQ("/tmp/testmeta/metapath/todelete/sub_0/meta0",pathname);

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }
TEST_F(fetch_todelete_pathTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/metapath", METAPATH);

  ASSERT_EQ(0,fetch_todelete_path(pathname,NUMSUBDIR));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_0/meta%d", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_todelete_path(pathname, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%d/meta%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/metapath/todelete/sub_%d", NUMSUBDIR-1);
  nftw(METAPATH, do_delete, 20, FTW_DEPTH);
 }


/* End of the test case for the function fetch_todelete_path */


/* Begin of the test case for the function fetch_block_path */

class fetch_block_pathTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    BLOCKPATH = (char *)malloc(METAPATHLEN);
    mkdir("/tmp/testmeta",0700);
    strcpy(BLOCKPATH, "/tmp/testmeta/blockpath");
   }

  virtual void TearDown() {
    nftw("/tmp/testmeta", do_delete, 20, FTW_DEPTH);
    free(BLOCKPATH);
    free(system_config);
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
TEST_F(fetch_block_pathTest, SubDirMod) {
  char pathname[METAPATHLEN];
  char expected_pathname[METAPATHLEN];
  int32_t ret_code;

  ASSERT_STREQ("/tmp/testmeta/blockpath", BLOCKPATH);

  ASSERT_EQ(0,fetch_block_path(pathname,NUMSUBDIR,0));

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_0/block%d_0", NUMSUBDIR);
  EXPECT_STREQ(expected_pathname,pathname);

  ASSERT_EQ(0,fetch_block_path(pathname, 0, (2*NUMSUBDIR)-1));

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%d/block0_%d", NUMSUBDIR-1,(2*NUMSUBDIR)-1);

  EXPECT_STREQ(expected_pathname,pathname);

  sprintf(expected_pathname,"/tmp/testmeta/blockpath/sub_%d", NUMSUBDIR-1);
  nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);
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
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
   }

  virtual void TearDown() {
    nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
    free(system_config);
   }
 };

TEST_F(read_system_configTest, PathNotExist) {
  EXPECT_EQ(-1,read_system_config("EmptyFile", system_config));
 }
TEST_F(read_system_configTest, GoodConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
  EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
  EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
  EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
  EXPECT_EQ(CACHE_DELTA, 10485760);
  EXPECT_EQ(META_SPACE_LIMIT, 1234567);
  EXPECT_EQ(RESERVED_CACHE_SPACE, 53687091);
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

  ASSERT_EQ(0,read_system_config(pathname, system_config));

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

  ASSERT_EQ(-1,read_system_config(pathname, system_config));
 }
TEST_F(read_system_configTest, UnsupportedProtocol) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_protocol.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname, system_config));
 }
TEST_F(read_system_configTest, UnsupportedProtocolS3) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_protocol_s3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname, system_config));
 }
TEST_F(read_system_configTest, OptionsTooLong) {
  char pathname[100];

  strcpy(pathname,"testpatterns/options_too_long.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(-1,read_system_config(pathname, system_config));
 }
TEST_F(read_system_configTest, WrongNumbersConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_wrong_soft_limit_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname, system_config));

  strcpy(pathname,"testpatterns/test_wrong_hard_limit_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname, system_config));

  strcpy(pathname,"testpatterns/test_wrong_delta_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname, system_config));

  strcpy(pathname,"testpatterns/test_wrong_block_size_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  EXPECT_EQ(-1,read_system_config(pathname, system_config));

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
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
   }

  virtual void TearDown() {
    unlink("/tmp/testHCFS");
    if (tmppath)
        nftw(tmppath, do_delete, 20, FTW_DEPTH);
    free(workpath);
    free(tmppath);
    free(system_config);
   }
 };

TEST_F(validate_system_configTest, PathNotExist) {
  EXPECT_EQ(-1,read_system_config("EmptyFile", system_config));
 }
TEST_F(validate_system_configTest, GoodConfig) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  ASSERT_EQ(0,validate_system_config(system_config));

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

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  ASSERT_EQ(0,validate_system_config(system_config));

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

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  ASSERT_EQ(-1,validate_system_config(system_config));
 }
TEST_F(validate_system_configTest, NoMetaPath) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  nftw(METAPATH, do_delete, 20, FTW_DEPTH);

  ASSERT_EQ(-1,validate_system_config(system_config));
 }
TEST_F(validate_system_configTest, NoBlockPath) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  nftw(BLOCKPATH, do_delete, 20, FTW_DEPTH);

  ASSERT_EQ(-1,validate_system_config(system_config));
 }
TEST_F(validate_system_configTest, InvalidValue) {
  char pathname[100];
  int64_t tmpval;

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  tmpval = MAX_BLOCK_SIZE;
  MAX_BLOCK_SIZE = 0;
  EXPECT_EQ(-1,validate_system_config(system_config));
  MAX_BLOCK_SIZE = tmpval;

  tmpval = CACHE_DELTA;
  CACHE_DELTA = MAX_BLOCK_SIZE - 1;
  EXPECT_EQ(-1,validate_system_config(system_config));
  CACHE_DELTA = tmpval;

  tmpval = CACHE_SOFT_LIMIT;
  CACHE_SOFT_LIMIT = MAX_BLOCK_SIZE - 1;
  EXPECT_EQ(-1,validate_system_config(system_config));
  CACHE_SOFT_LIMIT = tmpval;

  tmpval = CACHE_HARD_LIMIT;
  CACHE_HARD_LIMIT = CACHE_SOFT_LIMIT - 1;
  EXPECT_EQ(-1,validate_system_config(system_config));
  CACHE_HARD_LIMIT = tmpval;

 }
TEST_F(validate_system_configTest, MissingSWIFTConfig) {
  char pathname[100];
  char *tmpptr;

  strcpy(pathname,"testpatterns/test_good_hcfs.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  tmpptr = SWIFT_ACCOUNT;
  SWIFT_ACCOUNT = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_ACCOUNT = tmpptr;

  tmpptr = SWIFT_USER;
  SWIFT_USER = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_USER = tmpptr;

  tmpptr = SWIFT_PASS;
  SWIFT_PASS = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_PASS = tmpptr;

  tmpptr = SWIFT_URL;
  SWIFT_URL = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_URL = tmpptr;

  tmpptr = SWIFT_CONTAINER;
  SWIFT_CONTAINER = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_CONTAINER = tmpptr;

  tmpptr = SWIFT_PROTOCOL;
  SWIFT_PROTOCOL = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  SWIFT_PROTOCOL = tmpptr;

 }

TEST_F(validate_system_configTest, MissingS3Config) {
  char pathname[100];
  char *tmpptr;

  strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  tmpptr = S3_ACCESS;
  S3_ACCESS = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  S3_ACCESS = tmpptr;

  tmpptr = S3_SECRET;
  S3_SECRET = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  S3_SECRET = tmpptr;

  tmpptr = S3_URL;
  S3_URL = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  S3_URL = tmpptr;

  tmpptr = S3_BUCKET;
  S3_BUCKET = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  S3_BUCKET = tmpptr;

  tmpptr = S3_PROTOCOL;
  S3_PROTOCOL = NULL;
  EXPECT_EQ(-1,validate_system_config(system_config));
  S3_PROTOCOL = tmpptr;

 }

TEST_F(validate_system_configTest, NoReservedCacheSection) {
  char pathname[100];

  strcpy(pathname,"testpatterns/test_no_reserved_section.conf");

  ASSERT_EQ(0,access(pathname, F_OK));

  ASSERT_EQ(0,read_system_config(pathname, system_config));

  ASSERT_EQ(0,validate_system_config(system_config));

  EXPECT_EQ(RESERVED_CACHE_SPACE, system_config->max_block_size);
 }
/* End of the test case for the function validate_system_config*/

/* Unittest for reload_system_config */
class reload_system_configTest : public ::testing::Test {
protected:
	char *workpath, *tmppath;
	void SetUp()
	{
		hcfs_system =
			(SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		hcfs_system->system_restoring = NOT_RESTORING;
		workpath = NULL;
		tmppath = NULL;
		if (access("/tmp/testHCFS", F_OK) != 0) {
			workpath = get_current_dir_name();
			tmppath = (char *)malloc(strlen(workpath)+20);
			snprintf(tmppath, strlen(workpath)+20, "%s/tmpdir",
				workpath);
			if (access(tmppath, F_OK) != 0)
				mkdir(tmppath, 0700);
			symlink(tmppath, "/tmp/testHCFS");
		}
		mkdir("/tmp/testHCFS/metastorage", 0700);
		mkdir("/tmp/testHCFS/blockstorage", 0700);
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
	}

	void TearDown()
	{
		free(hcfs_system);
		unlink("/tmp/testHCFS");
		if (tmppath != NULL)
			nftw(tmppath, do_delete, 20, FTW_DEPTH);
		if (workpath != NULL)
			free(workpath);
		if (tmppath != NULL)
			free(tmppath);
		free(system_config);
	}
};

TEST_F(reload_system_configTest, NewConfigInvalid)
{
	char pathname[200];
	int32_t ret;

	strcpy(pathname,"testpatterns/test_good_hcfs.conf");
	read_system_config(pathname, system_config);
	strcpy(pathname,"testpatterns/test_good_hcfs_S3.conf");

	ret = reload_system_config(pathname);

	EXPECT_EQ(-EINVAL, ret);
	EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
	EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
	EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
	EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
	EXPECT_EQ(CACHE_DELTA, 10485760);
	EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
	EXPECT_EQ(CURRENT_BACKEND, SWIFT);
}

TEST_F(reload_system_configTest, NewConfig_TheSame)
{
	char pathname[200];
	int32_t ret;

	strcpy(pathname,"testpatterns/test_good_hcfs.conf");
	read_system_config(pathname, system_config);
	strcpy(pathname,"testpatterns/test_good_hcfs.conf");

	ret = reload_system_config(pathname);

	EXPECT_EQ(0, ret);
	EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
	EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
	EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
	EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
	EXPECT_EQ(CACHE_DELTA, 10485760);
	EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
	EXPECT_EQ(CURRENT_BACKEND, SWIFT);
}

TEST_F(reload_system_configTest, Set_Backend_Success)
{
	char pathname[200];
	int32_t ret;

	strcpy(pathname,"testpatterns/test_hcfs_backend_none.conf");
	read_system_config(pathname, system_config);
	strcpy(pathname,"testpatterns/test_good_hcfs.conf");

	ret = reload_system_config(pathname);

	EXPECT_EQ(0, ret);
	EXPECT_STREQ(METAPATH, "/tmp/testHCFS/metastorage");
	EXPECT_STREQ(BLOCKPATH, "/tmp/testHCFS/blockstorage");
	EXPECT_EQ(CACHE_SOFT_LIMIT, 53687091);
	EXPECT_EQ(CACHE_HARD_LIMIT, 107374182);
	EXPECT_EQ(CACHE_DELTA, 10485760);
	EXPECT_EQ(MAX_BLOCK_SIZE, 1048576);
	EXPECT_EQ(CURRENT_BACKEND, SWIFT);
}

/* End of unittest for reload_system_config */

/* Unittest of is_natural_number() */
class is_natural_numberTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(is_natural_numberTest, ZeroTest)
{
	EXPECT_EQ(TRUE, is_natural_number("0"));
}

TEST_F(is_natural_numberTest, NegativeNumber)
{
	EXPECT_EQ(FALSE, is_natural_number("-12345"));
}

TEST_F(is_natural_numberTest, NotNumber)
{
	EXPECT_EQ(FALSE, is_natural_number("lalala12345"));
}

TEST_F(is_natural_numberTest, InvalidNumber)
{
	EXPECT_EQ(FALSE, is_natural_number("00012345"));
}

TEST_F(is_natural_numberTest, PositiveNumber1)
{
	EXPECT_EQ(TRUE, is_natural_number("12345"));
}

TEST_F(is_natural_numberTest, PositiveNumber2)
{
	EXPECT_EQ(TRUE, is_natural_number("5"));
}
/* End of unittest of is_natural_number() */

/* Unittest of update_backend_usage() */
class update_fs_backend_usageTest : public ::testing::Test {
protected:
	FILE *fptr;

	void SetUp()
	{
		mkdir("utils_unittest_folder", 0700);
		fptr = fopen("utils_unittest_folder/mock_FSstat", "w+");
		sys_super_block = (SUPER_BLOCK_CONTROL *) malloc(sizeof(SUPER_BLOCK_CONTROL));
		sys_super_block->head.num_total_inodes = 10;
	}

	void TearDown()
	{
		fclose(fptr);
		nftw("utils_unittest_folder", do_delete, 20, FTW_DEPTH);
		free(sys_super_block);
	}
};

TEST_F(update_fs_backend_usageTest, UpdateSuccess)
{
	FS_CLOUD_STAT_T fs_cloud_stat;

	fs_cloud_stat.backend_system_size = 123456;
	fs_cloud_stat.backend_meta_size = 456;
	fs_cloud_stat.backend_num_inodes = 5566;
	fs_cloud_stat.pinned_size = 0;
	fs_cloud_stat.disk_meta_size = 456;
	fs_cloud_stat.disk_pinned_size = 0;

	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	/* Run */
	EXPECT_EQ(0, update_fs_backend_usage(fptr, 123, 456, 789, 0, 0, 456));

	/* Verify */
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	EXPECT_EQ(123456 + 123, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(456 + 456, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(5566 + 789, fs_cloud_stat.backend_num_inodes);
	EXPECT_EQ(0, fs_cloud_stat.pinned_size);
	EXPECT_EQ(0, fs_cloud_stat.disk_pinned_size);
	EXPECT_EQ(456 + 456, fs_cloud_stat.disk_meta_size);
}

TEST_F(update_fs_backend_usageTest, UpdateSuccessPin)
{
	FS_CLOUD_STAT_T fs_cloud_stat;

	fs_cloud_stat.backend_system_size = 123456;
	fs_cloud_stat.backend_meta_size = 456;
	fs_cloud_stat.backend_num_inodes = 5566;
	fs_cloud_stat.pinned_size = 56789;
	fs_cloud_stat.disk_meta_size = 456;
	fs_cloud_stat.disk_pinned_size = 56789;

	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	/* Run */
	EXPECT_EQ(0, update_fs_backend_usage(fptr, 123, 456, 789, 123, 123, 456));

	/* Verify */
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	EXPECT_EQ(123456 + 123, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(456 + 456, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(5566 + 789, fs_cloud_stat.backend_num_inodes);
	EXPECT_EQ(56789 + 123, fs_cloud_stat.pinned_size);
	EXPECT_EQ(56789 + 123, fs_cloud_stat.disk_pinned_size);
	EXPECT_EQ(456 + 456, fs_cloud_stat.disk_meta_size);
}

TEST_F(update_fs_backend_usageTest, UpdateSuccess_LessThanZero)
{
	FS_CLOUD_STAT_T fs_cloud_stat;

	fs_cloud_stat.backend_system_size = 123456;
	fs_cloud_stat.backend_meta_size = 456;
	fs_cloud_stat.backend_num_inodes = 5566;
	fs_cloud_stat.pinned_size = 0;
	fs_cloud_stat.disk_meta_size = 456;
	fs_cloud_stat.disk_pinned_size = 0;

	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	/* Run */
	EXPECT_EQ(0, update_fs_backend_usage(fptr, -12345678, -456666,
		-789999, 0, 0, -456666));

	/* Verify */
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	EXPECT_EQ(0, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(0, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(0, fs_cloud_stat.backend_num_inodes);
	EXPECT_EQ(0, fs_cloud_stat.pinned_size);
	EXPECT_EQ(0, fs_cloud_stat.disk_pinned_size);
	EXPECT_EQ(0, fs_cloud_stat.disk_meta_size);
}

TEST_F(update_fs_backend_usageTest, TestOldFormat)
{
	FS_CLOUD_STAT_T fs_cloud_stat;

	fs_cloud_stat.backend_system_size = 123456;
	fs_cloud_stat.backend_meta_size = 456;
	fs_cloud_stat.backend_num_inodes = 5566;
	fs_cloud_stat.pinned_size = 56789;
	fs_cloud_stat.disk_meta_size = -1;
	fs_cloud_stat.disk_pinned_size = -1;

	fseek(fptr, 0, SEEK_SET);
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);

	/* Run */
	EXPECT_EQ(0, update_fs_backend_usage(fptr, 123, 456, 789, 123, 123, 456));

	/* Verify */
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	EXPECT_EQ(123456 + 123, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(456 + 456, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(5566 + 789, fs_cloud_stat.backend_num_inodes);
	EXPECT_EQ(56789 + 123, fs_cloud_stat.pinned_size);
	EXPECT_EQ(-1, fs_cloud_stat.disk_pinned_size);
	EXPECT_EQ(-1, fs_cloud_stat.disk_meta_size);
}

/* End of unittest of update_backend_usage() */

/*
 * Unittest of change_system_meta()
 */
class change_system_metaTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system =
			(SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

TEST_F(change_system_metaTest, UpdateSuccess)
{
	int32_t ret;

	ret = change_system_meta(1, 2, 3, 4, 5, 6, FALSE);
	EXPECT_EQ(0, ret);

	EXPECT_EQ(1, hcfs_system->systemdata.system_size);
	EXPECT_EQ(2, hcfs_system->systemdata.system_meta_size);
	EXPECT_EQ(3, hcfs_system->systemdata.cache_size);
	EXPECT_EQ(4, hcfs_system->systemdata.cache_blocks);
	EXPECT_EQ(5, hcfs_system->systemdata.dirty_cache_size);
	EXPECT_EQ(6, hcfs_system->systemdata.unpin_dirty_data_size);
}
/*
 * End of unittest of change_system_meta()
 */

/*
 * Unittest of _shift_xfer_window()
 */
class _shift_xfer_windowTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system =
			(SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

TEST_F(_shift_xfer_windowTest, NoShiftOccur)
{
	int32_t ret;

	hcfs_system->systemdata.xfer_now_window = 1;
	hcfs_system->last_xfer_shift_time = time(NULL);
	sleep(1);

	_shift_xfer_window();
	EXPECT_EQ(1, hcfs_system->systemdata.xfer_now_window);
}

TEST_F(_shift_xfer_windowTest, ShiftSuccessful)
{
	int32_t idx, now_window, num_shifted;
	int32_t time_passed = 45;

	hcfs_system->systemdata.xfer_now_window = 1;
	hcfs_system->last_xfer_shift_time = time(NULL) - time_passed;

	_shift_xfer_window();
	EXPECT_EQ(3, hcfs_system->systemdata.xfer_now_window);

	now_window = hcfs_system->systemdata.xfer_now_window - 1;
	now_window = (now_window < 0) ? XFER_WINDOW_MAX - 1 : now_window;
	num_shifted = time_passed / XFER_SEC_PER_WINDOW;
	for (idx = 0; idx < num_shifted; idx++) {
		EXPECT_EQ(0, hcfs_system->systemdata.xfer_throughput[now_window]);
		EXPECT_EQ(0, hcfs_system->systemdata.xfer_total_obj[now_window]);
		now_window = now_window - 1;
		if (now_window < 0)
			now_window = XFER_WINDOW_MAX - 1;
	}
}
/*
 * End of unittest of change_system_meta()
 */

/*
 * Unittest of change_xfer_meta()
 */
class change_xfer_metaTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system =
			(SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

TEST_F(change_xfer_metaTest, UpdateSuccess)
{
	int32_t ret;

	hcfs_system->systemdata.xfer_now_window = 2;

	ret = change_xfer_meta(1, 2, 3, 4);
	EXPECT_EQ(0, ret);

	EXPECT_EQ(1, hcfs_system->systemdata.xfer_size_upload);
	EXPECT_EQ(2, hcfs_system->systemdata.xfer_size_download);
	EXPECT_EQ(3, hcfs_system->systemdata.xfer_throughput[2]);
	EXPECT_EQ(4, hcfs_system->systemdata.xfer_total_obj[2]);
}

TEST_F(change_xfer_metaTest, MinIsZero)
{
	int32_t ret;

	hcfs_system->systemdata.xfer_now_window = 3;

	ret = change_xfer_meta(-1, -2, -3, -4);
	EXPECT_EQ(0, ret);

	EXPECT_EQ(0, hcfs_system->systemdata.xfer_size_upload);
	EXPECT_EQ(0, hcfs_system->systemdata.xfer_size_download);
	EXPECT_EQ(0, hcfs_system->systemdata.xfer_throughput[3]);
	EXPECT_EQ(0, hcfs_system->systemdata.xfer_total_obj[3]);
}
/*
 * End of unittest of change_system_meta()
 */

/* Unittest of get_quota_from_backup() */
class get_quota_from_backupTest : public ::testing::Test {
protected:
	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));

		METAPATH = (char *)malloc(METAPATHLEN);
		strcpy(METAPATH, "get_quota_from_backup_dir");
		mkdir(METAPATH, 0700);
		mknod("get_quota_from_backup_dir/usermeta", 0700, 0);
		dec_success = TRUE;
		json_file_corrupt = FALSE;
	}

	void TearDown()
	{
		nftw(METAPATH, do_delete, 20, FTW_DEPTH);

		free(METAPATH);
		free(system_config);
	}
};

TEST_F(get_quota_from_backupTest, MetapathNotExist)
{
	int64_t quota;

	unlink("get_quota_from_backup_dir/usermeta");

	EXPECT_EQ(-ENOENT, get_quota_from_backup(&quota));
}

TEST_F(get_quota_from_backupTest, BackupNotExist)
{
	int64_t quota;

	dec_success = FALSE;

	EXPECT_EQ(-ENOENT, get_quota_from_backup(&quota));
}

TEST_F(get_quota_from_backupTest, JsonfileCorrupt)
{
	int64_t quota;

	json_file_corrupt = TRUE;

	EXPECT_EQ(-EINVAL, get_quota_from_backup(&quota));
}

TEST_F(get_quota_from_backupTest, Success)
{
	int64_t quota;

	EXPECT_EQ(0, get_quota_from_backup(&quota));
	EXPECT_EQ(5566, quota);
}

/* End of unittest of get_quota_from_backup() */
/*
        Unittest for init_cache_thresholds()
 */
class init_cache_thresholdsTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)calloc(
			    1, sizeof(SYSTEM_CONF_STRUCT));
			CACHE_HARD_LIMIT = 0;
			RESERVED_CACHE_SPACE = 0;
		}
		void TearDown()
		{
			free(system_config);
	}
};

TEST_F(init_cache_thresholdsTest, Successful)
{
	int32_t ret;

	CACHE_HARD_LIMIT = 100;
	RESERVED_CACHE_SPACE = 100;
	ret = init_cache_thresholds(system_config);

	EXPECT_EQ(ret, 0);
	EXPECT_EQ(CACHE_LIMITS(P_UNPIN), CACHE_HARD_LIMIT);
	EXPECT_EQ(PINNED_LIMITS(P_UNPIN), MAX_PINNED_LIMIT);
	EXPECT_EQ(CACHE_LIMITS(P_PIN), CACHE_HARD_LIMIT);
	EXPECT_EQ(PINNED_LIMITS(P_PIN), MAX_PINNED_LIMIT);
	EXPECT_EQ(CACHE_LIMITS(P_HIGH_PRI_PIN),
			CACHE_HARD_LIMIT + RESERVED_CACHE_SPACE);
	EXPECT_EQ(PINNED_LIMITS(P_HIGH_PRI_PIN),
			MAX_PINNED_LIMIT + RESERVED_CACHE_SPACE);
}
/*
        End of unittest of init_cache_thresholds()
 */

TEST(round_sizeTest, Change4KSuccess)
{
	EXPECT_EQ(0, round_size(0));
	EXPECT_EQ(0, round_size(-0));
	EXPECT_EQ(4096, round_size(1));
	EXPECT_EQ(8192, round_size(4097));
	EXPECT_EQ(-4096, round_size(-1));
	EXPECT_EQ(-8192, round_size(-4097));
}
