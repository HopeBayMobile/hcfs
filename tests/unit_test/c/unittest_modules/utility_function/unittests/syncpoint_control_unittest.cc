#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <inttypes.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
extern "C" {
#include "utils.h"
#include "global.h"
#include "hfuse_system.h"
#include "params.h"
#include "fuseop.h"
#include "mount_manager.h"
#include "super_block.h"
}
#include "gtest/gtest.h"

extern SYSTEM_CONF_STRUCT *system_config;

class syncpoint_controlEnvironment : public ::testing::Environment {
public:

	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			calloc(sizeof(SYSTEM_CONF_STRUCT), 1);
		hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->sync_control_sem), 1, 0);
		METAPATH = "syncpoint_control_folder";
	}
	void TearDown()
	{
		free(system_config);
		free(hcfs_system);
	}
};

::testing::Environment* const upload_env =
	::testing::AddGlobalTestEnvironment(new syncpoint_controlEnvironment);

/**
 * Unittest of init_syncpoint_resource()
 */
class init_syncpoint_resourceTest : public ::testing::Test {
protected:
	void SetUp()
	{
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				calloc(sizeof(SUPER_BLOCK_CONTROL), 1);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
		mkdir(METAPATH, 0700);
	}

	void TearDown()
	{
		if (sys_super_block)
			free(sys_super_block);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
	}
};

TEST_F(init_syncpoint_resourceTest, SuperBlockIs_NOT_Init)
{
	free(sys_super_block);
	sys_super_block = NULL;
	
	EXPECT_EQ(-EINVAL, init_syncpoint_resource());
}

TEST_F(init_syncpoint_resourceTest, ReAllocateResource_Success)
{
	SYNC_POINT_DATA data, exp_data;

	sys_super_block->sync_point_info = (SYNC_POINT_INFO *)
			calloc(sizeof(SYNC_POINT_INFO), 1);

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	/* Run */
	EXPECT_EQ(0, init_syncpoint_resource());

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES,
		sys_super_block->sync_point_info->sync_retry_times);
	ASSERT_TRUE(sys_super_block->sync_point_info->fptr != NULL);
	EXPECT_EQ(0, memcmp(&exp_data,
		&(sys_super_block->sync_point_info->data), sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&data, sizeof(SYNC_POINT_DATA), 0));

	EXPECT_EQ(0, memcmp(&exp_data, &data, sizeof(SYNC_POINT_DATA)));
}

TEST_F(init_syncpoint_resourceTest, AllocateNewResource_Success)
{
	SYNC_POINT_DATA data, exp_data;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	/* Run */
	EXPECT_EQ(0, init_syncpoint_resource());

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES,
		sys_super_block->sync_point_info->sync_retry_times);
	ASSERT_TRUE(sys_super_block->sync_point_info->fptr != NULL);
	EXPECT_EQ(0, memcmp(&exp_data,
		&(sys_super_block->sync_point_info->data), sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&data, sizeof(SYNC_POINT_DATA), 0));

	EXPECT_EQ(0, memcmp(&exp_data, &data, sizeof(SYNC_POINT_DATA)));
}

TEST_F(init_syncpoint_resourceTest, LoadOldData)
{
	SYNC_POINT_DATA data, exp_data;
	char path[300];
	FILE *fptr;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 123;
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_point = 0;
	exp_data.delete_sync_complete = TRUE;
	fetch_syncpoint_data_path(path);
	fptr = fopen(path, "w+");
	pwrite(fileno(fptr), &exp_data, sizeof(SYNC_POINT_DATA), 0);
	fclose(fptr);

	/* Run */
	EXPECT_EQ(0, init_syncpoint_resource());

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES,
		sys_super_block->sync_point_info->sync_retry_times);
	ASSERT_TRUE(sys_super_block->sync_point_info->fptr != NULL);
	EXPECT_EQ(0, memcmp(&exp_data,
		&(sys_super_block->sync_point_info->data), sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&data, sizeof(SYNC_POINT_DATA), 0));

	EXPECT_EQ(0, memcmp(&exp_data, &data, sizeof(SYNC_POINT_DATA)));
}
/**
 * End of unittest of init_syncpoint_resource()
 */

/**
 * Unittest of free_syncpoint_resource()
 */
class free_syncpoint_resourceTest : public ::testing::Test {
protected:
	void SetUp()
	{
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				calloc(sizeof(SUPER_BLOCK_CONTROL), 1);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
		mkdir(METAPATH, 0700);
	}

	void TearDown()
	{
		if (sys_super_block)
			free(sys_super_block);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
	}
};

TEST_F(free_syncpoint_resourceTest, FreeDataSucceess)
{
	char path[300];

	fetch_syncpoint_data_path(path);
	EXPECT_EQ(0, init_syncpoint_resource());
	EXPECT_EQ(0, access(path, F_OK));
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);

	/* Run */
	free_syncpoint_resource(TRUE);

	/* Verify */
	EXPECT_EQ(-1, access(path, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
	EXPECT_TRUE(NULL == sys_super_block->sync_point_info);
}

TEST_F(free_syncpoint_resourceTest, FreeDataSucceess_DoNOTRemoveFile)
{
	char path[300];

	fetch_syncpoint_data_path(path);
	EXPECT_EQ(0, init_syncpoint_resource());
	EXPECT_EQ(0, access(path, F_OK));
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);

	/* Run */
	free_syncpoint_resource(FALSE);

	/* Verify */
	EXPECT_EQ(0, access(path, F_OK));
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
	EXPECT_TRUE(NULL == sys_super_block->sync_point_info);
}
/**
 * End of Unittest of free_syncpoint_resource()
 */

/**
 * Unittest of check_sync_complete()
 */
class check_sync_completeTest : public ::testing::Test {
protected:
	void SetUp()
	{
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				calloc(sizeof(SUPER_BLOCK_CONTROL), 1);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
		mkdir(METAPATH, 0700);

		init_syncpoint_resource();
	}

	void TearDown()
	{
		free_syncpoint_resource(TRUE);
		if (sys_super_block)
			free(sys_super_block);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
	}
};

TEST_F(check_sync_completeTest, RetrySuccess_StillNeedToSync)
{
	SYNC_POINT_DATA *data;
	SYNC_POINT_DATA exp_data, verified_data;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 12;
	sys_super_block->head.last_dirty_inode = 345;
	sys_super_block->head.first_to_delete_inode = 78;
	sys_super_block->head.last_to_delete_inode = 910;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 345;
	exp_data.delete_sync_point = 910;
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = FALSE;

	/* Run */
	check_sync_complete(IS_DIRTY, 12);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES - 1,
		sys_super_block->sync_point_info->sync_retry_times);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&verified_data, sizeof(SYNC_POINT_DATA), 0));
	EXPECT_EQ(0, memcmp(&exp_data, &verified_data,
		sizeof(SYNC_POINT_DATA)));
}

TEST_F(check_sync_completeTest, RetrySuccess_StillNeedToSyncDeleteQueue)
{
	SYNC_POINT_DATA *data;
	SYNC_POINT_DATA exp_data, verified_data;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 12;
	sys_super_block->head.last_dirty_inode = 12;
	sys_super_block->head.first_to_delete_inode = 78;
	sys_super_block->head.last_to_delete_inode = 78;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 0;
	exp_data.delete_sync_point = 78;
	exp_data.upload_sync_complete = TRUE;
	exp_data.delete_sync_complete = FALSE;

	/* Run */
	check_sync_complete(IS_DIRTY, 12);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES - 1,
		sys_super_block->sync_point_info->sync_retry_times);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&verified_data, sizeof(SYNC_POINT_DATA), 0));
	EXPECT_EQ(0, memcmp(&exp_data, &verified_data,
		sizeof(SYNC_POINT_DATA)));
}

TEST_F(check_sync_completeTest, RetrySuccess_StillNeedToSyncDirtyQueue)
{
	SYNC_POINT_DATA *data;
	SYNC_POINT_DATA exp_data, verified_data;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 12;
	sys_super_block->head.last_dirty_inode = 12;
	sys_super_block->head.first_to_delete_inode = 78;
	sys_super_block->head.last_to_delete_inode = 78;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 12;
	exp_data.delete_sync_point = 0;
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = TRUE;

	/* Run */
	check_sync_complete(TO_BE_DELETED, 78);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(SYNC_RETRY_TIMES - 1,
		sys_super_block->sync_point_info->sync_retry_times);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&verified_data, sizeof(SYNC_POINT_DATA), 0));
	EXPECT_EQ(0, memcmp(&exp_data, &verified_data,
		sizeof(SYNC_POINT_DATA)));
}

TEST_F(check_sync_completeTest, DoNotNeedRetry_SyncComplete_DequeueDirtyInode)
{
	char path[300];
	SYNC_POINT_DATA *data;

	fetch_syncpoint_data_path(path);
	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 12;
	sys_super_block->head.last_dirty_inode = 12;
	sys_super_block->head.first_to_delete_inode = 0;
	sys_super_block->head.last_to_delete_inode = 0;

	/* Run */
	check_sync_complete(IS_DIRTY, 12);

	/* Verify */
	data = &(sys_super_block->sync_point_info->data);
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
	EXPECT_TRUE(NULL == sys_super_block->sync_point_info);
	EXPECT_EQ(-1, access(path, F_OK));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(check_sync_completeTest, DoNotNeedRetry_SyncComplete_DequeueDeleteInode)
{
	char path[300];
	SYNC_POINT_DATA *data;

	fetch_syncpoint_data_path(path);
	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 0;
	sys_super_block->head.last_dirty_inode = 0;
	sys_super_block->head.first_to_delete_inode = 78;
	sys_super_block->head.last_to_delete_inode = 78;

	/* Run */
	check_sync_complete(TO_BE_DELETED, 78);

	/* Verify */
	data = &(sys_super_block->sync_point_info->data);
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
	EXPECT_TRUE(NULL == sys_super_block->sync_point_info);
	EXPECT_EQ(-1, access(path, F_OK));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(check_sync_completeTest, NoChanceToSyncAgain_SyncComplete)
{
	char path[300];
	SYNC_POINT_DATA *data;

	fetch_syncpoint_data_path(path);
	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = TRUE;
	data->delete_sync_complete = TRUE;
	sys_super_block->head.first_dirty_inode = 12;
	sys_super_block->head.last_dirty_inode = 345;
	sys_super_block->head.first_to_delete_inode = 78;
	sys_super_block->head.last_to_delete_inode = 910;
	sys_super_block->sync_point_info->sync_retry_times = 0;

	/* Run */
	check_sync_complete(IS_DIRTY, 12);

	/* Verify */
	data = &(sys_super_block->sync_point_info->data);
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
	EXPECT_TRUE(NULL == sys_super_block->sync_point_info);
	EXPECT_EQ(-1, access(path, F_OK));
	EXPECT_EQ(ENOENT, errno);
}
/**
 * End of unittest of check_sync_complete()
 */

/**
 * Unittest of move_sync_point()
 */
class move_sync_pointTest : public ::testing::Test {
protected:
	void SetUp()
	{
		sys_super_block = (SUPER_BLOCK_CONTROL *)
				calloc(sizeof(SUPER_BLOCK_CONTROL), 1);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
		mkdir(METAPATH, 0700);

		init_syncpoint_resource();
	}

	void TearDown()
	{
		free_syncpoint_resource(TRUE);
		if (sys_super_block)
			free(sys_super_block);
		if (!access(METAPATH, F_OK))
			system("rm -rf syncpoint_control_folder");
	}
};

TEST_F(move_sync_pointTest, MoveDirtySyncPoint_Success)
{
	SYNC_POINT_DATA *data, exp_data, verified_data;
	SUPER_BLOCK_ENTRY sbentry;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = FALSE;
	data->delete_sync_complete = FALSE;
	data->upload_sync_point = 5566;
	data->delete_sync_point = 123;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_complete = TRUE;
	exp_data.delete_sync_complete = FALSE;
	exp_data.upload_sync_point = 0;
	exp_data.delete_sync_point = 123;
	memset(&sbentry, 0, sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	move_sync_point(IS_DIRTY, 5566, &sbentry);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&verified_data, sizeof(SYNC_POINT_DATA), 0));
	EXPECT_EQ(0, memcmp(&exp_data, &verified_data,
		sizeof(SYNC_POINT_DATA)));
}

TEST_F(move_sync_pointTest, MoveDeleteSyncPoint_Success)
{
	SYNC_POINT_DATA *data, exp_data, verified_data;
	SUPER_BLOCK_ENTRY sbentry;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = FALSE;
	data->delete_sync_complete = FALSE;
	data->upload_sync_point = 5566;
	data->delete_sync_point = 123;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = TRUE;
	exp_data.upload_sync_point = 5566;
	exp_data.delete_sync_point = 0;
	memset(&sbentry, 0, sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	move_sync_point(TO_BE_DELETED, 123, &sbentry);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(sizeof(SYNC_POINT_DATA),
		pread(fileno(sys_super_block->sync_point_info->fptr),
		&verified_data, sizeof(SYNC_POINT_DATA), 0));
	EXPECT_EQ(0, memcmp(&exp_data, &verified_data,
		sizeof(SYNC_POINT_DATA)));
}

TEST_F(move_sync_pointTest, StatusNotMatch_ChangeNothing)
{
	SYNC_POINT_DATA *data, exp_data, verified_data;
	SUPER_BLOCK_ENTRY sbentry;

	data = &(sys_super_block->sync_point_info->data);
	data->upload_sync_complete = FALSE;
	data->delete_sync_complete = FALSE;
	data->upload_sync_point = 5566;
	data->delete_sync_point = 123;

	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = FALSE;
	exp_data.upload_sync_point = 5566;
	exp_data.delete_sync_point = 123;
	memset(&sbentry, 0, sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	move_sync_point(NO_LL, 456, &sbentry);

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(0, memcmp(&exp_data, data, sizeof(SYNC_POINT_DATA)));
}

/**
 * End of unittest of move_sync_point()
 */
