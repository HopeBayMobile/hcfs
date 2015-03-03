#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "mock_param.h"

#include "utils.h"
#include "global.h"
#include "hfuse_system.h"
#include "fuseop.h"
#include "params.h"
#include "metaops.h"
}
#include "gtest/gtest.h"

/* Common Vars  */
static const ino_t self_inode = 10;
static const ino_t parent_inode = 5;

// Tests non-existing file
TEST(init_dir_pageTest, InitOK) {

        long long pos = 1000;

	DIR_ENTRY_PAGE *temppage = (DIR_ENTRY_PAGE*)malloc(sizeof(DIR_ENTRY_PAGE));

	EXPECT_EQ(0, init_dir_page(temppage, self_inode, parent_inode, pos));

        /* Test */
	EXPECT_EQ(2, temppage->num_entries);

	EXPECT_EQ(self_inode, (temppage->dir_entries[0]).d_ino);
	EXPECT_STREQ(".", (temppage->dir_entries[0]).d_name);
	EXPECT_EQ(D_ISDIR, (temppage->dir_entries[0]).d_type);

	EXPECT_EQ(parent_inode, (temppage->dir_entries[1]).d_ino);
	EXPECT_STREQ("..", (temppage->dir_entries[1]).d_name);
	EXPECT_EQ(D_ISDIR, (temppage->dir_entries[1]).d_type);

	EXPECT_EQ(pos, temppage->this_page_pos);

	free(temppage);
}


class dir_add_entryTest : public ::testing::Test {
	protected:

		char *self_name;

		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {
                        self_name = "selfname";

			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
                }

                virtual void TearDown() {
			free(body_ptr);
                }
};

TEST_F(dir_add_entryTest, NoLockError) {
	EXPECT_EQ(-1, dir_add_entry(parent_inode, self_inode, self_name, S_IFMT, body_ptr));
}
/* To be continued... */


class change_parent_inodeTest : public ::testing::Test {
	protected:

		ino_t parent_inode2;

		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {
			parent_inode2 = 6;

			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		}

                virtual void TearDown() {
			free(body_ptr);
		}
};
TEST_F(change_parent_inodeTest, ChangeOK) {
	EXPECT_EQ(0, change_parent_inode(INO_SEEK_DIR_ENTRY_OK, parent_inode, parent_inode2, body_ptr));
}
TEST_F(change_parent_inodeTest, DirEntryNotFound) {
	EXPECT_EQ(-1, change_parent_inode(INO_SEEK_DIR_ENTRY_NOTFOUND, parent_inode, parent_inode2, body_ptr));
}
TEST_F(change_parent_inodeTest, ChangeFail) {
	EXPECT_EQ(-1, change_parent_inode(INO_SEEK_DIR_ENTRY_FAIL, parent_inode, parent_inode2, body_ptr));
}


class decrease_nlink_inode_fileTest : public ::testing::Test {
	protected:

		virtual void SetUp() {
		
			hcfs_system = (SYSTEM_DATA_HEAD*)malloc(sizeof(SYSTEM_DATA_HEAD));
			sem_init(&(hcfs_system->access_sem), 0, 1);
			hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
			hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
			hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;

		}

		virtual void TearDown() {
		
		}
};
TEST_F(decrease_nlink_inode_fileTest, InodeStillReferenced) {
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2));
}
TEST_F(decrease_nlink_inode_fileTest, NoBlockFilesToDel) {
	char metapath[METAPATHLEN];
	char thisblockpath[400];

	FILE *tmp_file;

	fetch_meta_path(metapath, INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel);
	tmp_file = fopen(metapath, "w");
	fclose(tmp_file);
	
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel));
	EXPECT_EQ(MOCK_SYSTEM_SIZE, hcfs_system->systemdata.system_size);
}
TEST_F(decrease_nlink_inode_fileTest, BlockFilesToDel) {
	char metapath[METAPATHLEN];
	char thisblockpath[400];

	FILE *tmp_file;

	fetch_meta_path(metapath, INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel);
	tmp_file = fopen(metapath, "w");
	fclose(tmp_file);
	
	for (int i=0; i<10; i++) {
		fetch_block_path(thisblockpath, INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel, i);
		tmp_file = fopen(thisblockpath, "w");
		fclose(tmp_file);
	}

	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel));
	EXPECT_EQ(MOCK_SYSTEM_SIZE, hcfs_system->systemdata.system_size);
}


class seek_pageTest : public ::testing::Test {
	protected:

		char *metapath;
		long long target_page;

		FH_ENTRY *fh_ptr;
		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {
			metapath = "testpatterns/seek_page_meta_file";

			target_page = 0;

			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			body_ptr->fptr = fopen(metapath, "w+");
			setbuf(body_ptr->fptr, NULL);
			body_ptr->meta_opened = TRUE;

			fh_ptr = (FH_ENTRY*)malloc(sizeof(FH_ENTRY));
			fh_ptr->thisinode = self_inode;
			fh_ptr->meta_cache_ptr = body_ptr;
		}

                virtual void TearDown() {
			fclose(body_ptr->fptr);
			free(fh_ptr);
			free(body_ptr);
			remove(metapath);
		}
};
TEST_F(seek_pageTest, NoLockError) {
	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
}
TEST_F(seek_pageTest, LookupFileDataFailed) {
	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_FAIL;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
	sem_post(&(body_ptr->access_sem));
}
TEST_F(seek_pageTest, UpdateFileDataFailed) {
	fh_ptr->thisinode = INO_UPDATE_FILE_DATA_FAIL;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
	sem_post(&(body_ptr->access_sem));
}
TEST_F(seek_pageTest, TargetPageExisted) {
	target_page = 5;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK;
	sem_wait(&(body_ptr->access_sem));

	/* Test */
	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	/* Assume the position of first page is 0 */
	EXPECT_EQ((target_page+1)*sizeof(BLOCK_ENTRY_PAGE), fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}
/* Test for target block page isn't generated */
TEST_F(seek_pageTest, TargetPageNotExisted) {
	target_page = 10;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	EXPECT_EQ((target_page+1)*sizeof(BLOCK_ENTRY_PAGE), fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}
/* Test for first block page isn't generated */
TEST_F(seek_pageTest, FirstPageNotExisted) {
	target_page = 0;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	EXPECT_EQ(0, fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}


class advance_blockTest : public ::testing::Test {
	protected:

		char *metapath;
		off_t thisfilepos;
		long long entry_index;		

		BLOCK_ENTRY_PAGE *testpage1;
		BLOCK_ENTRY_PAGE *testpage2;

		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {

			FILE *fp;

			metapath = "testpatterns/advance_block_meta_file";
			thisfilepos = 0;
			entry_index = 0;

			testpage1 = (BLOCK_ENTRY_PAGE*)malloc(sizeof(BLOCK_ENTRY_PAGE));
			testpage2 = (BLOCK_ENTRY_PAGE*)malloc(sizeof(BLOCK_ENTRY_PAGE));

			testpage1->next_page = sizeof(BLOCK_ENTRY_PAGE);
			testpage2->next_page = 0;
			
			/* Create mock meta file */
			fp = fopen(metapath, "wb");
			fwrite(testpage1, sizeof(BLOCK_ENTRY_PAGE), 1, fp);
			fwrite(testpage2, sizeof(BLOCK_ENTRY_PAGE), 1, fp);
			fclose(fp);

			/* Open meta file  */
			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			body_ptr->fptr = fopen(metapath, "r+");
			setbuf(body_ptr->fptr, NULL);
			body_ptr->meta_opened = TRUE;

		}

                virtual void TearDown() {
			fclose(body_ptr->fptr);
			free(body_ptr);
			free(testpage1);
			free(testpage2);
			remove(metapath);
		}
};
TEST_F(advance_blockTest, FilePosNOTChange) {
	long long temp_index = 0;

	EXPECT_EQ(thisfilepos, advance_block(body_ptr, thisfilepos, &temp_index));
	EXPECT_EQ((entry_index + 1), temp_index);
}
TEST_F(advance_blockTest, AdvanceOK) {
	long long temp_index = MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(body_ptr->fptr), LOCK_EX);

	EXPECT_EQ(sizeof(BLOCK_ENTRY_PAGE), advance_block(body_ptr, thisfilepos, &temp_index));
	EXPECT_EQ(0, temp_index);
}
TEST_F(advance_blockTest, AllocatePageNeeded) {
	long long temp_index = MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(body_ptr->fptr), LOCK_EX);

	EXPECT_EQ(2*sizeof(BLOCK_ENTRY_PAGE), advance_block(body_ptr, sizeof(BLOCK_ENTRY_PAGE), &temp_index));
	EXPECT_EQ(0, temp_index);
}
