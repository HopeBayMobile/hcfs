extern "C" {
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include "mock_param.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"
}
#include "gtest/gtest.h"

extern SYSTEM_DATA_HEAD *hcfs_system;

class superblockEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));

			hcfs_system = (SYSTEM_DATA_HEAD *)
					malloc(sizeof(SYSTEM_DATA_HEAD));
			memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
			//sem_init(&(hcfs_system->access_sem), 0, 1);
		}
		void TearDown()
		{
			free(hcfs_system);
			free(system_config);
		}
};

::testing::Environment* const rebuild_superblock_env =
	::testing::AddGlobalTestEnvironment(new superblockEnvironment);


