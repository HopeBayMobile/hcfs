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
