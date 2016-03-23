#ifndef GW20_HCFS_API_H_
#define GW20_HCFS_API_H_

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
 *     cloud_total: Bytes,
 *     cloud_used: Bytes,
 *     cache_total: Bytes,
 *     cache_used: Bytes,
 *     cache_dirty: Bytes,
 *     pin_max: Bytes,
 *     pin_total: Bytes,
 *     xfer_up: Bytes,
 *     xfer_down: Bytes,
 *     cloud_conn: True|False,
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

/*Set property
 * @json_res result string in json format.
 * @key property of HCFS configuration.
 * @value value for input key.
 *
 * To set property with value for HCFS.
 *
 * >| Key supported | Valid value |
 * | ------------- |:-------------|
 * | cloudsync     | on/off|
 * | clouddl       | on/off|
 */
//void HCFS_set_property(char **json_res, char *key, char *value);

/*Get property
 * @json_res result string in json format.
 * @key property of HCFS configuration.
 *
 * To get value of property for HCFS. (Supported keys are listed in <HCFS_set_property>.)
 */
//void HCFS_get_property(char **json_res, char *key);

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

/*Volume mount
 * @json_res result string in json format.
 * @vol_name target volume name.
 * @mpt target mountpoint.
 *
 * To mount HCFS volume.
 */
//void HCFS_vol_mount(char **json_res, char *vol_name, char *mpt);

/*Volume umount
 * @json_res result string in json format.
 * @vol_name target volume name.
 *
 * To unmount HCFS volume.
 */
//void HCFS_vol_umount(char **json_res, char *vol_name);

/*Pin file
 * @json_res result string in json format.
 * @pin_path a valid pathname (cloud be a file or a directory).
 *
 * Pin a file so that it will never be replaced when doing cache replacement.
 * If the given (pin_path) is a directory, HCFS_pin_path() will recursively
 * pin all files and files in subdirectories.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if successful.|
 * | | 1 if this path was already pinned.|
 * | False | ENOSPC when pinned space is not available.|
 * | | Other linux errors.|
 */
void HCFS_pin_path(char **json_res, char *pin_path);

/*Pin app
 * @json_res result string in json format.
 * @app_path /data/data/<package-name>
 * @data_path /data/app/<codePath>
 * @sd0_path /storage/sdcard0/Android/<type>/<package-name>
 * @sd1_path /storage/sdcard1/Android/<type>/<package-name>
 *
 * Pin all binaries and data files created by a app.
 * Input are four possible path where contain files belong to this app.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if successful.|
 * | | 1 if this app was already pinned.|
 * | False | ENOSPC when pinned space is not available.|
 * | | Other linux errors.|
 */
void HCFS_pin_app(char **json_res, char *app_path, char *data_path,
		  char *sd0_path, char *sd1_path);

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
 * | True | 0 if successful.|
 * | | 1 if this path was already unpinned.|
 * | False | Linux errors.|
 */
void HCFS_unpin_path(char **json_res, char *pin_path);

/*Unpin app
 * @json_res result string in json format.
 * @app_path /data/data/<package-name>
 * @data_path /data/app/<codePath>
 * @sd0_path /storage/sdcard0/Android/<type>/<package-name>
 * @sd1_path /storage/sdcard1/Android/<type>/<package-name>
 *
 * Unpin all binaries and data files created by a app.
 * Input are four possible path where contain files belong to this app.
 *
 * Return code -
 *
 * >|||
 * | ------------- |:-------------|
 * | True | 0 if successful.|
 * | | 1 if this app was already unpinned.|
 * | False | Linux errors.|
 */
void HCFS_unpin_app(char **json_res, char *app_path, char *data_path,
		    char *sd0_path, char *sd1_path);

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

#endif  /* GW20_HCFS_API_H_ */
