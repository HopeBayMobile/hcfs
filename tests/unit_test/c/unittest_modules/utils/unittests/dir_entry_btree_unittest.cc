#include "gtest/gtest.h"
#include "cstdlib"
extern "C" {
#include "dir_entry_btree.h"
#include "fuseop.h"
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
	int num_entry = 500;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test */
	for (int times = 0 ; times < 100 ; times++) {
		DIR_ENTRY entry;
		int index;
		sprintf(entry.d_name, "test_filename%d123", times);
		ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
		EXPECT_EQ(linear_search(num_entry, &entry), index);
	}
}

TEST_F(dentry_binary_searchTest, BoundaryTest)
{	
	DIR_ENTRY entry;
	int index;
	int num_entry = 500;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test index 0 */
	strcpy(entry.d_name, "test");
	ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
	EXPECT_EQ(0, index);
	/* Test last index */
	strcpy(entry.d_name, "test_filename9999999");
	ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
	EXPECT_EQ(500, index);
}

TEST_F(dentry_binary_searchTest, FindEntrySuccess)
{
	int num_entry = 5000;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test */
	for (int times = 0 ; times < 100 ; times++) {
		DIR_ENTRY entry;
		int index;
		int ans;
		sprintf(entry.d_name, "test_filename%d", times * 23);
		ans = linear_search(num_entry, &entry);
		ASSERT_EQ(ans, dentry_binary_search(entry_array, num_entry, &entry, &index));
	}
}


/*
	End of unittest of dentry_binary_search()
 */

/*
	Unittest of insert_dir_entry_btree()
 */

class insert_dir_entry_btreeTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			struct stat dir_stat;
			DIR_META_TYPE meta;
			DIR_ENTRY_PAGE node;
			
			memset(&dir_stat, 0, sizeof(struct stat));
			memset(&meta, 0, sizeof(DIR_META_TYPE));
			memset(&node, 0, sizeof(DIR_ENTRY_PAGE));
			// root entry
			node.dir_entries[0].d_ino = 1;
			strcpy(node.dir_entries[0].d_name, ".");
			node.dir_entries[0].d_type = D_ISDIR;
			
			node.dir_entries[1].d_ino = 0;
			strcpy(node.dir_entries[1].d_name, "..");
			node.dir_entries[1].d_type = D_ISDIR;

			node.num_entries = 2;
			node.this_page_pos = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			// meta
			meta.root_entry_page = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			meta.tree_walk_list_head = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			// open file
			fptr = fopen("/tmp/test_dir_meta", "a+");
			ASSERT_TRUE(fptr != NULL);
			fwrite(&dir_stat, sizeof(struct stat), 1, fptr);
			fwrite(&meta, sizeof(DIR_META_TYPE), 1, fptr);
			fwrite(&node, sizeof(DIR_ENTRY_PAGE), 1, fptr);
			fh = fileno(fptr);
		}
		virtual void TearDown()
		{
			close(fh);
			unlink("/tmp/test_dir_meta");
		}
		int fh;
		FILE *fptr;
};

TEST_F(insert_dir_entry_btreeTest, Insert_To_Root_Without_Split)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY *overflow_median;
	long long *overflow_new_pos;
	DIR_ENTRY tmp_entries[MAX_DIR_ENTRIES_PER_PAGE + 2];
	long long tmp_child_pos[MAX_DIR_ENTRIES_PER_PAGE + 3];
			
	fseek(fptr, sizeof(struct stat), SEEK_SET);
	fread(&meta, sizeof(DIR_META_TYPE), 1, fptr);
	fread(&root_node, sizeof(DIR_ENTRY_PAGE), 1, fptr);
	overflow_median = NULL;
	overflow_new_pos = NULL;

	/* Test for 97 times since max_entries_num==99 (97 + "." + "..") */
	for (int times = 0 ; times < 97 ; times++) {
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		entry->d_ino = (times + 5) * 3;
		entry->d_type = D_ISDIR;
		ASSERT_EQ(0, insert_dir_entry_btree(entry, &root_node, fh, 
			overflow_median, overflow_new_pos, &meta, tmp_entries, 
			tmp_child_pos));
		ASSERT_TRUE(overflow_median == NULL);
		ASSERT_TRUE(overflow_new_pos == NULL);
		ASSERT_EQ(times + 2 + 1, root_node.num_entries);
		// ASSERT_EQ(times + 1, meta.total_children); // Why not plus 1 in this function?
		// Why not update meta to memory? 
	}
}

/*
	End of unittest of insert_dir_entry_btree()
 */

