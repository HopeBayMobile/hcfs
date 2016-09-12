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

#include "fuse_notify.h"
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
#include "api_interface.h"
#include "event_notification.h"
#include "syncpoint_control.h"
#include "do_restoration.h"
#include "rebuild_super_block.h"

/* TODO: A monitor thread to write system info periodically to a
	special directory in /dev/shm */

/************************************************************************
*
* Function name: init_hcfs_system_data
*        Inputs: int8_t restoring_status
*       Summary: Initialize HCFS system data.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t init_hcfs_system_data(int8_t restoring_status)
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
	hcfs_system->writing_sys_data = FALSE;
	hcfs_system->system_restoring = restoring_status;

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

void *_write_sys(__attribute__((unused)) void *fakeptr)
{
	int32_t ret, errcode;
	size_t ret_size;

	sleep(2);
	sem_wait(&(hcfs_system->access_sem));
	FSEEK(hcfs_system->system_val_fptr, 0, SEEK_SET);
	FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
	       hcfs_system->system_val_fptr);
	hcfs_system->writing_sys_data = FALSE;
	write_log(10, "Syncing system data\n");
	sem_post(&(hcfs_system->access_sem));

errcode_handle:
	return NULL;
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
	pthread_t write_sys_thread;

	if (need_lock == TRUE) {
		sem_wait(&(hcfs_system->access_sem));
		FSEEK(hcfs_system->system_val_fptr, 0, SEEK_SET);
		FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
		       hcfs_system->system_val_fptr);
		sem_post(&(hcfs_system->access_sem));
	} else {
		if (hcfs_system->writing_sys_data == FALSE) {
			hcfs_system->writing_sys_data = TRUE;
			pthread_create(&(write_sys_thread),
				&prefetch_thread_attr, &_write_sys,
				NULL);
		}
	}
	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: init_hfuse
*        Inputs: int8_t restoring_status
*       Summary: Initialize HCFS system.
*  Return value: 0 if successful. Otherwise returns the negation of the
*                appropriate error code.
*
*************************************************************************/
int32_t init_hfuse(int8_t restoring_status)
{
	int32_t ret_val = 0;

	if (ret_val == 0)
		ret_val = init_hcfs_system_data(restoring_status);
	if (ret_val == 0)
		ret_val = init_system_fh_table();
	if (ret_val == 0)
		ret_val = init_fs_manager();
	if (ret_val == 0)
		ret_val = init_mount_mgr();

	/* TODO: error handling for log files */
	init_sync_stat_control();

	if (ret_val == 0)
		ret_val = check_and_create_metapaths();
	if (ret_val == 0)
		ret_val = check_and_create_blockpaths();
	if (ret_val == 0)
		ret_val = init_pathlookup();
	if (ret_val == 0)
		ret_val = init_dirstat_lookup();

	return ret_val;
}

/* Helper function to initialize curl handles for downloading objects */
int32_t _init_download_curl(int32_t count)
{

	snprintf(download_curl_handles[count].id,
		 sizeof(((CURL_HANDLE *)0)->id) - 1, "download_thread_%d",
		 count);

	curl_handle_mask[count] = FALSE;
	download_curl_handles[count].curl_backend = NONE;
	download_curl_handles[count].curl = NULL;
	/* Do not init backend until actually needed */
	/*
	int32_t ret_val;
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
	int32_t ret;
	char syncpoint_path[400];

	fetch_syncpoint_data_path(syncpoint_path);
	if (!access(syncpoint_path, F_OK)) {
		super_block_exclusive_locking();
		ret = init_syncpoint_resource();
		super_block_exclusive_release();
		if (ret < 0) {
			write_log(0, "Error: Fail to init sync point data\n");
			exit(-1);
		}
	}

	if (CURRENT_BACKEND != NONE) {
		/*pthread_create(&cache_loop_thread, NULL, &run_cache_loop, NULL);*/
		/*pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);*/
		pthread_create(&delete_loop_thread, NULL, &delete_loop, NULL);
		pthread_create(&upload_loop_thread, NULL, &upload_loop, NULL);
	}
}

void init_download_module(void)
{
	int32_t count;

	if (CURRENT_BACKEND != NONE) {
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		sem_init(&nonread_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
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

int32_t init_event_notify_module(void)
{
	pthread_create(&event_loop_thread, NULL, &event_worker_loop, NULL);
	return init_event_queue();
}

/************************************************************************
Helper function for checking if the battery is below a threshold
******/
int32_t _is_battery_low()
{
	FILE *fptr;
	int32_t retval, powerlevel;

	if (access("/sys/class/power_supply/battery/capacity", F_OK) == 0)
		fptr = fopen("/sys/class/power_supply/battery/capacity", "r");
	else if (access("/sys/class/power_supply/BAT0/capacity", F_OK) == 0)
		fptr = fopen("/sys/class/power_supply/BAT0/capacity", "r");
	else
		fptr = NULL;

	if (fptr == NULL) {
		write_log(4, "Unable to access battery level\n");
		return FALSE;
	}

	powerlevel = 0;

	retval = fscanf(fptr, "%d\n", &powerlevel);

	if (retval < 1) {
		write_log(4, "Unable to read battery level\n");
		fclose(fptr);
		return FALSE;
	}

	/* Hack for Android: If powerlevel = 50, should check again
	as the value might be a fake from system */
	if (powerlevel == 50) {
		write_log(4, "Re-reading battery level in 10 seconds\n");
		sleep(10);
		fseek(fptr, 0, SEEK_SET);
		powerlevel = 0;
		retval = fscanf(fptr, "%d\n", &powerlevel);
		if (retval < 1) {
			write_log(4, "Unable to read battery level\n");
			fclose(fptr);
			return FALSE;
		}
	}

	fclose(fptr);

	if (powerlevel <= BATTERY_LOW_LEVEL) {
		write_log(4, "Battery low, shutting down. (level is %d)\n",
		          powerlevel);
		return TRUE;
	}

	write_log(4, "Battery level is %d. Continue with bootup\n",
	          powerlevel);
	return FALSE;
}

int32_t _check_partial_storage(void)
{
	char todelete_metapath[METAPATHLEN];
	char todelete_blockpath[BLOCKPATHLEN];

	snprintf(todelete_metapath, METAPATHLEN, "%s_todelete",
	         METAPATH);
	snprintf(todelete_blockpath, BLOCKPATHLEN, "%s_todelete",
	         BLOCKPATH);

/* FEATURE TODO: It's possible that restore_metapath exists but restore_blockpath
does not (i.e. no block to be restored in stage 1). Will need to first
check this situation to decide if need to rename blockpath to todelete
and then create a new blockpath */

/* FEATURE TODO: error handling? */
	if (access(RESTORE_METAPATH, F_OK) == 0) {
		/* Need to rename the path to the current one */
		/* First check if need to rename old path to todelete */
		if (access(METAPATH, F_OK) == 0)
			rename(METAPATH, todelete_metapath);
		rename(RESTORE_METAPATH, METAPATH);
	}

	if (access(RESTORE_BLOCKPATH, F_OK) == 0) {
		/* Need to rename the path to the current one */
		/* First check if need to rename old path to todelete */
		if (access(BLOCKPATH, F_OK) == 0)
			rename(BLOCKPATH, todelete_blockpath);
		rename(RESTORE_BLOCKPATH, BLOCKPATH);
	}

	return 0;
}

/* Function for queueing inodes marked as todelete in restoration stage 1
to the to_delete list in super block in stage 2. */
void _mark_sb_to_delete(void)
{
	int64_t count, count2, num_todelete;
	ino_t rootino, todelete_inode;
	char restore_todelete_list[METAPATHLEN];
	char todelete_metapath[METAPATHLEN];
	FILE *to_delete_fptr = NULL, *metafptr = NULL;
	struct stat tmpstat;
	int32_t ret, errcode;
	size_t ret_size;
	HCFS_STAT tmphcfsstat;

	if (ROOT_INFO.num_roots <= 0)
		return;
	for (count = 0; count < ROOT_INFO.num_roots; count++) {
		rootino = ROOT_INFO.roots[count];

		snprintf(restore_todelete_list, METAPATHLEN,
			"%s/todelete_list_%" PRIu64,
		         METAPATH, (uint64_t) rootino);
		/* Skip the todelete list if does not exist or
		cannot access */
		if (access(restore_todelete_list, F_OK) < 0)
			continue;
		ret = stat(restore_todelete_list, &tmpstat);
		if (ret < 0)
			continue;
		if (tmpstat.st_size == 0) {
			unlink(restore_todelete_list);
			continue;
		}
		num_todelete = (int64_t) (tmpstat.st_size / sizeof(ino_t));
		to_delete_fptr = fopen(restore_todelete_list, "r");
		if (to_delete_fptr == NULL)
			continue;
		/* Mark as todelete inodes in the list */
		for (count2 = 0; count2 < num_todelete; count2++) {
			FREAD(&todelete_inode, sizeof(ino_t), 1,
			      to_delete_fptr);
			fetch_todelete_path(todelete_metapath, todelete_inode);
			metafptr = fopen(todelete_metapath, "r");
			if (metafptr == NULL)
				continue;
			FREAD(&tmphcfsstat, sizeof(HCFS_STAT), 1, metafptr);
			rebuild_super_block_entry(todelete_inode, &tmphcfsstat,
			                          UNPIN);
			super_block_to_delete(todelete_inode, TRUE);
			write_log(10, "Marked %" PRIu64 " as to delete\n",
			          (uint64_t) todelete_inode);
errcode_handle:
			/* If error occurs, just close this meta and continue */
			fclose(metafptr);
			metafptr = NULL;
		}

		fclose(to_delete_fptr);
		to_delete_fptr = NULL;
		unlink(restore_todelete_list);
	}
}

void process_upgrade_meta_changes(void)
{
	DIR *dstream;
	struct dirent *tmpentry;
	char tmppath[METAPATHLEN];
	char tmppath1[METAPATHLEN];

	/* Handle changes in FS_CLOUD_STAT_T */
	snprintf(tmppath, METAPATHLEN - 1, "%s/FS_sync", METAPATH);

	dstream = opendir(tmppath);
	if (dstream == NULL)
		return;

	while ((tmpentry = readdir(dstream)) != NULL) {
		snprintf(tmppath1, METAPATHLEN - 1, "%s/%s", tmppath,
		         tmpentry->d_name);
		convert_cloud_stat_struct(tmppath1);
	}

	closedir(dstream);
	return;
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
	int32_t ret_val, ret;
	struct rlimit nofile_limit;
#ifndef _ANDROID_ENV_
	int32_t count;
#endif
	int8_t restoring_status;

	ret_val = ignore_sigpipe();

        if (ret_val < 0)
                exit(-ret_val);

	logptr = NULL;
	memset(&ROOT_INFO, 0, sizeof(ROOT_INFO_T));

#ifndef OPENSSL_IS_BORINGSSL
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
#endif

	/* skip malloc if already did (in unittest) */
	if (system_config == NULL) {
		system_config =
		    (SYSTEM_CONF_STRUCT *)malloc(sizeof(SYSTEM_CONF_STRUCT));
	}
	if (system_config == NULL) {
		write_log(0, "Error: Out of mem\n");
		exit(-1);
	}
	ret_val = init_system_config_settings(DEFAULT_CONFIG_PATH,
					      system_config);
	if (ret_val < 0)
		exit(-1);

	/* Move log opening earlier for android to log low battery events */
#ifdef _ANDROID_ENV_
	open_log("hcfs_android_log");
#endif

	/* Check if battery level is low. If so, shutdown */
	ret_val = _is_battery_low();
	if (ret_val == TRUE) {
		close_log();
		execlp("setprop", "setprop", "sys.powerctl",
		                 "shutdown", NULL);
		exit(-1);
	}

	nofile_limit.rlim_cur = 150000;
	nofile_limit.rlim_max = 150001;

	ret_val = setrlimit(RLIMIT_NOFILE, &nofile_limit);
	if (ret_val < 0) {
		write_log(0, "Error in setting open file limits\n");
		/* exit(-1); */
	}

	/* Convert meta changes if needed */
	process_upgrade_meta_changes();

	/* Check if the system is being restored (and in stage 2) */
	/* FEATURE TODO: Check if the backend setting is correct before
	switching metastorage / blockstorage */
	/* FEATURE TODO: Need to check if can correctly unmount when stage 2
	is in progress */
	restoring_status = NOT_RESTORING;
	init_restore_path();
	ret_val = check_restoration_status();
	if (ret_val == 1) {
		restoring_status = RESTORING_STAGE1;
	} else if (ret_val == 2) {
		/* If the system is in stage 2, make sure that
		meta and block storage are pointed to the partially
		restored one */
		restoring_status = RESTORING_STAGE2;
		write_log(10, "Checking if need to switch storage paths\n");
		_check_partial_storage();
	}

	ret_val = init_hfuse(restoring_status);
	if (ret_val < 0)
		exit(ret_val);

#ifdef _ANDROID_ENV_
	ret_val = init_pkg_cache();
	if (ret_val < 0)
		exit(ret_val);

	ret_val = init_hfuse_ll_notify_loop();
	if (ret_val < 0)
		exit(ret_val);

	/* Init event notify service */
	ret_val = init_event_notify_module();
	if (ret_val < 0)
		exit(ret_val);

	/* Init backend related services and super block */
	if ((CURRENT_BACKEND != NONE) &&
	    (restoring_status != RESTORING_STAGE1)) {
		init_download_module();
		ret = check_init_super_block();
		if (ret < 0) {
			exit(ret);
		} else if (ret > 0) { /* It just opened old superblock */
			write_log(10, "Debug: Open old superblock.\n");
			init_backend_related_module();
		} else {
			write_log(4, "Rebuild superblock.\n");
			/* Rebuild superblock.
			 * Do NOT init upload/delete mgmt */

			/* Check if need to mark inodes deleted in stage 1
			in super block */
			_mark_sb_to_delete();
		}

		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		pthread_create(&cache_loop_thread, NULL, &run_cache_loop, NULL);
	} else if ((CURRENT_BACKEND != NONE) &&
	    (restoring_status == RESTORING_STAGE1)) {
		ret = check_init_super_block();
		if (ret < 0) {
			exit(ret);
		} else if (ret > 0) { /* Just opened old superblock */
			write_log(8, "Open old (or create new) superblock\n");
		} else {
			write_log(0, "Unexpected sb error in stage 1\n");
			exit(-1);
		}

		/* Only bring up monitor thread if in restoration process */
		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		/* Try to reduce cache size if now in stage 1 of restoration */
		if (hcfs_system->system_restoring == RESTORING_STAGE1) {
			ret = restore_stage1_reduce_cache();
			if (ret == 0)
				start_download_minimal();
			else
				notify_restoration_result(1, ret);
		}

	} else {
		ret = check_init_super_block();
		if (ret < 0) {
			exit(ret);
		} else if (ret > 0) { /* Just opened old superblock */
			write_log(8, "Open old (or create new) superblock\n");
		} else {
			write_log(0, "Error: Cannot restore because"
				" there is no backend info.\n");
			exit(-1);
		}
	}
	hook_fuse(argc, argv);

	if (CURRENT_BACKEND != NONE) {
		if (CURRENT_BACKEND == SWIFTTOKEN) {
			/* Wake up all threads blocked in get_swift_auth fn */
			pthread_mutex_lock(&(swifttoken_control.waiting_lock));
			pthread_cond_broadcast(&(swifttoken_control.waiting_cond));
			pthread_mutex_unlock(&(swifttoken_control.waiting_lock));
		}
		/* When system restoring, deletion and uploading services
		 * are not enabled. They are created after restoration
		 * completed. */
		if (hcfs_system->system_restoring == NOT_RESTORING) {
			pthread_join(delete_loop_thread, NULL);
			pthread_join(upload_loop_thread, NULL);
		}
		if (hcfs_system->system_restoring != RESTORING_STAGE1)
			pthread_join(cache_loop_thread, NULL);
		destroy_monitor_loop_thread();
		pthread_join(monitor_loop_thread, NULL);
		write_log(10, "Debug: All threads terminated\n");
	}

	destroy_event_worker_loop_thread();
	pthread_join(event_loop_thread, NULL);

	destory_hfuse_ll_notify_loop();

	sync_hcfs_system_data(TRUE);
	destroy_dirstat_lookup();
	destroy_pathlookup();
	destroy_pkg_cache();

	write_log(4, "HCFS shutting down normally\n");
	close_log();
	sync();
#else  /* ! _ANDROID_ENV_ */
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
		sem_init(&download_curl_sem, 0, MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&nonread_download_curl_sem, 0, MAX_PIN_DL_CONCURRENCY);
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
			for (proc_idx = 1; proc_idx < CHILD_NUM; ++proc_idx)
				waitpid(child_pids[proc_idx], NULL, 0);
		}
		write_log(4, "HCFS (fuse) shutting down normally\n");
		close_log();
		destroy_dirstat_lookup();
		destroy_pathlookup();

		break;
	/* children processed begin */
	case 1:
		open_log("cache_maintain.log");
		run_cache_loop();
		write_log(4, "HCFS (cache) shutting down normally\n");
		close_log();
		break;
	case 2:
		open_log("backend_upload.log");

		/* Init curl handle */
		sem_init(&download_curl_sem, 0,
				MAX_DOWNLOAD_CURL_HANDLE);
		sem_init(&download_curl_control_sem, 0, 1);
		for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE;
				count++)
			_init_download_curl(count);

		pthread_create(&delete_loop_thread, NULL, &delete_loop, NULL);
		pthread_create(&monitor_loop_thread, NULL, &monitor_loop, NULL);
		upload_loop();
		pthread_join(delete_loop_thread, NULL);
		pthread_join(monitor_loop_thread, NULL);
		write_log(4, "HCFS (sync) shutting down normally\n");
		close_log();
		break;
	}
#endif /* _ANDROID_ENV_ */
	sem_post(&(api_server->shutdown_sem));
	destroy_api_interface();
	return 0;
}
