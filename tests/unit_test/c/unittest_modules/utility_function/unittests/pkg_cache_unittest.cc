#include "gtest/gtest.h"
#include "cstdlib"
#include <semaphore.h>
#include <string.h>
extern "C" {
#include "pkg_cache.h"
#include "fuseop.h"
#include <errno.h>
}

int32_t hash_pkg(const char *input)
{
	int32_t hash = 5381;
	int32_t index;

	index = 0;
	while (input[index]) {
		hash = (((hash << 5) + hash + input[index]) &
				(PKG_HASH_SIZE - 1));
		index++;
	}

	return hash;
}

/*
 * Unittest of init_pkg_cache()
 */
class init_pkg_cacheTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(init_pkg_cacheTest, InitSuccess)
{
	int value;

	init_pkg_cache();

	sem_getvalue(&pkg_cache.pkg_cache_lock, &value);
	EXPECT_EQ(1, value);
}
/*
 * End of unittest of init_pkg_cache()
 */

/*
 * Unittest of insert_cache_pkg()
 */
class insert_cache_pkgTest : public ::testing::Test {
protected:
	void SetUp()
	{
		init_pkg_cache();
	}

	void TearDown()
	{
		destroy_pkg_cache();
	}
};

TEST_F(insert_cache_pkgTest, InsertSomething)
{
	uid_t uids[] = {2, 4, 6, 8};
	std::string pkgname[] = {"a", "b", "c", "d"};
	int hash[4];

	for (int i = 0; i < 4; i++) {
		insert_cache_pkg(pkgname[i].c_str(), uids[i]);
		hash[i] = hash_pkg(pkgname[i].c_str());
	}

	for (int i = 0; i < 4; i++) {
		EXPECT_EQ(1, pkg_cache.pkg_hash[hash[i]].num_pkgs);
		EXPECT_STREQ(pkgname[i].c_str(),
			pkg_cache.pkg_hash[hash[i]].first_pkg_entry->pkgname);
		EXPECT_EQ(uids[i],
			pkg_cache.pkg_hash[hash[i]].first_pkg_entry->pkguid);
		EXPECT_EQ(0,
			pkg_cache.pkg_hash[hash[i]].first_pkg_entry->next);
	}

	EXPECT_EQ(4, pkg_cache.num_cache_pkgs);
}

TEST_F(insert_cache_pkgTest, ElementLimitExceeds)
{
	PKG_CACHE_ENTRY *now;
	uid_t uids[MAX_PKG_ENTRIES + 2];
	std::string pkgname[MAX_PKG_ENTRIES + 2];
	char pkg_name[300];
	int idx, k = 0;

	/* Generate mock pkgname and uid in the same hash bucket */
	for (int i = 0; k < MAX_PKG_ENTRIES + 2; i++) {
		sprintf(pkg_name, "%d", i);
		if (hash_pkg(pkg_name) == 0) {
			pkgname[k] = std::string(pkg_name);
			uids[k] = k;
			k++;
		}
	}

	/* Insert all of them */
	for (int i = 0; i < k; i++)
		insert_cache_pkg(pkgname[i].c_str(), uids[i]);

	/* Verify */
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.pkg_hash[0].num_pkgs);
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.num_cache_pkgs);

	now = pkg_cache.pkg_hash[0].first_pkg_entry;
	idx = MAX_PKG_ENTRIES + 1; /* last one */
	while (now) {
		EXPECT_STREQ(pkgname[idx].c_str(), now->pkgname);
		EXPECT_EQ(uids[idx], now->pkguid);
		now = now->next;
		idx--;
	}
	EXPECT_EQ(1, idx);
}


TEST_F(insert_cache_pkgTest, InsertExistEntry)
{
	PKG_CACHE_ENTRY *now;
	uid_t uids[MAX_PKG_ENTRIES];
	std::string pkgname[MAX_PKG_ENTRIES];
	char pkg_name[300];
	int idx, k = 0;

	/* Generate mock pkgname and uid in the same hash bucket */
	for (int i = 0; k < MAX_PKG_ENTRIES; i++) {
		sprintf(pkg_name, "%d", i);
		if (hash_pkg(pkg_name) == 0) {
			pkgname[k] = std::string(pkg_name);
			uids[k] = k;
			k++;
		}
	}
	/* Insert all of them */
	for (int i = 0; i < k; i++)
		insert_cache_pkg(pkgname[i].c_str(), uids[i]);

	/* head->7,6,5,4,3,2,1,0 */
	insert_cache_pkg(pkgname[MAX_PKG_ENTRIES - 1].c_str(), uids[MAX_PKG_ENTRIES - 1]);
	/* head->7,6,5,4,3,2,1,0 */
	insert_cache_pkg(pkgname[MAX_PKG_ENTRIES / 2].c_str(), uids[MAX_PKG_ENTRIES / 2]);
	/* head->4,7,6,5,3,2,1,0 */
	insert_cache_pkg(pkgname[0].c_str(), uids[0]);
	/* head->0,4,7,6,5,3,2,1 */

	/* Verify */
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.pkg_hash[0].num_pkgs);
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.num_cache_pkgs);
	now = pkg_cache.pkg_hash[0].first_pkg_entry;
	EXPECT_STREQ(pkgname[0].c_str(), now->pkgname);
	EXPECT_EQ(uids[0], now->pkguid);
	now = now->next;
	EXPECT_STREQ(pkgname[MAX_PKG_ENTRIES / 2].c_str(), now->pkgname);
	EXPECT_EQ(uids[MAX_PKG_ENTRIES / 2], now->pkguid);
	now = now->next;
	idx = MAX_PKG_ENTRIES - 1;
	while (now) {
		if (idx == (MAX_PKG_ENTRIES / 2) || idx == 0) {
			idx--;
			continue;
		}
		EXPECT_STREQ(pkgname[idx].c_str(), now->pkgname);
		EXPECT_EQ(uids[idx], now->pkguid);
		now = now->next;
		idx--;
	}
	EXPECT_EQ(0, idx);
}
/*
 * End of unittest of insert_cache_pkg()
 */

/*
 * Unittest of lookup_cache_pkg()
 */
class lookup_cache_pkgTest : public ::testing::Test {
protected:
	void SetUp()
	{
		init_pkg_cache();
	}

	void TearDown()
	{
		destroy_pkg_cache();
	}
};

TEST_F(lookup_cache_pkgTest, LookupEmptyCache)
{
	char pkg_name[300];
	uid_t uid;
	int ret;

	for (int i = 0; i < 10000 ; i++) {
		sprintf(pkg_name, "%d", i);
		ret = lookup_cache_pkg(pkg_name, &uid);
		ASSERT_EQ(-ENOENT, ret);
	}
}

TEST_F(lookup_cache_pkgTest, LookupHitNothing)
{
	PKG_CACHE_ENTRY *now;
	char pkg_name[300];
	uid_t uid;
	uid_t uids[MAX_PKG_ENTRIES];
	std::string pkgname[MAX_PKG_ENTRIES];
	int ret, idx, k = 0;

	/* Generate mock pkgname and uid in the same hash bucket */
	for (int i = 0; k < MAX_PKG_ENTRIES; i++) {
		sprintf(pkg_name, "%d", i);
		if (hash_pkg(pkg_name) == 0) {
			pkgname[k] = std::string(pkg_name);
			uids[k] = k;
			insert_cache_pkg(pkgname[k].c_str(), uids[k]);
			k++;
		}
	}

	/* Hit nothing */
	for (int i = 10000; i < 20000 ; i++) {
		sprintf(pkg_name, "%d", i);
		ret = lookup_cache_pkg(pkg_name, &uid);
		ASSERT_EQ(-ENOENT, ret);
	}

	/* Check structure */
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.pkg_hash[0].num_pkgs);
	EXPECT_EQ(MAX_PKG_ENTRIES, pkg_cache.num_cache_pkgs);

	now = pkg_cache.pkg_hash[0].first_pkg_entry;
	idx = MAX_PKG_ENTRIES - 1; /* last one */
	while (now) {
		EXPECT_STREQ(pkgname[idx].c_str(), now->pkgname);
		EXPECT_EQ(uids[idx], now->pkguid);
		now = now->next;
		idx--;
	}
	EXPECT_EQ(-1, idx);


}
