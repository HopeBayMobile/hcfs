#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "meta.h"
#include "xattr_ops.h"
int main(void)
{
	printf("DIR_META_HEADER_v0 %lu\n", sizeof(DIR_META_HEADER_v1));
	printf("DIR_META_HEADER %lu\n", sizeof(DIR_META_HEADER));
	printf("FILE_META_HEADER_v1 %lu\n", sizeof(FILE_META_HEADER_v1));
	printf("FILE_META_HEADER %lu\n", sizeof(FILE_META_HEADER));
	printf("SYMLINK_META_HEADER_v1 %lu\n", sizeof(SYMLINK_META_HEADER_v1));
	printf("SYMLINK_META_HEADER %lu\n", sizeof(SYMLINK_META_HEADER));

	printf("XATTR_PAGE %lu\n", sizeof(XATTR_PAGE));
	printf("PTR_ENTRY_PAGE %lu\n", sizeof(PTR_ENTRY_PAGE));
	printf("BLOCK_ENTRY_PAGE %lu\n", sizeof(BLOCK_ENTRY_PAGE));
	printf("META_MAGIC %lu\n", sizeof(META_MAGIC));
	printf("CLOUD_RELATED_DATA %lu\n", sizeof(CLOUD_RELATED_DATA));
	return 0;
}
