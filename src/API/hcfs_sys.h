/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GW20_HCFSAPI_SYS_H_
#define GW20_HCFSAPI_SYS_H_

#include <inttypes.h>

#define PASSPHRASE "lets encrypt configuration"

int32_t set_hcfs_config(char *arg_buf, uint32_t arg_len);

int32_t get_hcfs_config(char *arg_buf, uint32_t arg_len, char **value);

int32_t reload_hcfs_config();

int32_t toggle_cloud_sync(char *arg_buf, uint32_t arg_len);

int32_t get_sync_status();

int32_t reset_xfer_usage();

int32_t set_notify_server(char *arg_buf, uint32_t arg_len);

int32_t set_swift_access_token(char *arg_buf, uint32_t arg_len);

int32_t toggle_sync_point(int32_t api_code);

int32_t trigger_restore();

int32_t check_restore_status();

int32_t notify_applist_change();

int32_t collect_sys_logs();

int32_t retry_backend_conn();
#endif  /* GW20_HCFSAPI_SYS_H_ */
