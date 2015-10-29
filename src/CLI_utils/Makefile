CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_ -Wall -Wextra

HCFSvol: HCFSvol.o
	$(CC) -o HCFSvol HCFSvol.o

HCFSvol.o: HCFSvol.c
	$(CC) $(CFLAGS) -c HCFSvol.c

clean:
	rm -rf *.o HCFSvol
