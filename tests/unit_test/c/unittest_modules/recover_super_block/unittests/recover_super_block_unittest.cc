/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <semaphore.h>
#include <signal.h>
extern "C" {
#include "recover_super_block.h"
#include "fuseop.h"
#include "global.h"
#include "time.h"
}

#include "../../fff.h"
DEFINE_FFF_GLOBALS;


#define MIN_INODE_NO 2
#define MOCK_METAPATH "test_patterns"
#define MOCK_DIRTY_META_SIZE 512
#define MOCK_DIRTY_DATA_SIZE 1024

SYSTEM_CONF_STRUCT *system_config = NULL;
META_CACHE_ENTRY_STRUCT mock_meta_cache_entry;


/* Internal functions */
extern "C" {
int32_t _batch_read_sb_entries(SUPER_BLOCK_ENTRY *buf,
			       ino_t start_inode,
			       int64_t num);
int32_t _batch_write_sb_entries(SUPER_BLOCK_ENTRY *buf,
				ino_t start_inode,
				int64_t num);
int32_t _read_regfile_meta(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
			   FILE_STATS_TYPE *file_stats);
}


/* Definition for fake functions - Fake system calls */
/* fopen */
FAKE_VALUE_FUNC(FILE*, fopen, const char*, const char*);
typeof(fopen) *real_fopen = (typeof(fopen)*)dlsym(RTLD_NEXT, "fopen");

int32_t fopen_force_error = 0;
FILE *custom_fopen(const char *path, const char *mode)
{
	if (fopen_force_error) {
		errno = EIO;
		return NULL;
	}
	return real_fopen(path, mode);
}

/* fwrite */
FAKE_VALUE_FUNC(size_t, fwrite, const void*, size_t, size_t, FILE*);
typeof(fwrite) *real_fwrite = (typeof(fwrite)*)dlsym(RTLD_NEXT, "fwrite");

int32_t fwrite_force_error = 0;
size_t custom_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (fwrite_force_error) {
		errno = EIO;
		return 0;
	}
	return real_fwrite(ptr, size, nmemb, stream);
}

/* pread */
FAKE_VALUE_FUNC(ssize_t, pread, int, void*, size_t, off_t);
typeof(pread) *real_pread = (typeof(pread)*)dlsym(RTLD_NEXT, "pread");

int32_t pread_force_error = 0;
ssize_t custom_pread(int fd, void *buf, size_t count, off_t offset)
{
	if (pread_force_error) {
		errno = EIO;
		return -1;
	}
	return real_pread(fd, buf, count, offset);
}

/* pwrite */
FAKE_VALUE_FUNC(ssize_t, pwrite, int, const void*, size_t, off_t);
typeof(pwrite) *real_pwrite = (typeof(pwrite)*)dlsym(RTLD_NEXT, "pwrite");

int32_t pwrite_force_error = 0;
ssize_t custom_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	if (pwrite_force_error) {
		errno = EIO;
		return -1;
	}
	return real_pwrite(fd, buf, count, offset);
}


/* Fake hcfs functions */
FAKE_VALUE_FUNC_VARARG(int32_t, write_log, int32_t, const char*, ...);

FAKE_VALUE_FUNC(int32_t, super_block_share_locking);
FAKE_VALUE_FUNC(int32_t, super_block_share_release);
FAKE_VALUE_FUNC(int32_t, super_block_exclusive_locking);
FAKE_VALUE_FUNC(int32_t, super_block_exclusive_release);
FAKE_VALUE_FUNC(int32_t, write_super_block_head);

FAKE_VALUE_FUNC(int32_t, meta_cache_unlock_entry,META_CACHE_ENTRY_STRUCT*);
FAKE_VALUE_FUNC(int32_t, meta_cache_close_file, META_CACHE_ENTRY_STRUCT*);

FAKE_VALUE_FUNC(int32_t, sync_hcfs_system_data, char);

/* write_super_block_entry */
FAKE_VALUE_FUNC(int32_t, write_super_block_entry, ino_t, SUPER_BLOCK_ENTRY*);

int32_t write_super_block_entry_use_real = 0;
int32_t write_super_block_entry_force_error = 0;
int32_t custom_write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (write_super_block_entry_force_error)
		return -EIO;

	if (write_super_block_entry_use_real) {
		pwrite(sys_super_block->iofptr, inode_ptr, SB_ENTRY_SIZE,
		       SB_HEAD_SIZE + (this_inode - 1) * SB_ENTRY_SIZE);
	}

	return 0;
}

/* read_super_block_entry */
FAKE_VALUE_FUNC(int32_t, read_super_block_entry, ino_t, SUPER_BLOCK_ENTRY *);

int32_t read_super_block_entry_force_error = 0;
int32_t custom_read_super_block_entry(ino_t this_inode,
				      SUPER_BLOCK_ENTRY *inode_ptr)
{
	SUPER_BLOCK_ENTRY tmp_entry = {0};

	if (read_super_block_entry_force_error)
		return -EIO;

	tmp_entry.this_index = sys_super_block->head.last_dirty_inode;
	if (tmp_entry.this_index != 0)
		tmp_entry.util_ll_prev =
		    sys_super_block->head.last_dirty_inode - 1;

	memcpy(inode_ptr, &tmp_entry, sizeof(SUPER_BLOCK_ENTRY));
        return 0;
}

/* meta_cache_lock_entry */
FAKE_VALUE_FUNC(META_CACHE_ENTRY_STRUCT*, meta_cache_lock_entry, ino_t);

int32_t meta_cache_lock_entry_force_error = 0;
META_CACHE_ENTRY_STRUCT *custom_meta_cache_lock_entry(ino_t this_inode)
{
	if (meta_cache_lock_entry_force_error)
		return NULL;
	else
		return &mock_meta_cache_entry;
}

/* meta_cache_open_file */
FAKE_VALUE_FUNC(int32_t, meta_cache_open_file, META_CACHE_ENTRY_STRUCT*);

char meta_cache_open_file_realpath[strlen(MOCK_METAPATH) + 20];
int32_t meta_cache_open_file_force_error = 0;
int32_t custom_meta_cache_open_file(META_CACHE_ENTRY_STRUCT *body_ptr)
{
	if (meta_cache_open_file_force_error)
		return -EIO;

	body_ptr->fptr = fopen(meta_cache_open_file_realpath, "r+");
	body_ptr->meta_opened = TRUE;
        return 0;
}

void __attribute__((constructor)) Init(void)
{
#define CUSTOM_FAKE(F) F##_fake.custom_fake = custom_##F;
		/* Fake functions */
		CUSTOM_FAKE(fopen);
		CUSTOM_FAKE(fwrite);
		CUSTOM_FAKE(pread);
		CUSTOM_FAKE(pwrite);
		CUSTOM_FAKE(write_super_block_entry);
		CUSTOM_FAKE(read_super_block_entry);
		CUSTOM_FAKE(meta_cache_lock_entry);
		CUSTOM_FAKE(meta_cache_open_file);
#undef CUSTOM_FAKE
}

int32_t change_system_meta(int64_t arg1 __attribute__((unused)),
			   int64_t arg2 __attribute__((unused)),
			   int64_t arg3 __attribute__((unused)),
			   int64_t arg4 __attribute__((unused)),
			   int64_t dirty_cache_delta,
			   int64_t unpin_dirty_delta,
			   BOOL arg5 __attribute__((unused)))
{
	sem_wait(&(hcfs_system->access_sem));

	hcfs_system->systemdata.dirty_cache_size += dirty_cache_delta;
	if (hcfs_system->systemdata.dirty_cache_size < 0)
		hcfs_system->systemdata.dirty_cache_size = 0;

	/* Unpin & dirty means the space cannot be freed */
	hcfs_system->systemdata.unpin_dirty_data_size += unpin_dirty_delta;
	if (hcfs_system->systemdata.unpin_dirty_data_size < 0)
		hcfs_system->systemdata.unpin_dirty_data_size = 0;

	sem_post(&(hcfs_system->access_sem));

	return 0;
}

/* round_size */
int64_t round_size(int64_t size)
{
	int64_t blksize = 4096;
	int64_t ret_size;

	if (size >= 0) {
		/* round up to filesystem block size */
		ret_size = (size + blksize - 1) & (~(blksize - 1));
	} else {
		size = -size;
		ret_size = -((size + blksize - 1) & (~(blksize - 1)));
	}

	return ret_size;
}


/* Helper functions */
void _create_mock_sb_entry_arr(SUPER_BLOCK_ENTRY *sb_entry_arr,
			       int64_t num_entries,
			       BOOL reverse,
			       char status,
			       char pin_status)
{
	int64_t count, ino_index, ino_shift;

	ASSERT_TRUE(sb_entry_arr != NULL);
	ASSERT_LE(num_entries, MAX_NUM_ENTRY_HANDLE);

	if (!reverse) {
		ino_index = MIN_INODE_NO;
		ino_shift = 1;
	} else {
		ino_index = MIN_INODE_NO + num_entries - 1;
		ino_shift = -1;
	}

	for (count = 0; count < num_entries; count++) {
		sb_entry_arr[count].this_index = ino_index;
		sb_entry_arr[count].status = status;
		sb_entry_arr[count].pin_status = pin_status;
		sb_entry_arr[count].dirty_meta_size = MOCK_DIRTY_META_SIZE;
		ino_index += ino_shift;
	}
	return;
}

void _create_mock_meta_cache_arr(META_CACHE_ENTRY_STRUCT **meta_cache_arr,
			       int64_t num_entries,
			       BOOL set_null)
{
	int64_t count, ino_index, ino_shift;

	ASSERT_TRUE(meta_cache_arr != NULL);
	ASSERT_LE(num_entries, MAX_NUM_ENTRY_HANDLE);

	for (count = 0; count < num_entries; count++) {
		if (set_null)
			meta_cache_arr[count] = NULL;
		else
			meta_cache_arr[count] = &mock_meta_cache_entry;
	}
	return;
}

int32_t _create_mock_regfile_meta_file(char *pathname, FILE_STATS_TYPE *tmp_fs_t)
{
	int64_t wsize;
	FILE *tmp_fp;

	tmp_fp = fopen(pathname, "w");
	if (tmp_fp == NULL)
		return -1;

	wsize = pwrite(fileno(tmp_fp), tmp_fs_t, sizeof(FILE_STATS_TYPE),
		       sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE));
	if (wsize != sizeof(FILE_STATS_TYPE)) {
		fclose(tmp_fp);
		return -1;
	}

	fclose(tmp_fp);
	return 0;
}

int32_t _create_mock_sb_file(char *filename, int64_t num_entries)
{
	char fullpath[strlen(METAPATH) + 20];
	int64_t idx, wsize;
	FILE *sb_fp;
	SUPER_BLOCK_HEAD tmp_head;
	SUPER_BLOCK_ENTRY tmp_entry;

	if (filename == NULL)
		strncpy(fullpath, SUPERBLOCK, sizeof(fullpath));
	else
		snprintf(fullpath, sizeof(fullpath), "%s/%s", METAPATH,
			 filename);

	sb_fp = fopen(fullpath, "w");
	if (sb_fp == NULL)
		return -1;

	wsize = fwrite(&tmp_head, SB_HEAD_SIZE, 1, sb_fp);
	if (wsize <= 0) {
		fclose(sb_fp);
		unlink(fullpath);
		return -1;
	}

	tmp_entry.status = IS_DIRTY;
	tmp_entry.pin_status = ST_UNPIN;
	tmp_entry.dirty_meta_size = MOCK_DIRTY_META_SIZE;
	for (idx = MIN_INODE_NO - 1; idx < MIN_INODE_NO + num_entries; idx++) {
		tmp_entry.this_index = idx;
		fwrite(&tmp_entry, SB_ENTRY_SIZE, 1, sb_fp);
		if (wsize <= 0) {
			fclose(sb_fp);
			unlink(fullpath);
			return -1;
		}
	}

	fclose(sb_fp);
	return 0;
}


/* Test environment */
class RecoverSBEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp() {
		mkdir(MOCK_METAPATH, S_IRWXU | S_IRWXG | S_IRWXO);

		/* Mock config info */
		system_config =
		    (SYSTEM_CONF_STRUCT *)calloc(1, sizeof(SYSTEM_CONF_STRUCT));
		system_config->metapath =
		    (char *)calloc(1, (strlen(MOCK_METAPATH) + 10));
		strcpy(system_config->metapath, MOCK_METAPATH);
		system_config->superblock_name =
		    (char *)calloc(1, strlen(system_config->metapath) + 20);
		snprintf(system_config->superblock_name,
			 strlen(system_config->metapath) + 20, "%s/superblock",
			 system_config->metapath);

		/* Mock super block */
		sys_super_block = (SUPER_BLOCK_CONTROL *)calloc(
		    1, sizeof(SUPER_BLOCK_CONTROL));

		/* Mock system data */
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)calloc(1, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 1, 1);
	}

	virtual void TearDown() {
		free(system_config->metapath);
		free(system_config->superblock_name);
		free(system_config);
		free(sys_super_block);
		free(hcfs_system);
	}
};

::testing::Environment *const pyhcfs_env =
    ::testing::AddGlobalTestEnvironment(new RecoverSBEnvironment);

/* Unittest for need_recover_sb */
class need_recover_sbTest : public ::testing::Test
{
	protected:

	void SetUp() {}

	void TearDown() {}
};

TEST_F(need_recover_sbTest, AnotherRecoveryOngoing)
{
	sys_super_block->sb_recovery_meta.is_ongoing = TRUE;
	EXPECT_FALSE(need_recover_sb());
	sys_super_block->sb_recovery_meta.is_ongoing = FALSE;
}

TEST_F(need_recover_sbTest, SyncPointIsSet)
{
	sys_super_block->sync_point_is_set = TRUE;
	EXPECT_FALSE(need_recover_sb());
	sys_super_block->sync_point_is_set = FALSE;
}

TEST_F(need_recover_sbTest, RestorationIsOngoing)
{
	hcfs_system->system_restoring = RESTORING_STAGE1;
	EXPECT_FALSE(need_recover_sb());
	hcfs_system->system_restoring = NOT_RESTORING;
}

TEST_F(need_recover_sbTest, LastRecoverTimeTooClose)
{
	sys_super_block->sb_recovery_meta.last_recovery_ts = time(NULL);
	EXPECT_FALSE(need_recover_sb());
	sys_super_block->sb_recovery_meta.last_recovery_ts = 0;
}

TEST_F(need_recover_sbTest, ProgressFileExisted)
{
	char progressf_path[128];

	sprintf(progressf_path, "%s/%s", MOCK_METAPATH, PROGRESS_FILE);
	printf("Create mock progress file - %s\n", progressf_path);
	FILE *tmp_fp = fopen(progressf_path, "w");
	fclose(tmp_fp);
	EXPECT_TRUE(need_recover_sb());
	remove(progressf_path);
}

TEST_F(need_recover_sbTest, NumDirtyAndSizeNotMatched)
{
	sys_super_block->head.num_dirty = 0;
	hcfs_system->systemdata.dirty_cache_size = 999;
	hcfs_system->systemdata.unpin_dirty_data_size = 0;
	EXPECT_TRUE(need_recover_sb());

	sys_super_block->head.num_dirty = 0;
	hcfs_system->systemdata.dirty_cache_size = 0;
	hcfs_system->systemdata.unpin_dirty_data_size = 999;
	EXPECT_TRUE(need_recover_sb());

	sys_super_block->head.num_dirty =
	    hcfs_system->systemdata.dirty_cache_size =
		hcfs_system->systemdata.unpin_dirty_data_size = 0;
}

TEST_F(need_recover_sbTest, NumDirtyAndDirtyQueueNotMatched)
{
	sys_super_block->head.num_dirty = 0;
	sys_super_block->head.first_dirty_inode = 0;
	EXPECT_FALSE(need_recover_sb());

	sys_super_block->head.num_dirty = 999;
	sys_super_block->head.first_dirty_inode = 0;
	EXPECT_TRUE(need_recover_sb());

	sys_super_block->head.num_dirty = 0;
	sys_super_block->head.first_dirty_inode = 999;
	EXPECT_TRUE(need_recover_sb());

	sys_super_block->head.num_dirty =
	    sys_super_block->head.first_dirty_inode = 0;
}
/* End unittest for need_recover_sb */

/* Unittest for fetch_recover_progressf_path */
TEST(fetch_recover_progressf_pathTEST, Success)
{
	char correct_path[128];
	char progressf_path[128];

	sprintf(correct_path, "%s/%s", MOCK_METAPATH, PROGRESS_FILE);
	fetch_recover_progressf_path(progressf_path);
	EXPECT_EQ(0, strcmp(correct_path, progressf_path));
}
/* End unittest for fetch_recover_progressf_path */

/* Unittest for fetch_last_recover_progress */
class fetch_last_recover_progressTest : public ::testing::Test
{
        protected:
	ino_t start_inode, end_inode;
	ino_t target_inodes[2] = {10, 100};
	char progressf_path[128];

	void SetUp() { fetch_recover_progressf_path(progressf_path); }

	void TearDown() { unlink(progressf_path); }
};

TEST_F(fetch_last_recover_progressTest, Success)
{
	FILE *tmp_fp = fopen(progressf_path, "w");
	fwrite(target_inodes, 2, sizeof(ino_t), tmp_fp);
	fclose(tmp_fp);

	EXPECT_EQ(0, fetch_last_recover_progress(&start_inode, &end_inode));
	EXPECT_EQ(target_inodes[0], start_inode);
	EXPECT_EQ(target_inodes[1], end_inode);
}

TEST_F(fetch_last_recover_progressTest, ProgressFileNotExisted)
{
	EXPECT_EQ(-1, fetch_last_recover_progress(&start_inode, &end_inode));
	EXPECT_EQ(0, start_inode);
	EXPECT_EQ(0, end_inode);
}

TEST_F(fetch_last_recover_progressTest, ErrorInRead)
{
	char ch = "0";

	FILE *tmp_fp = fopen(progressf_path, "w");
	fwrite(&ch, 1, sizeof(char), tmp_fp);
	fclose(tmp_fp);

	EXPECT_EQ(-1, fetch_last_recover_progress(&start_inode, &end_inode));
	EXPECT_EQ(0, start_inode);
	EXPECT_EQ(0, end_inode);
}
/* End unittest for fetch_last_recover_progress */

/* Unittest for update_recover_progress */
class update_recover_progressTest : public ::testing::Test
{
        protected:
	char progressf_path[128];

	void SetUp() { fetch_recover_progressf_path(progressf_path); }

	void TearDown() { unlink(progressf_path); }
};

TEST_F(update_recover_progressTest, Success)
{
	ino_t inodes_in_file[2];
	ino_t target_inodes[2] = {10, 100};

	EXPECT_EQ(0, update_recover_progress(target_inodes[0], target_inodes[1]));

	FILE *tmp_fp = fopen(progressf_path, "r");
	fread(inodes_in_file, 2, sizeof(ino_t), tmp_fp);
	fclose(tmp_fp);

	EXPECT_EQ(target_inodes[0], inodes_in_file[0]);
	EXPECT_EQ(target_inodes[1], inodes_in_file[1]);
	EXPECT_EQ(0, access(progressf_path, F_OK));
}

TEST_F(update_recover_progressTest, ErrorInFopen)
{
	ino_t inodes_in_file[2];
	ino_t target_inodes[2] = {10, 100};

	fopen_force_error = 1;
	EXPECT_EQ(-1, update_recover_progress(target_inodes[0], target_inodes[1]));
	EXPECT_EQ(-1, access(progressf_path, F_OK));
	fopen_force_error = 0;
}

TEST_F(update_recover_progressTest, ErrorInFwrite)
{
	ino_t inodes_in_file[2];
	ino_t target_inodes[2] = {10, 100};

	fwrite_force_error = 1;
	EXPECT_EQ(-1, update_recover_progress(target_inodes[0], target_inodes[1]));
	EXPECT_EQ(-1, access(progressf_path, F_OK));
	fwrite_force_error = 0;
}
/* End unittest for need_recover_sb */

/* Unittest for unlink_recover_progress_file */
TEST(unlink_recover_progress_fileTest, Success)
{

	char progressf_path[128];
	fetch_recover_progressf_path(progressf_path);

	FILE *tmp_fp = fopen(progressf_path, "w");
	fclose(tmp_fp);

	unlink_recover_progress_file();

	EXPECT_EQ(-1, access(progressf_path, F_OK));
}
/* End unittest for unlink_recover_progress_file */

/* Unittest for set_recovery_flag */
class set_recovery_flagTest : public ::testing::Test
{
        protected:

        void SetUp() {}

	void TearDown()
	{
		sys_super_block->sb_recovery_meta.is_ongoing = FALSE;
		sys_super_block->sb_recovery_meta.start_inode = 0;
		sys_super_block->sb_recovery_meta.end_inode = 0;
		sys_super_block->sb_recovery_meta.last_recovery_ts = 0;
	}
};

TEST_F(set_recovery_flagTest, SetToOngoing)
{
	ino_t start = 10;
	ino_t end = 100;

	set_recovery_flag(TRUE, start, end);
	EXPECT_TRUE(sys_super_block->sb_recovery_meta.is_ongoing);
	EXPECT_EQ(start, sys_super_block->sb_recovery_meta.start_inode);
	EXPECT_EQ(end, sys_super_block->sb_recovery_meta.end_inode);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.last_recovery_ts);

	/* Ongoing already TRUE */
	set_recovery_flag(TRUE, start, end);
	EXPECT_TRUE(sys_super_block->sb_recovery_meta.is_ongoing);
	EXPECT_EQ(start, sys_super_block->sb_recovery_meta.start_inode);
	EXPECT_EQ(end, sys_super_block->sb_recovery_meta.end_inode);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.last_recovery_ts);
}

TEST_F(set_recovery_flagTest, SetToNotOngoing)
{
	ino_t start = 10;
	ino_t end = 100;

	set_recovery_flag(FALSE, start, end);
	EXPECT_FALSE(sys_super_block->sb_recovery_meta.is_ongoing);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.start_inode);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.end_inode);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.last_recovery_ts);

	/* Ongoing already TRUE */
	sys_super_block->sb_recovery_meta.is_ongoing = TRUE;
	set_recovery_flag(FALSE, start, end);
	EXPECT_FALSE(sys_super_block->sb_recovery_meta.is_ongoing);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.start_inode);
	EXPECT_EQ(0, sys_super_block->sb_recovery_meta.end_inode);
	EXPECT_FALSE((sys_super_block->sb_recovery_meta.last_recovery_ts == 0));
}
/* End unittest for set_recovery_flag */

/* Unittest for reset_queue_and_stat */
class reset_queue_and_statTest : public ::testing::Test
{
        protected:

	void SetUp()
	{
		sys_super_block->head.num_dirty = 100;
		sys_super_block->head.first_dirty_inode = 100;
		sys_super_block->head.last_dirty_inode = 100;
		hcfs_system->systemdata.dirty_cache_size = 100;
		hcfs_system->systemdata.unpin_dirty_data_size = 100;
	}

	void TearDown()
	{
		sys_super_block->head.num_dirty = 0;
		sys_super_block->head.first_dirty_inode = 0;
		sys_super_block->head.last_dirty_inode = 0;
		hcfs_system->systemdata.dirty_cache_size = 0;
		hcfs_system->systemdata.unpin_dirty_data_size = 0;
	}
};

TEST_F(reset_queue_and_statTest, Success)
{
	ASSERT_EQ(TRUE, sys_super_block->head.num_dirty > 0);
	ASSERT_EQ(TRUE, sys_super_block->head.first_dirty_inode > 0);
	ASSERT_EQ(TRUE, sys_super_block->head.last_dirty_inode > 0);
	ASSERT_EQ(TRUE, hcfs_system->systemdata.dirty_cache_size > 0);
	ASSERT_EQ(TRUE, hcfs_system->systemdata.unpin_dirty_data_size > 0);

	reset_queue_and_stat();

	EXPECT_EQ(0, sys_super_block->head.num_dirty);
	EXPECT_EQ(0, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(0, sys_super_block->head.last_dirty_inode);
	EXPECT_EQ(0, hcfs_system->systemdata.dirty_cache_size);
	EXPECT_EQ(0, hcfs_system->systemdata.unpin_dirty_data_size);
}
/* End unittest for reset_queue_and_stat */

/* Unittest for _batch_read_sb_entries */
class _batch_read_sb_entriesTest : public ::testing::Test
{
        protected:

        void SetUp() {}

	void TearDown()
	{
		close(sys_super_block->iofptr);
		unlink(SUPERBLOCK);
	}
};

TEST_F(_batch_read_sb_entriesTest, Success)
{
	int32_t idx, ret_code;
	int64_t num_entries = 999;
	SUPER_BLOCK_ENTRY entry_array[num_entries];

	ASSERT_EQ(0, _create_mock_sb_file(NULL, num_entries));
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	ret_code =
	    _batch_read_sb_entries(entry_array, MIN_INODE_NO, num_entries);
	EXPECT_EQ(0, ret_code);

	for (idx = 0; idx < num_entries; idx++) {
		EXPECT_EQ(idx + MIN_INODE_NO, entry_array[idx].this_index);
	}
}

TEST_F(_batch_read_sb_entriesTest, ErrorInPread)
{
	int32_t ret_code;
	int64_t num_entries = 10;
	SUPER_BLOCK_ENTRY entry_array[num_entries];

	ASSERT_EQ(0, _create_mock_sb_file(NULL, num_entries));
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	pread_force_error = 1;
	ret_code =
	    _batch_read_sb_entries(entry_array, MIN_INODE_NO, num_entries);
	EXPECT_EQ(-EIO, ret_code);
	pread_force_error = 0;
}

TEST_F(_batch_read_sb_entriesTest, ErrorShortRead)
{
	int32_t ret_code;
	int64_t num_entries = 10;
	SUPER_BLOCK_ENTRY entry_array[num_entries];

	ASSERT_EQ(0, _create_mock_sb_file(NULL, num_entries));
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	ret_code =
	    _batch_read_sb_entries(entry_array, MIN_INODE_NO, num_entries + 1);
	EXPECT_EQ(-EIO, ret_code);
}
/* End unittest for _batch_read_sb_entries */

/* Unittest for _batch_write_sb_entries */
class _batch_write_sb_entriesTest : public ::testing::Test
{
        protected:

        void SetUp() {}

	void TearDown()
	{
		close(sys_super_block->iofptr);
		unlink(SUPERBLOCK);
	}
};

TEST_F(_batch_write_sb_entriesTest, Success)
{
	int32_t ret_code;
	int64_t num_entries = 10;
	int64_t idx, ino_offset, rsize;
	SUPER_BLOCK_ENTRY array2write[num_entries];
	SUPER_BLOCK_ENTRY array2read[num_entries];

	ASSERT_EQ(0, _create_mock_sb_file(NULL, 0));
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	/* Create mock data */
	ino_offset = 17;
	for (idx = 0; idx < num_entries; idx++) {
		array2write[idx].this_index = ino_offset + idx;
	}
	ret_code =
	    _batch_write_sb_entries(array2write, MIN_INODE_NO, num_entries);
	EXPECT_EQ(0, ret_code);

	/* Verify write */
	rsize = pread(sys_super_block->iofptr, array2read,
		      SB_ENTRY_SIZE * num_entries,
		      SB_HEAD_SIZE + (MIN_INODE_NO - 1) * SB_ENTRY_SIZE);

	EXPECT_TRUE(rsize == SB_ENTRY_SIZE * num_entries);
	EXPECT_EQ(0, memcmp(array2write, array2read, num_entries));
}

TEST_F(_batch_write_sb_entriesTest, ErrorInPwrite)
{
	int32_t ret_code;
	int64_t num_entries = 10;
	SUPER_BLOCK_ENTRY array2write[num_entries];

	ASSERT_EQ(0, _create_mock_sb_file(NULL, 0));
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	pwrite_force_error = 1;
	ret_code =
	    _batch_write_sb_entries(array2write, MIN_INODE_NO, num_entries);
	EXPECT_EQ(-EIO, ret_code);
	pwrite_force_error = 0;
}
/* End unittest for _batch_write_sb_entries */

/* Unittest for _read_regfile_meta */
class _read_regfile_metaTest : public ::testing::Test
{
        protected:
	char meta_file_path[strlen(MOCK_METAPATH) + 20];
	int64_t target_num_blocks = 99;

        void SetUp() {
		int64_t wsize;
		FILE *tmp_fp;
		FILE_STATS_TYPE tmp_fs_t;

		/* Create mock regfile meta */
		snprintf(meta_file_path, sizeof(meta_file_path), "%s/%s",
			 MOCK_METAPATH, "mock_reg_meta_file");

		tmp_fs_t.num_blocks = target_num_blocks;
		ASSERT_EQ(0, _create_mock_regfile_meta_file(meta_file_path,
							    &tmp_fs_t));
	}

        void TearDown() {
		unlink(meta_file_path);
		meta_cache_open_file_fake.call_count = 0;
		meta_cache_close_file_fake.call_count = 0;
	}
};

TEST_F(_read_regfile_metaTest, Success)
{
	META_CACHE_ENTRY_STRUCT tmp_entry;
	FILE_STATS_TYPE tmp_fs_t;

	tmp_entry.meta_opened = TRUE;
	tmp_entry.fptr = fopen(meta_file_path, "r+");
	ASSERT_TRUE(tmp_entry.fptr != NULL);

	EXPECT_EQ(0, _read_regfile_meta(&tmp_entry, &tmp_fs_t));
	EXPECT_EQ(target_num_blocks, tmp_fs_t.num_blocks);

	fclose(tmp_entry.fptr);
}

TEST_F(_read_regfile_metaTest, SuccessWithFileNotOpen)
{
	META_CACHE_ENTRY_STRUCT tmp_entry;
	FILE_STATS_TYPE tmp_fs_t;

	strcpy(meta_cache_open_file_realpath, meta_file_path);

	tmp_entry.meta_opened = FALSE;
	tmp_entry.fptr = NULL;

	EXPECT_EQ(0, _read_regfile_meta(&tmp_entry, &tmp_fs_t));
	EXPECT_EQ(target_num_blocks, tmp_fs_t.num_blocks);
	EXPECT_EQ(meta_cache_open_file_fake.call_count,
		  meta_cache_close_file_fake.call_count);

	fclose(tmp_entry.fptr);
}

TEST_F(_read_regfile_metaTest, ErrorInOpenMetaFile)
{
	META_CACHE_ENTRY_STRUCT tmp_entry;
	FILE_STATS_TYPE tmp_fs_t;

	strcpy(meta_cache_open_file_realpath, meta_file_path);

	tmp_entry.meta_opened = FALSE;
	tmp_entry.fptr = NULL;

	meta_cache_open_file_force_error = 1;
	EXPECT_EQ(-1, _read_regfile_meta(&tmp_entry, &tmp_fs_t));
	meta_cache_open_file_force_error = 0;
}

TEST_F(_read_regfile_metaTest, ErrorInPread)
{
	META_CACHE_ENTRY_STRUCT tmp_entry;
	FILE_STATS_TYPE tmp_fs_t;

	strcpy(meta_cache_open_file_realpath, meta_file_path);

	tmp_entry.meta_opened = FALSE;
	tmp_entry.fptr = NULL;

	pread_force_error = 1;
	EXPECT_EQ(-1, _read_regfile_meta(&tmp_entry, &tmp_fs_t));
	EXPECT_EQ(meta_cache_open_file_fake.call_count,
		  meta_cache_close_file_fake.call_count);
	pread_force_error = 0;
}
/* End unittest for _read_regfile_meta */

/* Unittest for reconstruct_sb_entries */
class reconstruct_sb_entriesTest : public ::testing::Test
{
	protected:
	char meta_file_path[strlen(MOCK_METAPATH) + 20];
	int64_t num_entries = MAX_NUM_ENTRY_HANDLE;
	SUPER_BLOCK_ENTRY *sb_entry_arr;
	META_CACHE_ENTRY_STRUCT **meta_cache_arr;
	RECOVERY_ROUND_DATA round_data;

	void SetUp()
	{
		snprintf(meta_file_path, sizeof(meta_file_path), "%s/%s",
			 MOCK_METAPATH, "mock_reg_meta_file");

		sb_entry_arr = (SUPER_BLOCK_ENTRY *)calloc(
		    1, sizeof(SUPER_BLOCK_ENTRY) * num_entries);
		ASSERT_TRUE(sb_entry_arr != NULL);

		meta_cache_arr = (META_CACHE_ENTRY_STRUCT **)calloc(
		    1, sizeof(META_CACHE_ENTRY_STRUCT *) * num_entries);
		ASSERT_TRUE(meta_cache_arr != NULL);

		memset(&round_data, 0, sizeof(RECOVERY_ROUND_DATA));
	}

	void TearDown()
	{
		free(sb_entry_arr);
		free(meta_cache_arr);
	}
};

#define VERIFY_ROUND_DATA(A, B, C, D, E, F)                                    \
	do {                                                                   \
		EXPECT_EQ(round_data.prev_last_entry.this_index, A);           \
		EXPECT_EQ(round_data.this_first_entry.this_index, B);          \
		EXPECT_EQ(round_data.this_last_entry.this_index, C);           \
		EXPECT_EQ(round_data.this_num_dirty, D);                       \
		EXPECT_EQ(round_data.dirty_size_delta, E);                     \
		EXPECT_EQ(round_data.unpin_dirty_size_delta, F);               \
	} while (0)

TEST_F(reconstruct_sb_entriesTest, ErrorInvalidArgs)
{
	SUPER_BLOCK_ENTRY tmp_sb_entry;
	META_CACHE_ENTRY_STRUCT tmp_meta_cache;
	META_CACHE_ENTRY_STRUCT *meta_cache_ptr = &tmp_meta_cache;
	RECOVERY_ROUND_DATA tmp_round_data;

	EXPECT_EQ(-EINVAL, reconstruct_sb_entries(NULL, &meta_cache_ptr, 1,
						  &tmp_round_data));
	EXPECT_EQ(-EINVAL, reconstruct_sb_entries(&tmp_sb_entry, NULL, 1,
						  &tmp_round_data));
	EXPECT_EQ(-EINVAL, reconstruct_sb_entries(
			       &tmp_sb_entry, &meta_cache_ptr,
			       MAX_NUM_ENTRY_HANDLE + 1, &tmp_round_data));
	EXPECT_EQ(-EINVAL, reconstruct_sb_entries(&tmp_sb_entry,
						  &meta_cache_ptr, 1, NULL));
}

TEST_F(reconstruct_sb_entriesTest, ErrorInReadSBEntry)
{
	int32_t fake_retcode = -EIO;

	sys_super_block->head.last_dirty_inode = 999;

	read_super_block_entry_force_error = 1;
	EXPECT_EQ(fake_retcode,
		  reconstruct_sb_entries(sb_entry_arr, meta_cache_arr, 1,
					 &round_data));
	read_super_block_entry_force_error = 0;
	sys_super_block->head.last_dirty_inode = 0;
}

TEST_F(reconstruct_sb_entriesTest, SuccessZeroNumEntryHandle)
{
	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr, 0,
					    &round_data));
	VERIFY_ROUND_DATA(0, 0, 0, 0, 0, 0);
}

TEST_F(reconstruct_sb_entriesTest, SuccessZeroDirtyEntry)
{
	_create_mock_sb_entry_arr(sb_entry_arr, num_entries, FALSE, NO_LL,
				  ST_PIN);

	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					    num_entries, &round_data));
	VERIFY_ROUND_DATA(0, 0, 0, 0, 0, 0);
}

TEST_F(reconstruct_sb_entriesTest, SuccessNoRegFile)
{
	int64_t count;

	_create_mock_sb_entry_arr(sb_entry_arr, num_entries, FALSE, IS_DIRTY,
				  ST_PIN);
	_create_mock_meta_cache_arr(meta_cache_arr, num_entries, TRUE);

	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					    num_entries, &round_data));

	VERIFY_ROUND_DATA(0, sb_entry_arr[0].this_index,
			  sb_entry_arr[num_entries - 1].this_index, num_entries,
			  round_size(MOCK_DIRTY_META_SIZE) * num_entries, 0);

	/* Verify link between entries */
	for (count = 0; count < num_entries; count++) {
		if (count == 0)
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_prev);
		else
			EXPECT_EQ(count + MIN_INODE_NO - 1,
				  sb_entry_arr[count].util_ll_prev);

		if (count == (num_entries - 1))
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_next);
		else
			EXPECT_EQ(count + MIN_INODE_NO + 1,
				  sb_entry_arr[count].util_ll_next);
	}
}

TEST_F(reconstruct_sb_entriesTest, SuccessWithRegFile)
{
	int64_t count;
	FILE_STATS_TYPE tmp_fs_t;

	_create_mock_sb_entry_arr(sb_entry_arr, num_entries, FALSE, IS_DIRTY,
				  ST_PIN);

	/* Mock regfile meta */
	tmp_fs_t.dirty_data_size = MOCK_DIRTY_DATA_SIZE;
	ASSERT_EQ(0, _create_mock_regfile_meta_file(meta_file_path, &tmp_fs_t));

	mock_meta_cache_entry.fptr = fopen(meta_file_path, "r+");
	mock_meta_cache_entry.meta_opened = TRUE;
	ASSERT_TRUE(mock_meta_cache_entry.fptr != NULL);
	_create_mock_meta_cache_arr(meta_cache_arr, num_entries, FALSE);

	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					    num_entries, &round_data));

	VERIFY_ROUND_DATA(0, sb_entry_arr[0].this_index,
			  sb_entry_arr[num_entries - 1].this_index, num_entries,
			  round_size(MOCK_DIRTY_META_SIZE) * num_entries +
			      round_size(MOCK_DIRTY_META_SIZE) * num_entries,
			  0);

	/* Verify link between entries */
	for (count = 0; count < num_entries; count++) {
		if (count == 0)
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_prev);
		else
			EXPECT_EQ(count + MIN_INODE_NO - 1,
				  sb_entry_arr[count].util_ll_prev);

		if (count == (num_entries - 1))
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_next);
		else
			EXPECT_EQ(count + MIN_INODE_NO + 1,
				  sb_entry_arr[count].util_ll_next);
	}

	fclose(mock_meta_cache_entry.fptr);
	mock_meta_cache_entry.fptr = NULL;
	mock_meta_cache_entry.meta_opened = FALSE;
	unlink(meta_file_path);
}

TEST_F(reconstruct_sb_entriesTest, SuccessWithUnpinRegFile)
{
	int64_t count;
	FILE_STATS_TYPE tmp_fs_t;

	_create_mock_sb_entry_arr(sb_entry_arr, num_entries, FALSE, IS_DIRTY,
				  ST_UNPIN);

	/* Mock regfile meta */
	tmp_fs_t.dirty_data_size = MOCK_DIRTY_DATA_SIZE;
	ASSERT_EQ(0, _create_mock_regfile_meta_file(meta_file_path, &tmp_fs_t));

	mock_meta_cache_entry.fptr = fopen(meta_file_path, "r+");
	mock_meta_cache_entry.meta_opened = TRUE;
	ASSERT_TRUE(mock_meta_cache_entry.fptr != NULL);
	_create_mock_meta_cache_arr(meta_cache_arr, num_entries, FALSE);

	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					    num_entries, &round_data));

	VERIFY_ROUND_DATA(0, sb_entry_arr[0].this_index,
			  sb_entry_arr[num_entries - 1].this_index, num_entries,
			  round_size(MOCK_DIRTY_META_SIZE) * num_entries +
			      round_size(MOCK_DIRTY_DATA_SIZE) * num_entries,
			  round_size(MOCK_DIRTY_DATA_SIZE) * num_entries);

	/* Verify link between entries */
	for (count = 0; count < num_entries; count++) {
		if (count == 0)
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_prev);
		else
			EXPECT_EQ(count + MIN_INODE_NO - 1,
				  sb_entry_arr[count].util_ll_prev);

		if (count == (num_entries - 1))
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_next);
		else
			EXPECT_EQ(count + MIN_INODE_NO + 1,
				  sb_entry_arr[count].util_ll_next);
	}

	fclose(mock_meta_cache_entry.fptr);
	mock_meta_cache_entry.fptr = NULL;
	mock_meta_cache_entry.meta_opened = FALSE;
	unlink(meta_file_path);
}

TEST_F(reconstruct_sb_entriesTest, SuccessLastDirtyInoExisted)
{
	int64_t count;
	FILE_STATS_TYPE tmp_fs_t;

	sys_super_block->head.last_dirty_inode = 999;

	_create_mock_sb_entry_arr(sb_entry_arr, num_entries, FALSE, IS_DIRTY,
				  ST_PIN);

	_create_mock_meta_cache_arr(meta_cache_arr, num_entries, TRUE);

	EXPECT_EQ(0, reconstruct_sb_entries(sb_entry_arr, meta_cache_arr,
					    num_entries, &round_data));

	VERIFY_ROUND_DATA(sys_super_block->head.last_dirty_inode,
			  sb_entry_arr[0].this_index,
			  sb_entry_arr[num_entries - 1].this_index, num_entries,
			  round_size(MOCK_DIRTY_META_SIZE) * num_entries, 0);

	/* Verify link between entries */
	for (count = 0; count < num_entries; count++) {
		if (count == 0)
			EXPECT_EQ(sys_super_block->head.last_dirty_inode,
				  sb_entry_arr[count].util_ll_prev);
		else
			EXPECT_EQ(count + MIN_INODE_NO - 1,
				  sb_entry_arr[count].util_ll_prev);

		if (count == (num_entries - 1))
			EXPECT_EQ(0, sb_entry_arr[count].util_ll_next);
		else
			EXPECT_EQ(count + MIN_INODE_NO + 1,
				  sb_entry_arr[count].util_ll_next);
	}

	sys_super_block->head.last_dirty_inode = 0;
}
/* End unittest for reconstruct_sb_entries */

/* Unittest for update_reconstruct_result */
class update_reconstruct_resultTest : public ::testing::Test
{
	protected:
	int64_t num_dirty = 10;
	int64_t dirty_size = 1024;
	int64_t unpin_dirty_size = 1024 * 2;
	ino_t start = 100;
	ino_t end = 110;
	RECOVERY_ROUND_DATA round_data;

	void SetUp()
	{
		round_data.this_first_entry.this_index = start;
		round_data.this_last_entry.this_index = end;
		round_data.this_num_dirty = num_dirty;
		round_data.dirty_size_delta = dirty_size;
		round_data.unpin_dirty_size_delta = unpin_dirty_size;
	}

	void TearDown()
	{
		sys_super_block->head.first_dirty_inode = 0;
		sys_super_block->head.last_dirty_inode = 0;
		sys_super_block->head.num_dirty = 0;
		hcfs_system->systemdata.dirty_cache_size = 0;
		hcfs_system->systemdata.unpin_dirty_data_size = 0;
	}
};

#define ASSERT_SB_N_STAT_RESET(A, B, C, D, E)                                  \
	do {                                                                   \
		ASSERT_EQ(sys_super_block->head.first_dirty_inode, A);         \
		ASSERT_EQ(sys_super_block->head.last_dirty_inode, B);          \
		ASSERT_EQ(sys_super_block->head.num_dirty, C);                 \
		ASSERT_EQ(hcfs_system->systemdata.dirty_cache_size, D);        \
		ASSERT_EQ(hcfs_system->systemdata.unpin_dirty_data_size, E);   \
	} while (0)

#define VERIFY_SB_N_STAT(A, B, C, D, E)                                        \
	do {                                                                   \
		EXPECT_EQ(sys_super_block->head.first_dirty_inode, A);         \
		EXPECT_EQ(sys_super_block->head.last_dirty_inode, B);          \
		EXPECT_EQ(sys_super_block->head.num_dirty, C);                 \
		EXPECT_EQ(hcfs_system->systemdata.dirty_cache_size, D);        \
		EXPECT_EQ(hcfs_system->systemdata.unpin_dirty_data_size, E);   \
	} while (0)

TEST_F(update_reconstruct_resultTest, Success)
{
	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);

	EXPECT_EQ(0, update_reconstruct_result(round_data));

	VERIFY_SB_N_STAT(start, end, num_dirty, dirty_size, unpin_dirty_size);
}

TEST_F(update_reconstruct_resultTest, SuccessLastInodeExisted)
{
	int64_t now_last_dirty = 5;

	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);

	sys_super_block->head.first_dirty_inode = now_last_dirty;
	/* Already have dirty entries in SB */
	round_data.prev_last_entry.this_index = now_last_dirty;
	EXPECT_EQ(0, update_reconstruct_result(round_data));

	VERIFY_SB_N_STAT(now_last_dirty, end, num_dirty, dirty_size,
			 unpin_dirty_size);
}

TEST_F(update_reconstruct_resultTest, ErrorInWriteEuperBlockEntry)
{
	int64_t now_last_dirty = 5;

	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);
	write_super_block_entry_force_error = 1;

	/* Already have dirty entries in SB */
	round_data.prev_last_entry.this_index = now_last_dirty;
	EXPECT_EQ(-EIO, update_reconstruct_result(round_data));

	write_super_block_entry_force_error = 0;
	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);
}

TEST_F(update_reconstruct_resultTest, ErrorInWriteEuperBlockHead)
{
	int64_t now_last_dirty = 5;

	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);
	write_super_block_head_fake.return_val = -EIO;

	/* Already have dirty entries in SB */
	round_data.prev_last_entry.this_index = now_last_dirty;
	EXPECT_EQ(-EIO, update_reconstruct_result(round_data));

	RESET_FAKE(write_super_block_head);
	ASSERT_SB_N_STAT_RESET(0, 0, 0, 0, 0);
}
/* End unittest for update_reconstruct_result */

/* Unittest for recover_sb_queue_worker */
class recover_sb_queue_workerTest : public ::testing::Test
{
        protected:
	void *retval;
	int64_t num_entries;
	pthread_t worker_t;

	void SetUp()
	{
		RESET_FAKE(super_block_exclusive_locking);
		RESET_FAKE(super_block_exclusive_release);
	}

	void TearDown()
	{
		ASSERT_EQ(super_block_exclusive_locking_fake.call_count,
			  super_block_exclusive_release_fake.call_count);
	}
};

TEST_F(recover_sb_queue_workerTest, RecoveryAborted)
{
	sys_super_block->sb_recovery_meta.is_ongoing = TRUE;
	pthread_create(&worker_t, NULL, &recover_sb_queue_worker, NULL);
	EXPECT_EQ(0, pthread_join(worker_t, &retval));
	EXPECT_EQ(-1, *(int *)retval);
	sys_super_block->sb_recovery_meta.is_ongoing = FALSE;

	sys_super_block->sync_point_is_set = TRUE;
	pthread_create(&worker_t, NULL, &recover_sb_queue_worker, NULL);
	EXPECT_EQ(0, pthread_join(worker_t, &retval));
	EXPECT_EQ(-1, *(int *)retval);
	sys_super_block->sync_point_is_set = FALSE;

	hcfs_system->system_restoring = RESTORING_STAGE1;
	pthread_create(&worker_t, NULL, &recover_sb_queue_worker, NULL);
	EXPECT_EQ(0, pthread_join(worker_t, &retval));
	EXPECT_EQ(-1, *(int *)retval);
	hcfs_system->system_restoring = NOT_RESTORING;
}

TEST_F(recover_sb_queue_workerTest, Success)
{
	char meta_file_path[strlen(MOCK_METAPATH) + 20];
	int64_t num_entry_handle, idx, count;
	ino_t start_inode, end_inode;
	SUPER_BLOCK_ENTRY tmp_entry = {};
	SUPER_BLOCK_ENTRY sb_entry_array[MAX_NUM_ENTRY_HANDLE];
	FILE_STATS_TYPE tmp_fs_t;

	num_entries = (MAX_NUM_ENTRY_HANDLE - 1) * 99;

	sys_super_block->head.num_total_inodes = num_entries;
	_create_mock_sb_file(NULL, num_entries);
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

	snprintf(meta_file_path, sizeof(meta_file_path), "%s/%s", MOCK_METAPATH,
		 "mock_reg_meta_file");
	tmp_fs_t.dirty_data_size = MOCK_DIRTY_DATA_SIZE;
	ASSERT_EQ(0, _create_mock_regfile_meta_file(meta_file_path, &tmp_fs_t));

	memset(&mock_meta_cache_entry, 0, sizeof(mock_meta_cache_entry));
	mock_meta_cache_entry.this_stat.mode = S_IFREG;
	mock_meta_cache_entry.meta_opened = TRUE;
	mock_meta_cache_entry.fptr = fopen(meta_file_path, "r+");

	write_super_block_entry_use_real = 1;
	pthread_create(&worker_t, NULL, &recover_sb_queue_worker, NULL);
	EXPECT_EQ(0, pthread_join(worker_t, &retval));
	EXPECT_EQ(0, *(int *)retval);
	write_super_block_entry_use_real = 0;

	/* Verify */
	EXPECT_EQ(MIN_INODE_NO, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(MIN_INODE_NO + num_entries - 1,
		  sys_super_block->head.last_dirty_inode);
	EXPECT_EQ(num_entries, sys_super_block->head.num_dirty);
	EXPECT_EQ(round_size(MOCK_DIRTY_META_SIZE) * num_entries +
		      round_size(MOCK_DIRTY_DATA_SIZE) * num_entries,
		  hcfs_system->systemdata.dirty_cache_size);
	EXPECT_EQ(round_size(MOCK_DIRTY_DATA_SIZE) * num_entries,
		  hcfs_system->systemdata.unpin_dirty_data_size);

	start_inode = MIN_INODE_NO;
	end_inode = sys_super_block->head.num_total_inodes + MIN_INODE_NO - 1;
	while (start_inode < end_inode) {
		if ((start_inode + MAX_NUM_ENTRY_HANDLE) <= end_inode)
			num_entry_handle = MAX_NUM_ENTRY_HANDLE;
		else
			num_entry_handle = end_inode - start_inode + 1;

		pread(sys_super_block->iofptr, sb_entry_array,
		      SB_ENTRY_SIZE * num_entry_handle,
		      SB_HEAD_SIZE + (start_inode - 1) * SB_ENTRY_SIZE);

		for (idx = 0; idx < num_entry_handle; idx++) {
			if (sb_entry_array[idx].this_index !=
			    sys_super_block->head.first_dirty_inode)
				EXPECT_EQ(tmp_entry.util_ll_next,
					  sb_entry_array[idx].this_index);
			EXPECT_EQ(tmp_entry.this_index,
				  sb_entry_array[idx].util_ll_prev);
			memcpy(&tmp_entry, sb_entry_array + idx,
			       sizeof(tmp_entry));
		}
		start_inode += num_entry_handle;
	}
	EXPECT_EQ(tmp_entry.util_ll_next, 0);

	/* cleanup */
	close(sys_super_block->iofptr);
	fclose(mock_meta_cache_entry.fptr);
	mock_meta_cache_entry.fptr = NULL;
	mock_meta_cache_entry.meta_opened = FALSE;
	unlink(meta_file_path);
	unlink(SUPERBLOCK);

	sys_super_block->head.first_dirty_inode = 0;
	sys_super_block->head.last_dirty_inode = 0;
	sys_super_block->head.num_dirty = 0;
	hcfs_system->systemdata.dirty_cache_size = 0;
	hcfs_system->systemdata.unpin_dirty_data_size = 0;
}
/* End unittest for recover_sb_queue_worker */

