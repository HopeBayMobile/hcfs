#ifndef GW20_PIN_API_H_
#define GW20_PIN_API_H_

#include<inttypes.h>

int32_t pin_by_path(char *buf, uint32_t arg_len);

int32_t unpin_by_path(char *buf, uint32_t arg_len);

int32_t check_pin_status(char *buf, uint32_t arg_len);

int32_t check_dir_status(char *buf, uint32_t arg_len,
			int64_t *num_local, int64_t *num_cloud,
			int64_t *num_hybrid);

int32_t check_file_loc(char *buf, uint32_t arg_len);

#endif  /* GW20_PIN_API_H_ */
