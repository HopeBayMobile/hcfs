include ../../build/ccache.mk

CFLAGS=-D_ANDROID_ENV_

all: _pyhcfs.so parser

_pyhcfs.so: parser.c parser.h
	python pyhcfs_build.py

parser: CFLAGS+=-g -I../HCFS
parser: parser.c parser.h
	cc $(CFLAGS) $< -o $@

clean:
	rm -f _pyhcfs.c _pyhcfs.o _pyhcfs.so parser parser.o

check_pkgs:
	@$(shell cd ../../utils;pwd)/check_compile_pkgs.sh

.PHONY: _pyhcfs.so parser
