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


int _socket_conn()
{
	int fd, status;
	struct sockaddr_un addr;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	if (status < 0)
		return -errno;

	return fd;
}

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
		*total_size += stat_buf.st_size;
		*inode = stat_buf.st_ino;
	} else if (S_ISDIR(stat_buf.st_mode)) {
		_walk_folder(pathname, total_size);
		*inode = stat_buf.st_ino;
	} else
		return -99999;

	return 0;

}

int _pin_by_inode(const long long reserved_size, const unsigned int num_inodes,
		  const char *inode_array)
{

	int fd, code, status, size_msg, count, ret_code;
	int buf_idx;
	unsigned int cmd_len, reply_len, total_recv, to_recv;
	char buf[1000];

	fd = _socket_conn();
	if (fd < 0)
		return -1;

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

	printf("Recv\n");

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	printf("Pin result - %d\n", ret_code);

	close(fd);
	return ret_code;

}

int pin_by_path(char *buf, ssize_t arg_len)
{

	int ret_code;
	unsigned int num_inodes = 1;
	long long total_size = 0;
	char inode_array[300];
	char path[400];
	ino_t tmp_inode;

	memcpy(path, buf, arg_len);
	printf("%s\n", path);
	printf("%d\n", arg_len);
	ret_code = _get_path_stat(path, &tmp_inode, &total_size);
	if (ret_code < 0)
		return -1;

	memcpy(inode_array, &tmp_inode, sizeof(ino_t));
	ret_code = _pin_by_inode(total_size, num_inodes, inode_array);

	return ret_code;

}



