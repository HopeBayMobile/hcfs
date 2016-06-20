#include "logger.h"
#include "super_block.h"

int32_t write_log(int32_t level, char *format, ...)
{
	return 0;
}

int32_t read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int32_t super_block_exclusive_locking(void)
{
	return 0;
}

int32_t super_block_exclusive_release(void)
{
	return 0;
}

int32_t write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	return 0;
}

int32_t super_block_init(void)
{
	return 0;
}
