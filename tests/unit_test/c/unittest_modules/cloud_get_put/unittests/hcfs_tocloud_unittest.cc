#define __STDC_FORMAT_MACROS
#include "gtest/gtest.h"
#include <stddef.h>
#include <attr/xattr.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ftw.h>
#include "mock_params.h"
extern "C" {
#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "super_block.h"
#include "atomic_tocloud.h"
#include "mount_manager.h"
#include "do_restoration.h"
}

#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(BOOL, need_recover_sb);
FAKE_VOID_FUNC(start_sb_recovery);

extern SYSTEM_CONF_STRUCT *system_config;

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

class uploadEnvironment : public ::testing::Environment
{
public:
	char *workpath, *tmppath;

	virtual void SetUp() {
		int32_t shm_key;
#if 0
		shm_key = shmget(5678, sizeof(SYSTEM_DATA_HEAD),
		                 IPC_CREAT | 0666);
		if (shm_key < 0) {
			int32_t errcode;
			errcode = errno;
			printf("Error %d %s\n", errcode, strerror(errcode));
			return;
		}
#endif
		hcfs_system = (SYSTEM_DATA_HEAD *)
		              calloc(1, sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(sync_stat_ctl.stat_op_sem), 0, 1);
		sem_init(&(hcfs_system->access_sem), 0, 1);
		sem_init(&backup_pkg_sem, 0, 1);

		workpath = NULL;
		tmppath = NULL;
		no_backend_stat = TRUE;
		if (access("/tmp/testHCFS", F_OK) != 0) {
			workpath = get_current_dir_name();
			tmppath = (char *)malloc(strlen(workpath) + 20);
			snprintf(tmppath, strlen(workpath) + 20, "%s/tmpdir", workpath);
			if (access(tmppath, F_OK) != 0)
				mkdir(tmppath, 0700);
			symlink(tmppath, "/tmp/testHCFS");
		}
		system_config = (SYSTEM_CONF_STRUCT *)
		                calloc(1, sizeof(SYSTEM_CONF_STRUCT));
		system_config->max_block_size = 1000;
		if (!system_config)
			printf("Fail to allocate memory\n");

		METAPATH = (char *) malloc(METAPATHLEN);
		snprintf(METAPATH, METAPATHLEN - 1, "/tmp/testHCFS/metapath");
		if (access(METAPATH, F_OK) < 0)
			mkdir(METAPATH, 0744);

	}

	virtual void TearDown() {
		//    free(hcfs_system);
		nftw(METAPATH, do_delete, 20, FTW_DEPTH);
		unlink("/tmp/testHCFS");
		if (tmppath != NULL)
			nftw(tmppath, do_delete, 20, FTW_DEPTH);
		if (workpath != NULL)
			free(workpath);
		if (tmppath != NULL)
			free(tmppath);
		free(METAPATH);
		free(system_config);
	}
};

::testing::Environment* const upload_env =
	::testing::AddGlobalTestEnvironment(new uploadEnvironment);

/* Begin of the test case for the function init_sync_stat_control */

class init_sync_stat_controlTest : public ::testing::Test {
protected:
	int32_t count;
	char tmpmgrpath[100];
	char tmppath[200];
	virtual void SetUp() {
		no_backend_stat = TRUE;
		snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
	}

	virtual void TearDown() {
		char tmppath[200];
		snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
	}
};

TEST_F(init_sync_stat_controlTest, EmptyInit)
{
	char tmppath[200];
	int32_t ret;

	snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
	ret = access(tmppath, F_OK);
	ASSERT_NE(0, ret);
	init_sync_stat_control();

	/* Verify */
	ret = access(tmppath, F_OK);
	EXPECT_EQ(0, ret);

	/* Cleanup */
	nftw(tmppath, do_delete, 20, FTW_DEPTH);
}

TEST_F(init_sync_stat_controlTest, InitCleanup)
{
	char tmppath[100];
	char tmppath2[100];
	int32_t ret;

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
	ASSERT_EQ(0, ret);


	/* Cleanup */
	unlink(tmppath2);
	nftw(tmppath, do_delete, 20, FTW_DEPTH);
	ret = access(tmppath, F_OK);
	EXPECT_NE(0, ret);

	ret = access(tmppath2, F_OK);
	ASSERT_NE(0, ret);
}

/* End of the test case for the function init_sync_stat_control */


// Unittest of init_upload_control()

class InitUploadControlTool {
public:
	int fd;
	char progress_path[100];

	static InitUploadControlTool *Tool() {
		if (tool == NULL)
			tool = new InitUploadControlTool();
		return tool;
	}

	static void *upload_thread_function(void *ptr) {
		UPLOAD_THREAD_TYPE *ptr1;

		ptr1 = (UPLOAD_THREAD_TYPE *) ptr;
		usleep(100000);
		upload_ctl.threads_finished[ptr1->which_index] = TRUE;
		return NULL;
	}

	int32_t get_thread_index() {
		int32_t found_idle = -1;
		for (int32_t i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
			if (upload_ctl.threads_in_use[i] == FALSE &&
			    upload_ctl.threads_created[i] == FALSE) {
				found_idle = i;
				break;
			}
		}
		return found_idle;
	}

	void init_delete_ctl() {
		memset(&delete_ctl, 0, sizeof(DELETE_THREAD_CONTROL));
		sem_init(&(delete_ctl.delete_op_sem), 0, 1);
		sem_init(&(delete_ctl.delete_queue_sem), 0, MAX_DELETE_CONCURRENCY);
		memset(&(delete_ctl.threads_in_use), 0,
		       sizeof(char) * MAX_DELETE_CONCURRENCY);
		memset(&(delete_ctl.threads_created), 0,
		       sizeof(char) * MAX_DELETE_CONCURRENCY);
		delete_ctl.total_active_delete_threads = 0;

	}

	void generate_mock_meta_and_thread(int32_t num_block_entry,
	                                   char block_status,
	                                   char is_delete) {
		BLOCK_ENTRY_PAGE mock_block_page;
		FILE *mock_file_meta;

		init_sync_control();
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int32_t i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++)
			mock_block_page.block_entries[i].status = block_status;

		mock_file_meta = fopen(MOCK_META_PATH, "w+");
		if (mock_file_meta == NULL) {
			printf("Failed to generate mock\n");
			return;
		}
		printf("Working on generate mock\n");
		fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
		fclose(mock_file_meta);

		strcpy(progress_path, "/tmp/mock_progress_file");
		fd = open(progress_path, O_CREAT | O_RDWR);

		for (int32_t i = 0 ; i < num_block_entry ; i++) {
			ino_t inode = 1;
			FILE *ptr;
			char path[50];
			int32_t index;
			sprintf(path, "/tmp/testHCFS/data_%" PRIu64 "_%d",
			        (uint64_t)inode, i);
			ptr = fopen(path, "w+");
			fclose(ptr);
			setxattr(path, "user.dirty", "T", 1, 0);

			sem_wait(&(upload_ctl.upload_queue_sem));
			sem_wait(&(upload_ctl.upload_op_sem));
			index = get_thread_index();

			upload_ctl.upload_threads[index].inode = inode;
			upload_ctl.upload_threads[index].which_index = index;
			upload_ctl.upload_threads[index].is_delete = is_delete;
			upload_ctl.upload_threads[index].page_filepos = 0;
			upload_ctl.upload_threads[index].page_entry_index = i;
			upload_ctl.upload_threads[index].blockno = i;
			upload_ctl.upload_threads[index].is_block = TRUE;
			upload_ctl.upload_threads[index].seq = i;
			upload_ctl.upload_threads[index].progress_fd = fd;
			upload_ctl.upload_threads[index].backend_delete_type = FALSE;
			upload_ctl.threads_in_use[index] = TRUE;
			upload_ctl.threads_created[index] = TRUE;
			upload_ctl.threads_finished[index] = TRUE;
			upload_ctl.total_active_upload_threads++;

			pthread_create(&(upload_ctl.upload_threads_no[index]),
			               NULL,
			               InitUploadControlTool::upload_thread_function,
			               (void *) & (upload_ctl.upload_threads[index]));
			// create thread
			sem_post(&(upload_ctl.upload_op_sem));
		}
	}

private:
	InitUploadControlTool() {
	}

	static InitUploadControlTool *tool;

public:
	static void Destruct() {
		delete tool;
		tool = NULL;
	}
};


InitUploadControlTool *InitUploadControlTool::tool = NULL;

class init_upload_controlTest : public ::testing::Test {
protected:
	void SetUp() {
		no_backend_stat = TRUE;
		init_sync_stat_control();
		init_sync_control(); /* Add this init so that upload thread will not hang up */
		sem_init(&upload_ctl.upload_queue_sem, 0, 1);
		sem_init(&upload_ctl.upload_op_sem, 0, 1);
		hcfs_system->system_going_down = FALSE;
	}

	void TearDown() {
		char tmppath[200];
		char tmppath2[200];

		/* Join the sync_control thread */
		hcfs_system->system_going_down = TRUE;
		pthread_join(sync_ctl.sync_handler_thread, NULL);

		snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
		snprintf(tmppath2, 199, "%s/FSstat10", tmppath);
		unlink(tmppath2);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
		close(InitUploadControlTool::Tool()->fd);
		unlink(InitUploadControlTool::Tool()->progress_path);

		InitUploadControlTool::Destruct();

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
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_in_use),
	                    sizeof(zero_mem)));
	EXPECT_EQ(0, memcmp(zero_mem, &(upload_ctl.threads_created),
	                    sizeof(zero_mem)));

	/* Free resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
}

TEST_F(init_upload_controlTest, AllBlockExist_and_TerminateThreadSuccess)
{
	void *res;
	int32_t num_block_entry = 50;
	BLOCK_ENTRY_PAGE mock_block_page;
	FILE *mock_file_meta;
	int32_t semval;

	/* Run tested function */
	init_upload_control();

	/* Generate mock threads */
	InitUploadControlTool::Tool()->generate_mock_meta_and_thread(num_block_entry,
	        ST_LtoC, FALSE);
	sleep(2);

	/* Verify */
	ASSERT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int32_t i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i]) << "thread " << i << " is in use";
	}

	/* Make sure that block upload will trigger cache replacement */
	mock_file_meta = fopen(MOCK_META_PATH, "r+");
	fread(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, mock_file_meta);
	for (int32_t i = 0 ; i < num_block_entry ; i++) {
		char xattr_val[5] = "N";
		char path[50];
		BLOCK_UPLOADING_STATUS block_entry;

		ASSERT_EQ(ST_LtoC, mock_block_page.block_entries[i].status); // Check status
		sprintf(path, "/tmp/testHCFS/data_1_%d", i);
		getxattr(path, "user.dirty", xattr_val, 1);
		ASSERT_STREQ("T", xattr_val);

		pread(InitUploadControlTool::Tool()->fd, &block_entry,
		      sizeof(BLOCK_UPLOADING_STATUS),
		      i * sizeof(BLOCK_UPLOADING_STATUS));
		ASSERT_EQ(TRUE, block_entry.finish_uploading);
		ASSERT_EQ(i, block_entry.to_upload_seq);
		unlink(path);
	}

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
	unlink(MOCK_META_PATH);
}

TEST_F(init_upload_controlTest, MetaIsDeleted_and_TerminateThreadSuccess)
{
	void *res;
	int32_t num_block_entry = 80;
	memset(upload_ctl_todelete_blockno, 0, num_block_entry);

	/* Run tested function */
	init_upload_control();
	InitUploadControlTool::Tool()->init_delete_ctl();

	/* Generate mock threads */
	for (int32_t i = 0 ; i < num_block_entry ; i++) {
		ino_t inode = 1;
		int32_t index;
		//usleep(100000);
		printf("Generate mock threads %d\n", i);
		sem_wait(&(upload_ctl.upload_queue_sem));
		sem_wait(&(upload_ctl.upload_op_sem));
		index = InitUploadControlTool::Tool()->get_thread_index();
		upload_ctl.upload_threads[index].inode = inode;
		upload_ctl.upload_threads[index].page_filepos = 0;
		upload_ctl.upload_threads[index].page_entry_index = i;
		upload_ctl.upload_threads[index].blockno = i;
		upload_ctl.upload_threads[index].is_block = TRUE;
		upload_ctl.upload_threads[index].which_index = index;
		upload_ctl.threads_in_use[index] = TRUE;
		upload_ctl.threads_created[index] = TRUE;
		upload_ctl.threads_finished[index] = TRUE;
		upload_ctl.total_active_upload_threads++;
		pthread_create(&(upload_ctl.upload_threads_no[index]),
		               NULL, InitUploadControlTool::upload_thread_function,
		               (void *) & (upload_ctl.upload_threads[index]));
		// create thread
		sem_post(&(upload_ctl.upload_op_sem));
	}
	sleep(2);

	/* Verify */
	EXPECT_EQ(0, upload_ctl.total_active_upload_threads);
	for (int32_t i = 0 ; i < MAX_UPLOAD_CONCURRENCY ; i++) {
		ASSERT_EQ(FALSE, upload_ctl.threads_in_use[i]) << "thread " << i << " is in use";
		ASSERT_EQ(FALSE, upload_ctl.threads_created[i]) << "thread " << i << " is in use";
	}

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(upload_ctl.upload_handler_thread, &res));
}

/*
	End of unittest for init_upload_control()
 */

/*
	Unittest of init_sync_control()
 */

class init_sync_controlTest : public ::testing::Test {
protected:
	int num_inode;
	ino_t empty_ino_array[MAX_SYNC_CONCURRENCY];
	char empty_created_array[MAX_SYNC_CONCURRENCY];
	char metapath[100];

	void SetUp() {
		mkdir("/tmp/testHCFS", 0700);
		mkdir("mock_meta_folder", 0700);
		num_inode = 100;
		memset(empty_ino_array, 0, sizeof(ino_t) * MAX_SYNC_CONCURRENCY);
		memset(empty_created_array, 0, sizeof(char) * MAX_SYNC_CONCURRENCY);
		empty_created_array[MAX_SYNC_CONCURRENCY] = {0};

		shm_verified_data =
		    (LoopToVerifiedData *)malloc(sizeof(LoopToVerifiedData));
		sem_init(&shm_verified_data->record_inode_sem, 0, 1);
		shm_verified_data->record_handle_inode =
		    (int32_t *)malloc(sizeof(int32_t) * 1000);
		shm_verified_data->record_inode_counter = 0;

		sys_super_block = (SUPER_BLOCK_CONTROL *)
		                  malloc(sizeof(SUPER_BLOCK_CONTROL));
		sys_super_block->head.num_dirty = num_inode;
		hcfs_system->system_going_down = FALSE;
	}

	void TearDown() {
		hcfs_system->system_going_down = TRUE;
		free(shm_verified_data->record_handle_inode);
		sem_destroy(&shm_verified_data->record_inode_sem);
		free(shm_verified_data);

		free(sys_super_block);

		if (!access(metapath, F_OK))
			unlink(metapath);
		nftw("/tmp/testHCFS", do_delete, 20, FTW_DEPTH);
		nftw("mock_meta_folder", do_delete, 20, FTW_DEPTH);
	}
};

void *sync_thread_function(void *ptr)
{
	SYNC_THREAD_TYPE *ptr1;

	ptr1 = (SYNC_THREAD_TYPE *) ptr;
	usleep(100000); // Let thread busy
	sync_ctl.threads_finished[ptr1->which_index] = TRUE;
	return NULL;
}

TEST_F(init_sync_controlTest, DoNothing_ControlSuccess)
{
	void *res;
	IMMEDIATELY_RETRY_LIST retry_list;

	retry_list.num_retry = 0;
	retry_list.list_size = MAX_SYNC_CONCURRENCY;
	retry_list.retry_inode = (ino_t *)
	                         calloc(sizeof(ino_t) * MAX_SYNC_CONCURRENCY, 1);

	/* Run tested function */
	init_sync_control();
	sleep(1);

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use,
	                    sizeof(empty_ino_array)));
	EXPECT_EQ(0, memcmp(empty_created_array, &sync_ctl.threads_created,
	                    sizeof(empty_created_array)));
	EXPECT_EQ(0, sync_ctl.retry_list.num_retry);
	EXPECT_EQ(MAX_SYNC_CONCURRENCY, sync_ctl.retry_list.list_size);
	EXPECT_EQ(0, memcmp(retry_list.retry_inode,
	                    sync_ctl.retry_list.retry_inode,
	                    sizeof(ino_t) * MAX_SYNC_CONCURRENCY));

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));

	free(retry_list.retry_inode);
}

TEST_F(init_sync_controlTest, Multithread_ControlSuccess)
{
	void *res;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
	int32_t num_threads = 8;

	/* Run tested function */
	init_sync_control();

	/* Generate threads */
	for (int32_t i = 0 ; i < num_threads ; i++) {
		int32_t idle_thread = -1;
		sem_wait(&sync_ctl.sync_queue_sem);
		sem_wait(&sync_ctl.sync_op_sem);
		for (int32_t t_idx = 0 ; t_idx < MAX_SYNC_CONCURRENCY ; t_idx++) {
			if ((sync_ctl.threads_in_use[t_idx] == 0)
			    && (sync_ctl.threads_created[t_idx] == FALSE)) {
				idle_thread = t_idx;
				break;
			}
		}
		sync_ctl.threads_in_use[idle_thread] = i + 1;
		sync_ctl.threads_created[idle_thread] = TRUE;
		sync_ctl.threads_finished[idle_thread] = FALSE;
		sync_ctl.is_revert[idle_thread] = FALSE;
		sync_ctl.continue_nexttime[idle_thread] = FALSE;
		sync_ctl.threads_error[idle_thread] = FALSE;

		sync_threads[idle_thread].which_index = idle_thread;
		pthread_create(&sync_ctl.inode_sync_thread[idle_thread], NULL,
		               sync_thread_function,
		               (void *) & (sync_threads[idle_thread]));
		sync_ctl.total_active_sync_threads++;
		sem_post(&sync_ctl.sync_op_sem);
	}
	sleep(2);

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(0, memcmp(empty_created_array, &sync_ctl.threads_created, sizeof(empty_created_array)));

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));
}

TEST_F(init_sync_controlTest, LocalMetaNotExist_DoNotUpdateSB)
{
	int fd;
	void *res;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];

	/* Run tested function */
	init_sync_control();

	fd = open("/tmp/mock_progress_file", O_CREAT | O_RDWR);

	sem_wait(&sync_ctl.sync_queue_sem);
	sem_wait(&sync_ctl.sync_op_sem);
	sync_ctl.threads_in_use[0] = 2;
	sync_ctl.threads_created[0] = TRUE;
	sync_ctl.threads_finished[0] = FALSE;
	sync_ctl.is_revert[0] = FALSE;
	sync_ctl.continue_nexttime[0] = FALSE;
	sync_ctl.threads_error[0] = TRUE; /* Set upload error */
	sync_ctl.progress_fd[0] = fd;

	sync_threads[0].which_index = 0;
	pthread_create(&sync_ctl.inode_sync_thread[0], NULL,
	               sync_thread_function,
	               (void *) & (sync_threads[0]));
	sync_ctl.total_active_sync_threads++;
	sem_post(&sync_ctl.sync_op_sem);
	sleep(1);

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(0, shm_verified_data->record_inode_counter);
	EXPECT_EQ(-1, access("/tmp/mock_progress_file", F_OK));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(init_sync_controlTest, SyncFail_ContinueNextTime)
{
	FILE *fptr;
	int fd;
	void *res;
	FILE_META_TYPE filemeta;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
	HCFS_STAT tmpstat;
	char local_path[200];

	/* Run tested function */
	init_sync_control();

	/* The same as fetch_meta_path */
	fetch_toupload_meta_path(metapath, 2);
	strcpy(local_path, MOCK_META_PATH);
	mknod(local_path, 0700, 0);
	mknod(metapath, 0700, 0);
	tmpstat.mode = S_IFREG;
	tmpstat.size = 0;
	fptr = fopen(metapath, "r+");
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fclose(fptr);

	fd = open("/tmp/mock_progress_file", O_CREAT | O_RDWR, 0600);

	sem_wait(&sync_ctl.sync_queue_sem);
	sem_wait(&sync_ctl.sync_op_sem);
	sync_ctl.threads_in_use[0] = 2;
	sync_ctl.threads_created[0] = TRUE;
	sync_ctl.threads_finished[0] = FALSE;
	sync_ctl.is_revert[0] = FALSE;
	sync_ctl.continue_nexttime[0] = TRUE; /* Set continue_nexttime */
	sync_ctl.threads_error[0] = TRUE; /* Set upload error */
	sync_ctl.progress_fd[0] = fd;

	sync_threads[0].which_index = 0;
	pthread_create(&sync_ctl.inode_sync_thread[0], NULL,
	               sync_thread_function,
	               (void *) & (sync_threads[0]));
	sync_ctl.total_active_sync_threads++;
	sem_post(&sync_ctl.sync_op_sem);
	sleep(1);

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(0, shm_verified_data->record_inode_counter);
	EXPECT_EQ(0, access("/tmp/mock_progress_file", F_OK));
	EXPECT_EQ(0, access(metapath, F_OK));

	unlink("/tmp/mock_progress_file");
	unlink(metapath);
	unlink(local_path);
}

TEST_F(init_sync_controlTest, SyncSuccess)
{
	int fd;
	void *res;
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];

	/* Run tested function */
	init_sync_control();

	/* The same as fetch_meta_path */
	sprintf(metapath, "/tmp/testHCFS/mock_file_meta");
	mknod(metapath, 0700, 0);
	fd = open("/tmp/mock_progress_file", O_CREAT | O_RDWR);

	sem_wait(&sync_ctl.sync_queue_sem);
	sem_wait(&sync_ctl.sync_op_sem);
	sync_ctl.threads_in_use[0] = 2;
	sync_ctl.threads_created[0] = TRUE;
	sync_ctl.threads_finished[0] = FALSE;
	sync_ctl.is_revert[0] = FALSE;
	sync_ctl.continue_nexttime[0] = FALSE; /* No error */
	sync_ctl.threads_error[0] = FALSE; /* No error */
	sync_ctl.progress_fd[0] = fd;

	sync_threads[0].which_index = 0;
	pthread_create(&sync_ctl.inode_sync_thread[0], NULL,
	               sync_thread_function,
	               (void *) & (sync_threads[0]));
	sync_ctl.total_active_sync_threads++;
	sem_post(&sync_ctl.sync_op_sem);
	sleep(1);

	/* Reclaim resource */
	hcfs_system->system_going_down = TRUE;
	EXPECT_EQ(0, pthread_join(sync_ctl.sync_handler_thread, &res));

	/* Verify */
	EXPECT_EQ(0, sync_ctl.total_active_sync_threads);
	EXPECT_EQ(0, memcmp(empty_ino_array, &sync_ctl.threads_in_use, sizeof(empty_ino_array)));
	EXPECT_EQ(1, shm_verified_data->record_inode_counter);
	EXPECT_EQ(-1, access("/tmp/mock_progress_file", F_OK)); /* progress file will be deleted */
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(metapath, F_OK));

	unlink("/tmp/mock_progress_file");
	unlink(metapath);
}
/*
	End of unittest of init_sync_control()
 */

/*
	Unittest of sync_single_inode()
 */

class sync_single_inodeTest : public ::testing::Test {
public:
	char toupload_meta[100];
	char progress_file[100];
	int32_t max_objname_num;
	int fd;

protected:
	void SetUp() {
		/* First remove garbage if exists */
		unlink(MOCK_META_PATH);
		unlink(progress_file);
		unlink(toupload_meta);
		nftw("mock_meta_folder", do_delete, 20, FTW_DEPTH);

		mkdir("mock_meta_folder", 0700);
		/* Mock toupload meta for each inode */
		fetch_toupload_meta_path(toupload_meta, 1);
		mknod(toupload_meta, 0700, 0);

		strcpy(progress_file, "mock_meta_folder/progress_file");
		fd = open(progress_file, O_CREAT | O_RDWR);

		no_backend_stat = TRUE;
		init_sync_control(); /* Add this init so that upload thread will not hang up */
		init_sync_stat_control();
		max_objname_num = 4000;
		sem_init(&objname_counter_sem, 0, 1);
		sem_init(&backup_pkg_sem, 0, 1);
		objname_counter = 0;
		objname_list = (char **)malloc(sizeof(char *) * max_objname_num);
		for (int32_t i = 0 ; i < max_objname_num ; i++)
			objname_list[i] = (char *)malloc(sizeof(char) * 20);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
	}

	void TearDown() {
		void *res;
		char tmppath[200];
		char tmppath2[200];

		snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
		snprintf(tmppath2, 199, "%s/FSstat10", tmppath);
		unlink(tmppath2);
		snprintf(tmppath2, 199, "%s/tmpFSstat10", tmppath);
		unlink(tmppath2);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
		for (int32_t i = 0 ; i < max_objname_num ; i++)
			free(objname_list[i]);
		free(objname_list);

		unlink(MOCK_META_PATH);

		hcfs_system->system_going_down = TRUE;
		pthread_join(sync_ctl.sync_handler_thread, &res);

		/* Join the sync_control thread */
		hcfs_system->system_going_down = TRUE;
		pthread_join(sync_ctl.sync_handler_thread, NULL);
		close(fd);
		unlink(progress_file);
		unlink(toupload_meta);
		nftw("mock_meta_folder", do_delete, 20, FTW_DEPTH);
	}

	void write_mock_meta_file(char *metapath, int32_t total_page,
	                          uint8_t block_status, BOOL topin) {
		HCFS_STAT mock_stat;
		FILE_META_TYPE mock_file_meta;
		BLOCK_ENTRY_PAGE mock_block_page;
		BLOCK_UPLOADING_PAGE block_uploading_page;
		FILE_STATS_TYPE mock_statistics;
		CLOUD_RELATED_DATA mock_cloud_data;
		FILE *mock_metaptr, *mock_touploadptr;
		char buf[5000];
		size_t size;

		memset(&block_uploading_page, 0, sizeof(BLOCK_UPLOADING_PAGE));
		mock_total_page = total_page;
		mock_metaptr = fopen(metapath, "w+");
		mock_stat.size = total_page * MAX_BLOCK_ENTRIES_PER_PAGE *
		                 MAX_BLOCK_SIZE; /* Let total_blocks = 1000000/1000 = 1000 */
		mock_stat.mode = S_IFREG;
		mock_file_meta.root_inode = 10;
		mock_file_meta.local_pin = topin;
		fwrite(&mock_stat, sizeof(HCFS_STAT), 1, mock_metaptr); // Write stat
		fwrite(&mock_file_meta, sizeof(FILE_META_TYPE), 1, mock_metaptr); // Write file meta
		fwrite(&mock_statistics, sizeof(FILE_STATS_TYPE), 1, mock_metaptr);
		fwrite(&mock_cloud_data, sizeof(CLOUD_RELATED_DATA), 1, mock_metaptr);

		for (int32_t i = 0 ; i < MAX_BLOCK_ENTRIES_PER_PAGE ; i++) {
			mock_block_page.block_entries[i].status = block_status;
			mock_block_page.block_entries[i].seqnum = 1;

			block_uploading_page.status_entry[i].to_upload_seq = 1;
			block_uploading_page.status_entry[i].backend_seq = 0;
			if (block_status == ST_LDISK) {
				SET_TOUPLOAD_BLOCK_EXIST(block_uploading_page.status_entry[i].block_exist);
			}
		}
		mock_block_page.num_entries = MAX_BLOCK_ENTRIES_PER_PAGE;
		for (int32_t page_num = 0 ; page_num < total_page ; page_num++) {
			fwrite(&mock_block_page, sizeof(BLOCK_ENTRY_PAGE),
			       1, mock_metaptr); /* Linearly write block page */
			pwrite(fd, &block_uploading_page,
			       sizeof(BLOCK_UPLOADING_PAGE),
			       page_num * sizeof(BLOCK_UPLOADING_PAGE));
		}

		/* copy toupload meta */
		mock_touploadptr = fopen(toupload_meta, "w+");
		fseek(mock_metaptr, 0, SEEK_SET);
		fseek(mock_touploadptr, 0, SEEK_SET);
		while ((size = fread(buf, 1, 4096, mock_metaptr))) {
			fwrite(buf, 1, size, mock_touploadptr);
		}

		fclose(mock_touploadptr);
		fclose(mock_metaptr);
	}

	static int32_t objname_cmp(const void *s1, const void *s2) {
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

TEST_F(sync_single_inodeTest, MetaNotExist)
{
	SYNC_THREAD_TYPE mock_thread_type;
	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;
	mock_thread_type.which_index = 0;

	/* Run tested function */
	sync_single_inode(&mock_thread_type);
}

TEST_F(sync_single_inodeTest, SyncBlockFileSuccessNoPin)
{
	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = MOCK_META_PATH;
	int32_t total_page = 3;
	int32_t num_total_blocks = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;
	int32_t semval;

	/* Mock data */
	system_config->max_block_size = 1000;
	write_mock_meta_file(metapath, total_page, ST_LDISK, FALSE);

	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;
	mock_thread_type.which_index = 0;
	mock_thread_type.is_revert = FALSE;
	mock_thread_type.progress_fd = fd;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;

	/* Run tested function */
	init_upload_control();
	init_sync_stat_control();
	sync_single_inode(&mock_thread_type);
	sleep(2);

	hcfs_system->system_going_down = TRUE;
	sleep(1);

	/* Verify */
	printf("Begin to verify sync blocks\n");
	EXPECT_EQ(num_total_blocks, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *), sync_single_inodeTest::objname_cmp);
	for (int blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) { // Check uploaded-object is recorded
		char expected_objname[50];
		sprintf(expected_objname, "data_%lu_%d",
		        mock_thread_type.inode, blockno);
		ASSERT_STREQ(expected_objname, objname_list[blockno]) << "blockno = " << blockno;
		sprintf(expected_objname, "/tmp/testHCFS/data_%" PRIu64 "_%d",
		        (uint64_t)mock_thread_type.inode, blockno);
		unlink(expected_objname);
	}
	printf("Begin to check block status\n");
	metaptr = fopen(metapath, "r+");
	fseek(metaptr, sizeof(HCFS_STAT), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, metaptr);
	fseek(metaptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
	      sizeof(FILE_STATS_TYPE) + sizeof(CLOUD_RELATED_DATA),
	      SEEK_SET);
	while (!feof(metaptr)) {
		fread(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, metaptr); // Linearly read block meta
		for (int i = 0 ; i < block_page.num_entries ; i++) {
			ASSERT_EQ(ST_BOTH, block_page.block_entries[i].status); // Check status
		}
	}
	fclose(metaptr);
	unlink(metapath);
}

TEST_F(sync_single_inodeTest, SyncBlockFileSuccessPin)
{
	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = MOCK_META_PATH;
	int total_page = 3;
	int num_total_blocks = total_page * MAX_BLOCK_ENTRIES_PER_PAGE + 1;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;
	int32_t semval;

	/* Mock data */
	write_mock_meta_file(metapath, total_page, ST_LDISK, TRUE);

	system_config->max_block_size = 100;
	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;
	mock_thread_type.which_index = 0;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;

	/* Run tested function */
	init_upload_control();
	init_sync_stat_control();
	sync_single_inode(&mock_thread_type);
	sleep(2);

	/* No cache replacement triggered */
	sem_getvalue(&(hcfs_system->something_to_replace), &semval);
	EXPECT_EQ(0, semval);

	hcfs_system->system_going_down = TRUE;
	sleep(1);

	/* Verify */
	printf("Begin to verify sync blocks\n");
	EXPECT_EQ(num_total_blocks, objname_counter);
	qsort(objname_list, objname_counter, sizeof(char *),
	      sync_single_inodeTest::objname_cmp);

	/* Check uploaded-object is recorded */
	for (int32_t blockno = 0 ; blockno < num_total_blocks - 1 ; blockno++) {
		char expected_objname[50];
		sprintf(expected_objname, "data_%lu_%d",
		        mock_thread_type.inode, blockno);
		ASSERT_STREQ(expected_objname, objname_list[blockno]) <<
		        "blockno = " << blockno;
		sprintf(expected_objname, "/tmp/testHCFS/data_%" PRIu64 "_%d",
		        (uint64_t)mock_thread_type.inode, blockno);
		unlink(expected_objname);
	}

	printf("Begin to check block status\n");
	metaptr = fopen(metapath, "r+");
	fseek(metaptr, sizeof(HCFS_STAT), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, metaptr);
	fseek(metaptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
	      sizeof(FILE_STATS_TYPE) + sizeof(CLOUD_RELATED_DATA),
	      SEEK_SET);
	while (!feof(metaptr)) {
		/* Linearly read block meta */
		fread(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, metaptr);
		for (int32_t i = 0 ; i < block_page.num_entries ; i++) {
			ASSERT_EQ(ST_BOTH, block_page.block_entries[i].status);
		}
	}
	fclose(metaptr);
	unlink(metapath);
}

TEST_F(sync_single_inodeTest, Sync_Todelete_BlockFileSuccess)
{
	SYNC_THREAD_TYPE mock_thread_type;
	char metapath[] = MOCK_META_PATH;
	int32_t total_page = 3;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE filemeta;
	FILE *metaptr;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;
	/* Mock data */
	system_config->max_block_size = 1000;
	write_mock_meta_file(metapath, total_page, ST_TODELETE, FALSE);

	mock_thread_type.inode = 1;
	mock_thread_type.this_mode = S_IFREG;
	mock_thread_type.which_index = 0;
	mock_thread_type.is_revert = FALSE;

	/* Run tested function */
	init_upload_control();
	sync_single_inode(&mock_thread_type);
	sleep(1);

	/* Check status */
	printf("Begin to check block status\n");
	metaptr = fopen(metapath, "r+");
	fseek(metaptr, sizeof(HCFS_STAT), SEEK_SET);
	fread(&filemeta, sizeof(FILE_META_TYPE), 1, metaptr);
	fseek(metaptr, sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
	      sizeof(FILE_STATS_TYPE) + sizeof(CLOUD_RELATED_DATA),
	      SEEK_SET);
	while (!feof(metaptr)) {
		/* Linearly read block meta */
		fread(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, metaptr);
		for (int i = 0 ; i < block_page.num_entries ; i++) {
			ASSERT_EQ(ST_NONE, block_page.block_entries[i].status)
			        << "i = " << i;
		}

	}
	fclose(metaptr);
	unlink(metapath);
}
/*
	End of unittest of sync_single_inode()
 */

/*
	Unittest of upload_loop()
 */
int32_t inode_cmp(const void *a, const void *b)
{
	return *(int32_t *)a - *(int32_t *)b;
}

static void *upload_loop_thread_function(void *ptr)
{
#ifdef _ANDROID_ENV_
	upload_loop(NULL);
#else
	upload_loop();
#endif

	return NULL;
}

class upload_loopTest : public ::testing::Test
{
protected:
	FILE *mock_file_meta;
	int max_objname_num;
	char toupload_meta[100];
	char sync_path[100];
	int ret, errcode;

	void SetUp() {
		mkdir("mock_meta_folder", 0700);

		no_backend_stat = TRUE;
		init_sync_stat_control();
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
		snprintf(sync_path, 100, "%s/FS_sync", METAPATH);
		printf("%s\n", sync_path);
		ret = mkdir(sync_path, 0700);
		if (ret != 0) {
			errcode = errno;
			printf("%s\n", strerror(errcode));
		}
		mock_file_meta = fopen(MOCK_META_PATH, "w+");
		setbuf(mock_file_meta, NULL);

		objname_counter = 0;
		max_objname_num = 40;
		objname_list = (char **)malloc(sizeof(char *) * max_objname_num);
		for (int32_t i = 0 ; i < max_objname_num ; i++) {
			HCFS_STAT empty_stat;
			DIR_META_TYPE empty_meta;
			CLOUD_RELATED_DATA mock_cloud_data;
			FILE *fptr;

			objname_list[i] = (char *)malloc(sizeof(char) * 20);
			/* Mock toupload meta for each inode */
			fetch_toupload_meta_path(toupload_meta, (i + 1) * 5);
			fptr = fopen(toupload_meta, "w+");
			setbuf(fptr, NULL);
			memset(&empty_stat, 0, sizeof(HCFS_STAT));
			memset(&empty_meta, 0, sizeof(DIR_META_TYPE));
			memset(&mock_cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
			empty_meta.root_inode = 10;
			fseek(fptr, 0, SEEK_SET);
			fwrite(&empty_stat, sizeof(HCFS_STAT), 1, fptr);
			fwrite(&empty_meta, sizeof(DIR_META_TYPE), 1, fptr);
			fwrite(&mock_cloud_data, sizeof(CLOUD_RELATED_DATA),
			       1, fptr);
			fclose(fptr);
		}

		sem_init(&objname_counter_sem, 0, 1);
		sem_init(&backup_pkg_sem, 0, 1);

		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_paused = FALSE;
		CACHE_SOFT_LIMIT = 100000;
		hcfs_system->systemdata.cache_size = 0;

		/* Generate mock data and allocate space to check answer */
		shm_test_data = (LoopTestData *)malloc(sizeof(LoopTestData));
		shm_test_data->num_inode = max_objname_num; /* Test 40 nodes */
		shm_test_data->to_handle_inode = (int *)
		                                 malloc(sizeof(int) * shm_test_data->num_inode);
		shm_test_data->tohandle_counter = 0;
		for (int i = 0; i < max_objname_num; i++)
			/* mock inode, which is used as expected answer */
			shm_test_data->to_handle_inode[i] = (i + 1) * 5;

		/* Allocate space to store actual value */
		shm_verified_data = (LoopToVerifiedData *)malloc(sizeof(LoopToVerifiedData));
		shm_verified_data->record_handle_inode = (int *)
		        malloc(sizeof(int) * shm_test_data->num_inode);
		shm_verified_data->record_inode_counter = 0;
		sem_init(&(shm_verified_data->record_inode_sem), 0, 1);
	}

	void TearDown() {
		char tmppath[200];
		char tmppath2[200];
		snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
		snprintf(tmppath2, 199, "%s/FSstat10", tmppath);
		unlink(tmppath2);
		snprintf(tmppath2, 199, "%s/tmpFSstat10", tmppath);
		unlink(tmppath2);
		nftw(tmppath, do_delete, 20, FTW_DEPTH);
		for (int32_t i = 0 ; i < max_objname_num ; i++)
			free(objname_list[i]);
		free(objname_list);

		if (!access(toupload_meta, F_OK))
			unlink(toupload_meta);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);

		nftw("mock_meta_folder", do_delete, 20, FTW_DEPTH);

		free(shm_test_data->to_handle_inode);
		free(shm_test_data);
		free(shm_verified_data->record_handle_inode);
		free(shm_verified_data);
	}
};

TEST_F(upload_loopTest, UploadLoopWorkSuccess_OnlyTestDirCase)
{
	pthread_t thread_id;
	int32_t shm_key, shm_key2;
	HCFS_STAT empty_stat;
	DIR_META_TYPE empty_meta;
	BLOCK_ENTRY_PAGE mock_block_page;
	CLOUD_RELATED_DATA mock_cloud_data;

	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = TRUE;
	hcfs_system->sync_manual_switch = ON;
	hcfs_system->sync_paused = OFF;

	/* Write something into meta, int32_t the unittest, only test
	   the case that upload dir meta because regfile case has
	   been tested in sync_single_inodeTest(). */
	memset(&empty_stat, 0, sizeof(HCFS_STAT));
	memset(&empty_meta, 0, sizeof(DIR_META_TYPE));
	memset(&mock_cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	empty_stat.mode = S_IFDIR;
	empty_meta.root_inode = 10;
	fseek(mock_file_meta, 0, SEEK_SET);
	fwrite(&empty_stat, sizeof(HCFS_STAT), 1, mock_file_meta);
	fwrite(&empty_meta, sizeof(DIR_META_TYPE), 1, mock_file_meta);
	fwrite(&mock_cloud_data, sizeof(CLOUD_RELATED_DATA), 1, mock_file_meta);
	fclose(mock_file_meta);

	hcfs_system->systemdata.cache_size = CACHE_SOFT_LIMIT; // Let system upload
	hcfs_system->systemdata.dirty_cache_size = 100;

	/* Set first_dirty_inode to be uploaded */
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->head.first_dirty_inode = shm_test_data->to_handle_inode[0];
	sys_super_block->head.num_dirty = max_objname_num;

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
		ASSERT_EQ(shm_test_data->to_handle_inode[i],
		          shm_verified_data->record_handle_inode[i]);
		fetch_toupload_meta_path(toupload_meta,
		                         shm_verified_data->record_handle_inode[i]);
		ASSERT_EQ(-1, access(toupload_meta, F_OK));
	}
}

/*
	End of unittest of upload_loop()
 */

/* Begin of the test case for the function update_backend_stat */

class update_backend_statTest : public ::testing::Test
{
protected:
	int32_t count;
	char tmpmgrpath[100];
	virtual void SetUp() {
		no_backend_stat = TRUE;
		hcfs_system->system_going_down = FALSE;
		if (access(METAPATH, F_OK) < 0)
			mkdir(METAPATH, 0744);
		sem_init(&backup_pkg_sem, 0, 1);
	}

	virtual void TearDown() {
		hcfs_system->system_going_down = TRUE;
	}

};

TEST_F(update_backend_statTest, EmptyStat)
{
	char tmppath[200];
	char tmppath2[200];
	int32_t ret;
	FILE *fptr;
	FS_CLOUD_STAT_T fs_cloud_stat;

	snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
	snprintf(tmppath2, 199, "%s/FSstat14", tmppath);

	ret = access(tmppath, F_OK);
	ASSERT_NE(0, ret);
	init_sync_stat_control();

	ret = access(tmppath, F_OK);
	EXPECT_EQ(0, ret);

	ret = access(tmppath2, F_OK);
	ASSERT_NE(0, ret);

	ret = update_backend_stat(14, 1024768, 5566, 101, 0, 0, 5566);

	EXPECT_EQ(0, ret);
	ret = access(tmppath2, F_OK);
	ASSERT_EQ(0, ret);

	/* Verify content */

	fptr = fopen(tmppath2, "r");
	fseek(fptr, 0, SEEK_SET);
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);

	EXPECT_EQ(1024768, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(5566, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(101, fs_cloud_stat.backend_num_inodes);

	/* Cleanup */
	unlink(tmppath2);
	snprintf(tmppath2, 199, "%s/tmpFSstat14", tmppath);
	unlink(tmppath2);

	nftw(tmppath, do_delete, 20, FTW_DEPTH);
}

TEST_F(update_backend_statTest, UpdateExistingStat)
{
	char tmppath[200];
	char tmppath2[200];
	char tmppath3[200];
	int32_t ret;
	FILE *fptr;
	FS_CLOUD_STAT_T fs_cloud_stat;

	snprintf(tmppath, 199, "%s/FS_sync", METAPATH);
	snprintf(tmppath2, 199, "%s/FSstat14", tmppath);
	snprintf(tmppath3, 199, "%s/tmpFSstat14", tmppath);

	ret = access(tmppath, F_OK);
	ASSERT_NE(0, ret);
	init_sync_stat_control();

	ret = access(tmppath, F_OK);
	EXPECT_EQ(0, ret);

	ret = access(tmppath2, F_OK);
	ASSERT_NE(0, ret);

	fs_cloud_stat.backend_system_size = 7687483;
	fs_cloud_stat.backend_meta_size = 5566;
	fs_cloud_stat.backend_num_inodes = 34334;
	fptr = fopen(tmppath2, "w");
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);
	fptr = fopen(tmppath3, "w");
	fwrite(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);

	ret = update_backend_stat(14, 1024768, 123, -101, 0, 0, 123);

	EXPECT_EQ(0, ret);
	ret = access(tmppath2, F_OK);
	ASSERT_EQ(0, ret);

	/* Verify content */

	fptr = fopen(tmppath2, "r");
	fread(&fs_cloud_stat, sizeof(FS_CLOUD_STAT_T), 1, fptr);
	fclose(fptr);

	EXPECT_EQ(1024768 + 7687483, fs_cloud_stat.backend_system_size);
	EXPECT_EQ(5566 + 123, fs_cloud_stat.backend_meta_size);
	EXPECT_EQ(34334 - 101, fs_cloud_stat.backend_num_inodes);

	/* Cleanup */
	nftw(tmppath, do_delete, 20, FTW_DEPTH);
}

/* End of the test case for the function update_backend_stat */
