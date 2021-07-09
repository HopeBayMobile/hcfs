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

#ifndef GW20_HCFSAPI_PIN_H_
#define GW20_HCFSAPI_PIN_H_

#include <inttypes.h>

int32_t pin_by_path(char *buf, uint32_t arg_len);

int32_t unpin_by_path(char *buf, uint32_t arg_len);

int32_t check_pin_status(char *buf, uint32_t arg_len);

int32_t check_dir_status(char *buf, uint32_t arg_len,
			int64_t *num_local, int64_t *num_cloud,
			int64_t *num_hybrid);

int32_t check_file_loc(char *buf, uint32_t arg_len);

#endif  /* GW20_HCFSAPI_PIN_H_ */
