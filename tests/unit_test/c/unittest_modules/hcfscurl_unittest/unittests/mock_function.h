#include "params.h"
#include "global.h"

SYSTEM_CONF_STRUCT *system_config;
char http_perform_retry_fail;
char write_auth_header_flag;
char write_list_header_flag;

char swift_destroy;
char s3_destroy;

char let_retry;

#undef b64encode_str
int32_t b64encode_str(uint8_t *inputstr,
		      char *outputstr,
		      int32_t *outlen,
		      int32_t inputlen);
void update_backend_status(int32_t status, struct timespec *status_time);
int32_t change_xfer_meta(int64_t xfer_size_upload,
			 int64_t xfer_size_download,
			 int64_t xfer_throughtput,
			 int64_t xfer_total_obj);
int32_t ignore_sigpipe(void);
