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
 * >| Key supported | Valid value |
 * | ------------- |:-------------|
 * | current_backend | none/swift|
 * | swift_account | account|
 * | swift_user    | user|
 * | swift_pass    | password|
 * | swift_url     | <server url>:<port>|
 * | swift_container| container name|
 * | swift_protocol| http/https|
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
 */
void HCFS_get_config(char **json_res, char *key);

/*Reload config
 * @json_res result string in json format.
 *
 * Reload HCFS configuration file. Backend can be changed from NONE to NONE/SWIFT/S3.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 *     quota: Bytes,
 *     vol_used: Bytes,
 *     cloud_used: Bytes,
 *     cache_total: Bytes,
 *     cache_used: Bytes,
 *     cache_dirty: Bytes,
 *     pin_max: Bytes,
 *     pin_total: Bytes,
 *     xfer_up: Bytes,
 *     xfer_down: Bytes,
 *     cloud_conn: True|False,
 *     data_transfer: Integer (0 means no data transfer, 1 means data transfer in progress, 2 means data transfer in progress but slow.)
 * }
 * ```
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | ENOSPC when pinned space is not available.|
 * | | Other linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if object is not pinned.|
 * | | 1 if object is pinned.|
 * | | 2 if object is high-priority-pinned.|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | ENOENT if the inode does not existed.|
 * | | Other linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if the file status is "local"|
 * | | 1 if the file status is "cloud"|
 * | | 2 if the file status is "hybrid"|
 * | False | Linux errors.|
 */
void HCFS_file_status(char **json_res, char *pathname);

/*Reset xfer
 * @json_res result string in json format.
 *
 * To reset the value of upload/download statistic.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0|
 * | False | Linux errors.|
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
 * >|||
 * | ------------- |:-------------|
 * | True | 0 when setting sync point completed|
 * | | 1 if there is no dirty data|
 * | False | Linux errors.|
 */
void HCFS_set_sync_point(char **json_res);

/*Clear Sync Point
 * @json_res result string in json format.
 *
 * To clear an existed sync point.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 when the sync point is removed successfully|
 * | | 1 if no sync point is set|
 * | False | Linux errors.|
 */
void HCFS_clear_sync_point(char **json_res);

/*Init Restoration
 * @json_res result string in json format.
 *
 * To initiate a restoration process.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if successful|
 * | False | Linux errors.|
 */
void HCFS_trigger_restore(char **json_res);

/*Check Restoration Status
 * @json_res result string in json format.
 *
 * To check the status of restoration process.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if not being restored|
 * | | 1 if in stage 1 of restoration process|
 * | | 2 if in stage 2 of restoration process|
 * | False | Linux errors.|
 */
void HCFS_check_restore_status(char **json_res);
#endif  /* GW20_HCFS_API_H_ */
