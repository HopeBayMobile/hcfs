include ../../build/ccache.mk

CFLAGS=-D_ANDROID_ENV_ -D_FILE_OFFSET_BITS=64 -g -I../HCFS

TARGET = demo_pyhcfs _pyhcfs.so
all: check_pkgs $(TARGET)

_pyhcfs.so: | parser.c parser.h pyhcfs_build.py
	@python3 pyhcfs_build.py
	@mv -vf _pyhcfs.*.so _pyhcfs.so || :
	@rm -f _pyhcfs.c

demo_pyhcfs: demo_pyhcfs.o parser.o
	cc $(CFLAGS) $^ -o $@

%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(realpath $<)

clean:
	rm -rf $(TARGET) *.o

check_pkgs:
	@$(shell cd ../../utils;pwd)/check_compile_pkgs.sh

demo: all
	@echo ====== start demo ======
	python3 demo_pyhcfs.py
