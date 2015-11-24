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


int _walk_folder(char *pathname, long long *total_size)
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

int _get_path_stat(char *pathname, ino_t *inode, long long *total_size)
{

	int ret_code;
	struct stat stat_buf;

	ret_code = stat(pathname, &stat_buf);
	if (ret_code < 0)
		return ret_code;

	if (S_ISREG(stat_buf.st_mode)) {
		*inode = stat_buf.st_ino;
		if (total_size != NULL)
			*total_size += stat_buf.st_size;
	} else if (S_ISDIR(stat_buf.st_mode)) {
		*inode = stat_buf.st_ino;
		if (total_size != NULL)
			_walk_folder(pathname, total_size);
	} else
		return -1;

	return 0;

}

int _pin_by_inode(const long long reserved_size, const unsigned int num_inodes,
		  const char *inode_array)
{

	int fd, size_msg, count, ret_code;
	int buf_idx;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char buf[1000];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = PIN;
	cmd_len = sizeof(long long) + sizeof(unsigned int) +
		num_inodes * sizeof(ino_t);

	buf_idx = 0;
	memcpy(&(buf[buf_idx]), &reserved_size, sizeof(long long));

	buf_idx += sizeof(long long);
	memcpy(&(buf[buf_idx]), &num_inodes, sizeof(unsigned int));

	buf_idx += sizeof(unsigned int);
	memcpy(&(buf[buf_idx]), inode_array, sizeof(ino_t)*num_inodes);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	printf("Pin result - %d\n", ret_code);

	close(fd);
	return ret_code;

}

int pin_by_path(char *buf, unsigned int arg_len)
{

	int ret_code;
	unsigned int msg_len, num_inodes;
	long long total_size = 0;
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

		ret_code = _get_path_stat(path, &tmp_inode, &total_size);
		if (ret_code < 0)
			return ret_code;

		memcpy(&(inode_array[num_inodes*sizeof(ino_t)]),
				&tmp_inode, sizeof(ino_t));

		num_inodes += 1;
	}

	ret_code = _pin_by_inode(total_size, num_inodes, inode_array);

	return ret_code;

}

int _unpin_by_inode(const unsigned int num_inodes, const char *inode_array)
{

	int fd, size_msg, count, ret_code;
	int buf_idx;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char buf[1000];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = UNPIN;
	cmd_len = sizeof(long long) + sizeof(unsigned int) +
		num_inodes * sizeof(ino_t);

	buf_idx = 0;
	memcpy(&(buf[buf_idx]), &num_inodes, sizeof(unsigned int));

	buf_idx += sizeof(unsigned int);
	memcpy(&(buf[buf_idx]), inode_array, sizeof(ino_t)*num_inodes);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	printf("Pin result - %d\n", ret_code);

	close(fd);
	return ret_code;
}

int unpin_by_path(char *buf, unsigned int arg_len)
{

	int ret_code;
	unsigned int msg_len, num_inodes;
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

int check_pin_status(char *buf, unsigned int arg_len)
{

	int fd, size_msg, count, ret_code;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKPIN;
	cmd_len = sizeof(ino_t);

	memcpy(path, &(buf[0]), arg_len);

	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0) {
		close(fd);
		return ret_code;
	}

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);
	return ret_code;
}

int check_dir_status(char *buf, unsigned int arg_len,
			long long *num_local, long long *num_cloud,
			long long *num_hybrid)
{

	int fd, size_msg, count, ret_code;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKDIRSTAT;
	cmd_len = sizeof(ino_t);

	memcpy(path, &(buf[0]), arg_len);

	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0) {
		close(fd);
		return ret_code;
	}

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len > sizeof(unsigned int)) {
		size_msg = recv(fd, num_local, sizeof(long long), 0);
		size_msg = recv(fd, num_cloud, sizeof(long long), 0);
		size_msg = recv(fd, num_hybrid, sizeof(long long), 0);
	} else {
		size_msg = recv(fd, &ret_code, sizeof(int), 0);
	}

	close(fd);
	return ret_code;
}

int check_file_loc(char *buf, unsigned int arg_len)
{

	int fd, size_msg, count, ret_code;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char path[400];
	ino_t tmp_inode;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKLOC;
	cmd_len = sizeof(ino_t);

	memcpy(path, &(buf[0]), arg_len);

	ret_code = _get_path_stat(path, &tmp_inode, NULL);
	if (ret_code < 0) {
		close(fd);
		return ret_code;
	}

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, &tmp_inode, sizeof(ino_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);
	return ret_code;
}
