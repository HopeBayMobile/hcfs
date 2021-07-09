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
#ifndef GW20_HCFS_OBJDATA_H_
#define GW20_HCFS_OBJDATA_H_

#include "enc.h"

typedef struct {
	char **data;
	int32_t count;
} HTTP_meta;

HTTP_meta *new_http_meta(void);

void delete_http_meta(HTTP_meta *);

int32_t transform_objdata_to_header(HTTP_meta *meta,
				HCFS_encode_object_meta *encode_meta);

#endif
