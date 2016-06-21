all: _pyhcfs.so parser

_pyhcfs.so: parser.c parser.h
	cd ../../; ./build.sh pyhcfs

parser: parser.c parser.h
	cc -D_ANDROID_ENV_  -I../HCFS  parser.c parser.h  -g -o parser

clean:
	rm -f _pyhcfs.so pyhcfs
CFLAGS=-D_ANDROID_ENV_
.PHONY: _pyhcfs.so parser
