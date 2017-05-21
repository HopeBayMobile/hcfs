LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE		:= libfuse
LOCAL_SRC_FILES		:= $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/lib/*.c))
LOCAL_C_INCLUDES	:= $(LOCAL_PATH) $(LOCAL_PATH)/fuse
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_CFLAGS		+= -O0 \
			   -D_FILE_OFFSET_BITS=64 \
			   -DFUSE_USE_VERSION=26 \
			   -D__MULTI_THREAD

LOCAL_LDFLAGS		= -O0 \

ifeq ("$(APP_ABI)", "arm64-v8a")
ifneq ("$(NDK_TOOLCHAIN_VERSION)","clang")
  LOCAL_LDFLAGS += -mno-fix-cortex-a53-843419
  LOCAL_LDFLAGS += -mfix-cortex-a53-835769
endif
endif

include $(BUILD_SHARED_LIBRARY)
