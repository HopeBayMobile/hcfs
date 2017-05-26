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
#include "apk_mgmt.h"
#include "fuseop.h"
#include "hfuse_system.h"
#include "mount_manager.h"

#include "apk_mgmt_mock.h"
#include "ut_helper.h"
}

/* UT flow control */
int32_t iterate_hashlist_n_times = 0;
HASH_LIST_ITERATOR *mock_next(HASH_LIST_ITERATOR *iter);

/*
 * Google Tests
 */

void reset_unittest_env(void)
{
	create_hash_list_success = 0;
	init_hashlist_iter_ret = NULL;
	minapk_lookup_table = NULL;
	search_mount_success = 0;
	hcfs_system->system_going_down = FALSE;
}

class UnittestEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp()
	{
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)calloc(1, sizeof(SYSTEM_DATA_HEAD));
#define CUSTOM_FAKE(F) F##_fake.custom_fake = custom_##F
		CUSTOM_FAKE(create_hash_list);
		CUSTOM_FAKE(insert_hash_list_entry);
		CUSTOM_FAKE(lookup_hash_list_entry);
		CUSTOM_FAKE(remove_hash_list_entry);
		CUSTOM_FAKE(init_hashlist_iter);
		CUSTOM_FAKE(search_mount);
#undef CUSTOM_FAKE
		reset_unittest_env();
		reset_ut_helper();
		hcfs_system->sync_paused = 0;
	}

	virtual void TearDown() {}
};

::testing::Environment *const unitest_env =
    ::testing::AddGlobalTestEnvironment(new UnittestEnvironment);

class NotifyBufferSetUpAndTearDown : public ::testing::Test
{
	protected:
	virtual void SetUp() {}

	virtual void TearDown()
	{
		reset_unittest_env();
		reset_ut_helper();
	}
};

TEST(ToggleMinimalApkTest, ToggleOnSucceed)
{
	create_hash_list_success = 1;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	hcfs_system->set_minimal_apk = 0;

	ASSERT_EQ(toggle_use_minimal_apk(1), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 1);
}

TEST(ToggleMinimalApkTest, ToggleOnTwiceSucceed)
{
	create_hash_list_success = 1;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	hcfs_system->set_minimal_apk = 0;

	ASSERT_EQ(toggle_use_minimal_apk(1), 0);
	ASSERT_EQ(toggle_use_minimal_apk(1), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 1);
}

TEST(ToggleMinimalApkTest, ToggleOnFailed)
{
	create_hash_list_success = 0;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	hcfs_system->set_minimal_apk = 0;

	ASSERT_EQ(toggle_use_minimal_apk(1), -ENOMEM);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 0);
}

TEST(ToggleMinimalApkTest, ToggleOffSucceed)
{
	hcfs_system->set_minimal_apk = 1;
	search_mount_success = 1;
	hcfs_system->use_minimal_apk = 1;

	/* table */
	create_hash_list_success = 1;
	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 1;
	init_hashlist_iter_ret =
	    (HASH_LIST_ITERATOR *)calloc(sizeof(HASH_LIST_ITERATOR), 1);
	init_hashlist_iter_ret->base.next = (void *(*)(void *)) & mock_next;
	iterate_hashlist_n_times = 1;

	/* Test */
	EXPECT_EQ(0, toggle_use_minimal_apk(0));
	EXPECT_EQ(false, hcfs_system->use_minimal_apk);

	free(init_hashlist_iter_ret);
}

TEST(ToggleMinimalApkTest, ToggleOffEmptylist)
{
	hcfs_system->set_minimal_apk = 1;
	search_mount_success = 1;
	hcfs_system->use_minimal_apk = 1;

	/* table */
	create_hash_list_success = 1;
	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 1;
	init_hashlist_iter_ret =
	    (HASH_LIST_ITERATOR *)calloc(sizeof(HASH_LIST_ITERATOR), 1);
	init_hashlist_iter_ret->base.next = (void *(*)(void *)) & mock_next;
	iterate_hashlist_n_times = 0;

	/* Test */
	EXPECT_EQ(0, toggle_use_minimal_apk(0));
	EXPECT_EQ(hcfs_system->use_minimal_apk, false);

	free(init_hashlist_iter_ret);
}

TEST(ToggleMinimalApkTest, ToggleOffFailedOnInvalidMinapk)
{
	create_hash_list_success = 1;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	toggle_use_minimal_apk(1);

	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 0;
	EXPECT_EQ(-ENOMEM, toggle_use_minimal_apk(0));
	EXPECT_EQ(false, hcfs_system->use_minimal_apk);
}

TEST(ToggleMinimalApkTest, ToggleOffFailedOnSearchMount)
{
	create_hash_list_success = 1;
	minapk_lookup_table = NULL;
	hcfs_system->use_minimal_apk = 0;
	toggle_use_minimal_apk(1);

	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 1;
	search_mount_success = 0;
	EXPECT_EQ(-1, toggle_use_minimal_apk(0));
	EXPECT_EQ(false, hcfs_system->use_minimal_apk);
}

/**
 * Unittest for create_minapk_table()
 */
class create_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp() {}

	void TearDown()
	{
		reset_unittest_env();
		reset_ut_helper();
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
	void SetUp() { minapk_lookup_table = NULL; }

	void TearDown() { minapk_lookup_table = NULL; }
};

TEST_F(destroy_minapk_tableTest, DestroySuccess)
{
	minapk_lookup_table = (HASH_LIST *)1;
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
	minapk_lookup_table = (HASH_LIST *)1;
	insert_minapk_data_success = 0;
	EXPECT_EQ(-EEXIST, insert_minapk_data(2, "test_apk_name", 123));
}

TEST_F(insert_minapk_dataTest, InsertSuccess)
{
	minapk_lookup_table = (HASH_LIST *)1;
	insert_minapk_data_success = 1;
	EXPECT_EQ(0, insert_minapk_data(2, "test_apk_name", 123));
}
/**
 * End unittest for destroy_minapk_table()
 */

/**
 * Unittest for query_minapk_table()
 */
class query_minapk_dataTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		query_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}

	void TearDown()
	{
		query_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}
};

TEST_F(query_minapk_dataTest, Minapktable_NotExist)
{
	ino_t ino = 0;

	EXPECT_EQ(-EINVAL, query_minapk_data(2, "test_apk_name", &ino));
}

TEST_F(query_minapk_dataTest, QueryFail_EntryNotExist)
{
	ino_t ino = 0;

	minapk_lookup_table = (HASH_LIST *)1;
	query_minapk_data_success = 0;
	EXPECT_EQ(-ENOENT, query_minapk_data(2, "test_apk_name", &ino));
}

TEST_F(query_minapk_dataTest, QuerySuccess)
{
	ino_t ino = 0;

	minapk_lookup_table = (HASH_LIST *)1;
	query_minapk_data_success = 1;
	EXPECT_EQ(0, query_minapk_data(2, "test_apk_name", &ino));
	EXPECT_EQ(5566, ino);
}
/**
 * End unittest for destroy_minapk_table()
 */

/**
 * Unittest for remove_minapk_data()
 */
class remove_minapk_dataTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		remove_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}

	void TearDown()
	{
		remove_minapk_data_success = 0;
		minapk_lookup_table = NULL;
	}
};

TEST_F(remove_minapk_dataTest, Minapktable_NotExist)
{
	EXPECT_EQ(-EINVAL, remove_minapk_data(2, "test_apk_name"));
}

TEST_F(remove_minapk_dataTest, RemoveFail_EntryNotExist)
{
	minapk_lookup_table = (HASH_LIST *)1;
	remove_minapk_data_success = 0;
	EXPECT_EQ(-ENOENT, remove_minapk_data(2, "test_apk_name"));
}

TEST_F(remove_minapk_dataTest, RemoveSuccess)
{
	minapk_lookup_table = (HASH_LIST *)1;
	remove_minapk_data_success = 1;
	EXPECT_EQ(0, remove_minapk_data(2, "test_apk_name"));
}
/**
 * End unittest for remove_minapk_data()
 */

/**
 * Unittest for init_iterate_minapk_table()
 */
class init_iterate_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		init_hashlist_iter_success = 0;
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}

	void TearDown()
	{
		init_hashlist_iter_success = 0;
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}
};

TEST_F(init_iterate_minapk_tableTest, Minapktable_NotExist)
{
	EXPECT_EQ(-EINVAL, init_iterate_minapk_table());
}

TEST_F(init_iterate_minapk_tableTest, InitFail)
{
	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 0;
	EXPECT_EQ(-ENOMEM, init_iterate_minapk_table());
}

TEST_F(init_iterate_minapk_tableTest, InitSuccess)
{
	minapk_lookup_table = (HASH_LIST *)1;
	init_hashlist_iter_success = 1;
	EXPECT_EQ(0, init_iterate_minapk_table());
	EXPECT_EQ((HASH_LIST_ITERATOR *)1, minapk_lookup_iter);
}
/**
 * End unittest for init_iterate_minapk_table()
 */

/**
 * Unittest for iterate_minapk_table()
 */
class iterate_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		iterate_hashlist_n_times = 0;
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}

	void TearDown()
	{
		iterate_hashlist_n_times = 0;
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}
};

/* Helper for hash list iterator */
HASH_LIST_ITERATOR *mock_next(HASH_LIST_ITERATOR *iter)
{
	if (iterate_hashlist_n_times > 0) {
		iter->now_key = malloc(sizeof(MIN_APK_LOOKUP_KEY));
		((MIN_APK_LOOKUP_KEY *)(iter->now_key))->parent_ino = 1234;
		strcpy(((MIN_APK_LOOKUP_KEY *)(iter->now_key))->apk_name,
		       "test_apk_name");
		iter->now_data = malloc(sizeof(MIN_APK_LOOKUP_DATA));
		((MIN_APK_LOOKUP_DATA *)(iter->now_data))->min_apk_ino = 5566;
		iterate_hashlist_n_times--;
		return iter;
	}

	errno = ENOENT;
	return NULL;
}

TEST_F(iterate_minapk_tableTest, InvalidParameter)
{
	ino_t parent_ino, minapk_ino;
	char apk_file[400];

	minapk_lookup_table = (HASH_LIST *)1;
	minapk_lookup_iter = NULL;
	EXPECT_EQ(-EINVAL,
		  iterate_minapk_table(&parent_ino, apk_file, &minapk_ino));
	minapk_lookup_table = NULL;
	minapk_lookup_iter = (HASH_LIST_ITERATOR *)1;
	EXPECT_EQ(-EINVAL,
		  iterate_minapk_table(&parent_ino, apk_file, &minapk_ino));
	minapk_lookup_table = (HASH_LIST *)1;
	minapk_lookup_iter = (HASH_LIST_ITERATOR *)1;
	EXPECT_EQ(-EINVAL, iterate_minapk_table(NULL, apk_file, &minapk_ino));
	EXPECT_EQ(-EINVAL,
		  iterate_minapk_table(&parent_ino, NULL, &minapk_ino));
	EXPECT_EQ(-EINVAL, iterate_minapk_table(&parent_ino, apk_file, NULL));
}

TEST_F(iterate_minapk_tableTest, IteratorNoEntry)
{
	ino_t parent_ino, minapk_ino;
	char apk_file[400];

	minapk_lookup_table = (HASH_LIST *)1;
	minapk_lookup_iter =
	    (HASH_LIST_ITERATOR *)calloc(sizeof(HASH_LIST_ITERATOR), 1);
	minapk_lookup_iter->base.next = (void *(*)(void *)) & mock_next;

	/* Test */
	EXPECT_EQ(-ENOENT,
		  iterate_minapk_table(&parent_ino, apk_file, &minapk_ino));

	free(minapk_lookup_iter);
}

TEST_F(iterate_minapk_tableTest, IterateSuccess)
{
	ino_t parent_ino, minapk_ino;
	char apk_file[400];

	minapk_lookup_table = (HASH_LIST *)1;
	minapk_lookup_iter =
	    (HASH_LIST_ITERATOR *)calloc(sizeof(HASH_LIST_ITERATOR), 1);
	minapk_lookup_iter->base.next = (void *(*)(void *)) & mock_next;

	iterate_hashlist_n_times = 1;

	/* Test */
	ASSERT_EQ(0, iterate_minapk_table(&parent_ino, apk_file, &minapk_ino));
	EXPECT_EQ(1234, parent_ino);
	EXPECT_EQ(0, strcmp(apk_file, "test_apk_name"));
	EXPECT_EQ(5566, minapk_ino);

	free(minapk_lookup_iter->now_key);
	free(minapk_lookup_iter->now_data);
	free(minapk_lookup_iter);
}
/**
 * End unittest for iterate_minapk_table()
 */

/**
 * Unittest for end_iterate_minapk_table()
 */
class end_iterate_minapk_tableTest : public ::testing::Test
{
	protected:
	void SetUp()
	{
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}

	void TearDown()
	{
		minapk_lookup_table = NULL;
		minapk_lookup_iter = NULL;
	}
};

TEST_F(end_iterate_minapk_tableTest, NULL_hashlist)
{
	minapk_lookup_table = NULL;
	minapk_lookup_iter = (HASH_LIST_ITERATOR *)1;

	end_iterate_minapk_table();
	EXPECT_EQ((HASH_LIST_ITERATOR *)1, minapk_lookup_iter);
}

TEST_F(end_iterate_minapk_tableTest, DestroyIteratorSuccess)
{
	minapk_lookup_table = (HASH_LIST *)1;
	minapk_lookup_iter = (HASH_LIST_ITERATOR *)1;

	end_iterate_minapk_table();
	EXPECT_EQ(NULL, minapk_lookup_iter);
}
/**
 * End unittest for end_iterate_minapk_table()
 */
