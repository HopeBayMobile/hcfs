SHELL := /bin/bash
all :

build_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(build_dir)/ccache.mk
include $(build_dir)/ndk-build.mk
#include $(build_dir)/Application32.mk
#include $(build_dir)/Application64.mk

NDK_PROJECT_PATH := $(abspath .)

#####
# Target variables
#####
TARGETS32       += nexus-7
TARGETS32       += AOSP-kitkat-arm
TARGETS64       += nexus-5x
TARGETS         += $(TARGETS32)
TARGETS         += $(TARGETS64)
#   Add target list grouping by gdb or not
HCFS_OUTS32     := $(TARGETS32:%=out/%-hcfs)
HCFS_GDB_OUTS32 := $(TARGETS32:%=out/%-hcfs-gdb)
ALL_OUTS32      := $(HCFS_OUTS32) $(HCFS_GDB_OUTS32)
HCFS_OUTS64     := $(TARGETS64:%=out/%-hcfs)
HCFS_GDB_OUTS64 := $(TARGETS64:%=out/%-hcfs-gdb)
ALL_OUTS64      := $(HCFS_OUTS64) $(HCFS_GDB_OUTS64)
ALL_OUTS        := $(ALL_OUTS32) $(ALL_OUTS64)
HCFS_OUTS       := $(HCFS_OUTS32) $(HCFS_OUTS64)
HCFS_GDB_OUTS   := $(HCFS_GDB_OUTS32) $(HCFS_GDB_OUTS64)

#   let device be a variable of its targets
#   e.g. $(nexus-7) == out/nexus-7-hcfs out/nexus-7-hcfs-gdb
$(foreach t,$(TARGETS),$(eval $(t):= out/$(t)-hcfs out/$(t)-hcfs-gdb))

#####
# Common Build Flags
#####
#$(ALL_OUTS):      NDK_ARG     += V=1
$(ALL_OUTS):      NDK_ARG     += DEVICE=$*
$(ALL_OUTS32):      NDK_ARG     += NDK_APPLICATION_MK=Application32.mk
$(ALL_OUTS64):      NDK_ARG     += NDK_APPLICATION_MK=Application64.mk
$(ALL_OUTS):      NDK_ARG     += NDK_PROJECT_PATH=$(NDK_PROJECT_PATH)
$(ALL_OUTS):      NDK_ARG     += NDK_LIBS_OUT=$(NDK_PROJECT_PATH)/$@
$(ALL_OUTS):      NDK_ARG     += NDK_OUT=$(NDK_PROJECT_PATH)/$(@:out/%=obj/%)
$(ALL_OUTS):      NDK_ARG     += APP_CFLAGS="$(APP_CFLAGS)"
$(ALL_OUTS):      NDK_ARG     += APP_LDFLAGS="$(APP_LDFLAGS)"
$(HCFS_OUTS):     NDK_ARG     +=
$(HCFS_OUTS):     APP_CFLAGS  +=
$(HCFS_OUTS):     APP_LDFLAGS +=
$(HCFS_GDB_OUTS): NDK_ARG     += NDK_DEBUG=1
$(HCFS_GDB_OUTS): APP_CFLAGS  += -ggdb -O0 -DDEBUG_ON
$(HCFS_GDB_OUTS): APP_LDFLAGS += -ggdb -O0
$(ALL_OUTS64):      LIBPATH = lib64
$(ALL_OUTS64):      APP_ABI = arm64-v8a
$(ALL_OUTS32):      APP_ABI = armeabi-v7a
$(ALL_OUTS32):      LIBPATH = lib


#####
# Device Specific Rules
#####

# Some devices use BoringSSL instead of OpenSSL
TARGETS_USING_BORINGSSL := $(nexus-7) $(nexus-5x)
$(TARGETS_USING_BORINGSSL): NDK_ARG += OPENSSL_IS_BORINGSSL=1

#####
# Make rules
#####

all: out $(TARGETS)
	cd out; find -type f ! -name md5sum.md5 -exec md5sum {} \; > md5sum.md5

clean:
	@rm -rf {,*/}obj {,*/}out

# Requirement for all buils
$(ALL_OUTS): third_party/jansson force

# Add out rules driven from targets
# e.g. nexus-7: out/nexus-7-hcfs out/nexus-7-hcfs-gdb
$(foreach t,$(TARGETS),$(eval $(t): out/$(t)-hcfs out/$(t)-hcfs-gdb))

out/%-hcfs:
	@rm -rf $@
	ndk-build $(NDK_ARG)
	@mkdir -p $@/system/{bin,$(LIBPATH)}
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -I{} -0r mv {} $@/system/$(LIBPATH)
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' -type f -print0 | xargs -I{} -0r mv {} $@/system/bin
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
	  do if [ -f $@/system/$(LIBPATH)/$$i ]; then rm -f $@/system/$(LIBPATH)/$$i; fi done
	@ln -sf $(build_dir)/internal_testing_resource/* $@/
	@rm -rf $@/gdb.* $@/$(APP_ABI)


out/%-hcfs-gdb:
	@rm -rf $@ $@.zip
	ndk-build $(NDK_ARG)
	# Setup system folder, mapping to /system on device
	@mkdir -p $@/system/{bin,$(LIBPATH)}
	@rsync -ac -f'- /*/' $(@:out/%=obj/%)/local/$(APP_ABI)/ $@/$(APP_ABI)/
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -I{} -0r cp {} $@/system/$(LIBPATH)
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' ! -name 'gdb.setup' -type f -print0 | xargs -I{} -0r cp {} $@/system/bin
	# remove .so files already in device
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
	  do if [ -f $@/system/$(LIBPATH)/$$i ]; then rm -f $@/system/$(LIBPATH)/$$i; fi done
	@# copy resources
	ln -sf $(build_dir)/internal_testing_resource/* $@/
	sed -e "s#$(build_dir)/$(@:out/%=obj/%)/local/##g" \
	  -e "s#$(build_dir)/##g" \
	  -e "\$$afile $(APP_ABI)/hcfs" \
	  $@/$(APP_ABI)/gdb.setup > $@/gdb.setup
	$@/utils/prepare_gdb.setup.sh
ifneq "$(PASSWORD)" ""
	cd out && zip -P "$(PASSWORD)" -1 -r $(@:out/%=%).zip $(@:out/%=%)
	@rm -rf $@
endif

out:
	@mkdir -p $@

third_party/jansson: third_party/jansson/Android.mk
third_party/jansson/Android.mk:
	@$(build_dir)/../utils/init_submodule.sh

.PHONY: clean force copy_s58a_system_libs submodule
