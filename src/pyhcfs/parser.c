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
	int64_t end_pos;
	int32_t end_el;
	int32_t i;
	RET_META meta_data;
	HCFS_STAT *stat_data = &meta_data.stat;

	puts("============================================");
	puts("list_external_volume(\"testdata/fsmgr\", &ret_entry, &ret_num);");
	list_external_volume("testdata/fsmgr", &ret_entry, &ret_num);
	for (i = 0; i < ret_num; i++)
		printf("%lu %s\n", ret_entry[i].inode, ret_entry[i].d_name);

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

	puts("list_dir_inorder(\"testdata/meta423\"), 0, 0, 400, &end_pos, "
	     "&end_el, &(file_list[0])");
	PORTABLE_DIR_ENTRY file_list[400];

	list_dir_inorder("testdata/meta423", 0, 0, 400, &end_pos, &end_el,
			 &(file_list[0]));
	for (i = 0; i < 400; i++)
		printf("%s ", file_list[i].d_name);

	puts("\nlist_dir_inorder(\"testdata/meta423\", end_pos, end_el, 30, "
	     "&end_pos, &end_el, &(file_list2[0]))");
	PORTABLE_DIR_ENTRY file_list2[30];

	list_dir_inorder("testdata/meta423", end_pos, end_el, 30, &end_pos,
			 &end_el, &(file_list2[0]));
	for (i = 0; i < 30; i++)
		printf("%s ", file_list2[i].d_name);
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
	ssize_t ret_code;
	PORTABLE_DIR_ENTRY *ret_entry;

	if (meta_fd == -1)
		return -1;

	ret_code = pread(meta_fd, &tmp_head, sizeof(DIR_META_TYPE), 16);
	if (ret_code == -1) {
		close(meta_fd);
		return ret_code;
	}

	/* Initialize B-tree walk by first loading the first node of the
	 * tree walk.
	 */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		ret_code = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (ret_code == -1) {
			close(meta_fd);
			return ret_code;
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
		ret_code = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (ret_code == -1) {
			close(meta_fd);
			return ret_code;
		}
		for (count = 0; count < tpage.num_entries; count++) {
			switch (tpage.dir_entries[count].d_type) {
			case ANDROID_EXTERNAL:
			case ANDROID_MULTIEXTERNAL:
				ret_entry[num_walked].inode =
				    tpage.dir_entries[count].d_ino;
				strncpy(ret_entry[num_walked].d_name,
					tpage.dir_entries[count].d_name,
					sizeof(ret_entry[num_walked].d_name));
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
	int32_t ret_code;
	DIR_META_TYPE dir_meta;

	ret->result = 0;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1) {
		ret->result = -1;
		return -1;
	}

	ret_code = read(meta_fd, &stat_data, sizeof(stat_data));
	if (ret_code == -1)
		goto errcode_handle;

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
		ret_code = read(meta_fd, &dir_meta, sizeof(DIR_META_TYPE));
		if (ret_code == -1)
			goto errcode_handle;
		ret->child_number = dir_meta.total_children;
	} else {
		ret->child_number = 0;
	}

	ret_code = 0;
	ret->result = 0;
	goto end;

errcode_handle:
	ret_code = -errno;
	ret->result = -1;
end:
	close(meta_fd);
	return ret_code;
}

/************************************************************************
*
* Function name: _traverse_dir_btree
*        Inputs: const int32_t fd, const int64_t page_pos,
*                const int32_t start_el, const int32_t walk_left,
*		 const int32_t walk_up, const int32_t limit,
*		 TREE_WALKER *this_walk, PORTABLE_DIR_ENTRY *file_list
*       Summary: Helper function to traverse dir_entry btree recursively.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int64_t _traverse_dir_btree(const int32_t fd, const int64_t page_pos,
			    const int32_t start_el, const int32_t walk_left,
			    const int32_t walk_up, const int32_t limit,
			    TREE_WALKER *this_walk,
			    PORTABLE_DIR_ENTRY *file_list)
{
	uint32_t idx;
	int32_t ret_code;
	ssize_t ret_ssize;
	int32_t parent_start_el = -1;
	DIR_ENTRY_PAGE temppage;

	ret_ssize = pread(fd, &temppage, sizeof(DIR_ENTRY_PAGE), page_pos);
	if (ret_ssize < 0)
		return -errno;

	if (temppage.child_page_pos[start_el] == 0) {
		/* At leaf */
		for (idx = start_el; idx < temppage.num_entries; idx++) {
			if (this_walk->num_walked >= limit) {
				if (this_walk->is_walk_end == FALSE) {
					this_walk->is_walk_end = TRUE;
					this_walk->end_page_pos =
					    temppage.this_page_pos;
					this_walk->end_el_no = idx;
				}
				return 0;
			}
			memcpy(file_list + this_walk->num_walked,
			       &(temppage.dir_entries[idx]),
			       sizeof(PORTABLE_DIR_ENTRY));
			this_walk->num_walked += 1;
		}
	} else {
		if (walk_left) {
			ret_code = _traverse_dir_btree(
			    fd, temppage.child_page_pos[start_el], 0, TRUE,
			    FALSE, limit, this_walk, file_list);
			if (ret_code < 0)
				return ret_code;
		}

		for (idx = start_el; idx < temppage.num_entries; idx++) {
			if (this_walk->num_walked >= limit) {
				if (this_walk->is_walk_end == FALSE) {
					this_walk->is_walk_end = TRUE;
					this_walk->end_page_pos =
					    temppage.child_page_pos[idx + 1];
					this_walk->end_el_no = 0;
				}
				return 0;
			}
			memcpy(file_list + this_walk->num_walked,
			       &(temppage.dir_entries[idx]),
			       sizeof(PORTABLE_DIR_ENTRY));
			this_walk->num_walked += 1;
			ret_code = _traverse_dir_btree(
			    fd, temppage.child_page_pos[idx + 1], 0, TRUE,
			    FALSE, limit, this_walk, file_list);
			if (ret_code < 0)
				return ret_code;
		}
	}

	/* Need to backward to parent page for deeper traverse */
	if (walk_up && temppage.parent_page_pos != 0) {
		ret_ssize = pread(fd, &temppage, sizeof(DIR_ENTRY_PAGE),
				  temppage.parent_page_pos);
		if (ret_ssize < 0)
			return -errno;

		for (idx = 0; idx < temppage.num_entries + 1; idx++) {
			if (page_pos == temppage.child_page_pos[idx]) {
				parent_start_el = idx;
				break;
			}
		}
		if (parent_start_el >= 0) {
			ret_code = _traverse_dir_btree(
			    fd, temppage.this_page_pos, parent_start_el, FALSE,
			    TRUE, limit, this_walk, file_list);
			if (ret_code < 0)
				return ret_code;
		}
	}

	return 0;
}

/************************************************************************
*
* Function name: list_dir_inorder
*        Inputs: const char *meta_path, const int64_t page_pos,
*		 const int32_t start_el, const int32_t limit,
*		 int64_t *end_page_pos, int32_t *end_el_no,
*		 PORTABLE_DIR_ENTRY* file_list
*       Summary: Traverse the dir_entry tree in a given dir meta file and
*                return a list contains files and dirs in this dir in order.
*                The tree traverse will start from the (start_el) in (page_pos)
*                and the result list will stored in array (file_list).
*                Arguments (end_page_pos) and (end_el_no) indicate the start
*                position for next traverse.
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
			 const int32_t start_el, const int32_t limit,
			 int64_t *end_page_pos, int32_t *end_el_no,
			 PORTABLE_DIR_ENTRY *file_list)
{
	int32_t ret_code;
	int32_t meta_fd;
	ssize_t ret_ssize;
	struct stat_aarch64 meta_stat;
	DIR_META_TYPE dirmeta;
	TREE_WALKER this_walk;

	if (start_el > MAX_DIR_ENTRIES_PER_PAGE)
		return -EINVAL;

	if (limit <= 0 || limit > LIST_DIR_LIMIT)
		return -EINVAL;

	if (access(meta_path, R_OK) == -1)
		return -errno;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1)
		return -errno;

	ret_ssize = pread(meta_fd, &meta_stat, sizeof(struct stat_aarch64), 0);
	if (ret_ssize < 0)
		goto errcode_handle;
	if (!S_ISDIR(meta_stat.mode))
		return -ENOTDIR;

	/* Initialize stats about this tree walk */
	this_walk.is_walk_end = FALSE;
	this_walk.end_page_pos = 0;
	this_walk.end_el_no = 0;
	this_walk.num_walked = 0;

	if (page_pos == 0) {
		/* Traverse from tree root */
		ret_ssize = pread(meta_fd, &dirmeta, sizeof(DIR_META_TYPE),
				  sizeof(struct stat_aarch64));
		if (ret_ssize < 0)
			goto errcode_handle;

		ret_code = _traverse_dir_btree(meta_fd, dirmeta.root_entry_page,
					       0, TRUE, TRUE, limit, &this_walk,
					       file_list);
	} else {
		ret_code =
		    _traverse_dir_btree(meta_fd, page_pos, start_el, FALSE,
					TRUE, limit, &this_walk, file_list);
	}

	if (ret_code == 0) {
		*end_page_pos = this_walk.end_page_pos;
		*end_el_no = this_walk.end_el_no;
	}

	goto end;

errcode_handle:
	ret_code = -errno;
end:
	close(meta_fd);
	return ret_code;
}
