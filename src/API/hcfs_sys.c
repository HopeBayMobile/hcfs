#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

#include "hcfs_sys.h"

#include "global.h"
#include "utils.h"


int reset_xfer_usage()
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RESETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);

	return ret_code;
}
