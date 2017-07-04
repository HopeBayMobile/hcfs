/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: HCFS_api.h
* Abstract: This c header file for entry point of HCFSAPI.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#ifndef GW20_HCFS_API_H_
#define GW20_HCFS_API_H_

#include <inttypes.h>

/*Set config
 * @json_res result string in json format.
 * @key field in HCFS configuration.
 * @value value for input key.
 *
 * To set value of a specific field in HCFS configuration.
 *
 * > | Key supported   | Valid value         |
 *   | -------------   | :-------------      |
 *   | current_backend | none/swift          |
 *   | swift_account   | account             |
 *   | swift_user      | user                |
 *   | swift_pass      | password            |
 *   | swift_url       | <server url>:<port> |
 *   | swift_container | container name      |
 *   | swift_protocol  | http/https          |
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_set_config(char **json_res, char *key, char *value);

/*Get config
 * @json_res result string in json format.
 * @key field in HCFS configuration.
 *
 * Get the Value of specific field. (Supported keys are listed in <HCFS_set_config>.)
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_get_config(char **json_res, char *key);

/*Reload config
 * @json_res result string in json format.
 *
 * Reload HCFS configuration file. Backend can be changed from NONE to NONE/SWIFT/S3.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_reload_config(char **json_res);

/*Statistic
 * @json_res result string in json format.
 *
 * To fetch HCFS sytem statistics.
 *
 * Return data dict in json_res -
 * ```json
 * data: {
 *     quota:         Bytes,
 *     vol_used:      Bytes,
 *     cloud_used:    Bytes,
 *     cache_total:   Bytes,
 *     cache_used:    Bytes,
 *     cache_dirty:   Bytes,
 *     pin_max:       Bytes,
 *     pin_total:     Bytes,
 *     xfer_up:       Bytes,
 *     xfer_down:     Bytes,
 *     cloud_conn:    Bytes, (1 means conn, 0 means disconn, and 2 means retrying)
 *     data_transfer: Integer (0 means no data transfer, 1 means data transfer in progress, 2 means data transfer in progress but slow.)
 * }
 * ```
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_stat(char **json_res);

/*Statistic
 * @json_res result string in json format.
 *
 * To fetch the value of occupied size (Unpin-but-dirty size + pin size).
 *
 * Return data dict in json_res -
 * ```json
 * data: {
 *     occupied: Bytes,
 * }
 * ```
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_get_occupied_size(char **json_res);

/*Toggle sync
 * @json_res result string in json format.
 * @enabled 1 to turn on sync, 0 to turn off sync.
 *
 * To toggle if hcfs can/can't sync data in local cache to cloud storage.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_toggle_sync(char **json_res, int32_t enabled);

/*Sync status
 * @json_res result string in json format.
 *
 * To get status of cloud sync.
 *
 * Return data dict in json_res -
 * ```json
 * data: {
 *     enabled: boolean,
 * }
 * ```
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_get_sync_status(char **json_res);

/*Pin file
 * @json_res result string in json format.
 * @pin_path a valid pathname (cloud be a file or a directory).
 * @pin_type 1 for pin, 2 for high-priority-pin.
 *
 * Pin a file so that it will never be replaced when doing cache replacement.
 * If the given (pin_path) is a directory, HCFS_pin_path() will recursively
 * pin all files and files in subdirectories.
 *
 * Return code -
 *
 * > |               |                                            |
 *   | ------------- | :-------------                             |
 *   | True          | 0                                          |
 *   | False         | ENOSPC when pinned space is not available. |
 *   |               | Other linux errors.                        |
 */
void HCFS_pin_path(char **json_res, char *pin_path, char pin_type);

/*Unpin file
 * @json_res result string in json format.
 * @pin_path a valid pathname (cloud be a file or a directory).
 *
 * Unpin a file. If the given (pin_path) is a directory,
 * HCFS_unpin_path() will recursively unpin all files and
 * files in subdirectories.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_unpin_path(char **json_res, char *pin_path);

/*Pin status
 * @json_res result string in json format.
 * @pathname target pathname.
 *
 * To check a given (pathname) is pinned or unpinned.
 *
 * Return code -
 *
 * > |               |                                      |
 *   | ------------- | :-------------                       |
 *   | True          | 0 if object is not pinned.           |
 *   |               | 1 if object is pinned.               |
 *   |               | 2 if object is high-priority-pinned. |
 *   | False         | Linux errors.                        |
 */
void HCFS_pin_status(char **json_res, char *pathname);

/*Dir status
 * @json_res result string in json format.
 * @pathname target pathname.
 *
 * To get the status of (pathname).
 *
 * Return data dict in json_res -
 * ```json
 * data: {
 *     num_local: num,
 *     num_cloud: num,
 *     num_hybrid: num,
 * }
 * ```
 *
 * Return code -
 *
 * > |               |                                       |
 *   | ------------- | :-------------                        |
 *   | True          | 0                                     |
 *   | False         | ENOENT if the inode does not existed. |
 *   |               | Other linux errors.                   |
 */
void HCFS_dir_status(char **json_res, char *pathname);

/*File status
 * @json_res result string in json format.
 * @pathname target pathname.
 *
 * To get the status of (pathname).
 *
 * Return code -
 *
 * > |               |                                  |
 *   | ------------- | :-------------                   |
 *   | True          | 0 if the file status is "local"  |
 *   |               | 1 if the file status is "cloud"  |
 *   |               | 2 if the file status is "hybrid" |
 *   | False         | Linux errors.                    |
 */
void HCFS_file_status(char **json_res, char *pathname);

/*Reset xfer
 * @json_res result string in json format.
 *
 * To reset the value of upload/download statistic.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_reset_xfer(char **json_res);

/*Set Notify Server
 * @json_res result string in json format.
 * @path location of notify server
 *
 * To set the location of event notify server.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_set_notify_server(char **json_res, char *path);

/*Set Swift Token
 * @json_res result string in json format.
 * @url swift storage url(container name not included).
 * @token swift auth token
 *
 * To set the value of storage url and access token of swift.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_set_swift_token(char **json_res, char *url, char *token);

/*Set Sync Point
 * @json_res result string in json format.
 *
 * To set the sync point so that hcfs will send a notification if all
 * dirty data generated before this point are synchronized.
 *
 * Return code -
 *
 * > |               |                                     |
 *   | ------------- | :-------------                      |
 *   | True          | 0 when setting sync point completed |
 *   |               | 1 if there is no dirty data         |
 *   | False         | Linux errors.                       |
 */
void HCFS_set_sync_point(char **json_res);

/*Clear Sync Point
 * @json_res result string in json format.
 *
 * To clear an existed sync point.
 *
 * Return code -
 *
 * > |               |                                               |
 *   | ------------- | :-------------                                |
 *   | True          | 0 when the sync point is removed successfully |
 *   |               | 1 if no sync point is set                     |
 *   | False         | Linux errors.                                 |
 */
void HCFS_clear_sync_point(char **json_res);

/*Collect System logs
 * @json_res result string in json format.
 *
 * To collect system logs (hcfs log, logcat and dmesg)
 * for user feedback. This API will copy/dump all logs
 * to the dir "/sdcard/TeraLog/logs".
 *
 * Return code -
 *
 * > |               |                 |
 *   | ------------- | :-------------  |
 *   | True          | 0 if successful |
 *   | False         | Linux errors.   |
 */
void HCFS_collect_sys_logs(char **json_res);

/*Init Restoration
 * @json_res result string in json format.
 *
 * To initiate a restoration process.
 *
 * Return code -
 *
 * > |               |                 |
 *   | ------------- | :-------------  |
 *   | True          | 0 if successful |
 *   | False         | Linux errors.   |
 */
void HCFS_trigger_restore(char **json_res);

/*Check Restoration Status
 * @json_res result string in json format.
 *
 * To check the status of restoration process.
 *
 * Return code -
 *
 * > |               |                                        |
 *   | ------------- | :-------------                         |
 *   | True          | 0 if not being restored                |
 *   |               | 1 if in stage 1 of restoration process |
 *   |               | 2 if in stage 2 of restoration process |
 *   | False         | Linux errors.                          |
 */
void HCFS_check_restore_status(char **json_res);

/*Notify Applist Change
 * @json_res result string in json format.
 *
 * To inform HCFS that package lists in packages.xml
 * has changed and needs to be backed-up.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0              |
 *   | False         | Linux errors.  |
 */
void HCFS_notify_applist_change(char **json_res);

/*Check Package Boost Status
 * @json_res result string in json format.
 * @package_name target package
 *
 * To check if an installed package is boosted or not.
 *
 * Return code -
 *
 * > |               |                           |
 *   | ------------- | :-------------            |
 *   | True          | 0 if package is boosted   |
 *   |               | 1 if package is unboosted |
 *   | False         | Linux errors.             |
 */
void HCFS_check_package_boost_status(char **json_res, char *package_name);

/*Enable Booster
 * @json_res result string in json format.
 * @size initial size of smart cache.
 *
 * To setup environment of smart cache. This API will create img file, setup
 * loop device, process ext4 mkfs and mount.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_enable_booster(char **json_res, int64_t size);

/*Disable Booster
 * @json_res result string in json format.
 *
 * To destroy environment of smart cache. The existed img file for smart cache
 * will be deleted.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_disable_booster(char **json_res);

/*Trigger Boost
 * @json_res result string in json format.
 *
 * To move all packages selected from /data/data to smart cache. The list of
 * packages will stored in database maintained by Tera Mgmt APP. Note - This API
 * will return immediately when receiving API request. HCFSAPID will send an
 * event to notify the result after "boost" finished/failed.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_trigger_boost(char **json_res);

/*Trigger Unboost
 * @json_res result string in json format.
 *
 * To move all packages selected from smart cache to /data/data. The list of
 * packages will stored in database maintained by Tera Mgmt APP. Note - This API
 * will return immediately when receiving API request. HCFSAPID will send an
 * event to notify the result after "unboost" finished/failed.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_trigger_unboost(char **json_res);

/*Clear Booster package
 * @json_res result string in json format.
 * @package_name target package
 *
 * To clear data related to (package_name) in smart cache.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_clear_booster_package_remaining(char **json_res, char *package_name);

/*Mount smart cache
 * @json_res result string in json format.
 *
 * To mount smart cache.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_mount_smart_cache(char **json_res);

/*Umount smart cache
 * @json_res result string in json format.
 *
 * To umount smart cache.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_umount_smart_cache(char **json_res);

/*Create minimal apk
 * @json_res result string in json format.
 * @package_name target package name (e.g. com.xxx.xxx-1)
 * @blocking To determine if this API will return immediately or return
 *           until minimal apk was created. Passing TRUE(1) for blocking.
 *
 * To create minimal apk for target package.
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_create_minimal_apk(char **json_res,
			     char *package_name,
			     int32_t blocking,
			     int num_icon,
			     char *icon_name_list);

/*Check minimal apk
 * @json_res result string in json format.
 * @package_name target package name (e.g. com.xxx.xxx-1)
 *
 * To check whether the minimal apk of target package is existed or not.
 *
 * Return code -
 *
 * > |               |                               |
 *   | ------------- | :-------------                |
 *   | True          | 0 if not existed.             |
 *   |               | 1 if existed.                 |
 *   |               | 2 if minimal apk is creating. |
 *   | False         | Linux errors.                 |
 */
void HCFS_check_minimal_apk(char **json_res, char *package_name);

/*Retry to connect to backend
 * @json_res result string in json format.
 *
 * Immediately retry to connect to backend
 *
 * Return code -
 *
 * > |               |                |
 *   | ------------- | :------------- |
 *   | True          | 0 if success   |
 *   | False         | Linux errors.  |
 */
void HCFS_retry_conn(char **json_res);

#endif  /* GW20_HCFS_API_H_ */
