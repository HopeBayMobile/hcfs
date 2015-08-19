#include <lz4.h>

typedef
int (*compress_func)(const char* source, char* dest, int inputSize);

typedef
int (*decompress_func)(const char* source, char* dest, int inputSize,
		       int maxOutputSize);

extern compress_func compress_f;
extern decompress_func decompress_f;

