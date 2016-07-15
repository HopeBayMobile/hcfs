#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "gtest/gtest.h"
#include "mock_params.h"
extern "C" {
#include "tocloud_tools.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "atomic_tocloud.h"
//#include "utils.h"
//int32_t fetch_meta_path(char *pathname, ino_t this_inode);
}
//extern int32_t fetch_meta_path(char *pathname, ino_t this_inode);

/**
 * Unittest of pull_retry_inode()
 */
class pull_retry_inodeTest : public ::testing::Test {
protected:
	IMMEDIATELY_RETRY_LIST list;

	void SetUp()
	{
		memset(&list, 0, sizeof(IMMEDIATELY_RETRY_LIST));
		list.list_size = 8;
		list.retry_inode = (ino_t *) malloc(sizeof(ino_t) *
				list.list_size);
	}

	void TearDown()
	{
		free(list.retry_inode);
	}
};

TEST_F(pull_retry_inodeTest, PullManyTimesSuccess)
{
	int num_retry;

	for (int i = 0; i < list.list_size; i++)
		list.retry_inode[i] = i + 1;
	list.num_retry = list.list_size;

	num_retry = list.list_size;

	for (int i = 1; i <= list.list_size; i++) {
		ASSERT_EQ(i, pull_retry_inode(&list));
		ASSERT_EQ(num_retry - i, list.num_retry);
	}
	EXPECT_EQ(0, pull_retry_inode(&list));
	EXPECT_EQ(0, list.num_retry);
}

TEST_F(pull_retry_inodeTest, PullEmptyList)
{
	EXPECT_EQ(0, pull_retry_inode(&list));
	EXPECT_EQ(0, list.num_retry);
}
/**
 * End of unittest of pull_retry_inode()
 */

/**
 * Unittest of push_retry_inode()
 */
class push_retry_inodeTest : public ::testing::Test {
protected:
	IMMEDIATELY_RETRY_LIST list;

	void SetUp()
	{
		memset(&list, 0, sizeof(IMMEDIATELY_RETRY_LIST));
		list.list_size = 8;
		list.retry_inode = (ino_t *) calloc(sizeof(ino_t) *
				list.list_size, 1);
	}

	void TearDown()
	{
		free(list.retry_inode);
	}
};

TEST_F(push_retry_inodeTest, PushManyTimesSuccess)
{	
	for (int i = 1; i <= list.list_size; i++) {
		push_retry_inode(&list, i);
		ASSERT_EQ(i, list.num_retry);
	}

	for (int i = 1; i <= list.list_size; i++) {
		ASSERT_EQ(i, list.retry_inode[i - 1]);
	}
}
/**
 * End of unittest of push_retry_inode()
 */

/**
 * Unittest of change_block_status_to_BOTH()
 */
class change_block_status_to_BOTHTest : public ::testing::Test {
protected:
	void SetUp()
	{
		if (!access("tocloud_tools_test_folder", F_OK))
			system("rm -rf tocloud_tools_test_folder");
		mkdir("tocloud_tools_test_folder", 0600);
	}

	void TearDown()
	{
		if (!access("tocloud_tools_test_folder", F_OK))
			system("rm -rf tocloud_tools_test_folder");
	}
};

TEST_F(change_block_status_to_BOTHTest, ChangeStatusSuccess)
{
	char path[300];
	ino_t inode = 5;

	sprintf(path, "tocloud_tools_test_folder/mock_meta_%"PRIu64,
			(uint64_t)inode);
}
