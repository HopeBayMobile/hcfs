SHELL := /bin/bash
all :
build_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(build_dir)/ccache.mk
include $(build_dir)/ndk-build.mk
include $(build_dir)/Application.mk

NDK_PROJECT_PATH := $(abspath .)

TARGETS          += acer-s58a
#TARGETS          += acer-z630

all : submodule out $(TARGETS)
	cd out; find -mindepth 2 -type f -exec md5sum {} \; > md5sum.md5

submodule :
	@if [ ! -n "$$(grep "^gitlab.hopebaytech.com " ~/.ssh/known_hosts)" ]; \
		then ssh-keyscan gitlab.hopebaytech.com >> ~/.ssh/known_hosts 2>/dev/null; \
	fi
	@git submodule update --init $(build_dir)/third_party/jansson

clean :
	@rm -rf obj out
	@make clean -s -C $(build_dir)/third_party/lz4

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
$(TARGETS): NDK_ARG += NDK_PROJECT_PATH=$(NDK_PROJECT_PATH)
$(TARGETS): NDK_ARG += NDK_LIBS_OUT=$(NDK_PROJECT_PATH)/$@
$(TARGETS): NDK_ARG += NDK_OUT=$(NDK_PROJECT_PATH)/$(@:out/%=obj/%)

# hcfs Flags
# $(hcfs): NDK_ARG +=

# hcfs-gdb Flags
$(hcfs_gdb): NDK_ARG += NDK_DEBUG=1
$(hcfs_gdb): NDK_ARG += HCFS_CFLAGS="-ggdb -O0 -DDEBUG_ON"
$(hcfs_gdb): NDK_ARG += HCFS_LDFLAGS="-ggdb -O0"

# Device Flags
arch_arm64_lib_path := $(NDK_DIR)/platforms/android-21/arch-arm64/usr/lib
$(acer-s58a): NDK_ARG += OPENSSL_IS_BORINGSSL=1
$(acer-s58a): $(arch_arm64_lib_path)/.copy_s58a_system_libs

$(arch_arm64_lib_path)/.copy_s58a_system_libs:
	rsync -a $(build_dir)/prebuilt/acer-s58a/system/*.so $(arch_arm64_lib_path)/
	touch $(arch_arm64_lib_path)/.copy_s58a_system_libs

_nr_cpu := $(shell cat /proc/cpuinfo | grep processor | wc -l)
PARALLEL_JOBS := -j$(shell expr $(_nr_cpu) + $(_nr_cpu))

define prepare_out
	@mkdir -p $@/system/{bin,lib64}
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -0r mv -t $@/system/lib64
	@cd $@/system/lib64/; for i in $$(\ls $(build_dir)/prebuilt/acer-s58a/system/); \
		do if [ -f $$i ]; then rm -f $$i; fi done
	@mv -f $@/$(APP_ABI)/* $@/system/bin
	@cp -r $(build_dir)/internal_testing_resource/* $@/
endef

out/%-hcfs: force
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(prepare_out)
	@rm -rf $@/gdb.* $@/$(APP_ABI)

out/%-hcfs-gdb: force
	ndk-build $(NDK_ARG) $(PARALLEL_JOBS)
	$(prepare_out)
	@cp $@/system/*/* $@/$(APP_ABI)
	@rsync -a $(build_dir)/../src/{API,CLI_utils,HCFS} $@/src/
	@cp -r $(build_dir)/include $@/

out:
	@mkdir out

.PHONY: clean force copy_s58a_system_libs submodule
