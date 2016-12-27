#include <inttypes.h>
#include "utils.h"

SYSTEM_CONF_STRUCT *system_config;

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int32_t add_notify_event(int32_t event_id, char *event_info_json_str,
		char blocking)
{
	return 0;
}
