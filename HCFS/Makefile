CC = gcc
CFLAGS = -g -lfuse -lcurl -pthread -D_FILE_OFFSET_BITS=64

hcfs: hfuse_system.o utils.o superinode_ops.o hfuseops.o metaops.o fuse_dirops.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o
	$(CC) -g -o hcfs hfuse_system.o utils.o superinode_ops.o hfuseops.o metaops.o fuse_dirops.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o -lfuse -lcurl -pthread

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

metaops.o: metaops.c fuseop.h params.h
	$(CC) $(CFLAGS) -c metaops.c

filetables.o: filetables.c fuseop.h
	$(CC) $(CFLAGS) -c filetables.c

hcfscurl.o: hcfscurl.c hcfscurl.h
	$(CC) $(CFLAGS) -c hcfscurl.c

hcfs_tocloud.o: hcfs_tocloud.c hcfs_tocloud.h params.h hcfscurl.h super_inode.h fuseop.h
	$(CC) $(CFLAGS) -c hcfs_tocloud.c

hcfs_fromcloud.o: hcfs_fromcloud.c hcfscurl.h params.h fuseop.h
	$(CC) $(CFLAGS) -c hcfs_fromcloud.c

hcfs_cacheops.o: hcfs_cacheops.c hcfs_cache.h params.h fuseop.h super_inode.h
	$(CC) $(CFLAGS) -c hcfs_cacheops.c

clean:
	rm -rf *.o hcfs
