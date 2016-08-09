#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "gtest/gtest.h"
#include "mock_params.h"
extern "C" {
#include "tocloud_tools.h"
#include "hcfs_tocloud.h"
#include "global.h"
#include "fuseop.h"
#include "atomic_tocloud.h"
//#include "utils.h"
//int32_t fetch_meta_path(char *pathname, ino_t this_inode);
}
//extern int32_t fetch_meta_path(char *pathname, ino_t this_inode);

/**
 * Unittest of pull_retry_inode()
 */
class pull_retry_inodeTest : public ::testing::Test {
protected:
	IMMEDIATELY_RETRY_LIST list;

	void SetUp()
	{
		memset(&list, 0, sizeof(IMMEDIATELY_RETRY_LIST));
		list.list_size = 8;
		list.retry_inode = (ino_t *) malloc(sizeof(ino_t) *
				list.list_size);
	}

	void TearDown()
	{
		free(list.retry_inode);
	}
};

TEST_F(pull_retry_inodeTest, PullManyTimesSuccess)
{
	int num_retry;

	for (int i = 0; i < list.list_size; i++)
		list.retry_inode[i] = i + 1;
	list.num_retry = list.list_size;

	num_retry = list.list_size;

	for (int i = 1; i <= list.list_size; i++) {
		ASSERT_EQ(i, pull_retry_inode(&list));
		ASSERT_EQ(num_retry - i, list.num_retry);
	}
	EXPECT_EQ(0, pull_retry_inode(&list));
	EXPECT_EQ(0, list.num_retry);
}

TEST_F(pull_retry_inodeTest, PullEmptyList)
{
	EXPECT_EQ(0, pull_retry_inode(&list));
	EXPECT_EQ(0, list.num_retry);
}
/**
 * End of unittest of pull_retry_inode()
 */

/**
 * Unittest of push_retry_inode()
 */
class push_retry_inodeTest : public ::testing::Test {
protected:
	IMMEDIATELY_RETRY_LIST list;

	void SetUp()
	{
		memset(&list, 0, sizeof(IMMEDIATELY_RETRY_LIST));
		list.list_size = 8;
		list.retry_inode = (ino_t *) calloc(sizeof(ino_t) *
				list.list_size, 1);
	}

	void TearDown()
	{
		free(list.retry_inode);
	}
};

TEST_F(push_retry_inodeTest, PushManyTimesSuccess)
{	
	for (int i = 1; i <= list.list_size; i++) {
		push_retry_inode(&list, i);
		ASSERT_EQ(i, list.num_retry);
	}

	for (int i = 1; i <= list.list_size; i++) {
		ASSERT_EQ(i, list.retry_inode[i - 1]);
	}
}
/**
 * End of unittest of push_retry_inode()
 */

/**
 * Unittest of change_block_status_to_BOTH()
 */
class change_block_status_to_BOTHTest : public ::testing::Test {
protected:
	void SetUp()
	{
		hcfs_system = (SYSTEM_DATA_HEAD *)
				calloc(sizeof(SYSTEM_DATA_HEAD), 1);
		sem_init(&(hcfs_system->something_to_replace), 0, 0);
		sem_init(&(hcfs_system->access_sem), 0, 1);
		if (!access("tocloud_tools_test_folder", F_OK))
			system("rm -rf tocloud_tools_test_folder");
		mkdir("tocloud_tools_test_folder", 0700);
	}

	void TearDown()
	{
		free(hcfs_system);
		if (!access("tocloud_tools_test_folder", F_OK))
			system("rm -rf tocloud_tools_test_folder");
	}
};

TEST_F(change_block_status_to_BOTHTest, ChangeStatusSuccess)
{
	char path[300];
	ino_t inode = 5;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE tmppage, verified_page;
	FILE *fptr;
	int64_t pagepos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
	int64_t blockno = 10;
	int64_t seqnum = 123;
	int32_t val;

	sprintf(path, "tocloud_tools_test_folder/mock_meta_%"PRIu64,
			(uint64_t)inode);
	memset(&tmpstat, 0, sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
	filemeta.local_pin = P_UNPIN;
	fptr = fopen(path, "w+");
	setbuf(fptr, NULL);
	ASSERT_TRUE(fptr != NULL) << "ErrCode: " << errno;
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	tmppage.block_entries[10].status = ST_LtoC;
	tmppage.block_entries[10].seqnum = seqnum;
	tmppage.block_entries[10].uploaded = FALSE;
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);

	/* Run */
	EXPECT_EQ(0, change_block_status_to_BOTH(inode, blockno,
			pagepos, seqnum));

	/* Verify */
	fptr = fopen(path, "r");
	fseek(fptr, pagepos, SEEK_SET);
	fread(&verified_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);
	EXPECT_EQ(ST_BOTH, verified_page.block_entries[10].status);
	EXPECT_EQ(seqnum, verified_page.block_entries[10].seqnum);
	EXPECT_EQ(TRUE, verified_page.block_entries[10].uploaded);
	sem_getvalue(&(hcfs_system->something_to_replace), &val);
	EXPECT_EQ(1, val);

	unlink(path);	
}

TEST_F(change_block_status_to_BOTHTest, StatusIsNot_LtoC)
{
	char path[300];
	ino_t inode = 5;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE tmppage, verified_page;
	FILE *fptr;
	int64_t pagepos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
	int64_t blockno = 10;
	int64_t seqnum = 123;
	int32_t val;

	sprintf(path, "tocloud_tools_test_folder/mock_meta_%"PRIu64,
			(uint64_t)inode);
	memset(&tmpstat, 0, sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
	filemeta.local_pin = P_UNPIN;
	fptr = fopen(path, "w+");
	setbuf(fptr, NULL);
	ASSERT_TRUE(fptr != NULL) << "ErrCode: " << errno;
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	tmppage.block_entries[10].status = ST_LDISK;
	tmppage.block_entries[10].seqnum = seqnum + 1;
	tmppage.block_entries[10].uploaded = FALSE;
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);

	/* Run */
	EXPECT_EQ(0, change_block_status_to_BOTH(inode, blockno,
			pagepos, seqnum));

	/* Verify */
	fptr = fopen(path, "r");
	fseek(fptr, pagepos, SEEK_SET);
	fread(&verified_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);
	EXPECT_EQ(ST_LDISK, verified_page.block_entries[10].status);
	EXPECT_EQ(seqnum + 1, verified_page.block_entries[10].seqnum);
	EXPECT_EQ(FALSE, verified_page.block_entries[10].uploaded);

	sem_getvalue(&(hcfs_system->something_to_replace), &val);
	EXPECT_EQ(0, val);

	unlink(path);
}

TEST_F(change_block_status_to_BOTHTest, StatusIs_NONE)
{
	char path[300];
	ino_t inode = 5;
	HCFS_STAT tmpstat;
	FILE_META_TYPE filemeta;
	BLOCK_ENTRY_PAGE tmppage, verified_page;
	FILE *fptr;
	int64_t pagepos = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);
	int64_t blockno = 10;
	int64_t seqnum = 123;
	int32_t val;

	sprintf(path, "tocloud_tools_test_folder/mock_meta_%"PRIu64,
			(uint64_t)inode);
	memset(&tmpstat, 0, sizeof(HCFS_STAT));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
	filemeta.local_pin = P_UNPIN;
	fptr = fopen(path, "w+");
	setbuf(fptr, NULL);
	ASSERT_TRUE(fptr != NULL) << "ErrCode: " << errno;
	fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	tmppage.block_entries[10].status = ST_NONE;
	tmppage.block_entries[10].seqnum = seqnum;
	tmppage.block_entries[10].uploaded = FALSE;
	fwrite(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);

	/* Run */
	EXPECT_EQ(0, change_block_status_to_BOTH(inode, blockno,
			pagepos, seqnum));

	/* Verify */
	fptr = fopen(path, "r");
	fseek(fptr, pagepos, SEEK_SET);
	fread(&verified_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);
	EXPECT_EQ(ST_NONE, verified_page.block_entries[10].status);
	EXPECT_EQ(seqnum, verified_page.block_entries[10].seqnum);
	EXPECT_EQ(FALSE, verified_page.block_entries[10].uploaded);

	sem_getvalue(&(hcfs_system->something_to_replace), &val);
	EXPECT_EQ(0, val);

	unlink(path);
}
