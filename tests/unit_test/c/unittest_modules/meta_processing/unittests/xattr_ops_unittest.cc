#include <errno.h>
extern "C" {
#include "mock_param.h"
#include "xattr_ops.h"
#include "global.h"
#include "meta_mem_cache.h"
}
#include "string"
#include "gtest/gtest.h"


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

/*
	Unittest of insert_xattr()
 */
class insert_xattrTest : public ::testing::Test {
protected:
	META_CACHE_ENTRY_STRUCT *mock_meta_cache;
	XATTR_PAGE *mock_xattr_page;

	void SetUp()
	{
		mock_meta_cache = (META_CACHE_ENTRY_STRUCT *) 
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(mock_meta_cache, 0, sizeof(META_CACHE_ENTRY_STRUCT));

		mock_xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
		memset(mock_xattr_page, 0, sizeof(XATTR_PAGE));
	}

	void TearDown()
	{
		free(mock_meta_cache);
		free(mock_xattr_page);
	}

	void gen_random_string(std::string &str, const int len) {
		static const char alphanum[] =
			"0123456789"
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

TEST_F(insert_xattrTest, DefaultInsertManyKeys)
{
	int num_keys = 20000;
	std::string mock_keys[num_keys];

	/* Generate mock keys */
	for (int i = 0; i < num_keys ; i++) {
		gen_random_string(mock_keys[i], 25);
		std::cout << mock_keys[i] << std::endl;
	}
}

/*
	End of unittest of insert_xattr()
 */
