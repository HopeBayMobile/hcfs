LOCAL_PATH := $(dir $(call this-makefile))
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)
LIBS_PATH := $(BUILD_PATH)/prebuilt/$(DEVICE)

LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(CLEAR_VARS)
LOCAL_CFLAGS	:= -O0 $(HCFS_CFLAGS) -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD
LOCAL_LDFLAGS   += -O0 $(HCFS_LDFLAGS) -fuse-ld=mcld -mno-fix-cortex-a53-843419 -mfix-cortex-a53-835769
LOCAL_MODULE    := libfuse
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)../third_party/libfuse/*.c)
LOCAL_C_INCLUDES := $(BUILD_PATH)/include $(BUILD_PATH)/include/fuse
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES := $(LIBS_PATH)/system/libcurl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := $(LIBS_PATH)/system/libssl.so
include $(PREBUILT_SHARED_LIBRARY)

ifeq "$(INCLUDE_CRYPTO)" ""
  include $(CLEAR_VARS)
  LOCAL_MODULE    := libcrypto
  LOCAL_SRC_FILES := $(LIBS_PATH)/system/libcrypto.so
  include $(PREBUILT_SHARED_LIBRARY)
endif
export INCLUDE_CRYPTO := 1

ifeq "$(INCLUDE_SQLITE)" ""
  include $(CLEAR_VARS)
  LOCAL_MODULE    := libsqlite
  LOCAL_SRC_FILES := $(LIBS_PATH)/system/libsqlite.so
  include $(PREBUILT_SHARED_LIBRARY)
endif
export INCLUDE_SQLITE := 1

include $(CLEAR_VARS)
LOCAL_MODULE    := hcfs
LOCAL_CFLAGS    += -pie -fPIE -O0 $(HCFS_CFLAGS) -Wall -Wextra
LOCAL_CFLAGS    += -D_FILE_OFFSET_BITS=64
LOCAL_CFLAGS    += -D_ANDROID_ENV_ -DENCRYPT_ENABLE=0 -DDEDUP_ENABLE=0 -DSTAT_VFS_H="<fuse/sys/statvfs.h>" -D_ANDROID_PREMOUNT_
LOCAL_LDFLAGS   += -pie -fPIE -O0 $(HCFS_LDFLAGS)
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)../../src/HCFS/*.c)
LOCAL_C_INCLUDES := $(BUILD_PATH)/include/sqlite3 $(BUILD_PATH)/include $(BUILD_PATH)/include/jansson
LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto libsqlite libfuse libjansson

## Compression feature
COMPRESS_ENABLE := 0
ifeq "$(COMPRESS_ENABLE)" "0"
  LOCAL_CFLAGS += -DCOMPRESS_ENABLE=0
else
  LOCAL_CFLAGS += -DCOMPRESS_ENABLE=1
  LOCAL_C_INCLUDES += $(BUILD_PATH)/third_party/lz4
  LOCAL_SHARED_LIBRARIES += liblz4
endif

## Openssl
ifdef OPENSSL_IS_BORINGSSL
  LOCAL_C_INCLUDES += $(BUILD_PATH)/include/boringssl
else
  LOCAL_C_INCLUDES += $(BUILD_PATH)/include/openssl
endif
include $(BUILD_EXECUTABLE)

include $(BUILD_PATH)/third_party/Android.mk