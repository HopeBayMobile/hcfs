/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "gtest/gtest.h"
extern "C" {
#include "lookup_count.h"
#include "global.h"
#include "fuseop.h"
#include <semaphore.h>
#include <errno.h>
#include "mock_param.h"
}

/*
	Unittest of lookup_init()
 */
LOOKUP_HEAD_TYPE lookup_table[NUM_LOOKUP_ENTRIES];

TEST(lookup_initTest, InitLookupTableSuccess)
{
	int32_t val;

	/* Run  */
	EXPECT_EQ(0, lookup_init(lookup_table));

	/* Verify */
	for (int32_t i = 0; i < NUM_LOOKUP_ENTRIES; i++) {
		EXPECT_EQ(NULL, lookup_table[i].head);

		sem_getvalue(&lookup_table[i].entry_sem, &val);
		EXPECT_EQ(1, val);
	}
}

/*
	End of unittest of lookup_init()
 */

/*
	Unittest of lookup_increase()
 */

class InitLookupTableBaseClass : public ::testing::Test {
protected:
	void SetUp()
	{
		for (int32_t count = 0; count < NUM_LOOKUP_ENTRIES; count++) { 
			sem_init(&(lookup_table[count].entry_sem), 0, 1); 
			lookup_table[count].head = NULL;
		}
	}

	void TearDown()
	{
		for (int32_t count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
			LOOKUP_NODE_TYPE *prev_node;
			LOOKUP_NODE_TYPE *node = lookup_table[count].head;
			
			while (node) {
				prev_node = node;
				node = node->next;
				free(prev_node);
			}
			sem_destroy(&(lookup_table[count].entry_sem)); 
		}

	}

	void insert_many_mock_nodes(int32_t num)
	{
		LOOKUP_NODE_TYPE *ptr;

		for (int32_t i = 0; i < num; i++) {
			int32_t index = i % NUM_LOOKUP_ENTRIES;
			/* inode i has lookup_count = i */
			ptr = (LOOKUP_NODE_TYPE *)malloc(sizeof(LOOKUP_NODE_TYPE));
			memset(ptr, 0, sizeof(LOOKUP_NODE_TYPE));
			ptr->this_inode = i;
			ptr->lookup_count = i;
			ptr->to_delete = FALSE;
			ptr->d_type = D_ISREG;
			ptr->next = lookup_table[index].head;
			lookup_table[index].head = ptr;
		}
	}

	LOOKUP_NODE_TYPE *find_lookup_entry(ino_t inode)
	{
		int32_t index;
		LOOKUP_NODE_TYPE *ptr = NULL;

		index = inode % NUM_LOOKUP_ENTRIES;	
		ptr = lookup_table[index].head;
		while (ptr) {
			if (ptr->this_inode == inode)
				break;
			ptr = ptr->next;
		}

		return ptr;
	}
};


class lookup_increaseTest : public InitLookupTableBaseClass {
};

TEST_F(lookup_increaseTest, InsertOneNode_InEmptyTable)
{
	LOOKUP_NODE_TYPE *expected_node;
	uint32_t ret_count;
	int32_t index;

	expected_node = (LOOKUP_NODE_TYPE *) malloc(sizeof(LOOKUP_NODE_TYPE));
	memset(expected_node, 0, sizeof(LOOKUP_NODE_TYPE));
	expected_node->this_inode = 123;
	expected_node->lookup_count = 567777;
	expected_node->d_type = D_ISDIR;
	expected_node->to_delete = FALSE;
	expected_node->next = NULL;

	/* Run */
	ret_count = lookup_increase(lookup_table, expected_node->this_inode,
		expected_node->lookup_count, expected_node->d_type);

	/* Verify: find the entry and compare with expected answer */
	index = expected_node->this_inode % NUM_LOOKUP_ENTRIES;

	EXPECT_EQ(expected_node->lookup_count, ret_count);
	EXPECT_EQ(0, memcmp(expected_node, lookup_table[index].head, 
		sizeof(LOOKUP_NODE_TYPE)));

	free(expected_node);
}

TEST_F(lookup_increaseTest, InsertOneNode_InNonemptyTable)
{
	LOOKUP_NODE_TYPE *expected_node;
	LOOKUP_NODE_TYPE *ptr;
	uint32_t num_insert_node;
	uint32_t ret_count;

	num_insert_node = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_node);
	
	expected_node = (LOOKUP_NODE_TYPE *) malloc(sizeof(LOOKUP_NODE_TYPE));
	expected_node->this_inode = NUM_LOOKUP_ENTRIES * 3 + 123;
	expected_node->lookup_count = 567777;
	expected_node->d_type = D_ISDIR;
	expected_node->to_delete = FALSE;
	expected_node->next = NULL;

	/* Run */
	ret_count = lookup_increase(lookup_table, expected_node->this_inode,
		expected_node->lookup_count, expected_node->d_type);

	/* Verify: find the entry and compare with expected answer */
	EXPECT_EQ(expected_node->lookup_count, ret_count);
	
	ptr = find_lookup_entry(expected_node->this_inode);
	
	ASSERT_TRUE(ptr != NULL);
	EXPECT_EQ(expected_node->lookup_count, ptr->lookup_count);
	EXPECT_EQ(expected_node->d_type, ptr->d_type);
	EXPECT_EQ(expected_node->to_delete, ptr->to_delete);
	free(expected_node);
}

TEST_F(lookup_increaseTest, IncreaseManyNode)
{
	LOOKUP_NODE_TYPE *ptr;
	uint32_t num_insert_node;
	uint32_t ret_count;
	uint32_t add_amount;
	char expected_type;

	num_insert_node = NUM_LOOKUP_ENTRIES * 3;

	add_amount = 123;
	expected_type = D_ISREG;

	/* Run */ 
	for (ino_t inode = 1 ; inode <= num_insert_node ; inode++) {
		uint32_t  init_amount = inode;

		ret_count = lookup_increase(lookup_table,
			inode, init_amount, expected_type);
		/* Verify */
		EXPECT_EQ(init_amount, ret_count);
	}

	for (ino_t inode = 1 ; inode <= num_insert_node ; inode++) {
		ret_count = lookup_increase(lookup_table,
			inode, add_amount, expected_type);
		/* Verify */
		EXPECT_EQ(inode + add_amount, ret_count);
	}
	
	/* Verify: find all entry */   
	for (ino_t inode = 1 ; inode <= num_insert_node ; inode++) {
		ptr = find_lookup_entry(inode);	

		ASSERT_TRUE(ptr != NULL);
		EXPECT_EQ(inode + add_amount, ptr->lookup_count);
		EXPECT_EQ(expected_type, ptr->d_type);
	}
}

/*
	End of unittest of lookup_increase()
 */

/*
	Unittest of lookup_decrease()
 */

class lookup_decreaseTest : public InitLookupTableBaseClass {
	
};

TEST_F(lookup_decreaseTest, Arg_need_delete_IsNull)
{
	ino_t inode = 2;
	int32_t amount = 123;
	char d_type;

	/* Run */
	EXPECT_EQ(-1, lookup_decrease(lookup_table,
				inode, amount, &d_type, NULL));
}

TEST_F(lookup_decreaseTest, DecreaseInode_ButNotFound)
{
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int32_t amount = 123;
	char need_delete;
	char d_type;

	/* Run */
	EXPECT_EQ(-EINVAL, lookup_decrease(lookup_table,
				inode, amount, &d_type, &need_delete));
}

TEST_F(lookup_decreaseTest, DecreaseInodeSuccess_CountIsPositiveNumber)
{
	LOOKUP_NODE_TYPE *ptr;	
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int32_t amount;
	char need_delete;
	char d_type;
	uint32_t num_insert_inode;
	int32_t expected_count;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = 12;
	expected_count = inode - amount;

	/* Run */
	EXPECT_EQ(expected_count, lookup_decrease(lookup_table,
				inode, amount, &d_type, &need_delete));

	/* Verify */
	ptr = find_lookup_entry(inode);
	EXPECT_EQ(expected_count, ptr->lookup_count);
	EXPECT_EQ(need_delete, ptr->to_delete);
	EXPECT_EQ(d_type, ptr->d_type);
}

TEST_F(lookup_decreaseTest, DecreaseInodeSuccess_CountIsZero)
{
	LOOKUP_NODE_TYPE *ptr;	
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int32_t amount;
	char need_delete;
	char d_type;
	uint32_t num_insert_inode;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = inode;

	/* Run */
	EXPECT_EQ(0, lookup_decrease(lookup_table,
			inode, amount, &d_type, &need_delete));

	/* Verify */
	ptr = find_lookup_entry(inode);
	EXPECT_EQ(NULL, ptr);
}

TEST_F(lookup_decreaseTest, DecreaseInodeSuccess_CountIsNegativeNumber)
{
	LOOKUP_NODE_TYPE *ptr;	
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int32_t amount;
	char need_delete;
	char d_type;
	uint32_t num_insert_inode;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = inode * 2;

	/* Run */
	EXPECT_EQ(0, lookup_decrease(lookup_table,
			inode, amount, &d_type, &need_delete));

	/* Verify */
	ptr = find_lookup_entry(inode);
	EXPECT_EQ(NULL, ptr);
}

/*
	End of unittest of lookup_decrease()
 */

/*
	Unittest of lookup_markdelete()
 */

class lookup_markdeleteTest : public InitLookupTableBaseClass {

};

TEST_F(lookup_markdeleteTest, MarkDeleteFail_LookupEntryNotFound)
{	
	uint32_t num_insert_inode;
	ino_t inode_markdelete;

	/* Insert many inodes */
	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	inode_markdelete = NUM_LOOKUP_ENTRIES * 3 + 100;

	/* Run 3 times */
	EXPECT_EQ(-EINVAL, lookup_markdelete(lookup_table, inode_markdelete));
	EXPECT_EQ(-EINVAL, lookup_markdelete(lookup_table,
				inode_markdelete + 1));
	EXPECT_EQ(-EINVAL, lookup_markdelete(lookup_table,
				inode_markdelete + 2));
}

TEST_F(lookup_markdeleteTest, MarkDeleteSuccess)
{	
	uint32_t num_insert_inode;

	/* Insert many inodes */
	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);

	/* Run many times */
	for (ino_t inode = 0; inode < num_insert_inode; inode++) 
		// All vars not_delete is set as FALSE before running
		EXPECT_EQ(0, lookup_markdelete(lookup_table, inode));

	/* Verify */
	for (ino_t inode = 0; inode < num_insert_inode; inode++) {
		LOOKUP_NODE_TYPE *ptr;

		ptr = find_lookup_entry(inode);
		ASSERT_EQ(TRUE, ptr->to_delete);
	}
}

/*
	End of unittest of lookup_markdelete()
 */

/*
	Unittest of lookup_destroy()
 */

class lookup_destroyTest : public InitLookupTableBaseClass {
protected:
	void SetUp()
	{
		InitLookupTableBaseClass::SetUp();
		
		check_actual_delete_table = NULL;
	}

	void TearDown()
	{
		if (check_actual_delete_table)
			free(check_actual_delete_table);

		InitLookupTableBaseClass::TearDown();
	}
};

TEST_F(lookup_destroyTest, DestroyEmptyTableSuccess)
{
	MOUNT_T mount_t;

	/* Run */
	EXPECT_EQ(0, lookup_destroy(lookup_table, &mount_t));
}

TEST_F(lookup_destroyTest, DestroyTableSuccess)
{
	uint32_t num_insert_inode;
	MOUNT_T mount_t;

	/* Insert many inodes */
	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	check_actual_delete_table = (char *)malloc(num_insert_inode * sizeof(char));
	memset(check_actual_delete_table, FALSE, num_insert_inode * sizeof(char));

	/* Run */
	EXPECT_EQ(0, lookup_destroy(lookup_table, &mount_t));
	
	/* Verify */
	for (ino_t inode = 0; inode < num_insert_inode; inode++) {
		EXPECT_EQ(TRUE, check_actual_delete_table[inode]);
	}
}

/*
	End of unittest of lookup_destroy()
 */
