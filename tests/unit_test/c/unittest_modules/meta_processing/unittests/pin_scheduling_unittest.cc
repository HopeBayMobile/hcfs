extern "C" {
#include "pin_scheduling.h"
#include "fuseop.h"
#include "global.h"
#include "super_block.h"
}

#include <pthread.h>
#include "gtest/gtest.h"

extern SYSTEM_DATA_HEAD *hcfs_system;
extern SUPER_BLOCK_CONTROL *sys_super_block;

class pinning_loopTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sys_super_block = (SUPER_BLOCK_CONTROL *)
			malloc(sizeof(SUPER_BLOCK_CONTROL));
		memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
		init_pin_scheduler();
	}

	void TearDown()
	{
		free(hcfs_system);
		free(sys_super_block);
		destroy_pin_scheduler();
	}
};

TEST_F(pinning_loopTest, WorkNormally)
{
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_paused = FALSE;
	sys_super_block->head.num_pinning_inodes = 1;
	sys_super_block->head.first_pin_inode = 3;

	hcfs_system->system_going_down = TRUE;
}
