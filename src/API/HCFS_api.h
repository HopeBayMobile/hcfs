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
 * | swift_account | account|
 * | swift_user    | user|
 * | swift_pass    | password|
 * | swift_url     | <server url>:<port>|
 * | swift_container| container name|
 * | swift_protocol| http/https|
 */
void HCFS_set_config(char *json_res, char *key, char *value);

/*Get config
 * @json_res result string in json format.
 * @key field in HCFS configuration.
 *
 * Get the Value of specific field. (Supported keys are listed in <HCFS_set_config>.)
 */
void HCFS_get_config(char *json_res, char *key);

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
 *     cache_dirty_used: Bytes,
 *     cache_clean_used: Bytes,
 *     pin_total: Bytes,
 *     pin_used: Bytes,
 *     bw_upload: Bytes,
 *     bw_download: Bytes,
 *     cloud_conn: True|False,
 * }
 * ```
 */
void HCFS_stat(char *json_res);

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
void HCFS_set_property(char *json_res, char *key, char *value);

/*Get property
 * @json_res result string in json format.
 * @key property of HCFS configuration.
 *
 * To get value of property for HCFS. (Supported keys are listed in <HCFS_set_property>.)
 */
void HCFS_get_property(char *json_res, char *key);

/*Volume mount
 * @json_res result string in json format.
 * @vol_name target volume name.
 * @mpt target mountpoint.
 *
 * To mount HCFS volume.
 */
void HCFS_vol_mount(char *json_res, char *vol_name, char *mpt);

/*Volume umount
 * @json_res result string in json format.
 * @vol_name target volume name.
 *
 * To unmount HCFS volume.
 */
void HCFS_vol_umount(char *json_res, char *vol_name);

/*Pin file
 * @json_res result string in json format.
 * @pin_path a valid pathname (cloud be a file or a directory).
 *
 * Pin a file so that it will never be replaced when doing cache replacement.
 * If the given (pin_path) is a directory, HCFS_pin_path() will recursively
 * pin all files and files in subdirectories.
 */
void HCFS_pin_path(char *json_res, char *pin_path);

/*Pin app
 * @json_res result string in json format.
 * @app_path /data/data/<package-name>
 * @data_path /data/app/<codePath>
 * @sd0_path /storage/sdcard0/Android/<type>/<package-name>
 * @sd1_path /storage/sdcard1/Android/<type>/<package-name>
 *
 * Pin all binaries and data files created by a app.
 * Input are four possible path where contain files belong to this app.
 */
void HCFS_pin_app(char *json_res, char *app_path, char *data_path,
		  char *sd0_path, char *sd1_path);

/*Unpin file
 * @json_res result string in json format.
 * @pin_path a valid pathname (cloud be a file or a directory).
 *
 * Unpin a file. If the given (pin_path) is a directory,
 * HCFS_unpin_path() will recursively unpin all files and
 * files in subdirectories.
 */
void HCFS_unpin_path(char *json_res, char *pin_path);

/*Unpin app
 * @json_res result string in json format.
 * @app_path /data/data/<package-name>
 * @data_path /data/app/<codePath>
 * @sd0_path /storage/sdcard0/Android/<type>/<package-name>
 * @sd1_path /storage/sdcard1/Android/<type>/<package-name>
 *
 * Unpin all binaries and data files created by a app.
 * Input are four possible path where contain files belong to this app.
 */
void HCFS_unpin_app(char *json_res, char *app_path, char *data_path,
		    char *sd0_path, char *sd1_path);

/*Pin status
 * @json_res result string in json format.
 * @pathname target pathname.
 *
 * To check a given (pathname) is pinned or unpinned.
 */
void HCFS_pin_status(char *json_res, char *pathname);

/*File status
 * @json_res result string in json format.
 * @pathname target pathname.
 *
 * To get the status of (pathname).
 */
void HCFS_file_status(char *json_res, char *pathname);


#endif  /* GW20_HCFS_API_H_ */
