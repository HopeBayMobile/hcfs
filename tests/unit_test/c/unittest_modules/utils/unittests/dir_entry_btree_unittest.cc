#include "gtest/gtest.h"
#include "cstdlib"
extern "C" {
#include "dir_entry_btree.h"
}

static inline int compare(const void *a, const void *b)
{
	return strcmp(((DIR_ENTRY *)a)->d_name, ((DIR_ENTRY *)b)->d_name);
}

/*
	Unittest of dentry_binary_search()
 */

class dentry_binary_searchTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			entry_array = NULL;
		}
		virtual void TearDown()
		{
			if (entry_array != NULL)
				free(entry_array);
		}
		void generate_mock_data(int num)
		{
			if (entry_array != NULL)
				free(entry_array);
			entry_array = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY) * num);
			for (int i = 0 ; i < num ; i++) {
				char test_filename[30];
				sprintf(test_filename, "test_filename%d", i);
				strcpy(entry_array[i].d_name, test_filename);
			}
			qsort(entry_array, num, sizeof(DIR_ENTRY), compare);

		}
		int linear_search(int num, DIR_ENTRY *entry)
		{
			for (int i = 0 ; i < num ; i++) 
				if (compare(entry_array + i, entry) >= 0)
					return i;
			return num;
		}
		DIR_ENTRY *entry_array;
};

TEST_F(dentry_binary_searchTest, EntryNotFound)
{
	/* Mock data */
	generate_mock_data(500);
	/* Test */
	for (int times = 0 ; times < 50 ; times++) {
		DIR_ENTRY entry;
		int index;
		sprintf(entry.d_name, "test_filename%d123", times);
		ASSERT_EQ(-1, dentry_binary_search(entry_array, 50, &entry, &index));
		EXPECT_EQ(linear_search(50, &entry), index);
	}
}

/*
	End of unittest of dentry_binary_search()
 */
