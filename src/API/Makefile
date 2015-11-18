CC = gcc
CPPFLAGS =
CFLAGS = -lpthread

socket_serv: socket_serv.c pin_ops.o utils.o test
	$(CC) socket_serv.c pin_ops.o utils.o $(CFLAGS) -o sock_serv

test: test.c HCFS_api.o pin_ops.o hcfs_stat.o utils.o
	$(CC) test.c HCFS_api.o pin_ops.o hcfs_stat.o utils.o -ljansson -o test

HCFS_api.o: HCFS_api.c HCFS_api.h
	$(CC) -c HCFS_api.c

pin_ops.o: pin_ops.c pin_ops.h
	$(CC) -c pin_ops.c

hcfs_stat.o: hcfs_stat.c hcfs_stat.h
	$(CC) -c hcfs_stat.c

utils.p: utils.c utils.h
	$(CC) -c utils.c

doc: HCFS_API.h
	cldoc generate -c -- --basedir . --type html --static --merge docs  --output doc HCFS_api.h

clean:
	rm -f *.o sock_serv test
