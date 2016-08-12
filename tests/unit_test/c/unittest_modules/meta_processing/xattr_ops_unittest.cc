#include <errno.h>
#include <sys/xattr.h>
extern "C" {
#include "mock_param.h"
#include "xattr_ops.h"
#include "global.h"
#include "meta_mem_cache.h"
}
#include "string"
#include "gtest/gtest.h"
#include <utility>
#include <algorithm>

/*
	Unittest of parse_xattr_namespace()
 */
class parse_xattr_namespaceTest : public ::testing::Test {
protected:
	int32_t ret;
	char name_space;
	char key[300];

	void SetUp()
	{
		memset(key, 0, 300);
	}

	void TearDown()
	{
	}
};

TEST_F(parse_xattr_namespaceTest, EmptyName)
{
	/* Run */
	ret = parse_xattr_namespace("", &name_space, key);

	/* Verify */
	EXPECT_EQ(-EOPNOTSUPP, ret);
}

TEST_F(parse_xattr_namespaceTest, NoNamespace)
{
	int32_t ret1, ret2;

	/* Run */
	ret1 = parse_xattr_namespace("aloha", &name_space, key);
	ret2 = parse_xattr_namespace(".aloha", &name_space, key);

	/* Verify */
	EXPECT_EQ(-EOPNOTSUPP, ret1);
	EXPECT_EQ(-EOPNOTSUPP, ret2);
}

TEST_F(parse_xattr_namespaceTest, NoKey)
{
	/* Run */
	ret = parse_xattr_namespace("user.", &name_space, key);

	/* Verify */
	EXPECT_EQ(-EINVAL, ret);
}

TEST_F(parse_xattr_namespaceTest, NamespaceTooLong)
{
	/* Run */
	ret = parse_xattr_namespace("useruseruseruseruseruser.a",
		&name_space, key);

	/* Verify */
	EXPECT_EQ(-EOPNOTSUPP, ret);
}

TEST_F(parse_xattr_namespaceTest, NameTooLong)
{
	char name[300];
	char buf[280];

	/* Mock name */
	memset(buf, 'a', 280);
	memset(name, 0, 300);
	strcpy(name, "user.");
	memcpy(name + 5, buf, 280);

	/* Run */
	ret = parse_xattr_namespace(name, &name_space, key);

	/* Verify */
	EXPECT_EQ(-EINVAL, ret);
}

TEST_F(parse_xattr_namespaceTest, NamespaceIsUser)
{
	/* Run */
	ret = parse_xattr_namespace("user.hello", &name_space, key);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(USER, name_space);
	EXPECT_STREQ("hello", key);
}

TEST_F(parse_xattr_namespaceTest, NamespaceIsSystem)
{
	/* Run */
	ret = parse_xattr_namespace("system.hello", &name_space, key);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(SYSTEM, name_space);
	EXPECT_STREQ("hello", key);
}

TEST_F(parse_xattr_namespaceTest, NamespaceIsTrusted)
{
	/* Run */
	ret = parse_xattr_namespace("trusted.hello", &name_space, key);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(TRUSTED, name_space);
	EXPECT_STREQ("hello", key);
}

TEST_F(parse_xattr_namespaceTest, NamespaceIsSecurity)
{
	/* Run */
	ret = parse_xattr_namespace("security.hello", &name_space, key);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(SECURITY, name_space);
	EXPECT_STREQ("hello", key);
}
/*
	End of unittest of parse_xattr_namespace()
 */


using namespace std;

class XattrOperationBase : public ::testing::Test {
protected:
	META_CACHE_ENTRY_STRUCT *mock_meta_cache;
	XATTR_PAGE *mock_xattr_page;
	int64_t xattr_page_pos;
	const char *meta_path;

	void SetUp()
	{
		mock_meta_cache = (META_CACHE_ENTRY_STRUCT *)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(mock_meta_cache, 0, sizeof(META_CACHE_ENTRY_STRUCT));

		mock_xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
		memset(mock_xattr_page, 0, sizeof(XATTR_PAGE));
		xattr_page_pos = 0;

		meta_path = "/tmp/xattr_mock_meta";
		if (!access(meta_path, F_OK))
			unlink(meta_path);
		mock_meta_cache->fptr = fopen(meta_path, "wr+");
		fseek(mock_meta_cache->fptr, xattr_page_pos, SEEK_SET);
		fwrite(mock_xattr_page, sizeof(XATTR_PAGE), 1, mock_meta_cache->fptr);
	}

	void TearDown()
	{
		if (mock_meta_cache->fptr)
			fclose(mock_meta_cache->fptr);

		if (!access(meta_path, F_OK))
			unlink(meta_path);

		free(mock_meta_cache);
		free(mock_xattr_page);
	}

	void gen_random_string(std::string &str, const int32_t len) {
		static const char alphanum[] =
			"0123456789_.-"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		char s[len + 1];

		for (int32_t i = 0; i < len; ++i) {
			s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
		}

		s[len] = 0;
		str = std::string(s);
	}

};

/*
	Unittest of find_key_entry()
 */
class find_key_entryTest : public XattrOperationBase {
protected:
	void gen_key_page(KEY_LIST_PAGE &key_page, string *mock_keys,
		const int32_t &total_keys, int32_t &index)
	{
		int32_t num_insert;

		if (index + MAX_KEY_ENTRY_PER_LIST < total_keys)
			num_insert = MAX_KEY_ENTRY_PER_LIST;
		else
			num_insert = total_keys - index;

		for (int32_t i = 0; i < num_insert ; i++) {
			strcpy(key_page.key_list[i].key, mock_keys[index].c_str());
			key_page.key_list[i].key_size = mock_keys[index].length();

			index++;
		}

		key_page.num_xattr = num_insert;
	}
};

TEST_F(find_key_entryTest, FirstKeyPageIsEmpty)
{
	int32_t index;
	int32_t ret;

	ret = find_key_entry(mock_meta_cache, 0, NULL, &index,
		NULL, "key", NULL, NULL);

	EXPECT_EQ(1, ret);
	EXPECT_EQ(-1, index);
}

TEST_F(find_key_entryTest, KeysAllFound)
{
	KEY_LIST_PAGE target_key_page, prev_key_page, tmp_key_page;
	int64_t target_pos, prev_pos, first_key_page_pos;
	int32_t index, now_index, count;
	int32_t ret;
	int32_t num_keys = MAX_KEY_ENTRY_PER_LIST * 10;
	string mock_keys[num_keys];

	/* Gen mock keys and mock key_pages */
	for (int32_t i = 0 ; i < num_keys ; i++) {
		char tmp_key[30];
		sprintf(tmp_key, "test_key-%d", i);
		mock_keys[i] = string(tmp_key);
	}
	sort(mock_keys, mock_keys + num_keys);

	now_index = count = 0;
	fseek(mock_meta_cache->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	while (now_index < num_keys) { // Write mock key_pages
		count++;
		memset(&tmp_key_page, 0, sizeof(KEY_LIST_PAGE));
		gen_key_page(tmp_key_page, mock_keys, num_keys, now_index);
		if (now_index < num_keys)
			tmp_key_page.next_list_pos = sizeof(XATTR_PAGE) +
				count * sizeof(KEY_LIST_PAGE);
		else
			tmp_key_page.next_list_pos = 0;
		fwrite(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
			mock_meta_cache->fptr);
	}

	/* Run & Verify */
	first_key_page_pos = sizeof(XATTR_PAGE);
	for (int32_t i = 0; i < num_keys ; i++) {
		ret = find_key_entry(mock_meta_cache, first_key_page_pos,
			&target_key_page, &index, &target_pos,
			mock_keys[i].c_str(), &prev_key_page, &prev_pos);
		ASSERT_EQ(0, ret);
		ASSERT_NE(-1, index);
		ASSERT_STREQ(mock_keys[i].c_str(),
			target_key_page.key_list[index].key);
		if (target_pos == first_key_page_pos) {
			ASSERT_EQ(0, prev_pos);
		} else {
			ASSERT_EQ(target_pos - sizeof(KEY_LIST_PAGE), prev_pos);
			ASSERT_EQ(target_pos, prev_key_page.next_list_pos);
		}
	}
}

TEST_F(find_key_entryTest, KeysAllNotFound_KeyPageAllFull)
{
	KEY_LIST_PAGE target_key_page, prev_key_page, tmp_key_page;
	KEY_LIST_PAGE last_key_page;
	int64_t target_pos, prev_pos;
	int64_t first_key_page_pos, last_key_page_pos;
	int32_t index, now_index, count;
	int32_t ret;
	int32_t num_keys = MAX_KEY_ENTRY_PER_LIST * 10;
	string mock_keys[num_keys];

	/* Gen mock keys and mock key_pages */
	for (int32_t i = 0 ; i < num_keys ; i++) {
		char tmp_key[30];
		sprintf(tmp_key, "test_key-%d", i);
		mock_keys[i] = string(tmp_key);
	}
	sort(mock_keys, mock_keys + num_keys);

	now_index = count = 0;
	fseek(mock_meta_cache->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	while (now_index < num_keys) { // Write mock key_pages
		count++;
		memset(&tmp_key_page, 0, sizeof(KEY_LIST_PAGE));
		gen_key_page(tmp_key_page, mock_keys, num_keys, now_index);
		if (now_index < num_keys)
			tmp_key_page.next_list_pos = sizeof(XATTR_PAGE) +
				count * sizeof(KEY_LIST_PAGE);
		else
			tmp_key_page.next_list_pos = 0;
		fwrite(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
			mock_meta_cache->fptr);
	}

	/* Run & Verify. When all key pages are full and key is not found,
	   target_pos and target_page is last page position and last page,
	   respectively. */
	first_key_page_pos = sizeof(XATTR_PAGE);
	last_key_page_pos = sizeof(XATTR_PAGE) +
		(count - 1) * sizeof(KEY_LIST_PAGE);
	memcpy(&last_key_page, &tmp_key_page, sizeof(KEY_LIST_PAGE));
	for (int32_t i = 0; i < num_keys ; i++) {
		char tmp_key[30];

		sprintf(tmp_key, "test_key_not_found-%d", i);

		ret = find_key_entry(mock_meta_cache, first_key_page_pos,
			&target_key_page, &index, &target_pos,
			tmp_key, &prev_key_page, &prev_pos);
		ASSERT_EQ(1, ret);
		ASSERT_EQ(-1, index);
		ASSERT_EQ(last_key_page_pos, target_pos);
		ASSERT_EQ(0, memcmp(&last_key_page, &target_key_page,
			sizeof(KEY_LIST_PAGE)));
	}
}

TEST_F(find_key_entryTest, KeysAllNotFound_KeyPageNotFull_ReturnFittedIndex)
{
	KEY_LIST_PAGE target_key_page, tmp_key_page;
	int64_t target_pos, first_key_page_pos, now_pos;
	int32_t index, now_index, count;
	int32_t ret;
	int32_t num_keys = MAX_KEY_ENTRY_PER_LIST * 10;
	string mock_keys[num_keys];
	KEY_ENTRY tmp_key_buf[MAX_KEY_ENTRY_PER_LIST + 1];

	/* Gen mock keys and mock key_pages */
	for (int32_t i = 0 ; i < num_keys ; i++) {
		char tmp_key[30];
		sprintf(tmp_key, "test_key-%d", i);
		mock_keys[i] = string(tmp_key);
	}
	sort(mock_keys, mock_keys + num_keys);

	now_index = count = 0;
	fseek(mock_meta_cache->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	while (now_index < num_keys) { // Write mock key_pages
		count++;
		memset(&tmp_key_page, 0, sizeof(KEY_LIST_PAGE));
		gen_key_page(tmp_key_page, mock_keys, num_keys, now_index);
		if (now_index < num_keys)
			tmp_key_page.next_list_pos = sizeof(XATTR_PAGE) +
				count * sizeof(KEY_LIST_PAGE);
		else
			tmp_key_page.next_list_pos = 0;
		fwrite(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
			mock_meta_cache->fptr);
	}

	/* Run & Verify */
	now_pos = sizeof(XATTR_PAGE);
	first_key_page_pos = now_pos;
	while (now_pos) {
		KEY_ENTRY removed_key_entry;

		fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
		fread(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
			mock_meta_cache->fptr);
		for (uint32_t i = 0; i < tmp_key_page.num_xattr; i++) {

			/* Remove the target key in current such that
			   the function cannot find the key and also
			   return current page as target_page. */
			removed_key_entry = tmp_key_page.key_list[i];
			memcpy(tmp_key_buf, &(tmp_key_page.key_list[i + 1]),
				sizeof(KEY_ENTRY) *
				(tmp_key_page.num_xattr - i - 1));
			memcpy(&(tmp_key_page.key_list[i]), tmp_key_buf,
				sizeof(KEY_ENTRY) *
				(tmp_key_page.num_xattr - i - 1));
			tmp_key_page.num_xattr--;
			fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
			fwrite(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
				mock_meta_cache->fptr);

			/* Run the tested function. target_pos is the pos of
			   key_page that allows a new key entry. In this test,
			   target_page is exactly the current page. */
			ret = find_key_entry(mock_meta_cache, first_key_page_pos,
				&target_key_page, &index, &target_pos,
				removed_key_entry.key, NULL, NULL);
			ASSERT_EQ(1, ret);
			ASSERT_EQ(i, index);
			ASSERT_EQ(now_pos, target_pos);
			ASSERT_EQ(0, memcmp(&tmp_key_page, &target_key_page,
				sizeof(KEY_LIST_PAGE)));

			/* Restore the key page */
			memcpy(tmp_key_buf, &(tmp_key_page.key_list[i]),
				sizeof(KEY_ENTRY) *
				(tmp_key_page.num_xattr - i));
			tmp_key_page.key_list[i] = removed_key_entry;
			memcpy(&(tmp_key_page.key_list[i + 1]), tmp_key_buf,
				sizeof(KEY_ENTRY) *
				(tmp_key_page.num_xattr - i));
			tmp_key_page.num_xattr++;
			fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
			fwrite(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
				mock_meta_cache->fptr);
		}

		now_pos = tmp_key_page.next_list_pos;
	}
}

/*
	End of unittest of find_key_entry()
 */

/*
	Unittest of insert_xattr()
 */
class insert_xattrTest : public XattrOperationBase {

};

TEST_F(insert_xattrTest, DefaultInsertManyKeys)
{
	int32_t ret;
	int32_t num_keys = MAX_KEY_ENTRY_PER_LIST * MAX_KEY_HASH_ENTRY * 10;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 25);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Insert keys */
	cout << "Test: Begin to insert " << num_keys << " key-value pairs" << endl;
	for (int32_t i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, 0); // flag = 0 (default)
		ASSERT_EQ(0, ret);
	}
	cout << "Test: Insertion done." << endl;

	/* Verify */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[100];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			100, &actual_size);
		value_buffer[value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, CreateKey_WithoutValue_Success)
{
	int32_t ret;
	int32_t num_keys = 600;
	size_t value_size = 0;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		mock_value = "";
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Insert keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, XATTR_CREATE); /* flag = CREATE */
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[500];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			500, &actual_size);
		value_buffer[value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, CreateKeySuccess)
{
	int32_t ret;
	int32_t num_keys = 600;
	size_t value_size = 300;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Insert keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, XATTR_CREATE); // flag = CREATE
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[500];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			500, &actual_size);
		value_buffer[value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, ReplaceWithLongerValueSuccess)
{
	int32_t ret;
	int32_t num_keys = 600;
	size_t value_size = 30; // Original value size
	size_t replace_value_size = MAX_VALUE_BLOCK_SIZE * 3; // Replaced value size
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys and replace keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_value;

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, 0); // First insert keys with default flag
		ASSERT_EQ(0, ret);

		// Generate new value and replace later.
		gen_random_string(mock_value, replace_value_size);
		mock_xattr[i].second = mock_value;
	}

	for (int32_t i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			replace_value_size, XATTR_REPLACE); // Then insert with flag = REPLACE
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[replace_value_size + 10];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			replace_value_size + 10, &actual_size);
		value_buffer[replace_value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(replace_value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(0, mock_xattr_page->reclaimed_value_block);
	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, ReplaceWithShorterValueSuccess)
{
	int32_t ret;
	int32_t num_keys = 600;
	size_t value_size = MAX_VALUE_BLOCK_SIZE * 3; // Original value size
	size_t replace_value_size = 20; // Replaced value size
	int64_t now_pos;
	int32_t reclaimed_count;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys and replace keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_value;

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, 0); // First insert keys with default flag
		ASSERT_EQ(0, ret);

		// Generate new value and replace later.
		gen_random_string(mock_value, replace_value_size);
		mock_xattr[i].second = mock_value;
	}

	for (int32_t i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			replace_value_size, XATTR_REPLACE); // Then insert with flag = REPLACE
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[replace_value_size + 10];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			replace_value_size + 10, &actual_size);
		value_buffer[replace_value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(replace_value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}
	/* Verify reclaimed value-block (Because replace with shorter value) */
	reclaimed_count = 0;
	now_pos = mock_xattr_page->reclaimed_value_block;
	while (now_pos) {
		VALUE_BLOCK tmp_block;

		reclaimed_count++;
		fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
		fread(&tmp_block, sizeof(VALUE_BLOCK), 1, mock_meta_cache->fptr);
		now_pos = tmp_block.next_block_pos;
	}
	EXPECT_EQ(2 * num_keys, reclaimed_count); // For each key, reclaim 2 value-block.
	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, ReplaceWithNotExistKey_Fail)
{
	int32_t ret;
	int32_t num_keys = MAX_KEY_HASH_ENTRY - 1;
	int32_t num_not_exist_keys = 5000;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];
	pair<string, string> mock_not_exist_xattr[num_not_exist_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	for (int32_t i = 0; i < num_not_exist_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 10); // key len is diff with inserted keys.
		gen_random_string(mock_value, value_size);
		mock_not_exist_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys and replace fail */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_value;

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, 0);
		ASSERT_EQ(0, ret);
	}

	for (int32_t i = 0; i < num_not_exist_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_not_exist_xattr[i].first.c_str(),
			mock_not_exist_xattr[i].second.c_str(),
			value_size, XATTR_REPLACE);
		ASSERT_EQ(-ENODATA, ret); // Fail when replacing with a non-existed key
	}

	/* Verify whether original keys exist */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[value_size + 10];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			value_size + 10, &actual_size);
		value_buffer[value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(0, mock_xattr_page->reclaimed_value_block);
	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}

TEST_F(insert_xattrTest, CreateExistKey_Fail)
{
	int32_t ret;
	int32_t num_keys = 1000;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys twice fail */
	for (int32_t i = 0; i < num_keys ; i++) {
		string mock_value;

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, 0);
		ASSERT_EQ(0, ret);

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(),
			value_size, XATTR_CREATE);
		ASSERT_EQ(-EEXIST, ret); // Fail since key exists
	}

	/* Verify whether keys still exist */
	for (int32_t i = 0; i < num_keys ; i++) {
		size_t actual_size;
		char value_buffer[value_size + 10];

		ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
			mock_xattr[i].first.c_str(), value_buffer,
			value_size + 10, &actual_size);
		value_buffer[value_size] = '\0';
		ASSERT_EQ(0, ret);
		ASSERT_EQ(value_size, actual_size);
		ASSERT_STREQ(mock_xattr[i].second.c_str(), value_buffer) <<
		"key is " << mock_xattr[i].first << ", value is " <<
		mock_xattr[i].second;
	}

	EXPECT_EQ(0, mock_xattr_page->reclaimed_value_block);
	EXPECT_EQ(num_keys, mock_xattr_page->namespace_page[USER].num_xattr);
}
/*
	End of unittest of insert_xattr()
 */

/*
	Unittest of get_xattr()
 */

class get_xattrTest : public XattrOperationBase {

};

TEST_F(get_xattrTest, KeyNotFound)
{
	const char *key = "test_key";
	const char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int32_t ret;

	/* Insert key and get value */
	insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
		USER, key, value, strlen(value), XATTR_CREATE);

	ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
		"test_key_not_found", buf, 100, &actual_size);

	EXPECT_EQ(-ENODATA, ret);
}

TEST_F(get_xattrTest, KeyFound_GetSizeSuccess)
{
	const char *key = "test_key";
	const char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int32_t ret;

	actual_size = 0;

	/* Insert key and get value */
	insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
		USER, key, value, strlen(value), XATTR_CREATE);

	ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
		key, buf, 0, &actual_size);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(strlen(value), actual_size);
}

TEST_F(get_xattrTest, KeyFound_RangeError)
{
	const char *key = "test_key";
	const char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int32_t ret;

	actual_size = 0;

	/* Insert key and get value */
	insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
		USER, key, value, strlen(value), XATTR_CREATE);

	ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
		key, buf, 1, &actual_size);

	EXPECT_EQ(-ERANGE, ret);
	EXPECT_EQ(strlen(value), actual_size);
}

TEST_F(get_xattrTest, KeyFound_GetValueSuccess)
{
	const char *key = "test_key";
	const char *value = "test_value";
	char buf[100] = {0};
	size_t actual_size;
	int32_t ret;

	actual_size = 0;

	/* Insert key and get value */
	insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos,
		USER, key, value, strlen(value), XATTR_CREATE);

	ret = get_xattr(mock_meta_cache, mock_xattr_page, USER,
		key, buf, 100, &actual_size);
	buf[strlen(value)] = '\0';

	EXPECT_EQ(0, ret);
	EXPECT_EQ(strlen(value), actual_size);
	EXPECT_STREQ(value, buf);
}
/*
	End of unittest of get_xattr()
 */

/*
	Unittest of list_xattr()
 */
class list_xattrTest : public XattrOperationBase {
protected:
	char *key_buf;

	void SetUp()
	{
		XattrOperationBase::SetUp();
		key_buf = NULL;
	}

	void TearDown()
	{
		if (key_buf)
			free(key_buf);
		XattrOperationBase::TearDown();
	}

	int32_t gen_mock_keys(int32_t num_keys, size_t value_size, int32_t namespace_len,
		size_t &total_needed_size, pair<string, string> *mock_xattr)
	{
		int32_t ret;

		for (int32_t i = 0; i < num_keys ; i++) {
			char mock_key[200];
			string mock_value;

			sprintf(mock_key, "test-key%d", i);
			gen_random_string(mock_value, value_size);
			mock_xattr[i] = make_pair(string(mock_key), mock_value);
			total_needed_size += (strlen(mock_key) + namespace_len + 1);

			ret = insert_xattr(mock_meta_cache, mock_xattr_page,
				xattr_page_pos, USER, mock_xattr[i].first.c_str(),
				mock_xattr[i].second.c_str(), value_size, 0);
			if (ret < 0)
				return -1;
		}

		return 0;
	}
};

TEST_F(list_xattrTest, GetNeededKeySizeSuccess)
{
	int32_t ret;
	int32_t num_keys = 2000;
	int32_t namespace_len = 0;
	size_t value_size = 30;
	size_t total_needed_size = 0;
	size_t actual_size;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys and insert them */
	namespace_len = strlen("user.");
	ASSERT_EQ(0, gen_mock_keys(num_keys, value_size, namespace_len,
		total_needed_size, mock_xattr));

	/* Run list_xattr() */
	ret = list_xattr(mock_meta_cache, mock_xattr_page, NULL, 0, &actual_size);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(total_needed_size, actual_size);
}

TEST_F(list_xattrTest, KeyBufferRangeError)
{
	int32_t ret;
	int32_t num_keys = 4000;
	int32_t namespace_len = 0;
	size_t value_size = 30;
	size_t total_needed_size = 0;
	size_t actual_size;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys and insert them */
	namespace_len = strlen("user.");
	ASSERT_EQ(0, gen_mock_keys(num_keys, value_size, namespace_len,
		total_needed_size, mock_xattr));

	key_buf = (char *) malloc(sizeof(char) * 20);

	/* Run list_xattr() */
	ret = list_xattr(mock_meta_cache, mock_xattr_page, key_buf,
		20, &actual_size);

	/* Verify */
	EXPECT_EQ(-ERANGE, ret);
}

TEST_F(list_xattrTest, ListAllKeySuccess)
{
	int32_t ret;
	int32_t num_keys = 4000;
	int32_t namespace_len = 0;
	size_t value_size = 30;
	size_t total_needed_size = 0;
	size_t actual_size;
	size_t head_index, tail_index;
	char *key_buf;
	bool check_number[num_keys];
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys and insert them */
	namespace_len = strlen("user.");
	ASSERT_EQ(0, gen_mock_keys(num_keys, value_size, namespace_len,
		total_needed_size, mock_xattr));

	key_buf = (char *) malloc(sizeof(char) * total_needed_size);
	/* Run list_xattr() */
	ret = list_xattr(mock_meta_cache, mock_xattr_page, key_buf,
		total_needed_size, &actual_size);

	/* Verify */
	ASSERT_EQ(0, ret) << strerror(-ret);
	EXPECT_EQ(total_needed_size, actual_size);

	memset(check_number, 0, sizeof(bool) * num_keys);
	head_index = tail_index = 0;
	while (tail_index < total_needed_size) {
		if (key_buf[tail_index] == '\0') {
			char tmp_key_name[100];
			int32_t verified_num;

			memcpy(tmp_key_name, key_buf + head_index,
				sizeof(char) * (tail_index - head_index));
			tmp_key_name[tail_index - head_index] = '\0';
			sscanf(tmp_key_name, "user.test-key%d", &verified_num);
			ASSERT_LT(verified_num, num_keys) << verified_num;
			check_number[verified_num] = true; // Record
			tail_index++;
			head_index = tail_index;

		} else {
			tail_index++;
		}
	}

	for (int32_t i = 0; i < num_keys; i++) // Check all keys are recorded
		ASSERT_EQ(true, check_number[i]) << i;
}
/*
	End of unittest of list_xattr()
 */

/*
	Unittest of remove_xattr()
 */
class remove_xattrTest : public XattrOperationBase {
protected:
	int32_t gen_mock_keys(int32_t &num_keys, pair<string, string> *mock_xattr,
		size_t &value_size)
	{
		int32_t ret;

		for (int32_t i = 0; i < num_keys; i++) {
			char mock_key[200];
			string mock_value;

			sprintf(mock_key, "test-key%d", i);
			gen_random_string(mock_value, value_size);
			mock_xattr[i] = make_pair(string(mock_key), mock_value);

			ret = insert_xattr(mock_meta_cache, mock_xattr_page,
				xattr_page_pos, USER, mock_xattr[i].first.c_str(),
				mock_xattr[i].second.c_str(), value_size, 0);
			if (ret < 0)
				return -1;
		}

		return 0;
	}
};

TEST_F(remove_xattrTest, RemovedEntryNotFound)
{
	int32_t ret;
	int32_t num_keys = MAX_KEY_HASH_ENTRY - 1;
	size_t value_size = 10;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys and insert them */
	ret = gen_mock_keys(num_keys, mock_xattr, value_size);
	ASSERT_EQ(0, ret);

	/* Run remove_xattr(), return -ENODATA */
	for (int32_t i = 0; i < 1000; i++) {
		char key_not_found[100];

		sprintf(key_not_found, "test-key-not-found%d", i);
		ret = remove_xattr(mock_meta_cache, mock_xattr_page,
			0, USER, key_not_found);

		ASSERT_EQ(-ENODATA, ret);
	}
}

TEST_F(remove_xattrTest, RemoveManyKeysSuccess)
{
	int32_t ret;
	int32_t num_keys = MAX_KEY_ENTRY_PER_LIST * MAX_KEY_HASH_ENTRY * 10;
	int32_t reclaimed_count, expected_reclaimed_count;
	size_t actual_size;
	size_t value_size = MAX_VALUE_BLOCK_SIZE * 3 - 1;
	int64_t xattr_pos, now_pos;
	pair<string, string> mock_xattr[num_keys];
	KEY_LIST_PAGE tmp_key_page;

	xattr_pos = 0;

	/* Generate mock keys and insert them */
	ret = gen_mock_keys(num_keys, mock_xattr, value_size);
	ASSERT_EQ(0, ret);
	// Traverse all key page and record expected num of key_pages
	// to be relcaimed later.

	expected_reclaimed_count = 0;
	for (int32_t hash_count = 0; hash_count < MAX_KEY_HASH_ENTRY ; hash_count++) {
		int64_t pos;
		NAMESPACE_PAGE *namespace_page;

		namespace_page = &(mock_xattr_page->namespace_page[USER]);
		pos = namespace_page->key_hash_table[hash_count];
		while(pos) {
			fseek(mock_meta_cache->fptr, pos, SEEK_SET);
			fread(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1,
					mock_meta_cache->fptr);
			expected_reclaimed_count++;
			pos = tmp_key_page.next_list_pos;
		}
	}

	cout << "Test: Begin to test remove_xattr()" << endl;
	/* Run remove_xattr() and check structure return -ENODATA */
	for (int32_t i = 0; i < num_keys; i++) {

		ret = remove_xattr(mock_meta_cache, mock_xattr_page,
			xattr_pos, USER, mock_xattr[i].first.c_str());
		ASSERT_EQ(0, ret);
		ASSERT_EQ(num_keys - i - 1,
			mock_xattr_page->namespace_page[USER].num_xattr);

		// Check structure when half and end
		if ((i + 1) % (num_keys / 3) == 0) {
			for (int32_t verify = 0; verify < num_keys ; verify++) {
				char value_buf[value_size + 1];

				ret = get_xattr(mock_meta_cache, mock_xattr_page,
					USER, mock_xattr[verify].first.c_str(),
					value_buf, value_size + 1, &actual_size);
				if (verify <= i)
					ASSERT_EQ(-ENODATA, ret);
				else
					ASSERT_EQ(0, ret);
			}
		}
	}

	/* Verify reclaimed value-block (Because replace with shorter value) */
	reclaimed_count = 0;
	now_pos = mock_xattr_page->reclaimed_value_block;
	while (now_pos) {
		VALUE_BLOCK tmp_block;

		reclaimed_count++;
		fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
		fread(&tmp_block, sizeof(VALUE_BLOCK), 1, mock_meta_cache->fptr);
		now_pos = tmp_block.next_block_pos;
	}
	EXPECT_EQ(3 * num_keys, reclaimed_count); // For each key, reclaim 3 value-block.

	reclaimed_count = 0;
	now_pos = mock_xattr_page->reclaimed_key_list_page;
	while (now_pos) {
		reclaimed_count++;
		fseek(mock_meta_cache->fptr, now_pos, SEEK_SET);
		fread(&tmp_key_page, sizeof(KEY_LIST_PAGE), 1, mock_meta_cache->fptr);
		now_pos = tmp_key_page.next_list_pos;
	}
	EXPECT_EQ(expected_reclaimed_count, reclaimed_count);

}
/*
	End of unittest of remove_xattr()
 */
