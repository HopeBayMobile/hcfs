#include "gtest/gtest.h"
extern "C" {
#include "lookup_count.h"
#include "global.h"
#include "fuseop.h"
#include <semaphore.h>
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

		for (int i = 0; i< num; i++) {
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
};


class lookup_increaseTest : public InitLookupTableBaseClass {
};

TEST_F(lookup_increaseTest, IncreaseNode_InEmptyTable)
{
	LOOKUP_NODE_TYPE expected_node;
	unsigned ret_count;
	
	expected_node.this_inode = 123;
	expected_node.lookup_count = 567777;
	expected_node.d_type = D_ISDIR;
	expected_node.to_delete = FALSE;
	expected_node.next = NULL;

	/* Run */
	ret_count = lookup_increase(expected_node.this_inode, 
		expected_node.lookup_count, expected_node.d_type);

	/* Verify: find the entry and compare with expected answer */
	int index = expected_node.this_inode % NUM_LOOKUP_ENTRIES;

	EXPECT_EQ(expected_node.lookup_count, ret_count);
	EXPECT_EQ(0, memcmp(&expected_node, lookup_table[index].head, 
		sizeof(LOOKUP_NODE_TYPE)));
}


/*
	End of unittest of lookup_increase()
 */
