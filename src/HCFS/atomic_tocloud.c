#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hcfs_tocloud.h"
#include "atomic_tocloud.h"
#include "macro.h"
#include "global.h"
#include "metaops.h"

#define BLK_INCREMENTS MAX_BLOCK_ENTRIES_PER_PAGE
extern SYSTEM_CONF_STRUCT system_config;

/*
 * Tag inode as uploading or not_uploading in fuse process memory.
 *
 * Main function of communicating with fuse process. This aims to
 * tag or untag the inode is_uploading flag.
 *
 * @return 0 if succeeding in tagging status, otherwise -1 on error.
 */
int tag_status_on_fuse(ino_t this_inode, char status, int fd)
{
	int sockfd;
	int ret, resp, errcode;
	struct sockaddr_un addr;
	UPLOADING_COMMUNICATION_DATA data;

	/* Prepare data */
	data.inode = this_inode;
	data.is_uploading = status;
	data.progress_list_fd = fd;

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


	send(sockfd, &data, sizeof(UPLOADING_COMMUNICATION_DATA), 0);
	recv(sockfd, &resp, sizeof(int), 0);

	if (resp < 0) {
		write_log(0, "Communication error: Response code %d in %s",
			resp, __func__);
		ret = -1;
	} else {
		write_log(10, "Debug: Communicating to fuse success\n");
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

static inline long long _get_filepos(long long block_index)
{
	long long offset;
	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index 
			+ sizeof(PROGRESS_META);
	return offset;
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
	long long ret_pos, end_pos;
	BLOCK_UPLOADING_PAGE temp_page;
	PTR_ENTRY_PAGE temp_ptr_page, zero_ptr_page;
	long long tmp_pos;
	long long ptr_page_index, ptr_index;
	int errcode;
	ssize_t ret_ssize;
	int level, i;

	target_page = block_index / MAX_BLOCK_ENTRIES_PER_PAGE;
	
	which_indirect = check_page_level(target_page);
	PREAD(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	switch(which_indirect) {
	case 0:
		if (progress_meta.direct == 0) {
			end_pos = lseek(fd, 0, SEEK_END);
			memset(&temp_page, 0, sizeof(BLOCK_UPLOADING_PAGE));
			PWRITE(fd, &temp_page, sizeof(BLOCK_UPLOADING_PAGE),
				end_pos);
			progress_meta.direct = end_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);
		}
		return progress_meta.direct;
	case 1:
		if (progress_meta.single_indirect == 0) {
			end_pos = lseek(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				end_pos);
			progress_meta.single_indirect = end_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);	
		}
		tmp_pos = progress_meta.single_indirect;
		break;
	case 2:
		if (progress_meta.double_indirect == 0) {
			end_pos = lseek(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				end_pos);
			progress_meta.double_indirect = end_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);	
		}
		tmp_pos = progress_meta.double_indirect;
		break;
	case 3:
		if (progress_meta.triple_indirect == 0) {
			end_pos = lseek(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				end_pos);
			progress_meta.triple_indirect = end_pos;
			PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);	
		}
		tmp_pos = progress_meta.triple_indirect;
		break;
	case 4:
		if (progress_meta.quadruple_indirect == 0) {
			end_pos = lseek(fd, 0, SEEK_END);
			memset(&temp_ptr_page, 0, sizeof(PTR_ENTRY_PAGE));
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				end_pos);
			progress_meta.quadruple_indirect = end_pos;
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
			end_pos = lseek(fd, 0, SEEK_END);
			PWRITE(fd, &zero_ptr_page, sizeof(PTR_ENTRY_PAGE),
				end_pos);
			temp_ptr_page.ptr[ptr_page_index] = end_pos;
			PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE),
				tmp_pos);
		}
		tmp_pos = temp_ptr_page.ptr[ptr_page_index];
	}

	/* Create status page */
	if (temp_ptr_page.ptr[ptr_index] == 0) {	
		end_pos = lseek(fd, 0, SEEK_END);
		memset(&temp_page, 0, sizeof(BLOCK_UPLOADING_PAGE));
		PWRITE(fd, &temp_page, sizeof(BLOCK_UPLOADING_PAGE),
				end_pos);
		temp_ptr_page.ptr[ptr_index] = end_pos;
		PWRITE(fd, &temp_ptr_page, sizeof(PTR_ENTRY_PAGE), tmp_pos);
	}

	return temp_ptr_page.ptr[ptr_index];

errcode_handle:
	write_log(0, "Fail to create page in %s\n", __func__);
	return errcode;
}

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

	if (offset == 0) {
		/* Reading exceeds EOF. It may occur when backend
		   has truncated data and this block has not been
		   uploaded. */
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
	} else {
		memcpy(block_uploading_status,
			&(block_page.status_entry[entry_index]),
			sizeof(BLOCK_UPLOADING_STATUS));
	}

	return ret_ssize;

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
	off_t end_pos;
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
		write_log(0, "Error: Fail to set progress\n");
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
	off_t end_pos;
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
		write_log(0, "Error: Fail to set progress\n");
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

	if (block_uploading_status->finish_uploading == TRUE)
		write_log(10, "Debug: block_%lld finished uploading - "
			"fd = %d\n", block_index, fd);

	return 0;

errcode_handle:
	flock(fd, LOCK_UN);
	return errcode;
}
#endif

int open_progress_info(ino_t inode)
{
	int ret_fd;
	int errcode, ret;
	char filename[200];
	char pathname[200];

	sprintf(pathname, "%s/upload_bullpen", METAPATH);

	if (access(pathname, F_OK) == -1)
		mkdir(pathname, 0700);

	sprintf(filename, "%s/upload_progress_inode_%ld",
		pathname, inode);
	
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
		return ret_fd;
	} else {
		write_log(10, "Debug: Open progress-info file for inode %lld,"
			" fd = %d\n", inode, ret_fd);
	}

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
 * all backend data blocks, The field "finish_init" in progress meta will be
 * set as TRUE.
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
	memset(&progress_meta, 0, sizeof(PROGRESS_META));
	PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	if (backend_metafptr == NULL) { /* backend meta does not exist */
		progress_meta.finish_init_backend_data = TRUE;
		progress_meta.backend_size = 0;
		progress_meta.total_backend_blocks = 0;
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
		if ((cloud_status == ST_NONE) || (cloud_status == ST_TODELETE))
			continue;
			
		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));
		SET_CLOUD_BLOCK_EXIST(block_uploading_status.block_exist);
#if (DEDUP_ENABLE)
		memcpy(block_uploading_status.backend_objid,
				block_page.block_entries[e_index].obj_id,
				sizeof(char) * OBJID_LENGTH);
#else
		block_uploading_status.backend_seq = 
			MAX(block_page.block_entries[e_index].seqnum[0],
			block_page.block_entries[e_index].seqnum[1]);
#endif
		write_log(10, "Debug: init progress file block%ld_%lld", block, block_uploading_status.backend_seq);
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
	progress_meta.finish_init_backend_data = TRUE;
	progress_meta.backend_size = backend_size;
	progress_meta.total_backend_blocks = backend_blocks;
	PWRITE(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	flock(fd, LOCK_UN);

	return 0;

errcode_handle:
	flock(fd, LOCK_UN);
	return errcode;
}

int close_progress_info(int fd, ino_t inode)
{
	char filename[200];
	int ret, errcode;

	sprintf(filename, "%s/upload_bullpen/upload_progress_inode_%ld",
		METAPATH, inode);

	close(fd);
	UNLINK(filename);

	write_log(10, "Debug: Close progress-info file for inode %lld\n",
		inode);

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

#ifdef ARM_32bit_
	sprintf(pathname, "%s/hcfs_local_meta_%lld.tmp", path, inode);
#else
	sprintf(pathname, "%s/hcfs_local_meta_%ld.tmp", path, inode);
#endif

	return 0;

errcode_handle:
	return errcode;
}

int fetch_toupload_block_path(char *pathname, ino_t inode,
	long long block_no, long long seq)
{

#ifdef ARM_32bit_
	sprintf(pathname, "/dev/shm/hcfs_sync_block_%lld_%lld_%lld.tmp",
		inode, block_no, seq);
#else
	sprintf(pathname, "/dev/shm/hcfs_sync_block_%ld_%lld_%lld.tmp",
		inode, block_no, seq);
#endif

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

#ifdef ARM_32bit_
	sprintf(pathname, "%s/hcfs_backend_meta_%lld.tmp", path, inode);
#else
	sprintf(pathname, "%s/hcfs_backend_meta_%ld.tmp", path, inode);
#endif
	return 0;

errcode_handle:
	return errcode;
}

int fetch_progress_file_path(char *pathname, ino_t inode)
{

#ifdef ARM_32bit_
	sprintf(pathname, "%s/upload_bullpen/upload_progress_inode_%lld",
		METAPATH, inode);
#else
	sprintf(pathname, "%s/upload_bullpen/upload_progress_inode_%ld",
		METAPATH, inode);
#endif
	return 0;


}

/**
 * Check whether target file exists or not and copy source file.
 *
 * This function first checks whether source file exist and whether target file
 * does not exist. Then lock source file and copy it. The method of copying a
 * file is based on argument "copy", which is a function pointer that decides
 * how to copy a file from source to target. This function is thread-safe.
 *
 * @return 0 if succeed in copy, -EEXIST in case of target file existing.
 */
int check_and_copy_file(const char *srcpath, const char *tarpath)
{
	int errcode;
	int ret;
	size_t read_size;
	size_t ret_size;
	ssize_t ret_ssize;
	FILE *src_ptr, *tar_ptr;
	char filebuf[4100];
	long long temp_trunc_size;

	/* source file should exist */
	if (access(srcpath, F_OK) != 0) {
		errcode = errno;
		if (errcode == ENOENT)
			write_log(0, "Error: Source file does not exist. In %s\n",
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

	/* Lock source, do NOT need to lock target because
	   other processes will be locked at source file. */
	flock(fileno(src_ptr), LOCK_EX);

	/* Check again to avoid race condition*/
	if (access(tarpath, F_OK) == 0) {
		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -EEXIST;
	}

	if (access(srcpath, F_OK) < 0) {
		errcode = errno;
		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}

	tar_ptr = fopen(tarpath, "w+");
	if (tar_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));

		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
		return -errcode;
	}

	/* Copy */
	FSEEK(src_ptr, 0, SEEK_SET);
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
	ret_ssize = fgetxattr(fileno(src_ptr), "user.trunc_size",
		&temp_trunc_size, sizeof(long long));
	if (ret_ssize >= 0) {
		fsetxattr(fileno(tar_ptr), "user.trunc_size",
			&temp_trunc_size, sizeof(long long), 0);

		fremovexattr(fileno(src_ptr), "user.trunc_size");
		write_log(10, "Debug: trunc_size = %lld",temp_trunc_size);
	}

	/* Unlock soruce file */
	fclose(src_ptr);
	fclose(tar_ptr);
	flock(fileno(src_ptr), LOCK_UN);

	return 0;

errcode_handle:
	fclose(src_ptr);
	fclose(tar_ptr);
	flock(fileno(src_ptr), LOCK_UN);
	return errcode;
}

char did_block_finish_uploading(int fd, long long blockno)
{
	int ret;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	ret = get_progress_info(fd, blockno, &block_uploading_status);
	if (ret < 0) {
		write_log(0, "Error: Fail to get progress info\n");
		return FALSE;
	}
	return block_uploading_status.finish_uploading;
}


/**
 * Revert uploading for given inode
 *
 * Following are some crash points:
 * 1. open progress info file
 * 2. copy from local meta to to-upload meta
 * 3. download backend meta
 * 4. init all backend block (seq or obj-id)
 * 5. unlink downloaded meta
 * 6. upload blocks
 * 7. upload to-upload meta
 * 8. unlink to-upload meta
 * 9. delete all backend old blocks
 * 10. close progress info file
 *
 */
void revert_inode_uploading(SYNC_THREAD_TYPE *data_ptr)
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
	char finish_init;
	PROGRESS_META progress_meta;

	finish_init = FALSE;
	this_mode = data_ptr->this_mode;
	inode = data_ptr->inode;
	progress_fd = data_ptr->progress_fd;

	fetch_backend_meta_path(backend_meta_path, inode);
	fetch_toupload_meta_path(toupload_meta_path, inode);

	write_log(10, "Debug: Now begin to revert uploading inode_%ld\n", inode);
	/* Check backend meta exist */
	if (access(backend_meta_path, F_OK) == 0) {
		backend_meta_exist = TRUE;
	} else {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
			return;
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
			return;
		} else {
			toupload_meta_exist = FALSE;
		}
	}

	/*** Begin to revert ***/
	PREAD(progress_fd, &progress_meta, sizeof(PROGRESS_META), 0);
	if (ret_ssize == 0) {
		finish_init = FALSE;
	} else {
		total_blocks = progress_meta.total_backend_blocks;
		finish_init = progress_meta.finish_init_backend_data;
	}

	if (toupload_meta_exist == TRUE) {
		if ((backend_meta_exist == FALSE) && (finish_init == TRUE)) {
		/* Keep on uploading. case[5, 6], case6, case[6, 7],
		case7, case[7, 8], case8 */

			write_log(10, "Debug: begin revert uploading inode %ld\n",
				inode);
			sync_single_inode((void *)data_ptr);

		} else { 
		/* NOT begin to upload, so cancel uploading.
		case2, case[2, 3], case3, case[3, 4], case4, case[4, 5], case5,
		 */
			if (backend_meta_exist)
				unlink(backend_meta_path);
			unlink(toupload_meta_path);
		}
	} else {
		if (finish_init == FALSE) {
		/* Crash before copying local meta, so just 
		cancel uploading. case[1, 2] */
			if (backend_meta_exist)
				unlink(backend_meta_path);
		} else {
		/* Finish uploading all blocks and meta,
		remove backend old block. case[8, 9], case9, case[9. 10],
		case10 */
			delete_backend_blocks(progress_fd, total_blocks,
				inode, BACKEND_BLOCKS);
		}
	}

	return;

errcode_handle:
	write_log(0, "Error: Fail to revert uploading inode %ld\n", inode);
	return;
}

int uploading_revert()
{	
	DIR *dirptr;
	int fd;
	struct dirent temp_dirent;
	struct dirent *direntptr;
	char upload_pathname[100];
	char progress_filepath[300];
	int errcode, ret, count, inode_count;
	int total_reverted_inode;
	char all_finish;
	ino_t inode;
	ino_t reverted_inode[5 * MAX_SYNC_CONCURRENCY];
	SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];


	sprintf(upload_pathname, "%s/upload_bullpen", METAPATH);
	if (access(upload_pathname, F_OK) < 0) {
		write_log(2, "Upload bullpen does not exist\n");
		return 0;
	}

	dirptr = opendir(upload_pathname);
	if (dirptr == NULL) {
		errcode = errno;
		write_log(0, "Fail to open %s. Code %d, %s\n",
			upload_pathname, errcode, strerror(errcode));
		return -errcode;
	}

	ret = readdir_r(dirptr, &temp_dirent, &direntptr);
	if (ret > 0) {
		errcode = ret;
		write_log(0, "Fail to read %s. Code %d, %s\n",
			upload_pathname, errcode, strerror(errcode));
		closedir(dirptr);
		return -errcode;
	}

	errcode = 0;
	total_reverted_inode = 0;
	while (direntptr) {
		if (strncmp("upload_progress", temp_dirent.d_name, 15) == 0) {
			ret = sscanf(temp_dirent.d_name,
				"upload_progress_inode_%ld", &inode);
			sprintf(progress_filepath, "%s/%s", upload_pathname,
				temp_dirent.d_name);
			fd = open(progress_filepath, O_RDWR);
			if ((ret != 1) || (fd <= 0)) {
				ret = readdir_r(dirptr, &temp_dirent,
						&direntptr);
				if (ret > 0) {
					errcode = errno;
					break;
				}
				continue;
			}

			/* Error on total_reverted_inode >= MAX_SYNC_CONCURRENCY -1.
			Because number of threads to sync inode is less than
			MAX_SYNC_CONCURRENCY, then number of all undone threads
			shoud be less than it. */
			if (total_reverted_inode >= MAX_SYNC_CONCURRENCY - 1) {
				write_log(0, "Error: Why does number of reverted"
					" inode exceed MAX_SYNC_CONCURRENCY?\n");
			}
			sem_wait(&(sync_ctl.sync_queue_sem));
			sem_wait(&(sync_ctl.sync_op_sem));
			for (count = 0; count < MAX_SYNC_CONCURRENCY; 
								count++) {
				if (sync_ctl.threads_in_use[count] == 0)
					break;
			}
			sync_ctl.threads_in_use[count] = inode;
			sync_ctl.threads_created[count] = FALSE;
			sync_ctl.threads_error[count] = FALSE;
			sync_ctl.progress_fd[count] = fd;
			sync_ctl.is_revert[count] = TRUE;
			sync_threads[count].inode = inode;
			sync_threads[count].this_mode = S_IFREG; // temp
			sync_threads[count].progress_fd = fd;
			sync_threads[count].is_revert = TRUE;
			pthread_create(&(sync_ctl.inode_sync_thread[count]),
					NULL, (void *)&revert_inode_uploading,
					(void *)&(sync_threads[count]));
			sync_ctl.threads_created[count] = TRUE;
			sync_ctl.total_active_sync_threads++;

			sem_post(&(sync_ctl.sync_op_sem));
			reverted_inode[total_reverted_inode++] = inode;
		}
		
		ret = readdir_r(dirptr, &temp_dirent,
				&direntptr);
		if (ret > 0) {
			errcode = errno;
			break;
		}

	}

	closedir(dirptr);
	if (errcode > 0) {
		write_log(0, "Fail to traverse dir %s. Code %d, %s\n",
			upload_pathname, errcode , strerror(errcode));
		return -errcode;
	}

	/* Wait for all reveting threads */
	all_finish = TRUE;
	for (inode_count = 0; inode_count < total_reverted_inode ; inode_count++) {
		inode = reverted_inode[inode_count];
		sem_wait(&(sync_ctl.sync_op_sem));
		for (count = 0; count < MAX_SYNC_CONCURRENCY; count++) {
			if (sync_ctl.threads_in_use[count] == inode) {
				all_finish = FALSE;
				break;
			}
		}
		sem_post(&(sync_ctl.sync_op_sem));
		if (all_finish == FALSE) {
			usleep(100000);
			inode_count--;
			all_finish = TRUE;
		}
	}

	return 0;
}
