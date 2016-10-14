#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "global.h"

#define ENABLE_PRINT    0

int32_t write_log(int32_t level, const char *format, ...)
{
#if (ENABLE_PRINT == 1)
	va_list alist;
	
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
#endif
	return 0;
}
