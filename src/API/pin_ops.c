/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: pin_ops.c
* Abstract: This c source file for pin related operations.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include "pin_ops.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "global.h"
#include "socket_util.h"


#define DATA_PREFIX "/data/data"
#define APP_PREFIX "/data/app"
#define EXTERNAL_PREFIX "/storage/emulated"


/************************************************************************
 * *
 * * Function name: _walk_folder
 * *        Inputs: char *pathname, int64_t *total_size
 * *       Summary: Traverse a given folder (pathname) and calculate total
 * *		    size of it.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
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
	return 0;
}

/************************************************************************
 * *
 * * Function name: _validate_hcfs_path
 * *        Inputs: char *pathname
 * *       Summary: To check if this (pathname) is located in hcfs mountpoint.
 * *
 * *  Return value: 0 if pathname is in hcfs mountpoint.
 * *		    Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t _validate_hcfs_path(char *pathname)
{
	int32_t ret_code = -ENOENT;
	char *resolved_path = NULL;

	resolved_path = realpath(pathname, NULL);

	if (resolved_path == NULL)
		return ret_code;

	/* Following are mountpoints used in Android system. */
	if (!strncmp(resolved_path, DATA_PREFIX, sizeof(DATA_PREFIX) - 1)
	    || !strncmp(resolved_path, APP_PREFIX, sizeof(APP_PREFIX) - 1)
	    || !strncmp(resolved_path, EXTERNAL_PREFIX, sizeof(EXTERNAL_PREFIX) - 1))
		ret_code = 0;

	free(resolved_path);
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: _get_path_stat
 * *        Inputs: char *pathname, ino_t *inode, int64_t *total_size
 * *       Summary: To get the inode info and size(optional) of (pathname).
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
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

/************************************************************************
 * *
 * * Function name: _pin_by_inode
 * *        Inputs: const int64_t reserved_size, const uint32_t num_inodes
 * *                const char *inode_array
 * *       Summary: To pin all inodes in (inode_array)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t _pin_by_inode(const int64_t reserved_size, const uint32_t num_inodes,
		      const char *inode_array)
{

	int32_t fd, count, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;

}

/************************************************************************
 * *
 * * Function name: pin_by_path
 * *        Inputs: char *buf, uint32_t arg_len
 * *       Summary: To pin a given pathname.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t pin_by_path(char *buf, uint32_t arg_len)
{
	int32_t ret_code;
	uint32_t msg_len, num_inodes;
	int64_t total_size = 0;
	char inode_array[sizeof(ino_t) * 5];
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

/************************************************************************
 * *
 * * Function name: _unpin_by_inode
 * *        Inputs: const uint32_t num_inodes, const char *inode_array
 * *       Summary: To unpin all inodes in (inode_array)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t _unpin_by_inode(const uint32_t num_inodes, const char *inode_array)
{
	int32_t fd, count, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: unpin_by_path
 * *        Inputs: char *buf, uint32_t arg_len
 * *       Summary: To unpin a given pathname.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
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

/************************************************************************
 * *
 * * Function name: check_pin_status
 * *        Inputs: char *buf, uint32_t arg_len
 * *       Summary: To check if a pathname is pinned or not.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t check_pin_status(char *buf, uint32_t arg_len)
{

	int32_t fd, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	ino_t tmp_inode;

	ret_code = _get_path_stat(buf, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKPIN;
	cmd_len = sizeof(ino_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &tmp_inode, sizeof(ino_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: check_dir_status
 * *        Inputs: char *buf, uint32_t arg_len
 * *		    int64_t *num_local, int64_t *num_cloud,
 * *		    int64_t *num_hybrid
 * *       Summary: To get locations statistics of files in a folder.
 * *		    (local; hybrid; cloud)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t check_dir_status(char *buf, uint32_t arg_len,
			 int64_t *num_local, int64_t *num_cloud,
			 int64_t *num_hybrid)
{

	int32_t fd, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	ino_t tmp_inode;

	ret_code = _get_path_stat(buf, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKDIRSTAT;
	cmd_len = sizeof(ino_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &tmp_inode, sizeof(ino_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len > sizeof(uint32_t)) {
		recv(fd, num_local, sizeof(int64_t), 0);
		recv(fd, num_cloud, sizeof(int64_t), 0);
		recv(fd, num_hybrid, sizeof(int64_t), 0);
	} else {
		recv(fd, &ret_code, sizeof(int32_t), 0);
	}

	close(fd);
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: check_file_loc
 * *        Inputs: char *buf, uint32_t arg_len
 * *       Summary: To get the location info of a file.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t check_file_loc(char *buf, uint32_t arg_len)
{

	int32_t fd, count, ret_code;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;
	ino_t tmp_inode;

	ret_code = _get_path_stat(buf, &tmp_inode, NULL);
	if (ret_code < 0)
		return ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKLOC;
	cmd_len = sizeof(ino_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &tmp_inode, sizeof(ino_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);
	return ret_code;
}
