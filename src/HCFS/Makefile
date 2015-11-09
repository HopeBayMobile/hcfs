CC = gcc
CPPFLAGS = -DDEDUP_ENABLE=0 -DENCRYPT_ENABLE=1 -DCOMPRESS_ENABLE=0 -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_
CFLAGS = -g -lfuse -lcurl -pthread -lssl -lcrypto -Wall -Wextra

hcfs: objmeta.o mount_manager.o FS_manager.o api_interface.o logger.o lookup_count.o file_present.o hfuse_system.o utils.o super_block.o fuseop.o metaops.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o hcfs_clouddelete.o hcfs_cachebuild.o b64encode.o meta_mem_cache.o dir_entry_btree.o xattr_ops.o enc.o compress.o dedup_table.o path_reconstruct.o pin_scheduling.o
	$(CC) -g -o hcfs mount_manager.o FS_manager.o api_interface.o logger.o lookup_count.o file_present.o hfuse_system.o utils.o super_block.o fuseop.o metaops.o filetables.o hcfscurl.o hcfs_tocloud.o hcfs_fromcloud.o hcfs_cacheops.o hcfs_clouddelete.o hcfs_cachebuild.o b64encode.o meta_mem_cache.o dir_entry_btree.o xattr_ops.o enc.o compress.o dedup_table.o objmeta.o path_reconstruct.o pin_scheduling.o -lfuse -lcurl -pthread -lssl -lcrypto

path_reconstruct.o: path_reconstruct.c path_reconstruct.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c path_reconstruct.c

mount_manager.o: mount_manager.c mount_manager.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c mount_manager.c

FS_manager.o: FS_manager.c FS_manager.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c FS_manager.c

dir_entry_btree.o: dir_entry_btree.c dir_entry_btree.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c dir_entry_btree.c

api_interface.o: api_interface.c api_interface.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c api_interface.c

logger.o: logger.c logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c logger.c

meta_mem_cache.o: meta_mem_cache.c global.h params.h meta_mem_cache.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c meta_mem_cache.c

file_present.o: file_present.c global.h file_present.h super_block.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c file_present.c

fuseop.o: fuseop.c fuseop.h super_block.h params.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c fuseop.c

super_block.o: super_block.c fuseop.h super_block.h params.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c super_block.c

utils.o: utils.c fuseop.h params.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c utils.c

hfuse_system.o: hfuse_system.c fuseop.h super_block.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hfuse_system.c

metaops.o: metaops.c fuseop.h params.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c metaops.c

filetables.o: filetables.c fuseop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c filetables.c

hcfscurl.o: hcfscurl.c hcfscurl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfscurl.c

hcfs_tocloud.o: hcfs_tocloud.c hcfs_tocloud.h params.h hcfscurl.h super_block.h fuseop.h 
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfs_tocloud.c

hcfs_fromcloud.o: hcfs_fromcloud.c hcfscurl.h params.h fuseop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfs_fromcloud.c

hcfs_cacheops.o: hcfs_cacheops.c hcfs_cacheops.h params.h fuseop.h super_block.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfs_cacheops.c

hcfs_cachebuild.o: hcfs_cachebuild.c hcfs_cachebuild.h params.h fuseop.h super_block.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfs_cachebuild.c

hcfs_clouddelete.o: hcfs_clouddelete.c hcfs_clouddelete.h hcfs_tocloud.h params.h hcfscurl.h super_block.h fuseop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c hcfs_clouddelete.c

lookup_count.o: lookup_count.c lookup_count.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c lookup_count.c

b64encode.o: b64encode.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c b64encode.c

xattr_ops.o: xattr_ops.c xattr_ops.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c xattr_ops.c

enc.o: enc.h enc.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c enc.c

compress.o: compress.h compress.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c compress.c

objmeta.o: objmeta.h objmeta.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c objmeta.c

dedup_table.o: dedup_table.c dedup_table.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c dedup_table.c

pin_scheduling.o: pin_scheduling.c pin_scheduling.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c pin_scheduling.c

clean:
	rm -rf *.o hcfs
