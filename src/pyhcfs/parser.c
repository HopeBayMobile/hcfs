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
* Function name: list_external_volume
*        Inputs: int32_t buf_num,
*                DIR_ENTRY *ret_entry,
*                int32_t *ret_num
*       Summary: load fsmgr file and return all external volume
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_external_volume(const char *fs_mgr_path,
			     PORTABLE_DIR_ENTRY **entry_array_ptr,
			     uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;
	int32_t meta_fd;
	ssize_t ret_size;
	int32_t ret_val, tmp_errno = 0;
	PORTABLE_DIR_ENTRY *ret_entry;

	ret_val = 0;
	meta_fd = open(fs_mgr_path, O_RDONLY);
	if (meta_fd == -1)
		return ERROR_SYSCALL;

	ret_size = pread(meta_fd, &tmp_head, sizeof(DIR_META_TYPE), 16);
	if (ret_size == -1)
		goto errcode_handle;

	/* Initialize B-tree walk by first loading the first node of the
	 * tree walk.
	 */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		ret_size = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		/*printf("ret_val %zu\n", ret_size);*/
		if (ret_size == -1)
			goto errcode_handle;
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
		ret_val = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (ret_val == -1)
			goto errcode_handle;
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
	goto end;

errcode_handle:
	tmp_errno = errno;
end:
	close(meta_fd);
	errno = tmp_errno;
	return (ret_val >= 0) ? 0 : ERROR_SYSCALL;
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
void parse_meta(const char *meta_path, RET_META *ret)
{
	int32_t meta_fd, tmp_errno = 0;
	HCFS_STAT_v1 meta_stat;
	DIR_META_TYPE dir_meta;

	ret->result = 0;
	ret->child_number = 0;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1) {
		ret->result = ERROR_SYSCALL;
		return;
	}

	ret->result = read(meta_fd, &meta_stat, sizeof(meta_stat));
	if (ret->result == -1)
		goto errcode_handle;

	ret->result =
	    memcmp(&meta_stat.magic, &META_MAGIC[0], sizeof(meta_stat.magic));
	if (ret->result != 0 || meta_stat.metaver > CURRENT_META_VER ||
	    meta_stat.metaver < BACKWARD_COMPATIBILITY) {
		ret->result = ERROR_UNSUPPORT_VER;
		goto errcode_handle;
	}

	memcpy(&ret->stat.magic, &meta_stat.magic, sizeof(ret->stat.magic));
	ret->stat.metaver = meta_stat.metaver;
	ret->stat.dev = meta_stat.dev;
	ret->stat.ino = meta_stat.ino;
	ret->stat.mode = meta_stat.mode;
	ret->stat.nlink = meta_stat.nlink;
	ret->stat.uid = meta_stat.uid;
	ret->stat.gid = meta_stat.gid;
	ret->stat.rdev = meta_stat.rdev;
	ret->stat.size = meta_stat.size;
	ret->stat.blksize = meta_stat.blksize;
	ret->stat.blocks = meta_stat.blocks;
	ret->stat.atime = meta_stat.atime;
	ret->stat.atime_nsec = meta_stat.atime_nsec;
	ret->stat.mtime = meta_stat.mtime;
	ret->stat.mtime_nsec = meta_stat.mtime_nsec;
	ret->stat.ctime = meta_stat.ctime;
	ret->stat.ctime_nsec = meta_stat.ctime_nsec;

	if (S_ISDIR(meta_stat.mode))
		ret->file_type = D_ISDIR;
	else if (S_ISREG(meta_stat.mode))
		ret->file_type = D_ISREG;
	else if (S_ISLNK(meta_stat.mode))
		ret->file_type = D_ISLNK;
	else if (S_ISFIFO(meta_stat.mode))
		ret->file_type = D_ISFIFO;
	else if (S_ISSOCK(meta_stat.mode))
		ret->file_type = D_ISSOCK;

	if (ret->file_type == D_ISDIR) {
		ret->result = read(meta_fd, &dir_meta, sizeof(DIR_META_TYPE));
		if (ret->result == -1)
			goto errcode_handle;
		ret->child_number = dir_meta.total_children;
	}

	ret->result = 0;
	goto end;

errcode_handle:
	tmp_errno = errno;
end:
	close(meta_fd);
	errno = tmp_errno;
	return;
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
static int64_t _traverse_dir_btree(const int32_t fd, const int64_t page_pos,
			    const int32_t start_el, const int32_t walk_left,
			    const int32_t walk_up, const int32_t limit,
			    TREE_WALKER *this_walk,
			    PORTABLE_DIR_ENTRY *file_list)
{
	int32_t idx;
	int32_t ret_val;
	ssize_t ret_ssize;
	DIR_ENTRY_PAGE temppage;

	ret_ssize = pread(fd, &temppage, sizeof(DIR_ENTRY_PAGE), page_pos);
	if (ret_ssize < 0)
		return errno ? -errno : -1;

	if (temppage.child_page_pos[start_el] == 0) {
		/* At leaf */
		for (idx = start_el; idx < temppage.num_entries; idx++) {
			if (this_walk->is_walk_end)
				return 0;

			memcpy(file_list + this_walk->num_walked,
			       &(temppage.dir_entries[idx]),
				       sizeof(PORTABLE_DIR_ENTRY));
			this_walk->end_page_pos = temppage.this_page_pos;
			this_walk->end_el_no = idx;
			this_walk->num_walked += 1;

			if (this_walk->num_walked >= limit) {
				this_walk->end_el_no += 1;
				this_walk->is_walk_end = TRUE;
				return 0;
			}
		}
	} else {
		if (walk_left) {
			ret_val = _traverse_dir_btree(
			    fd, temppage.child_page_pos[start_el], 0, TRUE,
			    FALSE, limit, this_walk, file_list);
			if (ret_val < 0)
				return ret_val;
		}

		for (idx = start_el; idx < temppage.num_entries; idx++) {
			if (this_walk->is_walk_end)
				return 0;

			memcpy(file_list + this_walk->num_walked,
			       &(temppage.dir_entries[idx]),
			       sizeof(PORTABLE_DIR_ENTRY));
			this_walk->end_page_pos =
				temppage.this_page_pos;
			this_walk->end_el_no = idx;
			this_walk->num_walked += 1;

			if (this_walk->num_walked >= limit) {
				this_walk->end_page_pos =
					temppage.child_page_pos[idx + 1];
				this_walk->end_el_no = 0;
				this_walk->is_walk_end = TRUE;
				return 0;
			}

			ret_val = _traverse_dir_btree(
			    fd, temppage.child_page_pos[idx + 1], 0, TRUE,
			    FALSE, limit, this_walk, file_list);
			if (ret_val < 0)
				return ret_val;
		}
	}

	/* Need to backward to parent page for deeper traverse */
	if (walk_up && temppage.parent_page_pos != 0 &&
			this_walk->is_walk_end == FALSE) {
		ret_ssize = pread(fd, &temppage, sizeof(DIR_ENTRY_PAGE),
				  temppage.parent_page_pos);
		if (ret_ssize < 0)
			return errno ? -errno : -1;

		for (idx = 0; idx < temppage.num_entries + 1; idx++) {
			if (page_pos == temppage.child_page_pos[idx]) {
				ret_val = _traverse_dir_btree(
				    fd, temppage.this_page_pos, idx, FALSE,
				    TRUE, limit, this_walk, file_list);
				if (ret_val < 0)
					return ret_val;
				break;
			}
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
*  Return value: Number of children returned if successful.
*  		 Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
			 const int32_t start_el, const int32_t limit,
			 int64_t *end_page_pos, int32_t *end_el_no,
			 PORTABLE_DIR_ENTRY *file_list)
{
	int32_t ret_val = 0, tmp_errno = 0;
	int32_t meta_fd;
	ssize_t ret_ssize;
	HCFS_STAT_v1 meta_stat;
	DIR_META_TYPE dirmeta;
	TREE_WALKER this_walk;

	if (start_el > MAX_DIR_ENTRIES_PER_PAGE || limit <= 0 ||
	    limit > LIST_DIR_LIMIT) {
		errno = EINVAL;
		return ERROR_SYSCALL; 
	}

	if (access(meta_path, R_OK) == -1)
		return ERROR_SYSCALL;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1)
		 return ERROR_SYSCALL;

	ret_ssize = pread(meta_fd, &meta_stat, sizeof(HCFS_STAT_v1), 0);
	if (ret_ssize < 0) {
		ret_val = ERROR_SYSCALL;
		goto errcode_handle;
	}
	ret_val = memcmp(&meta_stat.magic, META_MAGIC, sizeof(meta_stat.magic));
	if (ret_val != 0 || meta_stat.metaver > CURRENT_META_VER ||
	    meta_stat.metaver < BACKWARD_COMPATIBILITY) {
		ret_val = ERROR_UNSUPPORT_VER;
		goto errcode_handle;
	}
	if (!S_ISDIR(meta_stat.mode)) {
		ret_val = ERROR_SYSCALL;
		errno = ENOTDIR;
		goto errcode_handle;
	}

	/* Initialize stats about this tree walk */
	this_walk.is_walk_end = FALSE;
	this_walk.end_page_pos = page_pos;
	this_walk.end_el_no = start_el;
	this_walk.num_walked = 0;

	if (page_pos == 0) {
		/* Traverse from tree root */
		ret_ssize = pread(meta_fd, &dirmeta, sizeof(DIR_META_TYPE),
				  sizeof(HCFS_STAT_v1));
		if (ret_ssize < 0) {
			ret_val = ERROR_SYSCALL;
			goto errcode_handle;
		}

		ret_val = _traverse_dir_btree(meta_fd, dirmeta.root_entry_page,
					       0, TRUE, TRUE, limit, &this_walk,
					       file_list);
	} else {
		ret_val =
		    _traverse_dir_btree(meta_fd, page_pos, start_el, FALSE,
					TRUE, limit, &this_walk, file_list);
	}

	if (ret_val == 0) {
		*end_page_pos = this_walk.end_page_pos;
		*end_el_no = this_walk.end_el_no;
		if (this_walk.num_walked > 0 &&
		    this_walk.num_walked < limit)
			*end_el_no += 1;
		ret_val = this_walk.num_walked;
	}

	goto end;

errcode_handle:
	tmp_errno = errno;
end:
	close(meta_fd);
	errno = tmp_errno;
	return (ret_val >= 0) ? 0 : ret_val;
}
