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

include $(CLEAR_VARS)
LOCAL_MODULE := liblz4
LOCAL_C_INCLUDES := lz4/lz4.c lz4/lz4frame.c lz4/lz4hc.c lz4/xxhash.c programs/lz4io.c
LOCAL_SRC_FILES := $(LOCAL_PATH)/lz4
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(BUILT_STATIC_LIBRARY)

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
LOCAL_CFLAGS    += -D_ANDROID_ENV_ -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -DDEDUP_ENABLE=0 -DSTAT_VFS_H="<fuse/sys/statvfs.h>" -D_ANDROID_PREMOUNT_
LOCAL_LDFLAGS   += -pie -fPIE -O0 $(HCFS_LDFLAGS) -fuse-ld=mcld -mno-fix-cortex-a53-843419 -mfix-cortex-a53-835769
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)../../src/HCFS/*.c)
LOCAL_C_INCLUDES := $(BUILD_PATH)/include/sqlite3 $(BUILD_PATH)/include
ifdef OPENSSL_IS_BORINGSSL
LOCAL_C_INCLUDES += $(BUILD_PATH)/include/boringssl
else
LOCAL_C_INCLUDES += $(BUILD_PATH)/include/openssl
endif
LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto liblz4 libsqlite libfuse
include $(BUILD_EXECUTABLE)

