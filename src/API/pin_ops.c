#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "pin_ops.h"

#include "global.h"
#include "utils.h"


#define DATA_PREFIX "/data/data"
#define APP_PREFIX "/data/app"
#define EXTERNAL_PREFIX "/storage/emulated"

int32_t _walk_folder(char *pathname, int64_t *total_size)
{

	char tmp_path[400];
	struct stat stat_buf;
	DIR *dir;
	struct dirent *dent;

	if (!(dir = opendir(pathname)))
		return -1;

	if (!(dent = readdir(dir)))
		return -1;

	do {
		if (dent->d_type == DT_DIR) {
			if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
				continue;
			snprintf(tmp_path, sizeof(tmp_path), "%s/%s", pathname, dent->d_name);
			_walk_folder(tmp_path, total_size);
		} else if (dent->d_type == DT_REG) {
			snprintf(tmp_path, sizeof(tmp_path), "%s/%s", pathname, dent->d_name);
			stat(tmp_path, &stat_buf);
			*total_size += stat_buf.st_size;
		}
	} while (dent = readdir(dir));

	closedir(dir);
}

int32_t _validate_hcfs_path(char *pathname)
{
	int32_t ret_code = -ENOENT;
	char *resolved_path = NULL;

	resolved_path = realpath(pathname, NULL);

	if (resolved_path == NULL)
		return ret_code;

	/* Check if this pathname is located in hcfs mountpoints */
	if (!strncmp(resolved_path, DATA_PREFIX, sizeof(DATA_PREFIX) - 1)
	    || !strncmp(resolved_path, APP_PREFIX, sizeof(APP_PREFIX) - 1)
	    || !strncmp(resolved_path, EXTERNAL_PREFIX, sizeof(EXTERNAL_PREFIX) - 1))
		ret_code = 0;

	free(resolved_path);
	return ret_code;
}

int32_t _get_path_stat(char *pathname, ino_t *inode, int64_t *total_size)
{

	int32_t ret_code;
	struct stat stat_buf;

	ret_code = _validate_hcfs_path(pathname);
	if (ret_code < 0)
		return ret_code;

	ret_code = stat(pathname, &stat_buf);
	if (ret_code < 0)
		return -errno;

	if (S_ISREG(stat_buf.st_mode) || S_ISDIR(stat_buf.st_mode)) {
		*inode = stat_buf.st_ino;
	} else {
		return -1;
	}

	return 0;

}

int32_t _pin_by_inode(const int64_t reserved_size, const uint32_t num_inodes,
		  const char *inode_array)
{

	int32_t fd, size_msg, count, ret_code;
	int32_t buf_idx;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	char buf[1000];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = PIN;
	cmd_len = sizeof(int64_t) + sizeof(uint32_t) +
		num_inodes * sizeof(ino_t);

	buf_idx = 0;
	memcpy(&(buf[buf_idx]), &reserved_size, sizeof(int64_t));

	buf_idx += sizeof(int64_t);
	memcpy(&(buf[buf_idx]), &num_inodes, sizeof(uint32_t));

	buf_idx += sizeof(uint32_t);
	memcpy(&(buf[buf_idx]), inode_array, sizeof(ino_t)*num_inodes);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	printf("Pin result - %d\n", ret_code);

	close(fd);
	return ret_code;

}

int32_t pin_by_path(char *buf, uint32_t arg_len)
{

	int32_t ret_code;
	uint32_t msg_len, num_inodes;
	int64_t total_size = 0;
	char inode_array[1600];
	char path[400];
	ssize_t path_len;
	ino_t tmp_inode;

	num_inodes = 0;
	msg_len = 0;

	while (msg_len < arg_len) {
		path_len = 0;
		memcpy(&path_len, &(buf[msg_len]), sizeof(ssize_t));
		msg_len += sizeof(ssize_t);

		memcpy(path, &(buf[msg_len]), path_len);
		msg_len += path_len;

		ret_code = _get_path_stat(path, &tmp_inode, NULL);
		if (ret_code < 0)
			return ret_code;

		memcpy(&(inode_array[num_inodes*sizeof(ino_t)]),
				&tmp_inode, sizeof(ino_t));

		num_inodes += 1;
	}

	ret_code = _pin_by_inode(total_size, num_inodes, inode_array);

	return ret_code;

}

int32_t _unpin_by_inode(const uint32_t num_inodes, const char *inode_array)
{

	int32_t fd, size_msg, count, ret_code;
	int32_t buf_idx;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	char buf[1000];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = UNPIN;
	cmd_len = sizeof(int64_t) + sizeof(uint32_t) +
		num_inodes * sizeof(ino_t);

	buf_idx = 0;
	memcpy(&(buf[buf_idx]), &num_inodes, sizeof(uint32_t));

	buf_idx += sizeof(uint32_t);
	memcpy(&(buf[buf_idx]), inode_array, sizeof(ino_t)*num_inodes);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	printf("Pin result - %d\n", ret_code);

	close(fd);
	return ret_code;
}

int32_t unpin_by_path(char *buf, uint32_t arg_len)
{

	int32_t ret_code;
	uint32_t msg_len, num_inodes;
	char inode_array[1600];
	char path[400];
	ssize_t path_len;
	ino_t tmp_inode;

	num_inodes = 0;
	msg_len = 0;

	while (msg_len < arg_len) {
		path_len = 0;
		memcpy(&path_len, &(buf[msg_len]), sizeof(ssize_t));
		msg_len += sizeof(ssize_t);

		memcpy(path, &(buf[msg_len]), path_len);
		msg_len += path_len;

		ret_code = _get_path_stat(path, &tmp_inode, NULL);
		if (ret_code < 0)
			return ret_code;

		memcpy(&(inode_array[num_inodes*sizeof(ino_t)]),
				&tmp_inode, sizeof(ino_t));

		num_inodes += 1;
	}

	ret_code = _unpin_by_inode(num_inodes, inode_array);

	return ret_code;
}

int32_t check_pin_status(char *buf, uint32_t arg_len)
{

	int32_t fd, size_msg, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;


	memcpy(path, &(buf[0]), arg_len);
	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKPIN;
	cmd_len = sizeof(ino_t);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;
}

int32_t check_dir_status(char *buf, uint32_t arg_len,
		     int64_t *num_local, int64_t *num_cloud,
		     int64_t *num_hybrid)
{

	int32_t fd, size_msg, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;


	memcpy(path, &(buf[0]), arg_len);
	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKDIRSTAT;
	cmd_len = sizeof(ino_t);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len > sizeof(uint32_t)) {
		size_msg = recv(fd, num_local, sizeof(int64_t), 0);
		size_msg = recv(fd, num_cloud, sizeof(int64_t), 0);
		size_msg = recv(fd, num_hybrid, sizeof(int64_t), 0);
	} else {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
	}

	close(fd);
	return ret_code;
}

int32_t check_file_loc(char *buf, uint32_t arg_len)
{

	int32_t fd, size_msg, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;


	memcpy(path, &(buf[0]), arg_len);
	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKLOC;
	cmd_len = sizeof(ino_t);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;
}
