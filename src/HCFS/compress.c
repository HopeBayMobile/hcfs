#define _GNU_SOURCE
#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
compress_func compress_f = zlib_compress_func;
compress_func compress_f = LZ4_compress;
compress_func compress_f = lzma_compress_func;
decompress_func decompress_f = zlib_decompress_func;
decompress_func decompress_f = lzma_decompress_func;
*/

compress_func compress_f = LZ4_compress;
decompress_func decompress_f = LZ4_decompress_safe;


/*
int lzma_compress_func (const char* source, char* dest, int inputSize){
    uint32_t preset = 5; //COMPRESSION_LEVEL
    lzma_check check = LZMA_CHECK_CRC64;
    lzma_stream strm = LZMA_STREAM_INIT;

    lzma_ret ret_xz = lzma_easy_encoder(&strm, preset, check);
    if (ret_xz != LZMA_OK) {
        return -1;
    }

    strm.next_in = source;
    strm.avail_in = inputSize;
    lzma_action action = LZMA_FINISH;

    strm.next_out = dest;
    strm.avail_out = inputSize*2;
    ret_xz = lzma_code(&strm, action);
    lzma_end(&strm);
    return inputSize*2 - strm.avail_out;
}
int lzma_decompress_func (const char* source, char* dest, int inputSize, int maxOutputSize){
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret_xz = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED);
    if (ret_xz != LZMA_OK) {
        return -1;
    }

    strm.next_in = source;
    strm.avail_in = inputSize;
    lzma_action action = LZMA_FINISH;

    strm.next_out = dest;
    strm.avail_out = maxOutputSize;
    ret_xz = lzma_code(&strm, action);
    lzma_end(&strm);
    return maxOutputSize - strm.avail_out;
}
static
int zlib_compress_func (const char* source, char* dest, int inputSize){
    int32_t level = 5; // 0 to 9, -1 = 6 //
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, level);
    if(ret != Z_OK){
        return -1;
    }
    strm.avail_in = inputSize;
    int flush = Z_FINISH;
    strm.next_in = (char*)source;
    strm.next_out = dest;
    strm.avail_out = inputSize*2;
    ret = deflate(&strm, flush);
    deflateEnd(&strm);
    return inputSize*2 - strm.avail_out;
}

static
int zlib_decompress_func (const char* source, char* dest, int inputSize, int maxOutputSize){
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = inflateInit(&strm);
    if(ret != Z_OK){
        return -1;
    }
    strm.avail_in = inputSize;
    strm.next_in = (char*)source;
    strm.avail_out = maxOutputSize;
    strm.next_out = dest;
    ret = inflate(&strm, Z_NO_FLUSH);
    inflateEnd(&strm);
    return maxOutputSize - strm.avail_out;
}
*/
