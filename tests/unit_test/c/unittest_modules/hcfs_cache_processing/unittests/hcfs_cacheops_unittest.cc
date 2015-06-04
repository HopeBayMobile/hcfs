#include "gtest/gtest.h"
extern "C" {
#include "hcfs_cacheops.h"
#include "hcfs_cachebuild.h"
#include "mock_params.h"
#include "fuseop.h"
#include "global.h"
}

class run_cache_loopTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 1, 1); 
		sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0); 
		sem_init(&(hcfs_system->check_cache_sem), 1, 0); 
		sem_init(&(hcfs_system->check_next_sem), 1, 0);

	}
	
	void TearDown()
	{
		free(hcfs_system);
	}
	
	void init_mock_cache_usage()
	{
		cache_usage_init();
		nonempty_cache_hash_entries = 0;
		for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
			CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)
				malloc(sizeof(CACHE_USAGE_NODE));
			node->this_inode = i * 5;
			node->next_node = NULL;
			node->last_access_time = 0;
			node->last_mod_time = 0;
			inode_cache_usage_hash[i] = node;
			nonempty_cache_hash_entries++;
		}
		
	}
private:
	void cache_usage_init()
	{
		CACHE_USAGE_NODE *node_ptr, *temp_ptr;

		nonempty_cache_hash_entries = 0;
		for (int count = 0; count < CACHE_USAGE_NUM_ENTRIES; count++) {
			node_ptr = inode_cache_usage_hash[count];
			while (node_ptr != NULL) {
				temp_ptr = node_ptr;
				node_ptr = node_ptr->next_node;
				free(temp_ptr);
			}   
			inode_cache_usage_hash[count] = NULL;
		}   
	}	
};

void *cache_loop_function(void *ptr)
{
	run_cache_loop();
	printf("Test: run_cache_loop() thread leave\n");
	return NULL;
}

TEST_F(run_cache_loopTest, DeleteLocalBlock_WhenFull)
{
	pthread_t thread_id;

	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT; 
	hcfs_system->system_going_down = FALSE;
	init_mock_cache_usage();

	pthread_create(&thread_id, NULL, cache_loop_function, NULL);
	sleep(1);
	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT - 1; 
	hcfs_system->system_going_down = TRUE;
	sleep(1);
}
