#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MOUNTFS 5

void main(int argc, char **argv)
{
	int fd,size_msg, status, FSlen;
	unsigned int code, cmd_len, reply_len, retcode;
	struct sockaddr_un addr;
	char buf[4096];

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	printf("%s\n", argv[1]);
	printf("%s\n", argv[2]);
	code = MOUNTFS;
	cmd_len = sizeof(int) + strlen(argv[1]) + strlen(argv[2]) + 2;
	FSlen = strlen(argv[1]) + 1;
	memcpy(buf, &FSlen, sizeof(int));
	strcpy(&(buf[sizeof(int)]), argv[1]);
	strcpy(&(buf[sizeof(int) + FSlen]), argv[2]);
	if (code >= 0) {
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);
	}
	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &retcode, sizeof(unsigned int), 0);
	close(fd);
	printf("Return code is %d\n", retcode);
	return;
}
