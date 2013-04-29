myfuse2 : myfuse2.o system.o inode.o fuseop.o
	   gcc -o myfuse2 myfuse2.o system.o inode.o fuseop.o -lfuse 

myfuse2.o: myfuse2.c myfuse.h
	   cc -c myfuse2.c -D_FILE_OFFSET_BITS=64

system.o: system.c myfuse.h
	   cc -c system.c -D_FILE_OFFSET_BITS=64

inode.o : inode.c myfuse.h
	   cc -c inode.c -D_FILE_OFFSET_BITS=64

fuseop.o: fuseop.c myfuse.h
	   cc -c fuseop.c -D_FILE_OFFSET_BITS=64

clean:
	rm myfuse2 myfuse2.o system.o inode.o fuseop.o
