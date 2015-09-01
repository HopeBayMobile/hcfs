CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall -Wextra

HCFSvol: HCFSvol.o
	$(CC) -o HCFSvol HCFSvol.o

HCFSvol.o: HCFSvol.c
	$(CC) $(CFLAGS) -c HCFSvol.c

clean:
	rm -rf *.o HCFSvol
