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

#include "HCFSvol.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include "meta.h"

void usage(void){
	int32_t i;
	printf("\nSupported Commands: ");
	for (i = 0; i < CMD_SIZE; i++) {
		printf("%s%s", cmd_list[i].name,
		       (i + 1 == CMD_SIZE) ? "\n" : ", ");
	}
}

int32_t main(int32_t argc, char **argv)
{
	int32_t fd, size_msg, status, count, retcode, code, fsname_len, cmd_idx;

	uint32_t cmd_len, reply_len, total_recv, to_recv;
	int32_t total_entries;
	struct sockaddr_un addr;
	char buf[4096];
	DIR_ENTRY *tmp;
	char *ptr;
	ino_t tmpino;
	int64_t num_local, num_cloud, num_hybrid, retllcode;
	uint32_t uint32_ret;
	int64_t downxfersize, upxfersize;
	const char *shm_hcfs_reporter = "/dev/shm/hcfs_reporter";
	int32_t first_size, rest_size, loglevel;
	ssize_t str_size;
	char vol_mode;

	code = -1;
	if (argc < 2) {
		printf("Invalid number of arguments\n");
		usage();
		exit(-EPERM);
	}
	for (cmd_idx = 0; cmd_idx < CMD_SIZE; cmd_idx++) {
		if (strcasecmp(argv[1], cmd_list[cmd_idx].name) == 0) {
			code = cmd_list[cmd_idx].code;
			break;
		}
	}

	if (code < 0) {
		printf("Unsupported action\n");
		usage();
		exit(-ENOTSUP);
	}

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, shm_hcfs_reporter, sizeof(addr.sun_path));
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, (const struct sockaddr *) &addr, sizeof(addr));
	printf("status is %d, %s.\n", status, strerror(errno));
	switch (code) {
	case TERMINATE:
	case UNMOUNTALL:
	case RESETXFERSTAT:
	case RELOADCONFIG:
	case TRIGGERUPDATEQUOTA:
	case INITIATE_RESTORATION:
	case NOTIFY_APPLIST_CHANGE:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	/* APIs at here send result in int64_t */
	case GETQUOTA:
	case GETPINSIZE:
	case GETCACHESIZE:
	case GETMAXPINSIZE:
	case GETMAXCACHESIZE:
	case GETDIRTYCACHESIZE:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retllcode, sizeof(int64_t), 0);
		if (retllcode < 0) {
			retcode = (int32_t) retllcode;
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		} else {
			printf("Returned value is %" PRId64 "\n", retllcode);
		}
		break;
	/* APIs at here send result in uint32_t */
	case CLOUDSTAT:
	case GETSYNCSWITCH:
	case GETSYNCSTAT:
	case GETXFERSTATUS:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &uint32_ret, sizeof(uint32_t), 0);
		if (code == CLOUDSTAT)
			printf("Backend is %s\n",
			       uint32_ret ? "ONLINE" : "OFFLINE");
		else if (code == GETSYNCSWITCH)
			printf("Sync controller is switched to %s\n",
			       uint32_ret ? "ON" : "OFF");
		else if (code == GETSYNCSTAT)
			printf("Sync to backend is %s\n",
			       uint32_ret ? "AVAILABLE" : "PAUSED");
		else if (code == GETXFERSTATUS)
			printf("Xfer status is %d\n", uint32_ret);
		else if (code == CHECK_RESTORATION_STATUS)
			printf("Restoration status is %d\n", uint32_ret);
		break;
	case CHECK_RESTORATION_STATUS:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		printf("Restoration status is %d\n", retcode);
		break;
	case CREATEVOL:
		if (argc < 4) {
			printf("Usage: HCFSvol create <Vol name> %s",
			       "internal/external\n");
			exit(-EINVAL);
		}
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

		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
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
		cmd_len = sizeof(int32_t) + strlen(argv[2]) + 1;
		if (argc >= 4)
			cmd_len += (strlen(argv[3]) + 1);
		else
			cmd_len += 1;

		memcpy(buf, &fsname_len, sizeof(int32_t));
		strcpy(buf + sizeof(int32_t), argv[2]);
		if (argc >= 4)
			strcpy(buf + sizeof(int32_t) + fsname_len + 1, argv[3]);
		else
			buf[sizeof(int32_t) + fsname_len + 1] = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
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
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case GETVOLSIZE:
	case GETMETASIZE:
	case GETCLOUDSIZE:
	case UNPINDIRTYSIZE:
	case OCCUPIEDSIZE:
		if (argc >= 3) {
			cmd_len = strlen(argv[2]) + 1;
			strcpy(buf, argv[2]);
		} else {
			cmd_len = 1;
			buf[0] = 0;
		}
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retllcode, sizeof(int64_t), 0);
		if (retllcode < 0) {
			retcode = (int32_t) retllcode;
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		} else {
			printf("Returned value is %" PRId64 "\n", retllcode);
		}
		break;
	case CHECKDIRSTAT:
		tmpino = atol(argv[2]);
		cmd_len = sizeof(ino_t);
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, &tmpino, sizeof(ino_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		if (reply_len == (3 * sizeof(int64_t))) {
			size_msg = recv(fd, &num_local, sizeof(int64_t), 0);
			size_msg = recv(fd, &num_cloud, sizeof(int64_t), 0);
			size_msg = recv(fd, &num_hybrid, sizeof(int64_t), 0);
			printf("Reply len %d\n", reply_len);
			printf("Num: local %" PRId64 ", cloud %" PRId64 ", hybrid %" PRId64 "\n",
				num_local, num_cloud, num_hybrid);
		} else {
			size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		}
		break;
	case GETXFERSTAT:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &downxfersize, sizeof(int64_t), 0);
		size_msg = recv(fd, &upxfersize, sizeof(int64_t), 0);
		printf("Reply len %d\n", reply_len);
		printf("Download %" PRId64 " bytes, upload %" PRId64 " bytes\n",
				downxfersize, upxfersize);
		break;
	case CHECKLOC:
	case CHECKPIN:
		tmpino = atol(argv[2]);
		cmd_len = sizeof(ino_t);
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, &tmpino, sizeof(ino_t), 0);
		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
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
			sizeof(int32_t) + sizeof(char);
		fsname_len = strlen(argv[2]) + 1;
		memcpy(buf + 1, &fsname_len, sizeof(int32_t)); /* first byte is vol mode */
		first_size = sizeof(int32_t) + 1;
		rest_size = sizeof(buf) - first_size;

		/* [mp mode][fsname len][fsname][mp] */
		snprintf(&(buf[first_size]), rest_size, "%s", argv[2]);
		snprintf(&(buf[first_size + fsname_len]), 4092 - fsname_len,
			 "%s", argv[3]);

		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case LISTVOL:
		cmd_len = 0;
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
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
	/* Toggle functions */
	case SETSYNCSWITCH:
	case TOGGLE_USE_MINIMAL_APK:
		status = -1;
		if (argc == 3) {
			for (char *p = argv[2]; *p != '\0'; ++p)
				*p = tolower(*p);
			if (strcasecmp(argv[2], "on") == 0 ||
			    strcasecmp(argv[2], "true") == 0 ||
			    strcasecmp(argv[2], "1") == 0) {
				status = TRUE;
			} else if (strcasecmp(argv[2], "off") == 0 ||
				   strcasecmp(argv[2], "false") == 0 ||
				   strcasecmp(argv[2], "0") == 0) {
				status = FALSE;
			}
		}

		if (status == -1) {
			printf("Usage: ./HCFSvol %s  true on 1 | false off 0\n",
			       cmd_list[cmd_idx].name);
			exit(-EINVAL);
		}

		cmd_len = sizeof(status);
		size_msg = send(fd, &code, sizeof(code), 0);
		size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
		size_msg = send(fd, &status, sizeof(status), 0);

		size_msg = recv(fd, &reply_len, sizeof(reply_len), 0);
		size_msg = recv(fd, &retcode, sizeof(retcode), 0);
		break;
	case CHANGELOG:
		if (argc != 3) {
			printf("./HCFSvol changelog <log level>\n");
			exit(-EINVAL);
		}
		loglevel = atoi(argv[2]);
		cmd_len = sizeof(int32_t);
		size_msg = send(fd, &code, sizeof(code), 0);
		size_msg = send(fd, &cmd_len, sizeof(cmd_len), 0);
		size_msg = send(fd, &loglevel, sizeof(int32_t), 0);

		size_msg = recv(fd, &reply_len, sizeof(reply_len), 0);
		size_msg = recv(fd, &retcode, sizeof(retcode), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case SETNOTIFYSERVER:
		cmd_len = strlen(argv[2]) + 1;
		strncpy(buf, argv[2], sizeof(buf));
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case SETSYNCPOINT:
	case CANCELSYNCPOINT:
		if (argc >= 3) {
			cmd_len = strlen(argv[2]) + 1;
			strcpy(buf, argv[2]);
		} else {
			cmd_len = 1;
			buf[0] = 0;
		}
		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retllcode, sizeof(int64_t), 0);
		if (code == SETSYNCPOINT) {
			if (retllcode < 0) {
				retcode = (int32_t) retllcode;
				printf("Command error: Code %d, %s\n",
						-retcode, strerror(-retcode));
			} else if (retllcode == 1) {
				printf("All data is clean now\n");
			} else if (retllcode == 0) {
				printf("Successfully set sync point.\n");
			} else {
				printf("Returned value is %" PRId64 "\n",
					retllcode);
			}
		} else {
			if (retllcode < 0) {
				retcode = (int32_t) retllcode;
				printf("Command error: Code %d, %s\n",
						-retcode, strerror(-retcode));
			} else if (retllcode == 1) {
				printf("Sync point was not set.\n");
			} else if (retllcode == 0) {
				printf("Successfully cancel sync point.\n");
			} else {
				printf("Returned value is %" PRId64 "\n",
					retllcode);
			}
		}
		break;

	case SETSWIFTTOKEN:
		cmd_len = 0;
		str_size = strlen(argv[2]) + 1;
		memcpy(buf + cmd_len, &str_size, sizeof(ssize_t));

		cmd_len += sizeof(ssize_t);
		memcpy(buf + cmd_len, argv[2], str_size);

		cmd_len += str_size;
		str_size = strlen(argv[3]) + 1;
		memcpy(buf + cmd_len, &str_size, sizeof(ssize_t));

		cmd_len += sizeof(ssize_t);
		memcpy(buf + cmd_len, argv[3], str_size);

		cmd_len += str_size;

		size_msg = send(fd, &code, sizeof(uint32_t), 0);
		size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
		size_msg = send(fd, buf, (cmd_len), 0);

		size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
		size_msg = recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n", -retcode,
			       strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	default:
		break;
	}
	close(fd);
	return 0;
}
