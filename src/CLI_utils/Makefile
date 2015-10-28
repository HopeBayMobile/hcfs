CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_ -Wall -Wextra

HCFSvol: HCFSvol.o pin_test.o
	$(CC) -o HCFSvol HCFSvol.o
	$(CC) -o pin_test pin_test.o

HCFSvol.o: HCFSvol.c
	$(CC) $(CFLAGS) -c HCFSvol.c

pin_test.o: pin_test.c
	$(CC) $(CFLAGS) -c pin_test.c

clean:
	rm -rf *.o HCFSvol
