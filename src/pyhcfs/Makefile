all: _pyhcfs.so pyhcfs

_pyhcfs.so: pyhcfs.c pyhcfs.h
	cd ../../; ./build.sh pyhcfs

pyhcfs: pyhcfs.c pyhcfs.h
	cc -D_ANDROID_ENV_  -I../HCFS  pyhcfs.c pyhcfs.h  -g -o pyhcfs

clean:
	rm -f _pyhcfs.so pyhcfs
CFLAGS=-D_ANDROID_ENV_
.PHONY: _pyhcfs.so pyhcfs
