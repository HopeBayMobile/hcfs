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
		char meta_name[50];
		for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
			sprintf(meta_name, "/tmp/run_cache_loop_filemeta%d", 
				(i + 1) * 5);
			unlink(meta_name);
		}
		
		free(hcfs_system);
	}
	
	void init_mock_cache_usage()
	{
		cache_usage_init();
		nonempty_cache_hash_entries = 0;
		for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
			CACHE_USAGE_NODE *node = (CACHE_USAGE_NODE *)
				malloc(sizeof(CACHE_USAGE_NODE));
			node->this_inode = (i + 1) * 5;
			node->next_node = NULL;
			node->last_access_time = 0;
			node->last_mod_time = 0;
			node->clean_cache_size = 100;
			inode_cache_usage_hash[i] = node;
			nonempty_cache_hash_entries++;

			genetate_mock_meta(node->this_inode);
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
	
	void genetate_mock_meta(ino_t inode)
	{
		struct stat file_stat;
		FILE_META_TYPE file_meta;
		BLOCK_ENTRY_PAGE file_entry;
		char meta_name[200];
		FILE *fptr;

		file_stat.st_size = 100000; // block_num = 100000/10000 = 10 = 1 page in meta
		file_entry.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int i = 0; i < file_entry.num_entries ; i++)
			file_entry.block_entries[i].status = ST_BOTH;

		sprintf(meta_name, "/tmp/run_cache_loop_filemeta%d", inode);
		fptr = fopen(meta_name, "w+");
		fseek(fptr, 0, SEEK_SET);
		fwrite(&file_stat, sizeof(struct stat), 1, fptr);
		fwrite(&file_meta, sizeof(FILE_META_TYPE), 1, fptr);
		//for (int blockno = 0; blockno < file_stat.st_size/MAX_BLOCK_SIZE ; blockno++)
		fwrite(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		fclose(fptr);

		for (int blockno = 0; blockno < file_stat.st_size/MAX_BLOCK_SIZE ; blockno++) {	
			sprintf(meta_name, "/tmp/run_cache_loop_block%d_%d", inode, blockno);
			mknod(meta_name, 0700, 0);
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
	BLOCK_ENTRY_PAGE file_entry;
	char meta_name[200];
	FILE *fptr;

	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT; 
	hcfs_system->system_going_down = FALSE;
	MAX_BLOCK_SIZE = 10000;
	init_mock_cache_usage();

	pthread_create(&thread_id, NULL, cache_loop_function, NULL);
	sleep(15);
	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT - 1; 
	hcfs_system->system_going_down = TRUE;
	sleep(2);


	for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
		sprintf(meta_name, "/tmp/run_cache_loop_filemeta%d", inode_cache_usage_hash[i]->this_inode);
		fptr = fopen(meta_name, "r");
		fseek(fptr, sizeof(struct stat) + sizeof(FILE_META_TYPE), SEEK_SET);
		fread(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		for (int entry = 0; entry < 10; entry++)
			ASSERT_EQ(ST_CLOUD, file_entry.block_entries[entry].status);
	}

}
