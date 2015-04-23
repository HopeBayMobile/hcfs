#include "gtest/gtest.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "global.h"
}

void *dsync_test_thread_fn(void *data)
{
	//sleep(0.1* *(int *)data);
	return NULL;
}

extern DSYNC_THREAD_CONTROL dsync_ctl;

TEST(init_dsync_controlTest, ControlThreadSuccess)
{
	/* Run the function to check whether it will terminate threads */
	init_dsync_control();
	/* Generate threads */
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
		int inode = i;
		dsync_ctl.threads_in_use[i] = inode;
		dsync_ctl.threads_created[i] = TRUE;
		dsync_ctl.total_active_dsync_threads++;
		EXPECT_EQ(0, pthread_create(&(dsync_ctl.inode_dsync_thread[i]), NULL, 
			dsync_test_thread_fn, (void *)&inode));
	}
	sleep(10);
	//pthread_kill(dsync_ctl.dsync_handler_thread, SIGKILL);
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	for (int i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++) {
		ASSERT_EQ(0, dsync_ctl.threads_in_use[i]) << "i = " << i;
		EXPECT_EQ(FALSE, dsync_ctl.threads_created[i]) << "i = " << i;
	}
}
