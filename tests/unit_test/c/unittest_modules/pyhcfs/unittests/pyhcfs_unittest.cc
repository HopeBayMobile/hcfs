/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <semaphore.h>
#include <signal.h>
extern "C" {
#include "fuseop.h"
#include "global.h"
#include "parser.h"
#include "time.h"
}

using ::testing::TestWithParam;
using ::testing::ValuesIn;

/* Defiinition for multiple versions/platforms */
const char* paths[] = {
	"test_data/v1/android",
	"test_data/v1/linux"
};

#define CONCAT_TEST_META_PATH(A) \
	char meta_path[128];\
	sprintf(meta_path, "%s/%s", GetParam(), A);\
	printf("Test meta path is `%s`\n", meta_path);


/* Definition for fake functions */
#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(ssize_t, pread, int, void *, size_t, off_t);
ssize_t (*const real_pread)(int fd, void *buf, size_t count, off_t offset) =
    (ssize_t(*)(int, void *, size_t, off_t))dlsym(RTLD_NEXT, "pread");

FAKE_VALUE_FUNC(ssize_t, read, int, void *, size_t);
ssize_t (*const real_read)(int fd, void *buf, size_t count) =
    (ssize_t(*)(int, void *, size_t))dlsym(RTLD_NEXT, "read");

int32_t pread_cnt_call_count = 0;
int32_t pread_cnt_error_on_call_count = -1;
ssize_t pread_cnt(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t ret;

	pread_cnt_call_count += 1;
	printf("pread_cnt_call_count %d\n", pread_cnt_call_count);
	ret = real_pread(fd, buf, count, offset);
	if (pread_cnt_call_count == pread_cnt_error_on_call_count)
		return -1;
	else
		return ret;
}

int32_t read_cnt_call_count = 0;
int32_t read_cnt_error_on_call_count = -1;
ssize_t read_cnt(int fd, void *buf, size_t count)
{
	ssize_t ret;

	read_cnt_call_count += 1;
	printf("read_cnt_call_count %d\n", read_cnt_call_count);
	ret = real_read(fd, buf, count);
	if (read_cnt_call_count == read_cnt_error_on_call_count)
		return -1;
	else
		return ret;
}

void reset_fake_functions(void)
{
	RESET_FAKE(pread);
	FFF_RESET_HISTORY();
	pread_cnt_call_count = 0;
	pread_cnt_error_on_call_count = -1;
	pread_fake.custom_fake = real_pread;
	read_fake.custom_fake = real_read;
	read_cnt_call_count = 0;
	read_cnt_error_on_call_count = -1;
}

/* Test environment */
class pyhcfsEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp() {}

	virtual void TearDown() {}
};

::testing::Environment *const pyhcfs_env =
    ::testing::AddGlobalTestEnvironment(new pyhcfsEnvironment);

/* Unittest for list_external_volume */
class list_external_volumeTest : public ::testing::TestWithParam<const char*>
{
	public:
	void SetUp() { reset_fake_functions(); }
	void TearDown() {}
};

TEST_P(list_external_volumeTest, ListExternalVolume)
{
	int32_t ret;
	uint64_t i, number;
	PORTABLE_DIR_ENTRY *list;
	CONCAT_TEST_META_PATH("fsmgr");

	ret =
	    list_external_volume(meta_path, &list, &number);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(number, 1);
	for (i = 0; i < number; i++) {
		puts(list[i].d_name);
	}
}
TEST_F(list_external_volumeTest, ListExternalVolumeNoFile)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	ret_code = list_external_volume("test_nexus_5x/....", &list, &number);
	EXPECT_LT(ret_code, 0);
}

TEST_F(list_external_volumeTest, ListExternalVolumeEIO)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	ret_code = list_external_volume("/proc/self/mem", &list, &number);
	EXPECT_LT(ret_code, 0);
}

TEST_F(list_external_volumeTest, ListExternalVolumeErrorOnPread2ndCall)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = pread_cnt;
	pread_cnt_error_on_call_count = 2;

	ret_code =
	    list_external_volume("test_data/v1/android/fsmgr", &list, &number);
	printf("%d\n", ret_code);
	printf("number %lu\n", number);
	EXPECT_LT(ret_code, 0);
}
TEST_F(list_external_volumeTest, ListExternalVolumeErrorOnPread3rdCall)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = pread_cnt;
	pread_cnt_error_on_call_count = 3;

	ret_code =
	    list_external_volume("test_data/v1/android/fsmgr", &list, &number);
	EXPECT_LT(ret_code, 0);
}

INSTANTIATE_TEST_CASE_P(ListExternalVolume, list_external_volumeTest, ValuesIn(paths));
/* End unittest for list_external_volume */

/* Unittest for parse_meta */
class parse_metaTest : public ::testing::TestWithParam<const char*>
{
	public:
	void SetUp() { reset_fake_functions(); }
	void TearDown() {}
};

TEST_P(parse_metaTest, OpenDir)
{
	RET_META meta;
	CONCAT_TEST_META_PATH("meta_isdir");

	parse_meta(meta_path, &meta);
	EXPECT_EQ(meta.result, 0);
	EXPECT_EQ(meta.file_type, D_ISDIR);
}
TEST_P(parse_metaTest, OpenFile)
{
	RET_META meta;
	CONCAT_TEST_META_PATH("meta_isreg");

	parse_meta(meta_path, &meta);
	EXPECT_EQ(meta.result, 0);
	EXPECT_EQ(meta.file_type, D_ISREG);
}
TEST_P(parse_metaTest, OpenFifo)
{
	RET_META meta;
	CONCAT_TEST_META_PATH("meta_isfifo");

	parse_meta(meta_path, &meta);
	EXPECT_EQ(meta.result, 0);
	EXPECT_EQ(meta.file_type, D_ISFIFO);
}
TEST_P(parse_metaTest, OpenLink)
{
	RET_META meta;
	CONCAT_TEST_META_PATH("meta_islnk");

	parse_meta(meta_path, &meta);
	EXPECT_EQ(meta.result, 0);
	EXPECT_EQ(meta.file_type, D_ISLNK);
}
TEST_F(parse_metaTest, OpenError)
{
	RET_META meta;

	parse_meta("test_data/v1/android/meta_not_existed", &meta);
	EXPECT_LT(meta.result, 0);
	EXPECT_EQ(meta.result, -1);
}
TEST_F(parse_metaTest, ParseMetaEIO)
{
	RET_META meta;

	parse_meta("/proc/self/mem", &meta);
	printf("parse_meta meta.result %d\n", meta.result);
	EXPECT_LT(meta.result, 0);
	EXPECT_EQ(meta.result, -1);
}
TEST_F(parse_metaTest, UnsupportedVersion)
{
	RET_META meta;

	parse_meta("test_data/v0/android/meta_isreg", &meta);
	printf("parse_meta meta.result %d\n", meta.result);
	EXPECT_EQ(meta.result, ERROR_UNSUPPORT_VER);
}
TEST_F(parse_metaTest, ReadDirChildNumError)
{
	RET_META meta;

	read_fake.custom_fake = read_cnt;
	read_cnt_error_on_call_count = 2;
	parse_meta("test_data/v1/android/meta_isdir", &meta);
	printf("parse_meta meta.result %d\n", meta.result);
	EXPECT_EQ(meta.result, -1);
}

INSTANTIATE_TEST_CASE_P(ParseMeta, parse_metaTest, ValuesIn(paths));
/* End unittest for parse_meta */

/* Unittest for List_Dir_Inorder */
class list_dir_inorderTest : public ::testing::TestWithParam<const char*>
{
	protected:
	int32_t ret_val;
	int32_t total_children, num_children, limit;
	int32_t end_el_no;
	int64_t end_page_pos;
	PORTABLE_DIR_ENTRY file_list[LIST_DIR_LIMIT];

	void SetUp()
	{
		reset_fake_functions();
		total_children = 1002;
		limit = 500;
		num_children = end_page_pos = end_el_no = 0;
		memset(file_list, 0, sizeof(PORTABLE_DIR_ENTRY) * limit);
	}

	void TearDown() {}
};

TEST_P(list_dir_inorderTest, FromTreeRootSuccessful)
{
	int32_t count = 0;
	FILE *fp;
	char fname[100];
	char *pos;
	CONCAT_TEST_META_PATH("meta_isdir");

	ret_val = list_dir_inorder(
	    meta_path, end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &num_children, &(file_list[0]));

	/* Compare filename */
	fp = fopen("test_data/v1/android/meta_isdir_filelist", "r");
	if (fp == NULL) {
		printf("Test data `meta_isdir_filelist` not found.\n");
		EXPECT_EQ(0, -1);
	}

	for (;;) {
		fgets(fname, 100, fp);
		if ((pos = strchr(fname, '\n')) != NULL)
			*pos = '\0';
		ASSERT_STREQ(file_list[count].d_name, fname);
		count += 1;
		if (count >= limit)
			break;
	}

	EXPECT_EQ(num_children, limit);
}

TEST_P(list_dir_inorderTest, TraverseAllSuccessful)
{
	limit = 77;
	CONCAT_TEST_META_PATH("meta_isdir");

	while (1) {
		ret_val = list_dir_inorder(meta_path, end_page_pos, end_el_no,
					   limit, &end_page_pos, &end_el_no,
					   &num_children, &(file_list[0]));

		if (num_children > 0 && num_children < limit) {
			ASSERT_EQ(num_children, total_children % limit);
		} else if (num_children > 0) {
			ASSERT_EQ(num_children, limit);
		} else {
			break;
		}
	}

	EXPECT_EQ(num_children, 0);
}

TEST_P(list_dir_inorderTest, TraverseAllSuccessful2)
{
	limit = 1;
	CONCAT_TEST_META_PATH("meta_isdir");

	while (1) {
		ret_val = list_dir_inorder(meta_path, end_page_pos, end_el_no,
					   limit, &end_page_pos, &end_el_no,
					   &num_children, &(file_list[0]));

		if (num_children > 0 && num_children < limit) {
			EXPECT_EQ(num_children, total_children % limit);
		} else if (num_children > 0) {
			EXPECT_EQ(num_children, limit);
		} else {
			break;
		}
	}

	EXPECT_EQ(num_children, 0);
}

TEST_P(list_dir_inorderTest, MetaIsReg)
{
	CONCAT_TEST_META_PATH("meta_isreg");
	ret_val = list_dir_inorder(
	    meta_path, end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &num_children, &(file_list[0]));

	EXPECT_EQ(num_children, 0);
	EXPECT_EQ(ret_val, -1);
}

TEST_F(list_dir_inorderTest, LimitExceeded)
{
	limit = LIST_DIR_LIMIT + 1;

	ret_val = list_dir_inorder(
	    "test_data/v1/android/meta_isdir", end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &num_children, &(file_list[0]));

	EXPECT_EQ(num_children, 0);
	EXPECT_EQ(ret_val, -1);
}

TEST_F(list_dir_inorderTest, StartELExceeded)
{
	end_el_no = MAX_DIR_ENTRIES_PER_PAGE + 1;

	ret_val = list_dir_inorder(
	    "test_data/v1/android/meta_isdir", end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &num_children, &(file_list[0]));

	EXPECT_EQ(num_children, 0);
	EXPECT_EQ(ret_val, -1);
}

TEST_F(list_dir_inorderTest, MetaPathNotExisted)
{
	ret_val = list_dir_inorder(
	    "test_data/v1/android/meta_not_existed", end_page_pos, end_el_no,
	    limit, &end_page_pos, &end_el_no, &num_children, &(file_list[0]));

	EXPECT_EQ(num_children, 0);
	EXPECT_EQ(ret_val, -1);
}

INSTANTIATE_TEST_CASE_P(ListDirInorder, list_dir_inorderTest, ValuesIn(paths));
/* End unittest for List_Dir_Inorder */

/* Unittest for List_File_Blocks */
class list_file_blocksTest : public ::testing::TestWithParam<const char*>
{
	protected:
	int32_t ret_val;
	int64_t ret_num, total_num;
	int64_t inode_num;
	PORTABLE_BLOCK_NAME *block_list;

	void SetUp()
	{
		reset_fake_functions();
		total_num = 4;
	}
	void TearDown() {}
};

TEST_P(list_file_blocksTest, OPSuccessful)
{
	int32_t idx;
	HCFS_STAT_v1 meta_stat;

	CONCAT_TEST_META_PATH("meta_isreg");
	ret_val =
	    list_file_blocks(meta_path, &block_list, &ret_num, &inode_num);

	FILE *fp = fopen(meta_path, "rb");
	if (fp == NULL) {
		printf("Test data `%s` not found\n", meta_path);
		ASSERT_EQ(0, -1);
	} else {
		fread(&meta_stat, sizeof(HCFS_STAT_v1), 1, fp);
	}

	EXPECT_EQ(ret_val, 0);
	EXPECT_EQ(ret_num, total_num);
	EXPECT_EQ(inode_num, meta_stat.ino);

	for (idx = 0; idx < ret_num; idx++)
		printf("obj #%ld seq %ld\n", block_list[idx].block_num,
		       block_list[idx].block_seq);
}

TEST_P(list_file_blocksTest, NotISREG)
{
	HCFS_STAT_v1 meta_stat;

	CONCAT_TEST_META_PATH("meta_isdir");
	ret_val =
	    list_file_blocks(meta_path, &block_list, &ret_num, &inode_num);

	EXPECT_EQ(ret_val, -1);
}

TEST_F(list_file_blocksTest, MetaNotExisted)
{
	char meta_path[100] = "test_data/v0/android/meta_not_existed";
	HCFS_STAT_v1 meta_stat;

	ret_val =
	    list_file_blocks(meta_path, &block_list, &ret_num, &inode_num);

	EXPECT_EQ(ret_val, -1);
}

TEST_F(list_file_blocksTest, MetaVersionNotSupport)
{
	char meta_path[100] = "test_data/v0/android/meta_isreg";
	HCFS_STAT_v1 meta_stat;

	ret_val =
	    list_file_blocks(meta_path, &block_list, &ret_num, &inode_num);

	EXPECT_EQ(ret_val, -2);
}

INSTANTIATE_TEST_CASE_P(ListFileBlocks, list_file_blocksTest, ValuesIn(paths));
/* End unittest for List_File_Blocks */

/* Unittest for get_vol_usage */
class get_vol_usageTest : public ::testing::TestWithParam<const char*>
{
	protected:
	int32_t ret_val;
	int64_t ret_num, total_num;
	int64_t inode_num;

	void SetUp() { reset_fake_functions(); }
	void TearDown() {}
};

TEST_P(get_vol_usageTest, OPSuccessful)
{
	int64_t vol_usage = 0;
	CONCAT_TEST_META_PATH("FSstat");
	ret_val = get_vol_usage(meta_path, &vol_usage);
	EXPECT_EQ(ret_val, 0);
	EXPECT_GT(vol_usage, 10000000000);
	EXPECT_LT(vol_usage, 20000000000);
}

TEST_F(get_vol_usageTest, FileNotExist)
{
	int64_t vol_usage = 0;
	ret_val = get_vol_usage("test_data/v1/android/meta_not_existed", &vol_usage);
	EXPECT_EQ(ret_val, -1);
	EXPECT_EQ(errno, ENOENT);
}
TEST_F(get_vol_usageTest, FileReadError)
{
	int64_t vol_usage = 0;
	ret_val = get_vol_usage("/proc/self/mem", &vol_usage);
	EXPECT_EQ(ret_val, -1);
	EXPECT_EQ(errno, EIO);
}

INSTANTIATE_TEST_CASE_P(GetVolUsage, get_vol_usageTest, ValuesIn(paths));
/* End unittest for get_vol_usage */
