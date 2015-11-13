#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
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
	ino_t tmpino;
	long long num_local, num_cloud, num_hybrid;

	if (argc < 2) {
		printf("Invalid number of arguments\n");
		exit(-EPERM);
	}
	if (strcasecmp(argv[1], "create") == 0) {
		if (argc < 4) {
			printf("Usage: HCFSvol create <Vol name> internal/external\n");
			exit(-EINVAL);
		}
		code = CREATEVOL;
	} else if (strcasecmp(argv[1], "delete") == 0)
		code = DELETEVOL;
	else if (strcasecmp(argv[1], "check") == 0)
		code = CHECKVOL;
	else if (strcasecmp(argv[1], "list") == 0)
		code = LISTVOL;
	else if (strcasecmp(argv[1], "terminate") == 0)
		code = TERMINATE;
	else if (strcasecmp(argv[1], "mount") == 0)
		code = MOUNTVOL;
	else if (strcasecmp(argv[1], "unmount") == 0)
		code = UNMOUNTVOL;
	else if (strcasecmp(argv[1], "checkmount") == 0)
		code = CHECKMOUNT;
	else if (strcasecmp(argv[1], "unmountall") == 0)
		code = UNMOUNTALL;
	else if (strcasecmp(argv[1], "checknode") == 0)
		code = CHECKDIRSTAT;
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
	case CREATEVOL:
#ifdef _ANDROID_ENV_
		cmd_len = strlen(argv[2]) + 2;
		strcpy(buf, argv[2]);
		if (strcasecmp(argv[3], "external") == 0) {
			buf[strlen(argv[2]) + 1] = ANDROID_EXTERNAL;
		} else if (strcasecmp(argv[3], "internal") == 0) {
			buf[strlen(argv[2]) + 1] = ANDROID_INTERNAL;
		} else {
			printf("Unsupported storage type\n");
			exit(-ENOTSUP);
		}

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
#endif
	case DELETEVOL:
	case CHECKVOL:
	case UNMOUNTVOL:
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
	case CHECKDIRSTAT:
		tmpino = atol(argv[2]);
		cmd_len = sizeof(ino_t);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &tmpino, sizeof(ino_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		if (reply_len == (3 * sizeof(long long))) {
			size_msg = recv(fd, &num_local, sizeof(long long), 0);
			size_msg = recv(fd, &num_cloud, sizeof(long long), 0);
			size_msg = recv(fd, &num_hybrid, sizeof(long long), 0);
			printf("Reply len %d\n", reply_len);
			printf("Num: local %lld, cloud %lld, hybrid %lld\n",
				num_local, num_cloud, num_hybrid);
		} else {
			size_msg = recv(fd, &retcode, sizeof(int), 0);
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		}
		break;
	case MOUNTVOL:
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
	case LISTVOL:
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
#ifdef _ANDROID_ENV_
		for (count = 0; count < total_entries; count++) {
			if (tmp[count].d_type == ANDROID_EXTERNAL)
				printf("%s\tExternal\n", tmp[count].d_name);
			else
				printf("%s\tInternal\n", tmp[count].d_name);
		}
#else
		for (count = 0; count < total_entries; count++)
			printf("%s\n", tmp[count].d_name);
#endif
		break;
	default:
		break;
	}
	close(fd);
	return;
}
