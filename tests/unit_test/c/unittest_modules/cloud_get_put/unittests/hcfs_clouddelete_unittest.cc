#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <ftw.h>
#include "gtest/gtest.h"
#include "mock_params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
#include "atomic_tocloud.h"
}

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir (fpath);
			break;
		default:
			unlink (fpath);
			break;
	}
	return (0);
}

class deleteEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    hcfs_system->backend_is_online = TRUE;
    hcfs_system->sync_manual_switch = ON;
    hcfs_system->sync_paused = OFF;

    workpath = NULL;
    tmppath = NULL;
    if (access("/tmp/testHCFS", F_OK) != 0) {
      workpath = get_current_dir_name();
      tmppath = (char *)malloc(strlen(workpath)+20);
      snprintf(tmppath, strlen(workpath)+20, "%s/tmpdir", workpath);
      if (access(tmppath, F_OK) != 0)
        mkdir(tmppath, 0700);
      symlink(tmppath, "/tmp/testHCFS");
     }
  }

  virtual void TearDown() {
    nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
    free(workpath);
    free(tmppath);
    free(hcfs_system);
  }
};

::testing::Environment* const delete_env = ::testing::AddGlobalTestEnvironment(new deleteEnvironment);


/*
	Unittest of init_dsync_control() & collect_finished_dsync_threads()
 */
void *dsync_test_thread_fn(void *data)
{
	usleep(10000 * *(int32_t *)data);
	return NULL;
}

extern DSYNC_THREAD_CONTROL dsync_ctl;

TEST(init_dsync_controlTest, ControlDsyncThreadSuccess)
{
	void *res;
	int32_t num_threads = 40;

	/* Run the function to check whether it will terminate threads */
	init_dsync_control();

	/* Generate threads */
	for (int32_t i = 0 ; i < num_threads ; i++) {
		int32_t inode = i+1;
		int32_t t_index;

		sem_wait(&dsync_ctl.dsync_queue_sem);
		sem_wait(&dsync_ctl.dsync_op_sem);
		t_index = -1;
		for (int32_t idx = 0 ; idx < MAX_DSYNC_CONCURRENCY ; idx++) {
			if ((dsync_ctl.threads_in_use[idx] == 0) &&
				(dsync_ctl.threads_created[idx] == FALSE)) {
				t_index = idx;
				break;
			}
		}
		dsync_ctl.threads_in_use[t_index] = inode;
		dsync_ctl.threads_created[t_index] = TRUE;
		dsync_ctl.threads_finished[t_index] = TRUE;
		dsync_ctl.retry_right_now[t_index] = FALSE;
		dsync_ctl.total_active_dsync_threads++;
		EXPECT_EQ(0, pthread_create(&(dsync_ctl.inode_dsync_thread[t_index]), NULL,
			dsync_test_thread_fn, (void *)&i));
		sem_post(&dsync_ctl.dsync_op_sem);
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(dsync_ctl.dsync_handler_thread));
	EXPECT_EQ(0, pthread_join(dsync_ctl.dsync_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);

	/* Check answer */
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	for (int32_t i = 0 ; i < MAX_DSYNC_CONCURRENCY ; i++) {
		ASSERT_EQ(0, dsync_ctl.threads_in_use[i]) << "thread_no = " << i;
		ASSERT_EQ(FALSE, dsync_ctl.threads_created[i]) << "thread_no = " << i;
	}
}
/*
	End of unittest init_dsync_control() & collect_finished_dsync_threads()
 */

/*
	Unittest of init_delete_control() & collect_finished_delete_threads()
 */
void *delete_test_thread_fn(void *data)
{
	usleep(10000 * *(int32_t *)data);
	return NULL;
}

extern DELETE_THREAD_CONTROL delete_ctl;

TEST(init_delete_controlTest, ControlDeleteThreadSuccess)
{
	void *res;
	int32_t num_threads = 40;

	/* Run the function to check whether it will terminate threads */
	init_delete_control();

	/* Generate threads */
	for (int32_t i = 0 ; i < num_threads ; i++) {
		int32_t t_index;

		sem_wait(&delete_ctl.delete_queue_sem);
		sem_wait(&delete_ctl.delete_op_sem);
		t_index = -1;
		for (int32_t idx = 0 ; idx < MAX_DELETE_CONCURRENCY ; idx++) {
			if((delete_ctl.threads_in_use[idx] == FALSE) &&
				(delete_ctl.threads_created[idx] == FALSE)) {
				t_index = idx;
				break;
			}
		}
		delete_ctl.threads_in_use[t_index] = TRUE;
		delete_ctl.threads_created[t_index] = TRUE;
		delete_ctl.threads_finished[t_index] = TRUE;
		delete_ctl.delete_threads[t_index].is_block = TRUE;
		delete_ctl.total_active_delete_threads++;
		EXPECT_EQ(0, pthread_create(&(delete_ctl.threads_no[t_index]), NULL,
			delete_test_thread_fn, (void *)&i));
		sem_post(&delete_ctl.delete_op_sem);
	}
	sleep(1);
	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);

	/* Check answer */
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads);
	for (int32_t i = 0 ; i < MAX_DELETE_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, delete_ctl.threads_in_use[i]) << "thread_no = " << i;
		ASSERT_EQ(FALSE, delete_ctl.threads_created[i]) << "thread_no = " << i;
	}
}

/*
	End of unittest init_delete_control() & collect_finished_delete_threads()
 */

/*
	Unittest of dsync_single_inode()
*/
class dsync_single_inodeTest : public ::testing::Test {
public:
	DSYNC_THREAD_TYPE *mock_thread_info;
	unsigned expected_num_objname;
	unsigned size_objname;
	char backend_meta[100];

	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			malloc(sizeof(SYSTEM_CONF_STRUCT));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		mock_thread_info =
		    (DSYNC_THREAD_TYPE *)calloc(1, sizeof(DSYNC_THREAD_TYPE));
		backend_meta[0] = 0;
	}
	virtual void TearDown()
	{
		sem_destroy(&objname_counter_sem);
		sem_destroy(&(sync_ctl.sync_op_sem));
		destroy_objname_buffer(expected_num_objname);
		free(mock_thread_info);
		free(system_config);
		if (!access(backend_meta, F_OK))
			unlink(backend_meta);
		free(dsync_ctl.retry_list.retry_inode);
		dsync_ctl.retry_list.retry_inode = NULL;
	}
	void init_objname_buffer(uint32_t num_objname)
	{
		size_objname = 50;
		objname_counter = 0;
		objname_list = (char **)malloc(sizeof(char *) * num_objname);
		for (uint32_t i = 0 ; i < num_objname ; i++)
			objname_list[i] = (char *)calloc(size_objname, sizeof(char));
		ASSERT_EQ(0, sem_init(&objname_counter_sem, 0, 1));
	}
	void destroy_objname_buffer(uint32_t num_objname)
	{
		uint32_t i;
		for (i = 0 ; i < num_objname ; i++)
			if(objname_list[i])
				free(objname_list[i]);
		if (objname_list)
			free(objname_list);
	}
	void init_sync_ctl()
	{
		sem_init(&(sync_ctl.sync_op_sem), 0, 1);
		for (int32_t i = 0 ; i < MAX_SYNC_CONCURRENCY ; i++)
			sync_ctl.threads_in_use[i] = 0;
	}
	static int32_t objname_cmp(const void *s1, const void *s2)
	{
		char *name1 = *(char **)s1;
		char *name2 = *(char **)s2;
		if (name1[0] == 'm') {
			return 1;
		} else if (name2[0] == 'm') {
			return -1;
		} else {
			char tmp_name[30];
			int32_t inode1, inode2;
			int32_t blocknum1, blocknum2;
			sscanf(name1, "data_%d_%d", &inode1, &blocknum1);
			sscanf(name2, "data_%d_%d", &inode2, &blocknum2);
			return  -blocknum2 + blocknum1;
		}

	}
};

TEST_F(dsync_single_inodeTest, DeleteAllBlockSuccess)
{
	FILE *meta;
	HCFS_STAT meta_stat;
	BLOCK_ENTRY_PAGE tmp_blockentry_page = {0};
	FILE_META_TYPE tmp_file_meta = {0};
	CLOUD_RELATED_DATA cloud_related = {0};
	FILE_STATS_TYPE file_stst = {0};
	int total_page = 3;
	void *res;
	FILE *backend_meta_fptr;
	size_t size;
	char buf[5000];
	expected_num_objname = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;

	init_hcfs_stat(&meta_stat);
	/* Mock an inode info & a meta file */
	mock_thread_info->inode = INODE__FETCH_TODELETE_PATH_SUCCESS;
	mock_thread_info->this_mode = S_IFREG;
	mock_thread_info->which_index = 0;
	meta_stat.size = 1000000; // Let total_blocks = 1000000/100 = 10000
	meta_stat.mode = S_IFREG; 
	MAX_BLOCK_SIZE = 100;
	cloud_related.upload_seq = 1;

	meta = fopen(TODELETE_PATH, "w+"); // Open mock meta
	ASSERT_NE(0, (meta != NULL));
	setbuf(meta, NULL);
	fseek(meta, 0, SEEK_SET);
	fwrite(&meta_stat, sizeof(HCFS_STAT), 1, meta); // Write stat
	fwrite(&tmp_file_meta, sizeof(FILE_META_TYPE), 1, meta); // Write file_meta_type
	fwrite(&file_stst, sizeof(FILE_STATS_TYPE), 1, meta); // Write file_meta_type
	fwrite(&cloud_related, sizeof(CLOUD_RELATED_DATA), 1, meta);
	for (int32_t i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++) {
		tmp_blockentry_page.block_entries[i].status = ST_CLOUD;
		tmp_blockentry_page.block_entries[i].uploaded = 1;
	}
	tmp_blockentry_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
	for (int32_t page_num = 0 ; page_num < total_page ; page_num++) {
		fwrite(&tmp_blockentry_page, sizeof(BLOCK_ENTRY_PAGE), 1, meta); // Write block page
	}
	fclose(meta);

	fetch_del_backend_meta_path(backend_meta, mock_thread_info->inode);

	/* Begin to test */
	init_delete_control();
	init_objname_buffer(expected_num_objname);
	init_sync_ctl();
	dsync_single_inode(mock_thread_info);

	/* Check answer */
	ASSERT_EQ(expected_num_objname, objname_counter); // Check # of object name.
	qsort(objname_list, expected_num_objname, sizeof(char *),
			dsync_single_inodeTest::objname_cmp);
	uint32_t block;
	for (block = 0 ; block < expected_num_objname - 1 ; block++) {
		char expected_objname[size_objname];
		sprintf(expected_objname, "data_%" PRIu64 "_%d",
				(uint64_t)mock_thread_info->inode, block);
		ASSERT_STREQ(expected_objname, objname_list[block]); // Check all obj was recorded.
	}
	char expected_objname[size_objname];
	sprintf(expected_objname, "meta_%" PRIu64, (uint64_t)mock_thread_info->inode);
	ASSERT_STREQ(expected_objname, objname_list[expected_num_objname - 1]); // Check meta was recorded.

	ASSERT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	ASSERT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	ASSERT_EQ(PTHREAD_CANCELED, res);
	ASSERT_EQ(0, delete_ctl.total_active_delete_threads); // Check all threads finished.
}

TEST_F(dsync_single_inodeTest, DeleteDirectorySuccess)
{
	FILE *meta;
	HCFS_STAT meta_stat;
	void *res;
	DIR_META_TYPE dirmeta = {0};
	CLOUD_RELATED_DATA cloud_related = {0};

	expected_num_objname = 1;
	init_hcfs_stat(&meta_stat);
	cloud_related.upload_seq = 1;
	/* Mock a dir meta file */
	mock_thread_info->inode = INODE__FETCH_TODELETE_PATH_SUCCESS;
	mock_thread_info->this_mode = S_IFDIR;
	mock_thread_info->which_index = 0;
	meta = fopen(TODELETE_PATH, "w+"); // Open mock meta
	fwrite(&meta_stat, sizeof(HCFS_STAT), 1, meta); // Write stat
	fwrite(&dirmeta, sizeof(DIR_META_TYPE), 1, meta);
	fwrite(&cloud_related, sizeof(CLOUD_RELATED_DATA), 1, meta);
	fclose(meta);

	/* Begin to test */
	init_delete_control();
	init_objname_buffer(expected_num_objname);
	init_sync_ctl();
	dsync_single_inode(mock_thread_info);

	/* Check answer */
	EXPECT_EQ(1, objname_counter); // Check # of object name.
	char expected_objname[size_objname];
	sprintf(expected_objname, "meta_%" PRIu64, (uint64_t) mock_thread_info->inode);
	EXPECT_STREQ(expected_objname, objname_list[0]); // Check meta was recorded.

	EXPECT_EQ(0, pthread_cancel(delete_ctl.delete_handler_thread));
	EXPECT_EQ(0, pthread_join(delete_ctl.delete_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads); // Check all threads finished.
}
/*
	End of unittest of dsync_single_inode()
*/

/*
	Unittest of delete_loop()
 */
int32_t inode_cmp(const void *a, const void *b)
{
	return *(int32_t *)a - *(int32_t *)b;
}

TEST(delete_loopTest, DeleteSuccess)
{
	pthread_t thread;
	void *res;
	int32_t size_objname;

	system_config = (SYSTEM_CONF_STRUCT *)
		malloc(sizeof(SYSTEM_CONF_STRUCT));
	memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
	hcfs_system->backend_is_online = TRUE;

	size_objname = 50;
	objname_counter = 0;
	objname_list = (char **)malloc(sizeof(char *) * 100);
	for (int32_t i = 0 ; i < 100 ; i++)
		objname_list[i] = (char *)malloc(sizeof(char)*size_objname);

	test_data.num_inode = 40;
	test_data.to_handle_inode = (int32_t *)malloc(sizeof(int32_t) * test_data.num_inode);
	test_data.tohandle_counter = 0;

	to_verified_data.record_handle_inode = (int32_t *)malloc(sizeof(int32_t) * test_data.num_inode);
	to_verified_data.record_inode_counter = 0;
	sem_init(&(to_verified_data.record_inode_sem), 0, 1);

	for (int32_t i = 0 ; i < test_data.num_inode ; i++)
		test_data.to_handle_inode[i] = (i + 1) * 5; // mock inode
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->head.first_to_delete_inode = test_data.to_handle_inode[0];

	/* Create a thread to run delete_loop() */
	ASSERT_EQ(0, pthread_create(&thread, NULL, delete_loop, NULL));
	sleep(20);

	/* Check answer */
	EXPECT_EQ(test_data.num_inode, to_verified_data.record_inode_counter);
	qsort(to_verified_data.record_handle_inode, to_verified_data.record_inode_counter, sizeof(int32_t), inode_cmp);
	for (int32_t i = 0 ; i < to_verified_data.record_inode_counter ; i++)
		ASSERT_EQ(test_data.to_handle_inode[i], to_verified_data.record_handle_inode[i]);
	EXPECT_EQ(0, dsync_ctl.total_active_dsync_threads);
	EXPECT_EQ(0, delete_ctl.total_active_delete_threads);

	/* Free resource */
	EXPECT_EQ(0, pthread_cancel(thread));
	EXPECT_EQ(0, pthread_join(thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	free(test_data.to_handle_inode);
	free(to_verified_data.record_handle_inode);
	free(sys_super_block);
	free(system_config);
}
/*
	End of unittest of delete_loop()
 */
