#include "gtest/gtest.h"
extern "C" {
#include "metaops.h"
#include "hfuse_system.h"
}

/* Rename HCFS main function */
#define main HCFSMain

extern "C" {
#include "hfuse_system.c"
}

/*
	Unittest for init_hcfs_system_data()
 */

class init_hcfs_system_dataTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			HCFSSYSTEM = (char *)malloc(sizeof(char) * 100);
			strcpy(HCFSSYSTEM, "/tmp/test_system_file");
		}
		virtual void TearDown()
		{
		}
};

TEST_F(init_hcfs_system_dataTest, Creat_and_Init_SystemDataSuccess)
{
	/* Run function */
	ASSERT_EQ(0, init_hcfs_system_data());
	/* Check answer */
	ASSERT_EQ(0, access(HCFSSYSTEM, F_OK));
	unlink(HCFSSYSTEM);
}

TEST_F(init_hcfs_system_dataTest, ReadSystemDataSuccess)
{
	FILE *fptr;
	SYSTEM_DATA_TYPE systemdata_answer;

	/* Mock data */
	systemdata_answer.system_size = 54321;
	systemdata_answer.cache_size = 98765;
	systemdata_answer.cache_blocks = 11223344;
	fptr = fopen(HCFSSYSTEM, "r+");
	if (fptr != NULL)
		unlink(HCFSSYSTEM);
	fptr = fopen(HCFSSYSTEM, "w+");
	fwrite(&systemdata_answer, sizeof(SYSTEM_DATA_TYPE), 1, fptr);
	fclose(fptr);
	/* Run function */
	ASSERT_EQ(0, init_hcfs_system_data());
	/* Check answer */
	ASSERT_EQ(0, access(HCFSSYSTEM, F_OK));
	EXPECT_EQ(0, memcmp(&systemdata_answer, &(hcfs_system->systemdata), 
						sizeof(SYSTEM_DATA_TYPE)));
	unlink(HCFSSYSTEM);
}
/*
	End of unittest for init_hcfs_system_data()
 */

/*
	Unittest for sync_hcfs_system_data()
 */
class sync_hcfs_system_dataTest : public init_hcfs_system_dataTest {
	protected:
		void init_mock_data()
		{
			init_hcfs_system_data();
			hcfs_system->systemdata.system_size = 54321;
			hcfs_system->systemdata.cache_size = 98765;
			hcfs_system->systemdata.cache_blocks = 11223344;
		}
		SYSTEM_DATA_TYPE systemdata_answer;

};

TEST_F(sync_hcfs_system_dataTest, NotNeedLock_SyncSuccess)
{
	SYSTEM_DATA_TYPE test_answer;

	/* Mock data */
	init_mock_data();
	/* Run function */
	ASSERT_EQ(0, sync_hcfs_system_data(FALSE));
	/* Check answer */
	fseek(hcfs_system->system_val_fptr, 0, SEEK_SET);
	fread(&test_answer, sizeof(SYSTEM_DATA_TYPE), 1, hcfs_system->system_val_fptr);
	EXPECT_EQ(0, memcmp(&test_answer, &(hcfs_system->systemdata), 
						sizeof(SYSTEM_DATA_TYPE)));
	fclose(hcfs_system->system_val_fptr);
	unlink(HCFSSYSTEM);
}

TEST_F(sync_hcfs_system_dataTest, NeedLock_SyncSuccess)
{
	SYSTEM_DATA_TYPE test_answer;

	/* Mock data */
	init_mock_data();
	/* Run function */
	ASSERT_EQ(0, sync_hcfs_system_data(TRUE));
	/* Check answer */
	fseek(hcfs_system->system_val_fptr, 0, SEEK_SET);
	fread(&test_answer, sizeof(SYSTEM_DATA_TYPE), 1, hcfs_system->system_val_fptr);
	EXPECT_EQ(0, memcmp(&test_answer, &(hcfs_system->systemdata), 
						sizeof(SYSTEM_DATA_TYPE)));
	fclose(hcfs_system->system_val_fptr);
	unlink(HCFSSYSTEM);
}
/*
	End of unittest for sync_hcfs_system_data()
 */
