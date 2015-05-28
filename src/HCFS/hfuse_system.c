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
	int shm_key;

	shm_key = shmget(2345, sizeof(SYSTEM_DATA_HEAD), IPC_CREAT | 0666);
	hcfs_system = (SYSTEM_DATA_HEAD *) shmat(shm_key, NULL, 0);

	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 1, 1);
	sem_init(&(hcfs_system->num_cache_sleep_sem), 1, 0);
	sem_init(&(hcfs_system->check_cache_sem), 1, 0);
	sem_init(&(hcfs_system->check_next_sem), 1, 0);
	hcfs_system->system_going_down = FALSE;

	hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
	if (hcfs_system->system_val_fptr == NULL) {
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "w+");
		fwrite(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE),
					1, hcfs_system->system_val_fptr);
		fclose(hcfs_system->system_val_fptr);
		hcfs_system->system_val_fptr = fopen(HCFSSYSTEM, "r+");
	}
	setbuf(hcfs_system->system_val_fptr, NULL);
	fread(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
						hcfs_system->system_val_fptr);

	return 0;
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
	if (need_lock == TRUE)
		sem_wait(&(hcfs_system->access_sem));
	fseek(hcfs_system->system_val_fptr, 0, SEEK_SET);
	fwrite(&(hcfs_system->systemdata), sizeof(SYSTEM_DATA_TYPE), 1,
						hcfs_system->system_val_fptr);
	if (need_lock == TRUE)
		sem_post(&(hcfs_system->access_sem));

	return 0;
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
	char rootmetapath[METAPATHLEN];
	ino_t root_inode;
	struct stat this_stat;
	DIR_META_TYPE this_meta;
	DIR_ENTRY_PAGE temppage;
	mode_t self_mode;
	FILE *metafptr;

	ret_val = super_block_init();
	init_pathname_cache();
	ret_val = init_system_fh_table();
	init_hcfs_system_data();

	/* Check if need to initialize the root meta file */
	fetch_meta_path(rootmetapath, 1);
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

		this_stat.st_ino = 1;

		metafptr = fopen(rootmetapath, "w");

		ret_val = fwrite(&this_stat, sizeof(struct stat), 1, metafptr);


		ret_val = fwrite(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		this_meta.root_entry_page = ftell(metafptr);
		this_meta.tree_walk_list_head = this_meta.root_entry_page;
		fseek(metafptr, sizeof(struct stat), SEEK_SET);

		ret_val = fwrite(&this_meta, sizeof(DIR_META_TYPE), 1,
								metafptr);

		init_dir_page(&temppage, 1, 0, this_meta.root_entry_page);

		ret_val = fwrite(&temppage, sizeof(DIR_ENTRY_PAGE), 1,
								metafptr);
		fclose(metafptr);
		super_block_mark_dirty(1);
	}
	return 0;
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
	FILE *fptr;

	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();

/*TODO: Error handling after reading system config*/
/*TODO: Allow reading system config path from program arg */

/* TODO: Selection of backend type via configuration */

	read_system_config(DEFAULT_CONFIG_PATH);

	ret_val = validate_system_config();

	if (ret_val < 0)
		exit(-1);

	nofile_limit.rlim_cur = 150000;
	nofile_limit.rlim_max = 150001;

	ret_val = setrlimit(RLIMIT_NOFILE, &nofile_limit);
	sprintf(curl_handle.id, "main");
	ret_val = hcfs_init_backend(&curl_handle);
	if ((ret_val < 200) || (ret_val > 299)) {
		printf("error in connecting to backend\n");
		exit(0);
	}

	ret_val = hcfs_list_container(&curl_handle);
	if ((ret_val < 200) || (ret_val > 299)) {
		printf("error in connecting to backend\n");
		exit(0);
	}
	printf("ret code %d\n", ret_val);

	hcfs_destroy_backend(curl_handle.curl);

	init_hfuse();
	this_pid = fork();
	if (this_pid == 0) {
		logfptr = fopen("cache_maintain_log", "a+");
		setbuf(logfptr, NULL);
		fprintf(logfptr, "\nStart logging cache cleanup\n");
		printf("Redirecting to cache log\n");
		dup2(fileno(logfptr), fileno(stdout));
		dup2(fileno(logfptr), fileno(stderr));

		run_cache_loop();
		fclose(logfptr);
	} else {
		this_pid1 = fork();
		if (this_pid1 == 0) {
			logfptr = fopen("backend_upload_log", "a+");
			setbuf(logfptr, NULL);
			fprintf(logfptr, "\nStart logging backend upload\n");
			printf("Redirecting to backend log\n");
			dup2(fileno(logfptr), fileno(stdout));
			dup2(fileno(logfptr), fileno(stderr));
			pthread_create(&delete_loop_thread, NULL, &delete_loop,
									NULL);
			upload_loop();
			fclose(logfptr);
		} else {

			logfptr = fopen("fuse_log", "a+");
			setbuf(logfptr, NULL);
			fprintf(logfptr, "\nStart logging fuse\n");
			printf("Redirecting to fuse log\n");
			dup2(fileno(logfptr), fileno(stdout));
			dup2(fileno(logfptr), fileno(stderr));
			sem_init(&download_curl_sem, 0,
					MAX_DOWNLOAD_CURL_HANDLE);
			sem_init(&download_curl_control_sem, 0, 1);

			for (count = 0; count <	MAX_DOWNLOAD_CURL_HANDLE;
								count++)
				_init_download_curl(count);

			hook_fuse(argc, argv);
			printf("Waiting for subprocesses to terminate\n");
			waitpid(this_pid, NULL, 0);
			waitpid(this_pid1, NULL, 0);
			fclose(logfptr);
		}
	}
	return 0;
}