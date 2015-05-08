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

		mock_block_page.next_page = 0;
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
			mock_block_page.block_entries[i].status = block_status;
		mock_file_meta = fopen("/tmp/mock_file_meta", "w+");
		fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
		fclose(mock_file_meta);

		for (int i = 0 ; i < num_block_entry ; i++) {
			ino_t inode = 1;
			FILE *ptr;
			char path[50];
			int index;
			sprintf(path, "/tmp/data_%d_%d",inode, i);
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

TEST(init_upload_controlTest, DoNothing_JustRun)
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



TEST(init_upload_controlTest, AllBlockExist_and_TerminateThreadSuccess)
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
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i])<< "thread " << i << " is in use";
	}
	mock_file_meta = fopen("/tmp/mock_file_meta", "r+");
	fread(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);	
	for (int i = 0 ; i < num_block_entry ; i++) {
		char xattr_val[5] = "N";
		char path[50];
		
		ASSERT_EQ(ST_BOTH, mock_block_page.block_entries[i].status);
		sprintf(path, "/tmp/data_%d_%d",1, i);
		getxattr(path, "user.dirty", xattr_val, 1);
		ASSERT_STREQ("F", xattr_val);
		unlink(path);
	}

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	unlink("/tmp/mock_file_meta");
}

TEST(init_upload_controlTest, BlockIsDeleted_and_TerminateThreadSuccess)
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
	mock_file_meta = fopen("/tmp/mock_file_meta", "r+");
	fread(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);	
	for (int i = 0 ; i < num_block_entry ; i++) {
		char path[50];	
		ASSERT_EQ(ST_NONE, mock_block_page.block_entries[i].status);
		sprintf(path, "/tmp/mockblock_%d_%d",1, i);
		unlink(path);
	}

	/* Reclaim resource */
	EXPECT_EQ(0, pthread_cancel(upload_ctl.upload_handler_thread));
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	EXPECT_EQ(PTHREAD_CANCELED, res);
	unlink("/tmp/mock_file_meta");
}



TEST(init_upload_controlTest, MetaIsDeleted_and_TerminateThreadSuccess)
{
	void *res;
	int num_block_entry = 10;
	memset(upload_ctl_todelete_blockno, 0, num_block_entry);
	
	/* Run tested function */
	init_upload_control();
	InitUploadControlTool::Tool()->init_delete_ctl();
		
	/* Generate mock threads */
	for (int i = 0 ; i < num_block_entry ; i++) {
		ino_t inode = 1;
		int index;
		
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
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i])<< "thread " << i << " is in use";
	}
	for (int i = 0 ; i < num_block_entry ; i++)
		ASSERT_EQ(TRUE, upload_ctl_todelete_blockno[i]);

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
	usleep(100000);
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
			
		pthread_cancel(sync_ctl.sync_handler_thread);
		pthread_join(sync_ctl.sync_handler_thread, &res);
	}
	void write_mock_meta_file(char *metapath, int total_page, char block_status)
	{
		struct stat mock_stat;
		FILE_META_TYPE mock_file_meta;
		BLOCK_ENTRY_PAGE mock_block_page;
		FILE *mock_metaptr;

		mock_metaptr = fopen(metapath, "w+");
		mock_stat.st_size = 1000000;
		mock_stat.st_mode = S_IFREG;
		fwrite(&mock_stat, sizeof(struct stat), 1, mock_metaptr); // Write stat
		
		mock_file_meta.next_block_page = sizeof(struct stat) + 
			sizeof(FILE_META_TYPE);
		fwrite(&mock_file_meta, sizeof(FILE_META_TYPE), 1, mock_metaptr); // Write file meta
		
		for (int i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
			mock_block_page.block_entries[i].status = block_status;
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int page_num = 0 ; page_num < total_page ; page_num++) {
			if (page_num == total_page - 1)
				mock_block_page.next_page = 0; // Last page
			else
				mock_block_page.next_page = sizeof(struct stat) + 
					sizeof(FILE_META_TYPE) + (page_num + 1) * 
					sizeof(BLOCK_ENTRY_PAGE); 
			fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE),
					1, mock_metaptr); // Write block page
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
	char metapath[] = "/tmp/mock_file_meta";
	int total_page = 3;
	int num_total_blocks = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;

	/* Mock data */
	write_mock_meta_file(metapath, total_page, ST_LDISK);
		
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
	for (int blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) {
		char expected_objname[20];
		sprintf(expected_objname, "data_%d_%d", mock_thread_type.inode, blockno);
		ASSERT_STREQ(expected_objname, objname_list[blockno]) << "blockno = " << blockno;
		sprintf(expected_objname, "/tmp/data_%d_%d", mock_thread_type.inode, blockno);
		unlink(expected_objname);
	}
	metaptr = fopen(metapath, "r+");
	fseek(metaptr, sizeof(struct stat), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, metaptr);
	int next_page = filemeta.next_block_page;
	while (next_page != 0) {
		fread(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, metaptr);
		for (int i = 0 ; i < block_page.num_entries ; i++)
			ASSERT_EQ(ST_BOTH, block_page.block_entries[i].status);
		next_page = block_page.next_page;
	}
	fclose(metaptr);
	unlink(metapath);
}

TEST_F(sync_single_inodeTest, Sync_Todelete_BlockFileSuccess)
{
	
	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = "/tmp/mock_file_meta";
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
	for (int blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) {
		char expected_objname[20];
		sprintf(expected_objname, "data_%d_%d", mock_thread_type.inode, blockno);
		ASSERT_STREQ(expected_objname, objname_list[blockno]) << "blockno = " << blockno;
		sprintf(expected_objname, "/tmp/data_%d_%d", mock_thread_type.inode, blockno);
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

TEST(upload_loopTest, UploadLoopWorkSuccess)
{
	pid_t pid;
	int shm_key, shm_key2;

	shm_key = shmget(1122, sizeof(LoopTestData), IPC_CREAT | 0666);
	shm_test_data = (LoopTestData *)shmat(shm_key, NULL, 0);
	shm_key2 = shmget(1244, sizeof(int)*shm_test_data->num_inode, IPC_CREAT | 0666);
	shm_test_data->to_handle_inode = (int *)shmat(shm_key2, NULL, 0);
	shm_test_data->num_inode = 40; // Test 40 nodes	
	shm_test_data->tohandle_counter = 0;
	for (int i = 0 ; i < shm_test_data->num_inode ; i++)
		shm_test_data->to_handle_inode[i] = (i + 1) * 5; // mock inode

	shm_key = shmget(5566, sizeof(LoopToVerifiedData), IPC_CREAT | 0666);
	shm_verified_data = (LoopToVerifiedData *) shmat(shm_key, NULL, 0);
	shm_key2 = shmget(1144, sizeof(int)*shm_test_data->num_inode, IPC_CREAT | 0666);
	shm_verified_data->record_handle_inode = (int *)shmat(shm_key2, NULL, 0);
	shm_verified_data->record_inode_counter = 0;
	sem_init(&(shm_verified_data->record_inode_sem), 0, 1);
	
	hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT;

	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->head.first_dirty_inode = shm_test_data->to_handle_inode[0];

	/* Create a process to run upload_loop() */
	pid = fork();
	if (pid == 0) {
		upload_loop();
		exit(0);
	}
	sleep(3);
	kill(pid, SIGKILL);

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
