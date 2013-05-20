mycfs : mycfs.o system.o inode.o fuseop.o metautils.o datasync.o
	   gcc -o mycfs mycfs.o system.o inode.o fuseop.o metautils.o datasync.o -lfuse 

mycfs.o: mycfs.c myfuse.h
	   cc -c mycfs.c -D_FILE_OFFSET_BITS=64

system.o: system.c myfuse.h
	   cc -c system.c -D_FILE_OFFSET_BITS=64

inode.o : inode.c myfuse.h
	   cc -c inode.c -D_FILE_OFFSET_BITS=64

fuseop.o: fuseop.c myfuse.h
	   cc -c fuseop.c -D_FILE_OFFSET_BITS=64

metautils.o: metautils.c myfuse.h
	   cc -c metautils.c -D_FILE_OFFSET_BITS=64

datasync.o: datasync.c myfuse.h
	   cc -c datasync.c -D_FILE_OFFSET_BITS=64

clean:
	rm mycfs mycfs.o system.o inode.o fuseop.o metautils.o datasync.o
