/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: compress.h
 * * Abstract: The header file for some compression helpers.
 * *
 * **************************************************************************/

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
