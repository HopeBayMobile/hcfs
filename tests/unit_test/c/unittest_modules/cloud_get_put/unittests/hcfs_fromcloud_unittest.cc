#include <semaphore.h>
#include <string>
#include <errno.h>
#include "gtest/gtest.h"
#include "curl/curl.h"
#include "attr/xattr.h"
extern "C"{
#include "hcfscurl.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "fuseop.h"
#include "mock_params.h"
}

extern SYSTEM_DATA_HEAD *hcfs_system;

class fromcloudEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;

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
    free(hcfs_system);
    rmdir(tmppath);
    unlink("/tmp/testHCFS");
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);

  }
};

::testing::Environment* const fromcloud_env = ::testing::AddGlobalTestEnvironment(new fromcloudEnvironment);

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

		num_obj = 35; // fetch 35 blocks
		objname_counter = 0; // Counter of recording actual value
		expected_obj_counter = 0; // Counter of recording expected value
		sem_init(&objname_counter_sem, 0, 1);
		objname_list = (char **)malloc(sizeof(char *)*num_obj);
		for (int i = 0 ; i < num_obj ; i++)
			objname_list[i] = (char *)malloc(sizeof(char)*40);
	}
	virtual void TearDown()
	{
		sem_destroy(&download_curl_sem);
		sem_destroy(&download_curl_control_sem);
		sem_destroy(&objname_counter_sem);
		for (int i = 0 ; i < num_obj ; i++)
			free(objname_list[i]);
		free(objname_list);
	}
	/* Static thread function, which is used to run function fetch_from_cloud() */
	static void *fetch_from_cloud_for_thread(void *block_no)
	{
		char tmp_filename[50];
		FILE *fptr;
		int ret;

		sprintf(tmp_filename, "/tmp/testHCFS/local_space%d", *(int *)block_no);
		fptr = fopen(tmp_filename, "w+");
		ret = fetch_from_cloud(fptr, 1, *(int *)block_no);
		fclose(fptr);
		unlink(tmp_filename);	
		return NULL;
	}
	std::string expected_objname[100]; // Expected answer list
	int expected_obj_counter;
	int num_obj;
};

int objname_cmp(const void *s1, const void *s2)
{
	char *name1 = *(char **)s1;
	char *name2 = *(char **)s2;
	int inode1, block1;
	int inode2, block2;
	sscanf(name1, "data_%d_%d", &inode1, &block1);
	sscanf(name2, "data_%d_%d", &inode2, &block2);
	return block1 - block2;
}

TEST_F(fetch_from_cloudTest, FetchSuccess)
{
	pthread_t tid[num_obj];
	int block_no[num_obj];

	/* Run fetch_from_cloud() with multi-threads */
	for (int i = 0 ; i < num_obj ; i++) {
		char tmp_filename[20];
		block_no[i] = (i + 1)*5;
		EXPECT_EQ(0, pthread_create(&tid[i], NULL, 
			fetch_from_cloudTest::fetch_from_cloud_for_thread, (void *)&block_no[i]));
		
		sprintf(tmp_filename, "data_%d_%d", 1, block_no[i]); // Expected value
		expected_objname[expected_obj_counter++] = std::string(tmp_filename); 
	}
	sleep(1);

	/* Check answer */
	EXPECT_EQ(num_obj, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *), objname_cmp); // Sort actual value
	for (int i = 0 ; i < num_obj ; i++) {
		pthread_join(tid[i], NULL);
		ASSERT_STREQ(expected_objname[i].c_str(), objname_list[i]);
	}
}

/*
	End of unittest of fetch_from_cloud()
*/

/*
	Unittest of prefetch_block()
*/

class prefetch_blockTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		prefetch_ptr = (PREFETCH_STRUCT_TYPE *)malloc(sizeof(PREFETCH_STRUCT_TYPE));
		prefetch_ptr->this_inode = 1;
		prefetch_ptr->block_no = 1;
		prefetch_ptr->page_start_fpos = 0;
		prefetch_ptr->entry_index = 0;
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
	//mock_page.next_page = 0;
	metafptr = fopen("/tmp/testHCFS/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NUM__FETCH_SUCCESS;

	/* Run */
	prefetch_block(prefetch_ptr);
	
	/* Check answer */
	EXPECT_EQ(0, hcfs_system->systemdata.cache_size);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_blocks);
	EXPECT_EQ(0, access("/tmp/testHCFS/tmp_block", F_OK));	
	unlink("/tmp/testHCFS/tmp_meta");
	unlink("/tmp/testHCFS/tmp_block");
}

TEST_F(prefetch_blockTest, PrefetchSuccess)
{
	FILE *metafptr;
	BLOCK_ENTRY_PAGE mock_page;
	BLOCK_ENTRY_PAGE result_page;
	int entry_index;
	int meta_fpos;
	char xattr_result;
	int ret, errcode;
	
	entry_index = prefetch_ptr->entry_index;
	meta_fpos = prefetch_ptr->page_start_fpos;
	mock_page.num_entries = 1;
	mock_page.block_entries[0].status = ST_CLOUD;
	//mock_page.next_page = 0;

	metafptr = fopen("/tmp/testHCFS/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NUM__FETCH_SUCCESS; // Control success or fail to fetch from cloud.

	/* Run */
	prefetch_block(prefetch_ptr);

	/* Check answer */
	EXPECT_EQ(EXTEND_FILE_SIZE, hcfs_system->systemdata.cache_size); // Total size = expected block size
	EXPECT_EQ(1, hcfs_system->systemdata.cache_blocks); // Prefetch one block from cloud
	EXPECT_EQ(0, access("/tmp/testHCFS/tmp_block", F_OK)); // Mock block path
	ret = getxattr("/tmp/testHCFS/tmp_block", "user.dirty", &xattr_result,
			sizeof(char));
	if (ret < 0) {
		errcode = errno;
		printf("Failed to getxattr. Code %d, %s\n", errcode,
			strerror(errcode));
	}
	EXPECT_EQ(1, ret);
	EXPECT_EQ('F', xattr_result); // xattr
	metafptr = fopen("/tmp/testHCFS/tmp_meta", "r");	
	fseek(metafptr, meta_fpos, SEEK_SET);
	fread(&result_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	EXPECT_EQ(ST_BOTH, result_page.block_entries[entry_index].status); // status
		
	unlink("/tmp/testHCFS/tmp_meta");
	unlink("/tmp/testHCFS/tmp_block");
}

TEST_F(prefetch_blockTest, PrefetchFail)
{
	/* Does prefetch_block fail? It seems that fetch_from_cloud() never return with failure  */	
}

/*
	End of unittest of prefetch_block()
*/

