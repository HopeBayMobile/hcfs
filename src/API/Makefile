CC = gcc
CPPFLAGS =
CFLAGS = -lpthread

socket_serv: socket_serv.c pin_ops.o test
	$(CC) socket_serv.c pin_ops.o $(CFLAGS) -o sock_serv

test: test.c HCFS_api.o pin_ops.o
	$(CC) test.c HCFS_api.o pin_ops.o -ljansson -o test

HCFS_api.o: HCFS_api.c HCFS_api.h
	$(CC) -c HCFS_api.c

pin_ops.o: pin_ops.c pin_ops.h
	$(CC) -c pin_ops.c

clean:
	rm -f *.o sock_serv test
