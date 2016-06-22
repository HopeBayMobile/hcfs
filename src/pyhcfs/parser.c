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
	puts("list_external_volume(\"testdata/fsmgr\", &ret_entry, &ret_num);");
	list_external_volume("testdata/fsmgr", &ret_entry, &ret_num);
	for (i = 0; i < ret_num; i++)
		printf("%lu %s\n", ret_entry[i].inode, ret_entry[i].name);
	
	puts("============================================");
	puts("parse_meta(\"testdata/meta423\", &meta_data);");
	parse_meta("testdata/meta423", &meta_data);
	printf("%20s: %d\n", "Result", meta_data.result);
	printf("%20s: %d\n", "Type (0=dir, 1=file)", meta_data.file_type);
	printf("%20s: %lu\n", "Child number", meta_data.child_number);
	printf("%20s: %lu\n", "dev", stat_data->dev);
	printf("%20s: %lu\n", "ino", stat_data->ino);
	printf("%20s: %d\n", "mode", stat_data->mode);
	printf("%20s: %lu\n", "nlink", stat_data->nlink);
	printf("%20s: %d\n", "uid", stat_data->uid);
	printf("%20s: %d\n", "gid", stat_data->gid);
	printf("%20s: %ld\n", "rdev", stat_data->rdev);
	printf("%20s: %lu\n", "size", stat_data->size);
	printf("%20s: %ld\n", "blksize", stat_data->blksize);
	printf("%20s: %ld\n", "blocks", stat_data->blocks);
	printf("%20s: %s", "atime", ctime(&(stat_data->atime)));
	printf("%20s: %lu\n", "atime_nsec", stat_data->atime_nsec);
	printf("%20s: %s", "mtime", ctime(&(stat_data->mtime)));
	printf("%20s: %lu\n", "mtime_nsec", stat_data->mtime_nsec);
	printf("%20s: %s", "ctime", ctime(&(stat_data->ctime)));
	printf("%20s: %lu\n", "ctime_nsec", stat_data->ctime_nsec);

	puts("============================================");
	puts("list_dir_inorder(\"testdata/meta423\")");
}

/************************************************************************
*
* Function name: list_external_volume
*        Inputs: int32_t buf_num,
*                DIR_ENTRY *ret_entry,
*                int32_t *ret_num
*       Summary: load fsmgr file and return all external volume
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_external_volume(char *fs_mgr_path,
			     PORTABLE_DIR_ENTRY **entry_array_ptr,
			     uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;
	int32_t meta_fd = open(fs_mgr_path, O_RDONLY);
	ssize_t return_code;
	PORTABLE_DIR_ENTRY *ret_entry;

	if (meta_fd == -1)
		return -1;

	return_code = pread(meta_fd, &tmp_head, sizeof(DIR_META_TYPE), 16);
	if (return_code == -1) {
		close(meta_fd);
		return return_code;
	}

	/* Initialize B-tree walk by first loading the first node of the
	 * tree walk.
	 */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		return_code = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (return_code == -1) {
			close(meta_fd);
			return return_code;
		}
		num_walked += tpage.num_entries;
		next_node_pos = tpage.tree_walk_next;
	}
	/* allocate memory for dir_entries */
	ret_entry = (PORTABLE_DIR_ENTRY *)malloc(sizeof(PORTABLE_DIR_ENTRY) *
						 num_walked);
	*entry_array_ptr = ret_entry;

	/* load dir_entries */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		return_code = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (return_code == -1) {
			close(meta_fd);
			return return_code;
		}
		for (count = 0; count < tpage.num_entries; count++) {
			switch (tpage.dir_entries[count].d_type) {
			case ANDROID_EXTERNAL:
			case ANDROID_MULTIEXTERNAL:
				ret_entry[num_walked].inode =
				    tpage.dir_entries[count].d_ino;
				strncpy(ret_entry[num_walked].name,
					tpage.dir_entries[count].d_name,
					sizeof(ret_entry[num_walked].name));
				num_walked++;
				break;
			default:
				break;
			}
		}
		next_node_pos = tpage.tree_walk_next;
	}

	*ret_num = num_walked;

	close(meta_fd);
	return 0;
}

/************************************************************************
*
* Function name: parse_meta
*        Inputs: char *meta_path
*       Summary: load fsmgr file and return all external volume
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*                ptr_ret_meta will be filled if return code is 0
*
*************************************************************************/
int32_t parse_meta(char *meta_path, RET_META *ret)
{
	int32_t meta_fd;
	struct stat_aarch64 stat_data;
	int32_t return_code;
	DIR_META_TYPE dir_meta;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1) {
		ret->result = -1;
		return -1;
	}

	return_code = read(meta_fd, &stat_data, sizeof(stat_data));
	if (return_code == -1) {
		close(meta_fd);
		ret->result = -1;
		return -1;
	}
	ret->result = 0;

	ret->stat.dev = stat_data.dev;
	ret->stat.ino = stat_data.ino;
	ret->stat.mode = stat_data.mode;
	ret->stat.nlink = stat_data.nlink;
	ret->stat.uid = stat_data.uid;
	ret->stat.gid = stat_data.gid;
	ret->stat.rdev = stat_data.rdev;
	ret->stat.size = stat_data.size;
	ret->stat.blksize = stat_data.blksize;
	ret->stat.blocks = stat_data.blocks;
	ret->stat.atime = stat_data.atime;
	ret->stat.atime_nsec = stat_data.atime_nsec;
	ret->stat.mtime = stat_data.mtime;
	ret->stat.mtime_nsec = stat_data.mtime_nsec;
	ret->stat.ctime = stat_data.ctime;
	ret->stat.ctime_nsec = stat_data.ctime_nsec;

	if (S_ISDIR(stat_data.mode))
		ret->file_type = D_ISDIR;
	else if (S_ISREG(stat_data.mode))
		ret->file_type = D_ISREG;
	else if (S_ISLNK(stat_data.mode))
		ret->file_type = D_ISLNK;
	else if (S_ISFIFO(stat_data.mode))
		ret->file_type = D_ISFIFO;
	else if (S_ISSOCK(stat_data.mode))
		ret->file_type = D_ISSOCK;

	if (ret->file_type == D_ISDIR) {
		return_code = read(meta_fd, &dir_meta, sizeof(DIR_META_TYPE));
		if (return_code == -1) {
			close(meta_fd);
			return -1;
		}
		ret->child_number = dir_meta.total_children;
	} else {
		ret->child_number = 0;
	}

	return 0;
}
