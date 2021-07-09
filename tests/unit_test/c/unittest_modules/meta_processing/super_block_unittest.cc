/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
extern "C" {
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include "mock_param.h"
#include "super_block.h"
#include "global.h"
#include "fuseop.h"
#include "utils.h"
}
#include "super_block_mock_params.h"
#include "gtest/gtest.h"

extern SYSTEM_DATA_HEAD *hcfs_system;
extern SYSTEM_CONF_STRUCT *system_config;

class superblockEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));

			system_config->max_cache_limit =
				(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));

			system_config->max_pinned_limit =
				(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
			META_SPACE_LIMIT = 12345;

			hcfs_system = (SYSTEM_DATA_HEAD *)
					malloc(sizeof(SYSTEM_DATA_HEAD));
			memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
			//sem_init(&(hcfs_system->access_sem), 0, 1);
		}
		void TearDown()
		{
			free(hcfs_system);
			free(system_config->max_cache_limit);
			free(system_config->max_pinned_limit);
			free(system_config);
		}
};

::testing::Environment* const metaops_env =
	::testing::AddGlobalTestEnvironment(new superblockEnvironment);

class MallocSuperBlockBaseClass : public ::testing::Test {
protected:
	char *sb_path;

	void SetUp()
	{
		sb_path = "testpatterns/mock_super_block";
		sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	}
	void TearDown()
	{
		free(sys_super_block);
	}
};

/*
	Unittest of write_super_block_head()
 */

class write_super_block_headTest : public MallocSuperBlockBaseClass {
};

TEST_F(write_super_block_headTest, WriteSuperBlockFail)
{
	/* Mock fd to make failure since no such file */
	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, write_super_block_head());

	close(sys_super_block->iofptr);
}

TEST_F(write_super_block_headTest, WriteSuperBlockSUCCESS)
{
	sys_super_block->iofptr = open(sb_path, O_CREAT | O_RDWR, 0600);

	/* Run */
	EXPECT_EQ(0, write_super_block_head());

	close(sys_super_block->iofptr);
	unlink(sb_path);
}

/*
	End of unittest of write_super_block_head()
 */

/*
	Unittest of read_super_block_entry()
 */
class read_super_block_entryTest : public MallocSuperBlockBaseClass {
};

TEST_F(read_super_block_entryTest, ReadEntryFail)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 7;

	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, read_super_block_entry(inode, &sb_entry));

	close(sys_super_block->iofptr);
}

TEST_F(read_super_block_entryTest, ReadEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 2, i;
	FILE *ptr;

	/* Write mock sb head & sb entry */
	ptr = fopen(sb_path, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (i = 0 ; i < inode - 1 ; i++)
		fwrite(&expected_sb_entry, sizeof(SUPER_BLOCK_ENTRY), 1, ptr);
	expected_sb_entry.util_ll_next = 123;
	expected_sb_entry.util_ll_prev = 456;
	expected_sb_entry.status = TO_BE_DELETED;
	expected_sb_entry.in_transit = TRUE;
	expected_sb_entry.mod_after_in_transit = TRUE;
	expected_sb_entry.this_index = 78;
	expected_sb_entry.generation = 9;
	fwrite(&expected_sb_entry, sizeof(SUPER_BLOCK_ENTRY), 1, ptr);
	fclose(ptr);

	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(0, read_super_block_entry(inode, &sb_entry));

	/* Verify */
	EXPECT_EQ(0, memcmp(&expected_sb_entry, &sb_entry, sizeof(SUPER_BLOCK_ENTRY)));
	close(sys_super_block->iofptr);
	unlink(sb_path);
}

/*
	End of unittest of read_super_block_entry()
 */

/*
	Unitest of write_super_block_entry()
 */

class write_super_block_entryTest : public MallocSuperBlockBaseClass {
};

TEST_F(write_super_block_entryTest, WriteEntryFail)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 2;

	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, write_super_block_entry(inode, &sb_entry));

	close(sys_super_block->iofptr);
}

TEST_F(write_super_block_entryTest, WriteEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 7, i;
	FILE *ptr;

	/* Write mock sb head & sb entry */
	ptr = fopen(sb_path, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (i = 0 ; i < inode ; i++)
		fwrite(&expected_sb_entry, sizeof(SUPER_BLOCK_ENTRY), 1, ptr);
	fclose(ptr);

	sys_super_block->iofptr = open(sb_path, O_RDWR, 0600);

	expected_sb_entry.util_ll_next = 123;
	expected_sb_entry.util_ll_prev = 456;
	expected_sb_entry.status = TO_BE_DELETED;
	expected_sb_entry.in_transit = TRUE;
	expected_sb_entry.mod_after_in_transit = TRUE;
	expected_sb_entry.this_index = 78;
	expected_sb_entry.generation = 9;

	/* Run */
	EXPECT_EQ(0, write_super_block_entry(inode, &expected_sb_entry));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) * sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&expected_sb_entry, &sb_entry, sizeof(SUPER_BLOCK_ENTRY)));
	close(sys_super_block->iofptr);
	unlink(sb_path);
}
/*
	End of unitest of write_super_block_entry()
 */

/*
	Unittest of super_block_init()
 */

TEST(super_block_initTest, InitSuccess)
{
	char *sb_path = "testpatterns/sb_path";
	char *unclaimedfile_path = "testpatterns/unclaimedfile_path";

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;

	/* Run */
	EXPECT_EQ(0, super_block_init());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}

/*
	End of unittest of super_block_init()
 */

/*
	Unittest of super_block_destroy()
 */

/* A base class used to init superblock and run super_block_init() */
class InitSuperBlockBaseClass : public ::testing::Test {
protected:
	void SetUp()
	{
		SUPERBLOCK = "testpatterns/sb_path";
		UNCLAIMEDFILE = "testpatterns/unclaimedfile_path";
		mock_super_block_init();
		memset(&(hcfs_system->systemdata), 0, sizeof(SYSTEM_DATA_TYPE));
	}

	void TearDown()
	{
		if (sys_super_block->iofptr > 0)
			close(sys_super_block->iofptr);
		free(sys_super_block);
		unlink(SUPERBLOCK);
		unlink(UNCLAIMEDFILE);
		memset(&(hcfs_system->systemdata), 0, sizeof(SYSTEM_DATA_TYPE));
	}

	void mock_super_block_init()
	{
		sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));

		memset(sys_super_block, 0, sizeof(SUPER_BLOCK_CONTROL));
		sem_init(&(sys_super_block->exclusive_lock_sem), 1, 1);
		sem_init(&(sys_super_block->share_lock_sem), 1, 1);
		sem_init(&(sys_super_block->share_CR_lock_sem), 1, 1);
		sys_super_block->share_counter = 0;

		sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);

		if (sys_super_block->iofptr < 0) {
			sys_super_block->iofptr = open(SUPERBLOCK, O_CREAT | O_RDWR,
					0600);
			pwrite(sys_super_block->iofptr, &(sys_super_block->head),
					sizeof(SUPER_BLOCK_HEAD), 0);
			close(sys_super_block->iofptr);
			sys_super_block->iofptr = open(SUPERBLOCK, O_RDWR);
		}
		sys_super_block->unclaimed_list_fptr = fopen(UNCLAIMEDFILE, "a+");
		setbuf(sys_super_block->unclaimed_list_fptr, NULL);

		pread(sys_super_block->iofptr, &(sys_super_block->head),
				sizeof(SUPER_BLOCK_HEAD), 0);
	}
};

class super_block_destroyTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_destroyTest, DestroySuccess)
{
	SUPER_BLOCK_HEAD expected_head, actual_result;

	/* Mock init super block */
	expected_head.num_inode_reclaimed = 5;
	expected_head.num_to_be_reclaimed = 6;
	expected_head.num_to_be_deleted = 7;
	expected_head.num_dirty = 8;
	expected_head.num_block_cached = 9;
	expected_head.num_total_inodes = 95;

	memcpy(&(sys_super_block->head), &expected_head, sizeof(SUPER_BLOCK_HEAD));

	/* Run */
	EXPECT_EQ(0, super_block_destroy());

	/* Verify */
	sys_super_block->iofptr = open(SUPERBLOCK, O_RDONLY, 0600);
	EXPECT_TRUE(sys_super_block->iofptr > 0);
	pread(sys_super_block->iofptr, &actual_result, sizeof(SUPER_BLOCK_HEAD), 0);
	EXPECT_EQ(0, memcmp(&(expected_head), &actual_result, sizeof(SUPER_BLOCK_HEAD)));
}

/*
	End of unittest of super_block_destroy()
 */

/*
	Unittest of super_block_read()
 */

class super_block_readTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_readTest, ReadEntryFail)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 7;

	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_read(inode, &sb_entry));
}

TEST_F(super_block_readTest, ReadEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 2, i;
	FILE *ptr;

	/* Write mock sb head & sb entry */
	close(sys_super_block->iofptr);

	ptr = fopen(SUPERBLOCK, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (i = 0 ; i < inode - 1 ; i++)
		fwrite(&expected_sb_entry, sizeof(SUPER_BLOCK_ENTRY), 1, ptr);
	expected_sb_entry.util_ll_next = 123;
	expected_sb_entry.util_ll_prev = 456;
	expected_sb_entry.status = TO_BE_DELETED;
	expected_sb_entry.in_transit = TRUE;
	expected_sb_entry.mod_after_in_transit = TRUE;
	expected_sb_entry.this_index = 78;
	expected_sb_entry.generation = 9;
	fwrite(&expected_sb_entry, sizeof(SUPER_BLOCK_ENTRY), 1, ptr);
	fclose(ptr);

	sys_super_block->iofptr = open(SUPERBLOCK, O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(0, super_block_read(inode, &sb_entry));

	/* Verify */
	EXPECT_EQ(0, memcmp(&expected_sb_entry, &sb_entry, sizeof(SUPER_BLOCK_ENTRY)));
}

/*
	End of unittest of super_block_read()
 */

/*
	Unittest of super_block_write()
 */

class super_block_writeTest : public InitSuperBlockBaseClass {
	/* Do not need to let write_super_block_entry() success */
};

TEST_F(super_block_writeTest, WriteEntryFail)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = FALSE;

	/* Run */
	EXPECT_EQ(-EBADF, super_block_write(inode, &sb_entry));
}

TEST_F(super_block_writeTest, AddDirtyNode_Dequeue_Enqueue_andWriteHeadFail)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);
	sb_entry.status = NO_LL;
	sb_entry.in_transit = FALSE;
	EXPECT_EQ(0, sys_super_block->head.num_dirty);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_write(inode, &sb_entry));

	/* Verify that # of dirty nodes = 1, which means addition success. */
	EXPECT_EQ(1, sys_super_block->head.num_dirty);
}

TEST_F(super_block_writeTest, ModAfterTransit)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;

	/* Run */
	EXPECT_EQ(-EBADF, super_block_write(inode, &sb_entry));

	/* Verify that modify after transitting is set as true. */
	EXPECT_EQ(TRUE, sb_entry.mod_after_in_transit);
}

/*
	End of unittest of super_block_write()
 */

/*
	Unittest of super_block_update_stat()
 */

class super_block_update_statTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_update_statTest, UpdateFail_SinceReadEntryFail)
{
	HCFS_STAT new_stat;
	ino_t inode = 8;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_update_stat(inode, &new_stat, FALSE));
}

TEST_F(super_block_update_statTest, UpdateStatSuccess)
{
	HCFS_STAT expected_stat;
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
	sb_entry.inode_stat.ino = inode;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	expected_stat.dev = 1;  /* expected stat */
	expected_stat.ino = inode;
	expected_stat.mode = S_IFDIR;
	expected_stat.nlink = 5;
	expected_stat.size = 123456;

	/* Run */
	EXPECT_EQ(0, super_block_update_stat(inode, &expected_stat, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, sizeof(HCFS_STAT)));
	EXPECT_EQ(IS_DIRTY, sb_entry.status);
	EXPECT_EQ(TRUE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(1, sys_super_block->head.num_dirty);
}

TEST_F(super_block_update_statTest, UpdateStatSuccessNoSync)
{
	HCFS_STAT expected_stat;
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
	sb_entry.inode_stat.ino = inode;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	expected_stat.dev = 1;  /* expected stat */
	expected_stat.ino = inode;
	expected_stat.mode = S_IFDIR;
	expected_stat.nlink = 5;
	expected_stat.size = 123456;

	/* Run */
	EXPECT_EQ(0, super_block_update_stat(inode, &expected_stat, TRUE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, sizeof(HCFS_STAT)));
	EXPECT_EQ(NO_LL, sb_entry.status);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(0, sys_super_block->head.num_dirty);
}

/*
	End of unittest of super_block_update_stat()
 */

/*
	Unittest of super_block_mark_dirty()
 */

class super_block_mark_dirtyTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_mark_dirtyTest, ReadEntryFail)
{
	ino_t inode = 8;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_mark_dirty(inode));
}

TEST_F(super_block_mark_dirtyTest, MarkDirtySuccess)
{
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
	sb_entry.inode_stat.ino = inode;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_mark_dirty(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);

	EXPECT_EQ(IS_DIRTY, sb_entry.status);
	EXPECT_EQ(TRUE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(1, sys_super_block->head.num_dirty);
	EXPECT_EQ(1, sb_head.num_dirty);
}

TEST_F(super_block_mark_dirtyTest, CancelToMarkDirty_BecauseInodeRemoved)
{
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry, sb_entry2;
	SUPER_BLOCK_HEAD sb_head;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
	sb_entry.inode_stat.ino = 0;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_mark_dirty(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry2, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);

	EXPECT_EQ(0, memcmp(&sb_entry2, &sb_entry, sizeof(SUPER_BLOCK_ENTRY)));
	EXPECT_EQ(0, sys_super_block->head.num_dirty);
	EXPECT_EQ(0, sb_head.num_dirty);
}
/*
	End of unittest of super_block_mark_dirty()
 */

/*
	Unittest of supert_block_update_transit()
 */

class super_block_update_transitTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_update_transitTest, ReadEntryFail)
{
	ino_t inode = 8;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run, don't care arguments */
	EXPECT_EQ(-EBADF, super_block_update_transit(inode, FALSE, FALSE));
}

TEST_F(super_block_update_transitTest, Set_is_start_transit_TRUE)
{
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = FALSE;
	sb_entry.mod_after_in_transit = FALSE;

	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_update_transit(inode, TRUE, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	EXPECT_EQ(TRUE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
}

TEST_F(super_block_update_transitTest, DequeueDirtyList_Success)
{
	ino_t inode = 7;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode;

	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_update_transit(inode, FALSE, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(0, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(0, sys_super_block->head.last_dirty_inode);
}

TEST_F(super_block_update_transitTest, FileModifyWhenSyncing_StayInQueue)
{
	ino_t inode = 7, inode_next = 8;
	SUPER_BLOCK_ENTRY sb_entry, sb_entry_next;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);
	uint64_t next_entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode_next - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode_next);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = TRUE;
	sb_entry.util_ll_next = 8;
	sb_entry.util_ll_prev = 0;

	memset(&sb_entry_next, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry_next.status = IS_DIRTY;
	sb_entry_next.in_transit = TRUE;
	sb_entry_next.mod_after_in_transit = FALSE;
	sb_entry_next.util_ll_next = 0;
	sb_entry_next.util_ll_prev = 7;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode_next;
	sys_super_block->sync_point_is_set = FALSE;

	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pwrite(sys_super_block->iofptr, &sb_entry_next,
		sizeof(SUPER_BLOCK_ENTRY), next_entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_update_transit(inode, FALSE, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(inode, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(inode_next, sys_super_block->head.last_dirty_inode);


}

TEST_F(super_block_update_transitTest, SyncPointIsSet_RelocateInodeToLastOne)
{
	ino_t inode = 7, inode_next = 8;
	SUPER_BLOCK_ENTRY sb_entry, sb_entry_next;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);
	uint64_t next_entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode_next - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode_next);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = TRUE;
	sb_entry.util_ll_next = 8;
	sb_entry.util_ll_prev = 0;

	memset(&sb_entry_next, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry_next.status = IS_DIRTY;
	sb_entry_next.in_transit = TRUE;
	sb_entry_next.mod_after_in_transit = FALSE;
	sb_entry_next.util_ll_next = 0;
	sb_entry_next.util_ll_prev = 7;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode_next;
	sys_super_block->sync_point_is_set = TRUE;

	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pwrite(sys_super_block->iofptr, &sb_entry_next,
		sizeof(SUPER_BLOCK_ENTRY), next_entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_update_transit(inode, FALSE, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(inode_next, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(inode, sys_super_block->head.last_dirty_inode);
}

/*
	End of unittest of supert_block_update_transit()
 */


/*
	Unittest of super_block_to_delete()
 */
class super_block_to_deleteTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_to_deleteTest, ReadEntryFail)
{
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_to_delete(inode, TRUE));
}

TEST_F(super_block_to_deleteTest, MarkToDelete_EnqueueSuccess)
{
	ino_t inode = 7;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	sys_super_block->head.num_active_inodes++;

	/* Run */
	EXPECT_EQ(0, super_block_to_delete(inode, TRUE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);

	EXPECT_EQ(TO_BE_DELETED, sb_entry.status);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(ST_DEL, sb_entry.pin_status);
	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(0, sb_head.num_active_inodes);
	EXPECT_EQ(1, sb_head.num_to_be_deleted);
	EXPECT_EQ(inode, sb_head.first_to_delete_inode);
	EXPECT_EQ(inode, sb_head.last_to_delete_inode);
}

TEST_F(super_block_to_deleteTest, MarkToDelete_DeferEnqueueSuccess)
{
	ino_t inode = 7;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	sys_super_block->head.num_active_inodes++;

	/* Run */
	EXPECT_EQ(0, super_block_to_delete(inode, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);

	EXPECT_EQ(NO_LL, sb_entry.status);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(ST_DEL, sb_entry.pin_status);
	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(0, sb_head.num_active_inodes);
	EXPECT_EQ(0, sb_head.num_to_be_deleted);
	EXPECT_EQ(0, sb_head.first_to_delete_inode);
	EXPECT_EQ(0, sb_head.last_to_delete_inode);
}

/*
	End of unittest of super_block_to_delete()
 */

class super_block_deleteTest : public InitSuperBlockBaseClass {
protected:
	void SetUp()
	{
		InitSuperBlockBaseClass::SetUp();
		METAPATH = "super_block_delete_Folder";
		if (access(METAPATH, F_OK) == 0)
			system("rm -rf super_block_delete_Folder");
		mkdir(METAPATH, 0700);
	}

	void TearDown()
	{
		if (access(METAPATH, F_OK) == 0)
			system("rm -rf super_block_delete_Folder");
		InitSuperBlockBaseClass::TearDown();
	}
};

TEST_F(super_block_deleteTest, ReadEntryFail)
{
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_delete(inode));
}

TEST_F(super_block_deleteTest, AddToUnclaimedFileSuccess)
{
	ino_t inode = 7;
	ino_t result_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_delete(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos); // Read entry
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0); // Head
	pread(fileno(sys_super_block->unclaimed_list_fptr), &result_inode,
		sizeof(ino_t), 0); // Read unclaimed_list from file

	EXPECT_EQ(TO_BE_RECLAIMED, sb_entry.status);
	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(ST_DEL, sb_entry.pin_status);
	EXPECT_EQ(1, sb_head.num_to_be_reclaimed);
	EXPECT_EQ(inode, result_inode);
}

TEST_F(super_block_deleteTest, NowFullScan_WriteToTempReclaimedFile)
{
	ino_t inode = 7;
	ino_t result_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	char path[300];
	uint64_t entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	sys_super_block->now_reclaim_fullscan = TRUE;
	sprintf(path, "%s/temp_unclaimed_list", METAPATH);
	ASSERT_EQ(-1, access(path, F_OK));
	ASSERT_EQ(ENOENT, errno);
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_DELETED;
	sb_entry.in_transit = TRUE;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos);

	/* Run */
	EXPECT_EQ(0, super_block_delete(inode));

	/* Verify */
	ASSERT_EQ(0, access(path, F_OK)) << errno;

	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		entry_filepos); // Read entry
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(fileno(sys_super_block->temp_unclaimed_fptr), &result_inode,
		sizeof(ino_t), 0); // Read temp_unclaimed_list from file

	EXPECT_EQ(NO_LL, sb_entry.status);
	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(ST_DEL, sb_entry.pin_status);
	EXPECT_EQ(0, sb_head.num_to_be_reclaimed);
	EXPECT_EQ(inode, result_inode);
	fclose(sys_super_block->temp_unclaimed_fptr);
	unlink(path);
}

/*
	Unittest of super_block_delete()
 */

/*
	Unittest of super_block_reclaim()
 */

class super_block_reclaimTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_reclaimTest, ReclaimNotTrigger)
{
	sys_super_block->head.num_to_be_reclaimed = 0;

	EXPECT_EQ(0, super_block_reclaim());
}

TEST_F(super_block_reclaimTest, ReclaimSuccess)
{
	uint64_t num_inode = RECLAIM_TRIGGER + 123;
	SUPER_BLOCK_ENTRY now_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t now_reclaimed_inode;

	/* Mock unclaimed list, head and entries. */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * num_inode); // Prepare entry

	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
		SUPER_BLOCK_ENTRY sb_entry;

		fwrite(&inode, sizeof(ino_t), 1,
			sys_super_block->unclaimed_list_fptr); // Add unclaimed list

		sb_entry.status = TO_BE_RECLAIMED; // Set status
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1)); // Write entry status
	}

	sys_super_block->head.num_to_be_reclaimed = num_inode; // Set head data
	sys_super_block->head.num_total_inodes = num_inode;
	pwrite(sys_super_block->iofptr, &sys_super_block->head,
		sizeof(SUPER_BLOCK_HEAD), 0); // Write head data

	/* Run */
	EXPECT_EQ(0, super_block_reclaim());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode;
	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
		uint64_t file_pos;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);

		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set

		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}
	EXPECT_EQ(num_inode, sys_super_block->head.last_reclaimed_inode); // Check last inode

	EXPECT_EQ(num_inode, sys_super_block->head.num_inode_reclaimed);
	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed); // Check number of to_be_reclaimed
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	EXPECT_EQ(0, sb_head.num_to_be_reclaimed);
}

TEST_F(super_block_reclaimTest, UnclaimedFileCorrupt_SkipInode)
{
	uint64_t num_inode = RECLAIM_TRIGGER + 123;
	SUPER_BLOCK_ENTRY now_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t now_reclaimed_inode;

	/* Mock unclaimed list, head and entries. */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * num_inode); // Prepare entry

	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
		SUPER_BLOCK_ENTRY sb_entry;

		fwrite(&inode, sizeof(ino_t), 1,
			sys_super_block->unclaimed_list_fptr); // Add unclaimed list

		sb_entry.status = TO_BE_RECLAIMED; // Set status
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1)); // Write entry status
	}

	sys_super_block->head.num_to_be_reclaimed = num_inode; // Set head data
	sys_super_block->head.num_total_inodes = 0;
	pwrite(sys_super_block->iofptr, &sys_super_block->head,
		sizeof(SUPER_BLOCK_HEAD), 0); // Write head data

	/* Run */
	EXPECT_EQ(0, super_block_reclaim());

	/* Verify. Just reclaim inode 1 */
	uint64_t file_pos;
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode;
	file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
		(now_reclaimed_inode - 1);
	pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);
	ASSERT_EQ(1, now_reclaimed_inode); // Check reclaimed inode
	ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set

	EXPECT_EQ(0, now_entry.util_ll_next);
	EXPECT_EQ(1, sys_super_block->head.last_reclaimed_inode); // Check last inode
	EXPECT_EQ(1, sys_super_block->head.num_inode_reclaimed);
	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed); // Check number of to_be_reclaimed
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	EXPECT_EQ(0, sb_head.num_to_be_reclaimed);
}


/*
	End of unittest of super_block_reclaim()
 */

/*
	Unittest of super_block_reclaim_fullscan()
 */

class super_block_reclaim_fullscanTest : public InitSuperBlockBaseClass {
protected:
	void SetUp()
	{
		InitSuperBlockBaseClass::SetUp();
		METAPATH = "super_block_reclaim_fullscan_Folder";
		if (access(METAPATH, F_OK) == 0)
			system("rm -rf super_block_reclaim_fullscan_Folder");
		sys_super_block->head.num_inode_reclaimed = 0;
		sys_super_block->head.num_to_be_reclaimed = 0;
		mkdir(METAPATH, 0700);
	}

	void TearDown()
	{
		if (access(METAPATH, F_OK) == 0)
			system("rm -rf super_block_reclaim_fullscan_Folder");
		InitSuperBlockBaseClass::TearDown();
	}

};

TEST_F(super_block_reclaim_fullscanTest, ReadEntryFail)
{
	/* Mock data. open a nonexisted file */
	close(sys_super_block->iofptr);
	sys_super_block->head.num_total_inodes = 5;
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-EBADF, super_block_reclaim_fullscan());

}

TEST_F(super_block_reclaim_fullscanTest, num_total_inodes_EqualsZero)
{
	/* Mock data. open a nonexisted file */
	sys_super_block->head.num_total_inodes = 0;

	/* Run */
	EXPECT_EQ(0, super_block_reclaim_fullscan());

	/* Verify */
	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(0, sys_super_block->head.num_inode_reclaimed);
	EXPECT_EQ(0, sys_super_block->head.first_reclaimed_inode);
	EXPECT_EQ(0, sys_super_block->head.last_reclaimed_inode);
}

TEST_F(super_block_reclaim_fullscanTest, ScanToBeReclaimedInodeSuccess)
{
	ino_t now_reclaimed_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;

	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		num_inode * sizeof(SUPER_BLOCK_ENTRY));

	/* Write mock entries to be reclaimed */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_RECLAIMED;
	for (ino_t inode = 2 ; inode <= num_inode ; inode++) { /* start from 2 */
		sb_entry.this_index = inode;
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1)); // Write entry status
	}
	sys_super_block->head.num_total_inodes = num_inode;

	/* Run */
	EXPECT_EQ(0, super_block_reclaim_fullscan());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode; // first entry
	for (ino_t inode = 2 ; inode <= num_inode ; inode++) {
		uint64_t file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);

		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set
		ASSERT_EQ(0, now_entry.util_ll_prev);

		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}
	ASSERT_EQ(0, now_reclaimed_inode); /* Next of last reclaimed inode */
	EXPECT_EQ(num_inode, sys_super_block->head.last_reclaimed_inode); // Check last inode

	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(num_inode - 1, sys_super_block->head.num_inode_reclaimed);
}

TEST_F(super_block_reclaim_fullscanTest, ScanReclaimedInodeSuccess)
{
	ino_t now_reclaimed_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;

	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		num_inode * sizeof(SUPER_BLOCK_ENTRY));

	/* Write mock entries to be reclaimed */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = RECLAIMED;
	for (ino_t inode = 2 ; inode <= num_inode ; inode++) { /* start from 2 */
		sb_entry.this_index = inode;
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1)); // Write entry status
	}
	sys_super_block->head.num_total_inodes = num_inode;

	/* Run */
	EXPECT_EQ(0, super_block_reclaim_fullscan());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode; // first entry
	for (ino_t inode = 2 ; inode <= num_inode ; inode++) {
		uint64_t file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);

		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set
		ASSERT_EQ(0, now_entry.util_ll_prev);

		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}
	EXPECT_EQ(num_inode, sys_super_block->head.last_reclaimed_inode); // Check last inode

	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(num_inode - 1, sys_super_block->head.num_inode_reclaimed);
}

TEST_F(super_block_reclaim_fullscanTest, ScanAndReclaim_EmptyInode_Success)
{
	ino_t now_reclaimed_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;

	/* Just truncate superblock */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		num_inode * sizeof(SUPER_BLOCK_ENTRY));

	sys_super_block->head.num_total_inodes = num_inode;

	/* Run */
	EXPECT_EQ(0, super_block_reclaim_fullscan());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode; // first entry
	for (ino_t inode = 2 ; inode <= num_inode ; inode++) {
		uint64_t file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);

		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set
		ASSERT_EQ(0, now_entry.util_ll_prev);

		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}
	EXPECT_EQ(num_inode, sys_super_block->head.last_reclaimed_inode); // Check last inode

	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(num_inode - 1, sys_super_block->head.num_inode_reclaimed);
}

TEST_F(super_block_reclaim_fullscanTest, ScanAndReclaim_EmptyInode_And_Reclaim_ToBeDeleteInode)
{
	ino_t now_reclaimed_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;
	int64_t ret_ssize;
	char temp_path[300], path[300];
	ino_t unclaimed_inode[num_inode / 2 + 1];
	ino_t verify_unclaimed_inode[num_inode / 2 + 1];
	int32_t i = 0;

	/* Just truncate superblock */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) +
		num_inode * sizeof(SUPER_BLOCK_ENTRY));

	sprintf(temp_path, "%s/temp_unclaimed_list", METAPATH);
	sys_super_block->temp_unclaimed_fptr = fopen(temp_path, "w+");
	setbuf(sys_super_block->temp_unclaimed_fptr, NULL);

	sprintf(path, "%s/unclaimed_list", METAPATH);
	sys_super_block->unclaimed_list_fptr = fopen(path, "w+");
	setbuf(sys_super_block->unclaimed_list_fptr, NULL);

	/* Write mock entries to be reclaimed */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_DELETED;
	for (ino_t inode = num_inode / 2 ; inode <= num_inode ; inode++) {
		sb_entry.this_index = inode;
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1));
		fwrite(&inode, sizeof(ino_t), 1,
			sys_super_block->temp_unclaimed_fptr);
		unclaimed_inode[i] = inode;
		i++;
	}
	fclose(sys_super_block->temp_unclaimed_fptr);
	sys_super_block->temp_unclaimed_fptr = NULL;
	sys_super_block->head.num_total_inodes = num_inode;

	/* Run */
	EXPECT_EQ(0, super_block_reclaim_fullscan());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode;
	for (ino_t inode = 2 ; inode < num_inode / 2 ; inode++) {
		uint64_t file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);
		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set
		ASSERT_EQ(0, now_entry.util_ll_prev);
		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}

	/* Check to-be-reclaimed inode */
	for (ino_t inode = num_inode / 2 ; inode <= num_inode ; inode++) {
		uint64_t file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
				(inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);
		ASSERT_EQ(TO_BE_RECLAIMED, now_entry.status);
	}

	EXPECT_EQ(num_inode / 2 - 1, sys_super_block->head.last_reclaimed_inode);
	EXPECT_EQ(num_inode - num_inode / 2 + 1,
			sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(num_inode / 2 - 2, sys_super_block->head.num_inode_reclaimed);
	ret_ssize = lseek(fileno(sys_super_block->unclaimed_list_fptr),
			0, SEEK_END);
	EXPECT_EQ((num_inode - num_inode / 2 + 1) * sizeof(ino_t), ret_ssize);

	pread(fileno(sys_super_block->unclaimed_list_fptr),
			verify_unclaimed_inode, ret_ssize, 0);
	EXPECT_EQ(0, memcmp(verify_unclaimed_inode, unclaimed_inode, ret_ssize));
	EXPECT_EQ(-1, access(temp_path, F_OK));
	EXPECT_EQ(ENOENT, errno);
}
/*
	End of unittest of super_block_reclaim_fullscan()
 */

/*
	Unittest of super_block_new_inode()
 */
class super_block_new_inodeTest : public InitSuperBlockBaseClass {
protected:
	HCFS_STAT expected_stat;

	void SetUp()
	{
		InitSuperBlockBaseClass::SetUp();
		expected_stat.ino = 2;
		expected_stat.mode = S_IFDIR;
		expected_stat.dev = 5;
		expected_stat.nlink = 6;
		expected_stat.size = 5566;
	}
};

TEST_F(super_block_new_inodeTest, NoReclaimedNodes)
{
	uint64_t generation;
	ino_t ret_node;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;

	/* Mock stat */
	generation = 0;

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation, TRUE);
	/* Inode 1 is reserved, so start from 2 */
	EXPECT_EQ(2, ret_node); // ret_node == 2 since system is empty

	/* Verify */
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY));

	EXPECT_EQ(1, sys_super_block->head.num_total_inodes); // Just a new inode
	EXPECT_EQ(1, sys_super_block->head.num_active_inodes); // the inode is active

	EXPECT_EQ(2, sb_entry.this_index); // inode == 1
	EXPECT_EQ(1, sb_entry.generation); // It is first time to be created
	EXPECT_EQ(ST_PIN, sb_entry.pin_status);
	EXPECT_EQ(1, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat,
		sizeof(HCFS_STAT)));
}

TEST_F(super_block_new_inodeTest, NoReclaimedNodes_NoMoreMetaSpace)
{
	uint64_t generation;
	ino_t ret_node;

	/* Mock stat */
	generation = 0;
	hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation, TRUE);
	EXPECT_EQ(0, ret_node); /*Return 0 because exceeding pinned size limit*/
	hcfs_system->systemdata.system_meta_size = 0;
}

TEST_F(super_block_new_inodeTest, GetInodeFromReclaimedNodes_ManyReclaimedInodes)
{
	uint64_t num_reclaimed;
	uint64_t generation;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t ret_node;

	/* Mock reclaimed inodes */
	num_reclaimed = 150;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.generation = 1;
	/* New inode number starts from 2 */
	for (ino_t inode = 2 ; inode <= num_reclaimed ; inode++) {
		sb_entry.this_index = inode;
		if (inode < num_reclaimed)
			sb_entry.util_ll_next = inode + 1;
		else
			sb_entry.util_ll_next = 0;

		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(inode - 1)); // Write entry
	}

	sys_super_block->head.num_inode_reclaimed = num_reclaimed;
	sys_super_block->head.num_total_inodes = num_reclaimed;
	sys_super_block->head.last_reclaimed_inode = num_reclaimed;
	sys_super_block->head.first_reclaimed_inode = 2;
	pwrite(sys_super_block->iofptr, &sys_super_block->head,
		sizeof(SUPER_BLOCK_HEAD), 0); // Write Head

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation, TRUE);
	EXPECT_EQ(2, ret_node); // ret_node == 2 since first_reclaimed = 2

	/* Verify */
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY));

	EXPECT_EQ(num_reclaimed, sb_head.num_total_inodes); // num_total_inodes doesn't change
	EXPECT_EQ(num_reclaimed - 1, sb_head.num_inode_reclaimed); // one node is used now
	EXPECT_EQ(num_reclaimed, sb_head.last_reclaimed_inode); // last reclaimed is the same
	EXPECT_EQ(3, sb_head.first_reclaimed_inode); // first reclaimed is now inode == 3
	EXPECT_EQ(1, sb_head.num_active_inodes); // a node return and be active now

	EXPECT_EQ(2, sb_entry.this_index); // inode == 1
	EXPECT_EQ(2, sb_entry.generation); // generation++
	EXPECT_EQ(ST_PIN, sb_entry.pin_status);
	EXPECT_EQ(2, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat,
		sizeof(HCFS_STAT)));
}

TEST_F(super_block_new_inodeTest, GetInodeFromReclaimedNodes_JustOneReclaimedNode)
{
	uint64_t generation;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t ret_node;

	/* Mock one reclaimed inode */
	hcfs_system->systemdata.pinned_size = MAX_PINNED_LIMIT + 1;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.generation = 1;
	sb_entry.util_ll_next = 0;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY));
		// Write entry

	sys_super_block->head.num_inode_reclaimed = 1;
	sys_super_block->head.num_total_inodes = 1; // Just one inode
	sys_super_block->head.last_reclaimed_inode = 2;
	sys_super_block->head.first_reclaimed_inode = 2;
	pwrite(sys_super_block->iofptr, &sys_super_block->head,
		sizeof(SUPER_BLOCK_HEAD), 0); // Write Head

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation, TRUE);
	EXPECT_EQ(2, ret_node); // ret_node == 2 since first_reclaimed = 2

	/* Verify */
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY));

	EXPECT_EQ(1, sb_head.num_total_inodes); // num_total_inodes doesn't change
	EXPECT_EQ(0, sb_head.num_inode_reclaimed); // No reclaimed inode now
	EXPECT_EQ(0, sb_head.last_reclaimed_inode); // No reclaimed inode now
	EXPECT_EQ(0, sb_head.first_reclaimed_inode); // No reclaimed inode now
	EXPECT_EQ(1, sb_head.num_active_inodes); // a node return and be active now

	EXPECT_EQ(2, sb_entry.this_index); // inode == 1
	EXPECT_EQ(2, sb_entry.generation); // generation++
	EXPECT_EQ(ST_PIN, sb_entry.pin_status);
	EXPECT_EQ(2, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat,
		sizeof(HCFS_STAT)));
}
/*
	End of unittest of super_block_new_inode()
 */

/*
	Unittest of ll_enqueue()
 */

class ll_enqueueTest : public InitSuperBlockBaseClass {
};

TEST_F(ll_enqueueTest, AlreadyInQueue_StatusTheSame)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;
	char test_status[] = {NO_LL, TO_BE_RECLAIMED, RECLAIMED, IS_DIRTY, TO_BE_DELETED};

	/* Run */
	for (uint32_t st = 0 ; st < sizeof(test_status) / sizeof(char) ; st++) {
		sb_entry.status = test_status[st];
		EXPECT_EQ(0, ll_enqueue(inode, test_status[st], &sb_entry));
	}
}

TEST_F(ll_enqueueTest, Enqueue_NO_LL)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode;

	/* Run */
	EXPECT_EQ(0, ll_enqueue(inode, NO_LL, &sb_entry));
}


TEST_F(ll_enqueueTest, Enqueue_TO_BE_RECLAIMED)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode;

	/* Run */
	EXPECT_EQ(0, ll_enqueue(inode, TO_BE_RECLAIMED, &sb_entry));
}

TEST_F(ll_enqueueTest, Enqueue_RECLAIMED)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;

	/* Need to pass dirty inode list check */
	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode;

	/* Run */
	EXPECT_EQ(0, ll_enqueue(inode, RECLAIMED, &sb_entry));
}

TEST_F(ll_enqueueTest, Enqueue_IS_DIRTY_WithEmptyList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;

	/* Run */
	EXPECT_EQ(0, ll_enqueue(inode, IS_DIRTY, &sb_entry));

	/* Verify */
	EXPECT_EQ(inode, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(inode, sys_super_block->head.last_dirty_inode);
	EXPECT_EQ(1, sys_super_block->head.num_dirty);

	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(IS_DIRTY, sb_entry.status);
}

TEST_F(ll_enqueueTest, Enqueue_TO_BE_DELETED_WithEmptyList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;

	/* Run */
	EXPECT_EQ(0, ll_enqueue(inode, TO_BE_DELETED, &sb_entry));

	/* Verify */
	EXPECT_EQ(inode, sys_super_block->head.first_to_delete_inode);
	EXPECT_EQ(inode, sys_super_block->head.last_to_delete_inode);
	EXPECT_EQ(1, sys_super_block->head.num_to_be_deleted);

	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(TO_BE_DELETED, sb_entry.status);
}

TEST_F(ll_enqueueTest, Enqueue_IS_DIRTY_ManyTimes)
{
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	for (ino_t inode = 1; inode <= num_inode ; inode++) {
		int64_t filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

		sb_entry.status = NO_LL;
		sb_entry.this_index = inode;
		EXPECT_EQ(0, ll_enqueue(inode, IS_DIRTY, &sb_entry));
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), filepos);
	}

	/* Verify head and entry data */
	EXPECT_EQ(1, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(num_inode, sys_super_block->head.last_dirty_inode);
	EXPECT_EQ(num_inode, sys_super_block->head.num_dirty);

	ino_t now_inode = sys_super_block->head.first_dirty_inode;
	for (ino_t expected_inode = 1 ;  now_inode ; expected_inode++) {
		int64_t filepos;

		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (now_inode - 1);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), filepos);
		/* Check status and inode number */
		ASSERT_EQ(IS_DIRTY, sb_entry.status) << "status = "
			<< sb_entry.status << ", need IS_DIRTY";
		ASSERT_EQ(expected_inode, now_inode) << "expected_inode = "
			<< expected_inode << ", now_inode = " << now_inode;
		ASSERT_EQ(8192, sb_entry.dirty_meta_size); /* Round size */
		now_inode = sb_entry.util_ll_next; // Go to next dirty inode
	}

	/* Round size */
	EXPECT_EQ(8192 * num_inode, hcfs_system->systemdata.dirty_cache_size);
}

TEST_F(ll_enqueueTest, Enqueue_TO_BE_DELETED_ManyTimes)
{
	SUPER_BLOCK_ENTRY sb_entry;
	uint64_t num_inode = 20000;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	for (ino_t inode = 1; inode <= num_inode ; inode++) {
		int64_t filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

		sb_entry.status = NO_LL;
		EXPECT_EQ(0, ll_enqueue(inode, TO_BE_DELETED, &sb_entry));
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), filepos);
	}

	/* Verify head and entry data */
	EXPECT_EQ(1, sys_super_block->head.first_to_delete_inode);
	EXPECT_EQ(num_inode, sys_super_block->head.last_to_delete_inode);
	EXPECT_EQ(num_inode, sys_super_block->head.num_to_be_deleted);

	ino_t now_inode = sys_super_block->head.first_to_delete_inode;
	for (ino_t expected_inode = 1 ;  now_inode ; expected_inode++) {
		int64_t filepos;

		filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (now_inode - 1);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), filepos);
		// Check status and inode number
		ASSERT_EQ(TO_BE_DELETED, sb_entry.status) << "status = "
			<< sb_entry.status << ", need IS_DIRTY";
		ASSERT_EQ(expected_inode, now_inode) << "expected_inode = "
			<< expected_inode << ", now_inode = " << now_inode;

		now_inode = sb_entry.util_ll_next; // Go to next dirty inode
	}
}

TEST_F(ll_enqueueTest, Enqueue_IS_DIRTY_When_Queue_Recovery)
{
	ino_t this_inode, start_inode, end_inode;
	SUPER_BLOCK_ENTRY sb_entry;

	start_inode = 10;
	end_inode = 100;
	this_inode = 50;

	sb_entry.status = IS_DIRTY;

	sys_super_block->sb_recovery_meta.is_ongoing = TRUE;
	sys_super_block->sb_recovery_meta.start_inode = start_inode;
	sys_super_block->sb_recovery_meta.end_inode = end_inode;

	EXPECT_EQ(0, ll_enqueue(this_inode, IS_DIRTY, &sb_entry));
	EXPECT_EQ(IS_DIRTY, sb_entry.status);

	sys_super_block->sb_recovery_meta.is_ongoing = FALSE;
	sys_super_block->sb_recovery_meta.start_inode = 0;
	sys_super_block->sb_recovery_meta.end_inode = 0;
}
/*
	End of unittest of ll_enqueue()
 */

/*
	Unittest of ll_dequeue()
*/

class ll_dequeueTest : public InitSuperBlockBaseClass {
protected:
	/* Generate a mock dirt_list or to_delete_list */
	void generate_mock_list(uint32_t num_inode, char status)
	{
		SUPER_BLOCK_ENTRY sb_entry;

		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.status = status;
		if (status == IS_DIRTY)
			sb_entry.dirty_meta_size = 5566;

		/* First dirty inode */
		sb_entry.util_ll_next = 2;
		sb_entry.util_ll_prev = 0;
		sb_entry.this_index = 1;
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
				sizeof(SUPER_BLOCK_HEAD));

		/* General dirty inode */
		for (ino_t inode = 2 ; inode < num_inode ; inode++) {
			int64_t filepos = sizeof(SUPER_BLOCK_HEAD) +
				sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

			sb_entry.util_ll_next = inode + 1;
			sb_entry.util_ll_prev = inode - 1;
			sb_entry.this_index = inode;
			pwrite(sys_super_block->iofptr, &sb_entry,
					sizeof(SUPER_BLOCK_ENTRY), filepos);
		}

		/* Last dirty inode */
		sb_entry.util_ll_next = 0;
		sb_entry.util_ll_prev = num_inode - 1;
		sb_entry.this_index = num_inode;
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
				sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY)
				* (num_inode - 1));

		/* Head */
		if (status == IS_DIRTY) {
			sys_super_block->head.first_dirty_inode = 1;
			sys_super_block->head.last_dirty_inode = num_inode;
			sys_super_block->head.num_dirty = num_inode;
		} else {
			sys_super_block->head.first_to_delete_inode = 1;
			sys_super_block->head.last_to_delete_inode = num_inode;
			sys_super_block->head.num_to_be_deleted = num_inode;
		}
	}

	/* Traverse all entry in dirty_list or to_delete_list */
	uint32_t traverse_all_list_entry(char status)
	{
		ino_t now_inode;
		uint32_t remaining_inode;
		SUPER_BLOCK_ENTRY sb_entry;

		if (status == IS_DIRTY)
			now_inode = sys_super_block->head.first_dirty_inode;
		else
			now_inode = sys_super_block->head.first_to_delete_inode;

		remaining_inode = 0;
		while (now_inode) {
			remaining_inode++;
			pread(sys_super_block->iofptr, &sb_entry,
				sizeof(SUPER_BLOCK_ENTRY), sizeof(SUPER_BLOCK_HEAD) +
				sizeof(SUPER_BLOCK_ENTRY) * (now_inode - 1));

			now_inode = sb_entry.util_ll_next;
		}

		return remaining_inode;
	}

};

TEST_F(ll_dequeueTest, Dequeue_NO_LL)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;

	/* Run */
	EXPECT_EQ(0, ll_dequeue(inode, &sb_entry));
}

TEST_F(ll_dequeueTest, Dequeue_TO_BE_RECLAIMED)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_RECLAIMED;

	/* Run */
	EXPECT_EQ(0, ll_dequeue(inode, &sb_entry));
}

TEST_F(ll_dequeueTest, Dequeue_RECLAIMED)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = RECLAIMED;

	/* Run */
	EXPECT_EQ(0, ll_dequeue(inode, &sb_entry));
}

TEST_F(ll_dequeueTest, Dequeue_IS_DIRTY_JustOneElementList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	/* Mock data */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;

	sys_super_block->head.first_dirty_inode = inode;
	sys_super_block->head.last_dirty_inode = inode;
	sys_super_block->head.num_dirty++;

	/* Run */
	EXPECT_EQ(0, ll_dequeue(inode, &sb_entry));

	/* Verify */
	EXPECT_EQ(0, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(0, sys_super_block->head.last_dirty_inode);
	EXPECT_EQ(0, sys_super_block->head.num_dirty);

	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(NO_LL, sb_entry.status);
}

TEST_F(ll_dequeueTest, Dequeue_TO_BE_DELETED_JustOneElementList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode = 5;

	/* Mock data */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_DELETED;

	sys_super_block->head.first_to_delete_inode = inode;
	sys_super_block->head.last_to_delete_inode = inode;
	sys_super_block->head.num_to_be_deleted++;

	/* Run */
	EXPECT_EQ(0, ll_dequeue(inode, &sb_entry));

	/* Verify */
	EXPECT_EQ(0, sys_super_block->head.first_to_delete_inode);
	EXPECT_EQ(0, sys_super_block->head.last_to_delete_inode);
	EXPECT_EQ(0, sys_super_block->head.num_to_be_deleted);

	EXPECT_EQ(0, sb_entry.util_ll_prev);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(NO_LL, sb_entry.status);
}

TEST_F(ll_dequeueTest, Dequeue_IS_DIRTY_ManyElementsInList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	uint32_t num_inode = 200;
	uint32_t remaining_inode;
	int64_t filepos;
	ino_t now_inode;

	/* Mock dirty_inode list */
	generate_mock_list(num_inode, IS_DIRTY);
	hcfs_system->systemdata.dirty_cache_size = 5566 * num_inode + 123;

	/* Run */
	remaining_inode = num_inode;
	now_inode = num_inode / 2; // Start dequeue from medium dirty inode

	while (remaining_inode) {
		filepos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_inode - 1);
		pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);
		ll_dequeue(now_inode, &sb_entry);
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);

		remaining_inode--;

		// Check remaining inodes by traversing all list entry
		ASSERT_EQ(remaining_inode, traverse_all_list_entry(IS_DIRTY));
		ASSERT_EQ(remaining_inode, sys_super_block->head.num_dirty);

		now_inode = (now_inode % num_inode) + 1; // Go to next dirty inode
	}

	/* Verify all inodes */
	for (ino_t inode = 1 ; inode < num_inode ; inode++) {
		int64_t filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);
		ASSERT_EQ(0, sb_entry.util_ll_next) << "inode = " << inode;
		ASSERT_EQ(0, sb_entry.util_ll_prev);
		ASSERT_EQ(NO_LL, sb_entry.status);
		ASSERT_EQ(0, sb_entry.dirty_meta_size);
	}


	EXPECT_EQ(123, hcfs_system->systemdata.dirty_cache_size);
}

TEST_F(ll_dequeueTest, Dequeue_IS_DIRTY_When_Queue_Recovery)
{
	ino_t this_inode, start_inode, end_inode;
	SUPER_BLOCK_ENTRY sb_entry;

	start_inode = 10;
	end_inode = 100;
	this_inode = 50;

	sb_entry.util_ll_next = sb_entry.util_ll_prev = 999;
	sb_entry.status = IS_DIRTY;

	sys_super_block->sb_recovery_meta.is_ongoing = TRUE;
	sys_super_block->sb_recovery_meta.start_inode = start_inode;
	sys_super_block->sb_recovery_meta.end_inode = end_inode;

	EXPECT_EQ(0, ll_dequeue(this_inode, &sb_entry));
	EXPECT_EQ(NO_LL, sb_entry.status);
	EXPECT_EQ(0, sb_entry.util_ll_next);
	EXPECT_EQ(0, sb_entry.util_ll_prev);

	sys_super_block->sb_recovery_meta.is_ongoing = FALSE;
	sys_super_block->sb_recovery_meta.start_inode = 0;
	sys_super_block->sb_recovery_meta.end_inode = 0;
}

TEST_F(ll_dequeueTest, Dequeue_TO_BE_DELETED_ManyElementsInList)
{
	SUPER_BLOCK_ENTRY sb_entry;
	uint32_t num_inode = 200;
	uint32_t remaining_inode;
	int64_t filepos;
	ino_t now_inode;

	/* Mock dirty_inode list */
	generate_mock_list(num_inode, TO_BE_DELETED);

	/* Run */
	remaining_inode = num_inode;
	now_inode = num_inode / 2; // Start dequeue from medium dirty inode

	while (remaining_inode) {
		filepos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) *
			(now_inode - 1);
		pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);
		ll_dequeue(now_inode, &sb_entry);
		pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);

		remaining_inode--;

		// Check remaining inodes by traversing all list entry
		ASSERT_EQ(remaining_inode, traverse_all_list_entry(TO_BE_DELETED));
		ASSERT_EQ(remaining_inode, sys_super_block->head.num_to_be_deleted);

		now_inode = (now_inode % num_inode) + 1; // Go to next dirty inode
	}

	/* Verify all inodes */
	for (ino_t inode = 1 ; inode < num_inode ; inode++) {
		int64_t filepos = sizeof(SUPER_BLOCK_HEAD) +
			sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);
		pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			filepos);

		ASSERT_EQ(0, sb_entry.util_ll_next) << "inode = " << inode;
		ASSERT_EQ(0, sb_entry.util_ll_prev);
		ASSERT_EQ(NO_LL, sb_entry.status);
	}
}

/*
	End of unittest of ll_dequeue()
*/

/*
	Unittest of super_block_share_locking()
*/

class super_block_share_lockingTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_share_lockingTest, LockingSuccess)
{
	int32_t value;
	int32_t num_share_counter = 56;

	/* Run */
	for (int32_t lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(0, super_block_share_locking());

	/* Verify */
	EXPECT_EQ(num_share_counter, sys_super_block->share_counter);

	sem_getvalue(&sys_super_block->share_lock_sem, &value);
	EXPECT_EQ(0, value);
}

/*
	End of unittest of super_block_share_locking()
*/

/*
	Unittest of super_block_share_release()
 */

class super_block_share_releaseTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_share_releaseTest, ReleaseFail)
{
	int32_t num_share_counter = 10;

	/* Run */
	for (int32_t lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(-1, super_block_share_release());
}

TEST_F(super_block_share_releaseTest, ReleaseSuccess)
{
	int32_t value;
	int32_t num_share_counter = 56;

	/* Mock locking */
	for (int32_t lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(0, super_block_share_locking());

	/* Run */
	for (int32_t lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(0, super_block_share_release());

	/* Verify */
	EXPECT_EQ(0, sys_super_block->share_counter);

	sem_getvalue(&sys_super_block->share_lock_sem, &value);
	EXPECT_EQ(1, value);
}

/*
	End of unittest of super_block_share_release()
 */

/* Unittest for super_block_finish_pinning */
class super_block_finish_pinningTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_finish_pinningTest, StatusIsUNPIN_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_UNPIN;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	EXPECT_EQ(0, super_block_finish_pinning(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_finish_pinningTest, StatusIsPIN_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PIN;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	EXPECT_EQ(0, super_block_finish_pinning(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_finish_pinningTest, StatusIsDEL_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_DEL;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));

	/* Run */
	EXPECT_EQ(0, super_block_finish_pinning(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_finish_pinningTest, StatusIsPINNING_ChangeToPIN)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t max_inode = 10;
	ino_t inode_marked_pin = 5;

	for (ino_t inode = 3; inode < max_inode; inode++) {
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.pin_status = ST_UNPIN;
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY));
		super_block_share_locking();
		pin_ll_enqueue(inode, &sb_entry); /* Enqueue */
		super_block_share_release();
	}

	/* Run */
	EXPECT_EQ(0, super_block_finish_pinning(inode_marked_pin));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode_marked_pin - 1) *
		sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, verified_entry.pin_ll_next);
	EXPECT_EQ(0, verified_entry.pin_ll_prev);
	EXPECT_EQ(ST_PIN, verified_entry.pin_status);
}

/* End of unittest for super_block_finish_pinning */

/* Unittest for super_block_mark_pin */
class super_block_mark_pinTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_mark_pinTest, StatusIsPIN_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_pin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_pinTest, StatusIsPINNING_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PINNING;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_pin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_pinTest, StatusIsDEL_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_DEL;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_pin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_pinTest, StatusIsUNPIN_MarkToPIN_CaseDir)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_UNPIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
			offset);
	/* Run */
	EXPECT_EQ(0, super_block_mark_pin(inode, S_IFDIR));

	/* Verify */
	sb_entry.pin_status = ST_PIN;
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_pinTest, StatusIsUNPIN_MarkToPIN_CaseRegfile)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 123;
	for (ino_t this_inode = 3; this_inode <= 10; this_inode++) {
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.pin_status = ST_UNPIN;
		offset = sizeof(SUPER_BLOCK_HEAD) + (this_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);

		pin_ll_enqueue(this_inode, &sb_entry); /* Enqueue */
	}
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_UNPIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);


	/* Run */
	EXPECT_EQ(0, super_block_mark_pin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), sizeof(SUPER_BLOCK_HEAD) +
		(inode - 1) * sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(ST_PINNING, verified_entry.pin_status);
	EXPECT_EQ(0, verified_entry.pin_ll_next);
	EXPECT_EQ(10, verified_entry.pin_ll_prev);
	EXPECT_EQ(sys_super_block->head.last_pin_inode, inode);
}
/* End of unittest for super_block_mark_pin */

/* Unittest for super_block_mark_unpin */
class super_block_mark_unpinTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_mark_unpinTest, StatusIsUNPIN_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_UNPIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_unpin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_unpinTest, StatusIsDEL_DoNothing)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_DEL;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_unpin(inode, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_unpinTest, StatusIsUNPIN_ChangeToPIN)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
		sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY),
		offset);

	/* Run */
	EXPECT_EQ(0, super_block_mark_unpin(inode, S_IFREG));

	/* Verify */
	sb_entry.pin_status = ST_UNPIN;
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY), offset);
	EXPECT_EQ(0, memcmp(&sb_entry, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY)));
}

TEST_F(super_block_mark_unpinTest, StatusIsPINNING_ChangeToPIN)
{
	SUPER_BLOCK_ENTRY sb_entry, verified_entry;
	ino_t max_inode = 10;
	ino_t inode_marked_unpin = 5;

	for (ino_t inode = 3; inode < max_inode; inode++) {
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.pin_status = ST_UNPIN;
		pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY),
			sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY));

		pin_ll_enqueue(inode, &sb_entry); /*Enqueue & set to "pinning"*/
	}

	/* Run */
	EXPECT_EQ(0, super_block_mark_unpin(inode_marked_unpin, S_IFREG));

	/* Verify */
	pread(sys_super_block->iofptr, &verified_entry,
		sizeof(SUPER_BLOCK_ENTRY),
		sizeof(SUPER_BLOCK_HEAD) + (inode_marked_unpin - 1) *
		sizeof(SUPER_BLOCK_ENTRY));
	EXPECT_EQ(0, verified_entry.pin_ll_next);
	EXPECT_EQ(0, verified_entry.pin_ll_prev);
	EXPECT_EQ(ST_UNPIN, verified_entry.pin_status);
}

/* End of unittest for super_block_mark_unpin */

/* Unittest for pin_ll_enqueue */
class pin_ll_enqueueTest : public InitSuperBlockBaseClass {
};

TEST_F(pin_ll_enqueueTest, StatusIsNotUNPIN)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);

	/* Run */
	EXPECT_EQ(ST_PIN, pin_ll_enqueue(inode, &sb_entry));
}

TEST_F(pin_ll_enqueueTest, EnqueueManyTimes_Success)
{
	SUPER_BLOCK_ENTRY sb_entry;
	off_t offset;
	int64_t now_pos;
	ino_t now_inode, expected_inode;

	for (ino_t inode = 2; inode <= 20; inode ++) {
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.pin_status = ST_UNPIN;
		sb_entry.this_index = inode;
		offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pwrite(sys_super_block->iofptr, &sb_entry,
				sizeof(SUPER_BLOCK_ENTRY), offset);
		pin_ll_enqueue(inode, &sb_entry);
	}
	EXPECT_EQ(19, sys_super_block->head.num_pinning_inodes);

	/* Verify from first to last one*/
	now_inode = sys_super_block->head.first_pin_inode;
	expected_inode = 2;
	while(now_inode) {
		ASSERT_EQ(expected_inode, now_inode);
		expected_inode++;

		offset = sizeof(SUPER_BLOCK_HEAD) + (now_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		now_inode = sb_entry.pin_ll_next;
	}
	EXPECT_EQ(21, expected_inode);

	/* Verify from last to first one */
	now_inode = sys_super_block->head.last_pin_inode;
	expected_inode = 20;
	while(now_inode) {
		ASSERT_EQ(expected_inode, now_inode);
		expected_inode--;

		offset = sizeof(SUPER_BLOCK_HEAD) + (now_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		now_inode = sb_entry.pin_ll_prev;
	}
	EXPECT_EQ(1, expected_inode);
}
/* End of unittest for pin_ll_enqueue */

/* Unittest for pin_ll_dequeue */
class pin_ll_dequeueTest : public InitSuperBlockBaseClass {
};

TEST_F(pin_ll_dequeueTest, StatusIsNotPINNING)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PIN;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);

	/* Run */
	EXPECT_EQ(ST_PIN, pin_ll_dequeue(inode, &sb_entry));
}

TEST_F(pin_ll_dequeueTest, InodeNotInQueue)
{
	SUPER_BLOCK_ENTRY sb_entry;
	ino_t inode;
	off_t offset;

	inode = 5;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.pin_status = ST_PINNING;
	offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
	pwrite(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);

	/* Run */
	EXPECT_EQ(-EIO, pin_ll_dequeue(inode, &sb_entry));
}

TEST_F(pin_ll_dequeueTest, DequeueSuccess)
{
	SUPER_BLOCK_ENTRY sb_entry;
	off_t offset;
	int64_t now_pos;
	ino_t now_inode, expected_inode;
	int32_t num_inode, ret;

	/* Run enqueue */
	for (ino_t inode = 2; inode <= 20; inode ++) {
		memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		sb_entry.pin_status = ST_UNPIN;
		sb_entry.this_index = inode;
		offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pwrite(sys_super_block->iofptr, &sb_entry,
				sizeof(SUPER_BLOCK_ENTRY), offset);
		pin_ll_enqueue(inode, &sb_entry);
	}

	EXPECT_EQ(2, sys_super_block->head.first_pin_inode);
	EXPECT_EQ(20, sys_super_block->head.last_pin_inode);

	/* Dequeue half inodes and verify */
	num_inode = 19;
	for (ino_t inode = 10; inode <= 20; inode++) {
		offset = sizeof(SUPER_BLOCK_HEAD) + (inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		ret = pin_ll_dequeue(inode, &sb_entry);
		ASSERT_EQ(0, ret);

		num_inode--;
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		ASSERT_EQ(num_inode, sys_super_block->head.num_pinning_inodes);
		ASSERT_EQ(0, sb_entry.pin_ll_next);
		ASSERT_EQ(0, sb_entry.pin_ll_prev);
	}
	EXPECT_EQ(2, sys_super_block->head.first_pin_inode);
	EXPECT_EQ(9, sys_super_block->head.last_pin_inode);

	/* Verify queue structure (from first one) */
	now_inode = sys_super_block->head.first_pin_inode;
	expected_inode = 2;
	while(now_inode) {
		ASSERT_EQ(expected_inode, now_inode);
		expected_inode++;

		offset = sizeof(SUPER_BLOCK_HEAD) + (now_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		now_inode = sb_entry.pin_ll_next;
	}
	EXPECT_EQ(10, expected_inode);

	/* Verify queue structure (from last one) */
	now_inode = sys_super_block->head.last_pin_inode;
	expected_inode = 9;
	while(now_inode) {
		ASSERT_EQ(expected_inode, now_inode);
		expected_inode--;

		offset = sizeof(SUPER_BLOCK_HEAD) + (now_inode - 1) *
			sizeof(SUPER_BLOCK_ENTRY);
		pread(sys_super_block->iofptr, &sb_entry,
			sizeof(SUPER_BLOCK_ENTRY), offset);
		now_inode = sb_entry.pin_ll_prev;
	}
	EXPECT_EQ(1, expected_inode);
}

/* End of unittest for pin_ll_dequeue */

/* Unittest for ll_rebuild_dirty */
class ll_rebuild_dirtyTest : public InitSuperBlockBaseClass {
protected:
	char mocksb_path[500] = "testpatterns/mocksb_for_rebuild_dirty";

	int32_t reload_sb_head()
	{

		int32_t fd = open(mocksb_path, O_RDONLY);
		pread(fd, &(sys_super_block->head), sizeof(SUPER_BLOCK_HEAD), 0);
		close(fd);
		return 0;
	}

	void gen_test_sblist()
	{
		SUPER_BLOCK_HEAD head;
		SUPER_BLOCK_ENTRY entry;
		int32_t ino = 2;

		if (access(mocksb_path, F_OK) != -1)
			unlink(mocksb_path);
		sys_super_block->iofptr = open(mocksb_path, O_CREAT | O_RDWR, 0600);

		/* SB head */
		memset(&head, 0, sizeof(SUPER_BLOCK_HEAD));
		write_super_block_head();

		/* Create some dirty inode */
		for (; ino < 15; ino++) {
		        memset(&entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		        entry.this_index = ino;
		        entry.status = NO_LL;
		        ll_enqueue(ino, IS_DIRTY, &entry);
		        write_super_block_entry(ino, &entry);
		        write_super_block_head();
		}

		/* Create some inode with status NO_LL */
		for (; ino < 20; ino++) {
		        memset(&entry, 0, sizeof(SUPER_BLOCK_ENTRY));
		        entry.this_index = ino;
		        entry.status = NO_LL;
		        write_super_block_entry(ino, &entry);
		}
	}

	int32_t inject_fault_enqueue(ino_t this_inode, int32_t fault_loc)
	{
		SUPER_BLOCK_ENTRY tempentry;

		read_super_block_entry(this_inode, &tempentry);

		ll_enqueue(this_inode, IS_DIRTY, &tempentry);
		if (fault_loc == 1)
			return -81;

		write_super_block_entry(this_inode, &tempentry);
		if (fault_loc == 2)
			return -82;

		write_super_block_head();

		return 0;
	}

	int32_t inject_fault_dequeue(ino_t this_inode, int32_t fault_loc)
	{
		SUPER_BLOCK_ENTRY tempentry;

		read_super_block_entry(this_inode, &tempentry);

		ll_dequeue(this_inode, &tempentry);
		if (fault_loc == 1)
			return -91;

		write_super_block_head();
		if (fault_loc == 2)
			return -92;

		write_super_block_entry(this_inode, &tempentry);
		return 0;
	}

	int32_t check_sblist_integrity()
	{
		SUPER_BLOCK_ENTRY entry1, entry2;
		ino_t first, last;

		memset(&entry1, 0, sizeof(SUPER_BLOCK_ENTRY));
		memset(&entry2, 0, sizeof(SUPER_BLOCK_ENTRY));

		first = sys_super_block->head.first_dirty_inode;
		last = sys_super_block->head.last_dirty_inode;

		/* traveral forward */
		if ((first == 0 || last == 0) && first != last)
			return 0;

		/* traveling forward */
		read_super_block_entry(first, &entry1);
		if (entry1.util_ll_prev != 0)
			return -1;

		while (entry1.util_ll_next != 0) {
			if (entry1.status != IS_DIRTY)
				return -1;
			read_super_block_entry(entry1.util_ll_next, &entry2);
			if (entry2.util_ll_prev != entry1.this_index)
				return -1;
			entry1 = entry2;
		}

		if (entry1.this_index != last)
			return -1;
		return 0;
	}
};

TEST_F(ll_rebuild_dirtyTest, rebuild_empty_listSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	sys_super_block->head.first_dirty_inode = 0;
	sys_super_block->head.last_dirty_inode = 0;

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_single_inodeSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	sys_super_block->head.first_dirty_inode = 0;
	sys_super_block->head.last_dirty_inode = 0;

	/* Test start */
	ll_rebuild_dirty(10);

	EXPECT_EQ(0, check_sblist_integrity());
	EXPECT_EQ(10, sys_super_block->head.first_dirty_inode);
	EXPECT_EQ(10, sys_super_block->head.last_dirty_inode);

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_noERR_listSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_enqueueERR_listSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-81, inject_fault_enqueue(16, 1));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_enqueueERR_list2SUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-82, inject_fault_enqueue(17, 2));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_dequeueERR_first_entrySUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-91, inject_fault_dequeue(2, 1));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_dequeueERR_first_entry2SUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-92, inject_fault_dequeue(2, 2));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_dequeueERR_last_entrySUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-91, inject_fault_dequeue(14, 1));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_dequeueERR_last_entry2SUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-92, inject_fault_dequeue(14, 2));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_multiple_dequeueERR_listSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-91, inject_fault_dequeue(2, 1));
	reload_sb_head();
	EXPECT_EQ(-92, inject_fault_dequeue(13, 2));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}

TEST_F(ll_rebuild_dirtyTest, rebuild_mix_enqueueERR_dequeueERR_listSUCCESS)
{
	/* Gen mock data for test */
	gen_test_sblist();
	EXPECT_EQ(-91, inject_fault_dequeue(2, 1));
	reload_sb_head();
	EXPECT_EQ(-92, inject_fault_dequeue(8, 2));
	reload_sb_head();
	EXPECT_EQ(-82, inject_fault_enqueue(15, 2));
	reload_sb_head();

	/* Test start */
	ll_rebuild_dirty(0);

	EXPECT_EQ(0, check_sblist_integrity());

	close(sys_super_block->iofptr);
	unlink(mocksb_path);
}
/* End for ll_rebuild_dirty */

/**
 * Unittest of super_block_set_syncpoint()
 */
class super_block_set_syncpointTest : public InitSuperBlockBaseClass {

};

TEST_F(super_block_set_syncpointTest, AllClean_DoNotNeedSet)
{
	sys_super_block->head.last_dirty_inode = 0;
	sys_super_block->head.last_to_delete_inode = 0;

	EXPECT_EQ(1, super_block_set_syncpoint());
	EXPECT_EQ(FALSE, sys_super_block->sync_point_is_set);
}

TEST_F(super_block_set_syncpointTest, InitSyncPointSuccess)
{
	SYNC_POINT_DATA exp_data;

	sys_super_block->head.last_dirty_inode = 123;
	sys_super_block->head.last_to_delete_inode = 456;
	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 123;
	exp_data.delete_sync_point = 456;
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = FALSE;

	/* Run */
	EXPECT_EQ(0, super_block_set_syncpoint());

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(0, memcmp(&exp_data,
			&(sys_super_block->sync_point_info->data),
			sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(SYNC_RETRY_TIMES,
		sys_super_block->sync_point_info->sync_retry_times);
}

TEST_F(super_block_set_syncpointTest, ResetSyncPointSuccess)
{
	SYNC_POINT_DATA exp_data;

	sys_super_block->sync_point_is_set = TRUE;
	sys_super_block->sync_point_info = (SYNC_POINT_INFO *)
		malloc(sizeof(SYNC_POINT_INFO));
	memset(sys_super_block->sync_point_info, 0, sizeof(SYNC_POINT_INFO));
	sem_init(&(sys_super_block->sync_point_info->ctl_sem), 0, 1);

	sys_super_block->head.last_dirty_inode = 123;
	sys_super_block->head.last_to_delete_inode = 456;
	memset(&exp_data, 0, sizeof(SYNC_POINT_DATA));
	exp_data.upload_sync_point = 123;
	exp_data.delete_sync_point = 456;
	exp_data.upload_sync_complete = FALSE;
	exp_data.delete_sync_complete = FALSE;

	/* Run */
	EXPECT_EQ(0, super_block_set_syncpoint());

	/* Verify */
	EXPECT_EQ(TRUE, sys_super_block->sync_point_is_set);
	EXPECT_EQ(0, memcmp(&exp_data,
			&(sys_super_block->sync_point_info->data),
			sizeof(SYNC_POINT_DATA)));
	EXPECT_EQ(SYNC_RETRY_TIMES,
		sys_super_block->sync_point_info->sync_retry_times);
}
/**
 * End of unittest of super_block_set_syncpoint()
 */

/**
 * Unittest of super_block_cancel_syncpoint()
 */
class super_block_cancel_syncpointTest : public InitSuperBlockBaseClass {

};

TEST_F(super_block_cancel_syncpointTest, NoSyncPointIsSet)
{
	sys_super_block->sync_point_is_set = FALSE;

	EXPECT_EQ(1, super_block_cancel_syncpoint());
}

TEST_F(super_block_cancel_syncpointTest, CancelSuccess)
{
	sys_super_block->sync_point_is_set = TRUE;

	EXPECT_EQ(0, super_block_cancel_syncpoint());
}
/**
 * End of unittest of super_block_cancel_syncpoint()
 */

/*
 * Unittest of check_init_super_block()
 */
class check_init_super_blockTest : public ::testing::Test {
protected:
	void SetUp()
	{
		METAPATH = "check_init_super_blockTestDir";
		if (!access(METAPATH, F_OK))
			system("rm -rf check_init_super_blockTestDir");
		mkdir(METAPATH, 0777);
	}
	void TearDown()
	{
		if (!access(METAPATH, F_OK))
			system("rm -rf check_init_super_blockTestDir");
	}
};

TEST_F(check_init_super_blockTest, Init_OldSuperblock_Success)
{
	char sb_path[400];
	char unclaimedfile_path[400] = "testpatterns/unclaimedfile_path";

	sprintf(sb_path, "%s/superblock", METAPATH);
	sprintf(unclaimedfile_path, "%s/unclaimedfile_path", METAPATH);

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;

	NUM_VOL = 0;
	CURRENT_BACKEND = SWIFT;

	/* Run */
	EXPECT_EQ(1, check_init_super_block());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}

TEST_F(check_init_super_blockTest, No_Superblock_But_RootExist_StartRebuild)
{
	char sb_path[400];
	char unclaimedfile_path[400] = "testpatterns/unclaimedfile_path";

	sprintf(sb_path, "%s/superblock", METAPATH);
	sprintf(unclaimedfile_path, "%s/unclaimedfile_path", METAPATH);

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;

	NUM_VOL = 1;
	CURRENT_BACKEND = SWIFT;

	/* Run */
	EXPECT_EQ(0, check_init_super_block());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}

TEST_F(check_init_super_blockTest, SuperblockExist_JustOpenIt)
{
	char sb_path[400];
	char unclaimedfile_path[400] = "testpatterns/unclaimedfile_path";
	FILE *fptr;
	SUPER_BLOCK_HEAD sb_head;

	memset(&sb_head, 0, sizeof(SUPER_BLOCK_HEAD));
	hcfs_system->system_restoring = NOT_RESTORING;
	//sb_head.now_rebuild = FALSE;
	sprintf(sb_path, "%s/superblock", METAPATH);
	sprintf(unclaimedfile_path, "%s/unclaimedfile_path", METAPATH);

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;
	fptr = fopen(SUPERBLOCK, "w+");
	pwrite(fileno(fptr), &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	fclose(fptr);

	NUM_VOL = 1;
	CURRENT_BACKEND = SWIFT;

	/* Run */
	EXPECT_EQ(1, check_init_super_block());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}

TEST_F(check_init_super_blockTest, SuperblockExist_KeepRebuilding)
{
	char sb_path[400];
	char unclaimedfile_path[400] = "testpatterns/unclaimedfile_path";
	FILE *fptr;
	SUPER_BLOCK_HEAD sb_head;

	memset(&sb_head, 0, sizeof(SUPER_BLOCK_HEAD));
	hcfs_system->system_restoring = RESTORING_STAGE2;
	//sb_head.now_rebuild = TRUE;
	sprintf(sb_path, "%s/superblock", METAPATH);
	sprintf(unclaimedfile_path, "%s/unclaimedfile_path", METAPATH);

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;
	fptr = fopen(SUPERBLOCK, "w+");
	pwrite(fileno(fptr), &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	fclose(fptr);

	NUM_VOL = 1;
	CURRENT_BACKEND = SWIFT;

	/* Run */
	EXPECT_EQ(0, check_init_super_block());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}

TEST_F(check_init_super_blockTest, SuperblockHeadCorrupted_StartRebuild)
{
	char sb_path[400];
	char unclaimedfile_path[400] = "testpatterns/unclaimedfile_path";
	FILE *fptr;
	char sb_head[5];

	memset(&sb_head, 0, 5);
	sprintf(sb_path, "%s/superblock", METAPATH);
	sprintf(unclaimedfile_path, "%s/unclaimedfile_path", METAPATH);

	SUPERBLOCK = sb_path;
	UNCLAIMEDFILE = unclaimedfile_path;
	fptr = fopen(SUPERBLOCK, "w+");
	pwrite(fileno(fptr), &sb_head, 5, 0);
	fclose(fptr);

	NUM_VOL = 1;
	CURRENT_BACKEND = SWIFT;

	/* Run */
	EXPECT_EQ(0, check_init_super_block());

	/* Verify */
	EXPECT_EQ(0, access(sb_path, F_OK));
	EXPECT_EQ(0, access(unclaimedfile_path, F_OK));

	unlink(sb_path);
	unlink(unclaimedfile_path);
}
/*
 * End unittest of check_init_super_block()
 */
