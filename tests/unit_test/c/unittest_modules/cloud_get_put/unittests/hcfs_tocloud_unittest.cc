#include "gtest/gtest.h"
#include "mock_params.h"
#include <attr/xattr.h>
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
}

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
			sprintf(path, "/tmp/mockblock_%d_%d",inode, i);
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
		sprintf(path, "/tmp/mockblock_%d_%d",1, i);
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
	ASSERT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i])<< "thread " << i << " is in use";
	}
}

