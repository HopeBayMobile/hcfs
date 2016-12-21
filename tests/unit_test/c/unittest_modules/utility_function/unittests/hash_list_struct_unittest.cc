#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
#include "hash_list_struct.h"
#include "hash_list_mock_params.h"
#include "fuseop.h"
}
#include "gtest/gtest.h"

int32_t mock_cmp(const void *a, const void *b)
{
	return (((TEST_KEY *)a)->key - ((TEST_KEY *)b)->key);
}

int32_t mock_hash(const void *a)
{
	return (((TEST_KEY *)a)->key) % TEST_HASH_SIZE;
}

/*
 * Unittest for create_hash_list()
 */
class create_hash_listTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	void SetUp()
	{
		hash_list = NULL;
	}

	void TearDown()
	{
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(create_hash_listTest, InvalidParameter)
{
	ASSERT_TRUE(NULL == create_hash_list(NULL, mock_cmp, NULL, 1, 1, 1));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_TRUE(NULL == create_hash_list(mock_hash, NULL, NULL, 1, 1, 1));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_TRUE(NULL ==
		    create_hash_list(mock_hash, mock_cmp, NULL, 0, 1, 1));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_TRUE(NULL ==
		    create_hash_list(mock_hash, mock_cmp, NULL, 1, 0, 1));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_TRUE(NULL ==
		    create_hash_list(mock_hash, mock_cmp, NULL, 1, 1, 0));
	ASSERT_EQ(EINVAL, errno);
}

TEST_F(create_hash_listTest, CreateSuccess)
{
	int32_t value;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));

	/* Check structure */
	ASSERT_TRUE(NULL != hash_list);
	EXPECT_EQ(TEST_HASH_SIZE, hash_list->table_size);
	EXPECT_EQ(sizeof(TEST_KEY), hash_list->key_size);
	EXPECT_EQ(sizeof(TEST_DATA), hash_list->data_size);
	EXPECT_EQ(mock_hash, hash_list->hash_ftn);
	EXPECT_EQ(mock_cmp, hash_list->key_cmp_ftn);
	for (int i = 0; i < TEST_HASH_SIZE; i++) {
		ASSERT_TRUE(NULL == hash_list->hash_table[i].first_entry);
		ASSERT_EQ(0, hash_list->hash_table[i].num_entries);
		sem_getvalue(&hash_list->hash_table[i].bucket_sem, &value);
		ASSERT_EQ(1, value);
	}
	EXPECT_EQ(NULL, hash_list->data_update_ftn);
	EXPECT_EQ(0, hash_list->num_lock_bucket);
	sem_getvalue(&hash_list->table_sem, &value);
	ASSERT_EQ(1, value);
	sem_getvalue(&hash_list->shared_var_sem, &value);
	ASSERT_EQ(1, value);
	sem_getvalue(&hash_list->can_lock_table_sem, &value);
	ASSERT_EQ(1, value);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(create_hash_listTest, CornerCaseSuccess)
{
	int32_t value;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, 1, 1, 1);

	/* Check structure */
	ASSERT_TRUE(NULL != hash_list);
	EXPECT_EQ(1, hash_list->table_size);
	EXPECT_EQ(1, hash_list->key_size);
	EXPECT_EQ(1, hash_list->data_size);
	EXPECT_EQ(mock_hash, hash_list->hash_ftn);
	EXPECT_EQ(mock_cmp, hash_list->key_cmp_ftn);
	for (int i = 0; i < 1; i++) {
		ASSERT_TRUE(NULL == hash_list->hash_table[i].first_entry);
		ASSERT_EQ(0, hash_list->hash_table[i].num_entries);
		sem_getvalue(&hash_list->hash_table[i].bucket_sem, &value);
		ASSERT_EQ(1, value);
	}
	EXPECT_EQ(NULL, hash_list->data_update_ftn);
	EXPECT_EQ(0, hash_list->num_lock_bucket);
	sem_getvalue(&hash_list->table_sem, &value);
	ASSERT_EQ(1, value);
	sem_getvalue(&hash_list->shared_var_sem, &value);
	ASSERT_EQ(1, value);
	sem_getvalue(&hash_list->can_lock_table_sem, &value);
	ASSERT_EQ(1, value);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}
/*
 * End unittest for create_hash_list()
 */

/*
 * Unittest for insert_hash_list_entry()
 */
class insert_hash_list_entryTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	void SetUp()
	{
		hash_list = NULL;
	}

	void TearDown()
	{
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(insert_hash_list_entryTest, InvalidParameter)
{
	TEST_KEY key;
	TEST_DATA data;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);
	ASSERT_TRUE(-EINVAL == insert_hash_list_entry(NULL, &key, &data));
	ASSERT_TRUE(-EINVAL == insert_hash_list_entry(hash_list, NULL, &data));
	ASSERT_TRUE(-EINVAL == insert_hash_list_entry(hash_list, &key, NULL));

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(insert_hash_list_entryTest, InsertionSuccess)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	key.key = TEST_HASH_SIZE + 123;
	data.data = 55667788;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);

	/* Verify */
	node = hash_list->hash_table[123].first_entry;
	EXPECT_EQ(2, hash_list->hash_table[123].num_entries);
	ASSERT_TRUE(NULL != node);
	EXPECT_EQ(TEST_HASH_SIZE + 123, ((TEST_KEY *)(node->key))->key);
	EXPECT_EQ(55667788, ((TEST_DATA *)(node->data))->data);
	node = node->next;
	ASSERT_TRUE(NULL != node);
	EXPECT_EQ(123, ((TEST_KEY *)(node->key))->key);
	EXPECT_EQ(5566, ((TEST_DATA *)(node->data))->data);
	EXPECT_TRUE(NULL == node->next);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(insert_hash_list_entryTest, InsertionFail_EntryExist)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(-EEXIST, ret);

	/* Verify */
	node = hash_list->hash_table[123].first_entry;
	ASSERT_TRUE(NULL != node);
	EXPECT_EQ(123, ((TEST_KEY *)(node->key))->key);
	EXPECT_EQ(5566, ((TEST_DATA *)(node->data))->data);
	EXPECT_TRUE(NULL == node->next);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}
/*
 * End unittest for insert_hash_list_entry()
 */

/*
 * Unittest for lookup_hash_list_entry()
 */
class lookup_hash_list_entryTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	void SetUp()
	{
		hash_list = NULL;
	}

	void TearDown()
	{
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(lookup_hash_list_entryTest, InvalidParameter)
{
	TEST_KEY key;
	TEST_DATA data;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);
	ASSERT_TRUE(-EINVAL == lookup_hash_list_entry(NULL, &key, &data));
	ASSERT_TRUE(-EINVAL == lookup_hash_list_entry(hash_list, NULL, &data));
	ASSERT_TRUE(-EINVAL == lookup_hash_list_entry(hash_list, &key, NULL));

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(lookup_hash_list_entryTest, LookupSuccess)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	key.key = TEST_HASH_SIZE + 123;
	data.data = 55667788;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);

	/* Verify */
	key.key = 123;
	ret = lookup_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(5566, data.data);
	key.key = TEST_HASH_SIZE + 123;
	ret = lookup_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(55667788, data.data);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(lookup_hash_list_entryTest, LookupFail_NOENT)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);

	/* Query */
	key.key = TEST_HASH_SIZE + 123;
	ret = lookup_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(-ENOENT, ret);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(lookup_hash_list_entryTest, LookupEmptyTable_NOENT)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Query */
	for (int i = 0; i < TEST_HASH_SIZE; i++) {
		key.key = i;
		ret = lookup_hash_list_entry(hash_list, &key, &data);
		ASSERT_EQ(-ENOENT, ret);
	}

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}
/*
 * End unittest for lookup_hash_list_entry()
 */

/*
 * Unittest for remove_hash_list_entry()
 */
class remove_hash_list_entryTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	void SetUp()
	{
		hash_list = NULL;
	}

	void TearDown()
	{
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(remove_hash_list_entryTest, InvalidParameter)
{
	TEST_KEY key;
	TEST_DATA data;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);
	ASSERT_TRUE(-EINVAL == remove_hash_list_entry(NULL, &key));
	ASSERT_TRUE(-EINVAL == remove_hash_list_entry(hash_list, NULL));

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(remove_hash_list_entryTest, RemoveSuccess)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	key.key = TEST_HASH_SIZE + 123;
	data.data = 55667788;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);

	/* Test */
	key.key = 123;
	ret = remove_hash_list_entry(hash_list, &key);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(1, hash_list->hash_table[123].num_entries);

	key.key = TEST_HASH_SIZE + 123;
	ret = remove_hash_list_entry(hash_list, &key);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(0, hash_list->hash_table[123].num_entries);

	/* Check structure */
	for (int i = 0; i < TEST_HASH_SIZE; i++) {
		ASSERT_TRUE(NULL == hash_list->hash_table[i].first_entry);
		ASSERT_EQ(0, hash_list->hash_table[i].num_entries);
	}

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}

TEST_F(remove_hash_list_entryTest, RemoveFail_NOENT)
{
	LIST_NODE *node;
	TEST_KEY key;
	TEST_DATA data;
	int32_t ret;

	hash_list = create_hash_list(mock_hash, mock_cmp, NULL, TEST_HASH_SIZE,
				     sizeof(TEST_KEY), sizeof(TEST_DATA));
	ASSERT_TRUE(NULL != hash_list);

	/* Prepare data */
	key.key = 123;
	data.data = 5566;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);
	key.key = TEST_HASH_SIZE + 123;
	data.data = 55667788;
	ret = insert_hash_list_entry(hash_list, &key, &data);
	ASSERT_EQ(0, ret);

	/* Test */
	key.key = TEST_HASH_SIZE * 2 + 123;
	ret = remove_hash_list_entry(hash_list, &key);
	ASSERT_EQ(-ENOENT, ret);
	EXPECT_EQ(2, hash_list->hash_table[123].num_entries);

	/* Check structure */
	node = hash_list->hash_table[123].first_entry;
	EXPECT_EQ(2, hash_list->hash_table[123].num_entries);
	ASSERT_TRUE(NULL != node);
	EXPECT_EQ(TEST_HASH_SIZE + 123, ((TEST_KEY *)(node->key))->key);
	EXPECT_EQ(55667788, ((TEST_DATA *)(node->data))->data);
	node = node->next;
	ASSERT_TRUE(NULL != node);
	EXPECT_EQ(123, ((TEST_KEY *)(node->key))->key);
	EXPECT_EQ(5566, ((TEST_DATA *)(node->data))->data);
	EXPECT_TRUE(NULL == node->next);

	/* Free resource */
	destroy_hash_list(hash_list);
	hash_list = NULL;
}
/*
 * End unittest for remove_hash_list_entry()
 */
