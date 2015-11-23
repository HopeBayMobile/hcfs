#include "super_block.h"
#include "pin_scheduling.h"

int super_block_share_locking(void)
{
	return 0;
}
int super_block_share_release(void)
{
	return 0;
}

int write_log(int level, char *format, ...)
{
	return 0;
}

int super_block_finish_pinning(ino_t this_inode)
{
	return 0;
}

int fetch_pinned_blocks(ino_t inode)
{
	return 0;
}
