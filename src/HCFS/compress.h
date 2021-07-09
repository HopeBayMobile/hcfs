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

#ifndef GW20_HCFS_COMPRESS_H_
#define GW20_HCFS_COMPRESS_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "global.h"

#if ENABLE(COMPRESS)
#include <lz4.h>
#endif

typedef int32_t (*compress_func)(const char *source, char *dest, int32_t inputSize);

typedef int32_t (*decompress_func)(const char *source, char *dest, int32_t inputSize,
			       int32_t maxOutputSize);

typedef int32_t (*compress_bound_func)(int32_t inputSize);

extern compress_func compress_f;
extern decompress_func decompress_f;
extern compress_bound_func compress_bound_f;

FILE *transform_compress_fd(FILE *, uint8_t **);

int32_t decompress_to_fd(FILE *, uint8_t *, int32_t);
#endif
