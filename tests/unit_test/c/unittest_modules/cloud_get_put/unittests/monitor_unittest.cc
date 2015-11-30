extern "C" {
#include "monitor.h"
#include "global.h"
#include "fuseop.h"
#include "time.h"
}

#include <gtest/gtest.h>

extern SYSTEM_DATA_HEAD *hcfs_system;
class monitorTest : public ::testing::Test {
	protected:
		void SetUp()
		{
			hcfs_system = malloc(sizeof(SYSTEM_DATA_HEAD));
			hcfs_system->system_going_down = FALSE;
		}

		void TearDown()
		{
			free(hcfs_system);
		}
};

TEST_F(monitorTest, Backend_Is_Online)
{
	// Prepare flag to let mock hcfs_test_backend return 200
	hcfs_system->backend_status_is_online = FALSE;
	// create thread to run monitor loop
	// sleep a while
	// let system shut down
	// join thread
	ASSERT_EQ(TRUE, hcfs_system->backend_status_is_online);
}

TEST_F(monitorTest, Backend_Is_Offline)
{
	// Prepare flag to let mock hcfs_test_backend return 300
	hcfs_system->backend_status_is_online = TRUE;
	// create thread to run monitor loop
	// sleep a while
	// let system shut down
	// join thread
	ASSERT_EQ(FALSE, hcfs_system->backend_status_is_online);
}
