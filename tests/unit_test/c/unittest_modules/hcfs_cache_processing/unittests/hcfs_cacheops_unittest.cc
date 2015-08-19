#include "gtest/gtest.h"
extern "C" {
#include "hcfs_cacheops.h"
#include "hcfs_cachebuild.h"
#include "mock_params.h"
#include "fuseop.h"
#include "global.h"
}

class cacheopEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {

    workpath = NULL;
    tmppath = NULL;
    if (access("/tmp/testHCFS", F_OK) != 0) {
      workpath = get_current_dir_name();
      tmppath = (char *)malloc(strlen(workpath)+20);
      snprintf(tmppath, strlen(workpath)+20, "%s/tmpdir", workpath);
      if (access(tmppath, F_OK) != 0)
        mkdir(tmppath, 0700);
      symlink(tmppath, "/tmp/testHCFS");
     }

  }

  virtual void TearDown() {
    unlink("/tmp/testHCFS");
    rmdir(tmppath);
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);
  }
};

::testing::Environment* const cacheop_env = ::testing::AddGlobalTestEnvironment(new cacheopEnvironment);

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
		char meta_name[200];
		char block_name[200];
		
		for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
			ino_t inode = inode_cache_usage_hash[i]->this_inode;

#ifdef ARM_32bit_
			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_filemeta%lld",
				inode);
#else
			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_filemeta%ld",
				inode);
#endif
			unlink(meta_name);
			for (int blockno = 0; blockno < 10 ; blockno++) {	
#ifdef ARM_32bit_
				sprintf(block_name,
					"/tmp/testHCFS/run_cache_loop_block%lld_%d", 
					inode, blockno);
#else
				sprintf(block_name,
					"/tmp/testHCFS/run_cache_loop_block%ld_%d", 
					inode, blockno);
#endif
				unlink(block_name);
			}
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
	
	/* Generate mock meta and blocks */
	void genetate_mock_meta(ino_t inode)
	{
		struct stat file_stat;
		FILE_META_TYPE file_meta;
		BLOCK_ENTRY_PAGE file_entry;
		char meta_name[200];
		FILE *fptr;

		file_stat.st_size = 1000; // block_num = 1000/100 = 10 = 1 page in meta
		file_entry.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int i = 0; i < file_entry.num_entries ; i++)
			file_entry.block_entries[i].status = ST_BOTH;

#ifdef ARM_32bit_
		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%lld",
				inode);
#else
		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%ld",
				inode);
#endif
		fptr = fopen(meta_name, "w+");
		fseek(fptr, 0, SEEK_SET);
		fwrite(&file_stat, sizeof(struct stat), 1, fptr);
		fwrite(&file_meta, sizeof(FILE_META_TYPE), 1, fptr);
		fwrite(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr); // Just write one page
		fclose(fptr);

		for (int blockno = 0; blockno < file_stat.st_size/MAX_BLOCK_SIZE ; blockno++) {	
#ifdef ARM_32bit_
			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_block%lld_%d",
				inode, blockno);
#else
			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_block%ld_%d",
				inode, blockno);
#endif
			mknod(meta_name, 0700, 0);
			truncate(meta_name, MAX_BLOCK_SIZE);
		}
	}
};

void *cache_loop_function(void *ptr)
{
	run_cache_loop();
	printf("Test: run_cache_loop() thread leave\n");
	return NULL;
}

TEST_F(run_cache_loopTest, DeleteLocalBlockSuccess)
{
	pthread_t thread_id;
	BLOCK_ENTRY_PAGE file_entry;
	char meta_name[200];
	FILE *fptr;

	/* Generate mock cache usage nodes and mock block file */
	MAX_BLOCK_SIZE = 100;
	CACHE_SOFT_LIMIT = 1;
	hcfs_system->systemdata.cache_size = CURRENT_CACHE_SIZE; 
	hcfs_system->system_going_down = FALSE;
	hcfs_system->systemdata.cache_blocks = CURRENT_BLOCK_NUM;
	printf("Test: Generate mock data (cache usage & block file).\n");
	init_mock_cache_usage();
	
	/* Run */
	pthread_create(&thread_id, NULL, cache_loop_function, NULL);
	/* TODO: How to fix this unittest so that we don't need to count on
		the process to finish within 30 or 120 seconds. */
#ifdef ARM_32bit_
	/* Change wait time to 120 secs for slower cpu */
	printf("Test: cache_loop() is running. process sleep 120 seconds.\n");
	sleep(120);
#else
	printf("Test: cache_loop() is running. process sleep 30 seconds.\n");
	sleep(30);
#endif
	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT - 1; 
	hcfs_system->system_going_down = TRUE;
	printf("Test: Let thread leave.\n");
	sleep(2);

	/* Verify */
	int expected_block_num = CURRENT_BLOCK_NUM;
	for (int i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
		struct stat file_stat;
		ino_t inode;
		
		inode = inode_cache_usage_hash[i]->this_inode;
#ifdef ARM_32bit_
		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%lld",
				inode);
#else
		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%ld",
				inode);
#endif
		fptr = fopen(meta_name, "r");
		
		fseek(fptr, 0, SEEK_SET);
		fread(&file_stat, sizeof(struct stat), 1, fptr);
		
		fseek(fptr, sizeof(struct stat) + sizeof(FILE_META_TYPE), SEEK_SET);
		fread(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		//** Verify status is changed to ST_CLOUD and block is removed. **
		for (int entry = 0; entry < file_stat.st_size/MAX_BLOCK_SIZE; entry++) {
			char block_name[200];
			
			ASSERT_EQ(ST_CLOUD, file_entry.block_entries[entry].status);
#ifdef ARM_32bit_
			sprintf(block_name,
				"/tmp/testHCFS/run_cache_loop_block%lld_%d",
				inode, entry);
#else
			sprintf(block_name,
				"/tmp/testHCFS/run_cache_loop_block%ld_%d",
				inode, entry);
#endif
			ASSERT_TRUE(access(block_name, F_OK) < 0);

			expected_block_num--;
		}
		fclose(fptr);
	}

	EXPECT_EQ(expected_block_num, hcfs_system->systemdata.cache_blocks);
}