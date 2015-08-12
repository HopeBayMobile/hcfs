CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wunused-variable -Wimplicit-function-declaration

HCFSvol: HCFSvol.o
	$(CC) -o HCFSvol HCFSvol.o

HCFSvol.o: HCFSvol.c
	$(CC) $(CFLAGS) -c HCFSvol.c

clean:
	rm -rf *.o HCFSvol
