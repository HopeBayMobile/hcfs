LOCAL_PATH := $(dir $(call this-makefile))

LIBS_PATH := prebuilt/$(TARGET_ARCH_ABI)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(CLEAR_VARS)
LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD -O0
LOCAL_MODULE    := libfuse
LOCAL_SRC_FILES := libfuse/cuse_lowlevel.c \
                   libfuse/fuse.c \
                   libfuse/fuse_kern_chan.c \
                   libfuse/fuse_loop.c \
                   libfuse/fuse_loop_mt.c \
                   libfuse/fuse_lowlevel.c \
                   libfuse/fuse_mt.c \
                   libfuse/fuse_opt.c \
                   libfuse/fuse_session.c \
                   libfuse/fuse_signals.c \
                   libfuse/helper.c \
                   libfuse/mount.c \
                   libfuse/mount_util.c \
                   libfuse/ulockmgr.c

LOCAL_C_INCLUDES := include
LOCAL_LDFLAGS += -O0 -fuse-ld=mcld
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES := $(LIBS_PATH)/libcurl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := $(LIBS_PATH)/libssl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcrypto
LOCAL_SRC_FILES := $(LIBS_PATH)/libcrypto.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := liblz4
LOCAL_SRC_FILES := $(LIBS_PATH)/liblz4.so
include $(PREBUILT_SHARED_LIBRARY)

ifeq "$(INCLUDE_SQLITE)" ""
  include $(CLEAR_VARS)
  LOCAL_MODULE    := libsqlite
  LOCAL_SRC_FILES := $(LIBS_PATH)/libsqlite.so
  include $(PREBUILT_SHARED_LIBRARY)
endif
export INCLUDE_SQLITE := 1

include $(CLEAR_VARS)
LOCAL_CFLAGS    := -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -D_ANDROID_PREMOUNT_ -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -DDEDUP_ENABLE=0 -DSTAT_VFS_H="<fuse/sys/statvfs.h>"
LOCAL_CFLAGS    += -pie -fPIE -O0 -Wall -Wextra
LOCAL_LDFLAGS   += -pie -fPIE -O0
LOCAL_MODULE    := hcfs
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)src/*.c)
LOCAL_C_INCLUDES := include/sqlite3 include
LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto liblz4 libsqlite libfuse
include $(BUILD_EXECUTABLE)

