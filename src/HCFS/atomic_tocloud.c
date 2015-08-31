#include "hcfs_tocloud.h"
#include "atomic_tocloud.h"
#include "macro.h"
#include "global.h"

#include <fcntl.h>
#include <sys/stat.h>

/**
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
		write_log(10, "Debug: Need init progress-info before getting");
		block_uploading_status->finish_uploading = FALSE;
	} else {
		write_log(10, "Debug: Get progress-info of block_%lld\n", 
			block_index);
	}

	return 0;

errcode_handle:
	return errcode;

}

int set_progress_info(int fd, long long block_index, 
	const char *finish_uploading,
	const long long *to_upload_seq,
	const long long *backend_seq)
{
	int errcode;
	long long offset;
	long long ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	if (finish_uploading != NULL)
		block_uploading_status.finish_uploading = *finish_uploading;
	if (to_upload_seq != NULL)
		block_uploading_status.to_upload_seq = *to_upload_seq;
	if (backend_seq != NULL)
		block_uploading_status.backend_seq = *backend_seq;

	memset(&block_uploading_status, 0, sizeof(BLOCK_UPLOADING_STATUS));

	flock(fd, LOCK_EX);
	PWRITE(fd, &block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	flock(fd, LOCK_UN);

	if (finish_uploading != NULL) {
		if (*finish_uploading == TRUE)
			write_log(10, "Debug: block_%lld finished uploading - "
				"fd = %d\n", block_index, fd);
	}

	return 0;

errcode_handle:
	return errcode;
}

int open_progress_info(ino_t inode)
{
	int ret_fd;
	int errcode;
	char filename[200];

	if (access("atomic_uploading_dir", F_OK) == -1) 
		mkdir("atomic_uploading_dir", 0700);

	sprintf(filename, "atomic_uploading_dir/upload_progress_inode_%ld", 
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

int init_progress_info(int fd, long long num_block)
{
	int i, errcode; 
	long long offset, ret_ssize;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	/* Do NOT need to lock file because this function will be called by
	   only one thread in sync_single_inode */
	offset = 0;
	memset(&block_uploading_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	for (i = 0; i < num_block; i++) {
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

	sprintf(filename, "atomic_uploading_dir/upload_progress_inode_%ld",
		inode);
	
	close(fd);
	UNLINK(filename);

	write_log(10, "Debug: Close progress-info file for inode %lld\n",
		inode);

	return 0;

errcode_handle:
	return errcode;
}
