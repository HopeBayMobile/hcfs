#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "global.h"
#define MAX_FILENAME_LEN 255

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

	if (argc < 2) {
		printf("Invalid number of arguments\n");
		exit(-EPERM);
	}
	if (strcasecmp(argv[1], "create") == 0)
		code = CREATEFS;
	else if (strcasecmp(argv[1], "delete") == 0)
		code = DELETEFS;
	else if (strcasecmp(argv[1], "check") == 0)
		code = CHECKFS;
	else if (strcasecmp(argv[1], "list") == 0)
		code = LISTFS;
	else if (strcasecmp(argv[1], "terminate") == 0)
		code = TERMINATE;
	else if (strcasecmp(argv[1], "mount") == 0)
		code = MOUNTFS;
	else if (strcasecmp(argv[1], "unmount") == 0)
		code = UNMOUNTFS;
	else if (strcasecmp(argv[1], "checkmount") == 0)
		code = CHECKMOUNT;
	else if (strcasecmp(argv[1], "unmountall") == 0)
		code = UNMOUNTALL;
	else
		code = -1;
	if (code < 0) {
		printf("Unsupported action\n");
		exit(-ENOTSUP);
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	switch (code) {
	case TERMINATE:
	case UNMOUNTALL:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case CREATEFS:
	case DELETEFS:
	case CHECKFS:
	case UNMOUNTFS:
	case CHECKMOUNT:
		cmd_len = strlen(argv[2]) + 1;
		strcpy(buf, argv[2]);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case MOUNTFS:
		cmd_len = strlen(argv[2]) + strlen(argv[3]) + 2 + sizeof(int);
		fsname_len = strlen(argv[2]) + 1;
		memcpy(buf, &fsname_len, sizeof(int));
		snprintf(&(buf[sizeof(int)]), 4092, "%s", argv[2]);
		snprintf(&(buf[sizeof(int) + fsname_len]),
					4092 - fsname_len, "%s", argv[3]);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case LISTFS:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		tmp = malloc(reply_len);
		if (tmp == NULL) {
			printf("Out of memory\n");
			break;
		}
		total_recv = 0;
		ptr = (char *) tmp;
		while (total_recv < reply_len) {
			if ((reply_len - total_recv) > 1024)
				to_recv = 1024;
			else
				to_recv = reply_len - total_recv;
			size_msg = recv(fd, &ptr[total_recv],
				to_recv, 0);
			total_recv += size_msg;
		}
		total_entries = reply_len / sizeof(DIR_ENTRY);
		for (count = 0; count < total_entries; count++)
			printf("%s\n", tmp[count].d_name);
		break;
	default:
		break;
	}
	close(fd);
	return;
}
