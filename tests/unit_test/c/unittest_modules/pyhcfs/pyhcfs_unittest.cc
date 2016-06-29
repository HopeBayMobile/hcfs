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

ssize_t real (int fd, void *buf, size_t count, off_t offset) asm("pread");

#include "../../fff.h"
DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(ssize_t, pread, int , void *, size_t , off_t );

/* Unittest for list_external_volume */
class list_external_volumeTest : public ::testing::Test {
	public:
	void SetUp() {
	RESET_FAKE(pread);
	FFF_RESET_HISTORY();
	pread_fake.custom_fake = &f;
	}

	void TearDown() {
	}
};

TEST_F(list_external_volumeTest, ListExternalVolume) {
	int32_t ret;
	uint64_t i, number;
	PORTABLE_DIR_ENTRY *list;

	ret = list_external_volume("test_nexus_5x/fsmgr", &list, &number);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(number, 1);
	for (i = 0; i < number; i++) {
		puts(list[i].d_name);
	}
}
TEST_F(list_external_volumeTest, ListExternalVolumeNoFile) {
	int32_t ret;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	ret = list_external_volume("test_nexus_5x/....", &list, &number);
	ASSERT_EQ(ret, -1);
}

TEST_F(list_external_volumeTest, ListExternalVolumeEIO) {
	int32_t ret;
	uint64_t number;
	PORTABLE_DIR_ENTRY *list;

	//ssize_t myReturnVals[3] = { 0, 0  };
	//ssize_t (*real_pread) (int, void *, size_t, off_t) = &pread;

	//pread_fake.custom_fake = real_pread;
	ret = list_external_volume("/proc/self/mem", &list, &number);
	ASSERT_EQ(ret, -1);
}

/* End unittest for list_external_volume */

/* Unittest for List_Dir_Inorder */
class list_dir_inorderTest : public ::testing::Test {
	protected:
	int32_t total_children, num_children, limit;
	int32_t end_el_no;
	int64_t end_page_pos;

	void SetUp() {
		total_children = 3003;
		limit = 500;
		end_page_pos = end_el_no = 0;
	}

	void TearDown() {
	}
};

TEST_F(list_dir_inorderTest, FromTreeRootSuccessful) {
	int32_t count = 0;
	int32_t cmp_res;
	FILE *fp;
	char fname[100];
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

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

TEST_F(list_dir_inorderTest, TraverseAllSuccessful) {
	PORTABLE_DIR_ENTRY file_list[limit];

	while (1) {
		num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

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

TEST_F(list_dir_inorderTest, ValidateOffset) {
	limit = 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, limit);
	ASSERT_EQ(end_page_pos, 240);
	ASSERT_EQ(end_el_no, 1);
}

TEST_F(list_dir_inorderTest, MetaIsReg) {
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta_isreg",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -ENOTDIR);
}

TEST_F(list_dir_inorderTest, LimitExceeded) {
	limit = LIST_DIR_LIMIT + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, StartELExceeded) {
	end_el_no = MAX_DIR_ENTRIES_PER_PAGE + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, MetaPathNotExisted) {
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/file_no_existed",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -ENOENT);
}
/* End unittest for List_Dir_Inorder */
