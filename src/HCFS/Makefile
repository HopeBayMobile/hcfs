CC = gcc
CFLAGS = -g -lfuse -lcurl -pthread -D_FILE_OFFSET_BITS=64 -lssl -lcrypto

hcfs: file_present.o hfuse_system.o utils.o super_block.o hfuseops.o metaops.o dir_lookup.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o hcfs_clouddelete.o hcfs_cachebuild.o b64encode.o meta_mem_cache.o dir_entry_btree.o
	$(CC) -g -o hcfs file_present.o hfuse_system.o utils.o super_block.o hfuseops.o metaops.o dir_lookup.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o hcfs_clouddelete.o hcfs_cachebuild.o b64encode.o meta_mem_cache.o dir_entry_btree.o -lfuse -lcurl -pthread -lssl -lcrypto

dir_entry_btree.o: dir_entry_btree.c dir_entry_btree.h
	$(CC) $(CFLAGS) -c dir_entry_btree.c

meta_mem_cache.o: meta_mem_cache.c global.h params.h meta_mem_cache.h
	$(CC) $(CFLAGS) -c meta_mem_cache.c

file_present.o: file_present.c global.h file_present.h super_block.h
	$(CC) $(CFLAGS) -c file_present.c

dir_lookup.o: dir_lookup.c fuseop.h dir_lookup.h
	$(CC) $(CFLAGS) -c dir_lookup.c

hfuseops.o: hfuseops.c fuseop.h dir_lookup.h super_block.h params.h
	$(CC) $(CFLAGS) -c hfuseops.c

super_block.o: super_block.c fuseop.h super_block.h params.h
	$(CC) $(CFLAGS) -c super_block.c

utils.o: utils.c fuseop.h params.h
	$(CC) $(CFLAGS) -c utils.c

hfuse_system.o: hfuse_system.c fuseop.h super_block.h dir_lookup.h
	$(CC) $(CFLAGS) -c hfuse_system.c

metaops.o: metaops.c fuseop.h params.h
	$(CC) $(CFLAGS) -c metaops.c

filetables.o: filetables.c fuseop.h
	$(CC) $(CFLAGS) -c filetables.c

hcfscurl.o: hcfscurl.c hcfscurl.h
	$(CC) $(CFLAGS) -c hcfscurl.c

hcfs_tocloud.o: hcfs_tocloud.c hcfs_tocloud.h params.h hcfscurl.h super_block.h fuseop.h
	$(CC) $(CFLAGS) -c hcfs_tocloud.c

hcfs_fromcloud.o: hcfs_fromcloud.c hcfscurl.h params.h fuseop.h
	$(CC) $(CFLAGS) -c hcfs_fromcloud.c

hcfs_cacheops.o: hcfs_cacheops.c hcfs_cache.h params.h fuseop.h super_block.h
	$(CC) $(CFLAGS) -c hcfs_cacheops.c

hcfs_cachebuild.o: hcfs_cachebuild.c hcfs_cache.h params.h fuseop.h super_block.h
	$(CC) $(CFLAGS) -c hcfs_cachebuild.c

hcfs_clouddelete.o: hcfs_clouddelete.c hcfs_clouddelete.h hcfs_tocloud.h params.h hcfscurl.h super_block.h fuseop.h
	$(CC) $(CFLAGS) -c hcfs_clouddelete.c

b64encode.o: b64encode.c
	$(CC) $(CFLAGS) -c b64encode.c

clean:
	rm -rf *.o hcfs
