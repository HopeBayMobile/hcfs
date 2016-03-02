BUILD_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
ifeq "$(INCLUDE_LIB_JASSON)" ""
  include $(BUILD_PATH)/third_party/jansson/Android.mk
endif
export INCLUDE_LIB_JASSON := 1

ifeq "$(INCLUDE_LZ4)" ""
  include $(CLEAR_VARS)
    LOCAL_MODULE := liblz4
    LOCAL_SRC_FILES := $(BUILD_PATH)/third_party/lz4/lz4.c
    LOCAL_C_INCLUDES := $(BUILD_PATH)/third_party/lz4
  include $(BUILD_SHARED_LIBRARY)
endif
export INCLUDE_LZ4 := 1
