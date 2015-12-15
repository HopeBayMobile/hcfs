#include <stdarg.h>
#include <stdio.h>
#include "params.h"

SYSTEM_CONF_STRUCT *system_config;

int write_log(int level, char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
	return 0;
}
