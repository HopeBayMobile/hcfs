#include "gtest/gtest.h"
#include <attr/xattr.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include "mock_params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
}


class uploadEnvironment : public ::testing::Environment {
 public:
  char *workpath, *tmppath;

  virtual void SetUp() {
    int shm_key;

    shm_key = shmget(2345, sizeof(SYSTEM_DATA_HEAD), IPC_CREAT | 0666);
    hcfs_system = (SYSTEM_DATA_HEAD *) shmat(shm_key, NULL, 0);

//    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    hcfs_system->system_going_down = FALSE;
    sem_init(&(sync_stat_ctl.stat_op_sem), 0, 1);

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
//    free(hcfs_system);
    unlink("/tmp/testHCFS");
    rmdir(tmppath);
    if (workpath != NULL)
      free(workpath);
    if (tmppath != NULL)
      free(tmppath);
  }
};

::testing::Environment* const upload_env = ::testing::AddGlobalTestEnvironment(new uploadEnvironment);

/* Begin of the test case for the function init_sync_stat_control */

class init_sync_stat_controlTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  virtual void SetUp() {
    METAPATH = (char *) malloc(METAPATHLEN);
    snprintf(METAPATH, METAPATHLEN - 1, "/tmp/testHCFS/metapath");
    if (access(METAPATH, F_OK) < 0)
      mkdir(METAPATH, 0744);

   }

  virtual void TearDown() {
     rmdir(METAPATH);
     free(METAPATH);
   }

 };

TEST_F(init_sync_stat_controlTest, EmptyInit) {
  char tmppath[200];
  int ret;

  snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);
  init_sync_stat_control();

  /* Verify */
  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  /* Cleanup */
  rmdir(tmppath);
 }

TEST_F(init_sync_stat_controlTest, InitCleanup) {
  char tmppath[100];
  char tmppath2[100];
  int ret;

  snprintf(tmppath, 99, "%s/FS_sync", METAPATH);
  snprintf(tmppath2, 99, "%s/FSstat12", tmppath);
  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);

  mkdir(tmppath, 0700);
  ret = access(tmppath, F_OK);
  ASSERT_EQ(0, ret);

  mknod(tmppath2, 0700 | S_IFREG, 0);
  ret = access(tmppath2, F_OK);
  ASSERT_EQ(0, ret);

  init_sync_stat_control();

  /* Verify */
  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  ret = access(tmppath2, F_OK);
  ASSERT_NE(0, ret);


  /* Cleanup */
  rmdir(tmppath);
 }

/* End of the test case for the function update_backend_stat */


/*
	Unittest of init_upload_control()
 */

class InitUploadControlTool {
public:
	static InitUploadControlTool *Tool()
	{
		if (tool == NULL)
			tool = new InitUploadControlTool();
		return tool;
	}
	static void *upload_thread_function(void *ptr)
	{
		usleep(100000);
		return NULL;
	}
	int get_thread_index()
	{
		int found_idle = -1;
		for (int i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
			if (upload_ctl.threads_in_use[i] == FALSE &&
					upload_ctl.threads_created[i] == FALSE) {
				found_idle = i;
				break;
			}
		}
		return found_idle;
	}
	void init_delete_ctl()
	{
		memset(&delete_ctl, 0, sizeof(DELETE_THREAD_CONTROL));
		sem_init(&(delete_ctl.delete_op_sem), 0, 1);
		sem_init(&(delete_ctl.delete_queue_sem), 0, MAX_DELETE_CONCURRENCY);
		memset(&(delete_ctl.threads_in_use), 0,
				sizeof(char) * MAX_DELETE_CONCURRENCY);
		memset(&(delete_ctl.threads_created), 0,
				sizeof(char) * MAX_DELETE_CONCURRENCY);
		delete_ctl.total_active_delete_threads = 0;

	}
	void generate_mock_meta_and_thread(int num_block_entry, char block_status, char is_delete)
	{
		BLOCK_ENTRY_PAGE mock_block_page;
		FILE *mock_file_meta;

		init_sync_control();
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
			mock_block_page.block_entries[i].status = block_status;

		mock_file_meta = fopen(MOCK_META_PATH, "w+");
		if (mock_file_meta == NULL) {
			printf("Failed to generate mock\n");
			return;
		}
		printf("Working on generate mock\n");
		fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
		fclose(mock_file_meta);

		for (int i = 0 ; i < num_block_entry ; i++) {
			ino_t inode = 1;
			FILE *ptr;
			char path[50];
			int index;
#ifdef ARM_32bit_
			sprintf(path, "/tmp/testHCFS/data_%lld_%d",inode, i);
#else
			sprintf(path, "/tmp/testHCFS/data_%d_%d",inode, i);
#endif
			ptr = fopen(path, "w+");
			fclose(ptr);
			setxattr(path, "user.dirty", "T", 1, 0);

			sem_wait(&(upload_ctl.upload_queue_sem));
			sem_wait(&(upload_ctl.upload_op_sem));
			index = get_thread_index();

			upload_ctl.upload_threads[index].inode = inode;
			upload_ctl.upload_threads[index].is_delete= is_delete;
			upload_ctl.upload_threads[index].page_filepos = 0;
			upload_ctl.upload_threads[index].page_entry_index = i;
			upload_ctl.upload_threads[index].blockno = i;
			upload_ctl.upload_threads[index].is_block = TRUE;
			upload_ctl.threads_in_use[index] = TRUE;
			upload_ctl.threads_created[index] = TRUE;
			upload_ctl.total_active_upload_threads++;

			pthread_create(&(upload_ctl.upload_threads_no[index]),
					NULL, InitUploadControlTool::upload_thread_function, NULL); // create thread
			sem_post(&(upload_ctl.upload_op_sem));
		}
	}
private:
	InitUploadControlTool() {}
	~InitUploadControlTool()
	{
		delete tool;
	}
	static InitUploadControlTool *tool;
};


InitUploadControlTool *InitUploadControlTool::tool = NULL;

class init_upload_controlTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
		unlink(MOCK_META_PATH);
	}
};

TEST_F(init_upload_controlTest, DoNothing_JustRun)
{
	void *res;
	char zero_mem[MAX_UPLOAD_CONCURRENCY] = {0};
	/* Run tested function */
	init_upload_control();
	sleep(1);

	/* Check */
	EXPECT_EQ(0, upload_ctl.total_active_upload_threads);
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_in_use), sizeof(zero_mem)));
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_created), sizeof(zero_mem)));

	/* Free resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
}



TEST_F(init_upload_controlTest, AllBlockExist_and_TerminateThreadSuccess)
{
	void *res;
	int num_block_entry = 80;
	BLOCK_ENTRY_PAGE mock_block_page;
	FILE *mock_file_meta;

	/* Run tested function */
	init_upload_control();

	/* Generate mock threads */
	InitUploadControlTool::Tool()->generate_mock_meta_and_thread(num_block_entry,
		ST_LtoC, FALSE);
	sleep(2);

	/* Verify */
	ASSERT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i]) << "thread " << i << " is in use";
	}
	mock_file_meta = fopen(MOCK_META_PATH, "r+");
	fread(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
	for (int i = 0 ; i < num_block_entry ; i++) {
		char xattr_val[5] = "N";
		char path[50];

		ASSERT_EQ(ST_BOTH, mock_block_page.block_entries[i].status); // Check status
		sprintf(path, "/tmp/testHCFS/data_1_%d", i);
		getxattr(path, "user.dirty", xattr_val, 1);
		ASSERT_STREQ("F", xattr_val);
		unlink(path);
	}

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	unlink(MOCK_META_PATH);
}

TEST_F(init_upload_controlTest, BlockIsDeleted_and_TerminateThreadSuccess)
{
	void *res;
	int num_block_entry = 80;
	BLOCK_ENTRY_PAGE mock_block_page;
	FILE *mock_file_meta;
	/* Run tested function */
	init_upload_control();

	/* Generate mock threads */
	InitUploadControlTool::Tool()->generate_mock_meta_and_thread(num_block_entry,
		ST_TODELETE, TRUE);
	sleep(2);

	/* Verify */
	ASSERT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i])<< "thread " << i << " is in use";
	}
	mock_file_meta = fopen(MOCK_META_PATH, "r+");
	fread(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
	for (int i = 0 ; i < num_block_entry ; i++) {
		char path[50];
		ASSERT_EQ(ST_NONE, mock_block_page.block_entries[i].status);
		sprintf(path, "/tmp/testHCFS/mockblock_1_%d", i);
		unlink(path);
	}

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	unlink(MOCK_META_PATH);
}



TEST_F(init_upload_controlTest, MetaIsDeleted_and_TerminateThreadSuccess)
{
	void *res;
	int num_block_entry = 80;
	memset(upload_ctl_todelete_blockno, 0, num_block_entry);

	/* Run tested function */
	init_upload_control();
	InitUploadControlTool::Tool()->init_delete_ctl();

	/* Generate mock threads */
	for (int i = 0 ; i < num_block_entry ; i++) {
		ino_t inode = 1;
		int index;
		//usleep(100000);
		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
		index = InitUploadControlTool::Tool()->get_thread_index();
		upload_ctl.upload_threads[index].inode = inode;
		upload_ctl.upload_threads[index].page_filepos = 0;
		upload_ctl.upload_threads[index].page_entry_index = i;
		upload_ctl.upload_threads[index].blockno = i;
		upload_ctl.upload_threads[index].is_block = TRUE;
		upload_ctl.threads_in_use[index] = TRUE;
		upload_ctl.threads_created[index] = TRUE;
		upload_ctl.total_active_upload_threads++;
		pthread_create(&(upload_ctl.upload_threads_no[index]),
			NULL, InitUploadControlTool::upload_thread_function, NULL); // create thread
		sem_post(&(upload_ctl.upload_op_sem));
	}
	sleep(2);

	/* Verify */
	EXPECT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i]) << "thread " << i << " is in use";
	}
	for (int i = 0 ; i < num_block_entry ; i++)
		ASSERT_EQ(TRUE, upload_ctl_todelete_blockno[i]) << "error in blockno " << i;

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
}

/*
	End of unittest for init_upload_control()
 */

/*
	Unittest of init_sync_control()
 */

void *sync_thread_function(void *ptr)
{
	usleep(100000); // Let thread busy
	return NULL;
}
TEST(init_sync_controlTest, DoNothing_ControlSuccess)
{
	ino_t empty_ino_array[MAX_SYNC_CONCURRENCY] = {0};
	char empty_created_array[MAX_SYNC_CONCURRENCY] = {0};
	void *res;

	/* Run tested function */
	init_sync_control();
	sleep(1);

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(0, memcmp(empty_created_array, &sync_ctl.threads_created, sizeof(empty_created_array)));

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(sync_ctl.sync_handler_thread));
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
}

TEST(init_sync_controlTest, Multithread_ControlSuccess)
{
	void *res;
	int num_threads = 100;
	ino_t empty_ino_array[MAX_SYNC_CONCURRENCY] = {0};
	char empty_created_array[MAX_SYNC_CONCURRENCY] = {0};

	/* Run tested function */
	init_sync_control();

	/* Generate threads */
	for (int i = 0 ; i < num_threads ; i++) {
		int idle_thread = -1;
		sem_wait(&sync_ctl.sync_queue_sem);
		sem_wait(&sync_ctl.sync_op_sem);
		for (int t_idx = 0 ; t_idx < MAX_SYNC_CONCURRENCY ; t_idx++) {
			if ((sync_ctl.threads_in_use[t_idx] == 0)
				&& (sync_ctl.threads_created[t_idx] == FALSE)) {
				idle_thread = t_idx;
				break;
			}
		}
		sync_ctl.threads_in_use[idle_thread] = i+1;
		sync_ctl.threads_created[idle_thread] = TRUE;
		pthread_create(&sync_ctl.inode_sync_thread[idle_thread], NULL,
			sync_thread_function, NULL);
		sync_ctl.total_active_sync_threads++;
		sem_post(&sync_ctl.sync_op_sem);
	}
	sleep(2);

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(0, memcmp(empty_created_array, &sync_ctl.threads_created, sizeof(empty_created_array)));

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(sync_ctl.sync_handler_thread));
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
}
/*
	End of unittest of init_sync_control()
 */

/*
	Unittest of sync_single_inode()
 */

class sync_single_inodeTest : public ::testing::Test {
protected:
	void SetUp()
	{
		max_objname_num = 4000;
		sem_init(&objname_counter_sem, 0, 1);
		objname_counter = 0;
		objname_list = (char **)malloc(sizeof(char *) * max_objname_num);
		for (int i = 0 ; i < max_objname_num ; i++)
			objname_list[i] = (char *)malloc(sizeof(char) * 20);

	}
	void TearDown()
	{
		void *res;
		for (int i = 0 ; i < max_objname_num ; i++)
			free(objname_list[i]);
		free(objname_list);

		unlink(MOCK_META_PATH);

		pthread_cancel(sync_ctl.sync_handler_thread);
		pthread_join(sync_ctl.sync_handler_thread, &res);
	}
	void write_mock_meta_file(char *metapath, int total_page, unsigned char block_status)
	{
		struct stat mock_stat;
		FILE_META_TYPE mock_file_meta;
		BLOCK_ENTRY_PAGE mock_block_page;
		FILE *mock_metaptr;

		mock_metaptr = fopen(metapath, "w+");
		mock_stat.st_size = 1000000; // Let total_blocks = 1000000/1000 = 1000
		mock_stat.st_mode = S_IFREG;
		fwrite(&mock_stat, sizeof(struct stat), 1, mock_metaptr); // Write stat

		fwrite(&mock_file_meta, sizeof(FILE_META_TYPE), 1, mock_metaptr); // Write file meta

		for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
			mock_block_page.block_entries[i].status = block_status;
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int page_num = 0 ; page_num < total_page ; page_num++) {
			fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE),
					1, mock_metaptr); // Linearly write block page
		}
		fclose(mock_metaptr);

	}
	static int objname_cmp(const void *s1, const void *s2)
	{
		char *name1 = *(char **)s1;
		char *name2 = *(char **)s2;
		if (name1[0] == 'm') {
			return 1;
		} else if (name2[0] == 'm') {
			return -1;
		} else {
			char tmp_name[30];
			int inode1, inode2;
			int blocknum1, blocknum2;
			sscanf(name1, "data_%d_%d", &inode1, &blocknum1);
			sscanf(name2, "data_%d_%d", &inode2, &blocknum2);
			return  -blocknum2 + blocknum1;
		}
	}
private:
	int max_objname_num;
};

TEST_F(sync_single_inodeTest, MetaNotExist)
{
	SYNC_THREAD_TYPE mock_thread_type;
	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;

	/* Run tested function */
	sync_single_inode(&mock_thread_type);
}

TEST_F(sync_single_inodeTest, SyncBlockFileSuccess)
{
	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = MOCK_META_PATH;
	int total_page = 3;
	int num_total_blocks = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;

	/* Mock data */
	write_mock_meta_file(metapath, total_page, ST_LDISK);

	system_config.max_block_size = 100;
	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;

	/* Run tested function */
	init_upload_control();
	init_sync_stat_control();
	sync_single_inode(&mock_thread_type);
	sleep(2);

	/* Verify */
	EXPECT_EQ(num_total_blocks, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *), sync_single_inodeTest::objname_cmp);
	for (int blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) { // Check uploaded-object is recorded
		char expected_objname[20];
#ifdef ARM_32bit_
		sprintf(expected_objname, "data_%lld_%d",
				mock_thread_type.inode, blockno);
#else
		sprintf(expected_objname, "data_%d_%d",
				mock_thread_type.inode, blockno);
#endif
		ASSERT_STREQ(expected_objname, objname_list[blockno]) << "blockno = " << blockno;
#ifdef ARM_32bit_
		sprintf(expected_objname, "/tmp/testHCFS/data_%lld_%d",
				mock_thread_type.inode, blockno);
#else
		sprintf(expected_objname, "/tmp/testHCFS/data_%d_%d",
				mock_thread_type.inode, blockno);
#endif
		unlink(expected_objname);
	}
	metaptr = fopen(metapath, "r+");
	fseek(metaptr, sizeof(struct stat), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, metaptr);
	while (!feof(metaptr)) {
		fread(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, metaptr); // Linearly read block meta
		for (int i = 0 ; i < block_page.num_entries ; i++) {
			ASSERT_EQ(ST_BOTH, block_page.block_entries[i].status); // Check status
		}
	}
	fclose(metaptr);
	unlink(metapath);
}

TEST_F(sync_single_inodeTest, Sync_Todelete_BlockFileSuccess)
{

	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = MOCK_META_PATH;
	int total_page = 3;
	int num_total_blocks = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;

	/* Mock data */
	write_mock_meta_file(metapath, total_page, ST_TODELETE);

	system_config.max_block_size = 1000;
	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;

	/* Run tested function */
	init_upload_control();
	sync_single_inode(&mock_thread_type);
	sleep(1);

	/* Verify */
	EXPECT_EQ(num_total_blocks, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *), sync_single_inodeTest::objname_cmp);
	for (int blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) {  // Check deleted-object is recorded
		char expected_objname[20];
#ifdef ARM_32bit_
		sprintf(expected_objname, "data_%lld_%d",
				mock_thread_type.inode, blockno);
#else
		sprintf(expected_objname, "data_%d_%d",
				mock_thread_type.inode, blockno);
#endif
		ASSERT_STREQ(expected_objname, objname_list[blockno]) << "objname = " << objname_list[blockno];
#ifdef ARM_32bit_
		sprintf(expected_objname, "/tmp/testHCFS/data_%lld_%d",
				mock_thread_type.inode, blockno);
#else
		sprintf(expected_objname, "/tmp/testHCFS/data_%d_%d",
				mock_thread_type.inode, blockno);
#endif
		unlink(expected_objname);
	}
	unlink(metapath);
}

/*
	End of unittest of sync_single_inode()
 */

/*
	Unittest of upload_loop()
 */

int inode_cmp(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static void *upload_loop_thread_function(void *ptr)
{
	upload_loop();

	return NULL;
}

class upload_loopTest : public ::testing::Test {
protected:
	FILE *mock_file_meta;
	int max_objname_num;

	void SetUp()
	{
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
		mock_file_meta = fopen(MOCK_META_PATH, "w+");

		objname_counter = 0;
		max_objname_num = 40;
		objname_list = (char **)malloc(sizeof(char *) * max_objname_num);
		for (int i = 0 ; i < max_objname_num ; i++)
			objname_list[i] = (char *)malloc(sizeof(char) * 20);

		sem_init(&objname_counter_sem, 0, 1);
	}

	void TearDown()
	{
		for (int i = 0 ; i < max_objname_num ; i++)
			free(objname_list[i]);
		free(objname_list);

		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(upload_loopTest, UploadLoopWorkSuccess_OnlyTestDirCase)
{
	pthread_t thread_id;
	int shm_key, shm_key2;
	struct stat empty_stat;
	DIR_META_TYPE empty_meta;
	BLOCK_ENTRY_PAGE mock_block_page;

	init_upload_control();
	init_sync_control();

	/* Write something into meta, int the unittest, only test
	   the case that upload dir meta because regfile case has
	   been tested in sync_single_inodeTest(). */
	memset(&empty_stat, 0, sizeof(struct stat));
	memset(&empty_meta, 0, sizeof(DIR_META_TYPE));
	fseek(mock_file_meta, 0, SEEK_SET);
	fwrite(&empty_stat, sizeof(struct stat), 1, mock_file_meta);
	fwrite(&empty_meta, sizeof(DIR_META_TYPE), 1, mock_file_meta);
	/*
	for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
		mock_block_page.block_entries[i].status = ST_LDISK;
	mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
	for (int page_num = 0 ; page_num < total_page ; page_num++) {
		fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE),
				1, mock_metaptr); // Linearly write block page
	}
*/
	fclose(mock_file_meta);

	/* Generate mock data and allocate space to check answer */
	shm_key = shmget(1122, sizeof(LoopTestData), IPC_CREAT | 0666);
	ASSERT_NE(-1, shm_key);
	shm_test_data = (LoopTestData *)shmat(shm_key, NULL, 0);
	ASSERT_NE((void *)-1, shm_test_data);
	shm_test_data->num_inode = max_objname_num; // Test 40 nodes

	shm_key2 = shmget(2222, sizeof(int)*shm_test_data->num_inode, IPC_CREAT | 0666);
	ASSERT_NE(-1, shm_key2);
	shm_test_data->to_handle_inode = (int *)shmat(shm_key2, NULL, 0);
	ASSERT_NE((void *)-1, shm_test_data->to_handle_inode);
	shm_test_data->tohandle_counter = 0;

	for (int i = 0 ; i < shm_test_data->num_inode ; i++)
		// mock inode, which is used as expected answer
		shm_test_data->to_handle_inode[i] = (i + 1) * 5;

	/* Allocate a share space to store actual value */
	shm_key = shmget(5566, sizeof(LoopToVerifiedData), IPC_CREAT | 0666);
	ASSERT_NE(-1, shm_key);
	shm_verified_data = (LoopToVerifiedData *) shmat(shm_key, NULL, 0);
	ASSERT_NE((void *)-1, shm_verified_data);

	shm_key2 = shmget(8899, sizeof(int)*shm_test_data->num_inode, IPC_CREAT | 0666);
	ASSERT_NE(-1, shm_key2);
	shm_verified_data->record_handle_inode = (int *)shmat(shm_key2, NULL, 0);
	ASSERT_NE((void *)-1, shm_verified_data->record_handle_inode);
	shm_verified_data->record_inode_counter = 0;
	sem_init(&(shm_verified_data->record_inode_sem), 0, 1);

	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT; // Let system upload

	/* Set first_dirty_inode to be uploaded */
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->head.first_dirty_inode = shm_test_data->to_handle_inode[0];

	/* Create a thread to run upload_loop() */
	pthread_create(&thread_id, NULL, upload_loop_thread_function, NULL);

	sleep(3);
	hcfs_system->system_going_down = TRUE; // Let upload_loop() exit
	pthread_join(thread_id, NULL);

	/* Verify */
	EXPECT_EQ(shm_test_data->num_inode, shm_verified_data->record_inode_counter);
	qsort(shm_verified_data->record_handle_inode, shm_verified_data->record_inode_counter,
		sizeof(int), inode_cmp);
	for (int i = 0 ; i < shm_test_data->num_inode ; i++) {
		EXPECT_EQ(shm_test_data->to_handle_inode[i], shm_verified_data->record_handle_inode[i]);
	}
}

/*
	End of unittest of upload_loop()
 */

/* Begin of the test case for the function update_backend_stat */

class update_backend_statTest : public ::testing::Test {
 protected:
  int count;
  char tmpmgrpath[100];
  virtual void SetUp() {
    METAPATH = (char *) malloc(METAPATHLEN);
    snprintf(METAPATH, METAPATHLEN - 1, "/tmp/testHCFS/metapath");
    if (access(METAPATH, F_OK) < 0)
      mkdir(METAPATH, 0744);

    no_backend_stat = TRUE;
   }

  virtual void TearDown() {
     rmdir(METAPATH);
     free(METAPATH);
   }

 };

TEST_F(update_backend_statTest, EmptyStat) {
  char tmppath[200];
  char tmppath2[200];
  int ret;
  FILE *fptr;
  long long sys_size, num_ino;

  snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
  snprintf(tmppath2, 199, "%s/FSstat14", tmppath);

  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);
  init_sync_stat_control();

  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  ret = access(tmppath2, F_OK);
  ASSERT_NE(0, ret);

  ret = update_backend_stat(14, 1024768, 101);

  EXPECT_EQ(0, ret);
  ret = access(tmppath2, F_OK);
  ASSERT_EQ(0, ret);

  /* Verify content */

  fptr = fopen(tmppath2, "r");
  fread(&sys_size, sizeof(long long), 1, fptr);
  fread(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  EXPECT_EQ(1024768, sys_size);
  EXPECT_EQ(101, num_ino);

  /* Cleanup */
  unlink(tmppath2);
  rmdir(tmppath);
 }
TEST_F(update_backend_statTest, UpdateExistingStat) {
  char tmppath[200];
  char tmppath2[200];
  int ret;
  FILE *fptr;
  long long sys_size, num_ino;

  snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
  snprintf(tmppath2, 199, "%s/FSstat14", tmppath);

  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);
  init_sync_stat_control();

  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  ret = access(tmppath2, F_OK);
  ASSERT_NE(0, ret);

  sys_size = 7687483;
  num_ino = 34334;
  fptr = fopen(tmppath2, "w");
  fwrite(&sys_size, sizeof(long long), 1, fptr);
  fwrite(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  ret = update_backend_stat(14, 1024768, -101);

  EXPECT_EQ(0, ret);
  ret = access(tmppath2, F_OK);
  ASSERT_EQ(0, ret);

  /* Verify content */

  fptr = fopen(tmppath2, "r");
  fread(&sys_size, sizeof(long long), 1, fptr);
  fread(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  EXPECT_EQ(1024768 + 7687483, sys_size);
  EXPECT_EQ(34334 - 101, num_ino);

  /* Cleanup */
  unlink(tmppath2);
  rmdir(tmppath);
 }

TEST_F(update_backend_statTest, UpdateLessThanZero) {
  char tmppath[200];
  char tmppath2[200];
  int ret;
  FILE *fptr;
  long long sys_size, num_ino;

  snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
  snprintf(tmppath2, 199, "%s/FSstat14", tmppath);

  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);
  init_sync_stat_control();

  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  ret = access(tmppath2, F_OK);
  ASSERT_NE(0, ret);

  sys_size = 7687;
  num_ino = 34;
  fptr = fopen(tmppath2, "w");
  fwrite(&sys_size, sizeof(long long), 1, fptr);
  fwrite(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  ret = update_backend_stat(14, -1024768, -101);

  EXPECT_EQ(0, ret);
  ret = access(tmppath2, F_OK);
  ASSERT_EQ(0, ret);

  /* Verify content */

  fptr = fopen(tmppath2, "r");
  fread(&sys_size, sizeof(long long), 1, fptr);
  fread(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  EXPECT_EQ(0, sys_size);
  EXPECT_EQ(0, num_ino);

  /* Cleanup */
  unlink(tmppath2);
  rmdir(tmppath);
 }

TEST_F(update_backend_statTest, DownloadUpdate) {
  char tmppath[200];
  char tmppath2[200];
  int ret;
  FILE *fptr;
  long long sys_size, num_ino;

  snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
  snprintf(tmppath2, 199, "%s/FSstat14", tmppath);

  no_backend_stat = FALSE;
  ret = access(tmppath, F_OK);
  ASSERT_NE(0, ret);
  init_sync_stat_control();

  ret = access(tmppath, F_OK);
  EXPECT_EQ(0, ret);

  ret = access(tmppath2, F_OK);
  ASSERT_NE(0, ret);

  ret = update_backend_stat(14, 1024768, -101);

  EXPECT_EQ(0, ret);
  ret = access(tmppath2, F_OK);
  ASSERT_EQ(0, ret);

  /* Verify content */

  fptr = fopen(tmppath2, "r");
  fread(&sys_size, sizeof(long long), 1, fptr);
  fread(&num_ino, sizeof(long long), 1, fptr);
  fclose(fptr);

  EXPECT_EQ(1024768 + 7687483, sys_size);
  EXPECT_EQ(34334 - 101, num_ino);

  /* Cleanup */
  unlink(tmppath2);
  rmdir(tmppath);
 }

/* End of the test case for the function update_backend_stat */

