#include <stdio.h>
#include <stdlib.h>
#include <attr/xattr.h>
#include <inttypes.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

