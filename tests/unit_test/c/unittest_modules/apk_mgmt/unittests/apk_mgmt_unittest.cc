/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt_unittest.cc
* Abstract:
* 	The unittest code for:
* 	1. Mimicing Full APK using Minimal APK
*
* Revision History
* 2016/12/07 Jethro Add unittest for apk_mgmt.c
*
**************************************************************************/
#include <gtest/gtest.h>

extern "C" {
#include "hfuse_system.h"
#include "fuseop.h"
#include "apk_mgmt.h"
#include "mount_manager.h"

#include "ut_helper.h"
#include "apk_mgmt_mock.h"
}

/*
 * Google Tests
 */

void reset_fake_functions(void)
{
	reset_ut_helper();
}
void reset_unittest_env(void)
{
	hcfs_system->system_going_down = FALSE;
}

class UnittestEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp()
	{
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)calloc(1, sizeof(SYSTEM_DATA_HEAD));
		reset_fake_functions();
#define CUSTOM_FAKE(F) F##_fake.custom_fake = custom_##F
		CUSTOM_FAKE(create_hash_list);
		CUSTOM_FAKE(insert_hash_list_entry);
#undef CUSTOM_FAKE
	}

	virtual void TearDown() {}
};

::testing::Environment *const unitest_env =
    ::testing::AddGlobalTestEnvironment(new UnittestEnvironment);

class NotifyBufferSetUpAndTearDown : public ::testing::Test
{
	protected:
	virtual void SetUp() {
	}

	virtual void TearDown()
	{
		reset_unittest_env();
		reset_fake_functions();
	}
};

TEST(ToggleMinimalApkTest, On)
{
	create_hash_list_success = 1;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	ASSERT_EQ(toggle_use_minimal_apk(1), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 1);
}
TEST(ToggleMinimalApkTest, Off)
{
	create_hash_list_success = 1;
	minapk_lookup_table = (HASH_LIST * )1;
	minapk_lookup_iter = (HASH_LIST_ITERATOR *)1;
	hcfs_system->use_minimal_apk = 1;
	EXPECT_EQ(toggle_use_minimal_apk(0), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 0);
}

/**
 * Unittest for create_minapk_table()
 */
class create_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		create_hash_list_success = 0;
		minapk_lookup_table = NULL;
	}

	void TearDown()
	{
		create_hash_list_success = 0;
		minapk_lookup_table = NULL;
	}
};

TEST_F(create_minapk_tableTest, CreateSuccess)
{
	int32_t ret;

	create_hash_list_success = 1;
	ret = create_minapk_table();
	EXPECT_EQ(0, ret);
	EXPECT_EQ((HASH_LIST *)1, minapk_lookup_table);
}

TEST_F(create_minapk_tableTest, CreateFail)
{
	int32_t ret;

	create_hash_list_success = 0;
	ret = create_minapk_table();
	EXPECT_EQ(-ENOMEM, ret);
	EXPECT_EQ(NULL, minapk_lookup_table);
}
/**
 * End unittest for create_minapk_table()
 */

/**
 * Unittest for destroy_minapk_table()
 */
class destroy_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		minapk_lookup_table = NULL;
	}

	void TearDown()
	{
		minapk_lookup_table = NULL;
	}
};

TEST_F(destroy_minapk_tableTest, DestroySuccess)
{
	minapk_lookup_table = (HASH_LIST * )1;
	destroy_minapk_table();

	EXPECT_EQ(NULL, minapk_lookup_table);
}
/**
 * End unittest for destroy_minapk_table()
 */

/**
 * Unittest for destroy_minapk_table()
 */
class insert_minapk_dataTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		insert_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}

	void TearDown()
	{
		insert_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}
};

TEST_F(insert_minapk_dataTest, Minapktable_NotExist)
{
	EXPECT_EQ(-EINVAL, insert_minapk_data(2, "test_apk_name", 123));
}

TEST_F(insert_minapk_dataTest, InsertFail_EntryExist)
{
	minapk_lookup_table = (HASH_LIST * )1;
	insert_minapk_data_success = 0;
	EXPECT_EQ(-EEXIST, insert_minapk_data(2, "test_apk_name", 123));
}

TEST_F(insert_minapk_dataTest, InsertSuccess)
{
	minapk_lookup_table = (HASH_LIST * )1;
	insert_minapk_data_success = 1;
	EXPECT_EQ(0, insert_minapk_data(2, "test_apk_name", 123));
}
