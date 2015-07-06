#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void main(int argc, char **argv)
{
	int fd,size_msg, status;
	unsigned int code, cmd_len;
	struct sockaddr_un addr;
	char buf[4096];

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	printf("%s\n", argv[1]);
	if (strcasecmp(argv[1], "test") == 0) {
		code = 2;
		cmd_len = 0;
	} else {
		printf("command not supported\n");
		code = -1;
	}

	if (code >= 0) {
		size_msg=send(fd, &code, sizeof(unsigned int), 0);
		size_msg=send(fd, &cmd_len, sizeof(unsigned int), 0);
	}
	close(fd);
	return;
}
