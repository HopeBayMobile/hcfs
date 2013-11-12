CC = gcc
CFLAGS = -g -lfuse -lcurl -pthread -D_FILE_OFFSET_BITS=64

fuse_dirops.o: fuse_dirops.c fuseop.h dir_lookup.h
	$(CC) $(CFLAGS) -c fuse_dirops.c

hfuseops.o: hfuseops.c fuseop.h dir_lookup.h super_inode.h params.h
	$(CC) $(CFLAGS) -c hfuseops.c

superinode_ops.o: superinode_ops.c fuseop.h super_inode.h params.h
	$(CC) $(CFLAGS) -c superinode_ops.c

utils.o: utils.c fuseop.h params.h
	$(CC) $(CFLAGS) -c utils.c
