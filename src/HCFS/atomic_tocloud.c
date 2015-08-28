#include "hcfs_tocloud.h"
#include "atomic_tocloud.h"
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
int tag_status_on_fuse(ino_t this_inode, char status)
{
	int sockfd;
	int ret, resp;
	struct sockaddr_un addr;
	UPLOADING_COMMUNICATION_DATA data;

	/* Prepare data */
	data.inode = this_inode;
	data.is_uploading = status;
	data.progress_list_fd = 0; // tmp

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

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	flock(fd, LOCK_EX);
	pread(fd, block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS), offset);
	flock(fd, LOCK_UN);

	return 0;
}

int set_progress_info(int fd, long long block_index, 
	const char *finish_uploading,
	const long long *to_upload_seq,
	const long long *backend_seq)
{
	long long offset;
	BLOCK_UPLOADING_STATUS block_uploading_status;

	offset = sizeof(BLOCK_UPLOADING_STATUS) * block_index;

	if (finish_uploading)
		block_uploading_status.finish_uploading = *finish_uploading;
	if (to_upload_seq)
		block_uploading_status.to_upload_seq = *to_upload_seq;
	if (backend_seq)
		block_uploading_status.backend_seq = *backend_seq;

	flock(fd, LOCK_EX);
	pwrite(fd, &block_uploading_status, sizeof(BLOCK_UPLOADING_STATUS),
		offset);
	flock(fd, LOCK_UN);

	return 0;
}

int init_progress_info(ino_t inode, long long num_block)
{
	int i;
	int ret_fd;
	int errcode;
	long long offset;
	char filename[200];
	BLOCK_UPLOADING_STATUS block_uploading_status;

	sprintf(filename, "upload_progress_inode_%ld", inode);
	ret_fd = open(filename, O_CREAT | O_RDWR);
	if (ret_fd < 0) {
		errcode = errno;
		write_log(0, "Error: Fail to open uploading progress file"
			" in %s. Code %d\n", __func__, errcode);
		return ret_fd;
	}

	offset = 0;
	memset(&block_uploading_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	for (i = 0; i < num_block; i++) {
		pwrite(ret_fd, &block_uploading_status,
			sizeof(BLOCK_UPLOADING_STATUS), offset);
		offset += sizeof(BLOCK_UPLOADING_STATUS);
	}

	return ret_fd;
}

int destroy_progress_info(int fd, ino_t inode)
{
	char filename[200];

	sprintf(filename, "upload_progress_inode_%ld", inode);
	
	close(fd);
	unlink(filename);
	return 0;
}
