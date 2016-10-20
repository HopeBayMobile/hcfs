/* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "alias.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"

#define MAX_FILENAME_LEN            255
#define MAX_REAL_INODE              100

extern "C" int32_t write_log(int32_t level, const char *format, ...);

typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	int32_t index;
} DIR_ENTRY;

ino_t real_ino = 1000;


/* Begin of the test case for the function seek_and_add_in_alias_group */

class SeekAndAddTest : public ::testing::Test {
protected:
	virtual void SetUp() {
	}

	virtual void TearDown() {
	}
};

TEST_F(SeekAndAddTest, SingleRealInode)
{
	int32_t ret_val;
	ino_t alias_ino[5];

	init_alias_group();

	/* Testing NULL pointer. */
	ret_val = seek_and_add_in_alias_group(real_ino,
				NULL, NULL, NULL);
	ASSERT_NE(0, ret_val);

	/* Testing new alias inode. */
	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino[0], "ABCDEF", "abcdef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino[0]));

	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino[1], "ABCDEF", "ABCdef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino[1]));

	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino[2], "ABCDEF", "abcDEF");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino[2]));

	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino[3], "ABCDEF", "AbcDef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino[3]));

	/* Testing existed alias inode. */
	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino[4], "ABCDEF", "abcdef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino[4]));
	ASSERT_EQ(alias_ino[0], alias_ino[4]);

	/* Free the inodes. */
	ret_val = delete_in_alias_group(real_ino);
	ASSERT_EQ(0, ret_val);
}

TEST_F(SeekAndAddTest, MultipleRealInodes)
{
	int32_t ret_val;
	ino_t alias_ino, backup_ino;

	init_alias_group();

	/* Testing new alias inode. */
	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino, "ABCDEF", "abcdef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));
	backup_ino = alias_ino;

	ret_val = seek_and_add_in_alias_group(real_ino + 1,
				&alias_ino, "Wxyz123", "wxYZ123");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));

	ret_val = seek_and_add_in_alias_group(real_ino + 2,
				&alias_ino, "Qwert", "qwerT");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));

	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino, "ABCDEF", "AbcDef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));

	ret_val = seek_and_add_in_alias_group(real_ino + 2,
				&alias_ino, "Qwert", "qWErt");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));

	/* Testing existed alias inode. */
	ret_val = seek_and_add_in_alias_group(real_ino,
				&alias_ino, "ABCDEF", "abcdef");
	ASSERT_EQ(0, ret_val);
	ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));
	ASSERT_EQ(backup_ino, alias_ino);

	/* Free the inodes. */
	ret_val = delete_in_alias_group(real_ino);
	ASSERT_EQ(0, ret_val);
	ret_val = delete_in_alias_group(real_ino + 1);
	ASSERT_EQ(0, ret_val);
	ret_val = delete_in_alias_group(real_ino + 2);
	ASSERT_EQ(0, ret_val);
}

/* End of the test case for the function seek_and_add_in_alias_group */

/* Begin of the test case for all alias operations */

class AliasInodeOperationTest : public ::testing::Test {
protected:
	DIR_ENTRY dir_entry[MAX_REAL_INODE];
	char selfname[MAX_FILENAME_LEN+1];
	char lowercase[MAX_FILENAME_LEN+1];
	int32_t count, ret_val, selected, total_inodes;

	virtual void SetUp() {
		ino_t alias_ino;

		srand(time(NULL));
		init_alias_group();

		/* Generate random strings real inode. */
		for (count = 0; count < MAX_REAL_INODE; count++) {
			real_ino = real_ino + 1;
			dir_entry[count].d_ino = real_ino;
			any_string(selfname, count + 3);
			strcpy(dir_entry[count].d_name, selfname);
		}
	}

	virtual void TearDown() {
		/* Free all alias inodes. */
		for (count = 0; count < MAX_REAL_INODE; count++) {
			delete_in_alias_group(dir_entry[count].d_ino);
		}
	}

	void create_alias_inodes(int32_t num1, int32_t num2,
	                         int32_t min_selected, DIR_ENTRY *inode) {
		int32_t loop1, loop2, num_inodes = 0;
		ino_t this_ino;

		if (min_selected >= MAX_REAL_INODE)
			min_selected = MAX_REAL_INODE - 1;

		for (loop1 = 0; loop1 < num1; loop1++) {
			selected = rand() % MAX_REAL_INODE;
			if (selected < min_selected)
				selected = min_selected;
			for (loop2 = 0; loop2 < num2; ) {
				any_alias_name(dir_entry[selected].d_name, selfname);
				if (!strcmp(dir_entry[selected].d_name, selfname))
					continue;
				seek_and_add_in_alias_group(dir_entry[selected].d_ino,
					&this_ino, dir_entry[selected].d_name, selfname);
				loop2++;
				/* Gurantee there is at least one inode selected. */
				if (inode && (num_inodes < total_inodes)
					&& ((total_inodes == 1) || (rand() % 2))) {
					inode[num_inodes].d_ino = this_ino;
					strcpy(inode[num_inodes].d_name, selfname);
					inode[num_inodes].index = selected;
					num_inodes++;
				}
			}
		}
		total_inodes = num_inodes;
	}

	void any_string(char *selfname, int32_t length) {
		int index = 0;
		static const char letter[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		/* Skip number at first 3 bytes. */
		for (index = 0; index < 3; index++) {
			selfname[index] = letter[rand() % 52 + 10];
		}
		for (; index < length; index++) {
			selfname[index] = letter[rand() % 62];
		}
		selfname[length] = '\0';
	}

	void any_alias_name(char *real_name, char *alias_name) {
		int32_t index, length;

		length = strlen(real_name);
		for (index = 0; index < length; index++) {
			if (real_name[index] <= '9') {
				alias_name[index] = real_name[index];
			} else if (real_name[index] <= 'Z') {
				alias_name[index] = real_name[index] + 32 * (rand() % 2);
			} else {
				alias_name[index] = real_name[index] - 32 * (rand() % 2);
			}
		}
		alias_name[index] = '\0';
	}

	void to_lowercase(const char *org_name, char *lower_case) {
		int i;
		if (!org_name || !lower_case)
			return;

		for (i = 0; org_name[i]; i++) {
			lower_case[i] = tolower(org_name[i]);
		}
		lower_case[i] = '\0';
	}

	/* Thread handle functions */
	void handle_inodes(int32_t handle_type);

	static void *handle_inode_thread(void *data) {
		static int32_t handle_type = 0;
		handle_type = handle_type % 3 + 1;
		((AliasInodeOperationTest *)data)->handle_inodes(handle_type);
		return NULL;
	}
};

/* Testing get_real_ino_in_alias_group function */
TEST_F(AliasInodeOperationTest, GetRealInodeNumber)
{
	ino_t this_ino, ret_ino;
	DIR_ENTRY any_inode[5];

	/* Testing non-existed real inode number. */
	this_ino = real_ino << 1;
	ret_ino = get_real_ino_in_alias_group(this_ino);
	ASSERT_EQ(this_ino, ret_ino);

	/* Testing non-existed alias inode number. */
	this_ino = MIN_ALIAS_VALUE << 1;
	ret_ino = get_real_ino_in_alias_group(this_ino);
	ASSERT_EQ(this_ino, ret_ino);

	/* Add some alias inodes for a random real inode. */
	total_inodes = 5;
	create_alias_inodes(1, 10, 0, any_inode);
	for (count = 0; count < total_inodes; count++) {
		/* Testing if we can get correct real inode number back. */
		ret_ino = get_real_ino_in_alias_group(any_inode[count].d_ino);
		ASSERT_EQ(ret_ino, dir_entry[any_inode[count].index].d_ino);
	}
}

/* Testing get_name_in_alias_group function basic check*/
TEST_F(AliasInodeOperationTest, GetInodeBasicCheck)
{
	char *alias_name;
	ino_t alias_ino;

	/* Add some alias inodes for a random real inode. */
	create_alias_inodes(1, 10, 70, NULL);

	/* Get the name back and compare it. */
	to_lowercase(dir_entry[selected].d_name, lowercase);
	do {
		alias_name = get_name_in_alias_group(dir_entry[selected].d_ino,
						lowercase, &alias_ino);
		if (alias_name == NULL)
			break;
		ASSERT_STRNE(NULL, alias_name);
		ASSERT_STRCASEEQ(dir_entry[selected].d_name, alias_name);
		ASSERT_EQ(1, IS_ALIAS_INODE(alias_ino));
		free(alias_name);
	} while (TRUE);
}

/* Testing update_in_alias_group function basic check. */
TEST_F(AliasInodeOperationTest, UpdateInodeBasicCheck)
{
	/* Add some alias inodes for a random real inode. */
	create_alias_inodes(1, 10, 0, NULL);

	/* Testing NULL pointer. */
	ret_val = update_in_alias_group(dir_entry[selected].d_ino, NULL);
	ASSERT_NE(0, ret_val);

	/* Testing non-existed real inode. */
	strcpy(selfname, "non-existed-file");
	ret_val = update_in_alias_group(real_ino << 1, selfname);
	ASSERT_NE(0, ret_val);

	/* Testing non-existed alias inode. */
	ret_val = update_in_alias_group(dir_entry[selected].d_ino, selfname);
	ASSERT_EQ(0, ret_val);
}

/* Testing if we can update the real inode to any alias name. */
TEST_F(AliasInodeOperationTest, UpdateRealInodeName)
{
	ino_t this_ino;
	DIR_ENTRY any_inode;
	char *real_name;

	for (count = 0; count < 100; count++) {
		/* Add some alias inodes for a random real inode. */
		total_inodes = 1;
		create_alias_inodes(1, rand() % 11 + 5, 0, &any_inode);

		/* Testing if we can update the name correctly. */
		ret_val = update_in_alias_group(dir_entry[selected].d_ino,
					any_inode.d_name);
		ASSERT_EQ(0, ret_val);

		/* Get the name back and compare it. */
		to_lowercase(dir_entry[selected].d_name, lowercase);
		real_name = get_name_in_alias_group(dir_entry[selected].d_ino,
						lowercase, NULL);
		ASSERT_STRNE(NULL, real_name);
		ASSERT_STREQ(any_inode.d_name, real_name);
		free(real_name);
	}
}

/* Testing delete_in_alias_group function */
TEST_F(AliasInodeOperationTest, DeleteSingleAliasInode)
{
	ino_t this_ino;
	DIR_ENTRY any_inode;

	/* Add some alias inodes for a random real inode. */
	total_inodes = 1;
	create_alias_inodes(1, 10, 0, &any_inode);

	/* Testing non-existed alias inode number. */
	this_ino = MIN_ALIAS_VALUE << 1;
	ret_val = delete_in_alias_group(this_ino);
	ASSERT_NE(0, ret_val);

	/* Testing delete any alias inode. */
	ret_val = delete_in_alias_group(any_inode.d_ino);
	ASSERT_EQ(0, ret_val);

	/* Testing delete a removed alias inode. */
	ret_val = delete_in_alias_group(any_inode.d_ino);
	ASSERT_EQ(0, ret_val);
}

/* Testing delete_in_alias_group function */
TEST_F(AliasInodeOperationTest, DeleteMultipleAliasInodes)
{
	ino_t this_ino;
	DIR_ENTRY any_inode[10];

	/* Add some alias inodes for some random real inodes. */
	total_inodes = 10;
	create_alias_inodes(20, 10, 0, any_inode);

	/* Testing delete some alias inodes and check if really removed. */
	for (count = 0; count < total_inodes; count++) {
		ret_val = delete_in_alias_group(any_inode[count].d_ino);
		ASSERT_EQ(0, ret_val);

		ret_val = delete_in_alias_group(any_inode[count].d_ino);
		ASSERT_EQ(0, ret_val);
	}
}

/* Testing delete_in_alias_group function */
TEST_F(AliasInodeOperationTest, DeleteSingleRealInode)
{
	ino_t this_ino;

	/* Add some alias inodes for a random real inode. */
	create_alias_inodes(1, 10, 0, NULL);

	/* Testing non-existed real inode number. */
	this_ino = real_ino << 1;
	ret_val = delete_in_alias_group(this_ino);
	ASSERT_NE(0, ret_val);

	/* Testing delete real inodes. */
	ret_val = delete_in_alias_group(dir_entry[selected].d_ino);
	ASSERT_EQ(0, ret_val);

	/* Testing delete a removed real inode. */
	ret_val = delete_in_alias_group(dir_entry[selected].d_ino);
	ASSERT_NE(0, ret_val);
}

/* Testing delete_in_alias_group function */
TEST_F(AliasInodeOperationTest, DeleteMultipleRealInodes)
{
	DIR_ENTRY any_inode[10];
	char *real_name;

	/* Add some alias inodes for some random real inodes. */
	total_inodes = 10;
	create_alias_inodes(20, 10, 0, any_inode);

	/* Testing delete some real inodes and check if really removed. */
	for (count = 0; count < total_inodes; count++) {
		selected = any_inode[count].index;
		/* Get the name back and compare it. */
		to_lowercase(dir_entry[selected].d_name, lowercase);
		real_name = get_name_in_alias_group(dir_entry[selected].d_ino,
						lowercase, NULL);
		if (real_name != NULL) {
			free(real_name);
			ret_val = delete_in_alias_group(dir_entry[selected].d_ino);
			ASSERT_EQ(0, ret_val);

			ret_val = delete_in_alias_group(dir_entry[selected].d_ino);
			ASSERT_NE(0, ret_val);
		}
	}
}

/* Testing create a huge number of alias inodes */
TEST_F(AliasInodeOperationTest, CreateOneMillionRandomAliasInodes)
{
	/* Random create One million alias inodes. */
	create_alias_inodes(10000, 100, 0, NULL);
}

/* Testing random create, update, delete inodes */
TEST_F(AliasInodeOperationTest, ConcurrentHandleInodesInMultiThreads)
{
	pthread_t tid[6];
	int32_t index;

	/* Create some threads to execute create, update, delete inodes. */
	for (index = 0; index < 6; index++)
		pthread_create(&tid[index], NULL, handle_inode_thread, this);

	/* Wait all threads complete. */
	for (index = 0; index < 6; index++)
		pthread_join(tid[index], NULL);
}

void AliasInodeOperationTest::handle_inodes(int32_t handle_type)
{
	int32_t loop, index;
	char alias_name[MAX_FILENAME_LEN+1];

	switch (handle_type) {
	/* Create random inodes. */
	case 1:
		for (loop = 0; loop < 1000; loop++) {
			create_alias_inodes(10, 10, rand() % MAX_REAL_INODE, NULL);
			usleep(10);
		}
		break;
	/* Update inode's name. */
	case 2:
		for (loop = 0; loop < 1000; loop++) {
			index = rand() % MAX_REAL_INODE;
			any_alias_name(dir_entry[index].d_name, alias_name);
			update_in_alias_group(dir_entry[index].d_ino, alias_name);
			usleep(10);
		}
		break;
	/* Delete random inode. */
	case 3:
		for (loop = 0; loop < 1000; loop++) {
			index = rand() % MAX_REAL_INODE;
			delete_in_alias_group(dir_entry[index].d_ino);
			usleep(10);
		}
		break;
	}
}

/* End of the test case for all alias operations */
