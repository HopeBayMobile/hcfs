/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
extern "C" {
#include "global.h"
#include "fuseop.h"
#include "time.h"
#include "parser.h"
}

#include "../../fff.h"
DEFINE_FFF_GLOBALS;

ssize_t real_pread(int fd, void *buf, size_t count, off_t offset) asm("pread");
FAKE_VALUE_FUNC(ssize_t, pread, int, void *, size_t, off_t);
int32_t pread_cnt_call_count;
int32_t pread_cnt_error_on_call_count;
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

/* Unittest for list_external_volume */
class list_external_volumeTest : public ::testing::Test
{
	public:
	void SetUp()
	{
		RESET_FAKE(pread);
		FFF_RESET_HISTORY();
		pread_cnt_call_count = 0;
		pread_cnt_error_on_call_count = -1;
	}
	void TearDown() {}
};

TEST_F(list_external_volumeTest, ListExternalVolume)
{
	int32_t ret;
	uint64_t i, number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = &real_pread;
	ret = list_external_volume("test_nexus_5x/fsmgr", &list, &number);
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

	pread_fake.custom_fake = &real_pread;
	ret_code = list_external_volume("test_nexus_5x/....", &list, &number);
	ASSERT_TRUE(ret_code < 0);
}

TEST_F(list_external_volumeTest, ListExternalVolumeEIO)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = &real_pread;
	ret_code = list_external_volume("/proc/self/mem", &list, &number);
	EXPECT_LT(ret_code, 0);
}

TEST_F(list_external_volumeTest, ListExternalVolumeErrorOnPread2ndCall)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = &pread_cnt;
	pread_cnt_error_on_call_count = 2;

	ret_code = list_external_volume("test_nexus_5x/fsmgr", &list, &number);
	printf("%d\n", ret_code);
	printf("number %lu\n", number);
	ASSERT_TRUE(ret_code < 0);
}
TEST_F(list_external_volumeTest, ListExternalVolumeErrorOnPread3rdCall)
{
	int32_t ret_code;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	pread_fake.custom_fake = &pread_cnt;
	pread_cnt_error_on_call_count = 3;

	ret_code = list_external_volume("test_nexus_5x/fsmgr", &list, &number);
	ASSERT_TRUE(ret_code < 0);
}
/* End unittest for list_external_volume */

/* Unittest for parse_meta */
class parse_metaTest : public ::testing::Test
{
	public:
	void SetUp()
	{
		RESET_FAKE(pread);
		FFF_RESET_HISTORY();
		pread_cnt_call_count = 0;
		pread_cnt_error_on_call_count = -1;
	}
	void TearDown() {}
};

TEST_F(parse_metaTest, OpenDir)
{
	RET_META meta;

	pread_fake.custom_fake = &real_pread;
	parse_meta("test_nexus_5x/meta", &meta);
	ASSERT_EQ(meta.result, 0);
	ASSERT_EQ(meta.file_type, D_ISDIR);
}
TEST_F(parse_metaTest, OpenFile)
{
	RET_META meta;

	parse_meta("test_nexus_5x/meta_isreg", &meta);
	ASSERT_EQ(meta.result, 0);
	ASSERT_EQ(meta.file_type, D_ISREG);
}
TEST_F(parse_metaTest, OpenFifo)
{
	RET_META meta;

	parse_meta("test_nexus_5x/meta_isfifo", &meta);
	ASSERT_EQ(meta.result, 0);
	ASSERT_EQ(meta.file_type, D_ISFIFO);
}
TEST_F(parse_metaTest, OpenLink)
{
	RET_META meta;

	parse_meta("test_nexus_5x/meta_islink", &meta);
	ASSERT_EQ(meta.result, 0);
	ASSERT_EQ(meta.file_type, D_ISLNK);
}
TEST_F(parse_metaTest, OpenError)
{
	RET_META meta;

	parse_meta("test_nexus_5x/not_exist", &meta);
	ASSERT_TRUE(meta.result < 0);
	ASSERT_EQ(meta.result, -1);
}
TEST_F(parse_metaTest, ParseMetaEIO)
{
	RET_META meta;

	parse_meta("/proc/self/mem", &meta);
	printf("parse_meta meta.result %d", meta.result);
	ASSERT_TRUE(meta.result < 0);
	ASSERT_EQ(meta.result, -1);
}
/* End unittest for parse_meta */

/* Unittest for List_Dir_Inorder */
class list_dir_inorderTest : public ::testing::Test
{
	protected:
	int32_t total_children, num_children, limit;
	int32_t end_el_no;
	int64_t end_page_pos;

	void SetUp()
	{
		total_children = 3003;
		limit = 500;
		end_page_pos = end_el_no = 0;
		pread_fake.custom_fake = &real_pread;
	}

	void TearDown() {}
};

TEST_F(list_dir_inorderTest, FromTreeRootSuccessful)
{
	int32_t count = 0;
	int32_t cmp_res;
	FILE *fp;
	char fname[100];
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children =
	    list_dir_inorder("test_nexus_5x/meta", end_page_pos, end_el_no,
			     limit, &end_page_pos, &end_el_no, &(file_list[0]));

	/* Compare filename */
	fp = fopen("test_nexus_5x/meta_filelist", "r");
	while (1) {
		fgets(fname, 100, fp);
		cmp_res = strncmp(file_list[count].d_name, fname,
				  strlen(file_list[count].d_name));
		ASSERT_EQ(0, cmp_res);
		count += 1;
		if (count >= limit)
			break;
	}

	ASSERT_EQ(num_children, limit);
}

TEST_F(list_dir_inorderTest, TraverseAllSuccessful)
{
	PORTABLE_DIR_ENTRY file_list[limit];

	while (1) {
		num_children = list_dir_inorder(
		    "test_nexus_5x/meta", end_page_pos, end_el_no, limit,
		    &end_page_pos, &end_el_no, &(file_list[0]));

		if (num_children > 0 && num_children < limit) {
			ASSERT_EQ(num_children, total_children % limit);
		} else if (num_children > 0) {
			ASSERT_EQ(num_children, limit);
		} else {
			break;
		}
	}

	ASSERT_EQ(num_children, 0);
}

TEST_F(list_dir_inorderTest, TraverseAllSuccessful2)
{
	PORTABLE_DIR_ENTRY file_list[limit];
	limit = 1;

	while (1) {
		num_children = list_dir_inorder(
		    "test_nexus_5x/meta", end_page_pos, end_el_no, limit,
		    &end_page_pos, &end_el_no, &(file_list[0]));

		if (num_children > 0 && num_children < limit) {
			ASSERT_EQ(num_children, total_children % limit);
		} else if (num_children > 0) {
			ASSERT_EQ(num_children, limit);
		} else {
			break;
		}
	}

	ASSERT_EQ(num_children, 0);
}

TEST_F(list_dir_inorderTest, ValidateOffset)
{
	limit = 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children =
	    list_dir_inorder("test_nexus_5x/meta", end_page_pos, end_el_no,
			     limit, &end_page_pos, &end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, limit);
	ASSERT_EQ(end_page_pos, 240);
	ASSERT_EQ(end_el_no, 1);
}

TEST_F(list_dir_inorderTest, MetaIsReg)
{
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder(
	    "test_nexus_5x/meta_isreg", end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -ENOTDIR);
}

TEST_F(list_dir_inorderTest, LimitExceeded)
{
	limit = LIST_DIR_LIMIT + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children =
	    list_dir_inorder("test_nexus_5x/meta", end_page_pos, end_el_no,
			     limit, &end_page_pos, &end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, StartELExceeded)
{
	end_el_no = MAX_DIR_ENTRIES_PER_PAGE + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children =
	    list_dir_inorder("test_nexus_5x/meta", end_page_pos, end_el_no,
			     limit, &end_page_pos, &end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, MetaPathNotExisted)
{
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder(
	    "test_nexus_5x/file_no_existed", end_page_pos, end_el_no, limit,
	    &end_page_pos, &end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -ENOENT);
}
/* End unittest for List_Dir_Inorder */
