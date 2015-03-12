#include "gtest/gtest.h"
extern "C" {
#include "metaops.h"
#include "hfuse_system.h"
#include "mock_params.h"
}

/* Rename HCFS main function */
#define main HCFSMain

extern "C" {
#include "hfuse_system.c"
}

/*
	Unittest for init_hcfs_system_data()
 */

class BaseClassWithSystemPath : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			HCFSSYSTEM = (char *)malloc(sizeof(char) * 100);
			strcpy(HCFSSYSTEM, "/tmp/test_system_file");
		}
		virtual void TearDown()
		{
			free(HCFSSYSTEM);
		}
};

class init_hcfs_system_dataTest : public BaseClassWithSystemPath {

};

TEST_F(init_hcfs_system_dataTest, Creat_and_Init_SystemDataSuccess)
{
	/* Run function */
	ASSERT_EQ(0, init_hcfs_system_data());
	/* Check answer */
	ASSERT_EQ(0, access(HCFSSYSTEM, F_OK));
	ASSERT_EQ(0, unlink(HCFSSYSTEM));
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
	//unlink(HCFSSYSTEM);
	ASSERT_EQ(0, unlink(HCFSSYSTEM));
}
/*
	End of unittest for init_hcfs_system_data()
 */

/*
	Unittest for sync_hcfs_system_data()
 */
class sync_hcfs_system_dataTest : public BaseClassWithSystemPath {
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
	ASSERT_EQ(0, unlink(HCFSSYSTEM));
	//unlink(HCFSSYSTEM);
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
	ASSERT_EQ(0, unlink(HCFSSYSTEM));
}
/*
	End of unittest for sync_hcfs_system_data()
 */

/*
	Unittest of init_hfuse()
 */

class init_hfuseTest : public BaseClassWithSystemPath {
};

TEST_F(init_hfuseTest, RootMetaPathAlreadyExist)
{
	mknod("/tmp/root_meta_path", 0700, S_IFREG);
	/* Run function */
	ASSERT_EQ(0, init_hfuse());
	/* Remove */
	unlink("/tmp/root_meta_path");
	unlink(HCFSSYSTEM);
}

TEST_F(init_hfuseTest, RootMetaPath)
{
	FILE *meta_ptr;
	struct stat result_stat;
	DIR_META_TYPE result_meta;

	/* Run function */
	ASSERT_EQ(0, init_hfuse());
	/* Check_answer */
	meta_ptr = fopen("/tmp/root_meta_path", "r+");
	ASSERT_TRUE( NULL != meta_ptr );
	fseek(meta_ptr, 0, SEEK_SET);
	fread(&result_stat, sizeof(struct stat), 1, meta_ptr);
	fseek(meta_ptr, sizeof(struct stat), SEEK_SET);
	fread(&result_meta, sizeof(DIR_META_TYPE), 1, meta_ptr);
	EXPECT_EQ(getuid(), result_stat.st_uid);
	EXPECT_EQ(getgid(), result_stat.st_gid);
	EXPECT_EQ(1, result_stat.st_ino);
	EXPECT_EQ(sizeof(DIR_META_TYPE) + sizeof(struct stat), result_meta.root_entry_page);
	/* Remove */
	unlink("/tmp/root_meta_path");
	unlink(HCFSSYSTEM);
}

/*
	End of unittest of init_hfuse()
 */

/*
	Unittest of main function
 */
class HCFSMainTest : public ::testing::Test {
	protected:
		virtual	void SetUp()
		{
			tmp_argv = (char **)malloc(sizeof(char *) * 1);
			tmp_argv[0] = (char *)malloc(sizeof(char) * 20);
			strcpy(tmp_argv[0], "none");
		}
		virtual void TearDown()
		{

		}
		char **tmp_argv;
};

TEST_F(HCFSMainTest, InitBackendFail)
{
	hcfs_list_container_success = TRUE;
	hcfs_init_backend_success = FALSE;
	/* Test */	
	EXPECT_EXIT(main(1, tmp_argv), testing::ExitedWithCode(0), "");
}

TEST_F(HCFSMainTest, ListContainerFail)
{
	hcfs_list_container_success = FALSE;
	hcfs_init_backend_success = TRUE;
	/* Test */
	EXPECT_EXIT(main(1, tmp_argv), testing::ExitedWithCode(0), "");
}

TEST_F(HCFSMainTest, MainFunctionSuccess)
{
	int saved_stdout = dup(fileno(stdout));
	int saved_stderr = dup(fileno(stderr));
	int pid = getpid();

	hcfs_list_container_success = TRUE;
	hcfs_init_backend_success = TRUE;
	/* Test */
	ASSERT_EQ(0, main(1, tmp_argv));
	if(getpid() == pid){
		pid_t pid = wait(NULL);
		if(pid!=-1)
			kill(pid, SIGKILL);
		pid = wait(NULL);
		if(pid!=-1)
			kill(pid, SIGKILL);
	}
	EXPECT_EQ(0, access("backend_upload_log", F_OK));
	EXPECT_EQ(0, access("cache_maintain_log", F_OK));
	EXPECT_EQ(0, access("fuse_log", F_OK));
	/* Restore */
	unlink("backend_upload_log");
	unlink("cache_maintain_log");
	unlink("fuse_log");
	dup2(saved_stdout, fileno(stdout));
	dup2(saved_stderr, fileno(stderr));
}
/*
	End of unittest of main function
 */
