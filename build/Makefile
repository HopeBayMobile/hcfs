SHELL := /bin/bash
all :
build_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(build_dir)/ccache.mk
include $(build_dir)/ndk-build.mk
include $(build_dir)/Application.mk

NDK_PROJECT_PATH := $(abspath .)

TARGETS          += acer-s58a
TARGETS          += nexus-5x
#TARGETS          += acer-z630

all : out third_party/jansson $(TARGETS)
	cd out; find -type f ! -name md5sum.md5 -exec md5sum {} \; > md5sum.md5

clean :
	@rm -rf {,*/}obj {,*/}out

define TARGET_RULE
$(1)	  : out/$(1)-hcfs out/$(1)-hcfs-gdb
$(1)	  :=out/$(1)-hcfs out/$(1)-hcfs-gdb
hcfs	  +=out/$(1)-hcfs
hcfs_gdb  +=out/$(1)-hcfs-gdb
endef

$(foreach t,$(TARGETS),$(eval $(call TARGET_RULE,$(t))))

# Common Flags
$(TARGETS): NDK_ARG += DEVICE=$*
$(TARGETS): NDK_ARG += NDK_APPLICATION_MK=Application.mk
$(TARGETS): NDK_ARG += NDK_PROJECT_PATH=$(NDK_PROJECT_PATH)
$(TARGETS): NDK_ARG += NDK_LIBS_OUT=$(NDK_PROJECT_PATH)/$@
$(TARGETS): NDK_ARG += NDK_OUT=$(NDK_PROJECT_PATH)/$(@:out/%=obj/%)
$(TARGETS): NDK_ARG += APP_CFLAGS="$(APP_CFLAGS)"
$(TARGETS): NDK_ARG += APP_LDFLAGS="$(APP_LDFLAGS)"
#$(TARGETS): NDK_ARG += V=1

# Device Flags
arch_arm64_lib_path := $(NDK_DIR)/platforms/android-21/arch-arm64/usr/lib
$(acer-s58a): NDK_ARG += OPENSSL_IS_BORINGSSL=1
$(acer-s58a): $(arch_arm64_lib_path)/.copy_s58a_system_libs
$(nexus-5x): NDK_ARG += OPENSSL_IS_BORINGSSL=1

$(arch_arm64_lib_path)/.copy_s58a_system_libs:
	rsync -a $(build_dir)/prebuilt/acer-s58a/system/*.so $(arch_arm64_lib_path)/
	touch $(arch_arm64_lib_path)/.copy_s58a_system_libs

# hcfs Flags
$(hcfs): NDK_ARG +=
$(hcfs): APP_CFLAGS+=
$(hcfs): APP_LDFLAGS+=

# DEBUG Flags
$(hcfs_gdb): NDK_ARG += NDK_DEBUG=1
$(hcfs_gdb): APP_CFLAGS+=-ggdb -O0 -DDEBUG_ON
$(hcfs_gdb): APP_LDFLAGS+=-ggdb -O0

out/%-hcfs: force
	ndk-build $(NDK_ARG)
	@mkdir -p $@/system/{bin,lib64}
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -0r mv -t $@/system/lib64
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' -type f -print0 | xargs -0r mv -t $@/system/bin
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
		do if [ -f $@/system/lib64/$$i ]; then rm -f $@/system/lib64/$$i; fi done
	@cp -r $(build_dir)/internal_testing_resource/* $@/
	@rm -rf $@/gdb.* $@/$(APP_ABI)

out/%-hcfs-gdb: force
	ndk-build $(NDK_ARG)
	@mkdir -p $@/system/{bin,lib64}
	@rsync -ac -f'- /*/' $(@:out/%=obj/%)/local/$(APP_ABI)/ $@/$(APP_ABI)/
	@find $@/$(APP_ABI) -maxdepth 1 -name '*.so' -type f -print0 | xargs -0r cp -t $@/system/lib64
	@find $@/$(APP_ABI) -maxdepth 1 ! -name '*.so' ! -name 'gdb.setup' -type f -print0 | xargs -0r cp -t $@/system/bin
	@for i in $$(\ls $(build_dir)/prebuilt/$*/system/); \
		do if [ -f $@/system/lib64/$$i ]; then rm -f $@/system/lib64/$$i; fi done
	@cp -r $(build_dir)/internal_testing_resource/* $@/
	@rsync -a $(build_dir)/../src/{API,CLI_utils,HCFS} $@/src/
	@cp -r $(build_dir)/prebuilt/include $(build_dir)/third_party $@/
	@sed -e "s#$(build_dir)/$(@:out/%=obj/%)/local/##g" \
		-e "s#/opt/android-ndk-r10e/#../../../../HCFS_android/android-ndk-r10e-win64/#g" \
		-e "s#$(build_dir)/##g" \
		-e "s#API_SERV/#src/API/#g" \
		-e "s#HCFS_CLI/#src/CLI_utils/#" \
		-e "s#HCFS/#src/HCFS/#g" \
		-e "\$$afile $(APP_ABI)/hcfs" \
		-e "\$$atarget remote :5678" \
		$@/$(APP_ABI)/gdb.setup > $@/gdb.setup
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
