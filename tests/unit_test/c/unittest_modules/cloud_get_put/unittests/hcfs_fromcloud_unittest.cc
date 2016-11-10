#define __STDC_FORMAT_MACROS
#include <semaphore.h>
#include <string>
#include <errno.h>
#include <ftw.h>
#include "gtest/gtest.h"
#include "curl/curl.h"
#include "attr/xattr.h"
extern "C" {
#include "hcfscurl.h"
#include "global.h"
#include "hcfs_fromcloud.h"
#include "fuseop.h"
#include "mock_params.h"
#include "meta_mem_cache.h"
#include "utils.h"
#include "ut_global.h"
}

extern SYSTEM_DATA_HEAD *hcfs_system;

static int do_delete(const char *fpath, const struct stat *sb,
                     int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
	case FTW_D:
	case FTW_DNR:
	case FTW_DP:
		rmdir(fpath);
		break;
	default:
		unlink(fpath);
		break;
	}
	return (0);
}

class fromcloudEnvironment : public ::testing::Environment
{
public:
	char *workpath, *tmppath;

	virtual void SetUp() {
		OPEN_BLOCK_PATH_FAIL = FALSE;
		OPEN_META_PATH_FAIL = FALSE;
		FETCH_BACKEND_BLOCK_TESTING = FALSE;
		hcfs_system = (SYSTEM_DATA_HEAD *)
		              malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);

		workpath = get_current_dir_name();
		tmppath = (char *)malloc(strlen(workpath) + 20);
		snprintf(tmppath, strlen(workpath) + 20, "%s/tmpdir", workpath);
		if (access(tmppath, F_OK) != 0)
			mkdir(tmppath, 0700);
		if (access("/tmp/testHCFS", F_OK) == 0) {
			unlink("/tmp/testHCFS");
		}
		symlink(tmppath, "/tmp/testHCFS");
	}

	virtual void TearDown() {
		free(hcfs_system);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
		unlink("/tmp/testHCFS");
		if (workpath != NULL)
			free(workpath);
		if (tmppath != NULL)
			free(tmppath);
	}
};

::testing::Environment* const fromcloud_env =
	::testing::AddGlobalTestEnvironment(new fromcloudEnvironment);

// Unittest of fetch_from_cloud()

class fetch_from_cloudTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		//sem_init(&(hcfs_system->xfer_download_in_progress_sem), 0, 0);
		for (int32_t i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++)
			curl_handle_mask[i] = FALSE;

		num_obj = 35; // fetch 35 blocks
		objname_counter = 0; // Counter of recording actual value
		expected_obj_counter = 0; // Counter of recording expected value
		sem_init(&objname_counter_sem, 0, 1);
		objname_list = (char **)malloc(sizeof(char *)*num_obj);
		for (int32_t i = 0 ; i < num_obj ; i++)
			objname_list[i] = (char *)malloc(sizeof(char) * 40);

		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_paused = FALSE;
	}

	virtual void TearDown() {
		sem_destroy(&download_curl_sem);
		sem_destroy(&download_curl_control_sem);
		sem_destroy(&objname_counter_sem);
		for (int32_t i = 0 ; i < num_obj ; i++)
			free(objname_list[i]);
		free(objname_list);
	}

	// Static thread function, which is used to run function
	// fetch_from_cloud() */
	static void *fetch_from_cloud_for_thread(void *arg) {
		int64_t block_no = *((int64_t*)arg);
		char tmp_filename[50];
		char objname[100];
		FILE *fptr;

		sprintf(objname, "data_1_%ld", block_no);
		sprintf(tmp_filename, "/tmp/testHCFS/local_space%ld", block_no);
		fptr = fopen(tmp_filename, "w+");
		fetch_from_cloud(fptr, READ_BLOCK, objname);
		fclose(fptr);
		unlink(tmp_filename);
		return NULL;
	}

	std::string expected_objname[100]; // Expected answer list
	int32_t expected_obj_counter;
	int32_t num_obj;
};

int32_t objname_cmp(const void *s1, const void *s2)
{
	char *name1 = *(char **)s1;
	char *name2 = *(char **)s2;
	int32_t inode1, block1;
	int32_t inode2, block2;
	sscanf(name1, "data_%d_%d", &inode1, &block1);
	sscanf(name2, "data_%d_%d", &inode2, &block2);
	return block1 - block2;
}

TEST_F(fetch_from_cloudTest, BackendOffline)
{
	hcfs_system->backend_is_online = FALSE;
	hcfs_system->sync_paused = TRUE;

	EXPECT_EQ(-EIO, fetch_from_cloud(NULL, 0, NULL));
}

TEST_F(fetch_from_cloudTest, FetchOneFile)
{
	char tmp_filename[50];
	char objname[100];
	char buffer[EXTEND_FILE_SIZE];
	FILE *fptr;
	off_t offset;

	sprintf(objname, TEST_DOWNLOAD_OBJ);
	sprintf(tmp_filename, "/tmp/testHCFS/data_test_download");
	unlink(tmp_filename);
	fptr = fopen(tmp_filename, "w+");
	setbuf(fptr, NULL);
	fetch_from_cloud(fptr, READ_BLOCK, objname);
	fseek(fptr, 0, SEEK_SET);
	fgets(buffer, 100, fptr);
	fclose(fptr);
	unlink(tmp_filename);
	EXPECT_EQ(0, strncmp(buffer, "Test content", strlen("Test content")));
}

TEST_F(fetch_from_cloudTest, FetchSuccess)
{
	pthread_t *tid = (pthread_t *)calloc(num_obj, sizeof(pthread_t));
	int64_t *block_no = (int64_t *)calloc(num_obj, sizeof(int64_t));

	/* Run fetch_from_cloud() with multi-threads */
	for (int32_t i = 0 ; i < num_obj ; i++) {
		char tmp_filename[20];
		block_no[i] = (i + 1) * 5;
		EXPECT_EQ(0, pthread_create(&tid[i], NULL,
		                            fetch_from_cloudTest::fetch_from_cloud_for_thread, (void *)&block_no[i]));

		sprintf(tmp_filename, "data_%d_%ld", 1, block_no[i]); // Expected value
		expected_objname[expected_obj_counter++] = std::string(tmp_filename);
	}
	sleep(1);

	/* Check answer */
	EXPECT_EQ(num_obj, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *), objname_cmp); // Sort actual value
	for (int32_t i = 0 ; i < num_obj ; i++) {
		pthread_join(tid[i], NULL);
		ASSERT_STREQ(expected_objname[i].c_str(), objname_list[i]);
	}
}

//	End of unittest of fetch_from_cloud()

//	Unittest of prefetch_block()

class prefetch_blockTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		prefetch_ptr = (PREFETCH_STRUCT_TYPE *)malloc(sizeof(PREFETCH_STRUCT_TYPE));
		prefetch_ptr->this_inode = 1;
		prefetch_ptr->block_no = 1;
		prefetch_ptr->page_start_fpos = 0;
		prefetch_ptr->entry_index = 0;
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		/* Used to fetch_from_cloud */
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (int32_t i = 0 ; i < MAX_DOWNLOAD_CURL_HANDLE ; i++)
			curl_handle_mask[i] = FALSE;

		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_paused = FALSE;
	}

	virtual void TearDown() {
		sem_destroy(&download_curl_sem);
		sem_destroy(&download_curl_control_sem);
	}

	PREFETCH_STRUCT_TYPE *prefetch_ptr;
};

TEST_F(prefetch_blockTest, BlockStatus_is_neither_STCLOUD_STCtoL)
{
	FILE *metafptr;
	BLOCK_ENTRY_PAGE mock_page;
	mock_page.num_entries = 1;
	mock_page.block_entries[0].status = ST_LDISK;
	//mock_page.next_page = 0;
	metafptr = fopen("/tmp/testHCFS/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NUM__FETCH_SUCCESS;

	/* Run */
	prefetch_block(prefetch_ptr);

	/* Check answer */
	EXPECT_EQ(0, hcfs_system->systemdata.cache_size);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_blocks);
	EXPECT_EQ(0, access("/tmp/testHCFS/tmp_block", F_OK));
	unlink("/tmp/testHCFS/tmp_meta");
	unlink("/tmp/testHCFS/tmp_block");
}

TEST_F(prefetch_blockTest, PrefetchSuccess)
{
	FILE *metafptr;
	BLOCK_ENTRY_PAGE mock_page;
	BLOCK_ENTRY_PAGE result_page;
	int32_t entry_index;
	int32_t meta_fpos;
	char xattr_result;
	int32_t ret, errcode;

	entry_index = prefetch_ptr->entry_index;
	meta_fpos = prefetch_ptr->page_start_fpos;
	mock_page.num_entries = 1;
	mock_page.block_entries[0].status = ST_CLOUD;
	//mock_page.next_page = 0;

	metafptr = fopen("/tmp/testHCFS/tmp_meta", "w+");
	fwrite(&mock_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	prefetch_ptr->block_no = BLOCK_NUM__FETCH_SUCCESS; // Control success or fail to fetch from cloud.

	/* Run */
	prefetch_block(prefetch_ptr);

	/* Check answer */
	EXPECT_EQ(ROUND_SIZE(EXTEND_FILE_SIZE), hcfs_system->systemdata.cache_size); // Total size = expected block size
	EXPECT_EQ(1, hcfs_system->systemdata.cache_blocks); // Prefetch one block from cloud
	EXPECT_EQ(0, access("/tmp/testHCFS/tmp_block", F_OK)); // Mock block path
	ret = getxattr("/tmp/testHCFS/tmp_block", "user.dirty", &xattr_result,
	               sizeof(char));
	if (ret < 0) {
		errcode = errno;
		printf("Failed to getxattr. Code %d, %s\n", errcode,
		       strerror(errcode));
	}
	EXPECT_EQ(1, ret);
	EXPECT_EQ('F', xattr_result); // xattr
	metafptr = fopen("/tmp/testHCFS/tmp_meta", "r");
	fseek(metafptr, meta_fpos, SEEK_SET);
	fread(&result_page, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
	fclose(metafptr);
	EXPECT_EQ(ST_BOTH, result_page.block_entries[entry_index].status); // status

	unlink("/tmp/testHCFS/tmp_meta");
	unlink("/tmp/testHCFS/tmp_block");
}

TEST_F(prefetch_blockTest, PrefetchFail)
{
	/* Does prefetch_block fail? It seems that fetch_from_cloud() never return with failure  */
}

//	End of unittest of prefetch_block()

// Unittest for download_block_manager
class download_block_managerTest : public ::testing::Test {
protected:
	void SetUp() {
		memset(&download_thread_ctl, 0, sizeof(DOWNLOAD_THREAD_CTL));
		sem_init(&(download_thread_ctl.ctl_op_sem), 0, 1);
		sem_init(&(download_thread_ctl.dl_th_sem), 0,
		         MAX_PIN_DL_CONCURRENCY);
	}

	void TearDown() {
	}
};

void* mock_thread_fn(void *arg)
{
	return NULL;
}

TEST_F(download_block_managerTest, CollectThreadsSuccess)
{
	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;

	/* Create download_block_manager */
	pthread_create(&(download_thread_ctl.manager_thread), NULL,
	               &download_block_manager, NULL);

	for (int32_t i = 0; i < MAX_PIN_DL_CONCURRENCY / 2; i++) {
		download_thread_ctl.block_info[i].dl_error = FALSE;
		download_thread_ctl.block_info[i].active = TRUE;
		sem_wait(&(download_thread_ctl.ctl_op_sem));
		pthread_create(&(download_thread_ctl.download_thread[i]),
		               NULL, mock_thread_fn, NULL);
		download_thread_ctl.active_th++;
		sem_post(&(download_thread_ctl.ctl_op_sem));
	}

	hcfs_system->system_going_down = TRUE;
	sleep(1);

	pthread_join(download_thread_ctl.manager_thread, NULL);

	/* Verify */
	EXPECT_EQ(0, download_thread_ctl.active_th);
	for (int32_t i = 0; i < MAX_PIN_DL_CONCURRENCY; i++) {
		ASSERT_EQ(FALSE,
		          download_thread_ctl.block_info[i].active);
	}
}

TEST_F(download_block_managerTest, CollectThreadsSuccess_With_ThreadError)
{
	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;

	/* Create download_block_manager */
	pthread_create(&(download_thread_ctl.manager_thread), NULL,
	               &download_block_manager, NULL);

	for (int32_t i = 0; i < MAX_PIN_DL_CONCURRENCY; i++) {
		download_thread_ctl.block_info[i].active = TRUE;
		download_thread_ctl.block_info[i].dl_error = TRUE;
		download_thread_ctl.block_info[i].this_inode = i;
		sem_wait(&(download_thread_ctl.ctl_op_sem));
		pthread_create(&(download_thread_ctl.download_thread[i]),
		               NULL, &mock_thread_fn, NULL);
		download_thread_ctl.active_th++;
		sem_post(&(download_thread_ctl.ctl_op_sem));
	}

	hcfs_system->system_going_down = TRUE;
	sleep(1);

	pthread_join(download_thread_ctl.manager_thread, NULL);

	/* Verify */
	EXPECT_EQ(0, download_thread_ctl.active_th);
	for (int32_t i = 0; i < MAX_PIN_DL_CONCURRENCY; i++) {
		char error_path[200];

		fetch_error_download_path(error_path, (ino_t)i);
		EXPECT_EQ(FALSE,
		          download_thread_ctl.block_info[i].active);
		EXPECT_EQ(0, access(error_path, F_OK));
		unlink(error_path);
	}
}

/* End of unittest for download_block_manager */

/* Unittest for fetch_pinned_blocks */
class fetch_pinned_blocksTest : public ::testing::Test {
protected:
	char metapath[200];

	void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
		                malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		MAX_BLOCK_SIZE = 100;
		CACHE_FULL = FALSE;

		fetch_meta_path(metapath, 0);
		if (access(metapath, F_OK) == 0)
			unlink(metapath);

		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		init_download_control();
	}

	void TearDown() {
		CACHE_FULL = FALSE;

		fetch_meta_path(metapath, 0);
		if (access(metapath, F_OK) == 0)
			unlink(metapath);

		hcfs_system->system_going_down = TRUE;
		destroy_download_control();
		free(system_config);
	}
};

TEST_F(fetch_pinned_blocksTest, MetaNotExist)
{
	ino_t inode = 5;

	/* Test */
	EXPECT_EQ(-ENOENT, fetch_pinned_blocks(inode));
}

TEST_F(fetch_pinned_blocksTest, NotRegfile_DirectlyReturn)
{
	ino_t inode;
	FILE *fptr;
	HCFS_STAT tmpstat;

	inode = 5;
	fetch_meta_path(metapath, inode);
	memset(&tmpstat, 0 , sizeof(HCFS_STAT));
	tmpstat.mode = S_IFDIR;
	fptr = fopen(metapath, "w+");
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fclose(fptr);

	/* Test */
	EXPECT_EQ(0, fetch_pinned_blocks(inode));

	/* Recover */
	unlink(metapath);
}

TEST_F(fetch_pinned_blocksTest, SystemGoingDown)
{
	ino_t inode;
	FILE *fptr;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;

	inode = 5;
	fetch_meta_path(metapath, inode);
	memset(&tmpstat, 0 , sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	tmpstat.mode = S_IFREG;
	tmpstat.size = 5;
	fptr = fopen(metapath, "w+");
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fclose(fptr);

	hcfs_system->system_going_down = TRUE;

	/* Test */
	EXPECT_EQ(-ESHUTDOWN, fetch_pinned_blocks(inode));

	/* Recover */
	unlink(metapath);
}

TEST_F(fetch_pinned_blocksTest, InodeBeUnpinned)
{
	ino_t inode;
	FILE *fptr;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE tmppage;

	inode = 5;
	fetch_meta_path(metapath, inode);
	memset(&tmpstat, 0 , sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
	tmpstat.mode = S_IFREG;
	tmpstat.size = 5;
	tmppage.block_entries[0].status = ST_LDISK;
	filemeta.local_pin = FALSE;

	fptr = fopen(metapath, "w+");
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);

	/* Test */
	EXPECT_EQ(-EPERM, fetch_pinned_blocks(inode));

	/* Recover */
	unlink(metapath);
}

TEST_F(fetch_pinned_blocksTest, BlockStatusIsLocal)
{
	ino_t inode;
	FILE *fptr;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE tmppage;

	inode = 5;
	fetch_meta_path(metapath, inode);
	memset(&tmpstat, 0 , sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
	tmpstat.mode = S_IFREG;
	tmpstat.size = 5;
	tmppage.block_entries[0].status = ST_LDISK;
	filemeta.local_pin = TRUE;

	fptr = fopen(metapath, "w+");
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);

	/* Test */
	EXPECT_EQ(0, fetch_pinned_blocks(inode));

	/* Recover */
	unlink(metapath);
}

/* End of unittest for fetch_pinned_blocks */

/* Unittest for fetch_backend_block */
class fetch_backend_blockTest : public ::testing::Test {
protected:
	void SetUp() {
		mkdir("/tmp/testHCFS", 0700);
		mknod("/tmp/testHCFS/tmp_meta", 0700, 0);
		OPEN_BLOCK_PATH_FAIL = FALSE;
		OPEN_META_PATH_FAIL = FALSE;
		FETCH_BACKEND_BLOCK_TESTING = TRUE;

		memset(&download_thread_ctl, 0, sizeof(DOWNLOAD_THREAD_CTL));
		download_thread_ctl.block_info[0].this_inode = 1;
		download_thread_ctl.block_info[0].block_no = 0;
		download_thread_ctl.block_info[0].dl_error = FALSE;

		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		sem_init(&nonread_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
		//sem_init(&pin_download_curl_sem, 0,
		//		MAX_DOWNLOAD_CURL_HANDLE / 2);
	}

	void TearDown() {
		nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
	}
};

TEST_F(fetch_backend_blockTest, FailToOpenBlockPath)
{
	pthread_t tid;

	OPEN_BLOCK_PATH_FAIL = TRUE;

	/* Test */
	pthread_create(&tid, NULL, fetch_backend_block,
	               &(download_thread_ctl.block_info[0]));
	pthread_join(tid, NULL);

	/* Verify */
	EXPECT_EQ(TRUE, download_thread_ctl.block_info[0].dl_error);
}

TEST_F(fetch_backend_blockTest, BlockStatusIsLDISK)
{
	pthread_t tid;

	NOW_STATUS = ST_LDISK;
	OPEN_META_PATH_FAIL = FALSE;

	/* Test */
	pthread_create(&tid, NULL, fetch_backend_block,
	               &(download_thread_ctl.block_info[0]));
	pthread_join(tid, NULL);

	/* Verify */
	EXPECT_EQ(FALSE, download_thread_ctl.block_info[0].dl_error);
}

TEST_F(fetch_backend_blockTest, FetchSuccess)
{
	pthread_t tid;

	NOW_STATUS = ST_CLOUD;
	OPEN_META_PATH_FAIL = FALSE;

	/* Test */
	pthread_create(&tid, NULL, fetch_backend_block,
	               &(download_thread_ctl.block_info[0]));
	pthread_join(tid, NULL);

	/* Verify */
	EXPECT_EQ(FALSE, download_thread_ctl.block_info[0].dl_error);
}

/* End of unittest for fetch_backend_block */

/* Unittest for fetch_quota_from_cloud */
class fetch_quota_from_cloudTest : public ::testing::Test
{
protected:
	char download_path[200];
	void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
		                malloc(sizeof(SYSTEM_CONF_STRUCT));
		system_config->metapath = (char *)malloc(100);
		strcpy(METAPATH, "fetch_quota_from_cloud_folder");
		if (!access(METAPATH, F_OK))
			nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		mkdir(METAPATH, 0700);
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		sem_init(&(hcfs_system->access_sem), 0, 1);

		memset(&download_usermeta_ctl, 0, sizeof(DOWNLOAD_USERMETA_CTL));
		sem_init(&(download_usermeta_ctl.access_sem), 0, 1);

		sprintf(download_path, "%s/new_usermeta", METAPATH);
		hcfs_system->systemdata.system_quota = 0;

		/* global var to control UT */
		usermeta_notfound = FALSE;
		FETCH_BACKEND_BLOCK_TESTING = FALSE;
	}

	void TearDown() {
		if (!access(download_path, F_OK))
			unlink(download_path);
		nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		free(METAPATH);
		free(system_config);
	}
};

TEST_F(fetch_quota_from_cloudTest, UsermetaNotFoundOnCloud)
{
	usermeta_notfound = TRUE;
	download_usermeta_ctl.active = TRUE;
	fetch_quota_from_cloud(NULL, TRUE);

	EXPECT_EQ(-1, access(download_path, F_OK));
	EXPECT_EQ(FALSE, download_usermeta_ctl.active);
	EXPECT_EQ(0, hcfs_system->systemdata.system_quota);
}

TEST_F(fetch_quota_from_cloudTest, FetchSuccess)
{
	usermeta_notfound = FALSE;
	download_usermeta_ctl.active = TRUE;
	fetch_quota_from_cloud(NULL, TRUE);

	EXPECT_EQ(-1, access(download_path, F_OK));
	EXPECT_EQ(FALSE, download_usermeta_ctl.active);
	EXPECT_EQ(5566, hcfs_system->systemdata.system_quota);
}

TEST_F(fetch_quota_from_cloudTest, SystemGoingDown)
{
	hcfs_system->system_going_down = TRUE;
	download_usermeta_ctl.active = TRUE;
	fetch_quota_from_cloud(NULL, TRUE);

	EXPECT_EQ(-1, access(download_path, F_OK));
	EXPECT_EQ(FALSE, download_usermeta_ctl.active);
	EXPECT_EQ(0, hcfs_system->systemdata.system_quota);
}
/* End of unittest for fetch_quota_from_cloud */

/* Unittest for update_quota() */
class update_quotaTest : public ::testing::Test {
protected:
	void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
		                malloc(sizeof(SYSTEM_CONF_STRUCT));
		system_config->metapath = (char *)malloc(100);
		strcpy(METAPATH, "fetch_quota_from_cloud_folder");
		if (!access(METAPATH, F_OK))
			nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		mkdir(METAPATH, 0700);
		CURRENT_BACKEND = NONE;

		memset(&download_usermeta_ctl, 0,
		       sizeof(DOWNLOAD_USERMETA_CTL));
		sem_init(&(download_usermeta_ctl.access_sem), 0, 1);
		download_usermeta_ctl.active = FALSE;

		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
	}

	void TearDown() {
		nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		free(METAPATH);
		free(system_config);
	}
};

TEST_F(update_quotaTest, NoBackend_RejectUpdate)
{
	CURRENT_BACKEND = NONE;

	EXPECT_EQ(-EPERM, update_quota());
	EXPECT_EQ(FALSE, download_usermeta_ctl.active);
}

TEST_F(update_quotaTest, ThreadIsRunning)
{
	download_usermeta_ctl.active = TRUE;
	CURRENT_BACKEND = SWIFT;

	EXPECT_EQ(-EBUSY, update_quota());
	EXPECT_EQ(TRUE, download_usermeta_ctl.active);
}

TEST_F(update_quotaTest, CreateThreadSuccess)
{
	/* Let thread sleep in the loop */
	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = FALSE;
	download_usermeta_ctl.active = FALSE;
	CURRENT_BACKEND = SWIFT;

	EXPECT_EQ(0, update_quota());
	EXPECT_EQ(TRUE, download_usermeta_ctl.active);

	/* Wake the thread up */
	hcfs_system->system_going_down = TRUE;
	sleep(2);
	EXPECT_EQ(FALSE, download_usermeta_ctl.active);
}

/* End of unittest for update_quota() */

/**
 * Unittest of fetch_object_busywait_conn()
 */
FILE *fptr;
char action_from;
char objname[200];
int ret_val;

class fetch_object_busywait_connTest : public ::testing::Test {
public:
	static void *testEntry(void *ptr) {
		ret_val = fetch_object_busywait_conn(fptr,
		                                     action_from, objname);
		return NULL;
	}

protected:
	pthread_t tid;

	void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
		                malloc(sizeof(SYSTEM_CONF_STRUCT));
		system_config->metapath = (char *)malloc(100);
		strcpy(METAPATH, "fetch_object_busywait_connTest");
		if (!access(METAPATH, F_OK))
			rmdir(METAPATH);
		mkdir(METAPATH, 0700);
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;

		memset(&download_usermeta_ctl, 0, sizeof(DOWNLOAD_USERMETA_CTL));
		sem_init(&(download_usermeta_ctl.access_sem), 0, 1);
		sem_init(&nonread_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);

		/* global var to control UT */
		usermeta_notfound = FALSE;
		FETCH_BACKEND_BLOCK_TESTING = FALSE;
	}

	void TearDown() {
		rmdir(METAPATH);
		free(METAPATH);
		free(system_config);
	}
};

TEST_F(fetch_object_busywait_connTest, BackendOffline_SystemShutdown)
{
	char objpath[200];

	CURRENT_BACKEND = SWIFT;
	hcfs_system->backend_is_online = FALSE;
	hcfs_system->system_going_down = FALSE;

	sprintf(objpath, "%s/mock_object", METAPATH);
	fptr = fopen(objpath, "w+");
	action_from = RESTORE_FETCH_OBJ;
	strcpy(objname, "mock_obj");

	/* Run */
	pthread_create(&tid, NULL,
	               &fetch_object_busywait_connTest::testEntry,
	               NULL);
	sleep(1);
	hcfs_system->system_going_down = TRUE;
	pthread_join(tid, NULL);

	/* Verify */
	EXPECT_EQ(-ESHUTDOWN, ret_val);

	fclose(fptr);
	unlink(objpath);
}

TEST_F(fetch_object_busywait_connTest, Backend_From_Offline_To_Online)
{
	char objpath[200];

	CURRENT_BACKEND = SWIFT;
	hcfs_system->backend_is_online = FALSE;
	hcfs_system->system_going_down = FALSE;
	usermeta_notfound = TRUE;

	sprintf(objpath, "%s/mock_object", METAPATH);
	fptr = fopen(objpath, "w+");
	action_from = RESTORE_FETCH_OBJ;
	strcpy(objname, "user_mock_obj");

	/* Run */
	pthread_create(&tid, NULL,
	               &fetch_object_busywait_connTest::testEntry, NULL);
	sleep(1);
	hcfs_system->backend_is_online = TRUE;
	pthread_join(tid, NULL);

	/* Verify */
	EXPECT_EQ(-ENOENT, ret_val);

	fclose(fptr);
	unlink(objpath);
}
/**
 * End unittest of fetch_object_busywait_conn()
 */
