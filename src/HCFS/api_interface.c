/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.c
* Abstract: The c source file for Defining API for controlling / monitoring
*
* Revision History
* 2015/6/10 Jiahong created this file, and moved prototype here.
*
**************************************************************************/

#include "api_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/file.h>

#include "macro.h"
#include "global.h"
#include "params.h"
#include "FS_manager.h"
#include "mount_manager.h"
#include "fuseop.h"
#include "super_block.h"
#include "dir_statistics.h"
#include "file_present.h"
#include "utils.h"

extern SYSTEM_CONF_STRUCT system_config;

/* TODO: Error handling if the socket path is already occupied and cannot
be deleted */
/* TODO: Perhaps should decrease the number of threads if loading not heavy */

/************************************************************************
*
* Function name: init_api_interface
*        Inputs: None
*       Summary: Initialize API server. The number of threads for accepting
*                incoming requests is specified in the header file.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_api_interface(void)
{
	int ret, errcode, count;
	int *val;
	int sock_flag;

	write_log(10, "Starting API interface");
	if (access(SOCK_PATH, F_OK) == 0)
		unlink(SOCK_PATH);
	api_server = malloc(sizeof(API_SERVER_TYPE));
	if (api_server == NULL) {
		errcode = ENOMEM;
		write_log(0, "Socket error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	sem_init(&(api_server->job_lock), 0, 1);
	api_server->num_threads = INIT_API_THREADS;
	api_server->last_update = 0;
	memset(api_server->job_count, 0, sizeof(int) * PROCESS_WINDOW);
	memset(api_server->job_totaltime, 0, sizeof(float) * PROCESS_WINDOW);
	memset(api_server->local_thread, 0,
			sizeof(pthread_t) * MAX_API_THREADS);

	api_server->sock.addr.sun_family = AF_UNIX;
	strcpy(api_server->sock.addr.sun_path, SOCK_PATH);
	api_server->sock.fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (api_server->sock.fd < 0) {
		errcode = errno;
		write_log(0, "Socket error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	ret = bind(api_server->sock.fd,
		(struct sockaddr *) &(api_server->sock.addr),
		sizeof(struct sockaddr_un));

	if (ret < 0) {
		errcode = errno;
		write_log(0, "Bind error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	sock_flag = fcntl(api_server->sock.fd, F_GETFL, 0);
	sock_flag = sock_flag | O_NONBLOCK;
	fcntl(api_server->sock.fd, F_SETFL, sock_flag);

	ret = listen(api_server->sock.fd, 16);

	if (ret < 0) {
		errcode = errno;
		write_log(0, "Bind error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	for (count = 0; count < INIT_API_THREADS; count++) {
		write_log(10, "Starting up API thread %d\n", count);
		val = malloc(sizeof(int));
		*val = count;
		ret = pthread_create(&(api_server->local_thread[count]), NULL,
			(void *)api_module, (void *)val);
		if (ret != 0) {
			errcode = ret;
			write_log(0, "Thread create error in %s. Code %d, %s\n",
				__func__, errcode, strerror(errcode));
			errcode = -errcode;
			goto errcode_handle;
		}
		val = NULL;
	}

	/* TODO: fork a thread that monitor usage and control extra threads */
	ret = pthread_create(&(api_server->monitor_thread), NULL,
			(void *)api_server_monitor, NULL);
	if (ret != 0) {
		errcode = ret;
		write_log(0, "Thread create error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: destroy_api_interface
*        Inputs: None
*       Summary: Destroy API server, and free resources.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int destroy_api_interface(void)
{
	int ret, errcode, count;

	/* Adding lock wait before terminating to prevent last sec
	thread changes */
	sem_wait(&(api_server->job_lock));
	for (count = 0; count < api_server->num_threads; count++)
		pthread_join(api_server->local_thread[count], NULL);
	pthread_join(api_server->monitor_thread, NULL);
	sem_post(&(api_server->job_lock));
	sem_destroy(&(api_server->job_lock));
	UNLINK(api_server->sock.addr.sun_path);
	free(api_server);
	api_server = NULL;
	return 0;
errcode_handle:
	return errcode;
}

int create_FS_handle(int arg_len, char *largebuf)
{
	DIR_ENTRY tmp_entry;
	char *buf, tmptype;
	int ret;

	buf = malloc(arg_len + 10);
#ifdef _ANDROID_ENV_
	memcpy(buf, largebuf, arg_len -1);
	buf[arg_len - 1] = 0;
	tmptype = largebuf[arg_len - 1];
	ret = add_filesystem(buf, tmptype, &tmp_entry);
#else
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;
	ret = add_filesystem(buf, &tmp_entry);
#endif

	free(buf);
	return ret;
}
int mount_FS_handle(int arg_len, char *largebuf)
{
	char *buf, *mpbuf;
	int ret;
	int fsname_len, mp_len;

	memcpy(&fsname_len, largebuf, sizeof(int));

	buf = malloc(fsname_len + 10);
	mp_len = arg_len - sizeof(int) - fsname_len;
	mpbuf = malloc(mp_len + 10);
	memcpy(buf, &(largebuf[sizeof(int)]), fsname_len);
	memcpy(mpbuf, &(largebuf[sizeof(int) + fsname_len]), mp_len);
	buf[fsname_len] = 0;
	mpbuf[mp_len] = 0;
	ret = mount_FS(buf, mpbuf);

	free(buf);
	free(mpbuf);
	return ret;
}

int unmount_FS_handle(int arg_len, char *largebuf)
{
	char *buf;
	int ret;

	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;
	ret = unmount_FS(buf);

	free(buf);
	return ret;
}

int mount_status_handle(int arg_len, char *largebuf)
{
	char *buf;
	int ret;

	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;
	ret = mount_status(buf);

	free(buf);
	return ret;
}

int delete_FS_handle(int arg_len, char *largebuf)
{
	char *buf;
	int ret;

	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;
	ret = delete_filesystem(buf);

	free(buf);
	return ret;
}

long long get_vol_size(int arg_len, char *largebuf)
{
	DIR_ENTRY temp_entry;
	MOUNT_T *tmp_info;
	char *buf;
	int ret, errcode;
	long long llretval;
	char temppath[METAPATHLEN];
	FILE *statfptr;
	FS_STAT_T tmpvolstat;
	ssize_t ret_ssize;

	statfptr = NULL;
	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;

	if ((arg_len == 0) || (buf[0] == '\0')) {
		/* Return the total size of all volumes */
		sem_wait(&(hcfs_system->access_sem));
		llretval = hcfs_system->systemdata.cache_size;
		sem_post(&(hcfs_system->access_sem));
		free(buf);
		return llretval;
	}

	sem_wait(&(fs_mgr_head->op_lock));
	sem_wait(&(mount_mgr.mount_lock));

	/* First check if FS already mounted */
	statfptr = NULL;

	ret = search_mount(buf, &tmp_info);
	if ((ret < 0) && (ret != -ENOENT)) {
		llretval = (long long) ret;
		goto error_handling;
	} else if (ret == 0) {
		/* Fetch stat from mounted volume */
		sem_wait(&(tmp_info->stat_lock));
		llretval = (tmp_info->FS_stat).system_size;
		sem_post(&(tmp_info->stat_lock));

		free(buf);
		sem_post(&(mount_mgr.mount_lock));
		sem_post(&(fs_mgr_head->op_lock));
		return llretval;
	}

	/* Check whether the filesystem exists */
	ret = check_filesystem_core(buf, &temp_entry);
	if (ret < 0) {
		llretval = (long long) ret;
		goto error_handling;
	}

	/* Fetch from stat file if not mounted */
	ret = fetch_stat_path(temppath, temp_entry.d_ino);
	if (ret < 0) {
		llretval = (long long) ret;
		goto error_handling;
	}

	statfptr = fopen(temppath, "r+");
	if (statfptr == NULL) {
		ret = (long long) errno;
		write_log(0, "IO error %d (%s)\n", ret,
		          strerror(ret));
		llretval = (long long) -ret;
		goto error_handling;
	}
	PREAD(fileno(statfptr), &tmpvolstat, sizeof(FS_STAT_T), 0);
	llretval = tmpvolstat.system_size;

	fclose(statfptr);
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));
	free(buf);
	return llretval;

errcode_handle:
	if (statfptr != NULL)
		fclose(statfptr);
	llretval = (long long) errcode;
error_handling:
	free(buf);
	sem_post(&(mount_mgr.mount_lock));
	sem_post(&(fs_mgr_head->op_lock));
	return llretval;
}

long long get_cloud_size(int arg_len, char *largebuf)
{
	DIR_ENTRY temp_entry;
	char *buf;
	int ret, errcode;
	long long llretval;
	char temppath[METAPATHLEN];
	FILE *statfptr;
	ssize_t ret_ssize;

	statfptr = NULL;
	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;

	if ((arg_len == 0) || (buf[0] == '\0')) {
		/* Return the total size of all volumes */
		sem_wait(&(hcfs_system->access_sem));
		llretval = hcfs_system->systemdata.backend_size;
		sem_post(&(hcfs_system->access_sem));
		free(buf);
		return llretval;
	}

	sem_wait(&(fs_mgr_head->op_lock));

	/* Check whether the filesystem exists */
	ret = check_filesystem_core(buf, &temp_entry);
	if (ret < 0) {
		llretval = (long long) ret;
		goto error_handling;
	}

	/* Fetch from stat file if not mounted */

	snprintf(temppath, METAPATHLEN - 1, "%s/FS_sync/FSstat%"FMT_INO_T,
		 METAPATH, temp_entry.d_ino);
	write_log(10, "Checking for FS stat in backend\n");
	statfptr = fopen(temppath, "r");
	if (statfptr == NULL) {
		ret = (long long) errno;
		write_log(0, "IO error %d (%s)\n", ret,
		          strerror(ret));
		llretval = (long long) -ret;
		goto error_handling;
	}
	flock(fileno(statfptr), LOCK_EX);
	PREAD(fileno(statfptr), &llretval, sizeof(long long), 0);
	flock(fileno(statfptr), LOCK_UN);
	fclose(statfptr);

	sem_post(&(fs_mgr_head->op_lock));
	free(buf);
	return llretval;

errcode_handle:
	if (statfptr != NULL) {
		flock(fileno(statfptr), LOCK_UN);
		fclose(statfptr);
	}
	llretval = (long long) errcode;
error_handling:
	free(buf);
	sem_post(&(fs_mgr_head->op_lock));
	return llretval;
}

int check_FS_handle(int arg_len, char *largebuf)
{
	DIR_ENTRY temp_entry;
	char *buf;
	int ret;

	buf = malloc(arg_len + 10);
	memcpy(buf, largebuf, arg_len);
	buf[arg_len] = 0;
	ret = check_filesystem(buf, &temp_entry);
	write_log(10, "Debug check volume %s returns %d\n", buf, ret);

	free(buf);
	return ret;
}

int list_FS_handle(DIR_ENTRY **entryarray, unsigned long *ret_entries)
{
	unsigned long num_entries, temp;
	int ret;

	ret = list_filesystem(0, NULL, &num_entries);

	write_log(10, "Debug list volume num volumes %ld\n", num_entries);
	if (ret < 0)
		return ret;
	if (num_entries > 0) {
		*entryarray = malloc(sizeof(DIR_ENTRY) * num_entries);
		ret = list_filesystem(num_entries, *entryarray, &temp);
	}
	write_log(10, "Debug list volume %d, %ld\n", ret, temp);
	*ret_entries = num_entries;
	return ret;
}

int unmount_all_handle(void)
{
	int ret;

	ret = unmount_all();

	return ret;
}

int pin_inode_handle(int arg_len, char *largebuf)
{
	ino_t pinned_inode;
	int retcode;

	retcode = 0;
	/* Error on transforming between 32bit and 64 bit ino_t? */
	memcpy(&pinned_inode, largebuf, sizeof(ino_t)); 
	write_log(10, "Debug: inode_num = %lld, sizeof(ino_t) = %d, "
		"arg_len = %d", pinned_inode, sizeof(ino_t), arg_len);
	// TODO: add semaphore of pinning group of inodes
	sem_wait(&(sys_super_block->pin_group_sem));
	retcode = pin_inode(pinned_inode);
	sem_post(&(sys_super_block->pin_group_sem));

	return retcode;
}

int unpin_inode_handle(int arg_len, char *largebuf)
{
	ino_t unpinned_inode;
	int retcode;

	retcode = 0;
	memcpy(&unpinned_inode, largebuf, sizeof(ino_t));
	write_log(10, "Debug: inode_num = %lld, sizeof(ino_t) = %d, "
		"arg_len = %d", unpinned_inode, sizeof(ino_t), arg_len);
	retcode = unpin_inode(unpinned_inode);

	return retcode;
}

int check_location_handle(int arg_len, char *largebuf)
{
	ino_t target_inode;
	int errcode;
	char metapath[METAPATHLEN];
	struct stat thisstat;
	META_CACHE_ENTRY_STRUCT *thisptr;
	char inode_loc;
	FILE_STATS_TYPE tmpstats;
	ssize_t ret_ssize;

	if (arg_len != sizeof(ino_t))
		return -EINVAL;

	memcpy(&target_inode, largebuf, sizeof(ino_t));
	write_log(10, "Debug API: checkpin inode %"FMT_INO_T"\n", target_inode);
	errcode = fetch_meta_path(metapath, target_inode);
	if (errcode < 0)
		return errcode;

	if (access(metapath, F_OK) != 0)
		return -ENOENT;

	thisptr = meta_cache_lock_entry(target_inode);
	if (thisptr == NULL)
		return -ENOMEM;

	errcode = meta_cache_lookup_file_data(target_inode, &thisstat,
						NULL, NULL, 0, thisptr);
	if (errcode < 0)
		goto errcode_handle;

	if (S_ISREG(thisstat.st_mode)) {
		errcode = meta_cache_open_file(thisptr);
		PREAD(fileno(thisptr->fptr), &tmpstats, sizeof(FILE_STATS_TYPE),
		      sizeof(struct stat) + sizeof(FILE_META_TYPE));
		if ((tmpstats.num_blocks == 0) ||
		    (tmpstats.num_blocks == tmpstats.num_cached_blocks))
			inode_loc = 0;  /* If the location is "local" */
		else if (tmpstats.num_cached_blocks == 0)
			inode_loc = 1;  /* If the location is "cloud" */
		else
			inode_loc = 2;  /* If the location is "hybrid" */
	} else {
		inode_loc = 0;  /* Non-file obj defaults to "local" */
	}

	meta_cache_close_file(thisptr);
	meta_cache_unlock_entry(thisptr);

	return inode_loc;

errcode_handle:
	meta_cache_unlock_entry(thisptr);
	return errcode;
}

int checkpin_handle(int arg_len, char *largebuf)
{
	ino_t target_inode;
	int retcode;
	char metapath[METAPATHLEN];
	struct stat thisstat;
	META_CACHE_ENTRY_STRUCT *thisptr;
	FILE_META_TYPE filemeta;
	DIR_META_TYPE dirmeta;
	SYMLINK_META_TYPE linkmeta;
	char is_local_pin;

	memcpy(&target_inode, largebuf, sizeof(ino_t));
	write_log(10, "Debug API: checkpin inode %"FMT_INO_T"\n", target_inode);
	retcode = fetch_meta_path(metapath, target_inode);
	if (retcode < 0)
		return retcode;

	if (access(metapath, F_OK) != 0)
		return -ENOENT;

	thisptr = meta_cache_lock_entry(target_inode);
	if (thisptr == NULL)
		return -ENOMEM;

	retcode = meta_cache_lookup_file_data(target_inode, &thisstat,
						NULL, NULL, 0, thisptr);
	if (retcode < 0)
		goto error_handling;

	if (S_ISREG(thisstat.st_mode)) {
		retcode = meta_cache_lookup_file_data(target_inode, NULL,
						&filemeta, NULL, 0, thisptr);
		if (retcode < 0)
			goto error_handling;
		is_local_pin = filemeta.local_pin;
	}

	if (S_ISDIR(thisstat.st_mode)) {
		retcode = meta_cache_lookup_dir_data(target_inode, NULL,
						&dirmeta, NULL, thisptr);
		if (retcode < 0)
			goto error_handling;
		is_local_pin = dirmeta.local_pin;
	}

	if (S_ISLNK(thisstat.st_mode)) {
		retcode = meta_cache_lookup_symlink_data(target_inode, NULL,
						&linkmeta, thisptr);
		if (retcode < 0)
			goto error_handling;
		is_local_pin = linkmeta.local_pin;
	}

	meta_cache_close_file(thisptr);
	meta_cache_unlock_entry(thisptr);

	if (is_local_pin == TRUE)
		return 1;
	else
		return 0;

error_handling:
	meta_cache_unlock_entry(thisptr);
	return retcode;
}

int check_dir_stat_handle(int arg_len, char *largebuf, DIR_STATS_TYPE *tmpstats)
{
	ino_t target_inode;
	int retcode;
	struct stat structstat;
	char metapath[METAPATHLEN];

	if (arg_len != sizeof(ino_t))
		return -EINVAL;

	memcpy(&target_inode, largebuf, sizeof(ino_t));
	write_log(10, "Debug API: target inode %"FMT_INO_T"\n", target_inode);
	retcode = fetch_meta_path(metapath, target_inode);
	if (retcode < 0) {
		tmpstats->num_local = retcode;
		tmpstats->num_cloud = retcode;
		tmpstats->num_hybrid = retcode;
		return retcode;
	}

	if (access(metapath, F_OK) != 0) {
		retcode = -ENOENT;
		tmpstats->num_local = retcode;
		tmpstats->num_cloud = retcode;
		tmpstats->num_hybrid = retcode;
		return retcode;
	}

	retcode = fetch_inode_stat(target_inode, &structstat, NULL, NULL);
	if (retcode == 0) {
		if (S_ISDIR(structstat.st_mode))
			retcode = read_dirstat_lookup(target_inode, tmpstats);
		else
			memset(tmpstats, 0, sizeof(DIR_STATS_TYPE));
	}
	if (retcode < 0) {
		tmpstats->num_local = retcode;
		tmpstats->num_cloud = retcode;
		tmpstats->num_hybrid = retcode;
	}
	write_log(10, "Dir stat lookup %lld, %lld, %lld\n",
		tmpstats->num_local, tmpstats->num_cloud, tmpstats->num_hybrid);
	return retcode;
}

/************************************************************************
*
* Function name: api_module
*        Inputs: void *index
*       Summary: Worker thread function for accepting incoming API calls
*                and process the requests. Will call other functions to
*                process requests if not defined in this function.
*  Return value: None
*
*************************************************************************/
/* TODO: Better error handling so that broken pipe arising from clients
not following protocol won't crash the system */
void api_module(void *index)
{
	int fd1;
	ssize_t size_msg, msg_len;
	struct timespec timer;
	struct timeval start_time, end_time;
	float elapsed_time;
	int retcode, sel_index, count, cur_index;
	size_t to_recv, to_send, total_sent;

	char buf[512];
	char *largebuf;
	char buf_reused;
	int msg_index;
	unsigned long num_entries;
	unsigned int api_code, arg_len, ret_len;
	long long llretval;

	DIR_ENTRY *entryarray;
	char *tmpptr;
	DIR_STATS_TYPE tmpstat;

	timer.tv_sec = 0;
	timer.tv_nsec = 100000000;

	write_log(10, "Startup index %d\n", *((int *)index));

	while (hcfs_system->system_going_down == FALSE) {
		fd1 = accept(api_server->sock.fd, NULL, NULL);
		if (fd1 < 0) {
			if (hcfs_system->system_going_down == TRUE)
				break;
			nanosleep(&timer, NULL);  /* Sleep for 0.1 sec */
			continue;
		}
		write_log(10, "Processing API request\n");
		msg_len = 0;
		msg_index = 0;
		largebuf = NULL;
		buf_reused = FALSE;

		gettimeofday(&start_time, NULL);

		/* Read API code */
		while (TRUE) {
			size_msg = recv(fd1, &buf[msg_len + msg_index], 4, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= 4)
				break;
		}
		if (msg_len < sizeof(unsigned int)) {
			/* Error reading API code. Return EINVAL. */
			write_log(5, "Invalid API code received\n");
			retcode = EINVAL;
			goto return_message;
		}
		memcpy(&api_code, &buf[msg_index], sizeof(unsigned int));
		msg_index += sizeof(unsigned int);
		msg_len -= sizeof(unsigned int);
		write_log(10, "API code is %d\n", api_code);

		/* Read total length of arguments */
		while (TRUE) {
			size_msg = recv(fd1, &buf[msg_len + msg_index], 4, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= 4)
				break;
		}
		if (msg_len < sizeof(unsigned int)) {
			/* Error reading API code. Return EINVAL. */
			write_log(5, "Invalid arg length received\n");
			retcode = EINVAL;
			goto return_message;
		}
		memcpy(&arg_len, &buf[msg_index], sizeof(unsigned int));
		msg_index += sizeof(unsigned int);
		msg_len -= sizeof(unsigned int);

		write_log(10, "API arg len is %d\n", arg_len);

		if (arg_len < 500) {
			/* Reuse the preallocated buffer */
			largebuf = &buf[msg_index];
			buf_reused = TRUE;
		} else {
			/* Allocate a buffer that's large enough */
			largebuf = malloc(arg_len+20);
			/* If error in allocating, return ENOMEM */
			if (largebuf == NULL) {
				write_log(0, "Out of memory in %s\n", __func__);
				retcode = ENOMEM;
				goto return_message;
			}
			/* If msg_len > 0, copy the rest of the message */
			if (msg_len > 0)
				memcpy(largebuf, &buf[msg_index], msg_len);
		}

		if (msg_len < 0)
			msg_len = 0;

		msg_index = 0;
		while (TRUE) {
			if (msg_len >= arg_len)
				break;
			if ((arg_len - msg_len) > 1024)
				to_recv = 1024;
			else
				to_recv = arg_len - msg_len;
			size_msg = recv(fd1, &largebuf[msg_len + msg_index],
					to_recv, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= arg_len)
				break;
		}
		if (msg_len < arg_len) {
			/* Error reading arguments. Return EINVAL. */
			write_log(5, "Error when reading API arguments\n");
			retcode = EINVAL;
			goto return_message;
		}

		switch (api_code) {
		case PIN:
			retcode = pin_inode_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case UNPIN:
			retcode = unpin_inode_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case CHECKDIRSTAT:
			retcode = check_dir_stat_handle(arg_len, largebuf,
							&tmpstat);
			if (retcode == 0) {
				ret_len = 3 * sizeof(long long);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &(tmpstat.num_local),
				     sizeof(long long), 0);
				send(fd1, &(tmpstat.num_cloud),
				     sizeof(long long), 0);
				send(fd1, &(tmpstat.num_hybrid),
				     sizeof(long long), 0);
			}
			break;
		case CHECKLOC:
			retcode = check_location_handle(arg_len, largebuf);
			if (retcode >= 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case CHECKPIN:
			retcode = checkpin_handle(arg_len, largebuf);
			if (retcode >= 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case TERMINATE:
			/* Terminate the system */
			unmount_all();
			hcfs_system->system_going_down = TRUE;
			retcode = 0;
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
			break;
		case VOLSTAT:
			/* Returns the system statistics */
			buf[0] = 0;
			retcode = 0;
			sem_wait(&(hcfs_system->access_sem));
			sprintf(buf, "%lld %lld %lld",
				hcfs_system->systemdata.system_size,
				hcfs_system->systemdata.cache_size,
				hcfs_system->systemdata.cache_blocks);
			sem_post(&(hcfs_system->access_sem));
			write_log(10, "debug stat hcfs %s\n", buf);
			ret_len = strlen(buf)+1;
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, buf, strlen(buf)+1, 0);
			break;
		case GETPINSIZE:
			buf[0] = 0;
			retcode = 0;
			ret_len = sizeof(long long);
			sem_wait(&(hcfs_system->access_sem));
			llretval = hcfs_system->systemdata.pinned_size;
			sem_post(&(hcfs_system->access_sem));
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case GETCACHESIZE:
			buf[0] = 0;
			retcode = 0;
			ret_len = sizeof(long long);
			sem_wait(&(hcfs_system->access_sem));
			llretval = hcfs_system->systemdata.cache_size;
			sem_post(&(hcfs_system->access_sem));
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case GETMAXPINSIZE:
			buf[0] = 0;
			retcode = 0;
			ret_len = sizeof(long long);
			llretval = MAX_PINNED_LIMIT;
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case GETMAXCACHESIZE:
			buf[0] = 0;
			retcode = 0;
			ret_len = sizeof(long long);
			llretval = CACHE_HARD_LIMIT;
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case GETVOLSIZE:
			llretval = get_vol_size(arg_len, largebuf);
			retcode = 0;
			ret_len = sizeof(long long);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case GETCLOUDSIZE:
			llretval = get_cloud_size(arg_len, largebuf);
			retcode = 0;
			ret_len = sizeof(long long);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &llretval, ret_len, 0);
			break;
		case TESTAPI:
			/* Simulate a long API call of 5 seconds */
			sleep(5);
			retcode = 0;
			cur_index = *((int *)index);
			write_log(10, "Index is %d\n", cur_index);
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
			break;
		case ECHOTEST:
			/*Echos the arguments back to the caller*/
			retcode = 0;
			ret_len = arg_len;
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			total_sent = 0;
			while (total_sent < ret_len) {
				if ((ret_len - total_sent) > 1024)
					to_send = 1024;
				else
					to_send = ret_len - total_sent;
				size_msg = send(fd1, &largebuf[total_sent],
					to_send, 0);
				total_sent += size_msg;
			}

			break;
		case CREATEVOL:
			retcode = create_FS_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case DELETEVOL:
			retcode = delete_FS_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case CHECKVOL:
			retcode = check_FS_handle(arg_len, largebuf);
			write_log(10, "retcode is %d\n", retcode);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case LISTVOL:
			/*Echos the arguments back to the caller*/
			retcode = list_FS_handle(&entryarray, &num_entries);
			tmpptr = (char *) entryarray;
			ret_len = sizeof(DIR_ENTRY) * num_entries;
			write_log(10, "Debug listFS return size %d\n", ret_len);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			total_sent = 0;
			while (total_sent < ret_len) {
				if ((ret_len - total_sent) > 1024)
					to_send = 1024;
				else
					to_send = ret_len - total_sent;
				size_msg = send(fd1, &tmpptr[total_sent],
					to_send, 0);
				total_sent += size_msg;
			}
			if (num_entries > 0)
				free(entryarray);
			break;
		case MOUNTVOL:
			retcode = mount_FS_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case UNMOUNTVOL:
			retcode = unmount_FS_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case CHECKMOUNT:
			retcode = mount_status_handle(arg_len, largebuf);
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		case UNMOUNTALL:
			retcode = unmount_all_handle();
			if (retcode == 0) {
				ret_len = sizeof(int);
				send(fd1, &ret_len, sizeof(unsigned int), 0);
				send(fd1, &retcode, sizeof(int), 0);
			}
			break;
		default:
			retcode = ENOTSUP;
			break;
		}
return_message:
		if (retcode != 0) {
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
		}
		if ((largebuf != NULL) && (buf_reused == FALSE))
			free(largebuf);
		largebuf = NULL;
		buf_reused = FALSE;
		close(fd1);

		/* Compute process time and update statistics */
		gettimeofday(&end_time, NULL);
		elapsed_time = (end_time.tv_sec + end_time.tv_usec * 0.000001)
			- (start_time.tv_sec + start_time.tv_usec * 0.000001);
		if (elapsed_time < 0)
			elapsed_time = 0;
		sem_wait(&(api_server->job_lock));
		sel_index = end_time.tv_sec % PROCESS_WINDOW;
		if (api_server->last_update < end_time.tv_sec) {
			/* reset statistics */
			count = end_time.tv_sec - api_server->last_update;
			if (count > PROCESS_WINDOW)
				count = PROCESS_WINDOW;
			cur_index = sel_index;
			while (count > 0) {
				count--;
				api_server->job_count[cur_index] = 0;
				api_server->job_totaltime[cur_index] = 0;
				if (count <= 0)
					break;
				cur_index--;
				if (cur_index < 0)
					cur_index = PROCESS_WINDOW - 1;
			}

			api_server->last_update = end_time.tv_sec;
		}
		api_server->job_count[sel_index] += 1;
		api_server->job_totaltime[sel_index] += elapsed_time;
		sem_post(&(api_server->job_lock));
		write_log(10, "Updated API server process time, %f sec\n",
			elapsed_time);
	}
	free(index);
}

/************************************************************************
*
* Function name: api_server_monitor
*        Inputs: None
*       Summary: Monitor thread for checking the current load of workers
*                and increase the number of works if load is high.
*  Return value: None
*
*************************************************************************/
void api_server_monitor(void)
{
	int count, totalrefs, index, ret;
	float totaltime, ratio;
	int *val;
	struct timeval cur_time;
	int sel_index, cur_index;
	struct timespec waittime;

	waittime.tv_sec = 1;
	waittime.tv_nsec = 0;

	while (hcfs_system->system_going_down == FALSE) {
		sleep(5);
		/* Using timed wait to handle system shutdown event */
		ret = sem_timedwait(&(api_server->job_lock), &waittime);
		if (ret < 0)
			continue;
		gettimeofday(&cur_time, NULL);

		/* Resets the statistics due to sliding window*/
		sel_index = cur_time.tv_sec % PROCESS_WINDOW;
		if (api_server->last_update < cur_time.tv_sec) {
			/* reset statistics */
			count = cur_time.tv_sec - api_server->last_update;
			if (count > PROCESS_WINDOW)
				count = PROCESS_WINDOW;
			cur_index = sel_index;
			while (count > 0) {
				count--;
				api_server->job_count[cur_index] = 0;
				api_server->job_totaltime[cur_index] = 0;
				if (count <= 0)
					break;
				cur_index--;
				if (cur_index < 0)
					cur_index = PROCESS_WINDOW - 1;
			}

			api_server->last_update = cur_time.tv_sec;
		}


		if (api_server->num_threads >= MAX_API_THREADS) {
			/* TODO: Perhaps could check whether to decrease
				number of threads */
			sem_post(&(api_server->job_lock));
			continue;
		}


		/* Compute worker loading using the statistics */
		totaltime = 0;
		totalrefs = 0;

		for (count = 0; count < PROCESS_WINDOW; count++) {
			totalrefs += api_server->job_count[count];
			totaltime += api_server->job_totaltime[count];
		}
		ratio = api_server->num_threads / totaltime;
		write_log(10, "Debug API monitor ref %d, time %f\n",
			totalrefs, totaltime);

		if (totalrefs > (INCREASE_RATIO * ratio)) {
			/* Add one more thread */
			val = malloc(sizeof(int));
			index = api_server->num_threads;
			*val = index;
			api_server->num_threads++;
			pthread_create(&(api_server->local_thread[index]), NULL,
				(void *)api_module, (void *)val);
			val = NULL;
			write_log(10, "Added one more thread to %d\n", index+1);
		}

		sem_post(&(api_server->job_lock));

	}
}
