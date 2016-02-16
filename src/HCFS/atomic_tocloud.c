#define _GNU_SOURCE
#include "atomic_tocloud.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hcfs_tocloud.h"
#include "macro.h"
#include "global.h"
#include "metaops.h"
#include "utils.h"
#include "hcfs_fromcloud.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE
extern SYSTEM_CONF_STRUCT *system_config;

/*
 * Tag inode as uploading or not_uploading in fuse process memory.
 *
 * Main function of communicating with fuse process. This aims to
 * tag or untag the inode is_uploading flag.
 *
 * @return 0 if succeeding in tagging status, otherwise -1 on error.
 */
int comm2fuseproc(ino_t this_inode, BOOL is_uploading,
		int fd, BOOL is_revert, BOOL finish_sync)
{
	int sockfd;
	int ret, resp, errcode;
	struct sockaddr_un addr;
	UPLOADING_COMMUNICATION_DATA data;

	/* Prepare data */
	data.inode = this_inode;
	data.is_uploading = is_uploading;
	data.is_revert = is_revert;
	data.progress_list_fd = fd;
	data.finish_sync = finish_sync;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, FUSE_SOCK_PATH);

	ret = connect(sockfd, (struct sockaddr *)&addr,
		sizeof(struct sockaddr_un));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Error: Fail to connect socket in %s. Code %d\n",
			__func__, errcode);
		close(sockfd);
		return -errcode;
	}

	resp = 0;
	send(sockfd, &data, sizeof(UPLOADING_COMMUNICATION_DATA), 0);
	recv(sockfd, &resp, sizeof(int), 0);

	if (resp < 0) {
		write_log(2, "Communication fail: Response code %d in %s",
			resp, __func__);
		ret = resp;
	} else {
		write_log(10, "Debug: Inode %"PRIu64
			" succeeded in communicating to fuse proc\n",
			(uint64_t)this_inode);
		ret = 0;
	}

	close(sockfd);
	return ret;
}

static inline long long longpow(long long base, int power)
{
	long long tmp;
	int count;

	tmp = 1;

	for (count = 0; count < power; count++)
		tmp = tmp * base;

	return tmp;
}


long long query_status_page(int fd, long long block_index)
{
	long long target_page;
	int which_indirect;
	long long ret_pos;
	int level, i;
	int errcode;
	ssize_t ret_ssize;
	long long ptr_index, ptr_page_index;
	PROGRESS_META progress_meta;
	PTR_ENTRY_PAGE temp_ptr_page;

	target_page = block_index / MAX_BLOCK_ENTRIES_PER_PAGE;
	which_indirect = check_page_level(target_page);
	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	switch(which_indirect) {
	case 0:
		return progress_meta.direct;
	case 1:
		ret_pos = progress_meta.single_indirect;
		break;
	case 2:
		ret_pos = progress_meta.double_indirect;
		break;
	case 3:
		ret_pos = progress_meta.triple_indirect;
		break;
	case 4:
		ret_pos = progress_meta.quadruple_indirect;
	}
	if (ret_pos == 0)
		return ret_pos;

	ptr_index = target_page - 1;
	for (i = 1; i < which_indirect; i++)
		ptr_index -= longpow(POINTERS_PER_PAGE, i);

	for(level = which_indirect - 1; level >= 0; level--) {
		PREAD(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE), ret_pos);
		if (level == 0)
			break;
		ptr_page_index = ptr_index / longpow(POINTERS_PER_PAGE, level);
		ptr_index = ptr_index % longpow(POINTERS_PER_PAGE, level);
		if (temp_ptr_page.ptr[ptr_page_index] == 0)
			return 0;

		ret_pos = temp_ptr_page.ptr[ptr_page_index];
	}

	return temp_ptr_page.ptr[ptr_index];

errcode_handle:
	return 0;

}

long long create_status_page(int fd, long long block_index)
{
	int which_indirect;
	long long target_page;
	PROGRESS_META progress_meta;
	BLOCK_UPLOADING_PAGE temp_page;
	PTR_ENTRY_PAGE temp_ptr_page, zero_ptr_page;
	long long tmp_pos;
	long long ptr_page_index, ptr_index;
	int errcode;
	ssize_t ret_ssize;
	int64_t ret_pos;
	int level, i;

	target_page = block_index / MAX_BLOCK_ENTRIES_PER_PAGE;

	which_indirect = check_page_level(target_page);
	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	switch(which_indirect) {
	case 0:
		if (progress_meta.direct == 0) {
			LSEEK(fd, 0, SEEK_END);
			memset(&temp_page, 0, sizeof(BLOCK_UPLOADING_PAGE));
			PWRITE(fd, &temp_page, sizeof(BLOCK_UPLOADING_PAGE),
				ret_pos);
			progress_meta.direct = ret_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		return progress_meta.direct;
	case 1:
		if (progress_meta.single_indirect == 0) {
			LSEEK(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				ret_pos);
			progress_meta.single_indirect = ret_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		tmp_pos = progress_meta.single_indirect;
		break;
	case 2:
		if (progress_meta.double_indirect == 0) {
			LSEEK(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				ret_pos);
			progress_meta.double_indirect = ret_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		tmp_pos = progress_meta.double_indirect;
		break;
	case 3:
		if (progress_meta.triple_indirect == 0) {
			LSEEK(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				ret_pos);
			progress_meta.triple_indirect = ret_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		tmp_pos = progress_meta.triple_indirect;
		break;
	case 4:
		if (progress_meta.quadruple_indirect == 0) {
			LSEEK(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				ret_pos);
			progress_meta.quadruple_indirect = ret_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		tmp_pos = progress_meta.quadruple_indirect;
		break;
	default:
		return 0;
	}

	ptr_index = target_page - 1;
	for (i = 1; i < which_indirect; i++)
		ptr_index -= longpow(POINTERS_PER_PAGE, i);

	/* Create ptr page */
	memset(&zero_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
	for (level = which_indirect - 1; level >= 0 ; level--) {
		PREAD(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE), tmp_pos);
		if (level == 0)
			break;
		ptr_page_index = ptr_index / longpow(POINTERS_PER_PAGE, level);
		ptr_index = ptr_index % longpow(POINTERS_PER_PAGE, level);

		if (temp_ptr_page.ptr[ptr_page_index] == 0) {
			LSEEK(fd, 0, SEEK_END);
			PWRITE(fd, &zero_ptr_page, sizeof(PTR_ENTRY_PAGE),
				ret_pos);
			temp_ptr_page.ptr[ptr_page_index] = ret_pos;
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				tmp_pos);
		}
		tmp_pos = temp_ptr_page.ptr[ptr_page_index];
	}

	/* Create status page */
	if (temp_ptr_page.ptr[ptr_index] == 0) {
		LSEEK(fd, 0, SEEK_END);
		memset(&temp_page, 0, sizeof(BLOCK_UPLOADING_PAGE));
		PWRITE(fd, &temp_page, sizeof(BLOCK_UPLOADING_PAGE),
				ret_pos);
		temp_ptr_page.ptr[ptr_index] = ret_pos;
		PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE), tmp_pos);
	}

	return temp_ptr_page.ptr[ptr_index];

errcode_handle:
	write_log(0, "Fail to create page in %s. Code %d\n", __func__, -errcode);
	return errcode;
}

/**
 * get_progress_info()
 *
 * Get uploading information for a specified block.
 *
 * @param fd File descriptor of a uploading progress file
 * @param block_index The block index that needs to be queried
 * @param block_uploading_status A pointer points to memory space 
 *        that will be stored with uploading info of the block.
 *
 * @return 0 on success, -ENOENT when the uploading info of the
 *         block not found. Otherwise return other negative error
 *         code
 */ 
int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	long long offset;
	int errcode;
	long long ret_ssize;
	int entry_index;
	BLOCK_UPLOADING_PAGE block_page;

	ret_ssize = 0;
	entry_index = block_index % MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fd, LOCK_EX);
	offset = query_status_page(fd, block_index);
	if (offset > 0)
		PREAD(fd, &block_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
	flock(fd, LOCK_UN);

	if (offset <= 0) {
		/* It may occur when query a truncated block. */
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
		return -ENOENT;
	} else {
		memcpy(block_uploading_status,
			&(block_page.status_entry[entry_index]),
			sizeof(BLOCK_UPLOADING_STATUS));
	}

	return 0;

errcode_handle:
	write_log(0, "Error: Fail to get progress-info of block_%lld\n",
			block_index);
	return errcode;

}

#if (DEDUP_ENABLE)
int set_progress_info(int fd, long long block_index,
	const char *toupload_exist, const char *backend_exist,
	const unsigned char *toupload_objid, const unsigned char *backend_objid,
	const char *finish)
{
	int errcode;
	long long offset;
	ssize_t ret_ssize;
	int entry_index;
	BLOCK_UPLOADING_STATUS *block_uploading_status;
	BLOCK_UPLOADING_PAGE status_page;

	entry_index = block_index % MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fd, LOCK_EX);
	offset = create_status_page(fd, block_index);
	if (offset > 0) {
		PREAD(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
		block_uploading_status =
			&(status_page.status_entry[entry_index]);
	} else {
		write_log(0, "Error: Fail to set progress. offset %lld\n", offset);
		flock(fd, LOCK_UN);
		return offset;
	}

	if (toupload_exist)
		block_uploading_status->block_exist = (((*toupload_exist) & 1) |
			(block_uploading_status->block_exist & 2));
	if (backend_exist)
		block_uploading_status->block_exist = (((*backend_exist) << 1) |
			(block_uploading_status->block_exist & 1));
	if (toupload_objid)
		memcpy(block_uploading_status->to_upload_objid, toupload_objid,
			sizeof(unsigned char) * OBJID_LENGTH);
	if (backend_objid)
		memcpy(block_uploading_status->backend_objid, backend_objid,
			sizeof(unsigned char) * OBJID_LENGTH);
	if (finish)
		block_uploading_status->finish_uploading = *finish;

	PWRITE(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
	flock(fd, LOCK_UN);

	if (finish)
		if (block_uploading_status->finish_uploading == TRUE)
			write_log(10, "Debug: block_%lld finished uploading - "
				"fd = %d\n", block_index, fd);

	return 0;

errcode_handle:
	flock(fd, LOCK_UN);
	return errcode;
}

#else
int set_progress_info(int fd, long long block_index,
	const char *toupload_exist, const char *backend_exist,
	const long long *toupload_seq, const long long *backend_seq,
	const char *finish)
{
	int errcode;
	long long offset;
	ssize_t ret_ssize;
	int entry_index;
	BLOCK_UPLOADING_STATUS *block_uploading_status;
	BLOCK_UPLOADING_PAGE status_page;

	entry_index = block_index % MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fd, LOCK_EX);
	offset = create_status_page(fd, block_index);
	if (offset > 0) {
		PREAD(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
		block_uploading_status =
			&(status_page.status_entry[entry_index]);
	} else {
		write_log(0, "Error: Fail to set progress. offset %lld\n", offset);
		flock(fd, LOCK_UN);
		return offset;
	}

	if (toupload_exist)
		block_uploading_status->block_exist = (((*toupload_exist) & 1) |
			(block_uploading_status->block_exist & 2));
	if (backend_exist)
		block_uploading_status->block_exist = (((*backend_exist) << 1) |
			(block_uploading_status->block_exist & 1));
	if (toupload_seq)
		block_uploading_status->to_upload_seq = *toupload_seq;
	if (backend_seq)
		block_uploading_status->backend_seq = *backend_seq;
	if (finish)
		block_uploading_status->finish_uploading = *finish;

	PWRITE(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
	flock(fd, LOCK_UN);

	if (finish)
		if (block_uploading_status->finish_uploading == TRUE)
			write_log(10, "Debug: block_%lld finished uploading - "
				"fd = %d\n", block_index, fd);

	return 0;

errcode_handle:
	flock(fd, LOCK_UN);
	return errcode;
}
#endif

int create_progress_file(ino_t inode)
{
	int ret_fd;
	int errcode, ret;
	char filename[200];
	char pathname[200];
	PROGRESS_META progress_meta;
	ssize_t ret_ssize;
	FILE *fptr;

	sprintf(pathname, "%s/upload_bullpen", METAPATH);

	if (access(pathname, F_OK) == -1)
		mkdir(pathname, 0700);

	sprintf(filename, "%s/upload_progress_inode_%"PRIu64,
		pathname, (uint64_t)inode);

	if (access(filename, F_OK) == 0) {
		write_log(0, "Error: Open \"%s\" but it exist. Unlink it\n",
			filename);
		UNLINK(filename);
	}

	ret_fd = open(filename, O_CREAT | O_RDWR);
	if (ret_fd < 0) {
		errcode = errno;
		write_log(0, "Error: Fail to open uploading progress file"
			" in %s. Code %d\n", __func__, errcode);
		return -errcode;
	} else {
		write_log(10, "Debug: Open progress-info file for inode %"PRIu64
			", fd = %d\n", (uint64_t)inode, ret_fd);
	}

	memset(&progress_meta, 0, sizeof(PROGRESS_META));
	progress_meta.now_action = PREPARING;
	PWRITE(ret_fd, &progress_meta, sizeof(PROGRESS_META), 0);

	return ret_fd;

errcode_handle:
	return errcode;
}

/**
 * Init uploading progress file
 *
 * @fd File descriptor of progress file
 * @backend_blocks Number of blocks of backend Regfile
 * @backend_size Size of RegFile of backend data
 * @backend_metafptr File pointer of downloaded backend RegFile
 *
 * This function initializes object-id and seq number of given backend data.
 * Other info is set as none, that is all zeros. Backend info of given
 * data block will not be set if status is ST_NONE or ST_TODELETE. After init
 * all backend data blocks, The field now_action in progress meta will be
 * set as NOW_UPLOADING.
 *
 * @return 0 if succeed. Otherwise negative error code.
 *
 */
int init_progress_info(int fd, long long backend_blocks,
		long long backend_size, FILE *backend_metafptr)
{
	int errcode;
	long long offset, ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;
	struct stat tempstat;
	long long e_index, which_page, current_page, page_pos;
	long long block;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE tempfilemeta;
	char cloud_status;
	PROGRESS_META progress_meta;
	BLOCK_UPLOADING_PAGE status_page;
	int entry_index;

	flock(fd, LOCK_EX);

	if (backend_metafptr == NULL) { /* backend meta does not exist */
		PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		progress_meta.now_action = NOW_UPLOADING;
		PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		flock(fd, LOCK_UN);

		return 0;
	}

	PREAD(fileno(backend_metafptr), &tempfilemeta, sizeof(FILE_META_TYPE),
							sizeof(struct stat));

	write_log(10, "Debug: backend blocks = %lld\n", backend_blocks);

	/* Write into progress info */
	current_page = -1;
	for (block = 0; block < backend_blocks; block++) {
		e_index = block % BLK_INCREMENTS;
		which_page = block / BLK_INCREMENTS;

		if (current_page != which_page) {
			page_pos = seek_page2(&tempfilemeta,
				backend_metafptr, which_page, 0);
			if (page_pos <= 0) {
				block += (BLK_INCREMENTS - 1);
				continue;
			}
			current_page = which_page;
			PREAD(fileno(backend_metafptr), &block_page,
					sizeof(BLOCK_ENTRY_PAGE), page_pos);
		}

		/* Skip if status is todelete or none */
		cloud_status = block_page.block_entries[e_index].status;
		if ((cloud_status == ST_NONE) || (cloud_status == ST_TODELETE)) {
			continue;
		}

		/* Set backend seq or object id (dedup mode) */
		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));
		SET_CLOUD_BLOCK_EXIST(block_uploading_status.block_exist);
#if (DEDUP_ENABLE)
		memcpy(block_uploading_status.backend_objid,
				block_page.block_entries[e_index].obj_id,
				sizeof(char) * OBJID_LENGTH);
#else
		block_uploading_status.backend_seq =  /* TODO: seq */
			block_page.block_entries[e_index].seqnum;

		write_log(10, "Debug: init progress file block%lld_%lld",
				block, block_uploading_status.backend_seq);
#endif
		/* Write this entry to progress file */
		entry_index = block % MAX_BLOCK_ENTRIES_PER_PAGE;
		offset = create_status_page(fd, block);
		if (offset < 0) {
			errcode = offset;
			goto errcode_handle;
		}
		PREAD(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
		memcpy(&(status_page.status_entry[entry_index]),
			&block_uploading_status,
			sizeof(BLOCK_UPLOADING_STATUS));
		PWRITE(fd, &status_page, sizeof(BLOCK_UPLOADING_PAGE), offset);
	}

	/* Finally write meta */
	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	progress_meta.now_action = NOW_UPLOADING;
	progress_meta.backend_size = backend_size;
	progress_meta.total_backend_blocks = backend_blocks;
	PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	flock(fd, LOCK_UN);

	return 0;

errcode_handle:
	flock(fd, LOCK_UN);
	return errcode;
}

int del_progress_file(int fd, ino_t inode)
{
	char filename[200];
	int ret, errcode;

	sprintf(filename, "%s/upload_bullpen/upload_progress_inode_%"PRIu64,
		METAPATH, (uint64_t)inode);

	close(fd);
	UNLINK(filename);

	write_log(10, "Debug: Close progress-info file for inode %"PRIu64"\n",
		(uint64_t)inode);

	return 0;

errcode_handle:
	return errcode;
}

int fetch_toupload_meta_path(char *pathname, ino_t inode)
{
	int errcode, ret;
	char path[200];

	sprintf(path, "%s/upload_bullpen", METAPATH);

	if (access(path, F_OK) == -1)
		MKDIR(path, 0700);

	sprintf(pathname, "%s/hcfs_local_meta_%"PRIu64".tmp",
			path, (uint64_t)inode);

	return 0;

errcode_handle:
	return errcode;
}

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq)
{

	sprintf(pathname, "/tmp/hcfs_sync_block_%"PRIu64"_%lld.tmp",
		(uint64_t)inode, block_no);

	return 0;
}

int fetch_backend_meta_path(char *pathname, ino_t inode)
{
	char path[200];
	int errcode;
	int ret;

	sprintf(path, "%s/upload_bullpen", METAPATH);

	if (access(path, F_OK) == -1)
		MKDIR(path, 0700);

	sprintf(pathname, "%s/hcfs_backend_meta_%"PRIu64".tmp",
			path, (uint64_t)inode);
	return 0;

errcode_handle:
	return errcode;
}

void fetch_del_backend_meta_path(char *pathname, ino_t inode)
{
	sprintf(pathname, "/tmp/backend_meta_%"PRIu64".del",
			(uint64_t)inode);
	return;
}

void fetch_progress_file_path(char *pathname, ino_t inode)
{

	sprintf(pathname, "%s/upload_bullpen/upload_progress_inode_%"PRIu64,
		METAPATH, (uint64_t)inode);

	return;
}

/**
 * Check whether target file exists or not and copy source file.
 *
 * This function is used when copying to-upload meta and blocks. It first
 * checks whether source file exist and whether target file does not exist.
 * Then lock source file and copy it. Note that if target file had been
 * copied, then never copy it again.
 *
 * @return 0 if succeed in copy, -EEXIST in case of target file existing.
 */
int check_and_copy_file(const char *srcpath, const char *tarpath, BOOL lock_src)
{
	int errcode;
	int ret;
	size_t read_size;
	size_t ret_size;
	ssize_t ret_ssize;
	FILE *src_ptr, *tar_ptr;
	char filebuf[4100];
	long long temp_trunc_size;
	long ret_pos;

	/* source file should exist */
	if (access(srcpath, F_OK) != 0) {
		errcode = errno;
		if (errcode == ENOENT)
			write_log(2, "Warn: Source file does not exist. In %s\n",
				__func__);
		else
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		return -errcode;
	}

	/* if target file exists, do not copy it.
	   (it may be copied by another process) */
	ret = access(tarpath, F_OK);
	if (ret == 0) {
		return -EEXIST;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "IO error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			return -errcode;
		}
	}

	src_ptr = fopen(srcpath, "r");
	if (src_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -errcode;
	}

	/* Lock source if needed */
	if (lock_src == TRUE)
		flock(fileno(src_ptr), LOCK_EX);

	/* Check again to avoid race condition*/
	if (access(tarpath, F_OK) == 0) {
		if (lock_src == TRUE)
			flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -EEXIST;
	}

	if (access(srcpath, F_OK) < 0) {
		errcode = errno;
		if (lock_src == TRUE)
			flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}

	tar_ptr = fopen(tarpath, "a+");
	if (tar_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));

		if (lock_src == TRUE)
			flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}
	fclose(tar_ptr);
	tar_ptr = fopen(tarpath, "r+");
	if (tar_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));

		if (lock_src == TRUE)
			flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}
	flock(fileno(tar_ptr), LOCK_EX);

	/* After locking target file, check file size. If size > 0,
	 * then do NOT need to copy it again. Just do nothing and return. */
	FSEEK(tar_ptr, 0, SEEK_END);
	FTELL(tar_ptr);
	if (ret_pos > 0) {
		if (lock_src == TRUE)
			flock(fileno(src_ptr), LOCK_UN);
		flock(fileno(tar_ptr), LOCK_UN);
		fclose(src_ptr);
		fclose(tar_ptr);
		return 0;
	}

	setbuf(tar_ptr, NULL);
	/* Begin to copy */
	FSEEK(src_ptr, 0, SEEK_SET);
	FSEEK(tar_ptr, 0, SEEK_SET);
	while (!feof(src_ptr)) {
		FREAD(filebuf, 1, 4096, src_ptr);
		read_size = ret_size;
		if (read_size > 0) {
			FWRITE(filebuf, 1, read_size, tar_ptr);
		} else {
			break;
		}
	}

	/* Copy xattr "trunc_size" if it exists */
#ifndef _ANDROID_ENV_
	ret_ssize = fgetxattr(fileno(src_ptr), "user.trunc_size",
		&temp_trunc_size, sizeof(long long));
	if (ret_ssize >= 0) {
		fsetxattr(fileno(tar_ptr), "user.trunc_size",
			&temp_trunc_size, sizeof(long long), 0);

		fremovexattr(fileno(src_ptr), "user.trunc_size");
		write_log(10, "Debug: trunc_size = %lld",temp_trunc_size);
	}
#endif

	/* Unlock soruce file */
	if (lock_src == TRUE)
		flock(fileno(src_ptr), LOCK_UN);
	flock(fileno(tar_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);

	return 0;

errcode_handle:
	if (lock_src == TRUE)
		flock(fileno(src_ptr), LOCK_UN);
	flock(fileno(tar_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);
	if (access(srcpath, F_OK) == 0)
		unlink(srcpath);
	return errcode;
}

char did_block_finish_uploading(int fd, long long blockno)
{
	int ret, errcode;
	ssize_t ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;
	PROGRESS_META progress_meta;

	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	if (blockno + 1 > progress_meta.total_toupload_blocks) {
		write_log(10, "Debug: Do not care about block %lld because #"
			" of to-upload blocks is %lld\n", blockno,
			progress_meta.total_toupload_blocks);
		return TRUE;
	}

	ret = get_progress_info(fd, blockno, &block_uploading_status);
	if (ret < 0) {
		if (ret != -ENOENT) {
			write_log(0, "Error: Fail to get progress info."
					" Code %d\n", -ret);
			return FALSE;
		} else {
			return FALSE;
		}
	}
	return block_uploading_status.finish_uploading;

errcode_handle:
	return errcode;
}

/**
 * init_backend_file_info()
 *
 * Initialize block information(seq/obj id) backend regfile meta. If it is NOT
 * reverting mode, then first download backend meta and init progress file using
 * this backend meta. In case of meta does not exist on cloud, it is regarded as
 * uploading first time. If it is reverting mode now, then just read the
 * progress meta and fetch backend size.
 *
 * @param ptr A pointer points to information of a now syncing inode.
 * @param first_upload Store whether it is uploaded first time.
 * @param backend_size File size on cloud of this inode.
 * @param total_backend_blocks Total # of blocks of this backend file.
 *
 * @return 0 on success, -ECANCELED when cancelling to sync,
 *         or other negative error code.
 */
int init_backend_file_info(const SYNC_THREAD_TYPE *ptr, long long *backend_size,
		long long *total_backend_blocks)
{
	FILE *backend_metafptr;
	char backend_metapath[400];
	char objname[400];
	struct stat tempfilestat;
	int errcode, ret;
	ssize_t ret_ssize;
	BOOL first_upload;

	if (!S_ISREG(ptr->this_mode))
		return 0;

	if (ptr->is_revert == TRUE) {
		/* Reverting/continuing to upload. Just read progress
		 * meta and check. */
		PROGRESS_META progress_meta;

		PREAD(ptr->progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);

		/* Cancel to coninue */
		if (progress_meta.now_action == PREPARING) {
			write_log(2, "Interrupt before uploading, do nothing and"
				" cancel uploading\n");
			return -ECANCELED;

		} else {
			*backend_size = progress_meta.backend_size;
			*total_backend_blocks =
				progress_meta.total_backend_blocks;
		}

	} else {	
		/* Try to download backend meta */
		backend_metafptr = NULL;
		fetch_backend_meta_path(backend_metapath, ptr->inode);
		backend_metafptr = fopen(backend_metapath, "w+");
		if (backend_metafptr == NULL) {
			errcode = errno;
			return -errcode;
		}
		setbuf(backend_metafptr, NULL); /* Do not need to lock */

		fetch_backend_meta_objname(objname, ptr->inode);
		ret = fetch_from_cloud(backend_metafptr, FETCH_FILE_META,
				objname);
		if (ret < 0) {
			if (ret == -ENOENT) {
				write_log(10, "Debug: upload first time\n");
				first_upload = TRUE;
				fclose(backend_metafptr);
				unlink(backend_metapath);
			} else { /* fetch error */
				write_log(0, "Fail to donwload %s in %s\n",
					objname, __func__);
				fclose(backend_metafptr);
				/* Be careful with using macro UNLINK */
				unlink(backend_metapath);
				return ret;
			}
		} else { /* Success */
			first_upload = FALSE;
		}

		/* Init backend info and unlink it */
		if (first_upload == TRUE) {
			ret = init_progress_info(ptr->progress_fd, 0, 0,
					NULL);
			*backend_size = 0;
			*total_backend_blocks = 0; 

		} else {
			PREAD(fileno(backend_metafptr), &tempfilestat,
					sizeof(struct stat), 0);
			*backend_size = tempfilestat.st_size;
			*total_backend_blocks = (*backend_size == 0) ? 
				0 : (*backend_size - 1) / MAX_BLOCK_SIZE + 1;
			ret = init_progress_info(ptr->progress_fd,
				*total_backend_blocks, *backend_size,
				backend_metafptr);

			fclose(backend_metafptr);
			unlink(backend_metapath);
		}

		write_log(10, "Debug: backend size = %lld\n", *backend_size);
		if (ret < 0) /* init progress fail */
			return ret;
	}

	return 0;

errcode_handle:
	fclose(backend_metafptr);
	unlink(backend_metapath);
	return errcode;
}

/**
 * Revert uploading for given inode
 *
 * Following are some crash points:
 * 1. open progress info file
 * 2. copy from local meta to to-upload meta
 *    - Communicate to fuse process and tag inode as uploading
 * 3. download backend meta
 * 4. init all backend block seq or obj-id
 * 5. unlink downloaded meta
 * -------------------------- Continue uploading after finish 5.
 * 6. upload blocks
 * 7. upload to-upload meta
 * 8. unlink to-upload meta
 * 9. delete all backend old blocks
 * 10. close progress info file
 *
 */
void continue_inode_upload(SYNC_THREAD_TYPE *data_ptr)
{
	char toupload_meta_exist, backend_meta_exist, local_meta_exist;
	char toupload_meta_path[200];
	char backend_meta_path[200];
	char local_meta_path[200];
	int errcode;
	mode_t this_mode;
	ino_t inode;
	int progress_fd;
	long long total_backend_blocks, total_toupload_blocks;
	ssize_t ret_ssize;
	char finish_init;
	int ret;
	PROGRESS_META progress_meta;
	int which_index;

	finish_init = FALSE;
	this_mode = data_ptr->this_mode;
	inode = data_ptr->inode;
	progress_fd = data_ptr->progress_fd;
	which_index = data_ptr->which_index;

	fetch_backend_meta_path(backend_meta_path, inode);
	fetch_toupload_meta_path(toupload_meta_path, inode);
	fetch_meta_path(local_meta_path, inode);

	write_log(10, "Debug: Now begin to revert uploading inode_%"PRIu64"\n",
			(uint64_t)inode);
	/* Check backend meta exist */
	if (access(backend_meta_path, F_OK) == 0) {
		backend_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			backend_meta_exist = FALSE;
		}
	}

	/* Check to-upload meta exist */
	if (access(toupload_meta_path, F_OK) == 0) {
		toupload_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			toupload_meta_exist = FALSE;
		}
	}

	/* Check local meta */
	if (access(local_meta_path, F_OK) == 0) {
		local_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			local_meta_exist = FALSE;
		}
	}

	/* If it is not regfile (strange), then just remove all and upload
	 * it again. */
	if (!S_ISREG(this_mode)) {
		if (toupload_meta_exist == TRUE)
			UNLINK(toupload_meta_path);
		if (backend_meta_exist == TRUE)
			UNLINK(backend_meta_path);
		sync_ctl.threads_error[data_ptr->which_index] = TRUE;
		sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
		return;
	}

	PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
	if (progress_meta.now_action != PREPARING) {
		total_backend_blocks = progress_meta.total_backend_blocks;
		total_toupload_blocks = progress_meta.total_toupload_blocks;
		finish_init = TRUE;
	} else {
		total_backend_blocks = 0;
		total_toupload_blocks = 0;
		finish_init = FALSE;
	}

	/*** Begin to check break point ***/
	if (toupload_meta_exist == TRUE) {
		if ((backend_meta_exist == FALSE) && (finish_init == TRUE)) {
		/* Keep on uploading. case[5, 6], case6, case[6, 7],
		case7, case[7, 8], case8 */
			if (local_meta_exist) {
				write_log(10, "Debug: begin continue uploading"
					" inode %"PRIu64"\n", (uint64_t)inode);
				sync_single_inode((void *)data_ptr);
				return;
			} else {
				delete_backend_blocks(progress_fd,
					total_toupload_blocks, inode,
					DEL_TOUPLOAD_BLOCKS);
				sync_ctl.threads_finished[data_ptr->which_index]
				       = TRUE;
				return;	
			}

		} else {
		/* NOT begin to upload, so cancel uploading.
		case2, case[2, 3], case3, case[3, 4], case4, case[4, 5], case5,
		 */
			if (backend_meta_exist)
				unlink(backend_meta_path);
			unlink(toupload_meta_path);
		}
	} else {
		if (finish_init == TRUE) {
		/* Finish uploading all blocks and meta,
		remove backend old block. case[8, 9], case9, case[9. 10],
		case10. Do not need to update backend size again. */
			delete_backend_blocks(progress_fd, total_backend_blocks,
				inode, DEL_BACKEND_BLOCKS);
			sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
			return;
		} else {
		/* Crash before copying local meta, so just
		cancel uploading. case[1, 2] */
			if (backend_meta_exist)
				unlink(backend_meta_path);
		}

	}

	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;

errcode_handle:
	write_log(0, "Error: Fail to revert/continue uploading inode %"PRIu64"\n",
			(uint64_t)inode);
	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;
}

/**
 * continue_inode_sync()
 *
 * Let inode continues to upload. When now_action is PREPARING,
 * cancel to upload this time. If now_action is NOW_UPLOADING,
 * keep inode uploading by calling sync_single_inode() subroutine.
 * If now_action is DEL_BACKEND_BLOCKS, then delete backend old blocks.
 * If now_action is DEL_TOUPLOAD_BLOCKS, then delete those new blocks
 * on cloud.
 */ 
void continue_inode_sync(SYNC_THREAD_TYPE *data_ptr)
{
	char toupload_meta_exist, backend_meta_exist;
	char toupload_meta_path[200];
	char backend_meta_path[200];
	int errcode;
	mode_t this_mode;
	ino_t inode;
	int progress_fd;
	long long total_blocks;
	ssize_t ret_ssize;
	int ret;
	char now_action;
	PROGRESS_META progress_meta;

	this_mode = data_ptr->this_mode;
	inode = data_ptr->inode;
	progress_fd = data_ptr->progress_fd;

	fetch_backend_meta_path(backend_meta_path, inode);
	fetch_toupload_meta_path(toupload_meta_path, inode);

	write_log(10, "Debug sync: Now begin to revert uploading inode_%"
			PRIu64"\n", (uint64_t)inode);

	/* Check backend meta exist */
	if (access(backend_meta_path, F_OK) == 0) {
		backend_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			backend_meta_exist = FALSE;
		}
	}

	/* Check to-upload meta exist */
	if (access(toupload_meta_path, F_OK) == 0) {
		toupload_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			goto errcode_handle;
		} else {
			toupload_meta_exist = FALSE;
		}
	}

	/* If it is not regfile (strange), then just remove all and upload
	 * it again. */
	if (!S_ISREG(this_mode)) {
		if (toupload_meta_exist == TRUE)
			UNLINK(toupload_meta_path);
		if (backend_meta_exist == TRUE)
			UNLINK(backend_meta_path);
		sync_ctl.threads_error[data_ptr->which_index] = TRUE;
		sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
		return;
	}

	/*** Begin to check break point ***/
	PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
	now_action = progress_meta.now_action;
	if (now_action == PREPARING) {
		write_log(4, "sync: Cancel to continue uploading inode %"
				PRIu64"\n", (uint64_t)inode);
		/* Do nothing and re-upload next time */

	} else if (now_action == NOW_UPLOADING) {
		if (toupload_meta_exist == TRUE) {
			write_log(4, "sync: Continue uploading inode %"
					PRIu64"\n", (uint64_t)inode);
			sync_single_inode((void *)data_ptr);
			return;
		} else {
			/* Maybe toupload_meta is uploaded
			 * and now_action flag is not set to
			 * DEL_BACKEND_BLOCKS because of 
			 * unexpected crash. mm...perhaps do
			 * nothing and re-upload next time. */
			write_log(2, "sync warn: inode %"PRIu64" toupload meta"
				"disappear. Perhaps crash?\n", (uint64_t)inode);
		}
	
	} else if (now_action == DEL_TOUPLOAD_BLOCKS) {
		write_log(4, "sync: Continue to del toupload blocks of inode %"
				PRIu64"\n", (uint64_t)inode);
		delete_backend_blocks(progress_fd,
			progress_meta.total_toupload_blocks,
			inode, DEL_TOUPLOAD_BLOCKS);

	} else if (now_action == DEL_BACKEND_BLOCKS) {
		write_log(4, "sync: Continue to del cloud old blocks of inode %"
				PRIu64"\n", (uint64_t)inode);
		delete_backend_blocks(progress_fd,
			progress_meta.total_backend_blocks,
			inode, DEL_BACKEND_BLOCKS);
	}

	if (toupload_meta_exist == TRUE)
		UNLINK(toupload_meta_path);
	if (backend_meta_exist == TRUE)
		UNLINK(backend_meta_path);
	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;

errcode_handle:
	write_log(0, "Error: Fail to revert/continue uploading inode %"PRIu64"\n",
			(uint64_t)inode);
	if (toupload_meta_exist == TRUE)
		unlink(toupload_meta_path);
	if (backend_meta_exist == TRUE)
		unlink(backend_meta_path);
	sync_ctl.threads_error[data_ptr->which_index] = TRUE;
	sync_ctl.threads_finished[data_ptr->which_index] = TRUE;
	return;
}

/**
 * change_action()
 *
 * Change now_action field in progress_meta.
 *
 * @param fd File descriptor of this progress file
 * @param now_action New action to be changed.
 *
 * @return 0 on success, otherwise negative error code.
 */ 
int change_action(int fd, char now_action)
{
	int ret, errcode;
	ssize_t ret_ssize;
	PROGRESS_META progress_meta;

	flock(fd, LOCK_EX);
	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	progress_meta.now_action = now_action;
	PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	flock(fd, LOCK_UN);

	return 0;

errcode_handle:
	return errcode;
}
