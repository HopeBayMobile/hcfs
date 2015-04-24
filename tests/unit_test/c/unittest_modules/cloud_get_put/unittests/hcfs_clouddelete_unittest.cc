#include "gtest/gtest.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "global.h"
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
