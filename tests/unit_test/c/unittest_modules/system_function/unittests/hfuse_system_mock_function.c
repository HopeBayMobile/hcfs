/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hfuse_system_mock_function.c
* Abstract: 
*
* Revision History
* 2016/1/14 Jethro add copyright.
*
**************************************************************************/
#include "sys/stat.h"
#include "curl/curl.h"
#include "fuseop.h"
#include "hcfscurl.h"
#include "mock_params.h"

int32_t super_block_init(void)
{
	return 0;
}

int32_t init_pathname_cache(void)
{
	return 0;
}

int32_t init_system_fh_table(void)
{
	return 0;
}

int32_t fetch_meta_path(char *pathname, ino_t this_inode)
{
	if (this_inode == 1)
		strcpy(pathname, "/tmp/root_meta_path");
	return 0;
}

ino_t super_block_new_inode(HCFS_STAT *in_stat)
{
	return 1;
}

int32_t init_dir_page(DIR_ENTRY_PAGE *tpage, ino_t self_inode, ino_t parent_inode, 
                                                int64_t this_page_pos)
{
	return 0;
}

int32_t super_block_mark_dirty(ino_t this_inode)
{
	return 0;
}

int32_t hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	if (hcfs_init_backend_success == TRUE)
		return 200;
	else
		return 0;
}

void hcfs_destroy_backend(CURL_HANDLE *curl_handle)
{
}

void ENGINE_load_builtin_engines(void)
{

}

int32_t ENGINE_register_all_complete(void)
{
	return 0;
}

int32_t init_cache_thresholds(SYSTEM_CONF_STRUCT *config)
{
	return 0;
}

int32_t init_system_config_settings(const char *config_path,
                                    SYSTEM_CONF_STRUCT *config)
{
	return 0;
}

int32_t hcfs_list_container(CURL_HANDLE *curl_handle)
{
	if (hcfs_list_container_success == TRUE)
		return 200;
	else
		return 0;
}

void *delete_loop(void *arg)
{
	return NULL;
}
#ifdef _ANDROID_ENV_
void *upload_loop(void *ptr)
{
	return NULL;
}

void *run_cache_loop(void *ptr)
{
	return NULL;
}
void *monitor_loop(void *ptr)
{
	return NULL;
}
#else
void upload_loop(void)
{
	/* kill child process */
	exit(0);
}

void run_cache_loop(void)
{
	/* kill child process */
	exit(0);
}
void monitor_loop(void)
{
	exit(0);
}
#endif
int32_t hook_fuse(int32_t argc, char **argv)
{
	return 0;
}

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t open_log(char *filename)
{
	mknod(filename, 0700, 0);
	return 0;
}

int32_t close_log(void)
{
	return 0;
}
int32_t init_mount_mgr(void)
{
	return 0;
}
int32_t init_fs_manager(void)
{
	return 0;
}
int32_t init_pathlookup(void)
{
	return 0;
}
void destroy_pathlookup(void)
{
	return;
}
int32_t init_dirstat_lookup(void)
{
	return 0;
}
void destroy_dirstat_lookup(void)
{
}
void init_sync_stat_control(void)
{
}
void update_sync_state(void)
{
}
int32_t ignore_sigpipe(void)
{
	return 0;
}

int32_t update_quota(void)
{
	return 0;
}

int32_t get_quota_from_backup(int64_t *quota)
{
	*quota = 0;
	return 0;
}

int32_t init_pkg_cache(void)
{
	return 0;
}

int32_t destroy_pkg_cache(void)
{
	return 0;
}
int32_t destroy_api_interface(void)
{
	return 0;
}
void destroy_monitor_loop_thread(void)
{
	return;
}

void fetch_syncpoint_data_path(char *path)
{
	strcpy(path, "mock_syncpoint_data");
}

int32_t init_syncpoint_resource(void)
{
	return 0;
}

int32_t init_event_queue(void) { return 0; }
#ifdef _ANDROID_ENV_
void *event_worker_loop(void *ptr) {}
#else
void *event_worker_loop(void) {}
#endif
void destroy_event_worker_loop_thread(void) {}

int32_t check_and_create_metapaths(void)
{
	return 0;
}
int32_t check_and_create_blockpaths(void)
{
	return 0;
}

int32_t super_block_exclusive_locking(void)
{
	return 0;
}

int32_t super_block_exclusive_release(void)
{
	return 0;
}
int32_t fetch_restore_stat_path(char *pathname)
{
	return 0;
}
int32_t check_init_super_block()
{
	return 0;
}
void init_restore_path(void)
{
	snprintf(RESTORE_METAPATH, METAPATHLEN, "%s_restore",
	         METAPATH);
	snprintf(RESTORE_BLOCKPATH, BLOCKPATHLEN, "%s_restore",
	         BLOCKPATH);
	sem_init(&(restore_sem), 0, 1);
}

int32_t check_restoration_status(void)
{
	return NOT_RESTORING;
}

int32_t restore_stage1_reduce_cache(void)
{
	return 0;
}
int32_t notify_restoration_result(int8_t stage, int32_t result)
{
	return 0;
}
void start_download_minimal(void)
{
	return;
}

