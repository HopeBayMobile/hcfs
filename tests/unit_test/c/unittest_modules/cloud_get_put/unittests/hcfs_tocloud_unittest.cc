#include "gtest/gtest.h"
#include "mock_params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
}

TEST(init_upload_controlTest, DoNothing_JustRun)
{
	void *res;
	char zero_mem[MAX_UPLOAD_CONCURRENCY] = {0};

	/* Run tested function */
	init_upload_control();
	sleep(1);
	
	/* Check */
	EXPECT_EQ(0, upload_ctl.total_active_upload_threads);
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_in_use)));
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_created)));
	
	/* Free resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handle_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handle_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);


}

TEST(init_upload_controlTest, ControlUploadThreadSuccess)
{
	void *res;

	/* Run tested function */
	init_upload_control();
}
