/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.c
* Abstract:
*	The c source file for python API, function are expected to run
*	without hcfs daemon as a independent .so library.
*
* Revision History
*	2016/6/16 Jethro add list_external_volume from list_filesystem
*
**************************************************************************/

#define _GNU_SOURCE
#include "parser.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define TEST_FILE(file) "test_data/v2/android/" #file

#define test_target "test_data/v2/android"
void test_list_file_blocks()
{
	int32_t ret_val = 0;
	int64_t vol_usage = 0;
	int64_t ret_num, i, inode;
	PORTABLE_BLOCK_NAME *list;
	char * s = NULL;

	printf("\n\nDemo list_file_blocks(\"%s\");\n", TEST_FILE(meta_isreg));
	puts("============================================");
	ret_val =
	    list_file_blocks(TEST_FILE(meta_isreg), &list, &ret_num, &inode);
	if (ret_val < 0)
		printf("Failed to list file blocks, errno %d\n", errno);
	else {
		for (i = 0; i < ret_num; i++) {
			printf("block name %lu_%lu\n", list[i].block_num,
			       list[i].block_seq);
		}
	}
	return;
}
void test_get_vol_usage()
{
	int32_t ret_val = 0;
	int64_t vol_usage = 0;
	char * s = NULL;

	printf("\n\nDemo get_vol_usage(\"%s\")\n", TEST_FILE(FSstat));
	puts("============================================");
	ret_val = get_vol_usage(TEST_FILE(FSstat), &vol_usage);
	if (ret_val < 0)
		printf("Failed to get volume usage, errno %d\n", errno);
	else
		printf("Volume usage is %ld\n", vol_usage);
	return;
}
void test_list_dir_inorder()
{
	int32_t idx, ret_code;
	int32_t num_walked = 0;
	int32_t num_children = 99;
	int32_t end_el = 0;
	int64_t end_pos = 0;
	PORTABLE_DIR_ENTRY file_list[400];
	char * s = NULL;

	asprintf(&s, "%s/meta_isdir", test_target);

	printf("\n\nDemo list_dir_inorder(\"%s\"), 0, 0, 400, &end_pos, "
	       "&end_el, &(file_list[0]);\n",
	       TEST_FILE(meta_isdir));
	puts("============================================");

	while (TRUE) {
		ret_code = list_dir_inorder(
		    TEST_FILE(meta_isdir), end_pos, end_el, num_children,
		    &end_pos, &end_el, &num_walked, &(file_list[0]));
		if (ret_code < 0) {
			printf("Failed to list dir\n");
			break;
		}
		if (num_walked == 0) {
			printf("No more children can be listed\n");
			printf("Total %d children traversed\n", num_walked);
			printf("next page_pos is %ld, next el_no is %d\n",
			       end_pos, end_el);
			break;
		} else if (num_walked < 0) {
			printf("Error %d\n", num_walked);
			break;
		}
		printf("Total %d children traversed\n", num_walked);
		printf("next page_pos is %ld, next el_no is %d\n", end_pos,
		       end_el);
		for (idx = 0; idx < num_walked; idx++)
			printf("%s ", file_list[idx].d_name);
		puts("\n");
	}
}
void test_parse_meta()
{
	PORTABLE_DIR_ENTRY *ret_entry;
	uint64_t ret_num;
	int32_t i;
	RET_META meta_data = {0};
	HCFS_STAT *stat_data = &meta_data.stat;
	char * s = NULL;

	asprintf(&s, "%s/FSstat", test_target);

	printf("\n\nDemo list_volume(\"%s\", &ret_entry, &ret_num);\n",
	       TEST_FILE(fsmgr));
	puts("============================================");
	list_volume(TEST_FILE(fsmgr), &ret_entry, &ret_num);
	for (i = 0; i < ret_num; i++)
		printf("%lu %s\n", ret_entry[i].inode, ret_entry[i].d_name);

	printf("\n\nDemo parse_meta(\"%s\", &meta_data);\n",
	       TEST_FILE(meta_isdir));
	puts("============================================");
	parse_meta(TEST_FILE(meta_isdir), &meta_data);
	printf("%20s: %" PRId32 "\n", "Result", meta_data.result);
	printf("%20s: %" PRId32 "\n", "Type (0=dir, 1=file)",
	       meta_data.file_type);
	printf("%20s: %" PRIu64 "\n", "Child number", meta_data.child_number);
	printf("%20s: %" PRIu64 "\n", "dev", stat_data->dev);
	printf("%20s: %" PRIu64 "\n", "ino", stat_data->ino);
	printf("%20s: %" PRId32 "\n", "mode", stat_data->mode);
	printf("%20s: %" PRIu64 "\n", "nlink", stat_data->nlink);
	printf("%20s: %" PRId32 "\n", "uid", stat_data->uid);
	printf("%20s: %" PRId32 "\n", "gid", stat_data->gid);
	printf("%20s: %" PRId64 "\n", "rdev", stat_data->rdev);
	printf("%20s: %" PRIu64 "\n", "size", stat_data->size);
	printf("%20s: %" PRId64 "\n", "blksize", stat_data->blksize);
	printf("%20s: %" PRId64 "\n", "blocks", stat_data->blocks);
	printf("%20s: %s", "atime", ctime(&(stat_data->atime)));
	printf("%20s: %" PRIu64 "\n", "atime_nsec", stat_data->atime_nsec);
	printf("%20s: %s", "mtime", ctime(&(stat_data->mtime)));
	printf("%20s: %" PRIu64 "\n", "mtime_nsec", stat_data->mtime_nsec);
	printf("%20s: %s", "ctime", ctime(&(stat_data->ctime)));
	printf("%20s: %" PRIu64 "\n", "ctime_nsec", stat_data->ctime_nsec);
}
/************************************************************************
*
* Function name: main
*       Summary: it will test functions on executing, only used when
*                compiled without pyhcs.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int main(void)
{
	test_parse_meta();
	test_list_dir_inorder();
	test_get_vol_usage();
	test_list_file_blocks();
}
