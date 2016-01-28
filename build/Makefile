all :
-include ccache.mk
-include ndk-build.mk

export NDK_PROJECT_PATH := $(abspath .)
export NDK_OUT := $(abspath ./obj)
export NDK_LIBS_OUT := $(abspath ./libs)

targets := acer-z630 acer-s58a
target_out := $(addsuffix -hcfs, $(targets)) $(addsuffix -hcfs-gdb, $(targets))
target_out := $(addprefix out/, $(target_out))

all : $(target_out)
	@cd out; find -mindepth 2 -type f -exec md5sum {} \; > md5sum.md5

clean :
	rm -rf libs obj out/*


define runprepareout
	@mkdir -p $@/system/bin
	@mkdir -p $@/system/lib
	@mv -f $@/armeabi/*.so $@/system/lib
	@mv -f $@/armeabi/* $@/system/bin
	@cp -r release_resource/* $@/
	@rm -r $@/armeabi
endef

$(target_out): NDK_ARG += DEVICE=$*
$(target_out): NDK_ARG += NDK_OUT=$(abspath ./obj/$*)
$(target_out): NDK_ARG += NDK_LIBS_OUT=$(abspath ./$@)
$(target_out): NDK_ARG += NDK_APPLICATION_MK=Application.mk
$(target_out): NDK_ARG += TARGET_PLATFORM=android-21
out/acer-s58a-hcfs out/acer-s58a-hcfs-gdb: NDK_ARG += OPENSSL_IS_BORINGSSL=1
out/%-hcfs-gdb: NDK_ARG += NDK_DEBUG=1
out/%-hcfs-gdb:	NDK_ARG += HCFS_CFLAGS="-ggdb -O0"
out/%-hcfs-gdb: NDK_ARG += HCFS_LDFLAGS="-ggdb -O0"

out/%-hcfs:
	ndk-build $(NDK_ARG)
	$(runprepareout)
	@rm -rf out/hcfs/gdb.*

out/%-hcfs-gdb:
	ndk-build $(NDK_ARG)
	@cp -r $@/armeabi $@/armeabi-tmp
	$(runprepareout)
	@mv $@/armeabi-tmp $@/armeabi
	@rsync -a ../src/{API,CLI_utils,HCFS} $@/src/
	@cp -r include $@/

out:
	mkdir out

.PHONY: $(target_out) clean
