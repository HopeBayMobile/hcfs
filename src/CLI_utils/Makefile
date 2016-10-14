EXEC = \
	HCFSvol \
	pin_test \

.PHONY: all
all: $(EXEC)

CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -Wall -Wextra -g -I../HCFS

%: %.c
	$(LINK.c) $(realpath $^) $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -rf *.o *.d $(EXEC)
