/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_system.c
* Abstract: The c source code file for HCFS system main function.
*
* Revision History
* 2015/2/11 Jiahong added header for this file, and revising coding style.
*           Also changed inclusion of hcfs_cache.h to hcfs_cacheops.h.
* 2015/6/1 Jiahong working on improving error handling
* 2015/6/1 Jiahong changing logger
*
**************************************************************************/
#define _GNU_SOURCE
#include "hfuse_system.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#ifndef _ANDROID_ENV_
#include <sys/shm.h>
#include <sys/ipc.h>
#endif

#include <openssl/hmac.h>
#include <openssl/engine.h>

#include "fuseop.h"
#include "meta_mem_cache.h"
#include "global.h"
#include "super_block.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "hcfs_cacheops.h"
#include "monitor.h"
#include "params.h"
#include "utils.h"
#include "filetables.h"
#include "macro.h"
#include "logger.h"
#include "mount_manager.h"
#include "FS_manager.h"
#include "path_reconstruct.h"

extern SYSTEM_CONF_STRUCT system_config;

/* TODO: A monitor thread to write system info periodically to a
	special directory in /dev/shm */

/************************************************************************
*
* Function name: init_hcfs_system_data
*        Inputs: None
*       Summary: Initialize HCFS system data.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int init_hcfs_system_data(void)
{
	int errcode;
	size_t ret_size;

#ifdef _ANDROID_ENV_
	hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
#else
	int shm_key;
	shm_key = shmget(2345, sizeof(SYSTEM_DATA_HEAD), IPC_CREAT | 0666);
	if (shm_key < 0) {
		errcode = errno;
		return -errcode;
	}
	hcfs_system = (SYSTEM_DATA_HEAD *) shmat(shm_key, NULL, 0);
	if (hcfs_system == (void *)-1) {
		errcode = errno;
		return -errcode;
	}
#endif

	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 1, 1);
	sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0);
	sem_init(&(hcfs_system->check_cache_sem), 1, 0);
	sem_init(&(hcfs_system->check_next_sem), 1, 0);
	hcfs_system->system_going_down = FALSE;

	hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
	if (hcfs_system->system_val_fptr == NULL) {
		errcode = errno;
		if (errcode != ENOENT) {
			write_log(0, "Error reading system file. Code %d, %s\n",
				errcode, strerror(errcode));
			return -errcode;
		}
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "w+");
		if (hcfs_system->system_val_fptr == NULL) {
			errcode = errno;
			write_log(0, "Error reading system file. Code %d, %s\n",
				errcode, strerror(errcode));
			return -errcode;
		}

		FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE),
					1, hcfs_system->system_val_fptr);
		fclose(hcfs_system->system_val_fptr);
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
		if (hcfs_system->system_val_fptr == NULL) {
			errcode = errno;
			write_log(0, "Error reading system file. Code %d, %s\n",
				errcode, strerror(errcode));
			return -errcode;
		}
	}
	setbuf(hcfs_system->system_val_fptr, NULL);
	FREAD(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
						hcfs_system->system_val_fptr);

	return 0;
errcode_handle:
	fclose(hcfs_system->system_val_fptr);
	return errcode;
}

/************************************************************************
*
* Function name: sync_hcfs_system_data
*        Inputs: char need_lock
*       Summary: Sync HCFS system meta to disk. "need_lock" indicates
*                whether we need to first acquire the access lock.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int sync_hcfs_system_data(char need_lock)
{
	int ret, errcode;
	size_t ret_size;

	if (need_lock == TRUE)
		sem_wait(&(hcfs_system->access_sem));
	FSEEK(hcfs_system->system_val_fptr, 0, SEEK_SET);
	FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
						hcfs_system->system_val_fptr);
	if (need_lock == TRUE)
		sem_post(&(hcfs_system->access_sem));

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: init_hfuse
*        Inputs: None
*       Summary: Initialize HCFS system.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int init_hfuse(void)
{
	int ret_val;

	ret_val = super_block_init();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_system_fh_table();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_hcfs_system_data();
	if (ret_val < 0)
		return ret_val;

	ret_val = init_fs_manager();
	if (ret_val < 0)
		return ret_val;

	ret_val = init_mount_mgr();
	if (ret_val < 0)
		return ret_val;

	return 0;
}

/* Helper function to initialize curl handles for downloading objects */
int _init_download_curl(int count)
{
	int ret_val;

	snprintf(download_curl_handles[count].id, 255,
				"download_thread_%d", count);

	curl_handle_mask[count] = FALSE;
	download_curl_handles[count].curl_backend = NONE;
	download_curl_handles[count].curl = NULL;
	/* Do not init backend until actually needed */
/*
	ret_val = hcfs_init_backend(&(download_curl_handles[count]));

	while ((ret_val < 200) || (ret_val > 299)) {
		write_log(0, "error in connecting to backend\n");
		if (download_curl_handles[count].curl != NULL)
			hcfs_destroy_backend(download_curl_handles[count].curl);
		ret_val = hcfs_init_backend(&(download_curl_handles[count]));
	}
*/
	return 0;
}

/************************************************************************
*
* Function name: main
*        Inputs: int argc, char **argv
*       Summary: Main function for HCFS system.
*  Return value: 0 if successful.
*
*************************************************************************/
/*TODO: Error handling after validating system config*/
int main(int argc, char **argv)
{
	CURL_HANDLE curl_handle;
	int ret_val;
	struct rlimit nofile_limit;
#ifdef _ANDROID_ENV_
	pthread_t delete_loop_thread;
	pthread_t upload_loop_thread;
	pthread_t cache_loop_thread;
	pthread_t monitor_loop_thread;
#else
#define NUMBER_OF_CHILDREN 3
	pid_t child_pids[NUMBER_OF_CHILDREN];
	pid_t this_pid;
	int process_num;
#endif  /* _ANDROID_ENV_ */
	int count;


	logptr = NULL;

	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

	/*TODO: Error handling after reading system config*/
	/*TODO: Allow reading system config path from program arg */

	/* TODO: Selection of backend type via configuration */

	ret_val = read_system_config(DEFAULT_CONFIG_PATH);

	if (ret_val < 0)
		exit(-1);

	ret_val = validate_system_config();

	if (ret_val < 0)
		exit(-1);

	nofile_limit.rlim_cur = 150000;
	nofile_limit.rlim_max = 150001;

	ret_val = setrlimit(RLIMIT_NOFILE, &nofile_limit);
	if (ret_val < 0) {
		write_log(0, "Error in setting open file limits\n");
		/* exit(-1); */
	}

	/* Only init backend when actual transfer is needed */
/*
	if (CURRENT_BACKEND != NONE) {
		sprintf(curl_handle.id, "main");
		ret_val = hcfs_init_backend(&curl_handle);
		if ((ret_val < 200) || (ret_val > 299)) {
			write_log(0,
				"Error in connecting to backend. Code %d\n",
				ret_val);
			write_log(0, "Backend %d\n", CURRENT_BACKEND);
			exit(-1);
		}

		ret_val = hcfs_list_container(&curl_handle);
		if (((ret_val < 200) || (ret_val > 299)) && (ret_val != 404)) {
			write_log(0, "Error in connecting to backend\n");
			exit(-1);
		}
		write_log(10, "ret code %d\n", ret_val);

		hcfs_destroy_backend(curl_handle.curl);
	}
*/
	ret_val = init_hfuse();
	if (ret_val < 0)
		exit(-1);

	/* TODO: error handling for log files */
#ifdef _ANDROID_ENV_
	ret_val = init_pathlookup();
	if (ret_val < 0)
		exit(ret_val);

	open_log("hcfs_android_log");
	write_log(2, "\nStart logging\n");
	if (CURRENT_BACKEND != NONE) {
		pthread_create(&cache_loop_thread, NULL, &run_cache_loop, NULL);
		pthread_create(&delete_loop_thread, NULL, &delete_loop, NULL);
		pthread_create(&upload_loop_thread, NULL, &upload_loop, NULL);
		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE; count++)
			_init_download_curl(count);
	}
	hook_fuse(argc, argv);
	/* TODO: modify this so that backend config can be turned on
	even when volumes are mounted */

	if (CURRENT_BACKEND != NONE) {
		pthread_join(cache_loop_thread, NULL);
		pthread_join(delete_loop_thread, NULL);
		pthread_join(upload_loop_thread, NULL);
		pthread_join(monitor_loop_thread, NULL);
	}
	close_log();
	destroy_pathlookup();
#else
	/* Start up children */
	for (process_num = 0; process_num < NUMBER_OF_CHILDREN;
			++process_num) {
		this_pid = fork();
		if (this_pid != 0) {
			child_pids[process_num] = this_pid;
			continue;
		}
		/* children process */
		if(process_num == 0) {
			open_log("cache_maintain.log");
			write_log(2, "\nStart logging cache cleanup\n");
			run_cache_loop();
			close_log();
		}
		else if(process_num == 1) {
			open_log("backend_upload.log");
			write_log(2, "\nStart logging backend upload\n");
			pthread_create(&delete_loop_thread, NULL, &delete_loop,
					NULL);
			upload_loop();
			pthread_join(delete_loop_thread, NULL);
			close_log();
		}
		else if(process_num == 2) {
			open_log("backend_connection.log");
			write_log(2, "\nStart logging backend connection\n");
			monitor_loop();
			close_log();
		}
		return 0;
		/* children process end */
	}

	/* Only main process runs to here */
	open_log("fuse.log");
	write_log(2, "\nStart logging fuse\n");
	sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
	sem_init(&download_curl_control_sem, 0, 1);

	for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE; count++)
		_init_download_curl(count);

	hook_fuse(argc, argv);
	write_log(2, "Waiting for subprocesses to terminate\n");
	for (process_num = 0; process_num < NUMBER_OF_CHILDREN;
			++process_num) {
		waitpid(child_pids[process_num], NULL, 0);
	}
	close_log();
#endif  /* _ANDROID_ENV_ */
	return 0;
}
