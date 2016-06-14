LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE		:= libfuse
LOCAL_SRC_FILES		:= $(patsubst $(LOCAL_PATH)/%, %, $(wildcard $(LOCAL_PATH)/lib/*.c))
LOCAL_C_INCLUDES	:= $(LOCAL_PATH) $(LOCAL_PATH)/fuse
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CFLAGS		+= -O0 \
			   -D_FILE_OFFSET_BITS=64 \
			   -DFUSE_USE_VERSION=26 \
			   -D__MULTI_THREAD

LOCAL_LDFLAGS		= -O0 \
			  -mno-fix-cortex-a53-843419 \
			  -mfix-cortex-a53-835769

include $(BUILD_SHARED_LIBRARY)
