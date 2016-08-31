SHELL := /bin/bash
all :
build_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(build_dir)/ccache.mk
include $(build_dir)/ndk-build.mk
include $(build_dir)/Application.mk

NDK_PROJECT_PATH := $(abspath .)

#####
# Target variables
#####
TARGETS       += acer-s58a
TARGETS       += nexus-5x
#   Add target list grouping by gdb or not
HCFS_OUTS     := $(TARGETS:%=out/%-hcfs)
HCFS_GDB_OUTS := $(TARGETS:%=out/%-hcfs-gdb)
ALL_OUTS      := $(HCFS_OUTS) $(HCFS_GDB_OUTS)

#   let device be a variable of its targets
#   e.g. $(nexus-5x) == out/nexus-5x-hcfs out/nexus-5x-hcfs-gdb
$(foreach t,$(TARGETS),$(eval $(t):= out/$(t)-hcfs out/$(t)-hcfs-gdb))

#####
# Common Build Flags
#####
#$(ALL_OUTS):      NDK_ARG     += V=1
$(ALL_OUTS):      NDK_ARG     += DEVICE=$*
$(ALL_OUTS):      NDK_ARG     += NDK_APPLICATION_MK=Application.mk
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

#####
# Device Specific Rules
#####

# Some devices use BoringSSL instead of OpenSSL
TARGETS_USING_BORINGSSL := $(acer-s58a) $(nexus-5x)
$(TARGETS_USING_BORINGSSL): NDK_ARG += OPENSSL_IS_BORINGSSL=1

# For s58a, copy required .so files to NDK lib
arch_arm64_lib_path := $(NDK_DIR)/platforms/android-21/arch-arm64/usr/lib
$(acer-s58a): $(arch_arm64_lib_path)/.copy_s58a_system_libs
$(arch_arm64_lib_path)/.copy_s58a_system_libs:
	rsync -a $(build_dir)/prebuilt/acer-s58a/system/*.so $(arch_arm64_lib_path)/
	touch $(arch_arm64_lib_path)/.copy_s58a_system_libs

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
# e.g. nexus-5x: out/nexus-5x-hcfs out/nexus-5x-hcfs-gdb
$(foreach t,$(TARGETS),$(eval $(t): out/$(t)-hcfs out/$(t)-hcfs-gdb))

out/%-hcfs:
	ndk-build $(NDK_ARG)
	@mkdir -p $@/system/{bin,lib64}
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -I{} -0r mv {} $@/system/lib64
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' -type f -print0 | xargs -I{} -0r mv {} $@/system/bin
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
		do if [ -f $@/system/lib64/$$i ]; then rm -f $@/system/lib64/$$i; fi done
	@ln -s $(build_dir)/internal_testing_resource/* $@/
	@rm -rf $@/gdb.* $@/$(APP_ABI)


out/%-hcfs-gdb:
	ndk-build $(NDK_ARG)
	# Setup system folder, mapping to /system on device
	@mkdir -p $@/system/{bin,lib64}
	@rsync -ac -f'- /*/' $(@:out/%=obj/%)/local/$(APP_ABI)/ $@/$(APP_ABI)/
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -I{} -0r cp {} $@/system/lib64
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' ! -name 'gdb.setup' -type f -print0 | xargs -I{} -0r cp {} $@/system/bin
	# remove .so files already in device
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
		do if [ -f $@/system/lib64/$$i ]; then rm -f $@/system/lib64/$$i; fi done
	@# copy resources
	ln -s $(build_dir)/internal_testing_resource/* $@/
	sed -e "s#$(build_dir)/$(@:out/%=obj/%)/local/##g" \
		-e "s#$(build_dir)/##g" \
		-e "\$$afile $(APP_ABI)/hcfs" \
		-e "\$$aset sysroot ." \
		-e "\$$atarget remote :5678" \
		-e "\$$acont" \
		$@/$(APP_ABI)/gdb.setup > $@/gdb.setup
	$@/utils/prepare_gdb.setup.sh

ifneq "$(PASSWORD)" ""
	zip -P "$(PASSWORD)" -1 -r $@.zip $@
	@rm -rf $@
endif

out:
	@mkdir -p $@

third_party/jansson: third_party/jansson/Android.mk
third_party/jansson/Android.mk:
	@$(build_dir)/../utils/init_submodule.sh

.PHONY: clean force copy_s58a_system_libs submodule
