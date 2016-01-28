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
#include <inttypes.h>

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

int main(int argc, char **argv)
{
	int fd, status, retcode, code;
	unsigned int cmd_len, reply_len;
	struct sockaddr_un addr;
	char buf[4096];
	struct stat tempstat;
	ino_t this_inode;
	long long reserved_size = 123;
	unsigned int num_inodes = 0;
	ino_t inode_list[1000];
	int i;

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

	for (i = 2; i < argc; i++) {
		if (stat(argv[i], &tempstat) < 0) {
			printf("%s does not exist\n", argv[i]);
			exit(-ENOENT);
		} else {
			this_inode = tempstat.st_ino;
		}
		inode_list[num_inodes] = this_inode;
		num_inodes++;

		printf("%s - inode %"PRIu64" - num_inode %d\n", argv[i], (uint64_t)this_inode, num_inodes);
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	switch (code) {
	case PIN:

		cmd_len = sizeof(long long) + sizeof(unsigned int) + 
			num_inodes * sizeof(ino_t);
		memcpy(buf, &reserved_size, sizeof(long long)); /* Pre-allocating pinned size (be allowed to be 0) */
		memcpy(buf + sizeof(long long), &num_inodes, /* # of inodes */
			sizeof(unsigned int));
		memcpy(buf + sizeof(long long) + sizeof(unsigned int), /* inode array */
			inode_list, sizeof(ino_t) * num_inodes);
		send(fd, &code, sizeof(unsigned int), 0);
		send(fd, &cmd_len, sizeof(unsigned int), 0);
		send(fd, buf, cmd_len, 0);

		recv(fd, &reply_len, sizeof(unsigned int), 0);
		recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case UNPIN:
		cmd_len = sizeof(unsigned int) + num_inodes * sizeof(ino_t);
		memcpy(buf, &num_inodes, sizeof(unsigned int)); /* # of inodes */
		memcpy(buf + sizeof(unsigned int), /* inode array */
			inode_list, sizeof(ino_t) * num_inodes);

		send(fd, &code, sizeof(unsigned int), 0);
		send(fd, &cmd_len, sizeof(unsigned int), 0);
		send(fd, buf, cmd_len, 0);

		recv(fd, &reply_len, sizeof(unsigned int), 0);
		recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	default:
		break;
	}
	close(fd);
	return 0;
}
