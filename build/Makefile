SHELL := /bin/bash
all :
-include ccache.mk
-include ndk-build.mk

export NDK_PROJECT_PATH := $(abspath .)

TARGETS          := acer-z630 acer-s58a
TARGETS-HCFS     := $(addsuffix -hcfs, $(TARGETS))
TARGETS-HCFS-GDB := $(addsuffix -hcfs-gdb, $(TARGETS))

all : out $(TARGETS)
	@cd out; find -mindepth 2 -type f -exec md5sum {} \; > md5sum.md5

clean :
	rm -rf libs obj out

define TARGET_RULE
$(1):          $(1)-hcfs $(1)-hcfs-gdb
$(1)-hcfs:     out/$(1)-hcfs
$(1)-hcfs-gdb: out/$(1)-hcfs-gdb
endef

$(foreach t,$(TARGETS),$(eval $(call TARGET_RULE,$(t))))

define runprepareout
	@mkdir -p $@/system/{bin,lib}
	@mv -f $@/armeabi/*.so $@/system/lib
	@mv -f $@/armeabi/* $@/system/bin
	@cp -r internal_testing_resource/* $@/
endef

# Common Flags
$(TARGETS): NDK_ARG += DEVICE=$*
$(TARGETS): NDK_ARG += NDK_APPLICATION_MK=Application.mk
$(TARGETS): NDK_ARG += TARGET_PLATFORM=android-21
$(TARGETS): NDK_ARG += NDK_LIBS_OUT=$@
$(TARGETS): NDK_ARG += NDK_OUT=$(@:out/%=obj/%)

# hcfs Flags

# hcfs-gdb Flags
$(TARGETS-HCFS-GDB): NDK_ARG += NDK_DEBUG=1
$(TARGETS-HCFS-GDB): NDK_ARG += HCFS_CFLAGS="-ggdb -O0"
$(TARGETS-HCFS-GDB): NDK_ARG += HCFS_LDFLAGS="-ggdb -O0"

# Device Flags
acer-s58a-%: NDK_ARG += OPENSSL_IS_BORINGSSL=1

_nr_cpu := $(shell cat /proc/cpuinfo | grep processor | wc -l)
PARALLEL_JOBS := -j$(shell expr $(_nr_cpu) + $(_nr_cpu))

out/%-hcfs:
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(runprepareout)
	@rm -rf $@/gdb.* $@/armeabi

out/%-hcfs-gdb:
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(runprepareout)
	@cp $@/system/*/* $@/armeabi
	@rsync -a ../src/{API,CLI_utils,HCFS} $@/src/
	@cp -r include $@/

out:
	mkdir out

.PHONY: clean
