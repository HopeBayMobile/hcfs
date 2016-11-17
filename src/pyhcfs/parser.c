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
#include "mount_manager.h"

/************************************************************************
*
* Function name: list_volume
*        Inputs: int32_t buf_num,
*                DIR_ENTRY *ret_entry,
*                int32_t *ret_num
*       Summary: load fsmgr file and return all volume
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_volume(const char *fs_mgr_path,
			     PORTABLE_DIR_ENTRY **entry_array_ptr,
			     uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;
	int32_t meta_fd;
	ssize_t ret_val;
	int32_t tmp_errno = 0;
	struct stat st;
	off_t remain_size;
	PORTABLE_DIR_ENTRY *ret_entry;

	ret_val = 0;
	meta_fd = open(fs_mgr_path, O_RDONLY);
	if (meta_fd == -1)
		return ERROR_SYSCALL;

	ret_val = pread(meta_fd, &tmp_head, sizeof(DIR_META_TYPE), 16);
	if (ret_val == -1 || ret_val < sizeof(DIR_META_TYPE)) {
		errno = (ret_val == -1)?errno:EINVAL;
		goto errcode_handle;
	}

	/* check remain size of fsmgr file is mutiple of
	 *  sizeof(DIR_ENTRY_PAGE)
	 */
	ret_val = fstat(meta_fd, &st);
	if (ret_val == -1)
		goto errcode_handle;
	remain_size = st.st_size - sizeof(DIR_META_TYPE) - 16;
	if (remain_size % sizeof(DIR_ENTRY_PAGE) != 0) {
		errno = EINVAL;
		goto errcode_handle;
	}

	/* Initialize B-tree walk by first loading the first node of the
	 * tree walk.
	 */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		ret_val = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		/*printf("ret_val %zu\n", ret_val);*/
		if (ret_val == -1 || ret_val < sizeof(DIR_ENTRY_PAGE)) {
			errno = (ret_val == -1) ? errno : EINVAL;
			goto errcode_handle;
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
		ret_val = pread(meta_fd, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (ret_val == -1 || ret_val < sizeof(DIR_ENTRY_PAGE)) {
			errno = (ret_val == -1)?errno:EINVAL;
			goto errcode_handle;
		}
		for (count = 0; count < tpage.num_entries; count++) {

			switch (tpage.dir_entries[count].d_type) {
			case ANDROID_INTERNAL:
			case ANDROID_EXTERNAL:
			case ANDROID_MULTIEXTERNAL:
				ret_entry[num_walked].inode =
					tpage.dir_entries[count].d_ino;
				ret_entry[num_walked].d_type =
					tpage.dir_entries[count].d_type;
				strncpy(ret_entry[num_walked].d_name,
					tpage.dir_entries[count].d_name,
					sizeof(ret_entry[num_walked].d_name));
				num_walked++;
				break;
			default:
				/* undefined d_ype */
				errno = EINVAL;
				goto errcode_handle;
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
	return (errno == 0) ? 0 : ERROR_SYSCALL;
}

int32_t check_meta_ver(HCFS_STAT_v1 const *meta_stat)
{
	int32_t ret_code;
	ret_code =
	    memcmp(&meta_stat->magic, &META_MAGIC[0], sizeof(meta_stat->magic));
	if (ret_code != 0 || meta_stat->metaver > CURRENT_META_VER ||
	    meta_stat->metaver < BACKWARD_COMPATIBILITY) {
		return ERROR_UNSUPPORT_VER;
	}
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

	ret->result = check_meta_ver(&meta_stat);
	if (ret->result != 0)
		goto errcode_handle;

	/* TODO: support multiple version when VERSION_NUM > 1 */
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
		ret->child_number = dir_meta.total_children + 2; /* Inclued . and .. */
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
			if (ret_val < 0) {
				return ret_val;
			}
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
			 int32_t *num_children, PORTABLE_DIR_ENTRY *file_list)
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
		*num_children = this_walk.num_walked;
	}

	goto end;

errcode_handle:
	tmp_errno = errno;
end:
	close(meta_fd);
	errno = tmp_errno;
	return (ret_val >= 0) ? 0 : ret_val;
}

/************************************************************************
*
* Function name: get_vol_usage
*        Inputs: const char *meta_path, int64_t* vol_usage,
*       Summary:
*  Return value: 0 if successful.
*  		 Otherwise returns negation of error code.
*
*************************************************************************/
int32_t get_vol_usage(const char *meta_path, int64_t *vol_usage)
{
	int32_t meta_fd, tmp_errno, ret_val, stat_ret_val;
	int64_t vol_meta;
	int64_t num_backend_inodes;
	int64_t max_inode;
	int64_t vol_pinned;
	ssize_t ret_ssize;
	struct stat buf;
	FS_CLOUD_STAT_T meta_stat;

	tmp_errno = 0;
	ret_val = 0;
	stat_ret_val = 0;

	meta_fd = open(meta_path, O_RDONLY);
	if (meta_fd == -1)
		return ERROR_SYSCALL;

	stat_ret_val = fstat(meta_fd, &buf);
	if (stat_ret_val < 0) {
		ret_val = ERROR_SYSCALL;
	       	goto errcode_handle;
	}
	if (buf.st_size == sizeof(FS_CLOUD_STAT_T)) {
                //FSStat current version
                ret_ssize = pread(meta_fd, &meta_stat, sizeof(FS_CLOUD_STAT_T), 0);
	}else if(buf.st_size == sizeof(FS_CLOUD_STAT_T_V1)){
                //FSStat old version
                ret_ssize = pread(meta_fd, &meta_stat, sizeof(FS_CLOUD_STAT_T_V1), 0);
	}else{
                ret_val = ERROR_SYSCALL;
                errno = EINVAL;
                goto errcode_handle;
	}
	
        if (ret_ssize < 0) {
                ret_val = ERROR_SYSCALL;
                goto errcode_handle;
        }

	*vol_usage = meta_stat.backend_system_size;
	vol_meta = meta_stat.backend_meta_size;
	num_backend_inodes = meta_stat.backend_num_inodes;
	max_inode = meta_stat.max_inode;
	vol_pinned = meta_stat.pinned_size;

	if (*vol_usage < 0 || *vol_usage < vol_meta || *vol_usage < vol_pinned
			   || max_inode < num_backend_inodes) {
		ret_val = ERROR_SYSCALL;
		errno = EINVAL;
		goto errcode_handle;
	} else {
		goto end;
	}

errcode_handle:
	tmp_errno = errno;
end:
	close(meta_fd);
	errno = tmp_errno;
	return (ret_val >= 0) ? 0 : ret_val;
}

/************************************************************************
*
* Function name: list_file_blocks
*        Inputs: const char *meta_path,
*        	 PORTABLE_BLOCK_NAME **block_list_ptr,
*        	 int64_t *ret_num, int64_t *inode_num
*       Summary:
*  Return value: 0 if successful.
*  		 Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_file_blocks(const char *meta_path,
			 PORTABLE_BLOCK_NAME **block_list_ptr,
			 int64_t *ret_num, int64_t *inode_num)
{
#ifdef MAX_BLOCK_SIZE
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE 1048576
#endif
	FILE *metafptr;
	int ret;
	int32_t tmp_errno, ret_val, errcode;
	size_t ret_size;
	//ssize_t ret_ssize;
	HCFS_STAT_v1 meta_stat;
	FILE_META_TYPE_v1 file_meta;
	int64_t allocate_blkname_num = 2;
	int64_t current_page;
	int64_t count;
	int64_t total_blocks;
        int64_t e_index, which_page;
	int64_t page_pos;
        BLOCK_ENTRY_PAGE tmppage;
	int64_t ret_idx;
	uint8_t status;

	*block_list_ptr = (PORTABLE_BLOCK_NAME *)malloc(
	    allocate_blkname_num * sizeof(PORTABLE_BLOCK_NAME));

	tmp_errno = 0;
	ret_val = 0;

	if (access(meta_path, R_OK) == -1)
		return ERROR_SYSCALL;

	metafptr = fopen(meta_path, "rb");
	if (metafptr == NULL)
		 return ERROR_SYSCALL;

	FREAD(&meta_stat, sizeof(HCFS_STAT_v1), 1, metafptr);

	ret_val = check_meta_ver(&meta_stat);
	if (ret_val != 0)
		goto errcode_handle;

	if (!S_ISREG(meta_stat.mode)) {
		ret_val = ERROR_SYSCALL;
		errno = EINVAL;
		goto errcode_handle;
	}

	/* Load file meta */
	FREAD(&file_meta, sizeof(FILE_META_TYPE_v1), 1, metafptr);

	/* loop over all meta blocks */
	current_page = -1;
	ret_idx = 0;
	total_blocks = ((meta_stat.size - 1) / MAX_BLOCK_SIZE) + 1;
	for (count = 0; count < total_blocks; count++) {
		e_index = count % MAX_BLOCK_ENTRIES_PER_PAGE;
		which_page = count / MAX_BLOCK_ENTRIES_PER_PAGE;

		if (current_page != which_page) {
			page_pos =
			    seek_page2(&file_meta, metafptr, which_page, 0);
			if (page_pos <= 0) {
				count += (MAX_BLOCK_ENTRIES_PER_PAGE - 1);
				continue;
			}
			current_page = which_page;
			FSEEK(metafptr, page_pos, SEEK_SET);
			memset(&tmppage, 0, sizeof(BLOCK_ENTRY_PAGE));
			FREAD(&tmppage, sizeof(BLOCK_ENTRY_PAGE), 1, metafptr);
		}
		status = tmppage.block_entries[e_index].status;
		if ((status == ST_NONE) || (status == ST_TODELETE))
			continue;

		/* load block entry */
		if(ret_idx >= allocate_blkname_num){
			allocate_blkname_num *=2;
			*block_list_ptr = (PORTABLE_BLOCK_NAME *)realloc(
			    *block_list_ptr,
			    allocate_blkname_num * sizeof(PORTABLE_BLOCK_NAME));
		}
		(*block_list_ptr)[ret_idx].block_seq =
		    tmppage.block_entries[e_index].seqnum;
		(*block_list_ptr)[ret_idx].block_num = count;
		ret_idx++;
	}
	*inode_num = meta_stat.ino;
	*ret_num = ret_idx;

	goto end;

errcode_handle:
	tmp_errno = errno;
end:
	fclose(metafptr);
	errno = tmp_errno;
	return (ret_val >= 0) ? 0 : ret_val;
}


/* START of external reference */

//LCOV_EXCL_START
int64_t seek_page2(FILE_META_TYPE *temp_meta,
		   FILE *fptr,
		   int64_t target_page,
		   int64_t hint_page)
{
	off_t filepos;
	int32_t which_indirect;

	/* TODO: hint_page is not used now. Consider how to enhance. */
	UNUSED(hint_page);
	/* First check if meta cache is locked */
	/* Do not actually create page here */
	/*TODO: put error handling for the read/write ops here*/

	if (target_page < 0)
		return -EPERM;

	which_indirect = check_page_level(target_page);

	switch (which_indirect) {
	case 0:
		filepos = temp_meta->direct;
		break;
	case 1:
		if (temp_meta->single_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 1);
		break;
	case 2:
		if (temp_meta->double_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 2);
		break;
	case 3:
		if (temp_meta->triple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 3);
		break;
	case 4:
		if (temp_meta->quadruple_indirect == 0)
			filepos = 0;
		else
			filepos = _load_indirect(target_page, temp_meta,
						fptr, 4);
		break;
	default:
		filepos = -1;
		break;
	}

	return filepos;
}

int64_t _load_indirect(int64_t target_page, FILE_META_TYPE *temp_meta,
			FILE *fptr, int32_t level)
{
	int64_t tmp_page_index;
	int64_t tmp_pos, tmp_target_pos;
	int64_t tmp_ptr_page_index, tmp_ptr_index;
	PTR_ENTRY_PAGE tmp_ptr_page;
	int32_t count, ret, errcode;
	size_t ret_size;
	int64_t ret_pos;

	tmp_page_index = target_page - 1;

	for (count = 1; count < level; count++)
		tmp_page_index -= (longpow(POINTERS_PER_PAGE, count));

	switch (level) {
	case 1:
		tmp_target_pos = temp_meta->single_indirect;
		break;
	case 2:
		tmp_target_pos = temp_meta->double_indirect;
		break;
	case 3:
		tmp_target_pos = temp_meta->triple_indirect;
		break;
	case 4:
		tmp_target_pos = temp_meta->quadruple_indirect;
		break;
	default:
		return 0;
	}

	tmp_ptr_index = tmp_page_index;

	for (count = level - 1; count >= 0; count--) {
		FSEEK(fptr, tmp_target_pos, SEEK_SET);
		FTELL(fptr);
		tmp_pos = ret_pos;
		if (tmp_pos != tmp_target_pos)
			return 0;
		FREAD(&tmp_ptr_page, sizeof(PTR_ENTRY_PAGE), 1, fptr);

		if (count == 0)
			break;

		tmp_ptr_page_index = tmp_ptr_index /
				(longpow(POINTERS_PER_PAGE, count));
		tmp_ptr_index = tmp_ptr_index %
				(longpow(POINTERS_PER_PAGE, count));
		if (tmp_ptr_page.ptr[tmp_ptr_page_index] == 0)
			return 0;

		tmp_target_pos = tmp_ptr_page.ptr[tmp_ptr_page_index];
	}


	return tmp_ptr_page.ptr[tmp_ptr_index];

errcode_handle:
	return errcode;
}
int32_t check_page_level(int64_t page_index)
{
	int64_t tmp_index;

	if (page_index == 0)
		return 0;   /*direct page (id 0)*/

	tmp_index = page_index - 1;

	if (tmp_index < POINTERS_PER_PAGE) /* if single-indirect */
		return 1;

	tmp_index = tmp_index - POINTERS_PER_PAGE;

	/* double-indirect */
	if (tmp_index < (longpow(POINTERS_PER_PAGE, 2)))
		return 2;

	tmp_index = tmp_index - (longpow(POINTERS_PER_PAGE, 2));

	/* triple-indirect */
	if (tmp_index < (longpow(POINTERS_PER_PAGE, 3)))
		return 3;

	tmp_index = tmp_index - (longpow(POINTERS_PER_PAGE, 3));

	/* TODO: boundary handling for quadruple indirect */
	return 4;
}

int64_t longpow(int64_t base, int32_t power)
{
	if (power < 0) return 0; /* FIXME: error handling */
	int64_t pow1024[] = {
		1, 1024, 1048576, 1073741824,
		1099511627776, 1125899906842624, 1152921504606846976};
	if (base == 1024 && power <= 6) { /* fast path */
		return pow1024[power];
		/* FIXME: if power >= 7, integer overflow */
	}

	uint64_t r = 1;
	/* slow path with slight optimization */
	while (power != 0) {
		if (power % 2 == 1) { /* q is odd */
			r *= base;
			power--;
		}
		base *= base;
		power /= 2;
	}
	return r;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	UNUSED(level);
	return 0;

}

//LCOV_EXCL_STOP
/* END of external reference */
