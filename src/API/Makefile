CC = gcc
CPPFLAGS =
CFLAGS = -lpthread -lsqlite3

socket_serv: socket_serv.c pin_ops.o hcfs_stat.o hcfs_sys.o utils.o global.h marco.h test
	$(CC) socket_serv.c pin_ops.o hcfs_stat.o hcfs_sys.o utils.o $(CFLAGS) -o sock_serv

HCFS_api.o: HCFS_api.c HCFS_api.h global.h marco.h
	$(CC) -c HCFS_api.c

test: test.c HCFS_api.o
	$(CC) test.c HCFS_api.o -ljansson -o test

pin_ops.o: pin_ops.c pin_ops.h
	$(CC) -c pin_ops.c

hcfs_stat.o: hcfs_stat.c hcfs_stat.h
	$(CC) -c hcfs_stat.c

hcfs_sys.o: hcfs_sys.c hcfs_sys.h
	$(CC) -c hcfs_sys.c

utils.o: utils.c utils.h
	$(CC) -c utils.c

publish: doc
	sudo cp -r doc /mnt/nas/CloudDataSolution/Gateway_2.0/

doc: HCFS_api.c HCFS_api.h
	cldoc generate -c -- --basedir . --type html --static --merge docs  --output doc HCFS_api.h

clean:
	rm -f *.o sock_serv test
