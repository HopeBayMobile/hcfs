# Copyright (C) 2012 Seth Huang<seth.hg@gmail.com>
#
LOCAL_PATH := $(call my-dir)
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)
LIBS_PATH := $(BUILD_PATH)/prebuilt/$(DEVICE)

include $(CLEAR_VARS)
LOCAL_CFLAGS	  += -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -I$(BUILD_PATH)/../src/HCFS/
LOCAL_CFLAGS	  += -pie -fPIE
LOCAL_LDFLAGS	  += -pie -fPIE
LOCAL_MODULE    := HCFSvol
LOCAL_SRC_FILES := ../../src/CLI_utils/HCFSvol.c
include $(BUILD_EXECUTABLE)

include $(BUILD_PATH)/third_party/Android.mk
include $(BUILD_PATH)/prebuilt/Android.mk
