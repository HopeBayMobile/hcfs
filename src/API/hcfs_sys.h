#ifndef GW20_HCFS_SYS_H_
#define GW20_HCFS_SYS_H_

#define PASSPHRASE "lets encrypt configuration"


int encrypt_config(unsigned char *output, unsigned char *input,
		   long input_len);

FILE *get_decrypt_configfp();

int set_hcfs_config(char *arg_buf, unsigned int arg_len);

int get_hcfs_config(char *arg_buf, unsigned int arg_len, char **value);

int reload_hcfs_config();

int toggle_cloud_sync(char *arg_buf, unsigned int arg_len);

int get_sync_status();

int reset_xfer_usage();

int query_pkg_uid(char *arg_buf, unsigned int arg_len, char **uid);

#endif  /* GW20_HCFS_SYS_H_ */
