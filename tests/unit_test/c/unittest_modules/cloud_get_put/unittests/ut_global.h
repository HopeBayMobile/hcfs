#include <stdio.h>

#ifdef UT_DEBUG
	#define DEBUG_PRINT(...) printf(__VA_ARGS__);
#else
	#define DEBUG_PRINT(...) do {} while(0);
#endif

