#include "gtest/gtest.h"
#include "params.h"
#include <vector>
#include <sys/xattr.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "mock_params.h"
#include "hcfs_cachebuild.h"
#include "fuseop.h"
#include "global.h"
}

SYSTEM_CONF_STRUCT *system_config;

/* A base class used to be derived from those need to mock cache_usage_node */

class BaseClassForCacheUsageArray : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
	}

	void TearDown()
	{
		printf("TearDown: Free cache_usage\n");
		for (int32_t index = 0 ; index < CACHE_USAGE_NUM_ENTRIES ; index++) {
			CACHE_USAGE_NODE *next;
			CACHE_USAGE_NODE *node; 
			
			node = inode_cache_usage_hash[index];
			while (node) {
				next = node->next_node;
				free(node);
				node = next;
			}

			inode_cache_usage_hash[index] = NULL;
		}

		if (hcfs_system)
			free(hcfs_system);
	}
	
	void generate_mock_cache_node(const int32_t num_node)
	{
		for (int32_t node_id = 0 ; node_id < num_node ; node_id++) {
			CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)malloc(sizeof(CACHE_USAGE_NODE));
			node->this_inode = node_id;
			push_node(node);
		}
	}

private:
	void push_node(CACHE_USAGE_NODE *node)
	{
		int32_t hash_index = node->this_inode % CACHE_USAGE_NUM_ENTRIES;
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

};

/*
	Unittest for cache_usage_hash_init()
 */

class cache_usage_hash_initTest : public BaseClassForCacheUsageArray {

};

TEST_F(cache_usage_hash_initTest, InitEmptyCacheSuccess)
{
	ASSERT_EQ(0, cache_usage_hash_init());
	for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
		ASSERT_EQ(NULL, inode_cache_usage_hash[i]);
	EXPECT_EQ(0, nonempty_cache_hash_entries);
}

TEST_F(cache_usage_hash_initTest, InitNonemptyCacheSuccess)
{
	/* Generate mock data */
	generate_mock_cache_node(100000);
	/* Test */
	ASSERT_EQ(0, cache_usage_hash_init());
	for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
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
		BaseClassForCacheUsageArray::SetUp();

		ASSERT_EQ(0, cache_usage_hash_init());	
		num_inode = 100000;
	}
	int32_t num_inode;
};

TEST_F(return_cache_usage_nodeTest, GetNodeInEmptyCache)
{
	int32_t random_ino;
	srand(time(NULL));
	random_ino = rand() % num_inode;
	EXPECT_EQ(NULL, return_cache_usage_node(random_ino));
}

TEST_F(return_cache_usage_nodeTest, GetNodeSuccess)
{	
	/* Generate mock data */
	generate_mock_cache_node(num_inode);
	/* Test */
	for (int32_t node_id = 0 ; node_id < num_inode ; node_id++) {
		CACHE_USAGE_NODE *tmp_node;
		tmp_node = return_cache_usage_node(node_id);
		ASSERT_TRUE(tmp_node != NULL) << "node_id = " << node_id << std::endl;
		ASSERT_EQ(tmp_node->this_inode, node_id);
	}
}

TEST_F(return_cache_usage_nodeTest, GetNodeFail)
{
	/* Generate mock data */
	generate_mock_cache_node(num_inode);
	/* Test for inode_num > 100000 */
	for (int32_t times = 0 ; times < 500 ; times++) {
		int32_t node_id = rand() % 100000 + num_inode;
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

class insert_cache_usage_nodeTest : public BaseClassForCacheUsageArray {

};

TEST_F(insert_cache_usage_nodeTest, InsertSuccess)
{
	CACHE_USAGE_NODE *now_node;
	bool found_node;

	ASSERT_EQ(0, cache_usage_hash_init());
	for (uint32_t node_id = 0 ; node_id < 500000 ; node_id++) {
		if (node_id % 10000 == 0)
			printf("%u ", node_id);
		if (node_id == 500000-1)
			printf("%u ", node_id);
		fflush(stdout);

		/* Mock data */
		CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)malloc(sizeof(CACHE_USAGE_NODE));
		memset(node, 0, sizeof(CACHE_USAGE_NODE));
		node->this_inode = node_id;
		
		/* Insert */
		insert_cache_usage_node(node_id, node);
		
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

class build_cache_usageTest : public BaseClassForCacheUsageArray {
protected:
	std::vector<CACHE_USAGE_NODE *> answer_node_list;
	std::vector<int32_t> answer_node_num_block;

	void SetUp()
	{
		BaseClassForCacheUsageArray::SetUp();

		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		init_mock_system_config();
		if (!access(BLOCKPATH, F_OK))
			delete_mock_dir(BLOCKPATH);
		else
			mkdir(BLOCKPATH, 0700);
		sem_init(&(hcfs_system->something_to_replace), 0, 2);
	}
	void TearDown()
	{
		size_t i;
		for (i = 0 ; i < answer_node_list.size() ; i++)
			free(answer_node_list[i]);
		delete_mock_dir(BLOCKPATH);
		rmdir(BLOCKPATH);
		free(system_config);

		BaseClassForCacheUsageArray::TearDown();
	}
	void generate_mock_data() {
		int32_t num_node;
		struct stat tmpstat; /* block ops */

		srand(time(NULL));
		num_node = rand() % 1000;
		for (int32_t times = 0 ; times < num_node ; times++) {
			CACHE_USAGE_NODE *answer_node;
			int32_t num_block_per_node;
			int32_t node_id = times * 100 + rand() % 50;

			num_block_per_node = rand() % 50;
			answer_node = (CACHE_USAGE_NODE *)malloc(sizeof(CACHE_USAGE_NODE));
			memset(answer_node, 0, sizeof(CACHE_USAGE_NODE));
			answer_node->this_inode = node_id;
			
			/* Generate a mock block data */
			for (int32_t block_id = 0 ; block_id < num_block_per_node ; block_id++) {
				char path[500];
				int32_t rand_size;
				int32_t fd;

				fetch_block_path(path, node_id, block_id);
				fd = creat(path, 0700);
				rand_size = rand() % 100;
				ftruncate(fd, rand_size);
#ifdef _ANDROID_ENV_
				stat(path, &tmpstat);
#endif
				if (block_id % 2) {
#ifdef _ANDROID_ENV_
					chmod(path, tmpstat.st_mode | S_ISVTX);
#else
					setxattr(path, "user.dirty", "T", 1, XATTR_CREATE);
#endif
					answer_node->dirty_cache_size += rand_size;
				} else {
#ifdef _ANDROID_ENV_
					chmod(path, tmpstat.st_mode & ~S_ISVTX);
#else
					setxattr(path, "user.dirty", "F", 1, XATTR_CREATE);
#endif
					answer_node->clean_cache_size += rand_size;
				}
				if (block_id == num_block_per_node - 1) {
					stat(path, &tmpstat);
					answer_node->last_access_time = tmpstat.st_atime;
					answer_node->last_mod_time = tmpstat.st_mtime;
				}
				close(fd);
			}
			/* Record the answer */
			answer_node_list.push_back(answer_node);
			answer_node_num_block.push_back(num_block_per_node);
		}
	}
	void delete_mock_dir(const char *path)
	{
		DIR *dirptr;
		struct dirent tmp_dirent;
		struct dirent *dirent_ptr;

		dirptr = opendir(path);
		if(dirptr == NULL)
			return;
		readdir_r(dirptr, &tmp_dirent, &dirent_ptr);
		while (dirent_ptr != NULL) {
			char tmp_name[200];
			if(strcmp(tmp_dirent.d_name, ".") == 0 || strcmp(tmp_dirent.d_name, "..") == 0) {
				readdir_r(dirptr, &tmp_dirent, &dirent_ptr);
				continue;
			}
			sprintf(tmp_name, "%s/%s", path, tmp_dirent.d_name);
			if(tmp_dirent.d_type == DT_REG)
				unlink(tmp_name);
			else
				delete_mock_dir(tmp_name);
			readdir_r(dirptr, &tmp_dirent, &dirent_ptr);
		}
		closedir(dirptr);
		rmdir(path);
	}

};

TEST_F(build_cache_usageTest, Nothing_in_Directory)
{
	build_cache_usage();
	for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i++)
		ASSERT_EQ(NULL, inode_cache_usage_hash[i]);
	EXPECT_EQ(0, nonempty_cache_hash_entries);
}

TEST_F(build_cache_usageTest, BuildCacheUsageSuccess)
{
	int32_t semval;
	size_t i;
	/* generate mock data */
	for (i = 0 ; i < NUMSUBDIR ; i++) {
		char tmp_path[100];
		sprintf(tmp_path, "%s/sub_%zu", BLOCKPATH, i);
		ASSERT_EQ(0, mkdir(tmp_path, 0700));
	}
	generate_mock_data();
	
	/* Run function to be tested */	
	build_cache_usage();

	sem_getvalue(&(hcfs_system->something_to_replace), &semval);
	EXPECT_EQ(0, semval);

	/* Check answer */
	for (i = 0 ; i < answer_node_list.size() ; i++) {
		CACHE_USAGE_NODE *ans_node = answer_node_list[i];
		CACHE_USAGE_NODE *now_node;
		ino_t node_id = ans_node->this_inode;
		bool found_node = false;
		bool expected_found_ans;
		
		expected_found_ans = (answer_node_num_block[i] > 0 ? true : false);
		now_node = inode_cache_usage_hash[node_id % CACHE_USAGE_NUM_ENTRIES];
		while (now_node != NULL) {
			if (now_node->this_inode == node_id) {
				found_node = true;
				break;
			}
			now_node = now_node->next_node;
		}

		ASSERT_EQ(expected_found_ans, found_node) << "inode = " << node_id << 
			", num_block = " << answer_node_num_block[i];
		if (found_node == false)
			continue;
		ASSERT_EQ(now_node->clean_cache_size, ans_node->clean_cache_size);
		ASSERT_EQ(now_node->dirty_cache_size, ans_node->dirty_cache_size);
		ASSERT_EQ(now_node->last_access_time, ans_node->last_access_time);
		ASSERT_EQ(now_node->last_mod_time, ans_node->last_mod_time);
	}
}

/*
	End of unittest for build_cache_usage()
 */
