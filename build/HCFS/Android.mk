LOCAL_PATH := $(call my-dir)
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)

LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

# For CI build version
ifndef VERSION_NUM
    VERSION_NUM:=Manual build $(shell date +%Y%m%d-%H%M%S)
    empty:=
    space:= $(empty) $(empty)
    VERSION_NUM := $(subst $(space),_,$(VERSION_NUM))
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := hcfs
LOCAL_CFLAGS    += -pie -fPIE -O0 -Wall -Wextra \
		   -D_FILE_OFFSET_BITS=64 \
		   -D_ANDROID_ENV_ \
		   -DENCRYPT_ENABLE=0 \
		   -DDEDUP_ENABLE=0 \
		   -DSTAT_VFS_H="<fuse/sys/statvfs.h>" \
		   -D_ANDROID_PREMOUNT_ \
		   -DVERSION_NUM=\"$(VERSION_NUM)\"
LOCAL_LDFLAGS   += -pie -fPIE -O0
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/../../src/HCFS/*.c))
LOCAL_SHARED_LIBRARIES += libcurl \
			  libssl \
			  libcrypto \
			  libsqlite \
			  libfuse \
			  libjansson

## Compression feature
COMPRESS_ENABLE := 0
ifeq "$(COMPRESS_ENABLE)" "0"
    LOCAL_CFLAGS += -DCOMPRESS_ENABLE=0
else
    LOCAL_CFLAGS += -DCOMPRESS_ENABLE=1
    LOCAL_SHARED_LIBRARIES += liblz4
endif
include $(BUILD_EXECUTABLE)

include $(BUILD_PATH)/third_party/Android.mk
include $(BUILD_PATH)/prebuilt/Android.mk
