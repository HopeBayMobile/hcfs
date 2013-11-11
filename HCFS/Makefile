CC = gcc
CFLAGS = -g -lfuse -lcurl -pthread -D_FILE_OFFSET_BITS=64

fuse_dirops.o: fuse_dirops.c fuseop.h dir_lookup.h
	$(CC) $(CFLAGS) -c fuse_dirops.c

hfuseops.o: hfuseops.c fuseop.h dir_lookup.h
	$(CC) $(CFLAGS) -c hfuseops.c
