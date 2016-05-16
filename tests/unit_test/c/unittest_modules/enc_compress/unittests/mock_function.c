#include <stdarg.h>
#include <stdio.h>
#include "params.h"

SYSTEM_CONF_STRUCT *system_config;

int32_t write_log(int32_t level, char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
	return 0;
}
