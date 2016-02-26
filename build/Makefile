SHELL := /bin/bash
all :
-include ccache.mk
-include ndk-build.mk
-include Application.mk
$(info $(shell git submodule update --init --recursive))

export NDK_PROJECT_PATH := $(abspath .)

TARGETS          += acer-s58a
#TARGETS          += acer-z630

all : out $(TARGETS)
	@cd out; find -mindepth 2 -type f -exec md5sum {} \; > md5sum.md5

clean :
	rm -rf libs obj out

define TARGET_RULE
$(1): out/$(1)-hcfs
$(1): out/$(1)-hcfs-gdb
hcfs     += out/$(1)-hcfs
hcfs_gdb += out/$(1)-hcfs-gdb
$(1)     := out/$(1)-hcfs out/$(1)-hcfs-gdb
endef

$(foreach t,$(TARGETS),$(eval $(call TARGET_RULE,$(t))))

# Common Flags
$(TARGETS): NDK_ARG += DEVICE=$*
$(TARGETS): NDK_ARG += NDK_APPLICATION_MK=Application.mk
$(TARGETS): NDK_ARG += TARGET_PLATFORM=android-21
$(TARGETS): NDK_ARG += NDK_LIBS_OUT=$(NDK_PROJECT_PATH)/$@
$(TARGETS): NDK_ARG += NDK_OUT=$(NDK_PROJECT_PATH)/$(@:out/%=obj/%)

# hcfs Flags
# $(hcfs): NDK_ARG +=

# hcfs-gdb Flags
$(hcfs_gdb): NDK_ARG += NDK_DEBUG=1
$(hcfs_gdb): NDK_ARG += HCFS_CFLAGS="-ggdb -O0"
$(hcfs_gdb): NDK_ARG += HCFS_LDFLAGS="-ggdb -O0"

# Device Flags
arch_arm64_lib_path := $(dir $(shell PATH=$(PATH) /usr/bin/which ndk-build))platforms/android-21/arch-arm64/usr/lib
$(acer-s58a): NDK_ARG += OPENSSL_IS_BORINGSSL=1
$(acer-s58a): $(arch_arm64_lib_path)/.copy_s58a_system_libs

$(arch_arm64_lib_path)/.copy_s58a_system_libs:
	rsync -a prebuilt/acer-s58a/system/*.so $(arch_arm64_lib_path)/
	touch $(arch_arm64_lib_path)/.copy_s58a_system_libs

_nr_cpu := $(shell cat /proc/cpuinfo | grep processor | wc -l)
PARALLEL_JOBS := -j$(shell expr $(_nr_cpu) + $(_nr_cpu))

define runprepareout
	@mkdir -p $@/system/{bin,lib64}
	mv -f $@/$(APP_ABI)/*.so $@/system/lib64
	cd $@/system/lib64; rm -f $(shell \ls $(NDK_PROJECT_PATH)/prebuilt/acer-s58a/system/)
	@mv -f $@/$(APP_ABI)/* $@/system/bin
	@cp -r internal_testing_resource/* $@/
endef

out/%-hcfs: force
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(runprepareout)
	@rm -rf $@/gdb.* $@/$(APP_ABI)

out/%-hcfs-gdb: force
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(runprepareout)
	@cp $@/system/*/* $@/$(APP_ABI)
	@rsync -a ../src/{API,CLI_utils,HCFS} $@/src/
	@cp -r include $@/

# (cd out/acer-s58a-hcfs/system/lib64/; rm -f `\ls ~/hcfs/build/prebuilt/acer-s58a/system/`)

out:
	mkdir out

.PHONY: clean force copy_s58a_system_libs
