extern "C" {
#include "meta_mem_cache.h"
#include "filetables.h"
#include "global.h"
#include <errno.h>
}
#include <vector>
#include "mock_params.h"
#include "gtest/gtest.h"

/*
	Unittest for init_system_fh_table()
 */
TEST(init_system_fh_tableTest, InitSuccess)
{
	/* Generate answer */
	int32_t val;
	char *flags_ans = (char *)malloc(sizeof(char) * MAX_OPEN_FILE_ENTRIES);
	FH_ENTRY *entry_table_ans =
		 (FH_ENTRY *)malloc(sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
	memset(flags_ans, 0, sizeof(char) * MAX_OPEN_FILE_ENTRIES);
	memset(entry_table_ans, 0, sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
	/* Run function */
	ASSERT_EQ(0, init_system_fh_table()) << "Testing init_system_fh_table() should success";
	/* Check answer*/
	EXPECT_EQ(0, memcmp(system_fh_table.entry_table_flags, flags_ans, 
		sizeof(char) * MAX_OPEN_FILE_ENTRIES));
	EXPECT_EQ(0, memcmp(system_fh_table.entry_table, entry_table_ans, 
		sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES));
	EXPECT_EQ(0, system_fh_table.num_opened_files);
	EXPECT_EQ(0, system_fh_table.last_available_index);
	sem_getvalue(&(system_fh_table.fh_table_sem), &val);
	EXPECT_EQ(1, val);
}
/*
	End of unittest for init_system_fh_table()
 */

/*
	Unittest for open_fh()
 */
class open_fhTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			init_system_fh_table();
		}

};

TEST_F(open_fhTest, num_opened_files_LimitExceeded)
{
	/* Mock data */
	system_fh_table.num_opened_files = MAX_OPEN_FILE_ENTRIES + 1;
	/* Run function & Test */
	ASSERT_EQ(-EMFILE, open_fh(1, 0)) << "Testing open_fh() should fail";
	/* Recover */
	system_fh_table.num_opened_files = 0;
}

TEST_F(open_fhTest, OpenfhSuccess)
{
	ino_t inode;
	int32_t index;
	for (int32_t times = 0 ; times < 500 ; times++) {
		/* Mock inode number */
		srand(time(NULL));
		inode = times;
		/* Run function */
		index = open_fh(inode, 0) ;
		/* Check answer */
		ASSERT_NE(-1, index) << "Fail with inode = " << inode;
		ASSERT_EQ(TRUE, system_fh_table.entry_table_flags[index]);
		ASSERT_EQ(inode, system_fh_table.entry_table[index].thisinode);
		ASSERT_EQ(times + 1, system_fh_table.num_opened_files);
	}
}
/*
	End of unittest for open_fh()
 */

/*
	Unittest for close_fh()
 */
class close_fhTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			init_system_fh_table();
		}
};

TEST_F(close_fhTest, CloseEmptyEntry) 
{
	ASSERT_EQ(-1, close_fh(0));
}

TEST_F(close_fhTest, meta_cache_lock_entry_ReturnNull)
{
	int32_t index;
	/* Mock data */
	index = open_fh(INO__META_CACHE_LOCK_ENTRY_FAIL, 0);
	ASSERT_NE(-1, index);
	/* Test */
	ASSERT_EQ(-1, close_fh(index));
}

TEST_F(close_fhTest, CloseSuccess)
{
	std::vector<int32_t> index_list;
	int32_t ans_num_opened_files = 0;

	init_system_fh_table();
	/* Mock data */
	for (int32_t num_inode = 0; num_inode < 500 ; num_inode++) {
		int32_t index;
		int32_t inode = num_inode * 27;
		index = open_fh(inode, 0);
		ASSERT_NE(-1, index) << "Fail to open fh with inode " << inode;
		index_list.push_back(index);
		ans_num_opened_files++;
	}
	/* Test */
	for (int32_t i = 0 ; i < index_list.size() ; i++) {
		int32_t index = index_list[i];
		ASSERT_EQ(0, close_fh(index)) << "Fail to open fh with inode " << index;
		ASSERT_EQ(FALSE, system_fh_table.entry_table_flags[index]);
		ASSERT_EQ(--ans_num_opened_files, system_fh_table.num_opened_files);
	}
}
/*
	End of unittest for close_fh()
 */
