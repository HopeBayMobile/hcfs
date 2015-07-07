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
	int ret;
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
	int ret1, ret2;

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
	long long xattr_page_pos;
	char *meta_path;

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

	void gen_random_string(std::string &str, const int len) {
		static const char alphanum[] =
			"0123456789_.-"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		char s[len + 1];

		for (int i = 0; i < len; ++i) {
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
		const int &total_keys, int &index)
	{
		int num_insert;

		if (index + MAX_KEY_ENTRY_PER_LIST < total_keys)
			num_insert = MAX_KEY_ENTRY_PER_LIST;
		else
			num_insert = total_keys - index;

		for (int i = 0; i < num_insert ; i++) {
			strcpy(key_page.key_list[i].key, mock_keys[index].c_str());
			key_page.key_list[i].key_size = mock_keys[index].length();

			index++;
		}

		key_page.num_xattr = num_insert;
	}
};

TEST_F(find_key_entryTest, FirstKeyPageIsEmpty)
{
	KEY_LIST_PAGE target_key_page, prev_key_page;
	long long target_pos, prev_pos;
	int index;
	int ret;

	ret = find_key_entry(mock_meta_cache, 0, &target_key_page, &index, 
		&target_pos, "key", &prev_key_page, &prev_pos);

	EXPECT_EQ(1, ret);
	EXPECT_EQ(-1, index);
}

TEST_F(find_key_entryTest, KeysAllFound)
{
	KEY_LIST_PAGE target_key_page, prev_key_page, tmp_key_page;
	long long target_pos, prev_pos, first_key_page_pos;
	int index, now_index, count;
	int ret;
	int num_keys = MAX_KEY_ENTRY_PER_LIST * 10;
	string mock_keys[num_keys];

	/* Gen mock keys and mock key_pages */
	for (int i = 0 ; i < num_keys ; i++) {
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
	for (int i = 0; i < num_keys ; i++) {
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
	int ret;
	int num_keys = 60000;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 25);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}
	
	/* Insert keys */
	cout << "Test: Begin to insert " << num_keys << " key-value pairs" << endl;
	for (int i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, 0);
		ASSERT_EQ(0, ret);
	}
	cout << "Test: Insertion done." << endl;

	/* Verify */
	for (int i = 0; i < num_keys ; i++) {
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
}

TEST_F(insert_xattrTest, CreateKeySuccess)
{
	int ret;
	int num_keys = 600;
	size_t value_size = 300;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}
	
	/* Insert keys */
	for (int i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, XATTR_CREATE);
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int i = 0; i < num_keys ; i++) {
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
}

TEST_F(insert_xattrTest, ReplaceWithLongerValueSuccess)
{
	int ret;
	int num_keys = 600;
	size_t value_size = 30; // Original value size
	size_t replace_value_size = MAX_VALUE_BLOCK_SIZE * 3; // Replaced value size
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}
	
	/* Create keys and replace keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_value;
		
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, 0);
		ASSERT_EQ(0, ret);
		
		// Generate new value and replace later.
		gen_random_string(mock_value, replace_value_size);
		mock_xattr[i].second = mock_value;
	}
	
	for (int i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			replace_value_size, XATTR_REPLACE);
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int i = 0; i < num_keys ; i++) {
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
}

TEST_F(insert_xattrTest, ReplaceWithShorterValueSuccess)
{
	int ret;
	int num_keys = 600;
	size_t value_size = MAX_VALUE_BLOCK_SIZE * 3; // Original value size
	size_t replace_value_size = 20; // Replaced value size
	long long now_pos;
	int reclaimed_count;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}
	
	/* Create keys and replace keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_value;
		
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, 0);
		ASSERT_EQ(0, ret);
		
		// Generate new value and replace later.
		gen_random_string(mock_value, replace_value_size);
		mock_xattr[i].second = mock_value;
	}
	
	for (int i = 0; i < num_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			replace_value_size, XATTR_REPLACE);
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	for (int i = 0; i < num_keys ; i++) {
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
}

TEST_F(insert_xattrTest, ReplaceWithNotExistKey_Fail)
{
	int ret;
	int num_keys = 1000;
	int num_not_exist_keys = 5000;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];
	pair<string, string> mock_not_exist_xattr[num_not_exist_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}
	
	for (int i = 0; i < num_not_exist_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 10); // key len is diff with inserted keys.
		gen_random_string(mock_value, value_size);
		mock_not_exist_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys and replace fail */
	for (int i = 0; i < num_keys ; i++) {
		string mock_value;
		
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, 0);
		ASSERT_EQ(0, ret);
	}
	
	for (int i = 0; i < num_not_exist_keys ; i++) {
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_not_exist_xattr[i].first.c_str(), 
			mock_not_exist_xattr[i].second.c_str(), 
			value_size, XATTR_REPLACE);
		ASSERT_EQ(-ENODATA, ret); // Fail when replacing with a non-existed key
	}

	/* Verify whether original keys exist */
	for (int i = 0; i < num_keys ; i++) {
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
}

TEST_F(insert_xattrTest, CreateExistKey_Fail)
{
	int ret;
	int num_keys = 1000;
	size_t value_size = 30;
	pair<string, string> mock_xattr[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		string mock_key;
		string mock_value;

		gen_random_string(mock_key, 40);
		gen_random_string(mock_value, value_size);
		mock_xattr[i] = make_pair(mock_key, mock_value);
	}

	/* Create keys twice fail */
	for (int i = 0; i < num_keys ; i++) {
		string mock_value;
		
		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, 0);
		ASSERT_EQ(0, ret);

		ret = insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
			USER, mock_xattr[i].first.c_str(), mock_xattr[i].second.c_str(), 
			value_size, XATTR_CREATE);
		ASSERT_EQ(-EEXIST, ret);
	}

	/* Verify whether keys still exist */
	for (int i = 0; i < num_keys ; i++) {
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
	char *key = "test_key";
	char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int ret;

	/* Insert key and get value */
	insert_xattr(mock_meta_cache, mock_xattr_page, xattr_page_pos, 
		USER, key, value, strlen(value), XATTR_CREATE);
	
	ret = get_xattr(mock_meta_cache, mock_xattr_page, USER, 
		"test_key_not_found", buf, 100, &actual_size);
	
	EXPECT_EQ(-ENODATA, ret);
}

TEST_F(get_xattrTest, KeyFound_GetSizeSuccess)
{
	char *key = "test_key";
	char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int ret;

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
	char *key = "test_key";
	char *value = "test_value";
	char buf[100];
	size_t actual_size;
	int ret;

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
	char *key = "test_key";
	char *value = "test_value";
	char buf[100] = {0};
	size_t actual_size;
	int ret;

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
