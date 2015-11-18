#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "utils.h"

#include "global.h"

int get_hcfs_socket_conn()
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
