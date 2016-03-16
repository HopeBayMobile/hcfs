#ifndef GW20_HCFS_SYS_H_
#define GW20_HCFS_SYS_H_

#define PASSPHRASE "lets encrypt configuration"


int32_t set_hcfs_config(char *arg_buf, uint32_t arg_len);

int32_t get_hcfs_config(char *arg_buf, uint32_t arg_len, char **value);

int32_t reload_hcfs_config();

int32_t toggle_cloud_sync(char *arg_buf, uint32_t arg_len);

int32_t get_sync_status();

int32_t reset_xfer_usage();

int32_t query_pkg_uid(char *arg_buf, uint32_t arg_len, char **uid);

#endif  /* GW20_HCFS_SYS_H_ */
