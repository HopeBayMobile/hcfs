myfuse3 : myfuse3.o system.o inode.o fuseop.o metautils.o
	   gcc -o myfuse3 myfuse3.o system.o inode.o fuseop.o metautils.o -lfuse 

myfuse3.o: myfuse3.c myfuse.h
	   cc -c myfuse3.c -D_FILE_OFFSET_BITS=64

system.o: system.c myfuse.h
	   cc -c system.c -D_FILE_OFFSET_BITS=64

inode.o : inode.c myfuse.h
	   cc -c inode.c -D_FILE_OFFSET_BITS=64

fuseop.o: fuseop.c myfuse.h
	   cc -c fuseop.c -D_FILE_OFFSET_BITS=64

metautils.o: metautils.c myfuse.h
	   cc -c metautils.c -D_FILE_OFFSET_BITS=64

clean:
	rm myfuse3 myfuse3.o system.o inode.o fuseop.o metautils.o
