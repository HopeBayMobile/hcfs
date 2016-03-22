/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: HCFSvol.c
* Abstract: The c source file for CLI utilities
*
* Revision History
* 2015/11/9 Jiahong adding this header
*
**************************************************************************/

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
#include "HCFSvol.h"

int main(int argc, char **argv)
{
	int fd, size_msg, status, count, retcode, code, fsname_len;
	unsigned int cmd_len, reply_len, total_recv, to_recv;
	int total_entries;
	struct sockaddr_un addr;
	char buf[4096];
	DIR_ENTRY *tmp;
	char *ptr;
	ino_t tmpino;
	long long num_local, num_cloud, num_hybrid, retllcode;
	uint32_t uint32_ret;
	long long downxfersize, upxfersize;
	char shm_hcfs_reporter[] = "/dev/shm/hcfs_reporter";
	int first_size, rest_size;
	char vol_mode;

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
	else if (strcasecmp(argv[1], "volsize") == 0)
		code = GETVOLSIZE;
	else if (strcasecmp(argv[1], "cloudsize") == 0)
		code = GETCLOUDSIZE;
	else if (strcasecmp(argv[1], "pinsize") == 0)
		code = GETPINSIZE;
	else if (strcasecmp(argv[1], "cachesize") == 0)
		code = GETCACHESIZE;
	else if (strcasecmp(argv[1], "location") == 0)
		code = CHECKLOC;
	else if (strcasecmp(argv[1], "ispin") == 0)
		code = CHECKPIN;
	else if (strcasecmp(argv[1], "maxpinsize") == 0)
		code = GETMAXPINSIZE;
	else if (strcasecmp(argv[1], "maxcachesize") == 0)
		code = GETMAXCACHESIZE;
	else if (strcasecmp(argv[1], "dirtysize") == 0)
		code = GETDIRTYCACHESIZE;
	else if (strcasecmp(argv[1], "getxfer") == 0)
		code = GETXFERSTAT;
	else if (strcasecmp(argv[1], "resetxfer") == 0)
		code = RESETXFERSTAT;
	else if (strcasecmp(argv[1], "cloudstat") == 0)
		code = CLOUDSTAT;
	else if (strcasecmp(argv[1], "setsyncswitch") == 0)
		code = SETSYNCSWITCH;
	else if (strcasecmp(argv[1], "getsyncswitch") == 0)
		code = GETSYNCSWITCH;
	else if (strcasecmp(argv[1], "getsyncstat") == 0)
		code = GETSYNCSTAT;
	else if (strcasecmp(argv[1], "reloadconfig") == 0)
		code = RELOADCONFIG;
	else
		code = -1;
	if (code < 0) {
		printf("Unsupported action\n");
		exit(-ENOTSUP);
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, shm_hcfs_reporter, sizeof(addr.sun_path));
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, (const struct sockaddr *) &addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	switch (code) {
	case TERMINATE:
	case UNMOUNTALL:
	case RESETXFERSTAT:
	case RELOADCONFIG:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	/* APIs at here send result in long long */
	case GETPINSIZE:
	case GETCACHESIZE:
	case GETMAXPINSIZE:
	case GETMAXCACHESIZE:
	case GETDIRTYCACHESIZE:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retllcode, sizeof(long long), 0);
		if (retllcode < 0) {
			retcode = (int) retllcode;
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		} else {
			printf("Returned value is %lld\n", retllcode);
		}
		break;
	/* APIs at here send result in uint32_t */
	case CLOUDSTAT:
	case GETSYNCSWITCH:
	case GETSYNCSTAT:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &uint32_ret, sizeof(uint32_t), 0);
		if (code == CLOUDSTAT)
			printf("Backend is %s\n",
			       uint32_ret ? "ONLINE (1)" : "OFFLINE (0)");
		else if (code == GETSYNCSWITCH)
			printf("Sync switch is %s\n",
			       uint32_ret ? "ON(1)" : "OFF(0)");
		else if (code == GETSYNCSTAT)
			printf("Sync process is %s\n",
			       uint32_ret ? "RUNNING(1)" : "PAUSED(0)");
		break;
	case CREATEVOL:
#ifdef _ANDROID_ENV_
		cmd_len = strlen(argv[2]) + 2;
		strncpy(buf, argv[2], sizeof(buf));
		if (strcasecmp(argv[3], "external") == 0) {
			buf[strlen(argv[2]) + 1] = ANDROID_EXTERNAL;
		} else if (strcasecmp(argv[3], "internal") == 0) {
			buf[strlen(argv[2]) + 1] = ANDROID_INTERNAL;
		} else if (strcasecmp(argv[3], "multiexternal") == 0) {
			buf[strlen(argv[2]) + 1] = ANDROID_MULTIEXTERNAL;
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
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
#endif
	case UNMOUNTVOL:
		memset(buf, 0, sizeof(buf));
		fsname_len = strlen(argv[2]);
		cmd_len = sizeof(int) + strlen(argv[2]) + 1;
		if (argc >= 4)
			cmd_len += (strlen(argv[3]) + 1);
		else
			cmd_len += 1;

		memcpy(buf, &fsname_len, sizeof(int));
		strcpy(buf + sizeof(int), argv[2]);
		if (argc >= 4)
			strcpy(buf + sizeof(int) + fsname_len + 1, argv[3]);
		else
			buf[sizeof(int) + fsname_len + 1] = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case DELETEVOL:
	case CHECKVOL:
	case CHECKMOUNT:
		cmd_len = strlen(argv[2]) + 1;
		strncpy(buf, argv[2], sizeof(buf));
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case GETVOLSIZE:
	case GETCLOUDSIZE:
		if (argc >= 3) {
			cmd_len = strlen(argv[2]) + 1;
			strcpy(buf, argv[2]);
		} else {
			cmd_len = 1;
			buf[0] = 0;
		}
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retllcode, sizeof(long long), 0);
		if (retllcode < 0) {
			retcode = (int) retllcode;
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		} else {
			printf("Returned value is %lld\n", retllcode);
		}
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
	case GETXFERSTAT:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &downxfersize, sizeof(long long), 0);
		size_msg = recv(fd, &upxfersize, sizeof(long long), 0);
		printf("Reply len %d\n", reply_len);
		printf("Download %lld bytes, upload %lld bytes\n",
				downxfersize, upxfersize);
		break;
	case CHECKLOC:
	case CHECKPIN:
		tmpino = atol(argv[2]);
		cmd_len = sizeof(ino_t);
		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &tmpino, sizeof(ino_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;

	case MOUNTVOL:
		if (argc == 5) {
			if (!strcmp("default", argv[4]))
				vol_mode = MP_DEFAULT;
			else if (!strcmp("read", argv[4]))
				vol_mode = MP_READ;
			else if (!strcmp("write", argv[4]))
				vol_mode = MP_WRITE;
			else {
				printf("Command error: %s is not supported\n",
						argv[4]);
				break;
			}
		} else {
			vol_mode = MP_DEFAULT;
		}
		buf[0] = vol_mode;
		cmd_len = strlen(argv[2]) + strlen(argv[3]) + 2 +
			sizeof(int) + sizeof(char);
		fsname_len = strlen(argv[2]) + 1;
		memcpy(buf + 1, &fsname_len, sizeof(int)); /* first byte is vol mode */
		first_size = sizeof(int) + 1;
		rest_size = sizeof(buf) - first_size;

		/* [mp mode][fsname len][fsname][mp] */
		snprintf(&(buf[first_size]), rest_size, "%s", argv[2]);
		snprintf(&(buf[first_size + fsname_len]), 4092 - fsname_len,
			 "%s", argv[3]);

		size_msg = send(fd, &code, sizeof(unsigned int), 0);
		size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
		size_msg = recv(fd, &retcode, sizeof(int), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
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
		ptr = (char *)tmp;
		while (total_recv < reply_len) {
			if ((reply_len - total_recv) > 1024)
				to_recv = 1024;
			else
				to_recv = reply_len - total_recv;
			size_msg = recv(fd, &ptr[total_recv], to_recv, 0);
			total_recv += size_msg;
		}
		total_entries = reply_len / sizeof(DIR_ENTRY);
#ifdef _ANDROID_ENV_
		for (count = 0; count < total_entries; count++) {
			if (tmp[count].d_type == ANDROID_EXTERNAL)
				printf("%s\tExternal\n", tmp[count].d_name);
			else if (tmp[count].d_type == ANDROID_MULTIEXTERNAL)
				printf("%s\tMultiExternal\n", tmp[count].d_name);
			else
				printf("%s\tInternal\n", tmp[count].d_name);
		}
#else
		for (count = 0; count < total_entries; count++)
			printf("%s\n", tmp[count].d_name);
#endif
		break;
	case SETSYNCSWITCH:
		if (strcasecmp(argv[2], "on") == 0) {
			status = TRUE;
		} else if (strcasecmp(argv[2], "off") == 0) {
			status = FALSE;
		} else {
			printf("Unsupported switch arg: %s, should be on/off\n", argv[2]);
			exit(-ENOTSUP);
		}

		cmd_len = sizeof(status);
		size_msg = send(fd, &code, sizeof(code), 0);
		size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
		size_msg = send(fd, &status, sizeof(status), 0);

		size_msg = recv(fd, &reply_len, sizeof(reply_len), 0);
		size_msg = recv(fd, &retcode, sizeof(retcode), 0);
		break;
	default:
		break;
	}
	close(fd);
	return 0;
}
