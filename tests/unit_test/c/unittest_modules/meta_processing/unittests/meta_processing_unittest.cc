#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
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
