include ../../build/ccache.mk

CFLAGS=-D_ANDROID_ENV_

all: _pyhcfs.so demo_pyhcfs

_pyhcfs.so: parser.c parser.h
	python3 pyhcfs_build.py
	mv -vf _pyhcfs.*.so _pyhcfs.so || :

parser.o demo_pyhcfs: CFLAGS+=-g -I../HCFS
demo_pyhcfs: demo_pyhcfs.c parser.h parser.o
	cc $(CFLAGS) $< parser.o -o $@

parser.o: parser.c parser.h
	cc $(CFLAGS) $< -c

clean:
	rm -rf _pyhcfs.c _pyhcfs.o _pyhcfs.so _pyhcfs.*.so parser parser.o __pycache__/

check_pkgs:
	@$(shell cd ../../utils;pwd)/check_compile_pkgs.sh

demo: all
	@echo ====== start demo ======
	python3 demo_pyhcfs.py

.PHONY: _pyhcfs.so parser
