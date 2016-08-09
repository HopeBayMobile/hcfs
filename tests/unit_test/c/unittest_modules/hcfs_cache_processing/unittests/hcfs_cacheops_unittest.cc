#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <errno.h>
#include <ftw.h>
#include "gtest/gtest.h"
extern "C" {
#include "hcfs_cacheops.h"
#include "hcfs_cachebuild.h"
#include "mock_params.h"
#include "fuseop.h"
#include "global.h"
}

SYSTEM_CONF_STRUCT *system_config;

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir (fpath);
			break;
		default:
			unlink (fpath);
			break;
	}
	return (0);
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
    nftw(tmppath, do_delete, 20, FTW_DEPTH);
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
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));

	}
	
	void TearDown()
	{
		char meta_name[200];
		char block_name[200];
		
		for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
			ino_t inode = inode_cache_usage_hash[i]->this_inode;

			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_filemeta%" PRIu64 "",
				(uint64_t)inode);
			unlink(meta_name);
			for (int32_t blockno = 0; blockno < 10 ; blockno++) {	
				sprintf(block_name,
					"/tmp/testHCFS/run_cache_loop_block%" PRIu64 "_%d", 
					(uint64_t)inode, blockno);
				unlink(block_name);
			}
		}
		
		free(hcfs_system);
		free(system_config);
	}
	
	void init_mock_cache_usage()
	{
		cache_usage_init();
		nonempty_cache_hash_entries = 0;
		for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
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
		for (int32_t count = 0; count < CACHE_USAGE_NUM_ENTRIES; count++) {
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
		HCFS_STAT file_stat;
		FILE_META_TYPE file_meta;
		BLOCK_ENTRY_PAGE file_entry;
		char meta_name[200];
		FILE *fptr;

		file_stat.size = 1000; // block_num = 1000/100 = 10 = 1 page in meta
		file_entry.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int32_t i = 0; i < file_entry.num_entries ; i++) {
			file_entry.block_entries[i].status = ST_BOTH;
			file_entry.block_entries[i].paged_out_count =
				UINT32_MAX - 10 - i;
		}

		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%" PRIu64 "",
				(uint64_t)inode);
		fptr = fopen(meta_name, "w+");
		fseek(fptr, 0, SEEK_SET);
		fwrite(&file_stat, sizeof(HCFS_STAT), 1, fptr);
		fwrite(&file_meta, sizeof(FILE_META_TYPE), 1, fptr);
		fwrite(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr); // Just write one page
		fclose(fptr);

		int64_t blockno;
		for (blockno = 0; blockno < file_stat.size/MAX_BLOCK_SIZE ; blockno++) {	
			sprintf(meta_name,
				"/tmp/testHCFS/run_cache_loop_block%zu_%" PRId64,
				inode, blockno);
			mknod(meta_name, 0700, 0);
			truncate(meta_name, MAX_BLOCK_SIZE);
		}
	}
};

void *cache_loop_function(void *ptr)
{
#ifdef _ANDROID_ENV_
	run_cache_loop(NULL);
#else
	run_cache_loop();
#endif
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
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;
	hcfs_system->systemdata.cache_blocks = CURRENT_BLOCK_NUM;
	printf("Test: Generate mock data (cache usage & block file).\n");
	init_mock_cache_usage();
	
	/* Run */
	pthread_create(&thread_id, NULL, cache_loop_function, NULL);
#ifdef ARM_32bit_
	/* Change wait time to 120 secs for slower cpu */
	printf("Test: cache_loop() is running. process sleep 20 seconds.\n");
	sleep(20);
#else
	printf("Test: cache_loop() is running. process sleep 15 seconds.\n");
	sleep(15);
#endif
	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT - 1; 
	hcfs_system->system_going_down = TRUE;
	printf("Test: Let thread leave.\n");
	sleep(2);

	/* Verify */
	int32_t expected_block_num = CURRENT_BLOCK_NUM;
	for (int32_t i = 0 ; i < CACHE_USAGE_NUM_ENTRIES ; i += 5) {
		HCFS_STAT file_stat;
		ino_t inode;
		
		inode = inode_cache_usage_hash[i]->this_inode;
		sprintf(meta_name, "/tmp/testHCFS/run_cache_loop_filemeta%" PRIu64 "",
				(uint64_t)inode);
		fptr = fopen(meta_name, "r");
		
		fseek(fptr, 0, SEEK_SET);
		fread(&file_stat, sizeof(HCFS_STAT), 1, fptr);
		
		fseek(fptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE), SEEK_SET);
		fread(&file_entry, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
		//** Verify status is changed to ST_CLOUD and block is removed. **
		for (int32_t entry = 0; entry < file_stat.size/MAX_BLOCK_SIZE; entry++) {
			char block_name[200];
			
			ASSERT_EQ(ST_CLOUD, file_entry.block_entries[entry].status);
			if (entry == 0)
				EXPECT_EQ(0,
					file_entry.block_entries[entry].paged_out_count);
			else
				EXPECT_EQ(UINT32_MAX - 9 - entry,
					file_entry.block_entries[entry].paged_out_count);
			sprintf(block_name,
				"/tmp/testHCFS/run_cache_loop_block%" PRIu64 "_%d",
				(uint64_t)inode, entry);
			ASSERT_TRUE(access(block_name, F_OK) < 0);

			expected_block_num--;
		}
		fclose(fptr);
	}

	EXPECT_EQ(expected_block_num, hcfs_system->systemdata.cache_blocks);
}

class sleep_on_cache_fullTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));

		sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0);
		sem_init(&(hcfs_system->check_cache_sem), 1, 0);
		sem_init(&(hcfs_system->check_next_sem), 1, 0);
		sem_init(&(hcfs_system->check_cache_replace_status_sem), 1, 0);
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));

	}
	
	void TearDown()
	{
		
		free(hcfs_system);
		free(system_config);
	}
};

void *wrapper_sleep_on_cache_full(void *ptr)
{
	int32_t retval;
	retval = sleep_on_cache_full();
	*((int32_t *) ptr) = retval;
	return ptr;
}
TEST_F(sleep_on_cache_fullTest, SleepAndWakeOneThread)
{
	pthread_t thread1;
	int32_t semval, retval;
	int32_t *ptrret;

	hcfs_system->systemdata.cache_replace_status = 0;
	hcfs_system->sync_paused = FALSE;
	pthread_create(&thread1, NULL, wrapper_sleep_on_cache_full, (void *) &retval);
	sleep(2);
	sem_getvalue(&(hcfs_system->num_cache_sleep_sem), &semval);
	EXPECT_EQ(1, semval);
	sem_post(&(hcfs_system->check_cache_sem));
	ptrret = &retval;
	pthread_join(thread1, (void **) &ptrret);
	EXPECT_EQ(0, retval);
}
TEST_F(sleep_on_cache_fullTest, SleepAndWakeTwoThreads)
{
	pthread_t thread1, thread2;
	int32_t semval, retval, retval2;
	int32_t *ptrret, *ptrret2;

	hcfs_system->systemdata.cache_replace_status = 0;
	hcfs_system->sync_paused = FALSE;
	pthread_create(&thread1, NULL, wrapper_sleep_on_cache_full, (void *) &retval);
	pthread_create(&thread2, NULL, wrapper_sleep_on_cache_full, (void *) &retval2);
	sleep(2);
	sem_getvalue(&(hcfs_system->num_cache_sleep_sem), &semval);
	EXPECT_EQ(2, semval);
	sem_post(&(hcfs_system->check_cache_sem));
	sem_post(&(hcfs_system->check_cache_sem));
	ptrret = &retval;
	pthread_join(thread1, (void **) &ptrret);
	EXPECT_EQ(0, retval);
	ptrret2 = &retval2;
	pthread_join(thread2, (void **) &ptrret2);
	EXPECT_EQ(0, retval2);
}
TEST_F(sleep_on_cache_fullTest, SleepAndChangeStatus)
{
	pthread_t thread1;
	int32_t semval, retval;
	int32_t *ptrret;

	hcfs_system->systemdata.cache_replace_status = 0;
	hcfs_system->sync_paused = FALSE;
	pthread_create(&thread1, NULL, wrapper_sleep_on_cache_full, (void *) &retval);
	sleep(2);
	sem_getvalue(&(hcfs_system->num_cache_sleep_sem), &semval);
	EXPECT_EQ(1, semval);
	hcfs_system->systemdata.cache_replace_status = -EIO;
	sem_post(&(hcfs_system->check_cache_sem));
	ptrret = &retval;
	pthread_join(thread1, (void **) &ptrret);
	EXPECT_EQ(-EIO, retval);
}
TEST_F(sleep_on_cache_fullTest, NoSleepIfNegativeStatus)
{
	pthread_t thread1;
	int32_t semval, retval;
	int32_t *ptrret;

	hcfs_system->systemdata.cache_replace_status = -3;
	hcfs_system->sync_paused = FALSE;
	pthread_create(&thread1, NULL, wrapper_sleep_on_cache_full, (void *) &retval);
	sleep(2);
	sem_getvalue(&(hcfs_system->num_cache_sleep_sem), &semval);
	EXPECT_EQ(0, semval);
	ptrret = &retval;
	pthread_join(thread1, (void **) &ptrret);
	EXPECT_EQ(-3, retval);
}

