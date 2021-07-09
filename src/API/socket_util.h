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

#ifndef GW20_HCFSAPI_SOCKUTIL_H_
#define GW20_HCFSAPI_SOCKUTIL_H_

#include <inttypes.h>

int32_t get_hcfs_socket_conn();

int32_t reads(int32_t fd, void *buf, int32_t count);

int32_t sends(int32_t fd, const void *buf, int32_t count);

#endif /* GW20_HCFSAPI_SOCKUTIL_H_ */
