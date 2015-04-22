#include <semaphore.h>
#include "gtest/gtest.h"
#include "curl/curl.h"
extern "C"{
#include "hcfscurl.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "fuseop.h"
#include "params.h"
}

/*
	Unittest of fetch_from_cloud()
*/

class fetch_from_cloudTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++)
			curl_handle_mask[i] = FALSE;
	}
	virtual void TearDown()
	{
		sem_destroy(&download_curl_sem);
		sem_destroy(&download_curl_control_sem);
	}
	/* Static thread function, which is used to run function fetch_from_cloud() */
	static void *fetch_from_cloud_for_thread(void *data)
	{
		char tmp_filename[50];
		FILE *fptr;
		int ret;

		sprintf(tmp_filename, "/tmp/local_space%d", *(int *)data);
		fptr = fopen(tmp_filename, "w+");
		ret = fetch_from_cloud(fptr, 0, 0);
		*(int *)data = ret;
		fclose(fptr);
		unlink(tmp_filename);
		return NULL;
	}
};


TEST_F(fetch_from_cloudTest, FetchSuccess)
{
	pthread_t tid[MAX_DOWNLOAD_CURL_HANDLE];
	int ret_val[MAX_DOWNLOAD_CURL_HANDLE];

	/* Run fetch_from_cloud() with multi-threads */
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
		ret_val[i] = i;
		EXPECT_EQ(0, pthread_create(&tid[i], NULL, 
			fetch_from_cloudTest::fetch_from_cloud_for_thread, (void *)&ret_val[i]));
	}
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
		pthread_join(tid[i], NULL);
		EXPECT_EQ(200, ret_val[i]);
	}
}

/*
	End of unittest of fetch_from_cloud()
*/

/*
	Unittest of prefetch_block()
*/

extern SYSTEM_DATA_HEAD *hcfs_system;

class prefetch_blockTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		prefetch_ptr = (PREFETCH_STRUCT_TYPE *)malloc(sizeof(PREFETCH_STRUCT_TYPE));
		prefetch_ptr->this_inode = 1;
		prefetch_ptr->block_no = 1;
		prefetch_ptr->page_start_fpos = 0;
		prefetch_ptr->entry_index = 0;
		hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		/* Used to fetch_from_cloud */
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++)
			curl_handle_mask[i] = FALSE;
	}
	virtual void TearDown()
	{
		sem_destroy(&download_curl_sem);
		sem_destroy(&download_curl_control_sem);
	}
	PREFETCH_STRUCT_TYPE *prefetch_ptr;
};

TEST_F(prefetch_blockTest, BlockStatus_is_neither_STCLOUD_STCtoL)
{
	FILE *metafptr;
	BLOCK_ENTRY_PAGE mock_page;
	mock_page.num_entries = 1;
	mock_page.block_entries[0].status = ST_LDISK;
	mock_page.next_page = 0;
	metafptr = fopen("/tmp/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NO__FETCH_SUCCESS;
//	 Testing 
	prefetch_block(prefetch_ptr);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_size);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_blocks);
	EXPECT_EQ(0, access("/tmp/tmp_block", F_OK));	
	unlink("/tmp/tmp_meta");
	unlink("/tmp/tmp_block");
}

TEST_F(prefetch_blockTest, PrefetchSuccess)
{
	FILE *metafptr;
	BLOCK_ENTRY_PAGE mock_page;
	
	mock_page.num_entries = 1;
	mock_page.block_entries[0].status = ST_CLOUD;
	mock_page.next_page = 0;

	metafptr = fopen("/tmp/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NO__FETCH_SUCCESS; // Control success or fail to fetch from cloud.
	/* Testing */
	prefetch_block(prefetch_ptr);
	EXPECT_EQ(EXTEND_FILE_SIZE, hcfs_system->systemdata.cache_size);
	EXPECT_EQ(1, hcfs_system->systemdata.cache_blocks);
	EXPECT_EQ(0, access("/tmp/tmp_block", F_OK));	

	unlink("/tmp/tmp_meta");
	unlink("/tmp/tmp_block");
}
/*
	End of unittest of prefetch_block()
*/

