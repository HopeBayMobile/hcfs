#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "global.h"
#define MAX_FILENAME_LEN 255
#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#endif

typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;

void main(int argc, char **argv)
{
	int fd,size_msg, status, count, retcode, code, fsname_len;
	unsigned int cmd_len, reply_len, total_recv, to_recv;
	int total_entries;
	struct sockaddr_un addr;
	char buf[4096];
	DIR_ENTRY *tmp;
	char *ptr;
	struct stat tempstat;
	ino_t this_inode;
	long long reserved_size = 0;
	unsigned int num_inodes = 1;

	if (argc < 2) {
		printf("Invalid number of arguments\n");
		exit(-EPERM);
	}
	if (strcasecmp(argv[1], "pin") == 0)
		code = PIN;
	else if (strcasecmp(argv[1], "unpin") == 0)
		code = UNPIN;
	else
		code = -1;
	if (code < 0) {
		printf("Unsupported action\n");
		exit(-ENOTSUP);
	}

	if (stat(argv[2], &tempstat) < 0) {
		printf("%s does not exist\n", argv[2]);
		exit(-ENOENT);
	} else {
		this_inode = tempstat.st_ino;
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	switch (code) {
	case PIN:

		cmd_len = sizeof(long long);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &reserved_size, sizeof(long long), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0) {
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
			break;
		}

		size_msg = send(fd, &num_inodes, sizeof(unsigned int), 0);
		size_msg = send(fd, &this_inode, sizeof(ino_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else if (retcode == 1)
			printf("It had been %s. Do not do that again.\n",
				code == PIN ? "pinned" : "unpinned");
		else
			printf("Returned value is %d\n", retcode);
		break;
	case UNPIN:
		cmd_len = sizeof(ino_t);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &this_inode, sizeof(ino_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else if (retcode == 1)
			printf("It had been %s. Do not do that again.\n",
				code == PIN ? "pinned" : "unpinned");
		else
			printf("Returned value is %d\n", retcode);
		break;
	default:
		break;
	}
	close(fd);
	return;
}
