
#define FUSE_NOTIFY_QUEUE_DEFAULT_LEN 2

typedef struct {
	FUSE_LL_NOTIFY *notifies;
	int32_t size;
	int32_t start;
	int32_t end;
	/* By allowing index move around virtual space (twice of actuall
	 * space), we can tell the queue is full or empty from the offset
	 * between start and end. */
} NOTIFY_CYCLE_BUF;
