#include "gtest/gtest.h"
extern "C" {
#include "lookup_count.h"
#include "global.h"
#include "fuseop.h"
#include <semaphore.h>
#include <errno.h>
}

/*
	Unittest of lookup_init()
 */

TEST(lookup_initTest, InitLookupTableSuccess)
{
	int val;

	/* Run  */
	EXPECT_EQ(0, lookup_init());

	/* Verify */
	for (int i = 0; i < NUM_LOOKUP_ENTRIES; i++) {
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
		for (int count = 0; count < NUM_LOOKUP_ENTRIES; count++) { 
			sem_init(&(lookup_table[count].entry_sem), 0, 1); 
			lookup_table[count].head = NULL;
		}
	}

	void TearDown()
	{
		for (int count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
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

	void insert_many_mock_nodes(int num)
	{
		LOOKUP_NODE_TYPE *ptr;

		for (int i = 0; i < num; i++) {
			int index = i % NUM_LOOKUP_ENTRIES;
			/* inode i has lookup_count = i */
			ptr = (LOOKUP_NODE_TYPE *)malloc(sizeof(LOOKUP_NODE_TYPE));
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
		int index;
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
	LOOKUP_NODE_TYPE expected_node;
	unsigned ret_count;
	int index;
	
	expected_node.this_inode = 123;
	expected_node.lookup_count = 567777;
	expected_node.d_type = D_ISDIR;
	expected_node.to_delete = FALSE;
	expected_node.next = NULL;

	/* Run */
	ret_count = lookup_increase(expected_node.this_inode, 
		expected_node.lookup_count, expected_node.d_type);

	/* Verify: find the entry and compare with expected answer */
	index = expected_node.this_inode % NUM_LOOKUP_ENTRIES;

	EXPECT_EQ(expected_node.lookup_count, ret_count);
	EXPECT_EQ(0, memcmp(&expected_node, lookup_table[index].head, 
		sizeof(LOOKUP_NODE_TYPE)));

}

TEST_F(lookup_increaseTest, InsertOneNode_InNonemptyTable)
{
	LOOKUP_NODE_TYPE expected_node;
	LOOKUP_NODE_TYPE *ptr;
	unsigned num_insert_node;
	unsigned ret_count;

	num_insert_node = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_node);
	
	expected_node.this_inode = NUM_LOOKUP_ENTRIES * 3 + 123;
	expected_node.lookup_count = 567777;
	expected_node.d_type = D_ISDIR;
	expected_node.to_delete = FALSE;
	expected_node.next = NULL;

	/* Run */
	ret_count = lookup_increase(expected_node.this_inode, 
		expected_node.lookup_count, expected_node.d_type);

	/* Verify: find the entry and compare with expected answer */
	EXPECT_EQ(expected_node.lookup_count, ret_count);
	
	ptr = find_lookup_entry(expected_node.this_inode);
	
	ASSERT_TRUE(ptr != NULL);
	EXPECT_EQ(expected_node.lookup_count, ptr->lookup_count);
	EXPECT_EQ(expected_node.d_type, ptr->d_type);
	EXPECT_EQ(expected_node.to_delete, ptr->to_delete);
}

TEST_F(lookup_increaseTest, IncreaseManyNode)
{
	LOOKUP_NODE_TYPE *ptr;
	unsigned num_insert_node;
	unsigned ret_count;
	unsigned add_amount;
	char expected_type;

	num_insert_node = NUM_LOOKUP_ENTRIES * 3;

	add_amount = 123;
	expected_type = D_ISREG;

	/* Run */ 
	for (ino_t inode = 1 ; inode <= num_insert_node ; inode++) {
		unsigned  init_amount = inode;

		ret_count = lookup_increase(inode, init_amount, expected_type);
		/* Verify */
		EXPECT_EQ(init_amount, ret_count);
	}

	for (ino_t inode = 1 ; inode <= num_insert_node ; inode++) {
		ret_count = lookup_increase(inode, add_amount, expected_type);
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
	int amount = 123;
	char d_type;

	/* Run */
	EXPECT_EQ(-1, lookup_decrease(inode, amount, &d_type, NULL));
}

TEST_F(lookup_decreaseTest, DecreaseInode_ButNotFound)
{
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int amount = 123;
	char need_delete;
	char d_type;

	/* Run */
	EXPECT_EQ(-EINVAL, lookup_decrease(inode, amount, &d_type, &need_delete));
}

TEST_F(lookup_decreaseTest, DecreaseInodeSuccess_CountIsPositiveNumber)
{
	LOOKUP_NODE_TYPE *ptr;	
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int amount;
	char need_delete;
	char d_type;
	unsigned num_insert_inode;
	int expected_count;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = 12;
	expected_count = inode - amount;

	/* Run */
	EXPECT_EQ(expected_count, lookup_decrease(inode, amount, &d_type, &need_delete));

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
	int amount;
	char need_delete;
	char d_type;
	unsigned num_insert_inode;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = inode;

	/* Run */
	EXPECT_EQ(0, lookup_decrease(inode, amount, &d_type, &need_delete));

	/* Verify */
	ptr = find_lookup_entry(inode);
	EXPECT_EQ(NULL, ptr);
}

TEST_F(lookup_decreaseTest, DecreaseInodeSuccess_CountIsNegativeNumber)
{
	LOOKUP_NODE_TYPE *ptr;	
	ino_t inode = NUM_LOOKUP_ENTRIES * 1.5;
	int amount;
	char need_delete;
	char d_type;
	unsigned num_insert_inode;

	num_insert_inode = NUM_LOOKUP_ENTRIES * 3;
	insert_many_mock_nodes(num_insert_inode);
	
	amount = inode * 2;

	/* Run */
	EXPECT_EQ(0, lookup_decrease(inode, amount, &d_type, &need_delete));

	/* Verify */
	ptr = find_lookup_entry(inode);
	EXPECT_EQ(NULL, ptr);
}

/*
	End of unittest of lookup_decrease()
 */
