#include "gtest/gtest.h"
#include "params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "global.h"
#include "fuseop.h"
}

/*
	Unittest of init_dsync_control() & collect_finished_dsync_threads()
 */
void *dsync_test_thread_fn(void *data)
{
	sleep(0.01* *(int *)data);
	return NULL;
}

extern DSYNC_THREAD_CONTROL dsync_ctl;

TEST(init_dsync_controlTest, ControlDsyncThreadSuccess)
{
	void *res;
	/* Run the function to check whether it will terminate threads */
	init_dsync_control();
	/* Generate threads */
	for (int i = 0 ; i < MAX_DSYNC_CONCURRENCY ; i++) {
		int inode = i+1;
		dsync_ctl.threads_in_use[i] = inode;
		dsync_ctl.threads_created[i] = TRUE;
		dsync_ctl.total_active_dsync_threads++;
		EXPECT_EQ(0, pthread_create(&(dsync_ctl.inode_dsync_thread[i]), NULL, 
			dsync_test_thread_fn, (void *)&inode));
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(dsync_ctl.dsync_handler_thread));
	EXPECT_EQ(0, pthread_join(dsync_ctl.dsync_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	/* Check answer */
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
	//sleep(0.01* *(int *)data);
	return NULL;
}

extern DELETE_THREAD_CONTROL delete_ctl;

TEST(init_delete_controlTest, ControlDeleteThreadSuccess)
{
	void *res;
	/* Run the function to check whether it will terminate threads */
	init_delete_control();
	/* Generate threads */
	for (int i = 0 ; i < MAX_DELETE_CONCURRENCY ; i++) {
		delete_ctl.threads_in_use[i] = TRUE;
		delete_ctl.threads_created[i] = TRUE;
		delete_ctl.delete_threads[i].is_block = TRUE;
		delete_ctl.total_active_delete_threads++;
		EXPECT_EQ(0, pthread_create(&(delete_ctl.threads_no[i]), NULL, 
			delete_test_thread_fn, (void *)&i));
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads);
	//* Check answer */
	for (int i = 0 ; i < MAX_DELETE_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, delete_ctl.threads_in_use[i]) << "thread_no = " << i;
		ASSERT_EQ(FALSE, delete_ctl.threads_created[i]) << "thread_no = " << i;
	}
}

/*
	End of unittest init_delete_control() & collect_finished_delete_threads()
 */

class dsync_single_inodeTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		mock_thread_info = (DSYNC_THREAD_TYPE *)malloc(sizeof(DSYNC_THREAD_TYPE));
	}
	virtual void TearDown()
	{
		free(mock_thread_info);
	}
	void init_objname_buffer()
	{
		objname_counter = 0;
		delete_objname = (char **)malloc(sizeof(char *)*500);
		for(int i = 0 ; i < 500 ; i++)
			delete_objname[i] = (char *)malloc(sizeof(char)*50);
		ASSERT_EQ(0, sem_init(&objname_counter_sem, 0, 100));
	}
	DSYNC_THREAD_TYPE *mock_thread_info;
};

TEST_F(dsync_single_inodeTest, CannotAccessMeta)
{
	FILE *meta;
	struct stat meta_stat;
	BLOCK_ENTRY_PAGE tmp_blockentry_page;
	FILE_META_TYPE tmp_file_meta;
	int total_page = 3;
	
	mock_thread_info->inode = INODE__FETCH_TODELETE_PATH_SUCCESS;
	mock_thread_info->this_mode = S_IFREG;
	
	meta = fopen(TODELETE_PATH, "w+"); // Open mock meta
	fwrite(&meta_stat, sizeof(struct stat), 1, meta); // Write stat
	tmp_file_meta.next_block_page = sizeof(struct stat) + sizeof(FILE_META_TYPE); 
	fwrite(&tmp_file_meta, sizeof(FILE_META_TYPE), 1, meta); // Write file_meta_type
	for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
		tmp_blockentry_page.block_entries[i].status = ST_CLOUD;
	tmp_blockentry_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
	for (int page_num = 0 ; page_num < total_page ; page_num++) {
		if(page_num == total_page - 1)
			tmp_blockentry_page.next_page = 0;
		else
			tmp_blockentry_page.next_page = sizeof(struct stat) + sizeof(FILE_META_TYPE) + 
				(page_num + 1) * sizeof(BLOCK_ENTRY_PAGE);
		fwrite(&tmp_blockentry_page, sizeof(BLOCK_ENTRY_PAGE), 1, meta); // Write block page
	}
	fclose(meta);

	init_delete_control();
	dsync_single_inode(mock_thread_info);	

}
