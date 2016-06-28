/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
extern "C" {
#include "global.h"
#include "fuseop.h"
#include "time.h"
#include "parser.h"
}

class pyhcfsTest : public ::testing::Test {
	protected:
	void SetUp() {
	}

	void TearDown() {
	}
};

TEST_F(pyhcfsTest, Traverse_Dir_Btree) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}
TEST_F(pyhcfsTest, List_External_Volume) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}
TEST_F(pyhcfsTest, Parse_Meta) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}

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

TEST_F(list_dir_inorderTest, FromTreeRootSuccessful)
{
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, limit);
}

TEST_F(list_dir_inorderTest, TraverseAllSuccessful)
{
	int32_t idx = 0;
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

TEST_F(list_dir_inorderTest, LimitExceeded)
{
	limit = LIST_DIR_LIMIT + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, StartELExceeded)
{
	end_el_no = MAX_DIR_ENTRIES_PER_PAGE + 1;
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/meta",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -EINVAL);
}

TEST_F(list_dir_inorderTest, MetaPathNotExisted)
{
	PORTABLE_DIR_ENTRY file_list[limit];

	num_children = list_dir_inorder("test_nexus_5x/file_no_existed",
				end_page_pos, end_el_no, limit, &end_page_pos,
				&end_el_no, &(file_list[0]));

	ASSERT_EQ(num_children, -ENOENT);
}
/* End unittest for List_Dir_Inorder */
