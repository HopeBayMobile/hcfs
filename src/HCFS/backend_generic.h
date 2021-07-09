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

#ifndef GW20_HCFS_BACKEND_OPERATION_H_
#define GW20_HCFS_BACKEND_OPERATION_H_

#include "hcfs_tocloud.h"

typedef struct {
	int32_t (*fill_object_info)(GOOGLEDRIVE_OBJ_INFO *obj_info,
				    char *objname,
				    char *objectID);
	int32_t (*download_fill_object_info)(GOOGLEDRIVE_OBJ_INFO *obj_info,
					  char *objname,
					  char *objectID);
	int32_t (*get_pkglist_id)(char *id);
	int32_t (*record_pkglist_id)(const char *id);
} BACKEND_OPERATION;

BACKEND_OPERATION backend_ops;

int32_t init_backend_ops(int32_t backend_type);

#endif
