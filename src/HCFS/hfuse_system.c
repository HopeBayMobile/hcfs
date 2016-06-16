/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_system.c
* Abstract: The c source code file for HCFS system main function.
*
* Revision History
* 2015/2/11 Jiahong added header for this file, and revising coding style.
*           Also changed inclusion of hcfs_cache.h to hcfs_cacheops.h.
* 2015/6/1 Jiahong working on improving error handling
* 2015/6/1 Jiahong changing logger
* 2016/5/23 Jiahong adding cache control init
*
**************************************************************************/
#include "hfuse_system.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
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
#include "hcfs_fromcloud.h"
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
#include "hcfs_fromcloud.h"
#include "pkg_cache.h"

/* TODO: A monitor thread to write system info periodically to a
	special directory in /dev/shm */

/************************************************************************
*
* Function name: init_cache_thresholds
*        Inputs: SYSTEM_CONF_STRUCT *config
*       Summary: Initialize max_pinned_limit and max_cache_limit for
*       	 different pin tyeps.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t init_cache_thresholds(SYSTEM_CONF_STRUCT *config)
{
	if (config->cache_hard_limit < 0)
		return -EINVAL;

	if (config->cache_reserved_space < 0)
		return -EINVAL;

	config->max_cache_limit[P_UNPIN] = CACHE_HARD_LIMIT;
	config->max_pinned_limit[P_UNPIN] = 0;

	config->max_cache_limit[P_PIN] = CACHE_HARD_LIMIT;
	config->max_pinned_limit[P_PIN] = MAX_PINNED_LIMIT;

	config->max_cache_limit[P_HIGH_PRI_PIN] =
		CACHE_HARD_LIMIT + RESERVED_CACHE_SPACE;
	config->max_pinned_limit[P_HIGH_PRI_PIN] =
		MAX_PINNED_LIMIT + RESERVED_CACHE_SPACE;

	return 0;
}

/************************************************************************
*
* Function name: init_hcfs_system_data
*        Inputs: None
*       Summary: Initialize HCFS system data.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t init_hcfs_system_data(void)
{
	int32_t errcode, ret;
	size_t ret_size;
	int64_t quota;

#ifdef _ANDROID_ENV_
	hcfs_system = (SYSTEM_DATA_HEAD *)malloc(sizeof(SYSTEM_DATA_HEAD));
#else
	int32_t shm_key;

	shm_key = shmget(2345, sizeof(SYSTEM_DATA_HEAD), IPC_CREAT | 0666);
	if (shm_key < 0) {
		errcode = errno;
		return -errcode;
	}
	hcfs_system = (SYSTEM_DATA_HEAD *)shmat(shm_key, NULL, 0);
	if (hcfs_system == (void *)-1) {
		errcode = errno;
		return -errcode;
	}
#endif

	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 1, 1);

	/* Init cache management control */
	sem_init(&(hcfs_system->something_to_replace), 1, 1);

	sem_init(&(hcfs_system->fuse_sem), 1, 0);
	sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0);
	sem_init(&(hcfs_system->check_cache_sem), 1, 0);
	sem_init(&(hcfs_system->check_next_sem), 1, 0);
	sem_init(&(hcfs_system->check_cache_replace_status_sem), 1, 0);
	sem_init(&(hcfs_system->monitor_sem), 1, 0);
	hcfs_system->system_going_down = FALSE;
	hcfs_system->backend_is_online = FALSE;
	hcfs_system->sync_manual_switch = !(access(HCFSPAUSESYNC, F_OK) == 0);
	update_sync_state(); /* compute hcfs_system->sync_paused */

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

		FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
		       hcfs_system->system_val_fptr);
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

	/* Use backup quota temporarily. It will be updated later. */
	ret = get_quota_from_backup(&quota);
	if (ret < 0) {
		write_log(5, "Backup usermeta looks unreliable. "
				"tmp set quota to %lld\n", CACHE_HARD_LIMIT);
		hcfs_system->systemdata.system_quota = CACHE_HARD_LIMIT;
	} else {
		write_log(5, "Backup usermeta exist. set quota to %lld\n",
				quota);
		hcfs_system->systemdata.system_quota = quota;
	}

	/* Xfer related */
	hcfs_system->last_xfer_shift_time = time(NULL);
	hcfs_system->xfer_upload_in_progress = FALSE;
	sem_init(&(hcfs_system->xfer_download_in_progress_sem), 1, 0);
	hcfs_system->systemdata.xfer_now_window = 0;
	memset(hcfs_system->systemdata.xfer_throughput, 0,
			sizeof(int64_t) * XFER_WINDOW_MAX);
	memset(hcfs_system->systemdata.xfer_total_obj, 0,
			sizeof(int64_t) * XFER_WINDOW_MAX);

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
int32_t sync_hcfs_system_data(char need_lock)
{
	int32_t ret, errcode;
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
int32_t init_hfuse(void)
{
	int32_t ret_val;

	ret_val = init_hcfs_system_data();
	if (ret_val < 0)
		return ret_val;
	ret_val = super_block_init();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_system_fh_table();
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
int32_t _init_download_curl(int32_t count)
{
	int32_t ret_val;

	snprintf(download_curl_handles[count].id,
		 sizeof(((CURL_HANDLE *)0)->id) - 1, "download_thread_%d",
		 count);

	curl_handle_mask[count] = FALSE;
	download_curl_handles[count].curl_backend = NONE;
	download_curl_handles[count].curl = NULL;
	UNUSED(ret_val);
	/* Do not init backend until actually needed */
	/*
		ret_val = hcfs_init_backend(&(download_curl_handles[count]));

		while ((ret_val < 200) || (ret_val > 299)) {
			write_log(0, "error in connecting to backend\n");
			if (download_curl_handles[count].curl != NULL)
				hcfs_destroy_backend(
					download_curl_handles[count].curl);
			ret_val = hcfs_init_backend(
					&(download_curl_handles[count])
				);
		}
	*/
	return 0;
}

/**
 * init_backend_related_module
 *
 * Initialize and create cache_loop_thread, delete_loop_thread,
 * upload_loop_thread, monitor_loop_thread. This function should be called
 * after setting up backend info.
 *
 * @return None
 */
void init_backend_related_module(void)
{
	int32_t count;

	if (CURRENT_BACKEND != NONE) {
		pthread_create(&cache_loop_thread, NULL, &run_cache_loop, NULL);
		pthread_create(&delete_loop_thread, NULL, &delete_loop, NULL);
		pthread_create(&upload_loop_thread, NULL, &upload_loop, NULL);
		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		sem_init(&pin_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
		for (count = 0; count < MAX_DOWNLOAD_CURL_HANDLE; count++)
			_init_download_curl(count);
		/* Init usermeta curl handle */
		snprintf(download_usermeta_curl_handle.id,
			sizeof(((CURL_HANDLE *)0)->id) - 1, "download_usermeta");
		download_usermeta_curl_handle.curl_backend = NONE;
		download_usermeta_curl_handle.curl = NULL;

		/* Update quota from cloud */
		memset(&download_usermeta_ctl, 0, sizeof(DOWNLOAD_USERMETA_CTL));
		sem_init(&(download_usermeta_ctl.access_sem), 0, 1);
		update_quota();
	}
}

/************************************************************************
*
* Function name: main
*        Inputs: int32_t argc, char **argv
*       Summary: Main function for HCFS system.
*  Return value: 0 if successful.
*
*************************************************************************/
/*TODO: Error handling after validating system config*/
int32_t main(int32_t argc, char **argv)
{
	CURL_HANDLE curl_handle;
	int32_t ret_val;
	struct rlimit nofile_limit;
#ifndef _ANDROID_ENV_
	int32_t count;
#endif

	ret_val = ignore_sigpipe();

        if (ret_val < 0)
                exit(-ret_val);

	logptr = NULL;

#ifndef OPENSSL_IS_BORINGSSL
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
#endif

	/*TODO: Error handling after reading system config*/
	/*TODO: Allow reading system config path from program arg */

	/* TODO: Selection of backend type via configuration */

	/* skip malloc if already did (in unittest) */
	if (system_config == NULL) {
		system_config =
		    (SYSTEM_CONF_STRUCT *)malloc(sizeof(SYSTEM_CONF_STRUCT));
	}
	if (system_config == NULL) {
		write_log(0, "Error: Out of mem\n");
		exit(-1);
	}
	ret_val = read_system_config(DEFAULT_CONFIG_PATH, system_config);

	if (ret_val < 0)
		exit(-1);

	ret_val = validate_system_config(system_config);

	if (ret_val < 0)
		exit(-1);

	ret_val = init_cache_thresholds(system_config);

	if (ret_val < 0)
		exit(-1);

	nofile_limit.rlim_cur = 150000;
	nofile_limit.rlim_max = 150001;

	ret_val = setrlimit(RLIMIT_NOFILE, &nofile_limit);
	if (ret_val < 0) {
		write_log(0, "Error in setting open file limits\n");
		/* exit(-1); */
	}

	UNUSED(curl_handle);
	/* Only init backend when actual transfer is needed */
	/*
		if (CURRENT_BACKEND != NONE) {
			sprintf(curl_handle.id, "main");
			ret_val = hcfs_init_backend(&curl_handle);
			if ((ret_val < 200) || (ret_val > 299)) {
				write_log(0,
					"Error in connecting to backend. Code
	   %d\n",
					ret_val);
				write_log(0, "Backend %d\n", CURRENT_BACKEND);
				exit(-1);
			}

			ret_val = hcfs_list_container(&curl_handle);
			if (((ret_val < 200) || (ret_val > 299)) && (ret_val !=
	   404)) {
				write_log(0, "Error in connecting to
	   backend\n");
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
	init_sync_stat_control();

#ifdef _ANDROID_ENV_
	ret_val = init_pathlookup();
	if (ret_val < 0)
		exit(ret_val);
	ret_val = init_dirstat_lookup();
	if (ret_val < 0)
		exit(ret_val);
	ret_val = init_pkg_cache();
	if (ret_val < 0)
		exit(ret_val);

	open_log("hcfs_android_log");
#ifdef VERSION_NUM
	write_log(2, "\nVersion: %s\nStart logging\n", VERSION_NUM);
#endif

	/* Init backend related services */
	if (CURRENT_BACKEND != NONE) {
		init_backend_related_module();
	}

	hook_fuse(argc, argv);
	/* TODO: modify this so that backend config can be turned on
	even when volumes are mounted */

	if (CURRENT_BACKEND != NONE) {
		pthread_join(cache_loop_thread, NULL);
		pthread_join(delete_loop_thread, NULL);
		pthread_join(upload_loop_thread, NULL);
		destroy_monitor_loop_thread();
		pthread_join(monitor_loop_thread, NULL);
		write_log(10, "Debug: All threads terminated\n");
	}
	close_log();
	destroy_dirstat_lookup();
	destroy_pathlookup();
	destroy_pkg_cache();
#else
	ret_val = init_pathlookup();
	if (ret_val < 0)
		exit(ret_val);
	ret_val = init_dirstat_lookup();
	if (ret_val < 0)
		exit(ret_val);

	/* Start up children */
	proc_idx = 0;
	if (CURRENT_BACKEND != NONE) {
		for (proc_idx = 0; proc_idx < CHILD_NUM; ++proc_idx) {
			this_pid = fork();
			if (this_pid != 0) {
				child_pids[proc_idx] = this_pid;
				continue;
			}
			/* exit with proc_idx from 0 to CHILD_NUM */
			break;
		}
	}

	switch (proc_idx) {
	case 0:
		/* main process */
		open_log("fuse.log");
		write_log(2, "\nStart logging fuse\n");
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&pin_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
		sem_init(&download_curl_control_sem, 0, 1);

		if (CURRENT_BACKEND != NONE) {
			/* Update quota from cloud */
			memset(&download_usermeta_ctl, 0,
					sizeof(DOWNLOAD_USERMETA_CTL));
			sem_init(&(download_usermeta_ctl.access_sem), 0, 1);
			update_quota();
		}

		for (count = 0; count < MAX_DOWNLOAD_CURL_HANDLE; count++)
			_init_download_curl(count);

		hook_fuse(argc, argv);
		write_log(2, "Waiting for subprocesses to terminate\n");
		if (CURRENT_BACKEND != NONE) {
			for (proc_idx = 1; proc_idx <= CHILD_NUM; ++proc_idx)
				waitpid(child_pids[proc_idx], NULL, 0);
		}
		close_log();
		destroy_dirstat_lookup();
		destroy_pathlookup();

		break;
	/* children processed begin */
	case 1:
		open_log("cache_maintain.log");
		write_log(2, "\nStart logging cache cleanup\n");
		run_cache_loop();
		close_log();
		break;
	case 2:
		open_log("backend_upload.log");
		write_log(2, "\nStart logging backend upload\n");
		
		/* Init curl handle */
		sem_init(&download_curl_sem, 0,
				MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE;
				count++)
			_init_download_curl(count, "upload_loop");

		pthread_create(&delete_loop_thread, NULL, &delete_loop, NULL);
		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		upload_loop();
		pthread_join(delete_loop_thread, NULL);
		pthread_join(monitor_loop_thread, NULL);
		close_log();
		break;
	}
#endif /* _ANDROID_ENV_ */
	return 0;
}
