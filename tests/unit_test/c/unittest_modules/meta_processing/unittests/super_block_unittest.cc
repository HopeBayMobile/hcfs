extern "C" {
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include "mock_param.h"
#include "super_block.h"
#include "global.h"
}
#include "gtest/gtest.h"

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
	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);
	
	/* Run */
	EXPECT_EQ(-1, write_super_block_head());

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
	EXPECT_EQ(-1, read_super_block_entry(inode, &sb_entry));

	close(sys_super_block->iofptr);
}

TEST_F(read_super_block_entryTest, ReadEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 2;
	FILE *ptr;
	
	/* Write mock sb head & sb entry */
	ptr = fopen(sb_path, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (int i = 0 ; i < inode - 1 ; i++)
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
	EXPECT_EQ(-1, write_super_block_entry(inode, &sb_entry));

	close(sys_super_block->iofptr);
}

TEST_F(write_super_block_entryTest, WriteEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 7;
	FILE *ptr;
	
	/* Write mock sb head & sb entry */
	ptr = fopen(sb_path, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (int i = 0 ; i < inode ; i++)
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
	}

	void TearDown()
	{
		if (sys_super_block->iofptr > 0)
			close(sys_super_block->iofptr);
		free(sys_super_block);
		unlink(SUPERBLOCK);
		unlink(UNCLAIMEDFILE);
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
	EXPECT_EQ(-1, super_block_read(inode, &sb_entry));
}

TEST_F(super_block_readTest, ReadEntrySuccess)
{
	SUPER_BLOCK_ENTRY sb_entry, expected_sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t inode = 2;
	FILE *ptr;
	
	/* Write mock sb head & sb entry */
	close(sys_super_block->iofptr);
	
	ptr = fopen(SUPERBLOCK, "w+");
	fwrite(&sb_head, sizeof(SUPER_BLOCK_HEAD), 1, ptr);
	memset(&expected_sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	for (int i = 0 ; i < inode - 1 ; i++)
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
	EXPECT_EQ(-1, super_block_write(inode, &sb_entry));
}

TEST_F(super_block_writeTest, AddDirtyNode_Dequeue_Enqueue_andWriteHead)
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
	EXPECT_EQ(-1, super_block_write(inode, &sb_entry));

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
	EXPECT_EQ(-1, super_block_write(inode, &sb_entry));

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
	struct stat new_stat;
	ino_t inode = 8;

	/* Mock data. open a nonexisted file */	
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-1, super_block_update_stat(inode, &new_stat));
}

TEST_F(super_block_update_statTest, UpdateStatSuccess)
{	
	struct stat expected_stat;
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) + 
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	expected_stat.st_dev = 1;  /* expected stat */
	expected_stat.st_ino = inode;
	expected_stat.st_mode = S_IFDIR;
	expected_stat.st_nlink = 5;
	expected_stat.st_size = 123456;

	/* Run */
	EXPECT_EQ(0, super_block_update_stat(inode, &expected_stat));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * (inode - 1));

	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, sizeof(struct stat)));
	EXPECT_EQ(IS_DIRTY, sb_entry.status);
	EXPECT_EQ(TRUE, sb_entry.mod_after_in_transit);
	EXPECT_EQ(1, sys_super_block->head.num_dirty);
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
	EXPECT_EQ(-1, super_block_mark_dirty(inode));
}

TEST_F(super_block_mark_dirtyTest, MarkDirtySuccess)
{
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	unsigned long entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) + 
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = NO_LL;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;
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

	/* Run */
	EXPECT_EQ(-1, super_block_update_transit(inode, FALSE));
}

TEST_F(super_block_update_transitTest, SetStartTransit_TRUE)
{
	ino_t inode = 8;
	SUPER_BLOCK_ENTRY sb_entry;
	unsigned long entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
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
	EXPECT_EQ(0, super_block_update_transit(inode, TRUE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		entry_filepos);

	EXPECT_EQ(TRUE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
}

TEST_F(super_block_update_transitTest, DequeueDirtyList_CancelTransitSuccess)
{
	ino_t inode = 7;
	SUPER_BLOCK_ENTRY sb_entry;
	unsigned long entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
		sizeof(SUPER_BLOCK_ENTRY) * (inode - 1);

	/* Mock data. Write a entry */
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) + 
		sizeof(SUPER_BLOCK_ENTRY) * inode);
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = IS_DIRTY;
	sb_entry.in_transit = TRUE;
	sb_entry.mod_after_in_transit = FALSE;

	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		entry_filepos);
	
	/* Run */
	EXPECT_EQ(0, super_block_update_transit(inode, FALSE));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		entry_filepos);

	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(FALSE, sb_entry.mod_after_in_transit);
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
	EXPECT_EQ(-1, super_block_to_delete(inode));
}

TEST_F(super_block_to_deleteTest, MarkToDeleteSuccess)
{
	ino_t inode = 7;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	unsigned long entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
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
	EXPECT_EQ(0, super_block_to_delete(inode));

	/* Verify */
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		entry_filepos);
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);

	EXPECT_EQ(FALSE, sb_entry.in_transit);
	EXPECT_EQ(0, sb_head.num_active_inodes);
	EXPECT_EQ(1, sb_head.num_to_be_deleted);
}

/*
	End of unittest of super_block_to_delete()
 */

class super_block_deleteTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_deleteTest, ReadEntryFail)
{
	ino_t inode = 7;

	/* Mock data. open a nonexisted file */	
	close(sys_super_block->iofptr);
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);

	/* Run */
	EXPECT_EQ(-1, super_block_delete(inode));
}

TEST_F(super_block_deleteTest, AddToUnclaimedFileSuccess)
{
	ino_t inode = 7;
	ino_t result_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	unsigned long entry_filepos = sizeof(SUPER_BLOCK_HEAD) +
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
	EXPECT_EQ(1, sb_head.num_to_be_reclaimed);
	EXPECT_EQ(inode, result_inode);
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
	unsigned long num_inode = RECLAIM_TRIGGER + 123;
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
	pwrite(sys_super_block->iofptr, &sys_super_block->head, 
		sizeof(SUPER_BLOCK_HEAD), 0); // Write head data
	
	/* Run */
	EXPECT_EQ(0, super_block_reclaim());

	/* Verify */
	now_reclaimed_inode = sys_super_block->head.first_reclaimed_inode;
	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
		unsigned long file_pos;

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

/*
	End of unittest of super_block_reclaim()
 */

/*
	Unittest of super_block_reclaim_fullscan()
 */

class super_block_reclaim_fullscanTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_reclaim_fullscanTest, ReadEntryFail)
{
	/* Mock data. open a nonexisted file */	
	close(sys_super_block->iofptr);
	sys_super_block->head.num_total_inodes = 5;
	sys_super_block->iofptr = open("/testpatterns/not_exist", O_RDONLY, 0600);
	
	/* Run */
	EXPECT_EQ(-1, super_block_reclaim_fullscan());

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

}

TEST_F(super_block_reclaim_fullscanTest, ScanReclaimedInodeSuccess)
{
	ino_t now_reclaimed_inode;
	SUPER_BLOCK_ENTRY sb_entry;
	unsigned long num_inode = 20000;
	
	ftruncate(sys_super_block->iofptr, sizeof(SUPER_BLOCK_HEAD) + 
		num_inode * sizeof(SUPER_BLOCK_ENTRY));
	
	/* Write mock entries to be reclaimed */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.status = TO_BE_RECLAIMED;
	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
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
	for (ino_t inode = 1 ; inode <= num_inode ; inode++) {
		unsigned long file_pos;
		SUPER_BLOCK_ENTRY now_entry;

		file_pos = sizeof(SUPER_BLOCK_HEAD) + sizeof(SUPER_BLOCK_ENTRY) * 
			(now_reclaimed_inode - 1);
		pread(sys_super_block->iofptr, &now_entry,
			sizeof(SUPER_BLOCK_ENTRY), file_pos);
		
		ASSERT_EQ(inode, now_reclaimed_inode); // Check reclaimed inode
		ASSERT_EQ(RECLAIMED, now_entry.status); // Check status is set
		
		now_reclaimed_inode = now_entry.util_ll_next; // Go to next reclaimed entry
	}
	EXPECT_EQ(num_inode, sys_super_block->head.last_reclaimed_inode); // Check last inode

	EXPECT_EQ(0, sys_super_block->head.num_to_be_reclaimed);
	EXPECT_EQ(num_inode, sys_super_block->head.num_inode_reclaimed);
}

/*
	End of unittest of super_block_reclaim_fullscan()
 */

/*
	Unittest of super_block_new_inode()
 */

class super_block_new_inodeTest : public InitSuperBlockBaseClass {
protected:
	struct stat expected_stat;
	
	void SetUp()
	{
		InitSuperBlockBaseClass::SetUp();
		expected_stat.st_ino = 1;
		expected_stat.st_mode = S_IFDIR;
		expected_stat.st_dev = 5;
		expected_stat.st_nlink = 6;
		expected_stat.st_size = 5566;
	}
};

TEST_F(super_block_new_inodeTest, NoReclaimedNodes)
{
	unsigned long generation;
	ino_t ret_node;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;

	/* Mock stat */
	generation = 0;

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation);
	EXPECT_EQ(1, ret_node); // ret_node == 1 since system is empty

	/* Verify */	
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD));

	EXPECT_EQ(1, sys_super_block->head.num_total_inodes); // Just a new inode
	EXPECT_EQ(1, sys_super_block->head.num_active_inodes); // the inode is active

	EXPECT_EQ(1, sb_entry.this_index); // inode == 1
	EXPECT_EQ(1, sb_entry.generation); // It is first time to be created
	EXPECT_EQ(1, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, 
		sizeof(struct stat)));
}

TEST_F(super_block_new_inodeTest, GetInodeFromReclaimedNodes_ManyReclaimedInodes)
{
	unsigned long num_reclaimed;
	unsigned long generation;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t ret_node;

	/* Mock reclaimed inodes */
	num_reclaimed = 150;
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.generation = 1;
	for (ino_t inode = 1 ; inode <= num_reclaimed ; inode++) {
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
	sys_super_block->head.first_reclaimed_inode = 1;
	pwrite(sys_super_block->iofptr, &sys_super_block->head, 
		sizeof(SUPER_BLOCK_HEAD), 0); // Write Head
	
	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation);
	EXPECT_EQ(1, ret_node); // ret_node == 1 since first_reclaimed = 1

	/* Verify */	
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD));

	EXPECT_EQ(num_reclaimed, sb_head.num_total_inodes); // num_total_inodes doesn't change
	EXPECT_EQ(num_reclaimed - 1, sb_head.num_inode_reclaimed); // one node is used now
	EXPECT_EQ(num_reclaimed, sb_head.last_reclaimed_inode); // last reclaimed is the same
	EXPECT_EQ(2, sb_head.first_reclaimed_inode); // first reclaimed is now inode == 2
	EXPECT_EQ(1, sb_head.num_active_inodes); // a node return and be active now

	EXPECT_EQ(1, sb_entry.this_index); // inode == 1
	EXPECT_EQ(2, sb_entry.generation); // generation++
	EXPECT_EQ(2, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, 
		sizeof(struct stat)));
}

TEST_F(super_block_new_inodeTest, GetInodeFromReclaimedNodes_JustOneReclaimedNode)
{
	unsigned long generation;
	SUPER_BLOCK_ENTRY sb_entry;
	SUPER_BLOCK_HEAD sb_head;
	ino_t ret_node;
	
	/* Mock one reclaimed inode */
	memset(&sb_entry, 0, sizeof(SUPER_BLOCK_ENTRY));
	sb_entry.generation = 1;
	sb_entry.util_ll_next = 0;
	pwrite(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD)); // Write entry

	sys_super_block->head.num_inode_reclaimed = 1;
	sys_super_block->head.num_total_inodes = 1; // Just one inode
	sys_super_block->head.last_reclaimed_inode = 1;
	sys_super_block->head.first_reclaimed_inode = 1;
	pwrite(sys_super_block->iofptr, &sys_super_block->head, 
		sizeof(SUPER_BLOCK_HEAD), 0); // Write Head

	/* Run */
	ret_node = super_block_new_inode(&expected_stat, &generation);
	EXPECT_EQ(1, ret_node); // ret_node == 1 since first_reclaimed = 1

	/* Verify */	
	pread(sys_super_block->iofptr, &sb_head, sizeof(SUPER_BLOCK_HEAD), 0);
	pread(sys_super_block->iofptr, &sb_entry, sizeof(SUPER_BLOCK_ENTRY), 
		sizeof(SUPER_BLOCK_HEAD));

	EXPECT_EQ(1, sb_head.num_total_inodes); // num_total_inodes doesn't change
	EXPECT_EQ(0, sb_head.num_inode_reclaimed); // No reclaimed inode now
	EXPECT_EQ(0, sb_head.last_reclaimed_inode); // No reclaimed inode now
	EXPECT_EQ(0, sb_head.first_reclaimed_inode); // No reclaimed inode now
	EXPECT_EQ(1, sb_head.num_active_inodes); // a node return and be active now

	EXPECT_EQ(1, sb_entry.this_index); // inode == 1
	EXPECT_EQ(2, sb_entry.generation); // generation++
	EXPECT_EQ(2, generation);
	EXPECT_EQ(0, memcmp(&expected_stat, &sb_entry.inode_stat, 
		sizeof(struct stat)));
}
/*
	End of unittest of super_block_new_inode()
 */

/*
	Unittest of super_block_share_locking()
*/

class super_block_share_lockingTest : public InitSuperBlockBaseClass {
};

TEST_F(super_block_share_lockingTest, LockingSuccess)
{
	int value;
	int num_share_counter = 56;

	/* Run */
	for (int lock = 0 ; lock < num_share_counter ; lock++)
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
	int num_share_counter = 10;

	/* Run */
	for (int lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(-1, super_block_share_release());
}

TEST_F(super_block_share_releaseTest, ReleaseSuccess)
{
	int value;
	int num_share_counter = 56;

	/* Mock locking */
	for (int lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(0, super_block_share_locking());
	
	/* Run */
	for (int lock = 0 ; lock < num_share_counter ; lock++)
		EXPECT_EQ(0, super_block_share_release());
	
	/* Verify */
	EXPECT_EQ(0, sys_super_block->share_counter);
	
	sem_getvalue(&sys_super_block->share_lock_sem, &value);
	EXPECT_EQ(1, value);
}

/*
	End of unittest of super_block_share_release()
 */	
