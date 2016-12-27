/*************************************************************************
 * *
 * * Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
 * *
 * * File Name: compress.c
 * * Abstract: The c source code file for some compression helpers.
 * *
 * **************************************************************************/

#include "compress.h"
#include "utils.h"
#include "logger.h"
#include "macro.h"

#if ENABLE(COMPRESS)

/************************************************************************
 * *
 * * Function name: compress_f
 * *        Inputs: const char* source, char* dest, int32_t inputSize
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
 * *        Inputs: const char* source, char* dest, int32_t compressedSize, int32_t
 * *                maxDecompressedSize
 * *       Summary: compressedSize is the precise full size of the
 *                  compressed block.
 *                  maxDecompressedSize is the size of destination buffer, which
 * must be
 *                  already allocated.
 * *
 * *  Return value: the number of bytes decompressed into destination buffer
 *                  (necessarily <= maxDecompressedSize)
 *                  If destination buffer is not large enough, decoding will
 * stop
 *                  and output an error code (<0).
 *                  If the source stream is detected malformed, the
 *                  function will stop decoding and return a negative result.
 * *
 * *************************************************************************/
decompress_func decompress_f = LZ4_decompress_safe;

/************************************************************************
 * *
 * * Function name: compress_bound_f
 * *        Inputs: int32_t inputSize
 * *       Summary:  the maximum size that compression may output in a
 *                   "worst case" scenario (input data not compressible)
 *                   This function is primarily useful for memory allocation
 * purposes
 * *  Return value:  maximum output size or 0, if input size is too large
 * *
 * *************************************************************************/
compress_bound_func compress_bound_f = LZ4_compressBound;

#endif  /* ENABLE(COMPRESS) */

/************************************************************************
 * *
 * * Function name: transform_compress_fd
 * *        Inputs: FILE* in_fd, open with 'r' mode
 *		    uint8_t** data
 * *       Summary: compress content read from in_fd, and return a new fd
 *		    data must be free outside this function
 * *
 * *  Return value: File* or NULL if failed
 * *
 * *************************************************************************/
FILE *transform_compress_fd(FILE *in_fd, uint8_t **data)
{
#if ENABLE(COMPRESS)
	uint8_t *buf = calloc(MAX_BLOCK_SIZE, sizeof(uint8_t));

	if (buf == NULL) {
		write_log(
		    0, "Failed to allocate memory in transform_compress_fd\n");
		return NULL;
	}
	int32_t read_count =
	    fread(buf, sizeof(uint8_t), MAX_BLOCK_SIZE, in_fd);
	uint8_t *new_data =
	    calloc(compress_bound_f(read_count), sizeof(uint8_t));
	if (new_data == NULL) {
		free(buf);
		write_log(
		    0, "Failed to allocate memory in transform_compress_fd\n");
		return NULL;
	}
	int32_t ret = compress_f((char *)buf, (char *)new_data, read_count);

	if (ret == 0) {
		free(buf);
		write_log(1, "Failed to compress\n");
		return NULL;
	}
	free(buf);
	*data = new_data;
	write_log(10, "compress_size: %d\n", ret);
#if defined(__ANDROID__) || defined(_ANDROID_ENV_)
	FILE *tmp_file = tmpfile();
	if (tmp_file == NULL) {
		write_log(2, "tmpfile() failed to create tmpfile\n");
		return NULL;
	}
	fwrite(new_data, sizeof(uint8_t), ret, tmp_file);
	rewind(tmp_file);
	return tmp_file;
#else
	return fmemopen(new_data, ret, "rb");
#endif /* __ANDROID__ */

#else
	UNUSED(in_fd);
	UNUSED(data);
	return NULL;
#endif /* ENABLE(COMPRESS) */
}
/************************************************************************
 * *
 * * Function name: decompress_to_fd
 * *        Inputs: FILE* decompress_to_fd, open with 'w' mode
 *		    uint8_t* input
 *		    int32_t input_length
 * *       Summary: Decompress and write to decrypt_to_fd
 * *
 * *  Return value: 0 if success or 1 if failed
 * *
 * *************************************************************************/
int32_t decompress_to_fd(FILE *decompress_to_fd, uint8_t *input,
		     int32_t input_length)
{
#if ENABLE(COMPRESS)
	write_log(10, "decompress_size: %d\n", input_length);
	uint8_t *output =
	    (uint8_t *)calloc(MAX_BLOCK_SIZE, sizeof(uint8_t));

	int32_t ret = decompress_f((char *)input, (char *)output, input_length,
			       MAX_BLOCK_SIZE);

	if (ret < 0) {
		free(output);
		write_log(2, "Failed decompress. Code: %d\n", ret);
		return 1;
	}
	fwrite(output, sizeof(uint8_t), ret, decompress_to_fd);
	free(output);
	return 0;
#else
	UNUSED(decompress_to_fd);
	UNUSED(input);
	UNUSED(input_length);
	return 1;
#endif
}
