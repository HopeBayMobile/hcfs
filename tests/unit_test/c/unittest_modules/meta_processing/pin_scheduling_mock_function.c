#include "super_block.h"
#include "pin_scheduling.h"
#include "mock_param.h"
#include "global.h"

int32_t super_block_share_locking(void)
{
	return 0;
}
int32_t super_block_share_release(void)
{
	return 0;
}

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;
	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

int32_t super_block_finish_pinning(ino_t this_inode)
{
	FINISH_PINNING = TRUE;
	sem_wait(&verified_inodes_sem);
	verified_inodes[verified_inodes_counter++] = this_inode;
	sys_super_block->head.num_pinning_inodes--;
	sem_post(&verified_inodes_sem);
	return 0;
}

int32_t fetch_pinned_blocks(ino_t inode)
{
	switch (inode) {
	case INO_PINNING_ENOSPC:
		return -ENOSPC;
	case INO_PINNING_EIO:
		return -EIO;
	case INO_PINNING_ENOENT:
		return -ENOENT;
	case INO_PINNING_ESHUTDOWN:
		return -ESHUTDOWN;
	}
	return 0;
}

int32_t super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	inode_ptr->pin_status = ST_PINNING;
	if (mock_inodes_counter == TOTAL_MOCK_INODES) {
		inode_ptr->pin_ll_next = 0;
		sys_super_block->head.first_pin_inode = 0;
	} else {
		inode_ptr->pin_ll_next = mock_inodes[mock_inodes_counter++];
	}

	return 0;
}

struct timespec UT_sleep;

void nonblock_sleep(uint32_t secs, BOOL (*wakeup_condition)(void))
{
	UT_sleep.tv_sec = 0;
	UT_sleep.tv_nsec = 99999999 ;
	nanosleep(&UT_sleep, NULL);
	return;
}
