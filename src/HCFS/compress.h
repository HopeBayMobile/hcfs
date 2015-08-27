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
#include <lz4.h>

#include "params.h"

extern SYSTEM_CONF_STRUCT system_config;

typedef
int (*compress_func)(const char* source, char* dest, int inputSize);

typedef
int (*decompress_func)(const char* source, char* dest, int inputSize,
		       int maxOutputSize);

typedef
int (*compress_bound_func)(int inputSize);

extern compress_func compress_f;
extern decompress_func decompress_f;
extern compress_bound_func compress_bound_f;

FILE *transform_compress_fd(FILE *, unsigned char **);

int decompress_to_fd(FILE *, unsigned char *, int);
#endif
