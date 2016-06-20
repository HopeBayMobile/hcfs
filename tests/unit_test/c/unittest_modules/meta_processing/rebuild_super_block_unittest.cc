#define __STDC_FORMAT_MACROS
extern "C" {
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <inttypes.h>
#include "rebuild_super_block.h"
#include "mock_param.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"
}
#include <cstdlib>
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
			sem_init(&(hcfs_system->access_sem), 0, 1);
			if (!access("rebuild_sb_running_folder", F_OK))
				system("rm -r ./rebuild_sb_running_folder");
			mkdir("rebuild_sb_running_folder", 0700);
			METAPATH = "rebuild_sb_running_folder";
		}
		void TearDown()
		{
			system("rm -r ./rebuild_sb_running_folder");
			free(hcfs_system);
			free(system_config);
		}
};

::testing::Environment* const rebuild_superblock_env =
	::testing::AddGlobalTestEnvironment(new superblockEnvironment);

class init_rebuild_sbTest: public ::testing::Test {
protected:
	char *sb_path, *queuefile_path;

	void SetUp()
	{
		sb_path = (char *) malloc(400);
		queuefile_path = (char *)malloc(400);
		sprintf(sb_path, "%s/superblock", METAPATH);
		sprintf(queuefile_path, "%s/rebuild_sb_queue", METAPATH);
		sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	}
	void TearDown()
	{
		free(sb_path);
		free(queuefile_path);
		free(sys_super_block);
		system("rm -r ./rebuild_sb_running_folder/*");
	}
};

TEST_F(init_rebuild_sbTest, BeginRebuildSuperBlock)
{
	SUPER_BLOCK_HEAD sb_head, exp_sb_head;
	FILE *fptr;
	ino_t exp_roots[5] = {234, 345, 456, 567, 678};
	ino_t roots[5];

	EXPECT_EQ(0, init_rebuild_sb(START_REBUILD_SB));

	/* Verify superblock */
	fptr = fopen(sb_path, "r");
	fseek(fptr, 0, SEEK_SET);
	fread(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, fptr);
	memset(&exp_sb_head, 0, sizeof(SUPER_BLOCK_HEAD));
	exp_sb_head.num_total_inodes = 5;
	exp_sb_head.now_rebuild = TRUE;

	EXPECT_EQ(0, memcmp(&exp_sb_head, &sb_head, sizeof(SUPER_BLOCK_HEAD)));
	fclose(fptr);
	unlink(sb_path);

	/* Verify queue file */
	pread(rebuild_sb_jobs->queue_fh, roots, sizeof(ino_t) * 5, 0);
	close(rebuild_sb_jobs->queue_fh);

	EXPECT_EQ(0, memcmp(exp_roots, roots, sizeof(ino_t) * 5));
	EXPECT_EQ(5, rebuild_sb_jobs->remaining_jobs);
	unlink(queuefile_path);

	/* Verify FSstat */
	for (int i = 0; i < 5; i++) {
		char fsstat_path[100];
		sprintf(fsstat_path, "%s/FS_sync/FSstat%"PRIu64,
			METAPATH, (uint64_t)roots[i]);
		ASSERT_EQ(0, access(fsstat_path, F_OK));
		unlink(fsstat_path);
	}
}
