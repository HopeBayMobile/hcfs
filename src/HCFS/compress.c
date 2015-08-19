#include "compress.h"

compress_func compress_f = LZ4_compress;
decompress_func decompress_f = LZ4_decompress_safe;


