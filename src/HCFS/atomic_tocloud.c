#include "hcfs_tocloud.h"
#include "atomic_tocloud.h"
#include "macro.h"
#include "global.h"

#include <fcntl.h>
#include <sys/stat.h>

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
	int ret, resp;
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

int get_progress_info(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	long long offset;
	int errcode;
	long long ret_ssize;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	flock(fd, LOCK_EX);
	PREAD(fd, block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	flock(fd, LOCK_UN);
	if (ret_ssize == 0) {
		/* Reading exceeds EOF. It may occur when backend
		   has truncated data and this block has not been
		   uploaded. */
		//write_log(10, "Debug: truncate?");
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
	}

	return 0;

errcode_handle:
	write_log(0, "Error: Fail to get progress-info of block_%lld\n",
			block_index);
	return errcode;

}

int set_progress_info(int fd, long long block_index, char finish_uploading,
	long long to_upload_seq, long long backend_seq)
{
	int errcode;
	long long offset;
	ssize_t ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;
/*
	block_uploading_status.finish_uploading = finish_uploading;
	block_uploading_status.to_upload_seq = to_upload_seq;
	block_uploading_status.backend_seq = backend_seq;
*/
	flock(fd, LOCK_EX);
	PREAD(fd, &block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	if (ret_ssize == 0) /* Init because backend meta is a truncated file */
		memset(&block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
	block_uploading_status.finish_uploading = finish_uploading;
	block_uploading_status.to_upload_seq = to_upload_seq;
	PWRITE(fd, &block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	flock(fd, LOCK_UN);

	if (finish_uploading == TRUE)
		write_log(10, "Debug: block_%lld finished uploading - "
			"fd = %d\n", block_index, fd);

	return 0;

errcode_handle:
	return errcode;
}

int get_progress_info_nonlock(int fd, long long block_index,
	BLOCK_UPLOADING_STATUS *block_uploading_status)
{
	long long offset;
	int errcode;
	long long ret_ssize;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	PREAD(fd, block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	if (ret_ssize == 0) {
		memset(block_uploading_status, 0,
			sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status->finish_uploading = FALSE;
	}

	return ret_ssize;

errcode_handle:
	write_log(0, "Error: Fail to get progress-info of block_%lld\n",
			block_index);
	return errcode;

}

int open_progress_info(ino_t inode)
{
	int ret_fd;
	int errcode;
	char filename[200];

	if (access("upload_bullpen", F_OK) == -1)
		mkdir("upload_bullpen", 0700);

	sprintf(filename, "upload_bullpen/upload_progress_inode_%ld",
		inode);
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
}

int init_progress_info(int fd, long long num_blocks, FILE *backend_metafptr)
{
	int errcode;
	long long offset, ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;
	struct stat tempstat;
	long long backend_blocks;
	long long progress_num_blocks;
	long long e_index, which_page, current_page, page_pos;
	long long block;
	BLOCK_ENTRY_PAGE block_page;
	FILE_META_TYPE tempfilemeta;

	/* Do NOT need to lock file because this function will be called by
	   the only one thread in sync_single_inode() */
	if (backend_metafptr == NULL) {
		offset = 0;
		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));
		for (block = 0; block < num_blocks; block++) {
			PWRITE(fd, &block_uploading_status,
					sizeof(BLOCK_UPLOADING_STATUS), offset);
			offset += sizeof(BLOCK_UPLOADING_STATUS);
		}

		return 0;
	}

	PREAD(fileno(backend_metafptr), &tempstat, sizeof(struct stat), 0);
	PREAD(fileno(backend_metafptr), &tempfilemeta, sizeof(FILE_META_TYPE),
							sizeof(struct stat));

	if (tempstat.st_size == 0)
		backend_blocks = 0;
	else
		backend_blocks = ((tempstat.st_size - 1)
				/ MAX_BLOCK_SIZE) + 1;
	progress_num_blocks = (num_blocks > backend_blocks) ?
		num_blocks : backend_blocks;
	write_log(10, "Debug: backend blocks = %lld\n", backend_blocks);

	/* Write into progress info */
	current_page = -1;
	offset = 0;
	for (block = 0; block < progress_num_blocks; block++) {
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

		memset(&block_uploading_status, 0,
				sizeof(BLOCK_UPLOADING_STATUS));
		block_uploading_status.backend_seq = 0; /* temp */

		PWRITE(fd, &block_uploading_status,
				sizeof(BLOCK_UPLOADING_STATUS), offset);
		offset += sizeof(BLOCK_UPLOADING_STATUS);
	}

	return 0;

errcode_handle:
	return errcode;
}

int close_progress_info(int fd, ino_t inode)
{
	char filename[200];
	int ret, errcode;

	sprintf(filename, "upload_bullpen/upload_progress_inode_%ld",
		inode);

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

	if (access("upload_bullpen", F_OK) == -1)
		MKDIR("upload_bullpen", 0700);

#ifdef ARM_32bit_
	sprintf(pathname, "upload_bullpen/hcfs_local_meta_%lld.tmp", inode);
#else
	sprintf(pathname, "upload_bullpen/hcfs_local_meta_%ld.tmp", inode);
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

/**
 * Check whether target file exists or not and copy source file.
 *
 * This function first checks whether source file exist and whether target file
 * does not exist and then lock source file and copy it if.
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

	tar_ptr = fopen(tarpath, "w");
	if (tar_ptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));

		flock(fileno(src_ptr), LOCK_UN);
		fclose(src_ptr);
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
	flock(fileno(src_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);

	return 0;

errcode_handle:
	flock(fileno(src_ptr), LOCK_UN);
	fclose(src_ptr);
	fclose(tar_ptr);
	return errcode;
}

char block_finish_uploading(int fd, long long blockno)
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
