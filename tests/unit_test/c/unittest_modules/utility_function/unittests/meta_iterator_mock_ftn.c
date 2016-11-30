#include <inttypes.h>
#include <stdlib.h>

#include "meta.h"

SYSTEM_CONF_STRUCT *system_config = NULL;
int32_t RETURN_PAGE_NOT_FOUND;

int32_t write_log(int32_t level, const char *format, ...)
{
	return 0;
}

int64_t seek_page2(FILE_META_TYPE *temp_meta, FILE *fptr,
		int64_t target_page, int64_t hint_page)
{
	if (RETURN_PAGE_NOT_FOUND)
		return 0;
	else
		/* Linear page arrangement */
		return sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			target_page * sizeof(BLOCK_ENTRY_PAGE);
}
