CC = gcc
CFLAGS = -g -lfuse -lcurl -pthread -D_FILE_OFFSET_BITS=64

hcfs: hfuse_system.o utils.o superinode_ops.o hfuseops.o metaops.o fuse_dirops.o filetables.o
	$(CC) -g -o hcfs hfuse_system.o utils.o superinode_ops.o hfuseops.o metaops.o fuse_dirops.o filetables.o -lfuse -lcurl

fuse_dirops.o: fuse_dirops.c fuseop.h dir_lookup.h
	$(CC) $(CFLAGS) -c fuse_dirops.c

hfuseops.o: hfuseops.c fuseop.h dir_lookup.h super_inode.h params.h
	$(CC) $(CFLAGS) -c hfuseops.c

superinode_ops.o: superinode_ops.c fuseop.h super_inode.h params.h
	$(CC) $(CFLAGS) -c superinode_ops.c

utils.o: utils.c fuseop.h params.h
	$(CC) $(CFLAGS) -c utils.c

hfuse_system.o: hfuse_system.c fuseop.h super_inode.h dir_lookup.h
	$(CC) $(CFLAGS) -c hfuse_system.c

metaops.o: metaops.c fuseop.h
	$(CC) $(CFLAGS) -c metaops.c

filetables.o: filetables.c fuseop.h
	$(CC) $(CFLAGS) -c filetables.c

clean:
	rm -rf *.o hcfs
