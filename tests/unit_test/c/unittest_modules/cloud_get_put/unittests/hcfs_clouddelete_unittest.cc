#include "gtest/gtest.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "global.h"
}

void *dsync_test_thread_fn(void *data)
{
	sleep(0.01* *(int *)data);
	return NULL;
}

extern DSYNC_THREAD_CONTROL dsync_ctl;

TEST(init_dsync_controlTest, ControlThreadSuccess)
{
	void *res;
	/* Run the function to check whether it will terminate threads */
	init_dsync_control();
	/* Generate threads */
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
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
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
		ASSERT_EQ(0, dsync_ctl.threads_in_use[i]) << "thread_no = " << i;
		EXPECT_EQ(FALSE, dsync_ctl.threads_created[i]) << "thread_no = " << i;
	}
}
