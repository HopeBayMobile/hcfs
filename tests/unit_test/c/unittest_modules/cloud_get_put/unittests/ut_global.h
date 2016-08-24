#include <stdio.h>

#ifdef UT_DEBUG
	#define DEBUG_PRINT(...) printf(__VA_ARGS__);
#else
	#define DEBUG_PRINT(...) do {} while(0);
#endif

#define BLKSIZE 4096
#define ROUND_SIZE(size)\
	((size) >= 0 ? (((size) + BLKSIZE - 1) & (~(BLKSIZE - 1))) :\
	-(((-(size)) + BLKSIZE - 1) & (~(BLKSIZE - 1))))

