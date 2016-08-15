CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -DARM_32bit_ -D_ANDROID_ENV_ -Wall -Wextra -g

HCFSvol: HCFSvol.o pin_test.o fake_copy_minimal.o
	$(CC) -o HCFSvol HCFSvol.o
	$(CC) -o pin_test pin_test.o
	$(CC) -o fake_copy_minimal fake_copy_minimal.o

HCFSvol.o: HCFSvol.c
	$(CC) $(CFLAGS) -c HCFSvol.c

pin_test.o: pin_test.c
	$(CC) $(CFLAGS) -c pin_test.c

fake_copy_minimal.o: fake_copy_minimal.c
	$(CC) $(CFLAGS) -c fake_copy_minimal.c

clean:
	rm -rf *.o HCFSvol pin_test
