#include <lz4.h>
/*
#include <inttypes.h>
#include <lzma.h>
#include <zlib.h>
*/

typedef
int (*compress_func)(const char* source, char* dest, int inputSize);

typedef
int (*decompress_func)(const char* source, char* dest, int inputSize,
		       int maxOutputSize);

extern compress_func compress_f;
extern decompress_func decompress_f;

/*
static
int lzma_compress_func (const char* source, char* dest, int inputSize);

static
int lzma_decompress_func (const char* source, char* dest, int inputSize, int maxOutputSize);

static
int zlib_compress_func (const char* source, char* dest, int inputSize);

static
int zlib_decompress_func (const char* source, char* dest, int inputSize, int maxOutputSize);
*/
