#include "gtest/gtest.h"
#include "params.h"
extern "C" {
#include "mock_tool.h"
#include "hcfs_cachebuild.h"
}

/* A base class used to be derived from those need to mock cache_usage_node */

class BaseClassForCacheUsageArray : public ::testing::Test {
	protected:
		void push_node(CACHE_USAGE_NODE *node)
		{
			int hash_index = node->this_inode % CACHE_USAGE_NUM_ENTRIES;
			CACHE_USAGE_NODE **first = &inode_cache_usage_hash[hash_index];
			if (first == NULL) {
				node->next_node = NULL;
				*first = node;
				nonempty_cache_hash_entries++;
			} else {
				node->next_node = *first;
				*first = node;
			}
		}
		void generate_mock_cache_node(const int num_node)
		{
			for (int node_id = 0 ; node_id < num_node ; node_id++) {
				CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)malloc(sizeof(CACHE_USAGE_NODE));
				node->this_inode = node_id;
				push_node(node);
			}
		}
};

/*
	Unittest for cache_usage_hash_init()
 */

class cache_usage_hash_initTest : public BaseClassForCacheUsageArray {

};

TEST_F(cache_usage_hash_initTest, InitEmptyCacheSuccess)
{
	ASSERT_EQ(0, cache_usage_hash_init());
	for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
		ASSERT_EQ(NULL, inode_cache_usage_hash[i]);
	EXPECT_EQ(0, nonempty_cache_hash_entries);
}

TEST_F(cache_usage_hash_initTest, InitNonemptyCacheSuccess)
{
	/* Generate mock data */
	generate_mock_cache_node(100000);
	/* Test */
	ASSERT_EQ(0, cache_usage_hash_init());
	for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
		ASSERT_EQ(NULL, inode_cache_usage_hash[i]);
	EXPECT_EQ(0, nonempty_cache_hash_entries);
}

/*
	End of unittest for cache_usage_hash_init()
 */

/*
	Unittest for return_cache_usage_node()
 */

class return_cache_usage_nodeTest : public BaseClassForCacheUsageArray {
	protected:
		virtual void SetUp()
		{
			ASSERT_EQ(0, cache_usage_hash_init());	
			num_inode = 100000;
		}
		int num_inode;
};

TEST_F(return_cache_usage_nodeTest, GetNodeInEmptyCache)
{
	int random_ino;
	srand(time(NULL));
	random_ino = rand() % num_inode;
	EXPECT_EQ(NULL, return_cache_usage_node(random_ino));
}

TEST_F(return_cache_usage_nodeTest, GetNodeSuccess)
{	
	/* Generate mock data */
	generate_mock_cache_node(num_inode);
	/* Test */
	for (int node_id = 0 ; node_id < num_inode ; node_id++) {
		CACHE_USAGE_NODE *tmp_node;
		tmp_node = return_cache_usage_node(node_id);
		ASSERT_TRUE(tmp_node != NULL) << "node_id = " << node_id << std::endl;
		ASSERT_EQ(tmp_node->this_inode, node_id);
	}
}

TEST_F(return_cache_usage_nodeTest, GetNodeFail)
{
	/* Generate mock data */
	generate_mock_cache_node(10000);
	/* Test for inode_num > 100000 */
	for (int times = 0 ; times < 500 ; times++) {
		int node_id = rand() % 100000 + num_inode;
		CACHE_USAGE_NODE *tmp_node;
		tmp_node = return_cache_usage_node(node_id);
		ASSERT_EQ(NULL, tmp_node);
	}
}

/*
	End of unittest for return_cache_usage_node()
 */

/*
	Unittest for insert_cache_usage_node()
 */

TEST(insert_cache_usage_nodeTest, InsertSuccess)
{
	CACHE_USAGE_NODE *now_node;
	bool found_node;

	ASSERT_EQ(0, cache_usage_hash_init());
	for (unsigned node_id = 0 ; node_id < 500000 ; node_id++) {
		/* Mock data */
		CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)malloc(sizeof(CACHE_USAGE_NODE));
		memset(node, 0, sizeof(CACHE_USAGE_NODE));
		node->this_inode = node_id;
		/* Insert */
		insert_cache_usage_node(node);
		/* Check whether the node is in the linked-list */
		found_node = false;
		now_node = inode_cache_usage_hash[node_id % CACHE_USAGE_NUM_ENTRIES];
		while (now_node != NULL) {
			if (now_node->this_inode == node_id) {
				found_node = true;
				break;
			}
			now_node = now_node->next_node;
		}
		ASSERT_EQ(true, found_node);
	}
}

/*
	End of unittest for insert_usage_node()
 */

/*
	Unittest for build_cache_usage()
 */

class build_cache_usageTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			init_mock_system_config();
			ASSERT_EQ(0, mkdir(BLOCKPATH, 0700));
		}
		virtual void TearDown()
		{
			rmdir(BLOCKPATH);
		}
};

TEST_F(build_cache_usageTest, Nothing_in_Directory)
{
	build_cache_usage();
	for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
		ASSERT_EQ(NULL, inode_cache_usage_hash[i]);
	EXPECT_EQ(0, nonempty_cache_hash_entries);
}

/*
	End of unittest for build_cache_usage()
 */
