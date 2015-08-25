/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: compress.c
 * * Abstract: The c source code file for some compression helpers.
 * *
 * **************************************************************************/

#include "compress.h"

/************************************************************************
 * *
 * * Function name: compress_f
 * *        Inputs: const char* source, char* dest, int inputSize
 *                  dest should be allocated as large as
 *                  compress_bound_f(inputSize)
 * *       Summary: Run compress on source and write to dest
 * *
 * *  Return value: the number of bytes written into buffer 'dest' (necessarily
 *                  <= maxOutputSize) or 0 if compression fails
 *
 * *************************************************************************/
compress_func compress_f = LZ4_compress;


/************************************************************************
 * *
 * * Function name: decompress_f
 * *        Inputs: const char* source, char* dest, int compressedSize, int
 * *                maxDecompressedSize
 * *       Summary: compressedSize is the precise full size of the
 *                  compressed block.
 *                  maxDecompressedSize is the size of destination buffer, which must be
 *                  already allocated.
 * *
 * *  Return value: the number of bytes decompressed into destination buffer
 *                  (necessarily <= maxDecompressedSize)
 *                  If destination buffer is not large enough, decoding will stop
 *                  and output an error code (<0).
 *                  If the source stream is detected malformed, the
 *                  function will stop decoding and return a negative result.
 * *
 * *************************************************************************/
decompress_func decompress_f = LZ4_decompress_safe;


/************************************************************************
 * *
 * * Function name: compress_bound_f
 * *        Inputs: int inputSize
 * *       Summary:  the maximum size that compression may output in a
 *                   "worst case" scenario (input data not compressible)
 *                   This function is primarily useful for memory allocation purposes
 * *  Return value:  maximum output size or 0, if input size is too large
 * *
 * *************************************************************************/
compress_bound_func compress_bound_f = LZ4_compressBound;
