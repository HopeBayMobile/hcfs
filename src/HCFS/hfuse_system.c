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
#include "hfuse_system.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>

#include "fuseop.h"
#include "meta_mem_cache.h"
#include "global.h"
#include "super_block.h"
#include "dir_lookup.h"
#include "hcfscurl.h"
#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "hcfs_cacheops.h"
#include "params.h"
#include "utils.h"
#include "filetables.h"
#include "macro.h"
#include "logger.h"

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
	int shm_key, errcode;
	size_t ret_size;

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

	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 1, 1);
	sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0);
	sem_init(&(hcfs_system->check_cache_sem), 1, 0);
	sem_init(&(hcfs_system->check_next_sem), 1, 0);
	hcfs_system->system_going_down = FALSE;

	hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
	if (hcfs_system->system_val_fptr == NULL) {
		errcode = errno;
		if (errcode != -ENOENT) {
			printf("Error reading system file. Code %d, %s\n",
				errcode, strerror(errcode));
			return -errcode;
		}
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "w+");
		if (hcfs_system->system_val_fptr == NULL) {
			errcode = errno;
			printf("Error reading system file. Code %d, %s\n",
				errcode, strerror(errcode));
			return -errcode;
		}

		FWRITE(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE),
					1, hcfs_system->system_val_fptr);
		fclose(hcfs_system->system_val_fptr);
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
		if (hcfs_system->system_val_fptr == NULL) {
			errcode = errno;
			printf("Error reading system file. Code %d, %s\n",
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
	int ret_val, ret, errcode;
	size_t ret_size;
	long ret_pos;
	char rootmetapath[METAPATHLEN];
	ino_t root_inode;
	struct stat this_stat;
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	mode_t self_mode;
	FILE *metafptr;

	ret_val = super_block_init();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_pathname_cache();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_system_fh_table();
	if (ret_val < 0)
		return ret_val;
	ret_val = init_hcfs_system_data();
	if (ret_val < 0)
		return ret_val;

	/* Check if need to initialize the root meta file */
	ret_val = fetch_meta_path(rootmetapath, 1);
	if (ret_val < 0)
		return ret_val;

	if (access(rootmetapath, F_OK) != 0) {
		memset(&this_stat, 0, sizeof(struct stat));
		memset(&this_meta, 0, sizeof(DIR_META_TYPE));
		memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

		self_mode = S_IFDIR | 0777;
		this_stat.st_mode = self_mode;

		/*One pointed by the parent, another by self*/
		this_stat.st_nlink = 2;
		this_stat.st_uid = getuid();
		this_stat.st_gid = getgid();
		this_stat.st_atime = time(NULL);
		this_stat.st_mtime = this_stat.st_atime;
		this_stat.st_ctime = this_stat.st_ctime;

		root_inode = super_block_new_inode(&this_stat, NULL);
		/*TODO: put error handling here if root_inode is not 1
					(cannot initialize system)*/
		if (root_inode != 1) {
			printf("Error initializing system\n");
			return -EPERM;
		}

		this_stat.st_ino = 1;

		metafptr = fopen(rootmetapath, "w");
		if (metafptr == NULL) {
			printf("IO error in initializing system\n");
			return -EIO;
		}

		FWRITE(&this_stat, sizeof(struct stat), 1, metafptr);


		FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		FTELL(metafptr);
		this_meta.root_entry_page = ret_pos;
		this_meta.tree_walk_list_head = this_meta.root_entry_page;
		FSEEK(metafptr, sizeof(struct stat), SEEK_SET);

		FWRITE(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		ret = init_dir_page(&temppage, 1, 0,
					this_meta.root_entry_page);
		if (ret < 0) {
			fclose(metafptr);
			return ret;
		}

		FWRITE(&temppage, sizeof(DIR_ENTRY_PAGE), 1,
								metafptr);
		fclose(metafptr);
		ret = super_block_mark_dirty(1);
		if (ret < 0)
			return ret;
	}
	return 0;

errcode_handle:
	if (metafptr != NULL)
		fclose(metafptr);
	return errcode;
}

/* Helper function to initialize curl handles for downloading objects */
int _init_download_curl(int count)
{
	int ret_val;

	curl_handle_mask[count] = FALSE;
	ret_val = hcfs_init_backend(&(download_curl_handles[count]));

	while ((ret_val < 200) || (ret_val > 299)) {
		printf("error in connecting to backend\n");
		if (download_curl_handles[count].curl != NULL)
			hcfs_destroy_backend(download_curl_handles[count].curl);
		ret_val = hcfs_init_backend(&(download_curl_handles[count]));
	}
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
	pid_t this_pid, this_pid1;
	int count;
	struct rlimit nofile_limit;
	pthread_t delete_loop_thread;

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
		printf("Error in setting open file limits\n");
/*
		exit(-1);
*/
	}
	sprintf(curl_handle.id, "main");
	ret_val = hcfs_init_backend(&curl_handle);
	if ((ret_val < 200) || (ret_val > 299)) {
		printf("error in connecting to backend. Code %d\n", ret_val);
		printf("Backend %d\n", CURRENT_BACKEND);
		exit(-1);
	}

	ret_val = hcfs_list_container(&curl_handle);
	if ((ret_val < 200) || (ret_val > 299)) {
		printf("error in connecting to backend\n");
		exit(-1);
	}
	printf("ret code %d\n", ret_val);

	hcfs_destroy_backend(curl_handle.curl);

	ret_val = init_hfuse();
	if (ret_val < 0)
		exit(-1);

	/* TODO: error handling for log files */
	this_pid = fork();
	if (this_pid == 0) {
		open_log("cache_maintain_log");
		write_log(0, "\nStart logging cache cleanup\n");

		run_cache_loop();
		close_log();
	} else {
		this_pid1 = fork();
		if (this_pid1 == 0) {
			open_log("backend_upload_log");
			write_log(0, "\nStart logging backend upload\n");
			pthread_create(&delete_loop_thread, NULL, &delete_loop,
									NULL);
			upload_loop();
			close_log();
		} else {

			open_log("fuse_log");
			write_log(0, "\nStart logging fuse\n");
			sem_init(&download_curl_sem, 0,
					MAX_DOWNLOAD_CURL_HANDLE);
			sem_init(&download_curl_control_sem, 0, 1);

			for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE;
								count++)
				_init_download_curl(count);

			hook_fuse(argc, argv);
			write_log(0, "Waiting for subprocesses to terminate\n");
			waitpid(this_pid, NULL, 0);
			waitpid(this_pid1, NULL, 0);
			close_log();
		}
	}
	return 0;
}
