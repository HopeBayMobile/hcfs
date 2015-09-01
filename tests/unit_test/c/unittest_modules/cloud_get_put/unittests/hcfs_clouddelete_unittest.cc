#include "gtest/gtest.h"
#include "mock_params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
}

class deleteEnvironment : public ::testing::Environment {
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

::testing::Environment* const delete_env = ::testing::AddGlobalTestEnvironment(new deleteEnvironment);


/*
	Unittest of init_dsync_control() & collect_finished_dsync_threads()
 */
void *dsync_test_thread_fn(void *data)
{
	usleep(10000 * *(int *)data);
	return NULL;
}

extern DSYNC_THREAD_CONTROL dsync_ctl;

TEST(init_dsync_controlTest, ControlDsyncThreadSuccess)
{
	void *res;
	int num_threads = 40;
	
	/* Run the function to check whether it will terminate threads */
	init_dsync_control();
	
	/* Generate threads */
	for (int i = 0 ; i < num_threads ; i++) {
		int inode = i+1;
		int t_index;
		
		sem_wait(&dsync_ctl.dsync_queue_sem);
		sem_wait(&dsync_ctl.dsync_op_sem);
		t_index = -1;
		for (int idx = 0 ; idx < MAX_DSYNC_CONCURRENCY ; idx++) {
			if ((dsync_ctl.threads_in_use[idx] == 0) && 
				(dsync_ctl.threads_created[idx] == FALSE)) {
				t_index = idx;
				break;
			}
		}
		dsync_ctl.threads_in_use[t_index] = inode;
		dsync_ctl.threads_created[t_index] = TRUE;
		dsync_ctl.total_active_dsync_threads++;
		EXPECT_EQ(0, pthread_create(&(dsync_ctl.inode_dsync_thread[t_index]), NULL, 
			dsync_test_thread_fn, (void *)&i));
		sem_post(&dsync_ctl.dsync_op_sem);
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(dsync_ctl.dsync_handler_thread));
	EXPECT_EQ(0, pthread_join(dsync_ctl.dsync_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	
	/* Check answer */
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	for (int i = 0 ; i < MAX_DSYNC_CONCURRENCY ; i++) {
		ASSERT_EQ(0, dsync_ctl.threads_in_use[i]) << "thread_no = " << i;
		ASSERT_EQ(FALSE, dsync_ctl.threads_created[i]) << "thread_no = " << i;
	}
}
/*
	End of unittest init_dsync_control() & collect_finished_dsync_threads()
 */

/*
	Unittest of init_delete_control() & collect_finished_delete_threads()
 */
void *delete_test_thread_fn(void *data)
{
	usleep(10000 * *(int *)data);
	return NULL;
}

extern DELETE_THREAD_CONTROL delete_ctl;

TEST(init_delete_controlTest, ControlDeleteThreadSuccess)
{
	void *res;
	int num_threads = 40;
		
	/* Run the function to check whether it will terminate threads */
	init_delete_control();
	
	/* Generate threads */
	for (int i = 0 ; i < num_threads ; i++) {
		int t_index;

		sem_wait(&delete_ctl.delete_queue_sem);
		sem_wait(&delete_ctl.delete_op_sem);
		t_index = -1;
		for (int idx = 0 ; idx < MAX_DELETE_CONCURRENCY ; idx++) {
			if((delete_ctl.threads_in_use[idx] == FALSE) &&
				(delete_ctl.threads_created[idx] == FALSE)) {
				t_index = idx;
				break;
			}
		}
		delete_ctl.threads_in_use[t_index] = TRUE;
		delete_ctl.threads_created[t_index] = TRUE;
		delete_ctl.delete_threads[t_index].is_block = TRUE;
		delete_ctl.total_active_delete_threads++;
		EXPECT_EQ(0, pthread_create(&(delete_ctl.threads_no[t_index]), NULL,
			delete_test_thread_fn, (void *)&i));
		sem_post(&delete_ctl.delete_op_sem);
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	
	/* Check answer */
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads);
	for (int i = 0 ; i < MAX_DELETE_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, delete_ctl.threads_in_use[i]) << "thread_no = " << i;
		ASSERT_EQ(FALSE, delete_ctl.threads_created[i]) << "thread_no = " << i;
	}
}

/*
	End of unittest init_delete_control() & collect_finished_delete_threads()
 */

/*
	Unittest of dsync_single_inode()
*/
class dsync_single_inodeTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		mock_thread_info = (DSYNC_THREAD_TYPE *)malloc(sizeof(DSYNC_THREAD_TYPE));
	}
	virtual void TearDown()
	{
		sem_destroy(&objname_counter_sem);
		sem_destroy(&(sync_ctl.sync_op_sem));
		destroy_objname_buffer(expected_num_objname);
		free(mock_thread_info);
	}
	void init_objname_buffer(unsigned num_objname)
	{
		size_objname = 50;
		objname_counter = 0;
		objname_list = (char **)malloc(sizeof(char *) * num_objname);
		for (int i = 0 ; i < num_objname ; i++)
			objname_list[i] = (char *)malloc(sizeof(char)*size_objname);
		ASSERT_EQ(0, sem_init(&objname_counter_sem, 0, 1));
	}
	void destroy_objname_buffer(unsigned num_objname)
	{	
		for (int i = 0 ; i < num_objname ; i++)
			if(objname_list[i])
				free(objname_list[i]);
		if (objname_list)
			free(objname_list);
	}
	void init_sync_ctl()
	{
		sem_init(&(sync_ctl.sync_op_sem), 0, 1);
		for (int i = 0 ; i < MAX_SYNC_CONCURRENCY ; i++)
			sync_ctl.threads_in_use[i] = 0;
	}
	static int objname_cmp(const void *s1, const void *s2)
	{
		char *name1 = *(char **)s1;
		char *name2 = *(char **)s2;
		if (name1[0] == 'm') {
			return 1;
		} else if (name2[0] == 'm') {
			return -1;
		} else {
			char tmp_name[30];
			int inode1, inode2; 
			int blocknum1, blocknum2;
			sscanf(name1, "data_%d_%d", &inode1, &blocknum1);
			sscanf(name2, "data_%d_%d", &inode2, &blocknum2);
			return  -blocknum2 + blocknum1;
		}

	}
	DSYNC_THREAD_TYPE *mock_thread_info;
	unsigned expected_num_objname;
	unsigned size_objname;
};

TEST_F(dsync_single_inodeTest, DeleteAllBlockSuccess)
{
	FILE *meta;
	struct stat meta_stat;
	BLOCK_ENTRY_PAGE tmp_blockentry_page;
	FILE_META_TYPE tmp_file_meta;
	int total_page = 3;
	expected_num_objname = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	void *res;
	
	/* Mock an inode info & a meta file */
	mock_thread_info->inode = INODE__FETCH_TODELETE_PATH_SUCCESS;
	mock_thread_info->this_mode = S_IFREG;
	meta_stat.st_size = 1000000; // Let total_blocks = 1000000/100 = 10000
	MAX_BLOCK_SIZE = 100;
	
	meta = fopen(TODELETE_PATH, "w+"); // Open mock meta
	fwrite(&meta_stat, sizeof(struct stat), 1, meta); // Write stat
	fwrite(&tmp_file_meta, sizeof(FILE_META_TYPE), 1, meta); // Write file_meta_type
	for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++) {
		tmp_blockentry_page.block_entries[i].status = ST_CLOUD;
		tmp_blockentry_page.block_entries[i].uploaded = 1;
	}
	tmp_blockentry_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
	for (int page_num = 0 ; page_num < total_page ; page_num++) {
		fwrite(&tmp_blockentry_page, sizeof(BLOCK_ENTRY_PAGE), 1, meta); // Write block page
	}
	fclose(meta);

	/* Begin to test */
	init_delete_control();
	init_objname_buffer(expected_num_objname);
	init_sync_ctl();
	dsync_single_inode(mock_thread_info);

	/* Check answer */
	EXPECT_EQ(expected_num_objname, objname_counter); // Check # of object name.
	qsort(objname_list, expected_num_objname, sizeof(char *), dsync_single_inodeTest::objname_cmp);
	for (int block = 0 ; block < expected_num_objname - 1 ; block++) {
		char expected_objname[size_objname];
#ifdef ARM_32bit_
		sprintf(expected_objname, "data_%lld_%d", mock_thread_info->inode, block);
#else
		sprintf(expected_objname, "data_%d_%d", mock_thread_info->inode, block);
#endif
		ASSERT_STREQ(expected_objname, objname_list[block]); // Check all obj was recorded.
	}
	char expected_objname[size_objname];
#ifdef ARM_32bit_
	sprintf(expected_objname, "meta_%lld", mock_thread_info->inode);
#else
	sprintf(expected_objname, "meta_%d", mock_thread_info->inode);
#endif
	EXPECT_STREQ(expected_objname, objname_list[expected_num_objname - 1]); // Check meta was recorded.
	
	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads); // Check all threads finished.
}

TEST_F(dsync_single_inodeTest, DeleteDirectorySuccess)
{
	FILE *meta;
	struct stat meta_stat;
	void *res;
	expected_num_objname = 1;

	/* Mock a dir meta file */
	mock_thread_info->inode = INODE__FETCH_TODELETE_PATH_SUCCESS;
	mock_thread_info->this_mode = S_IFDIR;	
	meta = fopen(TODELETE_PATH, "w+"); // Open mock meta
	fwrite(&meta_stat, sizeof(struct stat), 1, meta); // Write stat
	fclose(meta);
	
	/* Begin to test */
	init_delete_control();
	init_objname_buffer(expected_num_objname);
	init_sync_ctl();
	dsync_single_inode(mock_thread_info);

	/* Check answer */
	EXPECT_EQ(1, objname_counter); // Check # of object name.
	char expected_objname[size_objname];
	sprintf(expected_objname, "meta_%d", mock_thread_info->inode); 
	EXPECT_STREQ(expected_objname, objname_list[0]); // Check meta was recorded.
	
	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads); // Check all threads finished.
}
/*
	End of unittest of dsync_single_inode()
*/

/*
	Unittest of delete_loop()
 */
int inode_cmp(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

TEST(delete_loopTest, DeleteSuccess)
{
	pthread_t thread;
	void *res;
	int size_objname;

	size_objname = 50;
	objname_counter = 0;
	objname_list = (char **)malloc(sizeof(char *) * 100);
	for (int i = 0 ; i < 100 ; i++)
		objname_list[i] = (char *)malloc(sizeof(char)*size_objname);

	test_data.num_inode = 40;
	test_data.to_handle_inode = (int *)malloc(sizeof(int) * test_data.num_inode);
	test_data.tohandle_counter = 0;

	to_verified_data.record_handle_inode = (int *)malloc(sizeof(int) * test_data.num_inode);
	to_verified_data.record_inode_counter = 0;
	sem_init(&(to_verified_data.record_inode_sem), 0, 1);

	for (int i = 0 ; i < test_data.num_inode ; i++)
		test_data.to_handle_inode[i] = (i + 1) * 5; // mock inode
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->head.first_to_delete_inode = test_data.to_handle_inode[0];

	/* Create a thread to run delete_loop() */
	ASSERT_EQ(0, pthread_create(&thread, NULL, delete_loop, NULL));
	sleep(10);

	/* Check answer */
	EXPECT_EQ(test_data.num_inode, to_verified_data.record_inode_counter);
	qsort(&(to_verified_data.record_handle_inode), to_verified_data.record_inode_counter, sizeof(int), inode_cmp);
	for (int i = 0 ; i < to_verified_data.record_inode_counter ; i++)
		ASSERT_EQ(test_data.to_handle_inode[i], to_verified_data.record_handle_inode[i]);
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads);

	/* Free resource */
	EXPECT_EQ(0, pthread_cancel(thread));
	EXPECT_EQ(0, pthread_join(thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	free(test_data.to_handle_inode);
	free(to_verified_data.record_handle_inode);
	free(sys_super_block);
}
/*
	End of unittest of delete_loop()
 */
