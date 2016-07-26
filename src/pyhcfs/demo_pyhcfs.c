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

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "parser.h"

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
	PORTABLE_DIR_ENTRY *ret_entry;
	uint64_t ret_num;
	int32_t i;
	RET_META meta_data;
	HCFS_STAT *stat_data = &meta_data.stat;

	puts("============================================");
	puts("list_external_volume(\"../../tests/unit_test/python/test_nexus_5x/fsmgr\", &ret_entry, &ret_num);");
	list_external_volume("../../tests/unit_test/python/test_nexus_5x/fsmgr", &ret_entry, &ret_num);
	for (i = 0; i < ret_num; i++)
		printf("%lu %s\n", ret_entry[i].inode, ret_entry[i].d_name);

	puts("============================================");
	puts("parse_meta(\"../../tests/unit_test/python/test_nexus_5x/meta\", &meta_data);");
	parse_meta("../../tests/unit_test/python/test_nexus_5x/meta", &meta_data);
	printf("%20s: %"PRId32"\n", "Result", meta_data.result);
	printf("%20s: %"PRId32"\n", "Type (0=dir, 1=file)", meta_data.file_type);
	printf("%20s: %"PRIu64"\n", "Child number", meta_data.child_number);
	printf("%20s: %"PRIu64"\n", "dev", stat_data->dev);
	printf("%20s: %"PRIu64"\n", "ino", stat_data->ino);
	printf("%20s: %"PRId32"\n", "mode", stat_data->mode);
	printf("%20s: %"PRIu64"\n", "nlink", stat_data->nlink);
	printf("%20s: %"PRId32"\n", "uid", stat_data->uid);
	printf("%20s: %"PRId32"\n", "gid", stat_data->gid);
	printf("%20s: %"PRId64"\n", "rdev", stat_data->rdev);
	printf("%20s: %"PRIu64"\n", "size", stat_data->size);
	printf("%20s: %"PRId64"\n", "blksize", stat_data->blksize);
	printf("%20s: %"PRId64"\n", "blocks", stat_data->blocks);
	printf("%20s: %s", "atime", ctime(&(stat_data->atime)));
	printf("%20s: %"PRIu64"\n", "atime_nsec", stat_data->atime_nsec);
	printf("%20s: %s", "mtime", ctime(&(stat_data->mtime)));
	printf("%20s: %"PRIu64"\n", "mtime_nsec", stat_data->mtime_nsec);
	printf("%20s: %s", "ctime", ctime(&(stat_data->ctime)));
	printf("%20s: %"PRIu64"\n", "ctime_nsec", stat_data->ctime_nsec);

	puts("============================================");

	puts("list_dir_inorder(\"../../tests/unit_test/python/test_nexus_5x/meta\"), 0, 0, 400, &end_pos, "
	     "&end_el, &(file_list[0])");
	int32_t num_walked = 0;
	int32_t num_children = 99;
	int32_t end_el = 0;
	int64_t end_pos = 0;
	PORTABLE_DIR_ENTRY file_list[400];

	while (TRUE) {
		num_walked = list_dir_inorder("../../tests/unit_test/python/test_nexus_5x/meta",
				end_pos, end_el, num_children, &end_pos, &end_el,
				&(file_list[0]));
		if (num_walked == 0) {
			printf("No more children can be listed\n");
			printf("Total %d children traversed\n", num_walked);
			printf("next page_pos is %ld, next el_no is %d\n", end_pos, end_el);
			break;
		} else if (num_walked < 0) {
			printf("Error %d\n", num_walked);
			break;
		}
		printf("Total %d children traversed\n", num_walked);
		printf("next page_pos is %ld, next el_no is %d\n", end_pos, end_el);
		for (i = 0; i < num_walked; i++)
			printf("%s\n", file_list[i].d_name);
	}
}
