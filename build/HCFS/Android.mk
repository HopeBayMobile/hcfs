LOCAL_PATH := $(call my-dir)

#LIBS_PATH := libs/$(TARGET_ARCH_ABI)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(CLEAR_VARS)
LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD
LOCAL_MODULE    := libfuse
LOCAL_SRC_FILES := $(wildcard libfuse/*.c)
LOCAL_C_INCLUDES := fuse
LOCAL_CFLAGS += -pie -fPIE
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES := libs/libcurl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := libs/libssl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libcrypto
LOCAL_SRC_FILES := libs/libcrypto.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= liblz4
LOCAL_SRC_FILES := libs/liblz4.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libsqlite
LOCAL_SRC_FILES := libs/libsqlite.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS    := -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -DDEDUP_ENABLE=0 -DSTAT_VFS_H="<fuse/sys/statvfs.h>"
LOCAL_CFLAGS += -pie -fPIE -Wall -Wextra
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_MODULE    := hcfs
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/../../src/HCFS/*.c)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/sqlite3
LOCAL_STATIC_LIBRARIES := libfuse
LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto liblz4 libsqlite
include $(BUILD_EXECUTABLE)

