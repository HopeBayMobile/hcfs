#include "super_block.h"
#include "pin_scheduling.h"
#include "mock_param.h"
#include "global.h"

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
	FINISH_PINNING = TRUE;
	sem_wait(&verified_inodes_sem);
	verified_inodes[verified_inodes_counter++] = this_inode;
	sem_post(&verified_inodes_sem);
	return 0;
}

int fetch_pinned_blocks(ino_t inode)
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

int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr)
{
	if (mock_inodes_counter == TOTAL_MOCK_INODES) {
		inode_ptr->pin_ll_next = 0;
		sys_super_block->head.first_pin_inode = 0;
	} else {
		inode_ptr->pin_ll_next = mock_inodes[mock_inodes_counter++];
	}

	return 0;
}
